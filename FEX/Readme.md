# FEX-Emu macOS Native Port (FEXMacOS)

FEXMacOS is a high-performance translation layer designed to run x86_64 macOS and Windows applications natively on Apple Silicon (ARM64). Unlike traditional emulation, FEXMacOS bridges guest applications directly to the host kernel and native libraries, achieving Rosetta 2-class performance.

## Key Features

- **Hybrid Binary Support:** Natively load and execute both Mach-O (macOS) and PE (Windows) x86_64 binaries.
- **Windows API Bridge:** Integrated `PELoader` with automatic IAT thunking to redirect Windows DLL calls to native macOS host functions.
- **Mükemmel dyld Support:** Full emulation of the macOS Dynamic Linker, including Shared Cache mapping, Chained Fixups, and Rosetta environment simulation.
- **Hardware TSO:** Leverages Apple Silicon's hardware Total Store Ordering for maximum x86_64 memory consistency without performance loss.
- **Zero-ABI Overheads:** Transparent syscall and trap translation with 16KB page alignment shims.

## Getting Started

### Prerequisites
- Apple Silicon Mac (M1/M2/M3)
- macOS 12.0 or newer
- CMake and Clang (from Xcode)

### Building
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8 FEXMacOS
```

### Usage
Run any x86_64 binary directly:
```bash
./Bin/FEXMacOS /usr/bin/true       # Runs a native macOS x86 binary
./Bin/FEXMacOS ./win_app.exe      # Runs a Windows PE binary via the API Bridge
```

## Current Status
We are currently in **Phase 5: Windows API Bridge**. The core architecture is "Perfect," meaning all hardware and OS-level barriers (Memory, TSO, PAC, Shared Cache) have been resolved. The focus is now on implementing specific Windows API thunks for broader application compatibility.

---
*Developed as part of the FEX-Emu Perfect macOS Port Project (June 2026).*
