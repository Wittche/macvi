/* Minimal asm/msgbuf.h for macOS porting of FEX */
#ifndef _ASM_MSGBUF_H
#define _ASM_MSGBUF_H 1

#include <asm/ipcbuf.h>
#include <stdint.h>

struct msqid64_ds {
  struct ipc64_perm msg_perm;
  uint64_t msg_stime;
  uint64_t msg_rtime;
  uint64_t msg_ctime;
  uint64_t msg_cbytes;
  uint64_t msg_qnum;
  uint64_t msg_qbytes;
  uint64_t msg_lspid;
  uint64_t msg_lrpid;
  uint64_t __unused4;
  uint64_t __unused5;
};

#endif /* _ASM_MSGBUF_H */
