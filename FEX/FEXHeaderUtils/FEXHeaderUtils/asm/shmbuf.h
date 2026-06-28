/* Minimal asm/shmbuf.h for macOS porting of FEX */
#ifndef _ASM_SHMBUF_H
#define _ASM_SHMBUF_H 1

#include <asm/ipcbuf.h>
#include <stdint.h>

struct shmid64_ds {
  struct ipc64_perm shm_perm;
  uint64_t shm_segsz;
  uint64_t shm_atime;
  uint64_t shm_dtime;
  uint64_t shm_ctime;
  uint32_t shm_cpid;
  uint32_t shm_lpid;
  uint64_t shm_nattch;
  uint64_t __unused4;
  uint64_t __unused5;
};

#endif /* _ASM_SHMBUF_H */
