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
    print_out("[Guest] WindowProc called with uMsg: ");
    print_int(uMsg);
    
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        RECT rect;
        GetClientRect(hwnd, &rect);
        
        HBRUSH bg = CreateSolidBrush(0x00FF0000); // Blue
        FillRect(hdc, &rect, bg);
        DeleteObject(bg);
        
        TextOutA(hdc, 50, 50, "MacWI Input Test - Type or Click!", 33);
        
        EndPaint(hwnd, &ps);
        return 0;
    } else if (uMsg == WM_LBUTTONDOWN) {
        int x = lParam & 0xFFFF;
        int y = (lParam >> 16) & 0xFFFF;
        print_out("[Input] Mouse Left Clicked at X: ");
        print_int(x);
        print_out(" Y: ");
        print_int(y);
        print_out("\n");
        return 0;
    } else if (uMsg == WM_KEYDOWN) {
        print_out("[Input] Key Pressed! KeyCode: ");
        print_int(wParam);
        print_out("\n");
        return 0;
    } else if (uMsg == WM_CLOSE) {
        print_out("[Input] Closing Window...\n");
        PostQuitMessage(0);
        return 0;
    }
    
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void _start(void) {
    print_out("=== MacWI Input Subsystem Test ===\n");
    
    const char* class_name = "MacWI_Input_Class";
    
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
    
    RegisterClassExA(&wc);
    
    HWND hwnd = CreateWindowExA(
        0, class_name, "MacWI Input Test", 
        WS_OVERLAPPEDWINDOW,
        100, 100, 800, 600,
        NULL, NULL, wc.hInstance, NULL
    );
    
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    
    ExitProcess(0);
}
