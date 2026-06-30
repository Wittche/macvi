#include <windows.h>
#include <stdio.h>

int main() {
    DWORD start = GetTickCount();
    for (int i = 0; i < 1000000; i++) {
        GetTickCount();
    }
    DWORD end = GetTickCount();
    printf("1,000,000 GetTickCount calls took: %lu ms\n", end - start);
    return 0;
}
