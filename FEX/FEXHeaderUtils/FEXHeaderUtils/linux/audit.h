/* Minimal linux/audit.h for macOS porting of FEX */
#ifndef _LINUX_AUDIT_H
#define _LINUX_AUDIT_H 1

#define AUDIT_ARCH_X86_64	0xc000003e
#define AUDIT_ARCH_I386		0x40000003
#define AUDIT_ARCH_AARCH64	0xc00000b7

#define AUDIT_SECCOMP 1326

#endif /* linux/audit.h */
