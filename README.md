# MacWI — WoW64-like Compatibility Layer for Apple Silicon macOS

MacWI (Macintosh Windows Interoperability) is a WoW64-like compatibility layer
that enables running 32-bit Windows (Win32/PE32) applications on Apple Silicon
(ARM64) macOS. It achieves this by combining PE binary loading, a Win32 API
thunking layer, and an x86-to-ARM64 translation bridge.

> **Status:** Early development — PE parsing and structural foundations only.
> No actual x86 code execution yet.

## Architecture Overview

```
┌─────────────────────────────────────────────┐
│            32-bit Win32 Application         │
│                  (PE32 / x86)               │
├─────────────────────────────────────────────┤
│              Thunking Layer                 │
│   (32→64 bit marshaling, pointer mapping)   │
├──────────────┬──────────────────────────────┤
│  Win32 API   │      NT API Stubs           │
│   Stubs      │   (ntdll emulation)         │
│ (kernel32)   │                              │
├──────────────┴──────────────────────────────┤
│           Handle Table / Core               │
│    (object management, thread safety)       │
├─────────────────────────────────────────────┤
│         PE Loader & Parser                  │
│  (DOS/PE header, sections, imports)         │
├─────────────────────────────────────────────┤
│      macOS / POSIX / Apple Silicon          │
│   (mmap, pthreads, Mach-O, ARM64 host)     │
└─────────────────────────────────────────────┘
```

## Building

### Prerequisites

- macOS 12+ on Apple Silicon (ARM64)
- Xcode Command Line Tools or full Xcode
- CMake 3.20+

### Build Steps

```bash
# Clone or navigate to the project directory
cd macwi

# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Run tests
cd build && ctest --output-on-failure
```

### Running

```bash
./build/macwi path/to/your_app.exe
```

Currently, MacWI will parse and display PE header information but will **not**
execute the application.

## Project Structure

```
macwi/
├── CMakeLists.txt              # Root build configuration
├── README.md                   # This file
├── include/
│   └── macwi/
│       ├── types.h             # Core Win32-compatible type definitions
│       ├── pe.h                # PE format structures and loader API
│       ├── thunk.h             # Thunking layer (32↔64 bit marshaling)
│       └── handle.h            # Handle table management
├── src/
│   ├── main.c                  # Entry point — CLI and PE info display
│   ├── CMakeLists.txt          # Main src build config
│   ├── core/
│   │   ├── CMakeLists.txt
│   │   └── handle.c           # Thread-safe handle table implementation
│   ├── loader/
│   │   ├── CMakeLists.txt
│   │   ├── pe_parser.c        # PE header parsing and validation
│   │   └── pe_loader.c        # File I/O, mmap-based section mapping
│   ├── thunk/
│   │   ├── CMakeLists.txt
│   │   └── thunk.c            # Pointer conversion, param marshaling
│   └── win32/
│       ├── CMakeLists.txt
│       ├── kernel32.h          # kernel32.dll API declarations
│       ├── kernel32.c          # kernel32 stub implementations
│       ├── ntdll.h             # ntdll.dll API declarations
│       └── ntdll.c             # ntdll stub implementations
└── tests/
    ├── CMakeLists.txt
    └── test_pe_parser.c        # PE parser unit tests
```

## Design Principles

1. **Pure C11 + POSIX** — No C++ or Objective-C in the core; easy to audit.
2. **Modular libraries** — Each subsystem (loader, thunk, win32) is a separate
   static library with a clean public header.
3. **Thread-safe by default** — Handle tables and shared state are protected
   with `pthread_mutex`.
4. **Explicit error handling** — All fallible functions return `macwi_status_t`;
   no hidden exceptions.
5. **Convention: `macwi_` prefix** — Public API uses `macwi_` prefix;
   file-internal helpers use `internal_` prefix or `static`.

## Roadmap

- [x] PE32 header parsing and validation
- [x] Section mapping via `mmap`
- [x] Handle table with generation-based stale detection
- [x] kernel32 / ntdll API stubs (logging only)
- [x] Import Address Table (IAT) resolution
- [x] x86 instruction emulation (via FEXCore JIT Engine)
- [x] Full kernel32 implementation (file I/O, memory, sync, threading, VFS)
- [x] GDI32 / USER32 stubs for basic GUI support (Windows, Message Loops, Painting)
- [x] Advanced GUI controls (Button/Static/Edit), Timers, and GDI Fonts
- [ ] GDI Bitmaps and advanced graphics
## License

MIT License

Copyright (c) 2026 MacWI Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
