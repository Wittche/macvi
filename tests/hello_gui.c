#include <windows.h>

void __stdcall WinMainCRTStartup(void) {
    // 1. Register Window Class
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = DefWindowProcA; // Default proc is enough for simple show
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "MacWITestClass";

    RegisterClassExA(&wc);

    // 2. Create Window
    HWND hwnd = CreateWindowExA(
        0,                              // Optional window styles
        "MacWITestClass",               // Window class
        "Hello from MacWI GUI!",        // Window text
        WS_OVERLAPPEDWINDOW,            // Window style
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, // Size and position
        NULL,       // Parent window    
        NULL,       // Menu
        wc.hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (hwnd == NULL) {
        ExitProcess(1);
    }

    // 3. Show Window
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // 4. Run the message loop
    MSG msg = {0};
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    ExitProcess((UINT)msg.wParam);
}
