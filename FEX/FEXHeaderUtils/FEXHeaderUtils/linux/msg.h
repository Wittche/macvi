/* Minimal linux/msg.h for macOS porting of FEX */
#ifndef _LINUX_MSG_H
#define _LINUX_MSG_H 1

#include <linux/ipc.h>
#include <sys/msg.h>

#ifndef MSG_STAT
#define MSG_STAT 11
#endif

#ifndef MSG_INFO
#define MSG_INFO 12
#endif

#ifndef MSG_STAT_ANY
#define MSG_STAT_ANY 13
#endif

struct msgbuf {
	long mtype;     /* type of message */
	char mtext[1];  /* message text */
};

#endif /* _LINUX_MSG_H */
