/* Minimal sys/fsuid.h for macOS porting of FEX */
#ifndef _SYS_FSUID_H
#define _SYS_FSUID_H 1

#include <sys/types.h>

#ifdef __APPLE__
static inline int setfsuid(uid_t uid) { return 0; }
static inline int setfsgid(gid_t gid) { return 0; }
#endif

#endif /* _SYS_FSUID_H */
