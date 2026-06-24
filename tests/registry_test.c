#include <windows.h>
#include <stdio.h>

void print(const char* msg) {
    OutputDebugStringA(msg);
}

void __stdcall mainCRTStartup(void) {
    print("Registry Test Started.\n");
    
    HKEY hKey;
    DWORD disposition;
    
    // 1. Create Key
    LONG st = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\MacWI", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, &disposition);
    if (st != ERROR_SUCCESS) {
        print("Failed to create key.\n");
        ExitProcess(1);
    }
    
    print("Key created/opened successfully.\n");
    
    // 2. Set Value
    const char* val = "MacWI is awesome";
    st = RegSetValueExA(hKey, "TestString", 0, REG_SZ, (const BYTE*)val, lstrlenA(val) + 1);
    if (st != ERROR_SUCCESS) {
        print("Failed to set value.\n");
        ExitProcess(1);
    }
    
    print("Value set successfully.\n");
    
    // 3. Query Value
    char buffer[256] = {0};
    DWORD cbData = sizeof(buffer);
    DWORD type = 0;
    st = RegQueryValueExA(hKey, "TestString", NULL, &type, (BYTE*)buffer, &cbData);
    if (st == ERROR_SUCCESS) {
        print("Value queried successfully: ");
        print(buffer);
        print("\n");
    } else {
        print("Failed to query value.\n");
    }
    
    // 4. Close Key
    RegCloseKey(hKey);
    
    ExitProcess(0);
}
