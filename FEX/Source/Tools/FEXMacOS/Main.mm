#include <FEXCore/Config/Config.h>
#include <FEXCore/Core/Context.h>
#include <FEXCore/Core/CoreState.h>
#include <FEXCore/Core/X86Enums.h>
#include <FEXCore/Utils/LogManager.h>
#include "FEXCore/Core/FEXLibrary.h"
#include "../MacOSEmulation/MachOLoader.h"
#include "../MacOSEmulation/PELoader.h"
#include "../MacOSEmulation/VFSEmulator.h"
#include "../MacOSEmulation/MacOSSyscalls.h"

#ifdef __APPLE__
#import <AppKit/AppKit.h>
#endif

#include <iostream>
#include <fstream>
#include <thread>
#include <signal.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

static FEX_Context* g_Ctx = nullptr;
static FEX_Thread* g_Thread = nullptr;

static void MsgHandler(LogMan::DebugLevels Level, const char* Message) {
    const char* CharLevel {LogMan::DebugLevelStr(Level)};
    fprintf(stderr, "[FEX] %s: %s\n", CharLevel, Message);
}

static void AssertHandler(const char* Message) {
    fprintf(stderr, "[FEX-ASSERT] %s\n", Message);
}

static void cleanup_handler() {
    if (g_Ctx) FEX_Shutdown();
}

static void signal_handler(int sig, siginfo_t *info, void *ucontext) {
    if (sig == SIGBUS && g_Thread) {
        if (FEX_HandleSIGBUS(g_Thread, ucontext)) {
            // Handled by FEXCore (e.g. unaligned access emulated)
            return;
        }
    }

    ucontext_t *uc = (ucontext_t *)ucontext;
    uint64_t pc = 0;
    uint64_t lr = 0;
    uint64_t sp = 0;
    uint64_t x28 = 0;
    uint64_t x1 = 0;
    uint64_t x0 = 0;

#if defined(__APPLE__) && defined(__aarch64__)
    if (uc && uc->uc_mcontext) {
        pc = uc->uc_mcontext->__ss.__pc;
        lr = uc->uc_mcontext->__ss.__lr;
        sp = uc->uc_mcontext->__ss.__sp;
        x28 = uc->uc_mcontext->__ss.__x[28];
        x1 = uc->uc_mcontext->__ss.__x[1];
        x0 = uc->uc_mcontext->__ss.__x[0];
    }
#endif

    fprintf(stderr, "\n[FEXMacOS-Signal] SIGNAL %d received!\n", sig);
    fprintf(stderr, "[FEXMacOS-Signal] Fault Address: %p\n", info ? info->si_addr : nullptr);
    fprintf(stderr, "[FEXMacOS-Signal] Host Register State:\n");
    fprintf(stderr, "   PC:  0x%016llx\n", pc);
    fprintf(stderr, "   LR:  0x%016llx\n", lr);
    fprintf(stderr, "   SP:  0x%016llx\n", sp);
    fprintf(stderr, "   X0:  0x%016llx\n", x0);
    fprintf(stderr, "   X1:  0x%016llx\n", x1);
    fprintf(stderr, "   X28: 0x%016llx (STATE)\n\n", x28);

    cleanup_handler();
    exit(128 + sig);
}

#include "../MacOSEmulation/ELFLoader.h"

extern "C" {
void FEX_ShowWinecfgWindow();
bool FEX_IsWinecfgWindowOpen();
uint64_t Metal_CreateWindow(const char* title, int x, int y, int w, int h);
void* Metal_CreateLayer(uint64_t window_handle);
void Metal_ClearAndPresent(void* layer, float r, float g, float b, float a);
int Metal_GetMessage(void* msg);

void FEX_RegistryInit();
uint32_t FEX_RegistryOpenKey(uint64_t RootKey, const char* SubKey, uint64_t* OutHandle);
uint32_t FEX_RegistryQueryValue(uint64_t KeyHandle, const char* ValueName, uint32_t* OutType, void* Data, uint32_t* DataSize);
uint32_t FEX_RegistrySetValue(uint64_t KeyHandle, const char* ValueName, uint32_t Type, const void* Data, uint32_t DataSize);
uint32_t FEX_RegistryCloseKey(uint64_t KeyHandle);
void FEX_CleanExit(int status);
}

int main(int argc, char** argv) {
    // Force referencing the symbols to prevent stripping by linker
    volatile void* dummy_refs[] = {
        (void*)FEX_ShowWinecfgWindow,
        (void*)FEX_IsWinecfgWindowOpen,
        (void*)Metal_CreateWindow,
        (void*)Metal_CreateLayer,
        (void*)Metal_ClearAndPresent,
        (void*)Metal_GetMessage,
        (void*)FEX_RegistryInit,
        (void*)FEX_RegistryOpenKey,
        (void*)FEX_RegistryQueryValue,
        (void*)FEX_RegistrySetValue,
        (void*)FEX_RegistryCloseKey,
        (void*)FEX_CleanExit
    };
    (void)dummy_refs;

    LogMan::Throw::InstallHandler(AssertHandler);
    LogMan::Msg::InstallHandler(MsgHandler);

#ifdef __APPLE__
    int tso_enable = 1;
    if (sysctlbyname("kern.tso_enable", nullptr, nullptr, &tso_enable, sizeof(tso_enable)) != 0) {
        fprintf(stderr, "[FEXMacOS] Warning: Failed to enable hardware TSO (Total Store Ordering) on this system.\n");
    } else {
        fprintf(stderr, "[FEXMacOS] Info: Hardware TSO (Total Store Ordering) enabled successfully.\n");
    }
#endif

    struct sigaction sa;
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGSEGV, &sa, NULL);

    atexit(cleanup_handler);
    signal(SIGSYS, SIG_IGN);
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <x86_64_binary>" << std::endl;
        return 1;
    }

    bool is_64bit = true;
    {
        std::ifstream file(argv[1], std::ios::binary);
        if (file) {
            char dos_header[64];
            file.read(dos_header, 64);
            if (dos_header[0] == 'M' && dos_header[1] == 'Z') {
                uint32_t e_lfanew = *(uint32_t*)&dos_header[60];
                file.seekg(e_lfanew);
                char pe_sig[4];
                file.read(pe_sig, 4);
                if (pe_sig[0] == 'P' && pe_sig[1] == 'E' && pe_sig[2] == 0 && pe_sig[3] == 0) {
                    char file_hdr[20];
                    file.read(file_hdr, 20);
                    uint16_t opt_hdr_size = *(uint16_t*)&file_hdr[16];
                    if (opt_hdr_size >= 2) {
                        char magic_buf[2];
                        file.read(magic_buf, 2);
                        uint16_t magic = *(uint16_t*)magic_buf;
                        if (magic == 0x10b) { // PE32 (32-bit)
                            is_64bit = false;
                        }
                    }
                }
            }
        }
    }

    int init_flags = FEX_INIT_ENABLE_JIT;
    if (is_64bit) {
        init_flags |= FEX_INIT_64BIT_MODE;
        fprintf(stderr, "[FEXMacOS] Target binary detected as 64-bit (PE32+)\n");
    } else {
        fprintf(stderr, "[FEXMacOS] Target binary detected as 32-bit (PE32)\n");
    }

    FEX_Initialize(init_flags);
    FEX_RegistryInit(); // Initialize Registry

    // Make TSO default in FEXCore configuration
    FEXCore::Config::Set(FEXCore::Config::ConfigOption::CONFIG_TSOENABLED, "1");

    // Initialize VFS with Wine 11.9 DLL paths
    {
        auto& vfs = MacOSEmulation::VFSEmulator::Get();
        vfs.Init("/Users/firataktug/Desktop/winemacosport/FEX");
        vfs.SetWineDLLPath("/Users/firataktug/Desktop/winemacosport/wine-macosx-port-x64/usr/local/lib/wine/x86_64-windows");
    }
    
    // Verify registry load and save behavior directly
    {
        uint64_t hKey = 0;
        if (FEX_RegistryOpenKey(0x80000002, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", &hKey) == 0) {
            char prod[128] = {0};
            uint32_t type = 0;
            uint32_t size = sizeof(prod);
            if (FEX_RegistryQueryValue(hKey, "ProductName", &type, prod, &size) == 0) {
                fprintf(stderr, "[FEXMacOS-Verify] Initial ProductName read from registry: '%s'\n", prod);
            }
            // Set custom value to verify saving
            const char* testVal = "Windows 11 Pro";
            FEX_RegistrySetValue(hKey, "ProductName", 1, testVal, strlen(testVal) + 1);
            fprintf(stderr, "[FEXMacOS-Verify] Set ProductName to '%s'\n", testVal);
            FEX_RegistryCloseKey(hKey);
        }
    }

    g_Ctx = FEX_ContextCreate(init_flags);
    
    // Attach MacOSSyscalls
    auto syscall_handler = new MacOSEmulation::MacOSSyscalls();
    syscall_handler->Init();
    FEX_SetSyscallHandler(g_Ctx, syscall_handler);

    // ROOTFS-LESS VISION: Directly load Windows PE or x86_64 Mach-O
    std::thread guest_thread([&]() {
        fprintf(stderr, "[FEXMacOS] Native Bridge Mode: Loading %s\n", argv[1]);
        
        bool loaded = false;
        uint64_t EntryPoint = 0;
        uint64_t ImageBase = 0;

        // Try Mach-O Loader first (for native x86_64 macOS binaries like Wine)
        MacOSEmulation::MachOLoader MachOLoader;
        if (MachOLoader.Load(reinterpret_cast<FEXCore::Context::Context*>(g_Ctx), argv[1])) {
            EntryPoint = MachOLoader.GetEntryPoint();
            ImageBase = MachOLoader.GetBaseAddress();
            loaded = true;
            fprintf(stderr, "[FEXMacOS] Successfully loaded Mach-O binary\n");
        } else {
        // Fallback to PE Loader (for Windows .exe files)
        MacOSEmulation::PELoader Loader;
        Loader.SetDLLPath("/Users/firataktug/Desktop/winemacosport/wine-macosx-port-x64/usr/local/lib/wine/x86_64-windows");
        
        fprintf(stderr, "[FEXMacOS] Calling PELoader::Load for %s...\n", argv[1]);
        if (Loader.Load(g_Ctx, argv[1])) {
            fprintf(stderr, "[FEXMacOS] PELoader::Load completed successfully.\n");
            EntryPoint = Loader.GetEntryPoint();
            ImageBase = Loader.GetBaseAddress();
            loaded = true;
            fprintf(stderr, "[FEXMacOS] Successfully loaded Windows PE binary\n");
        } else {
            fprintf(stderr, "[FEXMacOS] PELoader::Load FAILED.\n");
        }
        }
        
        if (loaded) {
            uint64_t StackSize = 8 * 1024 * 1024;
            uint64_t GuestStack = FEX_MapMemory(g_Ctx, 0, StackSize, FEX_MEM_READ | FEX_MEM_WRITE);
            uint64_t FinalRSP = (GuestStack + StackSize - 0x1000) & ~0xFFFULL;

            g_Thread = FEX_ThreadCreate(g_Ctx, EntryPoint, FinalRSP);
            
            // CRITICAL: Explicitly set RSP register to ensure JIT doesn't start with 0
            FEX_ThreadSetReg(g_Thread, "rsp", FinalRSP);
            
            // Set up a dummy TLS area for dyld/Wine
            // On macOS x86_64, gs:0 often points to the pthread structure.
            uint64_t TLSSize = 0x4000;
            uint64_t TLSBase = FEX_MapMemory(g_Ctx, 0, TLSSize, FEX_MEM_READ | FEX_MEM_WRITE);
            FEX_ThreadSetTLSBase(g_Thread, TLSBase);
            
            FEX_InitWindowsEnvironment(g_Ctx, g_Thread, ImageBase, argc - 1, &argv[1]);
            
            fprintf(stderr, "[FEXMacOS] JIT Starting at 0x%llx (RSP: 0x%llx, TLS: 0x%llx)\n", EntryPoint, FinalRSP, TLSBase);
            fprintf(stderr, "[FEXMacOS] Calling FEX_ThreadExecute...\n");
            FEX_ThreadExecute(g_Thread);
            fprintf(stderr, "[FEXMacOS] FEX_ThreadExecute returned.\n");
            FEX_CleanExit(0);
        } else {
            fprintf(stderr, "[FEXMacOS] ERROR: Failed to load binary %s\n", argv[1]);
            FEX_CleanExit(1);
        }
    });

#ifdef __APPLE__
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp run];
#else
    guest_thread.join();
#endif

    return 0;
}
