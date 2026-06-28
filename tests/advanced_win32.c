#include <windows.h>

HANDLE g_mutex = NULL;
DWORD g_shared_counter = 0;

DWORD WINAPI ThreadFunc(LPVOID lpParam) {
    DWORD id = (DWORD)lpParam;
    
    // Test Heap
    HANDLE hHeap = GetProcessHeap();
    char* buf = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 64);
    if (buf) {
        // Just write something
        buf[0] = 'H';
        HeapFree(hHeap, 0, buf);
    }
    
    // Test Mutex
    WaitForSingleObject(g_mutex, INFINITE);
    g_shared_counter += id;
    ReleaseMutex(g_mutex);
    
    ExitThread(id);
    return id;
}

void _start(void) {
    DWORD written = 0;
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    
    WriteFile(hStdout, "Advanced Win32 Test Starting...\n", 32, &written, NULL);

    // Test Registry
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\MacWITest", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = 42;
        RegSetValueExA(hKey, "TestValue", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
        WriteFile(hStdout, "Registry Write Success\n", 23, &written, NULL);
    }
    
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\MacWITest", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val = 0;
        DWORD size = sizeof(val);
        DWORD type = 0;
        RegQueryValueExA(hKey, "TestValue", NULL, &type, (LPBYTE)&val, &size);
        RegCloseKey(hKey);
        if (val == 42) {
            WriteFile(hStdout, "Registry Read Success\n", 22, &written, NULL);
        }
    }

    // Test Mutex & Threading
    g_mutex = CreateMutexA(NULL, FALSE, "MacWIMutex");
    
    HANDLE hThreads[2];
    DWORD tid[2];
    hThreads[0] = CreateThread(NULL, 0, ThreadFunc, (LPVOID)10, 0, &tid[0]);
    hThreads[1] = CreateThread(NULL, 0, ThreadFunc, (LPVOID)20, 0, &tid[1]);
    
    WaitForSingleObject(hThreads[0], INFINITE);
    WaitForSingleObject(hThreads[1], INFINITE);
    
    CloseHandle(hThreads[0]);
    CloseHandle(hThreads[1]);
    CloseHandle(g_mutex);
    
    if (g_shared_counter == 30) {
        WriteFile(hStdout, "Threading Success\n", 18, &written, NULL);
    }

    WriteFile(hStdout, "Advanced Win32 Test Finished\n", 29, &written, NULL);
    ExitProcess(0);
}
