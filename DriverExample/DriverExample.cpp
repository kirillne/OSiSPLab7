#include "stdafx.h"

/*
	DriverExample - Main file
	This file contains a very simple implementation of a WDM driver. Note that it does not support all
	WDM functionality, or any functionality sufficient for practical use. The only thing this driver does
	perfectly, is loading and unloading.

	To install the driver, go to Control Panel -> Add Hardware Wizard, then select "Add a new hardware device".
	Select "manually select from list", choose device category, press "Have Disk" and enter the path to your
	INF file.
	Note that not all device types (specified as Class in INF file) can be installed that way.

	To start/stop this driver, use Windows Device Manager (enable/disable device command).

	If you want to speed up your driver development, it is recommended to see the BazisLib library, that
	contains convenient classes for standard device types, as well as a more powerful version of the driver
	wizard. To get information about BazisLib, see its website:
		http://bazislib.sysprogs.org/ cdsdf sd
*/
#define FILE_DEVICE_UNKNOWN             0x00000022
#define IOCTL_UNKNOWN_BASE              FILE_DEVICE_UNKNOWN
#define IOCTL_PROCOBSRV_GET_PROCINFO    \
	CTL_CODE(IOCTL_UNKNOWN_BASE, 0x0801, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)


void DriverExampleUnload(IN PDRIVER_OBJECT DriverObject);
NTSTATUS DriverExampleDispatchIoctl(	IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp	);
NTSTATUS DriverExampleCreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS DriverExampleDefaultHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS DriverExampleAddDevice(IN PDRIVER_OBJECT  DriverObject, IN PDEVICE_OBJECT  PhysicalDeviceObject);
NTSTATUS DriverExamplePnP(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
void NotifyRoutine(IN HANDLE ParentId,IN HANDLE ProcessId,IN BOOLEAN Create);

PDEVICE_OBJECT g_pDeviceObject;

typedef struct _deviceExtension
{
	PDEVICE_OBJECT DeviceObject;
	PDEVICE_OBJECT TargetDeviceObject;
	PDEVICE_OBJECT PhysicalDeviceObject;
	UNICODE_STRING DeviceInterface;

	HANDLE  hProcessId;
	//
	// Process section data

    PKEVENT ProcessEvent;
    HANDLE  hParentId;
    BOOLEAN bCreate;
} DriverExample_DEVICE_EXTENSION, *PDriverExample_DEVICE_EXTENSION;



typedef struct _ProcessCallbackInfo
{
    HANDLE  hParentId;
    HANDLE  hProcessId;
    BOOLEAN bCreate;
} PROCESS_CALLBACK_INFO, *PPROCESS_CALLBACK_INFO;


bool init;

// {7ed6ed9e-870c-4a7d-b811-c819917bae3b}
static const GUID GUID_DriverExampleInterface = {0x7ED6ED9E, 0x870c, 0x4a7d, {0xb8, 0x11, 0xc8, 0x19, 0x91, 0x7b, 0xae, 0x3b } };

#ifdef __cplusplus
extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath);
#endif

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath)
{
	unsigned i;

	init = FALSE;
	
	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
		DriverObject->MajorFunction[i] = DriverExampleDefaultHandler;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverExampleCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverExampleCreateClose;
	DriverObject->MajorFunction[IRP_MJ_PNP] = DriverExamplePnP;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverExampleDispatchIoctl;


	DriverObject->DriverUnload = DriverExampleUnload;
	DriverObject->DriverStartIo = NULL;
	DriverObject->DriverExtension->AddDevice = DriverExampleAddDevice;
	
	//создаем нотифаер на создание нового процесса в системе
	PsSetCreateProcessNotifyRoutine(NotifyRoutine, FALSE);
	init = TRUE;
	DbgPrint("Driver Start!\n");
	return STATUS_SUCCESS;
}

void NotifyRoutine(IN HANDLE ParentId,IN HANDLE ProcessId,IN BOOLEAN Create)
{
	PDriverExample_DEVICE_EXTENSION extension;
	extension = (PDriverExample_DEVICE_EXTENSION)g_pDeviceObject->DeviceExtension;
	
    extension->hParentId  = ParentId;
    extension->hProcessId = ProcessId;
    extension->bCreate    = Create;
	
    KeSetEvent(extension->ProcessEvent, 0, FALSE);
    KeClearEvent(extension->ProcessEvent);
	if(Create)
	{
		DbgPrint("NotifyRoutine: a new process was created");
	}
	else
	{
		DbgPrint("NotifyRoutine: process was closed");
	}
}

void DriverExampleUnload(IN PDRIVER_OBJECT DriverObject)
{
	if(init) PsSetCreateProcessNotifyRoutine(NotifyRoutine, TRUE);
	init = FALSE;
	DbgPrint("Goodbye from DriverExample!\n");
}

NTSTATUS DriverExampleCreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS DriverExampleDefaultHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	PDriverExample_DEVICE_EXTENSION deviceExtension = NULL;
	
	IoSkipCurrentIrpStackLocation(Irp);
	deviceExtension = (PDriverExample_DEVICE_EXTENSION) DeviceObject->DeviceExtension;
	return IoCallDriver(deviceExtension->TargetDeviceObject, Irp);
}

NTSTATUS DriverExampleAddDevice(IN PDRIVER_OBJECT  DriverObject, IN PDEVICE_OBJECT  PhysicalDeviceObject)
{
	PDEVICE_OBJECT DeviceObject = NULL;
	PDriverExample_DEVICE_EXTENSION pExtension = NULL;
	NTSTATUS status;
	UNICODE_STRING uszDriverString;
	RtlInitUnicodeString(&uszDriverString, L"\\Device\\DriverExample");// Задача функции RtlInitUnicodeString измерить unicode-строку и заполнить 
	
	
	status = IoCreateDevice(DriverObject,
						    sizeof(DriverExample_DEVICE_EXTENSION),
							&uszDriverString,
							FILE_DEVICE_UNKNOWN,
							0,
							0,
							&DeviceObject);
	
	UNICODE_STRING uszDeviceString;

	RtlInitUnicodeString(&uszDeviceString, L"\\DosDevices\\DriverExample");
    //
	// Create symbolic link to the user-visible name

    status = IoCreateSymbolicLink(&uszDeviceString, &uszDriverString);

	if (!NT_SUCCESS(status))
		return status;

	g_pDeviceObject = DeviceObject; //Save Device object

	pExtension = (PDriverExample_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

	pExtension->DeviceObject = DeviceObject;
	pExtension->PhysicalDeviceObject = PhysicalDeviceObject;
	pExtension->TargetDeviceObject = IoAttachDeviceToDeviceStack(DeviceObject, PhysicalDeviceObject);

	status = IoRegisterDeviceInterface(PhysicalDeviceObject, &GUID_DriverExampleInterface, NULL, &pExtension->DeviceInterface);
	ASSERT(NT_SUCCESS(status));

	UNICODE_STRING uszProcessEventString;
	HANDLE hProcessHandle;

	RtlInitUnicodeString(
		&uszProcessEventString, 
		L"\\BaseNamedObjects\\ProcessEvent"
		);
    pExtension->ProcessEvent = IoCreateNotificationEvent(
		&uszProcessEventString, 
		&hProcessHandle
		);
	//
    // Clear it out
	//
    KeClearEvent(pExtension->ProcessEvent);

	DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
	return STATUS_SUCCESS;
}


NTSTATUS DriverExampleIrpCompletion(
					  IN PDEVICE_OBJECT DeviceObject,
					  IN PIRP Irp,
					  IN PVOID Context
					  )
{
	PKEVENT Event = (PKEVENT) Context;

	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);

	KeSetEvent(Event, IO_NO_INCREMENT, FALSE);

	return(STATUS_MORE_PROCESSING_REQUIRED);
}

NTSTATUS DriverExampleForwardIrpSynchronous(
							  IN PDEVICE_OBJECT DeviceObject,
							  IN PIRP Irp
							  )
{
	PDriverExample_DEVICE_EXTENSION   deviceExtension;
	KEVENT event;
	NTSTATUS status;

	KeInitializeEvent(&event, NotificationEvent, FALSE);
	deviceExtension = (PDriverExample_DEVICE_EXTENSION) DeviceObject->DeviceExtension;

	IoCopyCurrentIrpStackLocationToNext(Irp);

	IoSetCompletionRoutine(Irp, DriverExampleIrpCompletion, &event, TRUE, TRUE, TRUE);

	status = IoCallDriver(deviceExtension->TargetDeviceObject, Irp);

	if (status == STATUS_PENDING) {
		KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
		status = Irp->IoStatus.Status;
	}
	return status;
}

NTSTATUS DriverExamplePnP(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	PDriverExample_DEVICE_EXTENSION pExt = ((PDriverExample_DEVICE_EXTENSION)DeviceObject->DeviceExtension);
	NTSTATUS status;

	ASSERT(pExt);

	switch (irpSp->MinorFunction)
	{
	case IRP_MN_START_DEVICE:
		IoSetDeviceInterfaceState(&pExt->DeviceInterface, TRUE);
		Irp->IoStatus.Status = STATUS_SUCCESS;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;

	case IRP_MN_QUERY_REMOVE_DEVICE:
		Irp->IoStatus.Status = STATUS_SUCCESS;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;

	case IRP_MN_REMOVE_DEVICE:
		IoSetDeviceInterfaceState(&pExt->DeviceInterface, FALSE);
		status = DriverExampleForwardIrpSynchronous(DeviceObject, Irp);
		IoDetachDevice(pExt->TargetDeviceObject);
		IoDeleteDevice(pExt->DeviceObject);
		RtlFreeUnicodeString(&pExt->DeviceInterface);
		Irp->IoStatus.Status = STATUS_SUCCESS;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;

	case IRP_MN_QUERY_PNP_DEVICE_STATE:
		status = DriverExampleForwardIrpSynchronous(DeviceObject, Irp);
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}
	return DriverExampleDefaultHandler(DeviceObject, Irp);
}

NTSTATUS DriverExampleDispatchIoctl(
	IN PDEVICE_OBJECT DeviceObject, 
	IN PIRP           Irp
	)
{
    NTSTATUS               ntStatus = STATUS_UNSUCCESSFUL;
    PIO_STACK_LOCATION     irpStack  = IoGetCurrentIrpStackLocation(Irp);//получения указателя на стек IRP пакета используется функция:
    PDriverExample_DEVICE_EXTENSION      extension = (PDriverExample_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PPROCESS_CALLBACK_INFO pProcCallbackInfo;
	//
    // These IOCTL handlers are the set and get interfaces between
    // the driver and the user mode app

    switch(irpStack->Parameters.DeviceIoControl.IoControlCode)//получение управляющего IOCTL кода;
    {
		case IOCTL_PROCOBSRV_GET_PROCINFO:
			{
				if (irpStack->Parameters.DeviceIoControl.OutputBufferLength >=// получение размера буфера которое ожидает приложение
				   sizeof(PROCESS_CALLBACK_INFO))
				{
					pProcCallbackInfo = (PPROCESS_CALLBACK_INFO)Irp->AssociatedIrp.SystemBuffer;//получение переданного буфера
					pProcCallbackInfo->hParentId  = extension->hParentId;
					pProcCallbackInfo->hProcessId = extension->hProcessId;
					pProcCallbackInfo->bCreate    = extension->bCreate;
    
					ntStatus = STATUS_SUCCESS;
				}
				break;
			}

        default:
            break;
    }

    Irp->IoStatus.Status = ntStatus;
	//
    // Set number of bytes to copy back to user-mode

    if(ntStatus == STATUS_SUCCESS)
        Irp->IoStatus.Information = //количество переданных байт.
			irpStack->Parameters.DeviceIoControl.OutputBufferLength;
    else
        Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return ntStatus;
}

