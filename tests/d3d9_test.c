#include <windows.h>
#include <d3d9.h>

void print_out(const char* msg) {
    DWORD written = 0;
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    WriteFile(hStdout, msg, lstrlenA(msg), &written, NULL);
}

void _start(void) {
    print_out("d3d9_test: Starting up...\n");

    HWND hwnd = CreateWindowExA(0, "STATIC", "D3D9 to Metal Test",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                100, 100, 800, 600,
                                NULL, NULL, GetModuleHandle(NULL), NULL);

    if (!hwnd) {
        print_out("d3d9_test: Failed to create window.\n");
        ExitProcess(1);
    }

    print_out("d3d9_test: Creating IDirect3D9...\n");
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) {
        print_out("d3d9_test: Direct3DCreate9 failed.\n");
        ExitProcess(1);
    }

    D3DPRESENT_PARAMETERS d3dpp;
    // ZeroMemory without CRT
    char* p = (char*)&d3dpp;
    for(int i=0; i<sizeof(d3dpp); i++) p[i] = 0;
    
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hwnd;

    print_out("d3d9_test: Creating IDirect3DDevice9...\n");
    IDirect3DDevice9* d3dDevice = NULL;
    HRESULT hr = d3d->lpVtbl->CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                           D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                           &d3dpp, &d3dDevice);

    if (FAILED(hr) || !d3dDevice) {
        print_out("d3d9_test: CreateDevice failed.\n");
        ExitProcess(1);
    }

    print_out("d3d9_test: Entering render loop...\n");
    for (int i = 0; i < 60; ++i) { // Run for a short while
        // Clear screen to Blue (ARGB)
        d3dDevice->lpVtbl->Clear(d3dDevice, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 255), 1.0f, 0);

        d3dDevice->lpVtbl->BeginScene(d3dDevice);
        // Draw things here in the future
        d3dDevice->lpVtbl->EndScene(d3dDevice);

        d3dDevice->lpVtbl->Present(d3dDevice, NULL, NULL, NULL, NULL);
        
        Sleep(16); // ~60fps
    }

    print_out("d3d9_test: Cleaning up...\n");
    d3dDevice->lpVtbl->Release(d3dDevice);
    d3d->lpVtbl->Release(d3d);
    
    print_out("d3d9_test: Done.\n");
    ExitProcess(0);
}
