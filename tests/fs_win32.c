#include <windows.h>

void print_out(const char* str) {
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written = 0;
    
    // Calculate string length manually since we have no stdlib
    const char* s = str;
    while(*s) s++;
    DWORD len = (DWORD)(s - str);
    
    WriteFile(hStdout, str, len, &written, NULL);
}

void print_int(DWORD val) {
    char buf[16];
    int i = 14;
    buf[15] = '\0';
    if (val == 0) {
        buf[i--] = '0';
    } else {
        while (val > 0 && i >= 0) {
            buf[i--] = '0' + (val % 10);
            val /= 10;
        }
    }
    print_out(&buf[i + 1]);
}

void print_hex(DWORD val) {
    char buf[16];
    int i = 14;
    buf[15] = '\0';
    if (val == 0) {
        buf[i--] = '0';
    } else {
        while (val > 0 && i >= 0) {
            DWORD rem = val % 16;
            buf[i--] = rem < 10 ? '0' + rem : 'A' + (rem - 10);
            val /= 16;
        }
    }
    print_out("0x");
    print_out(&buf[i + 1]);
}

void test_directory_api() {
    print_out("\n--- Testing Directory API ---\n");
    char buffer[MAX_PATH];
    
    GetSystemDirectoryA(buffer, MAX_PATH);
    print_out("System Directory: ");
    print_out(buffer);
    print_out("\n");
    
    GetWindowsDirectoryA(buffer, MAX_PATH);
    print_out("Windows Directory: ");
    print_out(buffer);
    print_out("\n");
}

void test_create_file() {
    print_out("\n--- Testing CreateFileA and WriteFile ---\n");
    char* filepath = "C:\\Windows\\System32\\macwi_test.txt";
    HANDLE hFile = CreateFileA(filepath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE) {
        print_out("FAILED to create file. Error: ");
        print_int(GetLastError());
        print_out("\n");
        return;
    }
    print_out("Successfully created file.\n");
    
    char* data = "Hello from MacWI Virtual File System!\n";
    DWORD bytesWritten = 0;
    DWORD len = 38; // length of string
    
    if (WriteFile(hFile, data, len, &bytesWritten, NULL)) {
        print_out("Successfully wrote bytes: ");
        print_int(bytesWritten);
        print_out("\n");
    } else {
        print_out("FAILED to write to file.\n");
    }
    CloseHandle(hFile);
    
    // Read back
    hFile = CreateFileA(filepath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        char buffer[100];
        DWORD bytesRead = 0;
        if (ReadFile(hFile, buffer, 99, &bytesRead, NULL)) {
            buffer[bytesRead] = '\0';
            print_out("Successfully read bytes: ");
            print_int(bytesRead);
            print_out(" -> ");
            print_out(buffer);
        }
        CloseHandle(hFile);
    }
}

void test_file_attributes() {
    print_out("\n--- Testing GetFileAttributesA ---\n");
    char* filepath = "C:\\Windows\\System32\\macwi_test.txt";
    DWORD attr = GetFileAttributesA(filepath);
    
    if (attr == INVALID_FILE_ATTRIBUTES) {
        print_out("FAILED to get attributes.\n");
    } else {
        print_out("Attributes: ");
        print_hex(attr);
        print_out("\n");
    }
}

void test_find_file() {
    print_out("\n--- Testing FindFirstFileA / FindNextFileA ---\n");
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA("C:\\Windows\\System32\\*", &fd);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        print_out("FAILED to find files. Error: ");
        print_int(GetLastError());
        print_out("\n");
        return;
    }
    
    print_out("Files found:\n");
    int count = 0;
    do {
        print_out("  ");
        print_out(fd.cFileName);
        print_out(" (Attr: ");
        print_hex(fd.dwFileAttributes);
        print_out(")\n");
        count++;
        if (count >= 10) {
            print_out("  ... (stopping at 10 to avoid spam)\n");
            break;
        }
    } while (FindNextFileA(hFind, &fd));
    
    FindClose(hFind);
    print_out("Total entries processed: ");
    print_int(count);
    print_out("\n");
}

void test_delete_file() {
    print_out("\n--- Testing DeleteFileA ---\n");
    char* filepath = "C:\\Windows\\System32\\macwi_test.txt";
    if (DeleteFileA(filepath)) {
        print_out("Successfully deleted file.\n");
    } else {
        print_out("FAILED to delete file.\n");
    }
}

void _start(void) {
    print_out("Starting MacWI File System Test...\n");
    
    test_directory_api();
    test_create_file();
    test_file_attributes();
    test_find_file();
    test_delete_file();
    
    print_out("\nAll tests completed.\n");
    ExitProcess(0);
}
