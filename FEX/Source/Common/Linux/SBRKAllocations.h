// SPDX-License-Identifier: MIT
#pragma once

namespace FEX::SBRKAllocations {
// Disable allocations through glibc's sbrk allocation method.
// Returns a pointer at the end of the sbrk region.
#ifdef __APPLE__
inline void* DisableSBRKAllocations() { return nullptr; }
inline void ReenableSBRKAllocations(void* Ptr) {}
#else
void* DisableSBRKAllocations();

// Allow sbrk again. Pass in the pointer returned by `DisableSBRKAllocations`
void ReenableSBRKAllocations(void* Ptr);
#endif
} // namespace FEX::SBRKAllocations
