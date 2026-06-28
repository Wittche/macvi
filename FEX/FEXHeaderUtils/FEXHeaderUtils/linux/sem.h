/* Minimal linux/sem.h for macOS porting of FEX */
#ifndef _LINUX_SEM_H
#define _LINUX_SEM_H 1

#include <linux/ipc.h>
#include <sys/sem.h>

#ifndef SEM_STAT
#define SEM_STAT 18
#endif

#ifndef SEM_INFO
#define SEM_INFO 19
#endif

#ifndef SEM_STAT_ANY
#define SEM_STAT_ANY 20
#endif

#endif /* linux/sem.h */
