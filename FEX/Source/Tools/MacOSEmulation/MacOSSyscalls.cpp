#include "MacOSSyscalls.h"
#include <iostream>
#include <FEXCore/Core/CoreState.h>
#include <FEXCore/Core/X86Enums.h>
#include <FEXCore/Core/Context.h>
#include <FEXCore/Config/Config.h>
#include <map>
#include <string>

#include "MetalBridge.h"
#include "Metal_D3D11.h"

extern "C" {
#include "winemac/macdrv_cocoa.h"

// Stubs for missing symbols from winemac C files
pthread_mutex_t ime_composition_rect_mutex;
CGRect ime_composition_rect;
int macdrv_layout_list_needs_update = 1;
bool retina_on = false;
bool retina_enabled = false;
bool macdrv_err_on = true;

// Config variables
int topmost_float_inactive = TOPMOST_FLOAT_INACTIVE_NONFULLSCREEN;
bool capture_displays_for_fullscreen = false;
bool left_option_is_alt = true;
bool right_option_is_alt = true;
bool left_command_is_ctrl = false;
bool right_command_is_ctrl = false;
bool allow_immovable_windows = false;
bool use_confinement_cursor_clipping = false;
bool cursor_clipping_locks_windows = false;
bool use_precise_scrolling = true;
int gl_surface_mode = GL_SURFACE_BEHIND;
CFDictionaryRef localized_strings = nullptr;
bool enable_app_nap = false;

void macdrv_create_remote_layer(void* hwnd, unsigned int context_id) {}
void macdrv_release_remote_layer(void* hwnd, unsigned int context_id) {}

}

static macdrv_event_queue g_MacEventQueue = nullptr;
static void MacDrvEventHandler(const macdrv_event *event) {
    // We poll the queue manually, so this can be empty or just log
    fprintf(stderr, "[MacOSSyscalls] Async Event Received: %d\n", event->type);
}

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static std::string TranslatePath(const uint16_t* wstr) {
    if (!wstr) return "";
    std::string s;
    while (*wstr) {
        s += (char)*wstr;
        wstr++;
    }
    size_t pos;
    while ((pos = s.find("\\")) != std::string::npos) {
        s.replace(pos, 1, "/");
    }
    if (s.length() >= 3 && (s.substr(0, 3) == "C:/" || s.substr(0, 3) == "c:/")) {
        s = "/Users/firataktug/Desktop/winemacosport/wine/drive_c/" + s.substr(3);
    }
    return s;
}

extern "C" {
void FEX_CleanExit(int status);
const char* FEX_GetStubName(uint64_t Address);
}

namespace MacOSEmulation {

bool MacOSSyscalls::Init() {
    std::cout << "[MacOSEmulation] Init MacOSSyscalls - Rosetta 2 Syscall Passthrough Mode" << std::endl;
    macdrv_start_cocoa_app(0);
    g_MacEventQueue = macdrv_create_event_queue(MacDrvEventHandler);
    return true;
}

static std::map<uint16_t, uint64_t> g_AtomToWndProc;
static std::map<uint64_t, uint64_t> g_HwndToWndProc;

struct FexCreateStructW {
    uint64_t lpCreateParams;
    uint64_t hInstance;
    uint64_t hMenu;
    uint64_t hwndParent;
    int32_t cy;
    int32_t cx;
    int32_t y;
    int32_t x;
    uint32_t style;
    uint64_t lpszName;
    uint64_t lpszClass;
    uint32_t dwExStyle;
};

static std::map<uint64_t, FexCreateStructW> g_HwndToCreateStruct;
static uint16_t g_NextAtom = 0xC000;
static uint64_t g_FocusHwnd = 0;
static std::vector<WinMSG> g_MessageQueue;
static std::vector<uint16_t> g_EditText;

std::map<uint64_t, macdrv_window> g_HwndToMacWin;

extern "C" macdrv_window GetMacDrvWindow(uint64_t hwnd) {
    if (g_HwndToMacWin.count(hwnd)) return g_HwndToMacWin[hwnd];
    return nullptr;
}

uint64_t MacOSSyscalls::HandleSyscall(FEXCore::Core::CpuStateFrame* Frame, FEXCore::HLE::SyscallArguments* Args) {
    uint64_t sys_num = Frame->State.gregs[FEXCore::X86State::REG_RAX];
    
    const char* stub_name = FEX_GetStubName(Frame->State.rip & ~0xFFFULL);
    if (stub_name) {
        uint64_t ret = 0;
        bool is64Bit = true; 
        uint64_t args_buf[12] = {0};
        
        if (is64Bit) {
            // Windows x64 ABI: RCX, RDX, R8, R9, [RSP+40], [RSP+48]...
            // Note: RCX is clobbered by 'syscall' instruction, so our STUB saves it to R10!
            args_buf[0] = Frame->State.gregs[FEXCore::X86State::REG_R10];
            args_buf[1] = Frame->State.gregs[FEXCore::X86State::REG_RDX];
            args_buf[2] = Frame->State.gregs[FEXCore::X86State::REG_R8];
            args_buf[3] = Frame->State.gregs[FEXCore::X86State::REG_R9];
            
            uint64_t rsp = Frame->State.gregs[FEXCore::X86State::REG_RSP];
            if (rsp > 0x10000) {
                for (int i=4; i<12; i++) {
                    uint64_t arg_addr = rsp + 16 + (i * 8);
                    args_buf[i] = *reinterpret_cast<uint64_t*>(arg_addr);
                }
            }
        }
        
        fprintf(stderr, "[MacOSSyscalls] !!! ENTERING HANDLE SYSCALL !!! %s (RAX=0x%llx RIP=0x%llx)\n", stub_name, sys_num, (uint64_t)Frame->State.rip);
        fprintf(stderr, "                ARGS: [0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx]\n", args_buf[0], args_buf[1], args_buf[2], args_buf[3], args_buf[4], args_buf[5]);
        
        // ucrtbase.dll & msvcrt.dll
        if (strstr(stub_name, "__p___argc")) {
            static int dummy_argc = 1;
            ret = (uint64_t)&dummy_argc;
        } else if (strstr(stub_name, "__p___argv") || strstr(stub_name, "__p___wargv")) {
            static const char* dummy_argv[] = {"notepad.exe", nullptr};
            static const char** p_dummy_argv = dummy_argv;
            ret = (uint64_t)&p_dummy_argv;
        } else if (strstr(stub_name, "_set_app_type")) {
            ret = 0;
        } else if (strstr(stub_name, "_configure_narrow_argv") || strstr(stub_name, "_initialize_narrow_environment") || strstr(stub_name, "_configure_wide_argv") || strstr(stub_name, "_initialize_wide_environment")) {
            ret = 0;
        } else if (strstr(stub_name, "_get_initial_narrow_environment") || strstr(stub_name, "_get_initial_wide_environment")) {
            static char* dummy_env[] = {nullptr};
            ret = (uint64_t)dummy_env;
        } else if (strstr(stub_name, "_initterm")) {
            ret = 0;
        }
        // kernel32.dll
        else if (strstr(stub_name, "IsWow64Process")) {
            uint32_t* Wow64Process = (uint32_t*)args_buf[1]; // PBOOL is 2nd arg
            if (Wow64Process) *Wow64Process = 0; // FALSE (we are 64-bit native)
            ret = 1; // TRUE (Success)
        } else if (strstr(stub_name, "GetCommandLineW")) {
            static const char16_t cmdline[] = u"C:\\Windows\\notepad.exe";
            ret = (uint64_t)cmdline;
        } else if (strstr(stub_name, "GetCommandLineA")) {
            static const char cmdline[] = "C:\\Windows\\notepad.exe";
            ret = (uint64_t)cmdline;
        } else if (strstr(stub_name, "GetModuleHandle")) {
            ret = 0x140000000;
        } else if (strstr(stub_name, "GetProcAddress")) {
            ret = 0;
        } else if (strstr(stub_name, "ExitProcess") || strstr(stub_name, "!exit")) {
            FEX_CleanExit((int)args_buf[0]);
        }
        // advapi32.dll
        else if (strstr(stub_name, "RegCreateKey") || strstr(stub_name, "RegOpenKey") || strstr(stub_name, "RegQueryValue")) {
            ret = 2; // ERROR_FILE_NOT_FOUND (forces caller to handle error instead of reading invalid handle)
        }
        // user32.dll (GUI Native Bridge)
        else if (strstr(stub_name, "CreateWindowEx")) {
            // CreateWindowExW: lpClassName is arg 1 (args_buf[1])
            uint64_t class_name_ptr = args_buf[1];
            
            macdrv_window_features wf = {0};
            wf.title_bar = 1;
            wf.close_button = 1;
            wf.minimize_button = 1;
            wf.resizable = 1;
            wf.maximize_button = 1;
            wf.shadow = 1;

            int x = (int)args_buf[4];
            int y = (int)args_buf[5];
            int width = (int)args_buf[6];
            int height = (int)args_buf[7];

            // CW_USEDEFAULT handling
            if (x == (int)0x80000000) x = 100;
            if (y == (int)0x80000000) y = 100;
            if (width == (int)0x80000000) width = 800;
            if (height == (int)0x80000000) height = 600;

            CGRect frame = CGRectMake(x, y, width, height);
            
            // Dummy HWND - just use a unique pointer/ID
            static uint64_t next_hwnd = 0x10000;
            uint64_t hwnd = next_hwnd++;
            
            macdrv_window mac_win = macdrv_create_cocoa_window(&wf, frame, (void*)hwnd, g_MacEventQueue);
            macdrv_order_cocoa_window(mac_win, nullptr, nullptr, true);
            
            // Store mapping from HWND to macdrv_window
            g_HwndToMacWin[hwnd] = mac_win;
            FexCreateStructW cs = {0};
            cs.dwExStyle = args_buf[0];
            cs.lpszClass = args_buf[1];
            cs.lpszName = args_buf[2];
            cs.style = args_buf[3];
            cs.x = args_buf[4];
            cs.y = args_buf[5];
            cs.cx = args_buf[6];
            cs.cy = args_buf[7];
            cs.hwndParent = args_buf[8];
            cs.hMenu = args_buf[9];
            cs.hInstance = args_buf[10];
            cs.lpCreateParams = args_buf[11];
            g_HwndToCreateStruct[hwnd] = cs;
            fprintf(stderr, "[MacOSSyscalls] Stored CREATESTRUCTW for HWND 0x%llx (lpClass=0x%llx)\n", hwnd, cs.lpszClass);
            
            // Check if class name is an ATOM (lower 16-bits only)
            if (class_name_ptr < 0x10000) {
                uint16_t atom = (uint16_t)class_name_ptr;
                if (g_AtomToWndProc.count(atom)) {
                    g_HwndToWndProc[hwnd] = g_AtomToWndProc[atom];
                    fprintf(stderr, "[MacOSSyscalls] CreateWindowEx: Mapped HWND 0x%llx to WndProc 0x%llx (Atom: 0x%x)\n", hwnd, g_AtomToWndProc[atom], atom);
                }
            } else {
                if (g_AtomToWndProc.count(0xC000)) {
                    g_HwndToWndProc[hwnd] = g_AtomToWndProc[0xC000];
                    fprintf(stderr, "[MacOSSyscalls] CreateWindowEx: Fallback mapped HWND 0x%llx to WndProc 0x%llx\n", hwnd, g_AtomToWndProc[0xC000]);
                }
            }
            if (args_buf[8] != 0) { // hwndParent != 0 -> It's a child window (Edit control)
                g_FocusHwnd = hwnd;
            }
            ret = hwnd;
        } else if (strstr(stub_name, "ShowWindow") || strstr(stub_name, "UpdateWindow")) {
            ret = 1;
        } else if (strstr(stub_name, "TranslateMessage")) {
            struct WinMSG* msg = (struct WinMSG*)args_buf[0];
            if (msg && msg->message == 0x0100 /* WM_KEYDOWN */) {
                uint16_t charCode = 0;
                uint32_t vk = msg->wParam;
                if (vk >= 'A' && vk <= 'Z') charCode = vk + 32; // Lowercase for now
                else if (vk >= '0' && vk <= '9') charCode = vk;
                else if (vk == 0x20) charCode = ' ';
                else if (vk == 0x0D) charCode = '\r';
                else if (vk == 0x08) charCode = '\b';
                
                if (charCode != 0) {
                    WinMSG charMsg = *msg;
                    charMsg.message = 0x0102; // WM_CHAR
                    charMsg.wParam = charCode;
                    g_MessageQueue.push_back(charMsg);
                }
            }
            ret = 1;
        } else if (strstr(stub_name, "GetMessage")) {
            struct WinMSG* msg = (struct WinMSG*)args_buf[0];
            if (!g_MessageQueue.empty()) {
                if (msg) *msg = g_MessageQueue.front();
                g_MessageQueue.erase(g_MessageQueue.begin());
                ret = 1;
            } else {
                PumpMacEvents();
                
                macdrv_event* event = nullptr;
                // event_mask_for_type for KEY_PRESS, KEY_RELEASE, MOUSE_BUTTON, MOUSE_MOVED_ABSOLUTE, WINDOW_CLOSE_REQUESTED
                uint64_t mask = 0xFFFFFFFFFFFFFFFFULL; // ALL events
                if (macdrv_copy_event_from_queue(g_MacEventQueue, mask, &event) && event) {
                    uint64_t hwnd = (uint64_t)macdrv_get_window_hwnd(event->window);
                    
                    if (event->type == KEY_PRESS || event->type == KEY_RELEASE) {
                        msg->hwnd = hwnd;
                        msg->message = (event->type == KEY_PRESS) ? 0x0100 : 0x0101; // WM_KEYDOWN/UP
                        msg->wParam = event->key.keycode; // This is a Mac keycode, ideally map to VK
                        msg->lParam = 0;
                        msg->time = event->key.time_ms;
                        ret = 1;
                    } else if (event->type == MOUSE_BUTTON) {
                        msg->hwnd = hwnd;
                        msg->message = event->mouse_button.pressed ? 0x0201 : 0x0202; // WM_LBUTTONDOWN/UP
                        msg->wParam = event->mouse_button.button; // Needs map
                        msg->lParam = ((event->mouse_button.y & 0xFFFF) << 16) | (event->mouse_button.x & 0xFFFF);
                        msg->time = event->mouse_button.time_ms;
                        ret = 1;
                    } else if (event->type == MOUSE_MOVED_ABSOLUTE) {
                        msg->hwnd = hwnd;
                        msg->message = 0x0200; // WM_MOUSEMOVE
                        msg->wParam = 0;
                        msg->lParam = ((event->mouse_moved.y & 0xFFFF) << 16) | (event->mouse_moved.x & 0xFFFF);
                        msg->time = event->mouse_moved.time_ms;
                        ret = 1;
                    } else if (event->type == WINDOW_CLOSE_REQUESTED) {
                        msg->hwnd = hwnd;
                        msg->message = 0x0112; // WM_SYSCOMMAND
                        msg->wParam = 0xF060; // SC_CLOSE
                        msg->lParam = 0;
                        ret = 1;
                    } else if (event->type == WINDOW_FRAME_CHANGED) {
                        msg->hwnd = hwnd;
                        msg->message = 0x0005; // WM_SIZE
                        msg->wParam = 0;
                        msg->lParam = ((int(event->window_frame_changed.frame.size.height) & 0xFFFF) << 16) | (int(event->window_frame_changed.frame.size.width) & 0xFFFF);
                        ret = 1;
                    } else if (event->type == WINDOW_GOT_FOCUS) {
                        g_FocusHwnd = hwnd;
                        msg->hwnd = hwnd;
                        msg->message = 0x0007; // WM_SETFOCUS
                        ret = 1;
                    } else {
                        // Unhandled event
                        msg->hwnd = 0;
                        msg->message = 0;
                        ret = 1;
                    }
                    macdrv_release_event(event);
                } else {
                    usleep(16000);
                    // Generate paints if idle
                    if (!g_HwndToWndProc.empty()) {
                        msg->hwnd = g_HwndToWndProc.begin()->first;
                        msg->message = 0x000F; // WM_PAINT
                        ret = 1;
                    } else {
                        ret = 0;
                    }
                }
            }
            struct WinMSG* ret_msg = (struct WinMSG*)args_buf[0];
            if (ret == 1 && ret_msg) {
                // Route Keyboard events to focused HWND
                if ((ret_msg->message == 0x0100 || ret_msg->message == 0x0101 || ret_msg->message == 0x0102) && g_FocusHwnd != 0) {
                    ret_msg->hwnd = g_FocusHwnd;
                }
            }
            ret = 1; // Return > 0 so the message loop continues
        } else if (strstr(stub_name, "DispatchMessage")) {
            struct WinMSG* msg = (struct WinMSG*)args_buf[0];
            if (msg && g_HwndToWndProc.count(msg->hwnd)) {
                if (msg->hwnd == g_FocusHwnd && g_FocusHwnd != 0) {
                    if (msg->message == 0x0102) { // WM_CHAR
                        uint16_t c = (uint16_t)msg->wParam;
                        if (c == '\b') {
                            if (!g_EditText.empty()) g_EditText.pop_back();
                        } else if (c == '\r') {
                            g_EditText.push_back('\n');
                        } else {
                            g_EditText.push_back(c);
                        }
                        ret = 0;
                        Frame->State.gregs[FEXCore::X86State::REG_RAX] = ret;
                        return ret;
                    } else if (msg->message == 0x000F) { // WM_PAINT
                        uint64_t hdc = (uint64_t)Metal_GetDC(msg->hwnd);
                        if (hdc) {
                            if (!g_EditText.empty()) {
                                Metal_ExtTextOutW((void*)hdc, 5, 5, 0, nullptr, g_EditText.data(), g_EditText.size(), nullptr);
                            }
                            Metal_PresentDC(msg->hwnd, (void*)hdc);
                            Metal_ReleaseDC(msg->hwnd, (void*)hdc);
                        }
                        ret = 0;
                        Frame->State.gregs[FEXCore::X86State::REG_RAX] = ret;
                        return ret;
                    }
                }
                
                uint64_t wndproc = g_HwndToWndProc[msg->hwnd];
                fprintf(stderr, "[MacOSSyscalls] DispatchMessage: Tail-calling WndProc 0x%llx for HWND 0x%llx (MSG: 0x%x)\n", wndproc, msg->hwnd, msg->message);
                
                // Set up arguments for WndProc(HWND, UINT, WPARAM, LPARAM)
                Frame->State.gregs[FEXCore::X86State::REG_RCX] = msg->hwnd;
                Frame->State.gregs[FEXCore::X86State::REG_RDX] = msg->message;
                Frame->State.gregs[FEXCore::X86State::REG_R8]  = msg->wParam;
                Frame->State.gregs[FEXCore::X86State::REG_R9]  = msg->lParam;
                
                uint64_t rsp = Frame->State.gregs[FEXCore::X86State::REG_RSP];
                uint64_t* stack = (uint64_t*)rsp;
                uint64_t saved_rdi = stack[0];
                uint64_t ret_to_winmain = stack[1];
                
                // Shift stack down by 8 bytes
                rsp -= 8;
                stack = (uint64_t*)rsp;
                stack[0] = saved_rdi;
                stack[1] = wndproc;
                stack[2] = ret_to_winmain;
                
                Frame->State.gregs[FEXCore::X86State::REG_RSP] = rsp;
                
                return 0;
            }
            ret = 0;
        } else if (strstr(stub_name, "DefWindowProc")) {
            uint64_t hwnd = args_buf[0];
            uint32_t msg = (uint32_t)args_buf[1];
            if (msg == 0x0081) { // WM_NCCREATE
                ret = 1;
            } else if (msg == 0x000F) { // WM_PAINT
                fprintf(stderr, "[MacOSSyscalls] DefWindowProc: Handling WM_PAINT for HWND 0x%llx\n", hwnd);
                // Clear the update region by calling BeginPaint and EndPaint
                uint64_t hdc = (uint64_t)Metal_BeginPaint(hwnd, nullptr);
                if (hdc) {
                    Metal_EndPaint(hwnd, nullptr);
                }
                ret = 0;
            } else {
                ret = 0;
            }
        } else if (strstr(stub_name, "PostQuitMessage")) {
            ret = 0;
        } else if (strstr(stub_name, "RegisterWindowMessage")) {
            ret = 0xC001;
        } else if (strstr(stub_name, "RegisterClass")) {
            // lpWndClass is arg 0 (args_buf[0])
            uint64_t lpWndClass = args_buf[0];
            if (lpWndClass) {
                // WNDCLASSW / WNDCLASSEXW both have lpfnWndProc at offset 8
                uint64_t wndproc = *reinterpret_cast<uint64_t*>(lpWndClass + 8);
                uint16_t atom = g_NextAtom++;
                g_AtomToWndProc[atom] = wndproc;
                fprintf(stderr, "[MacOSSyscalls] RegisterClass: Saved WndProc 0x%llx as Atom 0x%x\n", wndproc, atom);
                ret = atom;
            } else {
                ret = 0xC000;
            }
        } else if (strstr(stub_name, "BeginPaint")) {
            // BeginPaint(hwnd, lpPaint)
            uint64_t hwnd = args_buf[0];
            uint64_t lpPaint = args_buf[1];
            uint64_t hdc = (uint64_t)Metal_BeginPaint(hwnd, nullptr);
            if (lpPaint) {
                *reinterpret_cast<uint64_t*>(lpPaint) = hdc; // Save HDC in PAINTSTRUCT
            }
            ret = hdc;
        } else if (strstr(stub_name, "EndPaint")) {
            // EndPaint(hwnd, lpPaint)
            uint64_t lpPaint = args_buf[1];
            if (lpPaint) {
                uint64_t hdc = *reinterpret_cast<uint64_t*>(lpPaint);
                Metal_EndPaint(args_buf[0], (void*)hdc);
            }
            ret = 1;
        } else if (strstr(stub_name, "GetDC")) {
            ret = (uint64_t)Metal_GetDC(args_buf[0]);
        } else if (strstr(stub_name, "ReleaseDC")) {
            Metal_ReleaseDC(args_buf[0], (void*)args_buf[1]);
            ret = 1;
        } else if (strstr(stub_name, "ExtTextOutW")) {
            // ExtTextOutW(hdc, x, y, options, lprect, lpString, c, lpDx)
            Metal_ExtTextOutW((void*)args_buf[0], (int)args_buf[1], (int)args_buf[2], (uint32_t)args_buf[3], (void*)args_buf[4], (const uint16_t*)args_buf[5], (int)args_buf[6], (const int*)args_buf[7]);
            ret = 1;
        } else if (strstr(stub_name, "FillRect")) {
            Metal_FillRect((void*)args_buf[0], (void*)args_buf[1], (uint32_t)args_buf[2]);
            ret = 1;
        } else if (strstr(stub_name, "CreateSolidBrush")) {
            // HBRUSH CreateSolidBrush(COLORREF color)
            ret = args_buf[0]; // Cheating: brush handle IS the color
        } else if (strstr(stub_name, "CreateFontIndirectW") || strstr(stub_name, "SelectObject") || strstr(stub_name, "GetStockObject") || strstr(stub_name, "CreatePen")) {
            ret = 0x3000; // Fake GDI Object
        } else if (strstr(stub_name, "SetBkMode")) {
            Metal_SetBkMode((void*)args_buf[0], (int)args_buf[1]);
            ret = 1;
        } else if (strstr(stub_name, "SetTextColor")) {
            Metal_SetTextColor((void*)args_buf[0], (uint32_t)args_buf[1]);
            ret = 1;
        } else if (strstr(stub_name, "SetBkColor")) {
            Metal_SetBkColor((void*)args_buf[0], (uint32_t)args_buf[1]);
            ret = 1;
        } else if (strstr(stub_name, "GetTextMetricsW")) {
            uint32_t* tm = (uint32_t*)args_buf[1];
            if (tm) {
                tm[0] = 16; // tmHeight
                tm[1] = 12; // tmAscent
                tm[2] = 4;  // tmDescent
                tm[3] = 0;  // tmInternalLeading
                tm[4] = 0;  // tmExternalLeading
                tm[5] = 8;  // tmAveCharWidth
                tm[6] = 16; // tmMaxCharWidth
            }
            ret = 1;
        } else if (strstr(stub_name, "GetSystemMetrics")) {
            ret = 1; // Fake metric
        } else if (strstr(stub_name, "LoadStringW")) {
            // lpBuffer is arg 2
            uint16_t* buf = (uint16_t*)args_buf[2];
            if (buf) {
                buf[0] = 'A';
                buf[1] = 0;
            }
            ret = 1;
        } else if (strstr(stub_name, "LoadImageW") || strstr(stub_name, "LoadCursorW") || strstr(stub_name, "LoadIconW")) {
            ret = 0x1000; // Fake handle
        } else if (strstr(stub_name, "GetClientRect") || strstr(stub_name, "GetWindowRect")) {
            // lpRect is arg 1
            uint32_t* rect = (uint32_t*)args_buf[1];
            if (rect) {
                rect[0] = 0; // left
                rect[1] = 0; // top
                rect[2] = 800; // right
                rect[3] = 600; // bottom
            }
            fprintf(stderr, "[MacOSSyscalls] %s called for HWND 0x%llx\n", stub_name, args_buf[0]);
            ret = 1; // TRUE
        } else if (strstr(stub_name, "GetMonitorInfoW")) {
            // lpmi is arg 1
            uint32_t* mi = (uint32_t*)args_buf[1];
            if (mi) {
                // cbSize is mi[0]
                mi[1] = 0; mi[2] = 0; mi[3] = 800; mi[4] = 600; // rcMonitor
                mi[5] = 0; mi[6] = 0; mi[7] = 800; mi[8] = 600; // rcWork
                mi[9] = 1; // dwFlags = MONITORINFOF_PRIMARY
            }
            ret = 1; // TRUE
        } else if (strstr(stub_name, "GetMenu")) {
            ret = 0x2000; // Fake Menu Handle
        } else if (strstr(stub_name, "CheckMenuItem") || strstr(stub_name, "EnableMenuItem")) {
            ret = 0; // Previous state
        } else if (strstr(stub_name, "IsDebuggerPresent")) {
            ret = 0;
        } else if (strstr(stub_name, "SetFocus")) {
            uint64_t oldFocus = g_FocusHwnd;
            g_FocusHwnd = args_buf[0];
            fprintf(stderr, "[MacOSSyscalls] SetFocus: HWND 0x%llx (Old: 0x%llx)\n", g_FocusHwnd, oldFocus);
            ret = oldFocus;
        } else if (strstr(stub_name, "GetFocus")) {
            ret = g_FocusHwnd;
        } else if (strstr(stub_name, "CreateFileW")) {
            // HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
            std::string path = TranslatePath((const uint16_t*)args_buf[0]);
            uint32_t access = args_buf[1];
            uint32_t creation = args_buf[4];
            
            int flags = 0;
            if ((access & 0x80000000) && (access & 0x40000000)) flags |= O_RDWR;
            else if (access & 0x40000000) flags |= O_WRONLY;
            else flags |= O_RDONLY;
            
            if (creation == 1 || creation == 2) flags |= O_CREAT; // CREATE_NEW, CREATE_ALWAYS
            
            int fd = open(path.c_str(), flags, 0666);
            if (fd == -1) {
                ret = 0xFFFFFFFFFFFFFFFFULL; // INVALID_HANDLE_VALUE
            } else {
                ret = fd;
            }
        } else if (strstr(stub_name, "ReadFile")) {
            // BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
            int fd = (int)args_buf[0];
            void* buf = (void*)args_buf[1];
            uint32_t toRead = args_buf[2];
            uint32_t* bytesRead = (uint32_t*)args_buf[3];
            
            ssize_t res = read(fd, buf, toRead);
            if (res >= 0) {
                if (bytesRead) *bytesRead = (uint32_t)res;
                ret = 1; // TRUE
            } else {
                ret = 0; // FALSE
            }
        } else if (strstr(stub_name, "WriteFile")) {
            // BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
            int fd = (int)args_buf[0];
            void* buf = (void*)args_buf[1];
            uint32_t toWrite = args_buf[2];
            uint32_t* bytesWritten = (uint32_t*)args_buf[3];
            
            ssize_t res = write(fd, buf, toWrite);
            if (res >= 0) {
                if (bytesWritten) *bytesWritten = (uint32_t)res;
                ret = 1;
            } else {
                ret = 0;
            }
        } else if (strstr(stub_name, "CloseHandle")) {
            // BOOL CloseHandle(HANDLE hObject)
            int fd = (int)args_buf[0];
            if (fd > 2) close(fd);
            ret = 1;
        } else if (strstr(stub_name, "GetFileSize")) {
            // DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh)
            int fd = (int)args_buf[0];
            uint32_t* high = (uint32_t*)args_buf[1];
            struct stat st;
            if (fstat(fd, &st) == 0) {
                if (high) *high = (uint32_t)(st.st_size >> 32);
                ret = (uint32_t)(st.st_size & 0xFFFFFFFF);
            } else {
                ret = 0xFFFFFFFF; // INVALID_FILE_SIZE
            }
        } else if (strstr(stub_name, "GetFileSizeEx")) {
            // BOOL GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize)
            int fd = (int)args_buf[0];
            uint64_t* size = (uint64_t*)args_buf[1];
            struct stat st;
            if (fstat(fd, &st) == 0) {
                if (size) *size = st.st_size;
                ret = 1;
            } else {
                ret = 0;
            }
        } else if (strstr(stub_name, "RegOpenKey") || strstr(stub_name, "RegCreateKey")) {
            uint64_t* hkResult = (uint64_t*)args_buf[3]; // RegOpenKeyExW: 4th arg is phkResult
            // Wait, for RegCreateKeyExW it's 9th arg? No, let's just assume we return success.
            // A fake handle is returned if it's the last arg usually, but just returning 0 is enough to satisfy ERROR_SUCCESS.
            ret = 0; // ERROR_SUCCESS
        } else if (strstr(stub_name, "RegQueryValueEx")) {
            ret = 0; // ERROR_SUCCESS
        } else if (strstr(stub_name, "RegCloseKey")) {
            ret = 0;
        }
        // d3d11.dll
        else if (strstr(stub_name, "D3D11CreateDeviceAndSwapChain")) {
            ret = Metal_D3D11CreateDeviceAndSwapChain((void*)args_buf[0], args_buf[1], (void*)args_buf[2], args_buf[3], (void*)args_buf[4], args_buf[5], args_buf[6], (void*)args_buf[7], (void**)args_buf[8], (void**)args_buf[9], (void*)args_buf[10], (void**)args_buf[11]);
        } else if (strstr(stub_name, "D3D11CreateDevice")) {
            ret = Metal_D3D11CreateDevice((void*)args_buf[0], args_buf[1], (void*)args_buf[2], args_buf[3], (void*)args_buf[4], args_buf[5], args_buf[6], (void**)args_buf[7], (void*)args_buf[8], (void**)args_buf[9]);
        }
        
        Frame->State.gregs[FEXCore::X86State::REG_RAX] = ret;
        fprintf(stderr, "[MacOSSyscalls] !!! EXITING HANDLE SYSCALL !!! RAX=0x%llx RET=0x%llx\n", sys_num, ret);
        return ret;
    }


#if defined(__aarch64__)
    // Apple Silicon passthrough: svc #0x80
    // 
    // x86_64 macOS syscall numbering:
    //   Class 1 (Mach traps):       0x1000000 | trap_number  (e.g., 0x100000a = mach_vm_allocate, trap 10)
    //   Class 2 (BSD syscalls):      0x2000000 | syscall_num  (e.g., 0x2000004 = write, num 4)
    //   Class 3 (machine-dependent): 0x3000000 | num          (e.g., 0x3000003 = thread_fast_set_cthread_self)
    //
    // ARM64 macOS kernel expects in x16:
    //   Mach traps:       NEGATIVE trap number (e.g., x16 = -10 for mach_vm_allocate)
    //   BSD syscalls:     POSITIVE syscall number (e.g., x16 = 4 for write)
    //   Machine-dependent: POSITIVE with special handling
    //
    // Carry flag (bcc/bcs) indicates error on return.
    
    uint64_t arg1 = Args->Argument[1];
    uint64_t arg2 = Args->Argument[2];
    uint64_t arg3 = Args->Argument[3];
    uint64_t arg4 = Args->Argument[4];
    uint64_t arg5 = Args->Argument[5];
    uint64_t arg6 = Args->Argument[6];

    // Extract syscall class and number from x86_64 encoding
    uint32_t syscall_class = (sys_num >> 24) & 0xFF;
    uint32_t syscall_number = sys_num & 0x00FFFFFF;
    
    int64_t arm64_x16 = 0;
    
    switch (syscall_class) {
        case 1: // Mach traps: ARM64 expects NEGATIVE x16
            arm64_x16 = -(int64_t)syscall_number;
            break;
        case 2: // BSD syscalls: ARM64 expects POSITIVE x16
            arm64_x16 = (int64_t)syscall_number;
            break;
        case 3: // Machine-dependent syscalls
            // thread_fast_set_cthread_self is special
            if (syscall_number == 3) {
                Frame->State.gs_cached = arg1;
                Frame->State.fs_cached = arg1;
                fprintf(stderr, "[MacOSSyscalls] Emulated thread_fast_set_cthread_self(0x%llx)\n", arg1);
                return 0;
            }
            arm64_x16 = (int64_t)syscall_number;
            break;
        default:
            fprintf(stderr, "[MacOSSyscalls] ERROR: Unknown syscall class %d for syscall 0x%llx\n", syscall_class, (unsigned long long)sys_num);
            return -1;
    }

    // TRACE
    fprintf(stderr, "[MacOSSyscalls] Passthrough syscall 0x%llx -> ARM64 x16=%lld (class=%d, num=%d) (Args: 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx)\n", 
            (unsigned long long)sys_num, (long long)arm64_x16, syscall_class, syscall_number,
            (unsigned long long)arg1, (unsigned long long)arg2, (unsigned long long)arg3,
            (unsigned long long)arg4, (unsigned long long)arg5, (unsigned long long)arg6);
    
    // DEBUG: Dump critical state BEFORE syscall
    fprintf(stderr, "[MacOSSyscalls] PRE-SVC: Frame=%p, Frame->State.rip=0x%llx, Frame->State.gregs[RSP]=0x%llx, Frame->State.gregs[RAX]=0x%llx\n",
            (void*)Frame, (unsigned long long)Frame->State.rip, 
            (unsigned long long)Frame->State.gregs[FEXCore::X86State::REG_RSP],
            (unsigned long long)Frame->State.gregs[FEXCore::X86State::REG_RAX]);
    
    // DEBUG: Check Pointers structure integrity before syscall
    fprintf(stderr, "[MacOSSyscalls] PRE-SVC Pointers: SyscallHandlerObj=%p, SyscallHandlerFunc=%p, DispatcherLoopTop=%p\n",
            (void*)Frame->Pointers.SyscallHandlerObj, 
            (void*)Frame->Pointers.SyscallHandlerFunc,
            (void*)Frame->Pointers.DispatcherLoopTop);

    // For mach_vm_map/mach_vm_allocate, dump addr pointer before call
    uint64_t pre_addr_val = 0;
    if (syscall_class == 1 && (syscall_number == 10 || syscall_number == 15)) {
        // arg2 (x1) = pointer to address
        if (arg2 != 0) {
            pre_addr_val = *(uint64_t*)arg2;
            fprintf(stderr, "[MacOSSyscalls] PRE-SVC mach_vm_%s: *addr_ptr(0x%llx) = 0x%llx\n",
                    syscall_number == 10 ? "allocate" : "map",
                    (unsigned long long)arg2, (unsigned long long)pre_addr_val);
        }
    }
    
    fflush(stderr);

    register uint64_t x0 asm("x0") = arg1;
    register uint64_t x1 asm("x1") = arg2;
    register uint64_t x2 asm("x2") = arg3;
    register uint64_t x3 asm("x3") = arg4;
    register uint64_t x4 asm("x4") = arg5;
    register uint64_t x5 asm("x5") = arg6;
    register int64_t x16_reg asm("x16") = arm64_x16;
    
    register uint64_t carry_err asm("x8"); // use x8 for carry flag
    register uint64_t ret_x1 asm("x9"); // use x9 for x1 return
    
    asm volatile(
        "svc #0x80\n\t"
        "cset %0, cs\n\t"  // carry set = error
        "mov %2, x1\n\t"   // capture x1 return value
        : "=r" (carry_err), "+r" (x0), "=r" (ret_x1), "+r" (x1), "+r" (x2), "+r" (x3), "+r" (x4), "+r" (x5), "+r" (x16_reg)
        :
        : "memory", "cc", "x6", "x7", "x10", "x11", "x12", "x13", "x14", "x15", "x17"
    );

    uint64_t ret = x0;
    
    // DEBUG: Log return values
    fprintf(stderr, "[MacOSSyscalls] POST-SVC: ret(x0)=0x%llx, x1=0x%llx, carry=%llu\n",
            (unsigned long long)ret, (unsigned long long)ret_x1, (unsigned long long)carry_err);
    
    // For mach_vm_map/mach_vm_allocate, check where memory was mapped
    if (syscall_class == 1 && (syscall_number == 10 || syscall_number == 15)) {
        if (arg2 != 0) {
            uint64_t mapped_addr = *(uint64_t*)arg2;
            fprintf(stderr, "[MacOSSyscalls] POST-SVC mach_vm_%s: kern_return=%llu, *addr_ptr(0x%llx) = 0x%llx (was 0x%llx)\n",
                    syscall_number == 10 ? "allocate" : "map",
                    (unsigned long long)ret, (unsigned long long)arg2, 
                    (unsigned long long)mapped_addr, (unsigned long long)pre_addr_val);
            
            // Check if the mapped address overlaps with FEX dispatcher or JIT regions
            uint64_t disp_top = (uint64_t)Frame->Pointers.DispatcherLoopTop;
            uint64_t disp_base = disp_top & ~0xFFFULL; // Page-align down
            if (mapped_addr != 0 && mapped_addr < disp_base + 0x10000 && mapped_addr + arg3 > disp_base) {
                fprintf(stderr, "[MacOSSyscalls] !!! WARNING: mach_vm_%s mapped at 0x%llx OVERLAPS FEX Dispatcher at 0x%llx !!!\n",
                        syscall_number == 10 ? "allocate" : "map",
                        (unsigned long long)mapped_addr, (unsigned long long)disp_base);
            }
            
            // Also check Frame/State
            uint64_t frame_addr = (uint64_t)Frame;
            if (mapped_addr != 0 && mapped_addr < frame_addr + 0x10000 && mapped_addr + arg3 > frame_addr) {
                fprintf(stderr, "[MacOSSyscalls] !!! WARNING: mach_vm_%s mapped at 0x%llx OVERLAPS Frame at 0x%llx !!!\n",
                        syscall_number == 10 ? "allocate" : "map",
                        (unsigned long long)mapped_addr, (unsigned long long)frame_addr);
            }
        }
    }

    // DEBUG: Check Pointers structure integrity AFTER syscall
    fprintf(stderr, "[MacOSSyscalls] POST-SVC Pointers: SyscallHandlerObj=0x%llx, SyscallHandlerFunc=0x%llx, DispatcherLoopTop=0x%llx, ExitFunctionLink=0x%llx, ExitFunctionLinker=0x%llx\n",
            (unsigned long long)Frame->Pointers.SyscallHandlerObj,
            (unsigned long long)Frame->Pointers.SyscallHandlerFunc,
            (unsigned long long)Frame->Pointers.DispatcherLoopTop,
            (unsigned long long)Frame->Pointers.ExitFunctionLink,
            (unsigned long long)Frame->Pointers.ExitFunctionLinker);
    
    // DEBUG: Check Frame->State integrity after syscall
    fprintf(stderr, "[MacOSSyscalls] POST-SVC State: rip=0x%llx, RSP=0x%llx, RAX=0x%llx\n",
            (unsigned long long)Frame->State.rip,
            (unsigned long long)Frame->State.gregs[FEXCore::X86State::REG_RSP],
            (unsigned long long)Frame->State.gregs[FEXCore::X86State::REG_RAX]);

    fflush(stderr);

    if (syscall_class == 1) {
        // Mach traps: carry flag is ALWAYS set on ARM64, not an error indicator.
        Frame->State.flags[FEXCore::X86State::RFLAG_CF_RAW_LOC] = 0;
    } else if (carry_err) {
        // BSD syscalls: carry flag set means x0 has the errno.
        fprintf(stderr, "[MacOSSyscalls] BSD syscall 0x%llx returned error: errno=%llu\n", (unsigned long long)sys_num, (unsigned long long)ret);
        Frame->State.flags[FEXCore::X86State::RFLAG_CF_RAW_LOC] = 1;
    } else {
        Frame->State.flags[FEXCore::X86State::RFLAG_CF_RAW_LOC] = 0;
    }
    
    // Advance Guest RIP past the 2-byte syscall instruction (0F 05)
    // FEXCore sets Frame->State.rip to the start of the syscall instruction.
    // If we don't advance it, the JIT will loop or crash on return.
    Frame->State.rip += 2;
    
    fprintf(stderr, "[MacOSSyscalls] RETURNING: ret=0x%llx for syscall 0x%llx, advanced RIP to 0x%llx\n", 
            (unsigned long long)ret, (unsigned long long)sys_num, (unsigned long long)Frame->State.rip);
    
    fflush(stderr);
    
    return ret;
#else
    std::cerr << "[MacOSEmulation] Syscall passthrough is only supported on aarch64 Apple Silicon." << std::endl;
    return 0;
#endif
}

} // namespace MacOSEmulation
#include <unistd.h>
