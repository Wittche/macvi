/* Minimal linux/personality.h for macOS porting of FEX */
#ifndef _LINUX_PERSONALITY_H
#define _LINUX_PERSONALITY_H 1

#include <stdint.h>
#include <errno.h>

#define PER_LINUX	0x0000
#define PER_LINUX32	0x0008
#define UNAME26		0x0020000
#define ADDR_NO_RANDOMIZE 0x0040000
#define READ_IMPLIES_EXEC 0x0400000

#ifdef __APPLE__
static inline int personality(uint32_t persona) {
  return 0;
}
#endif

#endif /* _LINUX_PERSONALITY_H */
