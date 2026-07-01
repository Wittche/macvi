#include <stdio.h>
int main() {
    int idx = 0;
    printf("%d: CallbackReturn\n", idx++);
    printf("%d: GetLastError\n", idx++);
    printf("%d: SetLastError\n", idx++);
    printf("%d: GetModuleHandleA\n", idx++);
    printf("%d: GetTickCount\n", idx++);
    printf("%d: Sleep\n", idx++);
    printf("%d: OutputDebugStringA\n", idx++);
    printf("%d: lstrlenA\n", idx++);
    printf("%d: GetStdHandle\n", idx++);
    printf("%d: CreateFileA\n", idx++);
    printf("%d: FindFirstFileA\n", idx++);
    printf("%d: FindNextFileA\n", idx++);
    printf("%d: FindClose\n", idx++);
    printf("%d: ReadFile\n", idx++);
    printf("%d: WriteFile\n", idx++);
    return 0;
}
