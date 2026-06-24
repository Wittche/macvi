#include <windows.h>
#include <stdio.h>

void print(const char* msg) {
    OutputDebugStringA(msg);
}

// Global data to test synchronization
int g_counter = 0;
HANDLE g_mutex;

DWORD WINAPI ThreadProc(LPVOID lpParam) {
    print("Child thread started.\n");
    
    WaitForSingleObject(g_mutex, INFINITE);
    print("Child thread acquired mutex.\n");
    g_counter += 10;
    ReleaseMutex(g_mutex);
    print("Child thread released mutex.\n");
    
    return 42;
}

void __stdcall mainCRTStartup(void) {
    print("Thread Test Started.\n");
    
    g_mutex = CreateMutexA(NULL, FALSE, "TestMutex");
    if (g_mutex == NULL) {
        print("Failed to create mutex.\n");
        ExitProcess(1);
    }
    
    print("Mutex created successfully.\n");
    
    DWORD threadId;
    HANDLE hThread = CreateThread(NULL, 0, ThreadProc, NULL, 0, &threadId);
    if (hThread == NULL) {
        print("Failed to create thread.\n");
        ExitProcess(1);
    }
    
    print("Thread created successfully. Waiting for it...\n");
    
    // Parent thread updates counter as well
    WaitForSingleObject(g_mutex, INFINITE);
    g_counter += 5;
    ReleaseMutex(g_mutex);
    
    // Wait for child thread to finish
    WaitForSingleObject(hThread, INFINITE);
    
    print("Child thread joined.\n");
    
    if (g_counter == 15) {
        print("SUCCESS: Counter is 15. Synchronization works!\n");
    } else {
        print("FAILED: Counter is wrong.\n");
    }
    
    CloseHandle(g_mutex);
    CloseHandle(hThread);
    
    ExitProcess(0);
}
