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

void __start() {
    print("=== VFS Test ===\n");
    
    // Test CreateFileA for writing
    HANDLE hFile = CreateFileA("C:\\Windows\\System32\\test_vfs.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        print_err("Failed to create file for writing!\n");
        ExitProcess(1);
    }
    
    const char* data = "Hello from MacWI VFS!\n";
    DWORD written = 0;
    DWORD data_len = 0;
    while(data[data_len]) data_len++;
    if (!WriteFile(hFile, data, data_len, &written, NULL)) {
        print_err("Failed to write to file!\n");
        ExitProcess(1);
    }
    print("Wrote to C:\\Windows\\System32\\test_vfs.txt successfully.\n");
    
    // Test CreateFileA for reading
    HANDLE hRead = CreateFileA("c:\\windows\\system32\\TEST_VFS.TXT", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hRead == INVALID_HANDLE_VALUE) {
        print_err("Failed to open file for reading (case-insensitivity failed)!\n");
        ExitProcess(1);
    }
    
    char buf[128];
    DWORD read_bytes = 0;
    // We will test ReadFile.
    if (!ReadFile(hRead, buf, sizeof(buf)-1, &read_bytes, NULL)) {
        print_err("Failed to read from file!\n");
        ExitProcess(1);
    }
    buf[read_bytes] = '\0';
    
    print("Read from file: ");
    print(buf);
    
    print("VFS Test passed!\n");
    ExitProcess(0);
}
