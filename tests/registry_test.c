#include <windows.h>

void print_out(const char* str) {
    DWORD written = 0;
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    WriteFile(hStdout, str, lstrlenA(str), &written, NULL);
}

void print_int(int val) {
    char buf[16];
    int i = 0;
    if (val == 0) { buf[i++] = '0'; }
    else {
        if (val < 0) { buf[i++] = '-'; val = -val; }
        char tmp[16]; int j = 0;
        while (val > 0) { tmp[j++] = '0' + (val % 10); val /= 10; }
        while (j > 0) { buf[i++] = tmp[--j]; }
    }
    buf[i] = '\0';
    print_out(buf);
}

void _start(void) {
    print_out("=== MacWI Registry Persistence Test ===\n\n");

    HKEY hKey;
    DWORD disposition;

    // 1. Create Key
    print_out("1. Creating HKCU\\Software\\MacWI\\TestApp...\n");
    LONG st = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\MacWI\\TestApp", 0, NULL, 0, 0x000F003F, NULL, &hKey, &disposition);
    if (st != 0) {
        print_out("   FAILED! Error: ");
        print_int(st);
        print_out("\n");
        ExitProcess(1);
    }
    print_out("   SUCCESS. Disposition: ");
    print_int(disposition);
    print_out("\n\n");

    // 2. Set string value
    print_out("2. Setting string value 'AppName'...\n");
    const char* app_name = "MacWI Test Application v1.0";
    st = RegSetValueExA(hKey, "AppName", 0, REG_SZ, (const BYTE*)app_name, lstrlenA(app_name) + 1);
    if (st != 0) {
        print_out("   FAILED!\n");
        ExitProcess(1);
    }
    print_out("   SUCCESS.\n");

    // 3. Set DWORD value
    print_out("3. Setting DWORD value 'Version'...\n");
    DWORD version = 100;
    st = RegSetValueExA(hKey, "Version", 0, REG_DWORD, (const BYTE*)&version, sizeof(DWORD));
    if (st != 0) {
        print_out("   FAILED!\n");
        ExitProcess(1);
    }
    print_out("   SUCCESS.\n");

    // 4. Set another string
    print_out("4. Setting string value 'InstallPath'...\n");
    const char* path = "C:\\Program Files\\MacWI";
    st = RegSetValueExA(hKey, "InstallPath", 0, REG_SZ, (const BYTE*)path, lstrlenA(path) + 1);
    if (st != 0) {
        print_out("   FAILED!\n");
        ExitProcess(1);
    }
    print_out("   SUCCESS.\n\n");

    // 5. Query string value back
    print_out("5. Querying 'AppName' back...\n");
    char buffer[256] = {0};
    DWORD cbData = sizeof(buffer);
    DWORD type = 0;
    st = RegQueryValueExA(hKey, "AppName", NULL, &type, (BYTE*)buffer, &cbData);
    if (st == 0) {
        print_out("   Value: ");
        print_out(buffer);
        print_out("\n   Type: ");
        print_int(type);
        print_out(" (REG_SZ=1)\n");
    } else {
        print_out("   FAILED!\n");
    }

    // 6. Query DWORD value back
    print_out("6. Querying 'Version' back...\n");
    DWORD ver_read = 0;
    cbData = sizeof(DWORD);
    st = RegQueryValueExA(hKey, "Version", NULL, &type, (BYTE*)&ver_read, &cbData);
    if (st == 0) {
        print_out("   Value: ");
        print_int(ver_read);
        print_out("\n   Type: ");
        print_int(type);
        print_out(" (REG_DWORD=4)\n\n");
    } else {
        print_out("   FAILED!\n");
    }

    // 7. Close and re-open to test persistence
    print_out("7. Closing key...\n");
    RegCloseKey(hKey);
    print_out("   Key closed (registry should be saved to disk).\n\n");

    print_out("8. Re-opening HKCU\\Software\\MacWI\\TestApp...\n");
    HKEY hKey2;
    st = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\MacWI\\TestApp", 0, 0x000F003F, &hKey2);
    if (st != 0) {
        print_out("   FAILED to re-open! Error: ");
        print_int(st);
        print_out("\n");
        ExitProcess(1);
    }
    print_out("   SUCCESS.\n");

    // 9. Query the value again from the re-opened key
    print_out("9. Querying 'AppName' from re-opened key...\n");
    buffer[0] = '\0';
    cbData = sizeof(buffer);
    st = RegQueryValueExA(hKey2, "AppName", NULL, &type, (BYTE*)buffer, &cbData);
    if (st == 0) {
        print_out("   Value: ");
        print_out(buffer);
        print_out("\n   PERSISTENCE CHECK PASSED!\n\n");
    } else {
        print_out("   PERSISTENCE CHECK FAILED!\n\n");
    }

    RegCloseKey(hKey2);

    print_out("=== All Registry Tests Completed ===\n");
    ExitProcess(0);
}
