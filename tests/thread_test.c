#include <windows.h>

void print_out(const char* msg) {
    DWORD written = 0;
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    WriteFile(hStdout, msg, lstrlenA(msg), &written, NULL);
}

void print_int(int val) {
    char buf[16];
    int i = 0;
    if (val == 0) { buf[i++] = '0'; }
    else {
        if (val < 0) { buf[i++] = '-'; val = -val; }
        char tmp[16]; int j = 0;
        while (val > 0) { tmp[j++] = '0' + (val % 10); val /= 10; }
        while (j > 0) { buf[i++] = tmp[--j]; }
    }
    buf[i] = '\0';
    print_out(buf);
}

// Global data to test synchronization
int g_counter = 0;
HANDLE g_mutex;

DWORD WINAPI ThreadProc(LPVOID lpParam) {
    print_out("  [Child] Thread started.\n");

    WaitForSingleObject(g_mutex, INFINITE);
    print_out("  [Child] Acquired mutex.\n");
    g_counter += 10;
    ReleaseMutex(g_mutex);
    print_out("  [Child] Released mutex. Counter += 10\n");

    return 42;
}

void _start(void) {
    print_out("=== MacWI Threading Test ===\n\n");

    // 1. Create Mutex
    print_out("1. Creating mutex...\n");
    g_mutex = CreateMutexA(NULL, FALSE, "TestMutex");
    if (g_mutex == NULL) {
        print_out("   FAILED to create mutex!\n");
        ExitProcess(1);
    }
    print_out("   Mutex created successfully.\n\n");

    // 2. Create Thread
    print_out("2. Creating child thread...\n");
    DWORD threadId;
    HANDLE hThread = CreateThread(NULL, 0, ThreadProc, NULL, 0, &threadId);
    if (hThread == NULL) {
        print_out("   FAILED to create thread!\n");
        ExitProcess(1);
    }
    print_out("   Thread created. ID: ");
    print_int(threadId);
    print_out("\n\n");

    // 3. Parent thread updates counter
    print_out("3. [Parent] Acquiring mutex...\n");
    WaitForSingleObject(g_mutex, INFINITE);
    g_counter += 5;
    print_out("   [Parent] Counter += 5\n");
    ReleaseMutex(g_mutex);
    print_out("   [Parent] Released mutex.\n\n");

    // 4. Wait for child thread
    print_out("4. Waiting for child thread to finish...\n");
    WaitForSingleObject(hThread, INFINITE);
    print_out("   Child thread joined.\n\n");

    // 5. Verify
    print_out("5. Final counter value: ");
    print_int(g_counter);
    print_out("\n");
    if (g_counter == 15) {
        print_out("   SUCCESS: Counter is 15. Synchronization works!\n");
    } else {
        print_out("   FAILED: Counter is wrong!\n");
    }

    print_out("\n=== Threading Test Completed ===\n");

    CloseHandle(g_mutex);
    CloseHandle(hThread);

    ExitProcess(0);
}
