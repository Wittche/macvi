/* Minimal linux/mman.h for macOS porting of FEX */
#ifndef _LINUX_MMAN_H
#define _LINUX_MMAN_H 1

#include <sys/mman.h>

#ifndef MAP_GROWSDOWN
#define MAP_GROWSDOWN	0x00100
#endif
#ifndef MAP_DENYWRITE
#define MAP_DENYWRITE	0x00800
#endif
#ifndef MAP_EXECUTABLE
#define MAP_EXECUTABLE	0x01000
#endif
#ifndef MAP_LOCKED
#define MAP_LOCKED	0x02000
#endif
#ifndef MAP_NORESERVE
#define MAP_NORESERVE	0x04000
#endif
#ifndef MAP_POPULATE
#define MAP_POPULATE	0x08000
#endif
#ifndef MAP_NONBLOCK
#define MAP_NONBLOCK	0x10000
#endif
#ifndef MAP_STACK
#define MAP_STACK	0x20000
#endif
#ifndef MAP_HUGETLB
#define MAP_LITERAL	0x40000
#endif
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

#define MREMAP_MAYMOVE	1
#define MREMAP_FIXED	2
#define MREMAP_DONTUNMAP 4

#ifdef __APPLE__
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#ifndef MAP_ANON
#define MAP_ANON 0x1000
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline void* mremap(void* old_address, size_t old_size, size_t new_size, int flags, ...) {
  void* new_address = nullptr;
  if (flags & MREMAP_FIXED) {
    va_list args;
    va_start(args, flags);
    new_address = va_arg(args, void*);
    va_end(args);
  }

  if (new_size == old_size) {
    return old_address;
  }

  if (new_size < old_size) {
    if (munmap((char*)old_address + new_size, old_size - new_size) == -1) {
      return (void*)-1;
    }
    return old_address;
  }

  // new_size > old_size
  // Try to expand in place
  void* ptr = mmap((char*)old_address + old_size, new_size - old_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
  if (ptr != (void*)-1) {
    return old_address;
  }

  if (!(flags & MREMAP_MAYMOVE)) {
    errno = ENOMEM;
    return (void*)-1;
  }

  // Must move
  void* new_ptr = mmap(new_address, new_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | (new_address ? MAP_FIXED : 0), -1, 0);
  if (new_ptr == (void*)-1) {
    return (void*)-1;
  }

  memcpy(new_ptr, old_address, old_size);
  if (!(flags & MREMAP_DONTUNMAP)) {
    munmap(old_address, old_size);
  }
  return new_ptr;
}

#ifdef __cplusplus
}
#endif
#endif

#endif /* linux/mman.h */
