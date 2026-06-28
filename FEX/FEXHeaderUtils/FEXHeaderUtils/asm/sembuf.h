/* Minimal asm/sembuf.h for macOS porting of FEX */
#ifndef _ASM_SEMBUF_H
#define _ASM_SEMBUF_H 1

#include <asm/ipcbuf.h>
#include <stdint.h>

struct semid64_ds {
  struct ipc64_perm sem_perm;
  uint64_t sem_otime;
  uint64_t __unused1;
  uint64_t sem_ctime;
  uint64_t __unused2;
  uint64_t sem_nsems;
  uint64_t __unused3;
  uint64_t __unused4;
};

#endif /* _ASM_SEMBUF_H */
