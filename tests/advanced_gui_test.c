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

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_PAINT) {
        print_out("[WindowProc] Received WM_PAINT!\n");
        return 0;
    } else if (uMsg == WM_CLOSE) {
        print_out("[WindowProc] Received WM_CLOSE!\n");
        MessageBoxA(hwnd, "Are you sure you want to quit?", "MacWI Advanced GUI", 1); // 1 = MB_OKCANCEL
        PostQuitMessage(0);
        return 0;
    }
    
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void _start(void) {
    print_out("=== MacWI Advanced GUI Test ===\n");
    
    const char* class_name = "MacWI_Advanced_Class";
    
    WNDCLASSEXA wc;
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = 0;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hIcon = NULL;
    wc.hCursor = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = class_name;
    wc.hIconSm = NULL;
    
    print_out("Registering Window Class...\n");
    if (!RegisterClassExA(&wc)) {
        print_out("Failed to register window class!\n");
        ExitProcess(1);
    }
    
    print_out("Creating Window...\n");
    HWND hwnd = CreateWindowExA(
        0, class_name, "MacWI Phase 15", 
        WS_OVERLAPPEDWINDOW,
        100, 100, 800, 600,
        NULL, NULL, wc.hInstance, NULL
    );
    
    if (!hwnd) {
        print_out("Failed to create window!\n");
        ExitProcess(1);
    }
    
    print_out("Window Created! HWND: ");
    print_int((int)hwnd);
    print_out("\n");
    
    // Test SetWindowTextA
    SetWindowTextA(hwnd, "MacWI Phase 15 - Callbacks Active!");
    
    // Test MessageBoxA
    MessageBoxA(NULL, "Welcome to the MacWI Advanced GUI Test.\nThe window will now show.", "Welcome", 0);
    
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    
    // Message Loop
    MSG msg;
    print_out("Entering Message Loop...\n");
    while (GetMessageA(&msg, NULL, 0, 0)) {
        if (msg.message != 0) {
            print_out("Dispatching message: ");
            print_int(msg.message);
            print_out("\n");
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    
    print_out("Exiting...\n");
    ExitProcess(0);
}
