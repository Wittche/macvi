/* Minimal linux/utsname.h for macOS porting of FEX */
#ifndef _LINUX_UTSNAME_H
#define _LINUX_UTSNAME_H 1

#include <sys/utsname.h>

#ifdef __APPLE__
struct fex_utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
	char domainname[65];
};
#endif

#endif /* _LINUX_UTSNAME_H */
