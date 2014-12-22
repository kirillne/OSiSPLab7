// UserModeApp.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"
#include <map>


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
std::map< DWORD, HANDLE> openedProcesses;


BOOL EnableDebugPrivilege(BOOL bEnable)
{
    HANDLE hToken = NULL;
    LUID luid;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) return FALSE;
    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) return FALSE;

    TOKEN_PRIVILEGES tokenPriv;
    tokenPriv.PrivilegeCount = 1;
    tokenPriv.Privileges[0].Luid = luid;
    tokenPriv.Privileges[0].Attributes = bEnable ? SE_PRIVILEGE_ENABLED : 0;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tokenPriv, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) return FALSE;

    return TRUE;
}

int _tmain(int argc, _TCHAR* argv[])
{
	EnableDebugPrivilege(TRUE);
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
		Sleep(1000);
		GetModuleFileNameEx(Handle, 0, szFileName, MAX_PATH);
			

		if (!wcscmp(szFileName,TEXT("C:\\WINDOWS\\system32\\notepad.exe")))   
		{
			if (callbackInfo.bCreate == TRUE)
			{
				STARTUPINFO startupInfo;
				PROCESS_INFORMATION piProcess;
				ZeroMemory( &startupInfo, sizeof(startupInfo) );
				startupInfo.cb = sizeof(startupInfo);
				ZeroMemory( &piProcess, sizeof(piProcess) );
				SECURITY_ATTRIBUTES saProcess, saThread;
				saProcess.nLength = sizeof(saProcess);
				saProcess.lpSecurityDescriptor = NULL;
				saProcess.bInheritHandle = TRUE;
				saThread.nLength = sizeof(saThread);
				saThread.lpSecurityDescriptor = NULL;
				saThread.bInheritHandle = FALSE;
	
				TCHAR commandLine[] = L"";
				TCHAR applicationPath[] = L"C:\\Windows\\system32\\calc.exe";

				CreateProcess(applicationPath, commandLine, 
					&saProcess, &saThread, FALSE, 0, NULL, NULL, &startupInfo, &piProcess);
				openedProcesses.insert ( std::pair<DWORD,HANDLE>(callbackInfo.hProcessId ,piProcess.hProcess) );
				
			}
			else
			{

				std::map<DWORD,HANDLE>::iterator iter = openedProcesses.find(callbackInfo.hProcessId);
				if(iter != openedProcesses.end())
				{
					TerminateProcess(iter->second,0);
					openedProcesses.erase(iter);
				}
			}
		}
        CloseHandle(Handle);

		
	}
	return 0;
}

