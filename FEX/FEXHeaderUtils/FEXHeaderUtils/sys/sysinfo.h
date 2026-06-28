/* Minimal sys/sysinfo.h for macOS porting of FEX */
#ifndef _SYS_SYSINFO_H
#define _SYS_SYSINFO_H 1

#include <sys/types.h>
#include <errno.h>

struct sysinfo {
	long uptime;			/* Seconds since boot */
	unsigned long loads[3];		/* 1, 5, and 15 minute load averages */
	unsigned long totalram;		/* Total usable main memory size */
	unsigned long freeram;		/* Available memory size */
	unsigned long sharedram;	/* Amount of shared memory */
	unsigned long bufferram;	/* Memory used by buffers */
	unsigned long totalswap;	/* Total swap space size */
	unsigned long freeswap;		/* swap space still available */
	unsigned short procs;		/* Number of current processes */
	unsigned short pad;		/* Padding for binary compatibility */
	unsigned long totalhigh;	/* Total high memory size */
	unsigned long freehigh;		/* Available high memory size */
	unsigned int mem_unit;		/* Memory unit size in bytes */
	char _f[20-2*sizeof(long)-sizeof(int)];	/* Padding: libc5 uses this.. */
};

#ifdef __APPLE__
static inline int sysinfo(struct sysinfo *info) {
  errno = ENOSYS;
  return -1;
}
#endif

#endif /* _SYS_SYSINFO_H */
