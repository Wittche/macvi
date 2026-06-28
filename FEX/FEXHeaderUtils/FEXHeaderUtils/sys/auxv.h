/* Minimal sys/auxv.h for macOS porting of FEX */
#ifndef _SYS_AUXV_H
#define _SYS_AUXV_H 1

#include <unistd.h>

#ifndef AT_SYSINFO_EHDR
#define AT_SYSINFO_EHDR 33
#endif

#ifndef AT_UID
#define AT_EXECFD 2
#define AT_PAGESZ 6
#define AT_UID    11
#define AT_EUID   12
#define AT_GID    13
#define AT_EGID   14
#define AT_HWCAP  16
#define AT_CLKTCK 17
#define AT_SECURE 23
#define AT_HWCAP2 26
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline unsigned long getauxval(unsigned long type) {
  switch (type) {
    case AT_PAGESZ: return sysconf(_SC_PAGESIZE);
    case AT_UID: return getuid();
    case AT_EUID: return geteuid();
    case AT_GID: return getgid();
    case AT_EGID: return getegid();
    case AT_HWCAP: return 0;
    case AT_CLKTCK: return sysconf(_SC_CLK_TCK);
    case AT_SECURE: return issetugid();
    case AT_HWCAP2: return 0;
  }
  return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_AUXV_H */
