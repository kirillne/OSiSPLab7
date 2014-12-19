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

void DriverExampleUnload(IN PDRIVER_OBJECT DriverObject);
NTSTATUS DriverExampleCreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS DriverExampleDefaultHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS DriverExampleAddDevice(IN PDRIVER_OBJECT  DriverObject, IN PDEVICE_OBJECT  PhysicalDeviceObject);
NTSTATUS DriverExamplePnP(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
void NotifyRoutine(IN HANDLE ParentId,IN HANDLE ProcessId,IN BOOLEAN Create);

typedef struct _deviceExtension
{
	PDEVICE_OBJECT DeviceObject;
	PDEVICE_OBJECT TargetDeviceObject;
	PDEVICE_OBJECT PhysicalDeviceObject;
	UNICODE_STRING DeviceInterface;
} DriverExample_DEVICE_EXTENSION, *PDriverExample_DEVICE_EXTENSION;

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

	DriverObject->DriverUnload = DriverExampleUnload;
	DriverObject->DriverStartIo = NULL;
	DriverObject->DriverExtension->AddDevice = DriverExampleAddDevice;
	
	//создаем нотифаер на создание нового процесса в системе
	PsSetCreateProcessNotifyRoutine(NotifyRoutine, FALSE);
	init = TRUE;
	DbgPrint("Driver Start!\n");
	return STATUS_SUCCESS;
}
void GetProcessEXEName(ULONG PID)
{
    PSECTION_OBJECT                     sec1;
    PFILE_OBJECT                        fobj;
   // UNICODE_STRING                      devname;
   // ULONG                               bytesIO;
   // WCHAR                               textbuf[260];
    NTSTATUS st;
    PEPROCESS Process;
    ULONG retLen;
    PUNICODE_STRING us = NULL;

 st = PsLookupProcessByProcessId((HANDLE)PID, &Process);

  if (!NT_SUCCESS(st)) return;

    sec1 = Process->SectionObject;

    if ( MmIsAddressValid(sec1) )
    {
             DbgPrint("sec1 ok...\n");
        if ( MmIsAddressValid(sec1->Segment) )

        {
             DbgPrint("sec1->Segment ok...\n");
            if ( MmIsAddressValid( ((PSEGMENT)sec1->Segment)->ControlArea ) )

            {
                DbgPrint("sec1->Segment)->ControlArea ok...\n");
                fobj = ((PSEGMENT)sec1->Segment)->ControlArea->FilePointer;

                if ( !MmIsAddressValid(fobj) ) return;
else

 DbgPrint("sec1->Segment)->ControlArea->FilePointer ok...\n");

st = ObQueryNameString(fobj,
                  (POBJECT_NAME_INFORMATION)us,
                  0,
                  &retLen);

if (st != STATUS_INFO_LENGTH_MISMATCH) return;

  us = ExAllocatePoolWithTag(NonPagedPool, retLen, 'blah');

 if (!us) return;

st = ObQueryNameString(fobj, 
                   (POBJECT_NAME_INFORMATION)us, 
                   retLen, 
                  &retLen);

if (NT_SUCCESS(st)) {

 DbgPrint("%wZ\n", us);

}

ExFreePoolWithTag(us, 'blah');

  // ObQueryNameString(fobj, (POBJECT_NAME_INFORMATION)&textbuf, 260 * sizeof(WCHAR), &bytesIO);

  // DbgPrint("%ws\n", textbuf);

}
}
}
  ObDereferenceObject(Process);
}
bool CheckProcessName(HANDLE ProcessId)
{
	return 0;
}

void NotifyRoutine(IN HANDLE ParentId,IN HANDLE ProcessId,IN BOOLEAN Create)
{
	
	if(Create)
	{
		DbgPrint("NotifyRoutine: a new process was created");
		DbgPrint(GetProcessEXEName(ProcessId));
	}
	else
	{
		DbgPrint("NotifyRoutine: process was closed");
		DbgPrint(GetProcessEXEName(ProcessId));
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
	
	status = IoCreateDevice(DriverObject,
						    sizeof(DriverExample_DEVICE_EXTENSION),
							NULL,
							FILE_DEVICE_UNKNOWN,
							0,
							0,
							&DeviceObject);

	if (!NT_SUCCESS(status))
		return status;

	pExtension = (PDriverExample_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

	pExtension->DeviceObject = DeviceObject;
	pExtension->PhysicalDeviceObject = PhysicalDeviceObject;
	pExtension->TargetDeviceObject = IoAttachDeviceToDeviceStack(DeviceObject, PhysicalDeviceObject);

	status = IoRegisterDeviceInterface(PhysicalDeviceObject, &GUID_DriverExampleInterface, NULL, &pExtension->DeviceInterface);
	ASSERT(NT_SUCCESS(status));

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
