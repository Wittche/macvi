/* Minimal linux/types.h for macOS porting of FEX */
#ifndef _LINUX_TYPES_H
#define _LINUX_TYPES_H 1

#include <stdint.h>
#include <sys/types.h>

typedef uint32_t __u32;
typedef uint64_t __u64;
typedef uint16_t __u16;
typedef uint8_t  __u8;
typedef int32_t  __s32;
typedef int64_t  __s64;

typedef struct {
  int val[2];
} __kernel_fsid_t;

#endif /* linux/types.h */
