/* Minimal linux/fcntl.h for macOS porting of FEX */
#ifndef _LINUX_FCNTL_H
#define _LINUX_FCNTL_H 1

#include <fcntl.h>

#define F_LINUX_SPECIFIC_BASE 1024

#define F_ADD_SEALS (F_LINUX_SPECIFIC_BASE + 9)
#define F_GET_SEALS (F_LINUX_SPECIFIC_BASE + 10)

#define F_SEAL_SEAL         0x0001
#define F_SEAL_SHRINK       0x0002
#define F_SEAL_GROW         0x0004
#define F_SEAL_WRITE        0x0008
#define F_SEAL_FUTURE_WRITE 0x0010

#ifndef O_TMPFILE
#define O_TMPFILE 0
#endif

#ifndef O_PATH
#define O_PATH 0
#endif

#ifndef AT_EMPTY_PATH
#define AT_EMPTY_PATH 0x1000
#endif

#endif /* linux/fcntl.h */
