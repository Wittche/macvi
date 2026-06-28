/* Minimal asm/ipcbuf.h for macOS porting of FEX */
#ifndef _ASM_IPCBUF_H
#define _ASM_IPCBUF_H 1

#include <stdint.h>

struct ipc64_perm {
  uint32_t key;
  uint32_t uid;
  uint32_t gid;
  uint32_t cuid;
  uint32_t cgid;
  uint16_t mode;
  uint16_t pad1;
  uint16_t seq;
  uint16_t pad2;
  uint64_t unused1;
  uint64_t unused2;
};

#endif /* _ASM_IPCBUF_H */
