# MacWI Development Walkthrough

## Phase 22: Native DLL Loading, VFS, and Base Relocations
**Goal:** Enable loading of Native Windows DLLs into MacWI's emulator memory, mapping their sections, resolving imports, and executing them seamlessly using the FEXCore 32-bit x86 emulator.

### 1. FEXCore Architecture Shift for 32-bit PEs
We encountered significant memory and execution challenges while attempting to execute 32-bit x86 Windows binaries using a 64-bit emulator context.
- We switched `FEXCore` back to native 32-bit mode (`CONFIG_IS64BIT_MODE` = "0") to correctly manage 32-bit pointers and registers.
- We forced the `OSABI` to `OS_LINUX32` inside `MacWISyscallHandler`. This instructs FEXCore to interpret `int 0x80` traps correctly, which prevents accidental clobbering of 64-bit registers like `R11`.
- We updated our `MacWI` Thunking layer to generate `int 0x80` trampolines.
- We modified `macwi_thunk_read_param_32` to read Windows `__stdcall` and `__cdecl` arguments strictly from the guest stack (`ESP + 4`, `ESP + 8`), instead of reading from `x64` registers.

### 2. Resolving the `SIGBUS` at `10011066`
During execution of `vulkan_test.exe`, the application threw a `SIGBUS` (`Signal 10`) at `0x10011066`.
- **Root Cause Analysis:** We traced the stack operations step-by-step and found that `gcc` compiled a local string array `char hex_chars[] = "0123...CDEF"` into `vulkan_test.c`. The array was placed at an unaligned stack address (`EBP - 0x35`). Apple Silicon generated a memory fault (SIGBUS) when FEXCore JIT attempted a 32-bit `movl` to this unaligned address.
- **Fix:** We updated `vulkan_test.c` to use a string literal (`const char*`), allowing GCC to place it in `.rdata`, thereby avoiding unaligned stack writes.
- We also resolved ABI calling convention issues. MinGW `gcc` treats undefined functions as `__cdecl`, but our trampolines generated `__stdcall` returns (`ret 4`). We included `<windows.h>` to ensure all API declarations use `__stdcall`.

### Phase 23: Virtual File System (VFS)
- **Path Resolution:** Implemented `macwi_vfs_dos_to_unix` which takes Windows paths (e.g. `C:\Windows\System32`) and maps them case-insensitively to the host filesystem (e.g. `~/.macwi/drive_c/windows/system32/`).
- **File APIs:** Implemented `CreateFileA`, `ReadFile`, `WriteFile`, `GetFileSize`, `SetFilePointer`, and `CloseHandle` in `kernel32.c`.
- **Validation:** Created `tests/vfs_test.c` which successfully wrote to and read from a mapped Windows path.

## Phase 24: Memory Management
- **Memory Allocation:** Re-implemented `VirtualAlloc` and `VirtualFree` to use 32-bit `0x60000000` memory regions to map perfectly into the FEXCore guest memory space.
- **Memory Protection:** Implemented `VirtualProtect` and `VirtualQuery` stubs to prevent crashes in apps querying page sizes and protections.
- **Validation:** Created `tests/mem_test.c` which successfully allocates, writes, queries, and frees memory.

> [!TIP]
> Tests for Virtual File System (`vfs_test.exe`) and Memory Management (`mem_test.exe`) now run flawlessly through our 32-bit translation layer and emulator on Apple Silicon.

### 3. Execution Success
The emulator successfully executed the `vulkan_test.exe` binary. 
- It loaded `vulkan-1.dll`.
- It mapped and executed `kernel32.dll` system calls (`GetStdHandle`, `WriteFile`, `LoadLibraryA`, `GetProcAddress`, `ExitProcess`).
- It completed the execution successfully and gracefully exited with `ExitProcess(0)`.

```
[macwi] Loading PE file: ../tests/vulkan_test.exe
...
>>> Starting execution at RIP = 0x00000000004010EE <<<
...
Loading vulkan-1.dll...
Successfully loaded vulkan-1.dll at 0x50000000
Found vkGetInstanceProcAddr at 0x100000E0
vkCreateInstance thunk at 0x0
Test completed successfully.
[macwi:kernel32] ExitProcess(0)
```

## Next Steps
We are now fully prepared to load actual Windows games (e.g. D3D9 / DXVK) since our PE loader, thunking engine, memory manager, and emulator can faithfully load and execute complex 32-bit Windows executables!
