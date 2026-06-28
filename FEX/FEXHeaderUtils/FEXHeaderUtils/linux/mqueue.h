/* Minimal linux/mqueue.h for macOS porting of FEX */
#ifndef _LINUX_MQUEUE_H
#define _LINUX_MQUEUE_H 1

struct mq_attr {
  long mq_flags;
  long mq_maxmsg;
  long mq_msgsize;
  long mq_curmsgs;
};

#endif /* _LINUX_MQUEUE_H */
