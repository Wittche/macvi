#include <windows.h>

HANDLE g_event;

DWORD WINAPI ThreadFunc(LPVOID lpParam) {
    DWORD written;
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    
    char* msg1 = "[Guest] Thread started. Waiting for event...\n";
    WriteFile(hStdout, msg1, lstrlenA(msg1), &written, NULL);
    
    WaitForSingleObject(g_event, INFINITE);
    
    char* msg2 = "[Guest] Event signaled! Thread exiting.\n";
    WriteFile(hStdout, msg2, lstrlenA(msg2), &written, NULL);
    return 0;
}

void _start(void) {
    DWORD written;
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    
    char* msg1 = "[Guest] Starting sync_test...\n";
    WriteFile(hStdout, msg1, lstrlenA(msg1), &written, NULL);

    g_event = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (!g_event) {
        char* msgE = "[Guest] Failed to create event!\n";
        WriteFile(hStdout, msgE, lstrlenA(msgE), &written, NULL);
        ExitProcess(1);
    }

    DWORD tid;
    HANDLE hThread = CreateThread(NULL, 0, ThreadFunc, NULL, 0, &tid);
    
    char* msg2 = "[Guest] Thread created. Sleeping 1 seconds...\n";
    WriteFile(hStdout, msg2, lstrlenA(msg2), &written, NULL);
    
    Sleep(1000);
    
    char* msg3 = "[Guest] Setting event...\n";
    WriteFile(hStdout, msg3, lstrlenA(msg3), &written, NULL);
    
    SetEvent(g_event);
    
    char* msg4 = "[Guest] Waiting for thread to exit...\n";
    WriteFile(hStdout, msg4, lstrlenA(msg4), &written, NULL);
    
    WaitForSingleObject(hThread, INFINITE);
    
    char* msg5 = "[Guest] Success!\n";
    WriteFile(hStdout, msg5, lstrlenA(msg5), &written, NULL);

    ExitProcess(0);
}
