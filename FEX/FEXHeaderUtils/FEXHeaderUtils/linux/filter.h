/* Minimal linux/filter.h for macOS porting of FEX */
#ifndef _LINUX_FILTER_H
#define _LINUX_FILTER_H

#include <linux/bpf_common.h>
#include <linux/types.h>

struct sock_filter {	/* Filter block */
	__u16	code;   /* Actual filter code */
	__u8	jt;	/* Jump true */
	__u8	jf;	/* Jump false */
	__u32	k;      /* Generic multiuse field */
};

struct sock_fprog {	/* Required for SO_ATTACH_FILTER. */
	unsigned short		len;	/* Number of filter blocks */
	struct sock_filter *filter;
};

#define BPF_STMT(code, k) { (unsigned short)(code), 0, 0, k }
#define BPF_JUMP(code, k, jt, jf) { (unsigned short)(code), jt, jf, k }

#endif /* _LINUX_FILTER_H */
