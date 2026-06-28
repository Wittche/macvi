/* Minimal sys/timex.h for macOS porting of FEX */
#ifndef _SYS_TIMEX_H
#define _SYS_TIMEX_H 1

#include <sys/time.h>
#include <errno.h>

struct ntptimeval {
	struct timeval time;
	long maxerror;
	long esterror;
};

struct timex {
	unsigned int modes;
	long offset;
	long freq;
	long maxerror;
	long esterror;
	int status;
	long constant;
	long precision;
	long tolerance;
	struct timeval time;
	long tick;
	long ppsfreq;
	long jitter;
	int shift;
	long stabil;
	long jitcnt;
	long calcnt;
	long errcnt;
	long stbcnt;
	int tai;
};

#ifdef __APPLE__
#ifndef TIMER_ABSTIME
#define TIMER_ABSTIME 0x01
#endif

static inline int adjtimex(struct timex *buf) {
  errno = ENOSYS;
  return -1;
}

static inline int clock_adjtime(clockid_t clk_id, struct timex *buf) {
  errno = ENOSYS;
  return -1;
}
#endif

#endif /* sys/timex.h */
