/* Minimal linux/joystick.h for macOS porting of FEX */
#ifndef _LINUX_JOYSTICK_H
#define _LINUX_JOYSTICK_H 1

#include <linux/types.h>
#include <sys/ioctl.h>

#define JSIOCGVERSION		_IOR('j', 0x01, __u32)				/* get driver version */
#define JSIOCGAXES		_IOR('j', 0x11, __u8)				/* get number of axes */
#define JSIOCGBUTTONS		_IOR('j', 0x12, __u8)				/* get number of buttons */
#define JSIOCGNAME(len)		_IOC(_IOC_READ, 'j', 0x13, len)			/* get identifier string */

#endif /* _LINUX_JOYSTICK_H */
