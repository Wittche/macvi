#include <stdio.h>
int main() {
    int idx = 0;
    // callback
    idx++;
    // kernel32
    idx += 28;
    // user32
    idx += 20;
    // gdi32
    idx += 9;
    
    // ntdll starts here!
    printf("%d: NtCreateFile\n", idx++);
    printf("%d: NtClose\n", idx++);
    printf("%d: NtReadFile\n", idx++);
    printf("%d: NtWriteFile\n", idx++);
    printf("%d: NtAllocateVirtualMemory\n", idx++);
    printf("%d: NtFreeVirtualMemory\n", idx++);
    return 0;
}
