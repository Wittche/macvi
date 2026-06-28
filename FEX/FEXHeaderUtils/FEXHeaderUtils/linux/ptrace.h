/* Minimal linux/ptrace.h for macOS porting of FEX */
#ifndef _LINUX_PTRACE_H
#define _LINUX_PTRACE_H 1

#include <sys/ptrace.h>

#ifndef PTRACE_PEEKTEXT
#define PTRACE_PEEKTEXT 1
#endif
#ifndef PTRACE_PEEKDATA
#define PTRACE_PEEKDATA 2
#endif
#ifndef PTRACE_POKETEXT
#define PTRACE_POKETEXT 4
#endif
#ifndef PTRACE_POKEDATA
#define PTRACE_POKEDATA 5
#endif
#ifndef PTRACE_CONT
#define PTRACE_CONT 7
#endif
#ifndef PTRACE_KILL
#define PTRACE_KILL 8
#endif
#ifndef PTRACE_SINGLESTEP
#define PTRACE_SINGLESTEP 9
#endif
#ifndef PTRACE_GETREGS
#define PTRACE_GETREGS 12
#endif
#ifndef PTRACE_SETREGS
#define PTRACE_SETREGS 13
#endif
#ifndef PTRACE_GETFPREGS
#define PTRACE_GETFPREGS 14
#endif
#ifndef PTRACE_SETFPREGS
#define PTRACE_SETFPREGS 15
#endif
#ifndef PTRACE_ATTACH
#define PTRACE_ATTACH 16
#endif
#ifndef PTRACE_DETACH
#define PTRACE_DETACH 17
#endif
#ifndef PTRACE_SYSCALL
#define PTRACE_SYSCALL 24
#endif

#endif /* _LINUX_PTRACE_H */
