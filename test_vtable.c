#include <windows.h>
#include <d3d9.h>
#include <stdio.h>

int main() {
    IDirect3DDevice9Vtbl vtbl;
    printf("Release: %zu\n", ((size_t)&vtbl.Release - (size_t)&vtbl) / sizeof(void*));
    printf("Present: %zu\n", ((size_t)&vtbl.Present - (size_t)&vtbl) / sizeof(void*));
    printf("Clear: %zu\n", ((size_t)&vtbl.Clear - (size_t)&vtbl) / sizeof(void*));
    printf("BeginScene: %zu\n", ((size_t)&vtbl.BeginScene - (size_t)&vtbl) / sizeof(void*));
    printf("EndScene: %zu\n", ((size_t)&vtbl.EndScene - (size_t)&vtbl) / sizeof(void*));
    return 0;
}
