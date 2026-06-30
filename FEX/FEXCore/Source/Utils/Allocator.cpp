// SPDX-License-Identifier: MIT
#include "Utils/Allocator/HostAllocator.h"
#include "Utils/Allocator.h"
#include <FEXCore/Utils/Allocator.h>
#include <FEXCore/Utils/AllocatorHooks.h>
#include <FEXCore/Utils/CompilerDefs.h>
#include <FEXCore/Utils/LogManager.h>
#include <FEXCore/Utils/MathUtils.h>
#include <FEXCore/Utils/TypeDefines.h>
#include <FEXCore/fextl/vector.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <charconv>

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif

namespace FEXCore::Utils {
#ifdef __APPLE__
uint64_t GlobalMemoryBase = 0;
#endif
}

namespace FEXCore::Allocator {

#ifndef _WIN32
static fextl::unique_ptr<Alloc::HostAllocator> Alloc64;

static size_t HostVASize = 0;

MMAP_Hook mmap = ::mmap;
MUNMAP_Hook munmap = ::munmap;

static void* FEX_mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
  if (Alloc64) {
    return Alloc64->Mmap(addr, length, prot, flags, fd, offset);
  }
  return ::mmap(addr, length, prot, flags, fd, offset);
}

static int FEX_munmap(void* addr, size_t length) {
  if (Alloc64) {
    return Alloc64->Munmap(addr, length);
  }
  return ::munmap(addr, length);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static void AssignHookOverrides(size_t PageSize) {
  SetupAllocatorHooks(FEX_mmap, FEX_munmap);
  FEXCore::Allocator::mmap = FEX_mmap;
  FEXCore::Allocator::munmap = FEX_munmap;
  InitializeAllocator(PageSize);
}

void SetupHooks(size_t PageSize) {
  Alloc64 = Alloc::OSAllocator::Create64BitAllocator();
  AssignHookOverrides(PageSize);
}

void ClearHooks() {
  SetupAllocatorHooks(::mmap, ::munmap);
  FEXCore::Allocator::mmap = ::mmap;
  FEXCore::Allocator::munmap = ::munmap;

  Alloc::OSAllocator::ReleaseAllocatorWorkaround(std::move(Alloc64));
}
#pragma GCC diagnostic pop

void VirtualName(const char* Name, void* Ptr, size_t Size) {
  // nop
}

FEX_DEFAULT_VISIBILITY size_t DetermineVASize() {
  if (HostVASize) {
    return HostVASize;
  }

#ifdef __APPLE__
#if defined(__aarch64__)
  HostVASize = 48;
#else
  HostVASize = 47;
#endif
  return HostVASize;
#endif

  static constexpr std::array<uintptr_t, 7> TLBSizes = {
    57, 52, 48, 47, 42, 39, 36,
  };

  for (auto Bits : TLBSizes) {
    uintptr_t Size = 1ULL << (Bits - 1);
    void* Ptr = ::mmap(reinterpret_cast<void*>(Size), FEXCore::Utils::FEX_PAGE_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (Ptr != MAP_FAILED) {
      ::munmap(Ptr, FEXCore::Utils::FEX_PAGE_SIZE);
      HostVASize = Bits;
      return HostVASize;
    }
  }

  HostVASize = 36;
  return HostVASize;
}

fextl::vector<MemoryRegion> CollectMemoryGaps(uintptr_t Begin, uintptr_t End, int MapsFD) {
  fextl::vector<MemoryRegion> Regions;
  uintptr_t RegionEnd = 0;
  char Buffer[2048];
  const char* Cursor = Buffer;
  ssize_t Remaining = 0;
  bool EndOfFileReached = false;

  while (true) {
    const auto line_begin = Cursor;
    auto line_end = std::find(line_begin, Cursor + Remaining, '\n');
    if (line_end == Cursor + Remaining) {
      if (EndOfFileReached) {
        const auto MapBegin = std::max(RegionEnd, Begin);
        if (End > MapBegin) {
          Regions.push_back({(void*)MapBegin, End - MapBegin});
        }
        return Regions;
      }
      std::copy(Cursor, Cursor + Remaining, std::begin(Buffer));
      auto PendingBytes = Remaining;
      do {
        Remaining = read(MapsFD, Buffer + PendingBytes, sizeof(Buffer) - PendingBytes);
      } while (Remaining == -1 && errno == EAGAIN);
      if (Remaining < (ssize_t)(sizeof(Buffer) - PendingBytes)) EndOfFileReached = true;
      Remaining += PendingBytes;
      Cursor = Buffer;
      continue;
    }
    {
      uintptr_t RegionBegin {};
      auto result = std::from_chars(Cursor, line_end, RegionBegin, 16);
      Cursor = result.ptr + 1;
      result = std::from_chars(Cursor, line_end, RegionEnd, 16);
      Cursor = line_end + 1;
      Remaining -= (Cursor - line_begin);
      if (RegionEnd <= Begin) continue;
      const auto MapBegin = std::max(RegionBegin, Begin);
      if (RegionBegin > MapBegin) {
        Regions.push_back({(void*)MapBegin, RegionBegin - MapBegin});
      }
      if (RegionEnd >= End) return Regions;
    }
  }
}

fextl::vector<MemoryRegion> StealMemoryRegion(uintptr_t Begin, uintptr_t End) {
#ifndef __APPLE__
  const uintptr_t StackLocation_u64 = reinterpret_cast<uintptr_t>(alloca(0));
#endif
#ifdef __APPLE__
  fextl::vector<MemoryRegion> Regions;
  mach_vm_address_t address = Begin;
  mach_vm_size_t size = 0;
  uint32_t depth = 1;
  vm_region_submap_info_64 info;
  mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
  uintptr_t last_end = Begin;
  while (address < End) {
    if (mach_vm_region_recurse(mach_task_self(), &address, &size, &depth, (vm_region_recurse_info_t)&info, &count) != KERN_SUCCESS) break;
    if (address > last_end) {
      uintptr_t gap_begin = last_end;
      uintptr_t gap_end = std::min((uintptr_t)address, End);
      if (gap_end > gap_begin) Regions.push_back({(void*)gap_begin, gap_end - gap_begin});
    }
    last_end = address + size;
    address = last_end;
  }
  if (last_end < End) Regions.push_back({(void*)last_end, End - last_end});
  
  for (auto RegionIt = Regions.begin(); RegionIt != Regions.end(); ++RegionIt) {
    ::mmap(RegionIt->Ptr, RegionIt->Size, PROT_NONE, MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE | MAP_FIXED, -1, 0);
  }
#else
  const int MapsFD = open("/proc/self/maps", O_RDONLY);
  if (MapsFD == -1) return {};
  auto Regions = CollectMemoryGaps(Begin, End, MapsFD);
  close(MapsFD);
  {
    auto StackRegionIt = std::find_if(Regions.begin(), Regions.end(), [StackLocation_u64](auto& Region) {
      return reinterpret_cast<uintptr_t>(Region.Ptr) + Region.Size > StackLocation_u64;
    });
    bool IsStackMapping = StackRegionIt != Regions.end() || StackLocation_u64 <= End;
    if (IsStackMapping && StackRegionIt != Regions.begin() &&
        reinterpret_cast<uintptr_t>(std::prev(StackRegionIt)->Ptr) + std::prev(StackRegionIt)->Size <= End) {
      --StackRegionIt;
      auto Alloc = ::mmap(StackRegionIt->Ptr, StackRegionIt->Size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE | MAP_FIXED, -1, 0);
      if (Alloc != MAP_FAILED) Regions.erase(StackRegionIt);
    }
  }
  for (auto RegionIt = Regions.begin(); RegionIt != Regions.end(); ++RegionIt) {
    ::mmap(RegionIt->Ptr, RegionIt->Size, PROT_NONE, MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE | MAP_FIXED_NOREPLACE, -1, 0);
  }
#endif
  return Regions;
}

fextl::vector<MemoryRegion> Setup48BitAllocatorIfExists(size_t PageSize) {
#ifdef __APPLE__
  // We MUST use the custom allocator on macOS for 32-bit guests, because macOS ARM64
  // completely blocks mmap under 4GB (PAGEZERO is 4GB).
#endif
  size_t Bits = DetermineVASize();
  if (Bits < 47) {
      fprintf(stderr, "FEXDBG: DetermineVASize returned %zu, which is < 47! Failing!\n", Bits);
      return {};
  }
  uintptr_t Begin48BitVA = 0x0'0001'1000'0000ULL; // 4GB + 256MB (Avoids macwi executable ASLR overlap)
  uintptr_t End48BitVA = 0x0'4000'0000'0000ULL; // 64TB (fits in 47-bit)
  auto Regions = StealMemoryRegion(Begin48BitVA, End48BitVA);
  if (Regions.empty()) {
    fprintf(stderr, "FEXDBG: No 48-bit regions found to steal, skipping 64-bit allocator setup\n");
    return {};
  }
  fprintf(stderr, "FEXDBG: Setup48BitAllocatorIfExists SUCCESS! Got %zu regions.\n", Regions.size());
  
  // Find the lowest region base - this becomes our GlobalMemoryBase.
  // Guest address 0 maps to host address GlobalMemoryBase.
  // Guest address X maps to host address X + GlobalMemoryBase.
  uintptr_t lowest_base = UINTPTR_MAX;
  for (const auto& r : Regions) {
    uintptr_t base = reinterpret_cast<uintptr_t>(r.Ptr);
    if (base < lowest_base) {
      lowest_base = base;
    }
  }
  FEXCore::Utils::GlobalMemoryBase = lowest_base;
  fprintf(stderr, "FEXDBG: GlobalMemoryBase set to 0x%llx\n", (unsigned long long)FEXCore::Utils::GlobalMemoryBase);
  
  Alloc64 = Alloc::OSAllocator::Create64BitAllocatorWithRegions(Regions);
  AssignHookOverrides(PageSize);
  return Regions;
}

void ReclaimMemoryRegion(const fextl::vector<MemoryRegion>& Regions) {
  for (const auto& Region : Regions) ::munmap(Region.Ptr, Region.Size);
}

void LockBeforeFork(FEXCore::Core::InternalThreadState* Thread) {
  if (Alloc64) Alloc64->LockBeforeFork(Thread);
}

void UnlockAfterFork(FEXCore::Core::InternalThreadState* Thread, bool Child) {
  if (Alloc64) Alloc64->UnlockAfterFork(Thread, Child);
}
#else
void VirtualNameNOP(const char*, const void*, size_t) {}
void VirtualTHPNOP(const void* Ptr, size_t Size, THPControl Control) {}
using VirtualNamePtr = void (*)(const char*, const void*, size_t);
using VirtualTHPPtr = void (*)(const void*, size_t, THPControl);
VirtualNamePtr VirtualName {VirtualNameNOP};
VirtualTHPPtr VirtualTHPControl {VirtualTHPNOP};
struct HookPtrs {
  VirtualNamePtr VirtualName;
  VirtualTHPPtr VirtualTHPControl;
};
void SetupHooks(size_t PageSize, HookPtrs Ptrs) {
  VirtualName = Ptrs.VirtualName;
  VirtualTHPControl = Ptrs.VirtualTHPControl;
}
#endif
} // namespace FEXCore::Allocator
