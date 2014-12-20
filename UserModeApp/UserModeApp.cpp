// UserModeApp.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"


#define FILE_DEVICE_UNKNOWN             0x00000022
#define IOCTL_UNKNOWN_BASE              FILE_DEVICE_UNKNOWN
#define IOCTL_PROCOBSRV_GET_PROCINFO    \
	CTL_CODE(IOCTL_UNKNOWN_BASE, 0x0801, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)


typedef struct _ProcessCallbackInfo
{
    DWORD  hParentId;
    DWORD  hProcessId;
    BOOLEAN bCreate;
} PROCESS_CALLBACK_INFO, *PPROCESS_CALLBACK_INFO;


HANDLE  hDriverFile;
HANDLE	hEvent;

int _tmain(int argc, _TCHAR* argv[])
{
	hDriverFile = CreateFile(
			TEXT("\\\\.\\DriverExample"),
			GENERIC_READ | GENERIC_WRITE, 
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			0,                     // Default security
			OPEN_EXISTING,
			0,                     // Perform synchronous I/O
			0);                    // No template

	hEvent = OpenEvent(SYNCHRONIZE, FALSE, TEXT("ProcessEvent") );
	if (hEvent == INVALID_HANDLE_VALUE)
	{
		std::cout << "Error open event";
		return 0;
	}

	OVERLAPPED  ov          = { 0 };
	DWORD       dwBytesReturned;
	PROCESS_CALLBACK_INFO  callbackInfo;
	TCHAR szFileName[MAX_PATH];
	while (true)
	{
		SwitchToThread(); //Переключитесь На Нить
		WaitForSingleObject (hEvent,INFINITE);
		DeviceIoControl(//Функция DeviceIoControl отправляет управляющий код непосредственно указанному драйверу устройства, заставляя соответствующее устройство выполнить соответствующую операцию.
			hDriverFile,
			IOCTL_PROCOBSRV_GET_PROCINFO,
			0, 
			0,
			&callbackInfo, sizeof(callbackInfo),
			&dwBytesReturned,
			&ov
			);

		
		GetOverlappedResult( //будет дожидаться завершения выполнения операции 
			hDriverFile, 
			&ov,
			&dwBytesReturned, 
			TRUE
			);

			HANDLE Handle = OpenProcess(
				PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
				FALSE,
				callbackInfo.hProcessId 
			);
			
			GetModuleFileNameEx(Handle, 0, szFileName, MAX_PATH);
			
            CloseHandle(Handle);

		/*if (wcscmp (szFileName,ProcessName)== 0) //Функция wcscmp является эквивалентом функции strcmp для широких символов. Она сравнивает строку широких символов, на которую указывает s1, со строкой широких символов, на которую указывает s2.  
		{
			if (callbackInfo.bCreate == true)
			{
				MessageBoxA (0,"Калькулятор запущен","",0);
				STARTUPINFO si = { sizeof(si) };
				PROCESS_INFORMATION pi; 
				CreateProcess(NULL, TEXT("WINMINE"), NULL, NULL, FALSE, 0, NULL. NULL, &si, &pi);
				
			}
			else
			{
				MessageBoxA (0,"Калькулятор закрыт","",0);
			}
		}*/
		std::cout << szFileName << " Created";
	}
	return 0;
}

