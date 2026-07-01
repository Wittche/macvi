#include <windows.h>
#include <stdio.h>

void print(const char* str) {
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    DWORD len = 0;
    while(str[len]) len++;
    WriteFile(hStdout, str, len, &written, NULL);
}

void print_err(const char* str) {
    HANDLE hStderr = GetStdHandle(STD_ERROR_HANDLE);
    DWORD written;
    DWORD len = 0;
    while(str[len]) len++;
    WriteFile(hStderr, str, len, &written, NULL);
}

void print_hex(DWORD val) {
    char buf[16];
    const char* hex = "0123456789ABCDEF";
    int i = 7;
    buf[8] = '\0';
    while (i >= 0) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
        i--;
    }
    print("0x");
    print(buf);
}

void __start() {
    print("=== Memory Management Test ===\n");
    
    // 1. Test VirtualAlloc
    print("Testing VirtualAlloc...\n");
    void* mem = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem) {
        print_err("VirtualAlloc failed!\n");
        ExitProcess(1);
    }
    print("Allocated 4096 bytes at ");
    print_hex((DWORD)mem);
    print("\n");
    
    // 2. Test memory read/write
    print("Testing memory read/write...\n");
    char* str = (char*)mem;
    str[0] = 'M';
    str[1] = 'A';
    str[2] = 'C';
    str[3] = 'W';
    str[4] = 'I';
    str[5] = '\0';
    print("Wrote to memory. Read back: ");
    print(str);
    print("\n");
    
    // 3. Test VirtualProtect
    print("Testing VirtualProtect...\n");
    DWORD old_protect;
    if (!VirtualProtect(mem, 4096, PAGE_READONLY, &old_protect)) {
        print_err("VirtualProtect failed!\n");
        ExitProcess(1);
    }
    print("Changed protection to PAGE_READONLY. Old protect: ");
    print_hex(old_protect);
    print("\n");
    
    // 4. Test VirtualQuery
    print("Testing VirtualQuery...\n");
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(mem, &mbi, sizeof(mbi)) != sizeof(mbi)) {
        print_err("VirtualQuery failed!\n");
        ExitProcess(1);
    }
    print("VirtualQuery successful. BaseAddress: ");
    print_hex((DWORD)mbi.BaseAddress);
    print("\n");
    
    // 5. Test VirtualFree
    print("Testing VirtualFree...\n");
    if (!VirtualFree(mem, 0, MEM_RELEASE)) {
        print_err("VirtualFree failed!\n");
        ExitProcess(1);
    }
    
    print("Memory Management Test passed!\n");
    ExitProcess(0);
}
