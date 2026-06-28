// SPDX-License-Identifier: MIT
#pragma once

#include <FEXCore/Utils/LogManager.h>

#include <cstdint>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <mutex>
#include <unordered_map>
#ifndef _WIN32
#if defined(__APPLE__)
#include <sys/syscall.h>
#include <pthread.h>
#else
#include <syscall.h>
#endif
#else
#include <processthreadsapi.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/syscall.h>

// Missing or private Darwin syscalls
#ifndef SYS_nanosleep
#define SYS_nanosleep 240
#endif
#ifndef SYS_clock_gettime
#define SYS_clock_gettime 427
#endif
#ifndef SYS_clock_getres
#define SYS_clock_getres 428
#endif
#ifndef SYS_clock_nanosleep
#define SYS_clock_nanosleep 429
#endif
#ifndef SYS_thread_selfid
#define SYS_thread_selfid 372
#endif
#ifndef SYS_pselect
#define SYS_pselect 394
#endif
#ifndef SYS_ppoll
#define SYS_ppoll 395
#endif
#ifndef SYS_getentropy
#define SYS_getentropy 500
#endif
#ifndef SYS_preadv
#define SYS_preadv 290
#endif
#ifndef SYS_pwritev
#define SYS_pwritev 289
#endif

#endif

namespace FHU::Syscalls {

#if defined(__APPLE__)
// Mapping from Linux signal numbers to Darwin signal numbers.
inline int LinuxToDarwinSignal(int sig) {
  switch (sig) {
    case 1: return SIGHUP;
    case 2: return SIGINT;
    case 3: return SIGQUIT;
    case 4: return SIGILL;
    case 5: return SIGTRAP;
    case 6: return SIGABRT;
    case 7: return SIGBUS;
    case 8: return SIGFPE;
    case 9: return SIGKILL;
    case 10: return SIGUSR1;
    case 11: return SIGSEGV;
    case 12: return SIGUSR2;
    case 13: return SIGPIPE;
    case 14: return SIGALRM;
    case 15: return SIGTERM;
    case 17: return SIGCHLD;
    case 18: return SIGCONT;
    case 19: return SIGSTOP;
    case 20: return SIGTSTP;
    case 21: return SIGTTIN;
    case 22: return SIGTTOU;
    case 23: return SIGURG;
    case 24: return SIGXCPU;
    case 25: return SIGXFSZ;
    case 26: return SIGVTALRM;
    case 27: return SIGPROF;
    case 28: return SIGWINCH;
    case 29: return SIGIO;
    case 31: return SIGSYS;
    case 63: return SIGEMT;
    default: return -1;
  }
}

// Mapping from Darwin signal numbers to Linux signal numbers.
inline int DarwinToLinuxSignal(int sig) {
  switch (sig) {
    case SIGHUP: return 1;
    case SIGINT: return 2;
    case SIGQUIT: return 3;
    case SIGILL: return 4;
    case SIGTRAP: return 5;
    case SIGABRT: return 6;
    case SIGEMT: return 63;
    case SIGBUS: return 7;
    case SIGFPE: return 8;
    case SIGKILL: return 9;
    case SIGUSR1: return 10;
    case SIGSEGV: return 11;
    case SIGUSR2: return 12;
    case SIGPIPE: return 13;
    case SIGALRM: return 14;
    case SIGTERM: return 15;
    case SIGCHLD: return 17;
    case SIGCONT: return 18;
    case SIGSTOP: return 19;
    case SIGTSTP: return 20;
    case SIGTTIN: return 21;
    case SIGTTOU: return 22;
    case SIGURG: return 23;
    case SIGXCPU: return 24;
    case SIGXFSZ: return 25;
    case SIGVTALRM: return 26;
    case SIGPROF: return 27;
    case SIGWINCH: return 28;
    case SIGIO: return 29;
    case SIGSYS: return 31;
    default: return -1;
  }
}

inline void LinuxToDarwinSigset(uint64_t linux_mask, sigset_t* darwin_set) {
  sigemptyset(darwin_set);
  for (int i = 1; i <= 64; ++i) {
    if ((linux_mask >> (i - 1)) & 1) {
      int darwin_sig = LinuxToDarwinSignal(i);
      if (darwin_sig != -1) {
        sigaddset(darwin_set, darwin_sig);
      }
    }
  }
}

inline void DarwinToLinuxSigset(const sigset_t* darwin_set, uint64_t* linux_mask) {
  *linux_mask = 0;
  for (int i = 1; i < NSIG; ++i) {
    if (sigismember(darwin_set, i)) {
      int linux_sig = DarwinToLinuxSignal(i);
      if (linux_sig != -1) {
        *linux_mask |= (1ULL << (linux_sig - 1));
      }
    }
  }
}

// Mapping from Linux ARM64/x86-64 syscall numbers to Darwin ARM64 syscall numbers.
inline int LinuxToDarwin(int linux_syscall) {
  int result = -1;
  auto do_map = [&](int syscall) -> int {
    switch (syscall) {
      // x86-64 Guest & ARM64 Host Common / Crucial Mappings
      case 0: return SYS_read;        // x86-64: 0, ARM64: 63
      case 1: return SYS_write;       // x86-64: 1, ARM64: 64
      case 2: return SYS_open;        // x86-64: 2
      case 3: return SYS_close;       // x86-64: 3, ARM64: 57
      case 4: return SYS_fstatat;     // x86-64: 4 (stat)
      case 5: return SYS_fstat;       // x86-64: 5
      case 6: return SYS_fstatat;     // x86-64: 6 (lstat)
      case 7: return SYS_poll;        // x86-64: 7
      case 8: return SYS_lseek;       // x86-64: 8, ARM64: 62
      case 9: return SYS_mmap;        // x86-64: 9, ARM64: 222
      case 10: return SYS_mprotect;   // x86-64: 10, ARM64: 226
      case 11: return SYS_munmap;     // x86-64: 11, ARM64: 215
      case 12: return -1;             // x86-64: 12 (brk)
      case 13: return SYS_sigaction;  // x86-64: 13, ARM64: 134
      case 14: return SYS_sigprocmask; // x86-64: 14, ARM64: 135
      case 16: return SYS_ioctl;      // x86-64: 16, ARM64: 29
      case 19: return SYS_readv;      // x86-64: 19, ARM64: 65
      case 20: return SYS_writev;     // x86-64: 20, ARM64: 66
      case 21: return SYS_access;     // x86-64: 21
      case 24: return SYS_dup2;       // ARM64: 24 (dup3)
      case 25: return SYS_fcntl;      // ARM64: 25
      case 32: return SYS_dup;        // x86-64: 32
      case 33: return SYS_dup2;       // x86-64: 33
      case 34: return SYS_mkdirat;    // ARM64: 34
      case 35: return SYS_unlinkat;   // ARM64: 35
      case 37: return 27;             // x86-64: 37 (alarm) -> Darwin 27
      case 38: return SYS_renameat;   // ARM64: 38
      case 39: return SYS_getpid;     // x86-64: 39, ARM64: 172
      case 40: return SYS_mount;      // ARM64: 40
      case 41: return SYS_socket;     // x86-64: 41, ARM64: 198
      case 42: return SYS_connect;    // x86-64: 42, ARM64: 203
      case 43: return SYS_statfs;     // ARM64: 43
      case 44: return SYS_fstatfs;    // ARM64: 44
      case 45: return SYS_truncate;   // ARM64: 45
      case 46: return SYS_ftruncate;  // ARM64: 46
      case 47: return SYS_recvfrom;   // x86-64: 47, ARM64: 207
      case 48: return SYS_fchmodat;   // ARM64: 48
      case 49: return SYS_fchownat;   // ARM64: 49
      case 50: return SYS_fchown;     // ARM64: 50
      case 51: return SYS_openat;     // ARM64: 51
      case 52: return SYS_close;      // ARM64: 52
      case 56: return SYS_openat;     // ARM64: 56
      case 59: return SYS_pipe;       // ARM64: 59 (pipe2)
      case 60: return SYS_exit;       // x86-64: 60, ARM64: 93
      case 61: return -1;             // ARM64: 61 (getdents64 - handled in Syscalls.cpp)
      case 62: return SYS_lseek;      // ARM64: 62
      case 63: return 164;            // x86-64: 63 (uname), ARM64: 63 (read - Collision handled by prioritizing uname for now)
      case 64: return SYS_write;      // ARM64: 64
      case 65: return SYS_readv;      // ARM64: 65
      case 66: return SYS_writev;     // ARM64: 66
      case 67: return SYS_pread;      // ARM64: 67
      case 68: return SYS_pwrite;     // ARM64: 68
      case 69: return SYS_preadv;     // ARM64: 69
      case 70: return SYS_pwritev;    // ARM64: 70
      case 72: return SYS_fcntl;      // x86-64: 72, ARM64: 72 (pselect6 - Collision handled by prioritizing fcntl)
      case 73: return SYS_ppoll;      // ARM64: 73 (ppoll)
      case 78: return SYS_readlinkat; // ARM64: 78
      case 79: return 299;            // x86-64: 79 (getcwd), ARM64: 79 (fstatat - Collision handled by prioritizing getcwd)
      case 80: return SYS_chdir;      // x86-64: 80, ARM64: 80 (fstat)
      case 89: return SYS_readlink;   // x86-64: 89
      case 93: return SYS_exit;       // ARM64: 93
      case 94: return SYS_exit;       // ARM64: 94 (exit_group)
      case 101: return SYS_nanosleep; // ARM64: 101
      case 102: return SYS_getuid;    // x86-64: 102, ARM64: 174
      case 104: return SYS_getgid;    // x86-64: 104, ARM64: 176
      case 105: return SYS_setuid;    // x86-64: 105, ARM64: 146
      case 106: return SYS_setgid;    // x86-64: 106, ARM64: 144
      case 107: return SYS_geteuid;   // x86-64: 107, ARM64: 175
      case 108: return SYS_getegid;   // x86-64: 108, ARM64: 177
      case 110: return SYS_getppid;   // x86-64: 110, ARM64: 173
      case 113: return SYS_clock_gettime; // ARM64: 113
      case 114: return SYS_clock_getres;  // ARM64: 114
      case 115: return SYS_clock_nanosleep; // ARM64: 115
      case 134: return SYS_sigaction;  // ARM64: 134
      case 135: return SYS_sigprocmask; // ARM64: 135
      case 142: return -1;            // x86-64: 142 (sched_setparam)
      case 143: return -1;            // x86-64: 143 (sched_getparam)
      case 144: return -1;            // x86-64: 144 (sched_setscheduler)
      case 145: return -1;            // x86-64: 145 (sched_getscheduler)
      case 146: return -1;            // x86-64: 146 (sched_get_priority_max)
      case 147: return -1;            // x86-64: 147 (sched_get_priority_min)
      case 160: return 164;            // ARM64: 160 (uname)
      case 165: return SYS_getrusage;
      case 166: return SYS_umask;
      case 169: return SYS_gettimeofday;
      case 172: return SYS_getpid;
      case 173: return SYS_getppid;
      case 174: return SYS_getuid;
      case 175: return SYS_geteuid;
      case 176: return SYS_getgid;
      case 177: return SYS_getegid;
      case 178: return SYS_thread_selfid; // ARM64: 178 (gettid)
      case 186: return SYS_thread_selfid; // x86-64: 186 (gettid)
      case 200: return SYS_bind;
      case 201: return SYS_listen;
      case 202: return SYS_accept;
      case 203: return SYS_connect;
      case 204: return SYS_getsockname;
      case 205: return SYS_getpeername;
      case 206: return SYS_sendto;
      case 207: return SYS_recvfrom;
      case 208: return SYS_setsockopt;
      case 209: return SYS_getsockopt;
      case 210: return SYS_shutdown;
      case 211: return SYS_sendmsg;
      case 212: return SYS_recvmsg;
      case 215: return SYS_munmap;
      case 217: return -1;             // x86-64: 217 (getdents64 - handled in Syscalls.cpp)
      case 222: return SYS_mmap;
      case 226: return SYS_mprotect;
      case 228: return SYS_clock_gettime; // x86-64: 228
      case 231: return SYS_exit;       // x86-64: 231 (exit_group)
      case 233: return SYS_madvise;
      case 242: return SYS_accept;     // ARM64: 242 (accept4)
      case 257: return SYS_openat;    // x86-64: 257
      case 258: return SYS_mkdirat;
      case 260: return SYS_fchownat;
      case 262: return SYS_fstatat;   // x86-64: 262
      case 263: return SYS_unlinkat;
      case 264: return SYS_renameat;
      case 265: return SYS_linkat;
      case 266: return SYS_symlinkat;
      case 267: return SYS_readlinkat;
      case 268: return SYS_fchmodat;
      case 269: return SYS_faccessat;
      case 278: return SYS_getentropy; // ARM64: 278 (getrandom), x86-64: 318
      case 295: return SYS_preadv;     // x86-64: 295, ARM64: 69
      case 296: return SYS_pwritev;    // x86-64: 296, ARM64: 70
      case 318: return SYS_getentropy; // x86-64: 318 (getrandom)
        case 191: return SYS_getxattr; // x86-64: 191 (getxattr)
        case 192: return SYS_getxattr; // x86-64: 192 (lgetxattr) - Map to getxattr with NOFOLLOW in FM
        case 194: return SYS_listxattr; // x86-64: 194 (listxattr)
        case 195: return SYS_listxattr; // x86-64: 195 (llistxattr) - Map to listxattr with NOFOLLOW in FM
        case 197: return SYS_removexattr; // x86-64: 197 (removexattr)
        case 198: return SYS_removexattr; // x86-64: 198 (lremovexattr) - Map to removexattr with NOFOLLOW in FM
      case 439: return SYS_faccessat; // x86-64: 439 (faccessat2)
      case 437: return -1; // x86-64: 437 (openat2) - No Darwin equivalent
      default: return -1;
    }
  };
  result = do_map(linux_syscall);
  if (result == -1 && linux_syscall != -1 && linux_syscall != 12 && linux_syscall != 61 && linux_syscall != 217 && linux_syscall != 437 && linux_syscall != 439 && linux_syscall != 191 && linux_syscall != 192 && linux_syscall != 194 && linux_syscall != 195 && linux_syscall != 197 && linux_syscall != 198) {
    fprintf(stderr, "FEXDBG: [LinuxToDarwin] Unmapped linux_syscall=%d\n", linux_syscall);
    fflush(stderr);
  }
  return result;
}
#endif

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

#ifndef SEM_STAT_ANY
#define SEM_STAT_ANY 20
#endif

#ifndef SHM_STAT_ANY
#define SHM_STAT_ANY 15
#endif

#ifndef MSG_STAT_ANY
#define MSG_STAT_ANY 13
#endif

#ifndef CLONE_PIDFD
#define CLONE_PIDFD 0x00001000
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#ifndef SYS_statx
#define SYS_statx 291
#endif
#elif defined(__x86_64__) || defined(_M_X64)
#ifndef SYS_statx
#define SYS_statx 332
#endif
#endif

// Common syscall numbers
#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif

#ifndef _WIN32
inline int32_t getcpu(uint32_t* cpu, uint32_t* node) {
  // Third argument is unused
#if defined(__APPLE__)
  if (cpu) *cpu = 0; // macOS doesn't have an easy getcpu equivalent
  if (node) *node = 0;
  return 0;
#elif defined(HAS_SYSCALL_GETCPU) && HAS_SYSCALL_GETCPU
  return ::getcpu(cpu, node);
#else
  return ::syscall(SYS_getcpu, cpu, node, nullptr);
#endif
}

inline int32_t gettid() {
#if defined(__APPLE__)
  uint64_t tid;
  pthread_threadid_np(NULL, &tid);
  return (int32_t)tid;
#elif defined(HAS_SYSCALL_GETTID) && HAS_SYSCALL_GETTID
  return ::gettid();
#else
  return ::syscall(SYS_gettid);
#endif
}

#if defined(__APPLE__)
inline std::mutex& GetThreadMapMutex() {
  static std::mutex mtx;
  return mtx;
}
inline std::unordered_map<pid_t, pthread_t>& GetThreadMap() {
  static std::unordered_map<pid_t, pthread_t> map;
  return map;
}
inline void RegisterThreadPThread(pid_t tid, pthread_t thread) {
  std::lock_guard<std::mutex> lock(GetThreadMapMutex());
  GetThreadMap()[tid] = thread;
}
inline void UnregisterThreadPThread(pid_t tid) {
  std::lock_guard<std::mutex> lock(GetThreadMapMutex());
  GetThreadMap().erase(tid);
}
#endif

inline int32_t tgkill(pid_t tgid, pid_t tid, int sig) {
#if defined(__APPLE__)
  uint64_t self_tid;
  pthread_threadid_np(NULL, &self_tid);
  if ((uint64_t)tid == self_tid) {
    return ::pthread_kill(::pthread_self(), sig);
  }

  pthread_t target_thread {};
  {
    std::lock_guard<std::mutex> lock(GetThreadMapMutex());
    auto it = GetThreadMap().find(tid);
    if (it != GetThreadMap().end()) {
      target_thread = it->second;
    }
  }

  if (target_thread) {
    return ::pthread_kill(target_thread, sig);
  }

  return ::kill(tgid, sig);
#elif defined(HAS_SYSCALL_TGKILL) && HAS_SYSCALL_TGKILL
  return ::tgkill(tgid, tid, sig);
#else
  return ::syscall(SYS_tgkill, tgid, tid, sig);
#endif
}

struct statx_timestamp {
	int64_t	tv_sec;
	uint32_t tv_nsec;
	int32_t	__reserved;
};

struct statx {
	uint32_t stx_mask;
	uint32_t stx_blksize;
	uint64_t stx_attributes;
	uint32_t stx_nlink;
	uint32_t stx_uid;
	uint32_t stx_gid;
	uint16_t stx_mode;
	uint16_t __spare0[1];
	uint64_t stx_ino;
	uint64_t stx_size;
	uint64_t stx_blocks;
	uint64_t stx_attributes_mask;
	struct statx_timestamp	stx_atime;
	struct statx_timestamp	stx_btime;
	struct statx_timestamp	stx_ctime;
	struct statx_timestamp	stx_mtime;
	uint32_t stx_rdev_major;
	uint32_t stx_rdev_minor;
	uint32_t stx_dev_major;
	uint32_t stx_dev_minor;
	uint64_t stx_mnt_id;
	uint64_t __spare2[14];
};

inline int32_t statx(int dirfd, const char* pathname, int32_t flags, uint32_t mask, void* statxbuf) {
#if defined(__APPLE__)
  struct stat st;
  
  // AT_EMPTY_PATH is 0x1000 in Linux and mapped to 0x1000 in FEX on macOS.
  // We only use the FD-based stat if pathname is NULL or empty.
  if ((flags & 0x1000) && (!pathname || pathname[0] == '\0')) {
    int res;
    if (dirfd == AT_FDCWD) {
      res = fstatat(dirfd, ".", &st, 0);
    } else {
      res = fstat(dirfd, &st);
    }
    if (res == -1) return -1;
    goto fill_statx;
  }

  {
    // Mask out statx-specific flags and our private AT_EMPTY_PATH before calling fstatat.
    // Known macOS fstatat flags: AT_SYMLINK_NOFOLLOW (0x20), AT_REMOVEDIR (0x80), AT_SYMLINK_FOLLOW (0x40), AT_EACCESS (0x10).
    int host_flags = flags & (AT_SYMLINK_NOFOLLOW | AT_REMOVEDIR | AT_SYMLINK_FOLLOW | AT_EACCESS);
    int res = fstatat(dirfd, pathname ? pathname : "", &st, host_flags);
    if (res == -1) return -1;
  }

fill_statx:
  struct statx* stx = reinterpret_cast<struct statx*>(statxbuf);
  memset(stx, 0, sizeof(*stx));

  stx->stx_mask = 0x7ff; // STATX_BASIC_STATS
  stx->stx_blksize = st.st_blksize;
  stx->stx_nlink = st.st_nlink;
  stx->stx_uid = st.st_uid;
  stx->stx_gid = st.st_gid;
  stx->stx_mode = st.st_mode;
  stx->stx_ino = st.st_ino;
  stx->stx_size = st.st_size;
  stx->stx_blocks = st.st_blocks;
  stx->stx_atime.tv_sec = st.st_atimespec.tv_sec;
  stx->stx_atime.tv_nsec = st.st_atimespec.tv_nsec;
  stx->stx_ctime.tv_sec = st.st_ctimespec.tv_sec;
  stx->stx_ctime.tv_nsec = st.st_ctimespec.tv_nsec;
  stx->stx_mtime.tv_sec = st.st_mtimespec.tv_sec;
  stx->stx_mtime.tv_nsec = st.st_mtimespec.tv_nsec;
  stx->stx_dev_major = major(st.st_dev);
  stx->stx_dev_minor = minor(st.st_dev);
  stx->stx_rdev_major = major(st.st_rdev);
  stx->stx_rdev_minor = minor(st.st_rdev);

  return 0;
#elif defined(HAS_SYSCALL_STATX) && HAS_SYSCALL_STATX
  return ::statx(dirfd, pathname, flags, mask, reinterpret_cast<struct statx* __restrict>(statxbuf));
#else
  return ::syscall(SYS_statx, dirfd, pathname, flags, mask, statxbuf);
#endif
}

inline int32_t renameat2(int olddirfd, const char* oldpath, int newdirfd, const char* newpath, unsigned int flags) {
#if defined(__APPLE__)
  return -1; // Stub
#elif defined(HAS_SYSCALL_RENAMEAT2) && HAS_SYSCALL_RENAMEAT2
  return ::renameat2(olddirfd, oldpath, newdirfd, newpath, flags);
#else
  return ::syscall(SYS_renameat2, olddirfd, oldpath, newdirfd, newpath, flags);
#endif
}

inline int32_t pidfd_open(pid_t pid, unsigned int flags) {
#if defined(__APPLE__)
  return -1; // Stub
#else
  return ::syscall(SYS_pidfd_open, pid, flags);
#endif
}
#else

inline int32_t getcpu(uint32_t* cpu, uint32_t* node) {
  if (cpu) {
    *cpu = GetCurrentProcessorNumber();
  }
  if (node) {
    *node = 0;
  }
  return 0;
}

inline int32_t gettid() {
  return GetCurrentThreadId();
}

#endif

} // namespace FHU::Syscalls
