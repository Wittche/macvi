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
        print_out("[WindowProc] Received WM_PAINT! Drawing GDI elements...\n");
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        // Fill background with grey
        HBRUSH bgBrush = CreateSolidBrush(0x00D0D0D0); // Grey
        RECT bgRect = {0, 0, 800, 600};
        FillRect(hdc, &bgRect, bgBrush);
        DeleteObject(bgBrush);
        
        // Draw a red rectangle
        HBRUSH redBrush = CreateSolidBrush(0x000000FF); // Red (BBGGRR)
        HBRUSH oldBrush = SelectObject(hdc, redBrush);
        Rectangle(hdc, 50, 50, 250, 200);
        
        // Draw a blue rectangle
        HBRUSH blueBrush = CreateSolidBrush(0x00FF0000); // Blue (BBGGRR)
        SelectObject(hdc, blueBrush);
        Rectangle(hdc, 300, 50, 500, 200);
        
        SelectObject(hdc, oldBrush);
        DeleteObject(redBrush);
        DeleteObject(blueBrush);
        
        // Draw Text
        const char* text1 = "Hello from MacWI GDI!";
        TextOutA(hdc, 50, 250, text1, lstrlenA(text1));
        
        const char* text2 = "CoreGraphics backend is working synchronously.";
        TextOutA(hdc, 50, 280, text2, lstrlenA(text2));
        
        EndPaint(hwnd, &ps);
        return 0;
    } else if (uMsg == WM_CLOSE) {
        print_out("[WindowProc] Received WM_CLOSE!\n");
        PostQuitMessage(0);
        return 0;
    }
    
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void _start(void) {
    print_out("=== MacWI Phase 16: GDI32 Test ===\n");
    
    const char* class_name = "MacWI_GDI_Class";
    
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
    
    if (!RegisterClassExA(&wc)) {
        print_out("Failed to register window class!\n");
        ExitProcess(1);
    }
    
    HWND hwnd = CreateWindowExA(
        0, class_name, "MacWI Phase 16 - GDI Drawing", 
        WS_OVERLAPPEDWINDOW,
        100, 100, 800, 600,
        NULL, NULL, wc.hInstance, NULL
    );
    
    if (!hwnd) {
        print_out("Failed to create window!\n");
        ExitProcess(1);
    }
    
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    
    MSG msg;
    print_out("Entering Message Loop...\n");
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    
    print_out("Exiting...\n");
    ExitProcess(0);
}
