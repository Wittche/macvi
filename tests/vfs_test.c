#include <windows.h>
#include <stdio.h>

void print(const char* msg) {
    OutputDebugStringA(msg);
}

void __stdcall mainCRTStartup(void) {
    print("VFS Test Started.\n");
    
    // Windows absolute path
    const char* file_path = "C:\\Windows\\System32\\macwi_vfs_test.txt";
    
    // Write
    HANDLE hFile = CreateFileA(file_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        print("Failed to open file for writing.\n");
        ExitProcess(1);
    }
    
    const char* data = "Hello from MacWI VFS!\n";
    DWORD written = 0;
    WriteFile(hFile, data, lstrlenA(data), &written, NULL);
    CloseHandle(hFile);
    print("File written successfully to C:\\Windows\\System32.\n");
    
    // Read
    hFile = CreateFileA(file_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        print("Failed to open file for reading.\n");
        ExitProcess(1);
    }
    
    char buffer[128] = {0};
    DWORD read_bytes = 0;
    ReadFile(hFile, buffer, sizeof(buffer)-1, &read_bytes, NULL);
    CloseHandle(hFile);
    
    if (read_bytes > 0) {
        print("Data read from file: ");
        print(buffer);
    } else {
        print("Failed to read data.\n");
    }
    
    ExitProcess(0);
}
