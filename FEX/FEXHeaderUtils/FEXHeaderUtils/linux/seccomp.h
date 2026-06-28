/* Minimal linux/seccomp.h for macOS porting of FEX */
#ifndef _LINUX_SECCOMP_H
#define _LINUX_SECCOMP_H 1

#include <stdint.h>

struct seccomp_data {
  int nr;
  uint32_t arch;
  uint64_t instruction_pointer;
  uint64_t args[6];
};

struct seccomp_notif_sizes {
  uint16_t seccomp_notif;
  uint16_t seccomp_notif_resp;
  uint16_t seccomp_data;
};

struct seccomp_notif {
  uint64_t id;
  uint32_t pid;
  uint32_t flags;
  struct seccomp_data data;
};

struct seccomp_notif_resp {
  uint64_t id;
  int64_t val;
  int32_t error;
  uint32_t flags;
};

#define SECCOMP_RET_KILL_PROCESS 0x80000000U
#define SECCOMP_RET_KILL_THREAD  0x00000000U
#define SECCOMP_RET_TRAP         0x00030000U
#define SECCOMP_RET_ERRNO        0x00050000U
#define SECCOMP_RET_USER_NOTIF   0x7fc00000U
#define SECCOMP_RET_TRACE        0x7ff00000U
#define SECCOMP_RET_LOG          0x7ffb0000U
#define SECCOMP_RET_ALLOW        0x7fff0000U

#define SECCOMP_RET_ACTION_FULL  0xffff0000U
#define SECCOMP_RET_ACTION       0x7fff0000U
#define SECCOMP_RET_DATA         0x0000ffffU

#define SECCOMP_MODE_DISABLED 0
#define SECCOMP_MODE_STRICT   1
#define SECCOMP_MODE_FILTER   2

#define SECCOMP_SET_MODE_STRICT   0
#define SECCOMP_SET_MODE_FILTER   1
#define SECCOMP_GET_ACTION_AVAIL  2
#define SECCOMP_GET_NOTIF_SIZES   3

#define SECCOMP_FILTER_FLAG_TSYNC        (1UL << 0)
#define SECCOMP_FILTER_FLAG_LOG          (1UL << 1)
#define SECCOMP_FILTER_FLAG_SPEC_ALLOW   (1UL << 2)
#define SECCOMP_FILTER_FLAG_NEW_LISTENER (1UL << 3)
#define SECCOMP_FILTER_FLAG_TSYNC_ESRCH  (1UL << 4)

#endif /* linux/seccomp.h */
