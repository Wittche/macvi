#include <windows.h>

void print_out(const char* str) {
    DWORD written = 0;
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    WriteFile(hStdout, str, lstrlenA(str), &written, NULL);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Fill background white
            RECT rect;
            rect.left = 0;
            rect.top = 0;
            rect.right = 800;
            rect.bottom = 600;
            HBRUSH hBrush = (HBRUSH)1; // White brush mapped internally
            FillRect(hdc, &rect, hBrush);
            
            // Draw text
            TextOutA(hdc, 50, 50, "Hello, MacWI GUI World!", 23);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
        case WM_CLOSE: {
            print_out("Window closed. Exiting...\n");
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void _start(void) {
    print_out("Starting MacWI GUI Test...\n");

    HINSTANCE hInstance = GetModuleHandle(NULL);
    
    WNDCLASSEXA wc;
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = 0;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = NULL;
    wc.hCursor = NULL;
    wc.hbrBackground = (HBRUSH)1;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "MacWIGUIClass";
    wc.hIconSm = NULL;

    if (!RegisterClassExA(&wc)) {
        print_out("RegisterClassExA failed!\n");
        ExitProcess(1);
    }

    HWND hwnd = CreateWindowExA(
        0,
        "MacWIGUIClass",
        "MacWI First Window",
        0, // dwStyle
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hwnd) {
        print_out("CreateWindowExA failed!\n");
        ExitProcess(1);
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        if (msg.message == WM_PAINT || msg.message == WM_CLOSE || msg.message == WM_DESTROY) {
            // For this minimal test, since DispatchMessage cannot easily call WindowProc from host context,
            // we will manually call WindowProc here in the guest!
            WindowProc(msg.hwnd, msg.message, msg.wParam, msg.lParam);
        } else {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    print_out("Message loop ended.\n");
    ExitProcess(0);
}
