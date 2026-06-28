/* Minimal linux/termios.h for macOS porting of FEX */
#ifndef _LINUX_TERMIOS_H
#define _LINUX_TERMIOS_H 1

#include <sys/termios.h>

#ifndef TCGETS
#define TCGETS		0x5401
#endif
#ifndef TCSETS
#define TCSETS		0x5402
#endif
#ifndef TCSETSW
#define TCSETSW		0x5403
#endif
#ifndef TCSETSF
#define TCSETSF		0x5404
#endif
#ifndef TCGETA
#define TCGETA		0x5405
#endif
#ifndef TCSETA
#define TCSETA		0x5406
#endif
#ifndef TCSETAW
#define TCSETAW		0x5407
#endif
#ifndef TCSETAF
#define TCSETAF		0x5408
#endif
#ifndef TCSBRK
#define TCSBRK		0x5409
#endif
#ifndef TCXONC
#define TCXONC		0x540A
#endif
#ifndef TCFLSH
#define TCFLSH		0x540B
#endif
#ifndef TIOCEXCL
#define TIOCEXCL	0x540C
#endif
#ifndef TIOCNXCL
#define TIOCNXCL	0x540D
#endif
#ifndef TIOCSCTTY
#define TIOCSCTTY	0x540E
#endif
#ifndef TIOCGPGRP
#define TIOCGPGRP	0x540F
#endif
#ifndef TIOCSPGRP
#define TIOCSPGRP	0x5410
#endif
#ifndef TIOCOUTQ
#define TIOCOUTQ	0x5411
#endif
#ifndef TIOCSTI
#define TIOCSTI		0x5412
#endif
#ifndef TIOCGWINSZ
#define TIOCGWINSZ	0x5413
#endif
#ifndef TIOCSWINSZ
#define TIOCSWINSZ	0x5414
#endif
#ifndef TIOCMGET
#define TIOCMGET	0x5415
#endif
#ifndef TIOCMBIS
#define TIOCMBIS	0x5416
#endif
#ifndef TIOCMBIC
#define TIOCMBIC	0x5417
#endif
#ifndef TIOCMSET
#define TIOCMSET	0x5418
#endif
#ifndef TIOCGSOFTCAR
#define TIOCGSOFTCAR	0x5419
#endif
#ifndef TIOCSSOFTCAR
#define TIOCSSOFTCAR	0x541A
#endif
#ifndef FIONREAD
#define FIONREAD	0x541B
#endif
#ifndef TIOCINQ
#define TIOCINQ		FIONREAD
#endif
#ifndef TIOCLINUX
#define TIOCLINUX	0x541C
#endif
#ifndef TIOCCONS
#define TIOCCONS	0x541D
#endif
#ifndef TIOCGSERIAL
#define TIOCGSERIAL	0x541E
#endif
#ifndef TIOCSSERIAL
#define TIOCSSERIAL	0x541F
#endif
#ifndef TIOCPKT
#define TIOCPKT		0x5420
#endif
#ifndef FIONBIO
#define FIONBIO		0x5421
#endif
#ifndef TIOCNOTTY
#define TIOCNOTTY	0x5422
#endif
#ifndef TIOCSETD
#define TIOCSETD	0x5423
#endif
#ifndef TIOCGETD
#define TIOCGETD	0x5424
#endif
#ifndef TCSBRKP
#define TCSBRKP		0x5425
#endif
#ifndef TIOCSBRK
#define TIOCSBRK	0x5427
#endif
#ifndef TIOCCBRK
#define TIOCCBRK	0x5428
#endif
#ifndef TIOCGSID
#define TIOCGSID	0x5429
#endif
#ifndef TIOCGRS485
#define TIOCGRS485	0x542E
#endif
#ifndef TIOCSRS485
#define TIOCSRS485	0x542F
#endif
#ifndef TIOCGPTN
#define TIOCGPTN	0x80045430
#endif
#ifndef TIOCSPTLCK
#define TIOCSPTLCK	0x40045431
#endif
#ifndef TIOCGDEV
#define TIOCGDEV	0x80045432
#endif
#ifndef TCGETX
#define TCGETX		0x5432
#endif
#ifndef TCSETX
#define TCSETX		0x5433
#endif
#ifndef TCSETXF
#define TCSETXF		0x5434
#endif
#ifndef TCSETXW
#define TCSETXW		0x5435
#endif
#ifndef TIOCVHANGUP
#define TIOCVHANGUP	0x5437
#endif
#ifndef TIOCGPKT
#define TIOCGPKT	0x80045438
#endif
#ifndef TIOCGPTLCK
#define TIOCGPTLCK	0x80045439
#endif
#ifndef TIOCGEXCL
#define TIOCGEXCL	0x80045440
#endif
#ifndef TIOCGPTPEER
#define TIOCGPTPEER	0x5441
#endif
#ifndef TIOCSERCONFIG
#define TIOCSERCONFIG	0x5453
#endif
#ifndef TIOCSERGWILD
#define TIOCSERGWILD	0x5454
#endif
#ifndef TIOCSERSWILD
#define TIOCSERSWILD	0x5455
#endif
#ifndef TIOCGLCKTRMIOS
#define TIOCGLCKTRMIOS	0x5456
#endif
#ifndef TIOCSLCKTRMIOS
#define TIOCSLCKTRMIOS	0x5457
#endif
#ifndef TIOCSERGSTRUCT
#define TIOCSERGSTRUCT	0x5458
#endif
#ifndef TIOCSERGETLSR
#define TIOCSERGETLSR	0x5459
#endif
#ifndef TIOCSERGETMULTI
#define TIOCSERGETMULTI	0x545A
#endif
#ifndef TIOCSERSETMULTI
#define TIOCSERSETMULTI	0x545B
#endif
#ifndef TIOCMIWAIT
#define TIOCMIWAIT	0x545C
#endif
#ifndef TIOCGICOUNT
#define TIOCGICOUNT	0x545D
#endif
#ifndef FIOQSIZE
#define FIOQSIZE	0x5460
#endif

#endif /* _LINUX_TERMIOS_H */
