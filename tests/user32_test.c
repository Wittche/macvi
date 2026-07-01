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

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CLOSE:
            print_out("Received WM_CLOSE!\n");
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            print_out("Received WM_DESTROY!\n");
            PostQuitMessage(0);
            break;
        case WM_LBUTTONDOWN:
            print_out("Mouse clicked! x=");
            print_int(LOWORD(lParam));
            print_out(", y=");
            print_int(HIWORD(lParam));
            print_out("\n");
            break;
        case WM_KEYDOWN:
            print_out("Key down! vk=");
            print_int(wParam);
            print_out("\n");
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void _start() {
    print_out("Starting user32_test...\n");

    WNDCLASSEXA wc;
    HWND hwnd;
    MSG Msg;

    wc.cbSize        = sizeof(WNDCLASSEXA);
    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hIcon         = NULL;
    wc.hCursor       = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = "myWindowClass";
    wc.hIconSm       = NULL;

    if(!RegisterClassExA(&wc)) {
        print_out("Window Registration Failed!\n");
        ExitProcess(0);
    }

    hwnd = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "myWindowClass",
        "MacWI Window Test",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, wc.hInstance, NULL);

    if(hwnd == NULL) {
        print_out("Window Creation Failed!\n");
        ExitProcess(0);
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    print_out("Window created, entering message loop...\n");

    while(GetMessage(&Msg, NULL, 0, 0) > 0) {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }

    print_out("user32_test finished successfully.\n");
    ExitProcess(Msg.wParam);
}
