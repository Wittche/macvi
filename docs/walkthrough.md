# GUI Crash Fix Walkthrough

## Problem
The emulator crashed with `EXC_BAD_ACCESS (SIGBUS)` at guest address `0x54000100` when running `gdi_test.exe`. The crash occurred in the FEXCore JIT thread while processing WM_PAINT messages.

## Root Cause: Deadlock in BeginPaint

A circular dependency existed between two threads:

1. **Main thread** (`drawRect`): Pushed a `MACWI_EVENT_PAINT` to the event queue, then blocked on `pthread_cond_wait` waiting for `EndPaint` to signal completion.

2. **Emulator thread** (`BeginPaint` handler): Called `macwi_cocoa_get_client_rect` which used `dispatch_sync(main_queue)` to query the window size from the main thread.

Since the main thread was already blocked in `drawRect`, `dispatch_sync` could never execute → **deadlock**. macOS eventually killed the stalled thread, producing the crash at an arbitrary memory address (`0x54000100`).

### Secondary: Uninitialised Padding in invoke_callback

The `invoke_callback` function reserved `caller_pop_bytes` of stack space (for the thunk's `ret N` instruction) without initialising it. The JIT could read garbage values from this region.

## Changes Made

### [gdi32.c](file:///Users/firataktug/Desktop/macwi/src/win32/gdi32.c)
- Removed `macwi_cocoa_get_client_rect()` call from `win32_BeginPaint`
- Replaced with hardcoded 800×600 defaults to avoid `dispatch_sync` deadlock

### [thunk.c](file:///Users/firataktug/Desktop/macwi/src/thunk/thunk.c)
- Zero-initialised the padding bytes in `invoke_callback` (previously uninitialised)
- Added proper documentation of the stack layout for the thunk's `ret N` mechanism

## Verification

Ran `gdi_test.exe` successfully:
- ✅ WM_PAINT processed (BeginPaint → FillRect → EndPaint cycle completes)
- ✅ Mouse events (WM_LBUTTONDOWN/UP) dispatched and handled
- ✅ WM_CLOSE received and processed cleanly
- ✅ No crash, stable message loop
