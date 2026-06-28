/* Minimal linux/socket.h for macOS porting of FEX */
#ifndef _LINUX_SOCKET_H
#define _LINUX_SOCKET_H 1

#include <sys/socket.h>

struct mmsghdr {
	struct msghdr msg_hdr;
	unsigned int  msg_len;
};

#ifdef __APPLE__
#include <errno.h>
#include <sys/types.h>

static inline int recvmmsg(int, struct mmsghdr*, unsigned int, int, struct timespec*) {
  errno = ENOSYS;
  return -1;
}
static inline int sendmmsg(int, struct mmsghdr*, unsigned int, int) {
  errno = ENOSYS;
  return -1;
}
static inline int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
  if (flags == 0) return accept(sockfd, addr, addrlen);
  errno = ENOSYS;
  return -1;
}

#ifndef SO_ATTACH_FILTER
#define SO_ATTACH_FILTER 26
#endif
#ifndef SO_ATTACH_REUSEPORT_CBPF
#define SO_ATTACH_REUSEPORT_CBPF 51
#endif
#ifndef SO_RCVTIMEO_NEW
#define SO_RCVTIMEO_NEW 66
#endif
#ifndef SO_SNDTIMEO_NEW
#define SO_SNDTIMEO_NEW 67
#endif
#ifndef SO_SNDBUFFORCE
#define SO_SNDBUFFORCE 32
#endif
#ifndef SO_RCVBUFFORCE
#define SO_RCVBUFFORCE 33
#endif
#ifndef SO_NO_CHECK
#define SO_NO_CHECK 11
#endif
#ifndef SO_PRIORITY
#define SO_PRIORITY 12
#endif
#ifndef SO_BSDCOMPAT
#define SO_BSDCOMPAT 14
#endif
#ifndef SO_PASSCRED
#define SO_PASSCRED 16
#endif
#ifndef SO_PEERCRED
#define SO_PEERCRED 17
#endif
#ifndef SO_SECURITY_AUTHENTICATION
#define SO_SECURITY_AUTHENTICATION 22
#endif
#ifndef SO_SECURITY_ENCRYPTION_TRANSPORT
#define SO_SECURITY_ENCRYPTION_TRANSPORT 23
#endif
#ifndef SO_SECURITY_ENCRYPTION_NETWORK
#define SO_SECURITY_ENCRYPTION_NETWORK 24
#endif
#ifndef SO_DETACH_FILTER
#define SO_DETACH_FILTER 27
#endif
#ifndef SO_PEERNAME
#define SO_PEERNAME 28
#endif
#ifndef SO_PEERSEC
#define SO_PEERSEC 31
#endif
#ifndef SO_PASSSEC
#define SO_PASSSEC 34
#endif
#ifndef SO_MARK
#define SO_MARK 36
#endif
#ifndef SO_PROTOCOL
#define SO_PROTOCOL 38
#endif
#ifndef SO_DOMAIN
#define SO_DOMAIN 39
#endif
#ifndef SO_RXQ_OVFL
#define SO_RXQ_OVFL 40
#endif
#ifndef SO_WIFI_STATUS
#define SO_WIFI_STATUS 41
#endif
#ifndef SCM_WIFI_STATUS
#define SCM_WIFI_STATUS SO_WIFI_STATUS
#endif
#ifndef SO_PEEK_OFF
#define SO_PEEK_OFF 42
#endif
#ifndef SO_NOFCS
#define SO_NOFCS 43
#endif
#ifndef SO_LOCK_FILTER
#define SO_LOCK_FILTER 44
#endif
#ifndef SO_SELECT_ERR_QUEUE
#define SO_SELECT_ERR_QUEUE 45
#endif
#ifndef SO_BUSY_POLL
#define SO_BUSY_POLL 46
#endif
#ifndef SO_MAX_PACING_RATE
#define SO_MAX_PACING_RATE 47
#endif
#ifndef SO_BPF_EXTENSIONS
#define SO_BPF_EXTENSIONS 48
#endif
#ifndef SO_INCOMING_CPU
#define SO_INCOMING_CPU 49
#endif
#ifndef SO_ATTACH_BPF
#define SO_ATTACH_BPF 50
#endif
#ifndef SO_DETACH_BPF
#define SO_DETACH_BPF SO_DETACH_FILTER
#endif
#ifndef SO_ATTACH_REUSEPORT_EBPF
#define SO_ATTACH_REUSEPORT_EBPF 52
#endif
#ifndef SO_CNX_ADVICE
#define SO_CNX_ADVICE 53
#endif
#ifndef SCM_TIMESTAMPING_OPT_STATS
#define SCM_TIMESTAMPING_OPT_STATS 54
#endif
#ifndef SO_MEMINFO
#define SO_MEMINFO 55
#endif
#ifndef SO_INCOMING_NAPI_ID
#define SO_INCOMING_NAPI_ID 56
#endif
#ifndef SO_COOKIE
#define SO_COOKIE 57
#endif
#ifndef SCM_TIMESTAMPING_PKTINFO
#define SCM_TIMESTAMPING_PKTINFO 58
#endif
#ifndef SO_PEERGROUPS
#define SO_PEERGROUPS 59
#endif
#ifndef SO_ZEROCOPY
#define SO_ZEROCOPY 60
#endif
#ifndef SO_TXTIME
#define SO_TXTIME 61
#endif
#ifndef SCM_TXTIME
#define SCM_TXTIME SO_TXTIME
#endif
#ifndef SO_BINDTOIFINDEX
#define SO_BINDTOIFINDEX 62
#endif
#ifndef SO_TIMESTAMP_NEW
#define SO_TIMESTAMP_NEW 63
#endif
#ifndef SO_TIMESTAMPNS_NEW
#define SO_TIMESTAMPNS_NEW 64
#endif
#ifndef SO_TIMESTAMPING_NEW
#define SO_TIMESTAMPING_NEW 65
#endif

#endif

#endif /* _LINUX_SOCKET_H */
