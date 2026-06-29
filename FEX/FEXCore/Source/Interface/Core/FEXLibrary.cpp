// SPDX-License-Identifier: MIT
#include "FEXCore/Core/FEXLibrary.h"
#include "FEXCore/Core/Context.h"
#include "FEXCore/Core/CoreState.h"
#include "FEXCore/Core/HostFeatures.h"
#include "FEXCore/Core/X86Enums.h"
#include "FEXCore/HLE/SyscallHandler.h"
#include "FEXCore/Core/CodeCache.h"
#include "FEXCore/Debug/InternalThreadState.h"
#include "FEXCore/Config/Config.h"
#include "FEXCore/Utils/CompilerDefs.h"
#include <FEXCore/Utils/ArchHelpers/Arm64.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#endif


struct FEX_Context {
  FEXCore::Context::Context* Ctx;
};

struct FEX_Thread {
  FEXCore::Context::Context* Ctx;
  FEXCore::Core::InternalThreadState* Thread;
};

// Minimal structures to avoid conflicts
struct WinPEB { uint8_t i[16]; uint64_t ImageBaseAddress; uint64_t Ldr; uint64_t ProcessParameters; uint64_t SubSystemData; uint64_t ProcessHeap; uint32_t OSMajorVersion; };
struct WinTEB { uint64_t r1[1]; uint64_t StackBase; uint64_t StackLimit; uint64_t r2[3]; uint64_t Self; uint64_t r3[5]; uint64_t ProcessEnvironmentBlock; };

struct WinGlobals {
    int argc;
    uint64_t argv_ptr;
    char cmd_line[512];
    uint64_t argv_array[16];
};
static uint64_t g_win_globals_guest = 0;
static uint64_t g_image_base = 0;
static bool g_Is32Bit = false;

static uint64_t GetParam(FEXCore::Core::CpuStateFrame* Frame, int index) {
    if (g_Is32Bit) {
        uint64_t esp = Frame->State.gregs[4];
        return *(uint32_t*)(esp + 4 + index * 4);
    } else {
        if (index == 0) return Frame->State.gregs[1]; // RCX
        if (index == 1) return Frame->State.gregs[2]; // RDX
        if (index == 2) return Frame->State.gregs[8]; // R8
        if (index == 3) return Frame->State.gregs[9]; // R9
        uint64_t rsp = Frame->State.gregs[4];
        return *(uint64_t*)(rsp + 40 + (index - 4) * 8);
    }
}

static std::mutex g_stub_map_mutex;
static std::unordered_map<uint64_t, std::string> g_stub_map;

struct SavedThreadState {
    uint64_t rip;
    uint64_t rsp;
    uint64_t gregs[16];
};

static std::mutex g_window_map_mutex;
static std::unordered_map<std::string, uint64_t> g_class_wndprocs;
static std::unordered_map<uint64_t, uint64_t> g_window_wndprocs;
static thread_local std::vector<SavedThreadState> t_saved_states;

struct WinMSG {
    uint64_t hwnd;
    uint32_t message;
    uint64_t wParam;
    uint64_t lParam;
    uint32_t time;
    int32_t pt_x;
    int32_t pt_y;
};

extern "C" {
FEX_DEFAULT_VISIBILITY void FEX_RegisterStubAddress(uint64_t Address, const char* Name) {
    std::lock_guard<std::mutex> lock(g_stub_map_mutex);
    g_stub_map[Address] = Name;
}

FEX_DEFAULT_VISIBILITY const char* FEX_GetStubName(uint64_t Address) {
    std::lock_guard<std::mutex> lock(g_stub_map_mutex);
    auto it = g_stub_map.find(Address);
    if (it != g_stub_map.end()) {
        return it->second.c_str();
    }
    return nullptr;
}
}


typedef uint64_t (*Metal_CreateWindow_t)(const char* title, int x, int y, int w, int h);
typedef void* (*Metal_CreateLayer_t)(uint64_t window_handle);
typedef void (*Metal_ClearAndPresent_t)(void* layer, float r, float g, float b, float a);
typedef int (*Metal_GetMessage_t)(struct WinMSG* msg);
typedef void (*FEX_ShowWinecfgWindow_t)();
typedef bool (*FEX_IsWinecfgWindowOpen_t)();

typedef uint32_t (*FEX_RegistryOpenKey_t)(uint64_t RootKey, const char* SubKey, uint64_t* OutHandle);
typedef uint32_t (*FEX_RegistryQueryValue_t)(uint64_t KeyHandle, const char* ValueName, uint32_t* OutType, void* Data, uint32_t* DataSize);
typedef uint32_t (*FEX_RegistrySetValue_t)(uint64_t KeyHandle, const char* ValueName, uint32_t Type, const void* Data, uint32_t DataSize);
typedef uint32_t (*FEX_RegistryCloseKey_t)(uint64_t KeyHandle);


class DummySyscallHandler : public FEXCore::HLE::SyscallHandler {
public:
    FEX_Context* Ctx;
    DummySyscallHandler(FEX_Context* Ctx) : Ctx(Ctx) {
        OSABI = FEXCore::HLE::SyscallOSABI::OS_LINUX64;
    }

    uint64_t HandleSyscall(FEXCore::Core::CpuStateFrame* Frame, FEXCore::HLE::SyscallArguments* Args) override {
        uint32_t Class = Args->Argument[0] >> 24;
        uint32_t Index = Args->Argument[0] & 0xFFFFFF;

        if (Class == 0x2 && Index == 197) { // mmap
            return (uint64_t)mmap((void*)Args->Argument[1], Args->Argument[2], Args->Argument[3], Args->Argument[4], Args->Argument[5], Args->Argument[6]);
        }

        if (Class == 0x3) {
            uint64_t rcx_in = GetParam(Frame, 0);
            uint64_t rdx_in = GetParam(Frame, 1);
            uint64_t r8_in  = GetParam(Frame, 2);
            uint64_t r9_in  = GetParam(Frame, 3);
            
            uint64_t stub_page = Frame->State.rip & ~0xFFFULL;
            const char* stub_name = FEX_GetStubName(stub_page);
            if (stub_name) {
                fprintf(stderr, "[FEX-Thunk] Call to %s (ID: %d) RIP: 0x%llx\n", stub_name, (int)Index, (unsigned long long)Frame->State.rip);
            } else {
                fprintf(stderr, "[FEX-Thunk] Call ID: %d RIP: 0x%llx\n", (int)Index, (unsigned long long)Frame->State.rip);
            }

            // Resolve symbols dynamically from the host binary
            static auto p_Metal_CreateWindow = (Metal_CreateWindow_t)dlsym(RTLD_DEFAULT, "Metal_CreateWindow");
            static auto p_Metal_CreateLayer = (Metal_CreateLayer_t)dlsym(RTLD_DEFAULT, "Metal_CreateLayer");
            static auto p_Metal_ClearAndPresent = (Metal_ClearAndPresent_t)dlsym(RTLD_DEFAULT, "Metal_ClearAndPresent");
            static auto p_Metal_GetMessage = (Metal_GetMessage_t)dlsym(RTLD_DEFAULT, "Metal_GetMessage");
            static auto p_FEX_ShowWinecfgWindow = (FEX_ShowWinecfgWindow_t)dlsym(RTLD_DEFAULT, "FEX_ShowWinecfgWindow");
            static auto p_FEX_IsWinecfgWindowOpen = (FEX_IsWinecfgWindowOpen_t)dlsym(RTLD_DEFAULT, "FEX_IsWinecfgWindowOpen");

            static auto p_FEX_RegistryOpenKey = (FEX_RegistryOpenKey_t)dlsym(RTLD_DEFAULT, "FEX_RegistryOpenKey");
            static auto p_FEX_RegistryQueryValue = (FEX_RegistryQueryValue_t)dlsym(RTLD_DEFAULT, "FEX_RegistryQueryValue");
            static auto p_FEX_RegistrySetValue = (FEX_RegistrySetValue_t)dlsym(RTLD_DEFAULT, "FEX_RegistrySetValue");
            static auto p_FEX_RegistryCloseKey = (FEX_RegistryCloseKey_t)dlsym(RTLD_DEFAULT, "FEX_RegistryCloseKey");


            typedef void (*FEX_CleanExit_t)(int status);
            static auto p_FEX_CleanExit = (FEX_CleanExit_t)dlsym(RTLD_DEFAULT, "FEX_CleanExit");

            uint64_t Result = 0;
            switch (Index) {
                case 0: {
                    if (p_FEX_CleanExit) {
                        p_FEX_CleanExit(rcx_in);
                    } else {
                        exit(rcx_in);
                    }
                    break;
                }
                case 10: Result = g_image_base; break;
                case 11: {
                    uint64_t symbol_val = rdx_in;
                    if (symbol_val >> 16) {
                        const char* name = (const char*)symbol_val;
                        fprintf(stderr, "[FEX-Thunk] GetProcAddress: resolving '%s'\n", name);
                        if (strcmp(name, "__wine_dbg_header") == 0) {
                            static uint64_t debug_header_stub = 0;
                            if (!debug_header_stub) {
                                debug_header_stub = FEX_MapMemory(Ctx, 0, 0x1000, FEX_MEM_READ | FEX_MEM_WRITE);
                                uint8_t* code = (uint8_t*)debug_header_stub;
                                uint32_t syscall_num = (0x3 << 24) | 502;
                                code[0] = 0x48; code[1] = 0xC7; code[2] = 0xC0; *(uint32_t*)&code[3] = syscall_num;
                                code[7] = 0x0F; code[8] = 0x05;
                                code[9] = 0xC3;
                                mprotect((void*)debug_header_stub, 0x1000, PROT_READ | PROT_EXEC);
                            }
                            Result = debug_header_stub;
                        } else if (strcmp(name, "__wine_dbg_get_channel_flags") == 0) {
                            static uint64_t dbg_channel_stub = 0;
                            if (!dbg_channel_stub) {
                                dbg_channel_stub = FEX_MapMemory(Ctx, 0, 0x1000, FEX_MEM_READ | FEX_MEM_WRITE);
                                uint8_t* code = (uint8_t*)dbg_channel_stub;
                                code[0] = 0x31; code[1] = 0xC0; code[2] = 0xC3; // xor eax, eax; ret
                                mprotect((void*)dbg_channel_stub, 0x1000, PROT_READ | PROT_EXEC);
                            }
                            Result = dbg_channel_stub;
                        } else if (strcmp(name, "__wine_dbg_output") == 0) {
                            static uint64_t debug_output_stub = 0;
                            if (!debug_output_stub) {
                                debug_output_stub = FEX_MapMemory(Ctx, 0, 0x1000, FEX_MEM_READ | FEX_MEM_WRITE);
                                uint8_t* code = (uint8_t*)debug_output_stub;
                                uint32_t syscall_num = (0x3 << 24) | 501;
                                code[0] = 0x48; code[1] = 0xC7; code[2] = 0xC0; *(uint32_t*)&code[3] = syscall_num;
                                code[7] = 0x0F; code[8] = 0x05;
                                code[9] = 0xC3;
                                mprotect((void*)debug_output_stub, 0x1000, PROT_READ | PROT_EXEC);
                            }
                            Result = debug_output_stub;
                        }
                    } else {
                        fprintf(stderr, "[FEX-Thunk] GetProcAddress: resolving ordinal %d\n", (int)(symbol_val & 0xFFFF));
                    }
                    break;
                }
                
                // Cocoa/Metal Windowing & Event loop
                case 60: { // CreateWindowExW
                    uint64_t lpClassName_guest = rdx_in; // RDX
                    uint64_t lpWindowName_guest = r8_in; // R8
                    int X = (int32_t)GetParam(Frame, 4);
                    int Y = (int32_t)GetParam(Frame, 5);
                    int W = (int32_t)GetParam(Frame, 6);
                    int H = (int32_t)GetParam(Frame, 7);
                    char title_buf[256] = "FEX Window";
                    if (lpWindowName_guest) {
                        wchar_t* wstr = (wchar_t*)lpWindowName_guest;
                        int i = 0;
                        while (wstr[i] && i < 255) {
                            title_buf[i] = (char)wstr[i];
                            i++;
                        }
                        title_buf[i] = 0;
                    }
                    if (p_Metal_CreateWindow) {
                        Result = p_Metal_CreateWindow(title_buf, X, Y, W, H);
                    }
                    
                    std::string className = "";
                    if (lpClassName_guest) {
                        if ((lpClassName_guest >> 16) == 0) {
                            className = "#" + std::to_string(lpClassName_guest & 0xFFFF);
                        } else {
                            wchar_t* wstr = (wchar_t*)lpClassName_guest;
                            int i = 0;
                            while (wstr[i]) {
                                className += (char)wstr[i];
                                i++;
                            }
                        }
                    }
                    
                    if (Result) {
                        std::lock_guard<std::mutex> lock(g_window_map_mutex);
                        auto it = g_class_wndprocs.find(className);
                        if (it != g_class_wndprocs.end()) {
                            g_window_wndprocs[Result] = it->second;
                            fprintf(stderr, "[FEX-Thunk] CreateWindowExW: title='%s', class='%s' -> HWND 0x%llx (WndProc 0x%llx)\n", 
                                    title_buf, className.c_str(), Result, it->second);
                        } else {
                            fprintf(stderr, "[FEX-Thunk] CreateWindowExW: title='%s', class='%s' -> HWND 0x%llx (No WndProc registered)\n", 
                                    title_buf, className.c_str(), Result);
                        }
                    }
                    break;
                }
                case 61: { // ShowWindow
                    uint64_t hwnd = rcx_in; // RCX
                    uint64_t cmd = rdx_in;  // RDX
                    fprintf(stderr, "[FEX-Thunk] ShowWindow: HWND=0x%llx, cmd=%d\n", hwnd, (int)cmd);
                    Result = 1;
                    break;
                }
                case 62: { // UpdateWindow
                    uint64_t hwnd = rcx_in; // RCX
                    fprintf(stderr, "[FEX-Thunk] UpdateWindow: HWND=0x%llx\n", hwnd);
                    static void* s_layer = nullptr;
                    if (!s_layer && hwnd && p_Metal_CreateLayer) {
                        s_layer = p_Metal_CreateLayer(hwnd);
                    }
                    if (s_layer && p_Metal_ClearAndPresent) {
                        p_Metal_ClearAndPresent(s_layer, 0.0f, 0.0f, 1.0f, 1.0f);
                    }
                    Result = 1;
                    break;
                }
                case 64: { // GetMessageW
                    uint64_t lpMsg_guest = rcx_in; // RCX
                    if (lpMsg_guest && p_Metal_GetMessage) {
                        struct WinMSG msg;
                        while (true) {
                            int got = p_Metal_GetMessage(&msg);
                            if (got) {
                                if (g_Is32Bit) {
                                    *(uint32_t*)(lpMsg_guest + 0) = (uint32_t)msg.hwnd;
                                    *(uint32_t*)(lpMsg_guest + 4) = (uint32_t)msg.message;
                                    *(uint32_t*)(lpMsg_guest + 8) = (uint32_t)msg.wParam;
                                    *(uint32_t*)(lpMsg_guest + 12) = (uint32_t)msg.lParam;
                                    *(uint32_t*)(lpMsg_guest + 16) = msg.time;
                                    *(int32_t*)(lpMsg_guest + 20) = msg.pt_x;
                                    *(int32_t*)(lpMsg_guest + 24) = msg.pt_y;
                                } else {
                                    memcpy((void*)lpMsg_guest, &msg, sizeof(msg));
                                }
                                if (msg.message == 0x0012) { // WM_QUIT
                                    Result = 0;
                                } else {
                                    Result = 1;
                                }
                                break;
                            }
                            usleep(10000); // 10ms
                        }
                    }
                    break;
                }
                case 65: Result = 1; break; // TranslateMessage
                case 66: { // DispatchMessageW
                    uint64_t lpMsg_guest = rcx_in; // RCX
                    if (lpMsg_guest) {
                        uint64_t hwnd = g_Is32Bit ? *(uint32_t*)(lpMsg_guest + 0) : *(uint64_t*)(lpMsg_guest + 0);
                        uint32_t message = g_Is32Bit ? *(uint32_t*)(lpMsg_guest + 4) : *(uint32_t*)(lpMsg_guest + 8);
                        uint64_t wParam = g_Is32Bit ? *(uint32_t*)(lpMsg_guest + 8) : *(uint64_t*)(lpMsg_guest + 16);
                        uint64_t lParam = g_Is32Bit ? *(uint32_t*)(lpMsg_guest + 12) : *(uint64_t*)(lpMsg_guest + 24);
                        
                        uint64_t wndproc_guest_addr = 0;
                        {
                            std::lock_guard<std::mutex> lock(g_window_map_mutex);
                            auto it = g_window_wndprocs.find(hwnd);
                            if (it != g_window_wndprocs.end()) {
                                wndproc_guest_addr = it->second;
                            }
                        }
                        
                        if (wndproc_guest_addr) {
                            // Hijack context!
                            SavedThreadState saved;
                            saved.rip = Frame->State.rip;
                            saved.rsp = Frame->State.gregs[4]; // RSP
                            for (int i = 0; i < 16; ++i) {
                                saved.gregs[i] = Frame->State.gregs[i];
                            }
                            t_saved_states.push_back(saved);
                            
                            // Set up WndProc return stub
                            static uint64_t wndproc_ret_stub = 0;
                            if (!wndproc_ret_stub) {
                                wndproc_ret_stub = FEX_MapMemory(Ctx, 0, 0x1000, FEX_MEM_READ | FEX_MEM_WRITE);
                                uint8_t* code = (uint8_t*)wndproc_ret_stub;
                                uint32_t syscall_num = (0x3 << 24) | 69;
                                if (g_Is32Bit) {
                                    // 32-bit ret stub: mov eax, syscall_num; syscall; ret 16
                                    code[0] = 0xB8; *(uint32_t*)&code[1] = syscall_num;
                                    code[5] = 0x0F; code[6] = 0x05;
                                    code[7] = 0xC2; code[8] = 16; code[9] = 0; // ret 16
                                } else {
                                    code[0] = 0x48; code[1] = 0xC7; code[2] = 0xC0; *(uint32_t*)&code[3] = syscall_num;
                                    code[7] = 0x0F; code[8] = 0x05;
                                    code[9] = 0xC3;
                                }
                                mprotect((void*)wndproc_ret_stub, 0x1000, PROT_READ | PROT_EXEC);
                                FEX_RegisterStubAddress(wndproc_ret_stub, "WndProcRetStub");
                            }
                            
                            if (g_Is32Bit) {
                                uint64_t guest_esp = Frame->State.gregs[4];
                                guest_esp = (guest_esp - 20) & ~0xF;
                                *(uint32_t*)(guest_esp + 0) = (uint32_t)wndproc_ret_stub;
                                *(uint32_t*)(guest_esp + 4) = (uint32_t)hwnd;
                                *(uint32_t*)(guest_esp + 8) = (uint32_t)message;
                                *(uint32_t*)(guest_esp + 12) = (uint32_t)wParam;
                                *(uint32_t*)(guest_esp + 16) = (uint32_t)lParam;
                                
                                Frame->State.gregs[4] = guest_esp;
                            } else {
                                uint64_t guest_rsp = Frame->State.gregs[4];
                                guest_rsp = (guest_rsp - 8) & ~0xF;
                                *(uint64_t*)guest_rsp = wndproc_ret_stub;
                                
                                Frame->State.gregs[4] = guest_rsp; // RSP
                                Frame->State.gregs[1] = hwnd;      // RCX
                                Frame->State.gregs[2] = message;   // RDX
                                Frame->State.gregs[8] = wParam;    // R8
                                Frame->State.gregs[9] = lParam;    // R9
                            }
                            
                            Frame->State.rip = wndproc_guest_addr;
                            
                            fprintf(stderr, "[FEX-Thunk] DispatchMessageW: Hijacking thread to run WndProc at 0x%llx (HWND=0x%llx, MSG=%d)\n", 
                                    wndproc_guest_addr, hwnd, message);
                            return 0; // Return from syscall into hijacked RIP
                        }
                    }
                    Result = 0;
                    break;
                }
                case 67: { // DefWindowProcW
                    uint64_t hwnd = rcx_in; // RCX
                    uint64_t msg = rdx_in;  // RDX
                    uint64_t wp = r8_in;   // R8
                    uint64_t lp = r9_in;   // R9
                    fprintf(stderr, "[FEX-Thunk] DefWindowProcW: HWND=0x%llx, MSG=%d, wParam=0x%llx, lParam=0x%llx\n", hwnd, (int)msg, wp, lp);
                    Result = 0;
                    break;
                }
                case 69: { // WndProcRet
                    uint64_t wndproc_result = Frame->State.gregs[0]; // RAX
                    if (!t_saved_states.empty()) {
                        SavedThreadState saved = t_saved_states.back();
                        t_saved_states.pop_back();
                        
                        Frame->State.rip = saved.rip;
                        Frame->State.gregs[4] = saved.rsp;
                        for (int i = 0; i < 16; ++i) {
                            Frame->State.gregs[i] = saved.gregs[i];
                        }
                        
                        Result = wndproc_result; // Returned RAX value
                        fprintf(stderr, "[FEX-Thunk] WndProcRet: Restored context. WndProc returned 0x%llx\n", wndproc_result);
                    } else {
                        fprintf(stderr, "[FEX-Thunk] ERROR: WndProcRet called but t_saved_states is empty!\n");
                        Result = 0;
                    }
                    break;
                }
                case 85: { // RegisterClassExW
                    uint64_t lpwcx_guest = rcx_in; // RCX
                    uint16_t atom = 0;
                    if (lpwcx_guest) {
                        uint64_t lpfnWndProc = 0;
                        uint64_t lpszClassName_guest = 0;
                        if (g_Is32Bit) {
                            lpfnWndProc = *(uint32_t*)(lpwcx_guest + 8);
                            lpszClassName_guest = *(uint32_t*)(lpwcx_guest + 40);
                        } else {
                            lpfnWndProc = *(uint64_t*)(lpwcx_guest + 8);
                            lpszClassName_guest = *(uint64_t*)(lpwcx_guest + 64);
                        }
                        
                        std::string className = "";
                        if (lpszClassName_guest) {
                            if ((lpszClassName_guest >> 16) == 0) {
                                className = "#" + std::to_string(lpszClassName_guest & 0xFFFF);
                            } else {
                                wchar_t* wstr = (wchar_t*)lpszClassName_guest;
                                int i = 0;
                                while (wstr[i]) {
                                    className += (char)wstr[i];
                                    i++;
                                }
                            }
                        }
                        
                        if (!className.empty()) {
                            std::lock_guard<std::mutex> lock(g_window_map_mutex);
                            g_class_wndprocs[className] = lpfnWndProc;
                            static uint16_t nextAtom = 0xC000;
                            atom = nextAtom++;
                            g_class_wndprocs["#" + std::to_string(atom)] = lpfnWndProc;
                            fprintf(stderr, "[FEX-Thunk] RegisterClassExW: Registered class '%s' (atom %d) -> WndProc=0x%llx\n", 
                                    className.c_str(), (int)atom, lpfnWndProc);
                        }
                    }
                    Result = atom;
                    break;
                }
                case 86: { // RegisterClassW
                    uint64_t lpwc_guest = rcx_in; // RCX
                    uint16_t atom = 0;
                    if (lpwc_guest) {
                        uint64_t lpfnWndProc = 0;
                        uint64_t lpszClassName_guest = 0;
                        if (g_Is32Bit) {
                            lpfnWndProc = *(uint32_t*)(lpwc_guest + 4);
                            lpszClassName_guest = *(uint32_t*)(lpwc_guest + 36);
                        } else {
                            lpfnWndProc = *(uint64_t*)(lpwc_guest + 8);
                            lpszClassName_guest = *(uint64_t*)(lpwc_guest + 72);
                        }
                        
                        std::string className = "";
                        if (lpszClassName_guest) {
                            if ((lpszClassName_guest >> 16) == 0) {
                                className = "#" + std::to_string(lpszClassName_guest & 0xFFFF);
                            } else {
                                wchar_t* wstr = (wchar_t*)lpszClassName_guest;
                                int i = 0;
                                while (wstr[i]) {
                                    className += (char)wstr[i];
                                    i++;
                                }
                            }
                        }
                        
                        if (!className.empty()) {
                            std::lock_guard<std::mutex> lock(g_window_map_mutex);
                            g_class_wndprocs[className] = lpfnWndProc;
                            static uint16_t nextAtom = 0xD000;
                            atom = nextAtom++;
                            g_class_wndprocs["#" + std::to_string(atom)] = lpfnWndProc;
                            fprintf(stderr, "[FEX-Thunk] RegisterClassW: Registered class '%s' (atom %d) -> WndProc=0x%llx\n", 
                                    className.c_str(), (int)atom, lpfnWndProc);
                        }
                    }
                    Result = atom;
                    break;
                }
                case 90: { // GetSystemMetrics
                    int index = (int)rcx_in; // RCX
                    if (index == 0) Result = 1920; // SM_CXSCREEN
                    else if (index == 1) Result = 1080; // SM_CYSCREEN
                    else Result = 0;
                    fprintf(stderr, "[FEX-Thunk] GetSystemMetrics: index=%d -> %d\n", index, (int)Result);
                    break;
                }
                case 91: { // GetClientRect
                    uint64_t hwnd = rcx_in; // RCX
                    uint64_t lpRect_guest = rdx_in; // RDX
                    if (lpRect_guest) {
                        *(int32_t*)(lpRect_guest + 0) = 0;
                        *(int32_t*)(lpRect_guest + 4) = 0;
                        *(int32_t*)(lpRect_guest + 8) = 640;
                        *(int32_t*)(lpRect_guest + 12) = 480;
                    }
                    fprintf(stderr, "[FEX-Thunk] GetClientRect: HWND=0x%llx -> 640x480\n", hwnd);
                    Result = 1;
                    break;
                }
                case 92: { // GetDC
                    uint64_t hwnd = rcx_in; // RCX
                    Result = hwnd ? (hwnd | 0xDC00) : 0xDC01;
                    fprintf(stderr, "[FEX-Thunk] GetDC: HWND=0x%llx -> HDC 0x%llx\n", hwnd, Result);
                    break;
                }
                case 93: { // ReleaseDC
                    uint64_t hwnd = rcx_in; // RCX
                    uint64_t hdc = rdx_in; // RDX
                    fprintf(stderr, "[FEX-Thunk] ReleaseDC: HWND=0x%llx, HDC=0x%llx\n", hwnd, hdc);
                    Result = 1;
                    break;
                }
                case 94: { // GetDeviceCaps
                    uint64_t hdc = rcx_in; // RCX
                    int index = (int)rdx_in; // RDX
                    if (index == 88) Result = 96;
                    else if (index == 90) Result = 96;
                    else if (index == 12) Result = 32;
                    else Result = 1;
                    fprintf(stderr, "[FEX-Thunk] GetDeviceCaps: HDC=0x%llx, index=%d -> %d\n", hdc, index, (int)Result);
                    break;
                }
                case 95: { // DeleteObject
                    uint64_t hgdi = rcx_in; // RCX
                    fprintf(stderr, "[FEX-Thunk] DeleteObject: HGDI=0x%llx\n", hgdi);
                    Result = 1;
                    break;
                }
                case 96: { // SetWindowTextW
                    uint64_t hwnd = rcx_in; // RCX
                    uint64_t lpString_guest = rdx_in; // RDX
                    std::string text = "";
                    if (lpString_guest) {
                        wchar_t* wstr = (wchar_t*)lpString_guest;
                        int i = 0;
                        while (wstr[i] && i < 512) {
                            text += (char)wstr[i];
                            i++;
                        }
                    }
                    fprintf(stderr, "[FEX-Thunk] SetWindowTextW: HWND=0x%llx, text='%s'\n", hwnd, text.c_str());
                    Result = 1;
                    break;
                }
                case 97: { // GetWindowTextW
                    uint64_t hwnd = rcx_in; // RCX
                    uint64_t lpString_guest = rdx_in; // RDX
                    int nMaxCount = (int)r8_in; // R8
                    int len = 0;
                    if (lpString_guest && nMaxCount > 0) {
                        wchar_t* wstr = (wchar_t*)lpString_guest;
                        std::wstring mock_title = L"FEX Window Title";
                        len = std::min((int)mock_title.length(), nMaxCount - 1);
                        for (int i = 0; i < len; ++i) {
                            wstr[i] = mock_title[i];
                        }
                        wstr[len] = 0;
                    }
                    fprintf(stderr, "[FEX-Thunk] GetWindowTextW: HWND=0x%llx -> len=%d\n", hwnd, len);
                    Result = len;
                    break;
                }
                case 98: { // SendMessageW
                    uint64_t hwnd = rcx_in; // RCX
                    uint32_t msg = (uint32_t)rdx_in; // RDX
                    uint64_t wp = r8_in; // R8
                    uint64_t lp = r9_in; // R9
                    fprintf(stderr, "[FEX-Thunk] SendMessageW: HWND=0x%llx, MSG=%d, wParam=0x%llx, lParam=0x%llx\n", hwnd, msg, wp, lp);
                    uint64_t wndproc_guest_addr = 0;
                    {
                        std::lock_guard<std::mutex> lock(g_window_map_mutex);
                        auto it = g_window_wndprocs.find(hwnd);
                        if (it != g_window_wndprocs.end()) {
                            wndproc_guest_addr = it->second;
                        }
                    }
                    if (wndproc_guest_addr) {
                        SavedThreadState saved;
                        saved.rip = Frame->State.rip;
                        saved.rsp = Frame->State.gregs[4];
                        for (int i = 0; i < 16; ++i) {
                            saved.gregs[i] = Frame->State.gregs[i];
                        }
                        t_saved_states.push_back(saved);
                        
                        static uint64_t wndproc_ret_stub = 0;
                        if (!wndproc_ret_stub) {
                            wndproc_ret_stub = FEX_MapMemory(Ctx, 0, 0x1000, FEX_MEM_READ | FEX_MEM_WRITE);
                            uint8_t* code = (uint8_t*)wndproc_ret_stub;
                            uint32_t syscall_num = (0x3 << 24) | 69;
                            if (g_Is32Bit) {
                                code[0] = 0xB8; *(uint32_t*)&code[1] = syscall_num;
                                code[5] = 0x0F; code[6] = 0x05;
                                code[7] = 0xC2; code[8] = 16; code[9] = 0; // ret 16
                            } else {
                                code[0] = 0x48; code[1] = 0xC7; code[2] = 0xC0; *(uint32_t*)&code[3] = syscall_num;
                                code[7] = 0x0F; code[8] = 0x05;
                                code[9] = 0xC3;
                            }
                            mprotect((void*)wndproc_ret_stub, 0x1000, PROT_READ | PROT_EXEC);
                            FEX_RegisterStubAddress(wndproc_ret_stub, "WndProcRetStub");
                        }
                        
                        if (g_Is32Bit) {
                            uint64_t guest_esp = Frame->State.gregs[4];
                            guest_esp = (guest_esp - 20) & ~0xF;
                            *(uint32_t*)(guest_esp + 0) = (uint32_t)wndproc_ret_stub;
                            *(uint32_t*)(guest_esp + 4) = (uint32_t)hwnd;
                            *(uint32_t*)(guest_esp + 8) = (uint32_t)msg;
                            *(uint32_t*)(guest_esp + 12) = (uint32_t)wp;
                            *(uint32_t*)(guest_esp + 16) = (uint32_t)lp;
                            
                            Frame->State.gregs[4] = guest_esp;
                        } else {
                            uint64_t guest_rsp = Frame->State.gregs[4];
                            guest_rsp = (guest_rsp - 8) & ~0xF;
                            *(uint64_t*)guest_rsp = wndproc_ret_stub;
                            
                            Frame->State.gregs[4] = guest_rsp;
                            Frame->State.gregs[1] = hwnd;
                            Frame->State.gregs[2] = msg;
                            Frame->State.gregs[8] = wp;
                            Frame->State.gregs[9] = lp;
                        }
                        Frame->State.rip = wndproc_guest_addr;
                        
                        fprintf(stderr, "[FEX-Thunk] SendMessageW: Hijacking thread to run WndProc at 0x%llx\n", wndproc_guest_addr);
                        return 0;
                    }
                    Result = 0;
                    break;
                }
                case 99: { // SendDlgItemMessageW
                    uint64_t hDlg = rcx_in; // RCX
                    int nIDDlgItem = (int)rdx_in; // RDX
                    uint32_t Msg = (uint32_t)r8_in; // R8
                    uint64_t wParam = r9_in; // R9
                    uint64_t lParam = GetParam(Frame, 4);
                    fprintf(stderr, "[FEX-Thunk] SendDlgItemMessageW: hDlg=0x%llx, itemID=%d, MSG=%d, wParam=0x%llx, lParam=0x%llx\n", 
                            hDlg, nIDDlgItem, Msg, wParam, lParam);
                    Result = 0;
                    break;
                }
                
                // Registry Calls
                case 80: { // RegOpenKeyExW
                    uint64_t hKey = rcx_in; // RCX
                    uint64_t lpSubKey_guest = rdx_in; // RDX
                    uint64_t phkResult_guest = GetParam(Frame, 4);
                    
                    std::string subkey_str = "";
                    if (lpSubKey_guest) {
                        wchar_t* wstr = (wchar_t*)lpSubKey_guest;
                        int i = 0;
                        while (wstr[i]) {
                            subkey_str += (char)wstr[i];
                            i++;
                        }
                    }
                    
                    uint64_t outHandle = 0;
                    uint32_t status = 0;
                    if (p_FEX_RegistryOpenKey) {
                        status = p_FEX_RegistryOpenKey(hKey, subkey_str.c_str(), &outHandle);
                    }
                    if (phkResult_guest) {
                        *(uint32_t*)phkResult_guest = (uint32_t)outHandle;
                    }
                    fprintf(stderr, "[FEX-Thunk] RegOpenKeyExW: hKey=0x%llx, subkey='%s' -> outHandle=0x%llx, status=%d\n", 
                            hKey, subkey_str.c_str(), outHandle, status);
                    Result = status;
                    break;
                }
                case 81: { // RegQueryValueExW
                    uint64_t hKey = rcx_in; // RCX
                    uint64_t lpValueName_guest = rdx_in; // RDX
                    uint64_t lpType_guest = r9_in; // R9
                    uint64_t lpData_guest = GetParam(Frame, 4);
                    uint64_t lpcbData_guest = GetParam(Frame, 5);
                    
                    std::string value_str = "";
                    if (lpValueName_guest) {
                        wchar_t* wstr = (wchar_t*)lpValueName_guest;
                        int i = 0;
                        while (wstr[i]) {
                            value_str += (char)wstr[i];
                            i++;
                        }
                    }
                    
                    uint32_t type = 0;
                    uint32_t size = 0;
                    uint8_t temp_buf[512] = {0};
                    uint32_t temp_size = sizeof(temp_buf);
                    
                    uint32_t status = 0;
                    if (p_FEX_RegistryQueryValue) {
                        status = p_FEX_RegistryQueryValue(hKey, value_str.c_str(), &type, temp_buf, &temp_size);
                    }
                    
                    if (status == 0) {
                        if (type == 1 || type == 2) { // REG_SZ, REG_EXPAND_SZ (ASCII to UTF-16)
                            wchar_t wbuf[256];
                            int i = 0;
                            while (temp_buf[i] && i < 255) {
                                wbuf[i] = (wchar_t)temp_buf[i];
                                i++;
                            }
                            wbuf[i] = 0;
                            uint32_t utf16_size = (i + 1) * sizeof(wchar_t);
                            
                            if (lpData_guest && lpcbData_guest) {
                                uint32_t guest_limit = *(uint32_t*)lpcbData_guest;
                                uint32_t to_copy = std::min(utf16_size, guest_limit);
                                memcpy((void*)lpData_guest, wbuf, to_copy);
                            }
                            size = utf16_size;
                        } else {
                            if (lpData_guest && lpcbData_guest) {
                                uint32_t guest_limit = *(uint32_t*)lpcbData_guest;
                                uint32_t to_copy = std::min(temp_size, guest_limit);
                                memcpy((void*)lpData_guest, temp_buf, to_copy);
                            }
                            size = temp_size;
                        }
                    }
                    
                    if (lpType_guest) *(uint32_t*)lpType_guest = type;
                    if (lpcbData_guest) *(uint32_t*)lpcbData_guest = size;
                    
                    fprintf(stderr, "[FEX-Thunk] RegQueryValueExW: hKey=0x%llx, value='%s', type=%d, size=%d -> status=%d\n", 
                            hKey, value_str.c_str(), type, size, status);
                    Result = status;
                    break;
                }
                case 82: { // RegCreateKeyExW
                    uint64_t hKey = rcx_in; // RCX
                    uint64_t lpSubKey_guest = rdx_in; // RDX
                    uint64_t phkResult_guest = GetParam(Frame, 7);
                    
                    std::string subkey_str = "";
                    if (lpSubKey_guest) {
                        wchar_t* wstr = (wchar_t*)lpSubKey_guest;
                        int i = 0;
                        while (wstr[i]) {
                            subkey_str += (char)wstr[i];
                            i++;
                        }
                    }
                    
                    uint64_t outHandle = 0;
                    uint32_t status = 0;
                    if (p_FEX_RegistryOpenKey) {
                        status = p_FEX_RegistryOpenKey(hKey, subkey_str.c_str(), &outHandle);
                    }
                    if (phkResult_guest) {
                        *(uint32_t*)phkResult_guest = (uint32_t)outHandle;
                    }
                    fprintf(stderr, "[FEX-Thunk] RegCreateKeyExW: hKey=0x%llx, subkey='%s' -> outHandle=0x%llx, status=%d\n", 
                            hKey, subkey_str.c_str(), outHandle, status);
                    Result = status;
                    break;
                }
                case 83: { // RegCreateKeyW
                    uint64_t hKey = rcx_in; // RCX
                    uint64_t lpSubKey_guest = rdx_in; // RDX
                    uint64_t phkResult_guest = r8_in; // R8
                    
                    std::string subkey_str = "";
                    if (lpSubKey_guest) {
                        wchar_t* wstr = (wchar_t*)lpSubKey_guest;
                        int i = 0;
                        while (wstr[i]) {
                            subkey_str += (char)wstr[i];
                            i++;
                        }
                    }
                    
                    uint64_t outHandle = 0;
                    uint32_t status = 0;
                    if (p_FEX_RegistryOpenKey) {
                        status = p_FEX_RegistryOpenKey(hKey, subkey_str.c_str(), &outHandle);
                    }
                    if (phkResult_guest) {
                        *(uint32_t*)phkResult_guest = (uint32_t)outHandle;
                    }
                    fprintf(stderr, "[FEX-Thunk] RegCreateKeyW: hKey=0x%llx, subkey='%s' -> outHandle=0x%llx, status=%d\n", 
                            hKey, subkey_str.c_str(), outHandle, status);
                    Result = status;
                    break;
                }
                case 84: { // RegSetValueExW
                    uint64_t hKey = rcx_in; // RCX
                    uint64_t lpValueName_guest = rdx_in; // RDX
                    uint32_t dwType = (uint32_t)r9_in; // R9
                    uint64_t lpData_guest = GetParam(Frame, 4);
                    uint32_t cbData = (uint32_t)GetParam(Frame, 5);
                    
                    std::string value_str = "";
                    if (lpValueName_guest) {
                        wchar_t* wstr = (wchar_t*)lpValueName_guest;
                        int i = 0;
                        while (wstr[i]) {
                            value_str += (char)wstr[i];
                            i++;
                        }
                    }
                    
                    std::vector<uint8_t> data_vec;
                    if (dwType == 1 || dwType == 2) {
                        wchar_t* wdata = (wchar_t*)lpData_guest;
                        std::string ascii_str = "";
                        int i = 0;
                        while (wdata && wdata[i] && (i * sizeof(wchar_t) < cbData)) {
                            ascii_str += (char)wdata[i];
                            i++;
                        }
                        data_vec.assign(ascii_str.begin(), ascii_str.end());
                        data_vec.push_back(0); // null-terminated
                    } else {
                        data_vec.assign((uint8_t*)lpData_guest, (uint8_t*)lpData_guest + cbData);
                    }
                    
                    uint32_t status = 0;
                    if (p_FEX_RegistrySetValue) {
                        status = p_FEX_RegistrySetValue(hKey, value_str.c_str(), dwType, data_vec.data(), data_vec.size());
                    }
                    
                    fprintf(stderr, "[FEX-Thunk] RegSetValueExW: hKey=0x%llx, value='%s', type=%d, size=%d -> status=%d\n", 
                            hKey, value_str.c_str(), dwType, (int)data_vec.size(), status);
                    Result = status;
                    break;
                }
                case 135: { // RegCloseKey
                    uint64_t hKey = rcx_in; // RCX
                    if (p_FEX_RegistryCloseKey) {
                        Result = p_FEX_RegistryCloseKey(hKey);
                    }
                    fprintf(stderr, "[FEX-Thunk] RegCloseKey: hKey=0x%llx -> status=%d\n", hKey, (int)Result);
                    break;
                }

                
                case 500: { // PropertySheetW
                    fprintf(stderr, "[FEX-Thunk] PropertySheetW: Launching Cocoa window...\n");
                    if (p_FEX_ShowWinecfgWindow && p_FEX_IsWinecfgWindowOpen) {
                        p_FEX_ShowWinecfgWindow();
                        while (p_FEX_IsWinecfgWindowOpen()) {
                            usleep(50000);
                        }
                    } else {
                        fprintf(stderr, "[FEX-Thunk] ERROR: PropertySheetW bridge functions not resolved!\n");
                    }
                    Result = 1; // IDOK
                    break;
                }
                case 501: { // __wine_dbg_output
                    const char* str = (const char*)rcx_in;
                    if (str) {
                        fprintf(stderr, "[Wine-Debug] %s", str);
                    }
                    Result = 0;
                    break;
                }
                case 502: { // __wine_dbg_header
                    fprintf(stderr, "[Wine-Debug-Header] ");
                    Result = 0;
                    break;
                }

                case 130: case 131: 
                    if (g_win_globals_guest) Result = g_win_globals_guest + (g_Is32Bit ? 8 : offsetof(WinGlobals, cmd_line));
                    break;
                case 132: Result = (uint64_t)malloc(r8_in); break;
                case 133: case 134: if (r8_in) free((void*)r8_in); Result = 1; break;
                case 136: Result = 1; break;
                case 150: Result = 0x1337BEEF; break;
                case 211: Result = g_win_globals_guest + (g_Is32Bit ? 0 : offsetof(WinGlobals, argc)); break;
                case 212: Result = g_win_globals_guest + (g_Is32Bit ? 4 : offsetof(WinGlobals, argv_ptr)); break;

                // Memory functions
                case 200: Result = (uint64_t)memcpy((void*)rcx_in, (void*)rdx_in, r8_in); break;
                case 201: Result = (uint64_t)memset((void*)rcx_in, (int)rdx_in, r8_in); break;
                case 202: Result = (uint64_t)strlen((const char*)rcx_in); break;

                // Math functions (x64: arg in xmm0, return in xmm0)
                case 250: { // sin
                    double arg = *(double*)&Frame->State.xmm.avx.data[0][0];
                    double res = sin(arg);
                    *(double*)&Frame->State.xmm.avx.data[0][0] = res;
                    fprintf(stderr, "[FEX-Thunk] sin(%f) = %f\n", arg, res);
                    break;
                }
                case 251: { // cos
                    double arg = *(double*)&Frame->State.xmm.avx.data[0][0];
                    double res = cos(arg);
                    *(double*)&Frame->State.xmm.avx.data[0][0] = res;
                    fprintf(stderr, "[FEX-Thunk] cos(%f) = %f\n", arg, res);
                    break;
                }
                case 252: { // sinf
                    float arg = *(float*)&Frame->State.xmm.avx.data[0][0];
                    float res = sinf(arg);
                    *(float*)&Frame->State.xmm.avx.data[0][0] = res;
                    fprintf(stderr, "[FEX-Thunk] sinf(%f) = %f\n", arg, res);
                    break;
                }
                case 253: { // cosf
                    float arg = *(float*)&Frame->State.xmm.avx.data[0][0];
                    float res = cosf(arg);
                    *(float*)&Frame->State.xmm.avx.data[0][0] = res;
                    fprintf(stderr, "[FEX-Thunk] cosf(%f) = %f\n", arg, res);
                    break;
                }

                default: Result = 0; break;
            }
            Frame->State.gregs[0] = Result;
            return Result;
        }
        return 0;
    }

    FEXCore::HLE::ExecutableRangeInfo QueryGuestExecutableRange(FEXCore::Core::InternalThreadState* Thread, uint64_t Address) override {
        return {0, UINT64_MAX, true};
    }
    std::optional<FEXCore::ExecutableFileSectionInfo> LookupExecutableFileSection(FEXCore::Core::InternalThreadState* Thread, uint64_t GuestAddr) override { return std::nullopt; }
};

#include "FEXCore/Utils/HostFeatures.h"
#include "Interface/Context/Context.h"

extern "C" {

#include "FEXCore/Utils/Allocator.h"

FEX_DEFAULT_VISIBILITY FEX_Result FEX_Initialize(int Flags) {
  FEXCore::Config::Initialize();
  FEXCore::Config::Load();
  
  if (!(Flags & FEX_INIT_64BIT_MODE)) {
      // For 32-bit guests, we MUST set up the 48-bit (or 47-bit) allocator 
      // so GlobalMemoryBase is initialized and GuestVA->HostVA translation works!
      FEXCore::Allocator::Setup48BitAllocatorIfExists(4096);
  }
  
  return FEX_SUCCESS;
}

class LibrarySignalDelegator : public FEXCore::SignalDelegator {
public:
    uintptr_t GetThunkCallbackRET() const override { return 0; }
};

FEX_DEFAULT_VISIBILITY FEX_Context* FEX_ContextCreate(int Flags) {
    FEXCore::Config::Set(FEXCore::Config::CONFIG_IS64BIT_MODE, (Flags & FEX_INIT_64BIT_MODE) ? "1" : "0");
    g_Is32Bit = !(Flags & FEX_INIT_64BIT_MODE);

    FEXCore::HostFeatures Features = FEX::FetchHostFeatures();
    auto Ctx = new FEX_Context();
    auto CtxImpl = new FEXCore::Context::ContextImpl(Features);
    Ctx->Ctx = CtxImpl;
    
    Ctx->Ctx->SetSyscallHandler(new DummySyscallHandler(Ctx));
    
    // Provide a minimal SignalDelegator to avoid null dereference in InitCore
    Ctx->Ctx->SetSignalDelegator(new LibrarySignalDelegator());
    
    // Enable clean exit on HLT instruction (used by guest code to signal completion)
    Ctx->Ctx->EnableExitOnHLT();
    
    // Explicitly initialize the dispatcher
    Ctx->Ctx->InitCore();
    
    return Ctx;
}

FEX_DEFAULT_VISIBILITY void FEX_SetSyscallHandler(FEX_Context* Ctx, void* Handler) {
    Ctx->Ctx->SetSyscallHandler((FEXCore::HLE::SyscallHandler*)Handler);
}

FEX_DEFAULT_VISIBILITY void FEX_Shutdown(void) {}

FEX_DEFAULT_VISIBILITY bool FEX_HandleSIGBUS(FEX_Thread* Thread, void* ucontext) {
#if defined(__aarch64__) || defined(_M_ARM_64)
    ucontext_t* uc = (ucontext_t*)ucontext;
#ifdef __APPLE__
    uintptr_t pc = uc->uc_mcontext->__ss.__pc;
    uint64_t* gprs = &uc->uc_mcontext->__ss.__x[0];
#else
    uintptr_t pc = uc->uc_mcontext.pc;
    uint64_t* gprs = &uc->uc_mcontext.regs[0];
#endif
    
    // Check if it's handled by JIT
    std::optional<int32_t> Offset = FEXCore::ArchHelpers::Arm64::HandleUnalignedAccess(Thread->Thread, 
                                        FEXCore::ArchHelpers::Arm64::UnalignedHandlerType::HalfBarrier, 
                                        pc, gprs, true);
    if (Offset.has_value()) {
#ifdef __APPLE__
        uc->uc_mcontext->__ss.__pc += *Offset;
#else
        uc->uc_mcontext.pc += *Offset;
#endif
        return true; // Handled
    }
#endif
    return false; // Not handled
}

FEX_DEFAULT_VISIBILITY FEX_Thread* FEX_ThreadCreate(FEX_Context* Ctx, uint64_t EntryPoint, uint64_t StackPointer) {
  auto Thread = Ctx->Ctx->CreateThread(EntryPoint, StackPointer);
  auto Res = new FEX_Thread();
  Res->Ctx = Ctx->Ctx;
  Res->Thread = Thread;

  // Initialize GDT for the thread
  using gdt_segment = FEXCore::Core::CPUState::gdt_segment;
  auto GDT = (gdt_segment*)malloc(sizeof(gdt_segment) * 32);
  memset(GDT, 0, sizeof(gdt_segment) * 32);
  Thread->CurrentFrame->State.segment_arrays[0] = GDT;
  
  if (g_Is32Bit) {
      // Setup 32-bit GDT entries
      // Entry 4: 32-bit Code Segment (Selector 0x23)
      GDT[4].P = 1;
      GDT[4].S = 1;
      GDT[4].Type = 0xB;
      GDT[4].L = 0;
      GDT[4].D = 1; // 32-bit
      GDT[4].DPL = 3;
      GDT[4].G = 1;
      GDT[4].Limit0 = 0xFFFF;
      GDT[4].Limit1 = 0xF;

      // Entry 5: 32-bit Data Segment (Selector 0x2B)
      GDT[5].P = 1;
      GDT[5].S = 1;
      GDT[5].Type = 0x3;
      GDT[5].D = 1; // 32-bit
      GDT[5].DPL = 3;
      GDT[5].G = 1;
      GDT[5].Limit0 = 0xFFFF;
      GDT[5].Limit1 = 0xF;

      Thread->CurrentFrame->State.cs_idx = (4 << 3) | 3;
      Thread->CurrentFrame->State.ss_idx = (5 << 3) | 3;
      Thread->CurrentFrame->State.ds_idx = (5 << 3) | 3;
      Thread->CurrentFrame->State.es_idx = (5 << 3) | 3;
  } else {
      // Entry 1: 64-bit Code Segment (Selector 0x8)
      GDT[1].P = 1;
      GDT[1].S = 1;
      GDT[1].Type = 0xB; // Code, Execute/Read, Accessed
      GDT[1].L = 1;      // Long Mode (64-bit)
      GDT[1].DPL = 3;

      // Entry 2: 64-bit Data Segment (Selector 0x10)
      GDT[2].P = 1;
      GDT[2].S = 1;
      GDT[2].Type = 0x3; // Data, Read/Write, Accessed
      GDT[2].DPL = 3;

      Thread->CurrentFrame->State.cs_idx = (1 << 3) | 3;
      Thread->CurrentFrame->State.ss_idx = (2 << 3) | 3;
      Thread->CurrentFrame->State.ds_idx = (2 << 3) | 3;
      Thread->CurrentFrame->State.es_idx = (2 << 3) | 3;
  }

  // Initialize internal JIT RSB stack (callret_sp) using native mmap
  void* CRStackRaw = ::mmap(nullptr, 0x100000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  uint64_t CRStack = (uint64_t)CRStackRaw;
  Thread->CurrentFrame->State.callret_sp = CRStack + 0x100000 - 0x100;

  fprintf(stderr, "[FEXLibrary] Thread created. GDT: %p, CS: 0x%x, RIP: 0x%llx, RSP: 0x%llx\n", 
          GDT, Thread->CurrentFrame->State.cs_idx, (unsigned long long)EntryPoint, (unsigned long long)StackPointer);

  return Res;
}

FEX_DEFAULT_VISIBILITY void FEX_ThreadDestroy(FEX_Thread* Thread) {
  if (Thread->Thread->CurrentFrame->State.segment_arrays[0]) {
    free(Thread->Thread->CurrentFrame->State.segment_arrays[0]);
  }
  Thread->Ctx->DestroyThread(Thread->Thread);
  delete Thread;
}

FEX_DEFAULT_VISIBILITY FEX_Result FEX_ThreadExecute(FEX_Thread* Thread) {
  fprintf(stderr, "[FEX-JIT] Entering RIP: 0x%llx (RSP: 0x%llx)\n", 
          (unsigned long long)Thread->Thread->CurrentFrame->State.rip,
          (unsigned long long)Thread->Thread->CurrentFrame->State.gregs[4]);
  
  fprintf(stderr, "[FEX-JIT] Calling Ctx->ExecuteThread...\n");
  Thread->Ctx->ExecuteThread(Thread->Thread);
  fprintf(stderr, "[FEX-JIT] Ctx->ExecuteThread returned.\n");
  return FEX_SUCCESS;
}

FEX_DEFAULT_VISIBILITY FEX_Result FEX_ThreadSetReg(FEX_Thread* Thread, const char* RegName, uint64_t Value) {
  auto& State = Thread->Thread->CurrentFrame->State;
  if (strcmp(RegName, "rax") == 0) State.gregs[0] = Value;
  else if (strcmp(RegName, "rsp") == 0) State.gregs[4] = Value;
  else if (strcmp(RegName, "rip") == 0) State.rip = Value;
  return FEX_SUCCESS;
}

FEX_DEFAULT_VISIBILITY FEX_Result FEX_ThreadGetReg(FEX_Thread* Thread, const char* RegName, uint64_t* OutValue) {
  auto& State = Thread->Thread->CurrentFrame->State;
  if (strcmp(RegName, "rax") == 0) *OutValue = State.gregs[0];
  else if (strcmp(RegName, "rip") == 0) *OutValue = State.rip;
  return FEX_SUCCESS;
}

FEX_DEFAULT_VISIBILITY FEX_Result FEX_ThreadSetTLSBase(FEX_Thread* Thread, uint64_t Base) {
    Thread->Thread->CurrentFrame->State.fs_cached = Thread->Thread->CurrentFrame->State.gs_cached = Base;
    return FEX_SUCCESS;
}

FEX_DEFAULT_VISIBILITY FEX_Result FEX_SetThreadTLSBase(FEX_Thread* Thread, uint64_t Base) {
    return FEX_ThreadSetTLSBase(Thread, Base);
}

FEX_DEFAULT_VISIBILITY FEX_Result FEX_InitWindowsEnvironment(FEX_Context* Ctx, FEX_Thread* Thread, uint64_t ImageBase, int argc, char** argv) {
    g_image_base = ImageBase;
    uintptr_t ta = (uintptr_t)FEX_MapMemory(Ctx, 0x40000000, 0x20000, FEX_MEM_READ | FEX_MEM_WRITE);
    if (!ta) ta = (uintptr_t)FEX_MapMemory(Ctx, 0, 0x20000, FEX_MEM_READ | FEX_MEM_WRITE);

    if (ta) {
        uintptr_t host_ta = ta + FEXCore::Utils::GlobalMemoryBase;
        WinTEB* teb = (WinTEB*)host_ta; 
        WinPEB* peb = (WinPEB*)(host_ta + 0x8000);
        g_win_globals_guest = ta + 0x10000;
        WinGlobals* globals = (WinGlobals*)(host_ta + 0x10000);

        memset((void*)host_ta, 0, 0x20000);

        if (g_Is32Bit) {
            // 32-bit TEB (at ta):
            *(uint32_t*)(host_ta + 0x18) = (uint32_t)ta; // Self pointer at 0x18
            *(uint32_t*)(host_ta + 0x30) = (uint32_t)(ta + 0x8000); // PEB pointer at 0x30
            
            // 32-bit PEB (at peb):
            *(uint32_t*)((uintptr_t)peb + 0x08) = (uint32_t)ImageBase; // ImageBase
            *(uint32_t*)((uintptr_t)peb + 0x18) = 0x1337BEEF; // ProcessHeap

            // 32-bit WinGlobals layout:
            // offset 0: argc, offset 4: argv_ptr, offset 8: cmd_line, offset 520: argv_array
            uintptr_t host_globals = host_ta + 0x10000;
            *(uint32_t*)(host_globals + 0) = argc;
            strcpy((char*)(host_globals + 8), "notepad.exe");
            *(uint32_t*)(host_globals + 520) = (uint32_t)(g_win_globals_guest + 8);
            *(uint32_t*)(host_globals + 4) = (uint32_t)(g_win_globals_guest + 520);
        } else {
            teb->Self = ta; teb->ProcessEnvironmentBlock = (uint64_t)(ta + 0x8000);
            peb->ImageBaseAddress = ImageBase; peb->OSMajorVersion = 10; peb->ProcessHeap = 0x1337BEEF;

            globals->argc = argc;
            strcpy(globals->cmd_line, "notepad.exe");
            globals->argv_array[0] = g_win_globals_guest + offsetof(WinGlobals, cmd_line);
            globals->argv_ptr = g_win_globals_guest + offsetof(WinGlobals, argv_array);
        }

        if (g_Is32Bit) {
            Thread->Thread->BaseFrameState.State.fs_cached = ta;
            Thread->Thread->BaseFrameState.State.gs_cached = ta;
            Thread->Thread->BaseFrameState.State.fs_idx = (5 << 3) | 3;
            // Map 32-bit registers to correct entry values
            Thread->Thread->BaseFrameState.State.gregs[2] = ta; // EDX = TEB
            Thread->Thread->BaseFrameState.State.gregs[4] = 0x24000; // ESP
            Thread->Thread->BaseFrameState.State.gregs[8] = 0x24000; // EBP
            
            // PEB is at TEB + 0x30
            Thread->Thread->BaseFrameState.State.gregs[3] = ta + 0x30; // EBX = PEB
        } else {
            Thread->Thread->BaseFrameState.State.gs_cached = ta;
            Thread->Thread->BaseFrameState.State.gregs[2] = ta; 
            Thread->Thread->BaseFrameState.State.gregs[3] = (uint64_t)(ta + 0x8000); 
        }

        fprintf(stderr, "[FEXWindows] Ready. TEB: 0x%llx (Is32Bit: %d)\n", (unsigned long long)ta, (int)g_Is32Bit);
    }
    return FEX_SUCCESS;
}

FEX_DEFAULT_VISIBILITY FEX_Result FEX_LoadELF(FEX_Context* Ctx, const char* Path, uint64_t* OutEntryPoint) { return FEX_ERROR_GENERIC; }
FEX_DEFAULT_VISIBILITY void FEX_ContextDestroy(FEX_Context* Ctx) { delete Ctx; }
FEX_DEFAULT_VISIBILITY uint64_t FEX_MapMemory(FEX_Context* Ctx, uint64_t GuestAddr, uint64_t Size, int Perms) { 
  int prot = 0;
  if (Perms & FEX_MEM_READ) prot |= PROT_READ;
  if (Perms & FEX_MEM_WRITE) prot |= PROT_WRITE;
  if (Perms & FEX_MEM_EXEC) prot |= PROT_EXEC;

#if defined(__APPLE__) && defined(__aarch64__)
  // Apple Silicon Hardened Runtime requires MAP_JIT for PROT_EXEC, which enforces W^X.
  // The host CPU never executes guest memory natively, so we only need READ/WRITE permissions natively.
  prot &= ~PROT_EXEC;
#endif

  // Enforce 32-bit (under 4GB) allocations for 32-bit guest modes
  if (g_Is32Bit && !GuestAddr) {
      static uint64_t s_32BitGuestAllocator = 0x10000000; // Start at 256 MB guest address
      // Try multiple candidate addresses in the 32-bit range
      for (int attempt = 0; attempt < 64; attempt++) {
          uint64_t guestCandidate = s_32BitGuestAllocator;
          if (guestCandidate + Size > 0xFFFF0000ULL) break; // Stay under 4GB guest range
          
          // Convert guest candidate to host hint address
          uint64_t hostHint = guestCandidate + FEXCore::Utils::GlobalMemoryBase;
          
          int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#if defined(__APPLE__) && defined(__aarch64__)
          if (prot & PROT_EXEC) flags |= MAP_JIT;
#endif
          void* res = FEXCore::Allocator::mmap((void*)hostHint, Size, prot, flags, -1, 0);
          if (res != MAP_FAILED) {
              uint64_t hostAddr = (uint64_t)res;
              uint64_t guestAddr = hostAddr - FEXCore::Utils::GlobalMemoryBase;
              if (hostAddr == hostHint && guestAddr + Size <= 0xFFFF0000ULL) {
                  s_32BitGuestAllocator = ((guestAddr + Size + 0xFFFF) & ~0xFFFFULL);
                  fprintf(stderr, "[FEX-MapMem] 32-bit alloc: guest=0x%llx host=0x%llx size=0x%llx\n",
                          (unsigned long long)guestAddr, (unsigned long long)hostAddr, (unsigned long long)Size);
                  return guestAddr;
              }
              // Got a different address, unmap and try next slot
              FEXCore::Allocator::munmap(res, Size);
          }
          s_32BitGuestAllocator += 0x1000000; // Skip 16MB ahead
      }
      fprintf(stderr, "[FEX] WARNING: Could not allocate 0x%llx bytes below 4GB for 32-bit guest!\n", (unsigned long long)Size);
  }

  if (GuestAddr) {
      // Convert guest address to host hint
      uint64_t hostHint = GuestAddr + FEXCore::Utils::GlobalMemoryBase;
      // Use MAP_FIXED to force the allocation at the specific guest address.
      // Do NOT use MAP_JIT with MAP_FIXED because macOS blocks that combination.
      // Since FEXCore only reads/writes this memory natively, PROT_EXEC is not strictly required
      // for the native mmap call on Apple Silicon, FEX handles execution internally.
      int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED;
      
      int native_prot = prot;
#if defined(__APPLE__) && defined(__aarch64__)
      // macOS forbids MAP_FIXED + MAP_JIT and MAP_FIXED + PROT_EXEC without MAP_JIT
      native_prot &= ~PROT_EXEC;
#endif

      // Use native ::mmap because FEXCore::Allocator::mmap might return Linux error codes instead of MAP_FAILED
      void* res = ::mmap((void*)hostHint, Size, native_prot, flags, -1, 0);
      if (res != MAP_FAILED) {
          fprintf(stderr, "[FEX-MapMem] fixed alloc: guest=0x%llx host=0x%llx size=0x%llx\n",
                  (unsigned long long)GuestAddr, (unsigned long long)(uint64_t)res, (unsigned long long)Size);
          return GuestAddr;
      } else {
          perror("[FEX-MapMem] fixed mmap failed");
      }
      return 0;
  }

  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#if defined(__APPLE__) && defined(__aarch64__)
  if (prot & PROT_EXEC) flags |= MAP_JIT;
#endif
  void* res = FEXCore::Allocator::mmap(nullptr, Size, prot, flags, -1, 0);
  if (res == MAP_FAILED) {
      return 0;
  }
  return (uint64_t)res - FEXCore::Utils::GlobalMemoryBase; 
}
FEX_DEFAULT_VISIBILITY FEX_Result FEX_WriteMemory(FEX_Context* Ctx, uint64_t GuestAddr, const void* Data, uint64_t Size) { memcpy((void*)(GuestAddr + FEXCore::Utils::GlobalMemoryBase), Data, Size); return FEX_SUCCESS; }
FEX_DEFAULT_VISIBILITY FEX_Result FEX_ReadMemory(FEX_Context* Ctx, uint64_t GuestAddr, void* Data, uint64_t Size) { memcpy(Data, (void*)(GuestAddr + FEXCore::Utils::GlobalMemoryBase), Size); return FEX_SUCCESS; }
FEX_DEFAULT_VISIBILITY FEX_Result FEX_UnmapMemory(FEX_Context* Ctx, uint64_t GuestAddr, uint64_t Size) { FEXCore::Allocator::munmap((void*)(GuestAddr + FEXCore::Utils::GlobalMemoryBase), Size); return FEX_SUCCESS; }

}
