/* Minimal linux/shm.h for macOS porting of FEX */
#ifndef _LINUX_SHM_H
#define _LINUX_SHM_H 1

#include <linux/ipc.h>
#include <sys/shm.h>

#ifndef SHM_STAT
#define SHM_STAT 13
#endif

#ifndef SHM_INFO
#define SHM_INFO 14
#endif

#ifndef SHM_STAT_ANY
#define SHM_STAT_ANY 15
#endif

#ifndef SHM_LOCK
#define SHM_LOCK 11
#endif

#ifndef SHM_UNLOCK
#define SHM_UNLOCK 12
#endif

#ifndef SHM_EXEC
#define SHM_EXEC 0100000
#endif

#endif /* _LINUX_SHM_H */
