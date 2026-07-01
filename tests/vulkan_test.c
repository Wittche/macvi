#include <windows.h>
#include <stdint.h>

typedef void* (*vkGetInstanceProcAddr_t)(void* instance, const char* pName);

static void print(const char* str) {
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    
    // Calculate string length
    DWORD len = 0;
    while(str[len]) len++;
    
    WriteFile(hStdout, str, len, &written, NULL);
}

static void print_hex(uintptr_t val) {
    char buf[32];
    char hex_chars[] = "0123456789ABCDEF";
    int i = 30;
    buf[31] = '\0';
    
    if (val == 0) {
        buf[i--] = '0';
    } else {
        while (val > 0) {
            buf[i--] = hex_chars[val % 16];
            val /= 16;
        }
    }
    buf[i--] = 'x';
    buf[i--] = '0';
    print(&buf[i + 1]);
}

void _start() {
    print("Loading vulkan-1.dll...\n");
    HMODULE hVulkan = LoadLibraryA("vulkan-1.dll");
    if (!hVulkan) {
        print("Failed to load vulkan-1.dll!\n");
        ExitProcess(1);
    }
    print("Successfully loaded vulkan-1.dll at ");
    print_hex((uintptr_t)hVulkan);
    print("\n");

    vkGetInstanceProcAddr_t gpa = (vkGetInstanceProcAddr_t)GetProcAddress(hVulkan, "vkGetInstanceProcAddr");
    if (!gpa) {
        print("Failed to find vkGetInstanceProcAddr!\n");
        ExitProcess(1);
    }
    print("Found vkGetInstanceProcAddr at ");
    print_hex((uintptr_t)gpa);
    print("\n");

    // Try to resolve something through it
    void* create_inst = gpa(NULL, "vkCreateInstance");
    print("vkCreateInstance thunk at ");
    print_hex((uintptr_t)create_inst);
    print("\n");
    
    // Call the thunk (which currently just returns 0)
    if (create_inst) {
        typedef int (*vkCreateInstance_t)(void*, void*, void*);
        vkCreateInstance_t vk_create = (vkCreateInstance_t)create_inst;
        int res = vk_create(NULL, NULL, NULL);
        if (res == 0) {
            print("vkCreateInstance returned VK_SUCCESS (0)\n");
        } else {
            print("vkCreateInstance returned an error\n");
        }
    }

    print("Test completed successfully.\n");
    ExitProcess(0);
}
