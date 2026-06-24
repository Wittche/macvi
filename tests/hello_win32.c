#include <windows.h>

void __stdcall WinMainCRTStartup(void) {
    const char* msg = "Hello from x86 Windows on ARM64 macOS!\n";
    DWORD written = 0;
    
    // OutputDebugStringA directly
    OutputDebugStringA("Starting Win32 Hello World...\n");
    
    // Test CreateFileA and WriteFile
    HANDLE hFile = CreateFileA("hello_output.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, msg, 39, &written, NULL);
        CloseHandle(hFile);
        OutputDebugStringA("Successfully wrote to hello_output.txt\n");
    } else {
        OutputDebugStringA("Failed to open file for writing.\n");
    }

    ExitProcess(42);
}
