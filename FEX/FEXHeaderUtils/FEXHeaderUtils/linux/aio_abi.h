/* Minimal linux/aio_abi.h for macOS porting of FEX */
#ifndef _LINUX_AIO_ABI_H
#define _LINUX_AIO_ABI_H 1

#include <stdint.h>

typedef uint64_t aio_context_t;

enum {
	IOCB_CMD_PREAD = 0,
	IOCB_CMD_PWRITE = 1,
	IOCB_CMD_FSYNC = 2,
	IOCB_CMD_FDSYNC = 3,
	IOCB_CMD_PREADV = 7,
	IOCB_CMD_PWRITEV = 8,
};

struct iocb {
	uint64_t aio_data;
	uint32_t aio_key;
	uint32_t aio_rw_flags;
	uint16_t aio_lio_opcode;
	int16_t  aio_reqprio;
	uint32_t aio_fildes;
	uint64_t aio_buf;
	uint64_t aio_nbytes;
	int64_t  aio_offset;
	uint64_t aio_reserved2;
	uint32_t aio_flags;
	uint32_t aio_resfd;
};

struct io_event {
	uint64_t data;
	uint64_t obj;
	int64_t  res;
	int64_t  res2;
};

#endif /* _LINUX_AIO_ABI_H */
