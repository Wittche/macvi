/* Minimal sys/epoll.h for macOS porting of FEX */
#ifndef _SYS_EPOLL_H
#define _SYS_EPOLL_H 1

#include <stdint.h>
#include <sys/types.h>

enum EPOLL_EVENTS {
  EPOLLIN = 0x001,
  EPOLLPRI = 0x002,
  EPOLLOUT = 0x004,
  EPOLLRDNORM = 0x040,
  EPOLLRDBAND = 0x080,
  EPOLLWRNORM = 0x100,
  EPOLLWRBAND = 0x200,
  EPOLLMSG = 0x400,
  EPOLLERR = 0x008,
  EPOLLHUP = 0x010,
  EPOLLRDHUP = 0x2000,
  EPOLLEXCLUSIVE = 1U << 28,
  EPOLLWAKEUP = 1U << 29,
  EPOLLONESHOT = 1U << 30,
  EPOLLET = 1U << 31
};

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

#define EPOLL_CLOEXEC 02000000
#define EPOLL_NONBLOCK 00004000

typedef union epoll_data {
  void *ptr;
  int fd;
  uint32_t u32;
  uint64_t u64;
} epoll_data_t;

struct epoll_event {
  uint32_t events;
  epoll_data_t data;
} __attribute__ ((__packed__));

#ifdef __cplusplus
extern "C" {
#endif

inline int epoll_create(int size) { return -1; }
inline int epoll_create1(int flags) { return -1; }
inline int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) { return -1; }
inline int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) { return -1; }
inline int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask) { return -1; }

#ifdef __cplusplus
}
#endif

#endif /* sys/epoll.h */
