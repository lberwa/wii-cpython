#ifndef WII_SYS_IOCTL_H
#define WII_SYS_IOCTL_H

/*
 * devkitPPC/newlib does not provide <sys/ioctl.h>.  SQLite's Unix VFS includes
 * it unconditionally, and CPython's socketmodule uses ioctl(FIONBIO) when the
 * configure result says the header exists.  The implementation lives in
 * Modules/wii_socket_stubs.c and forwards to libogc net_ioctl().
 */

#if !defined(FIONREAD) || !defined(FIONBIO)
#define IOCPARM_MASK 0x7f
#define IOC_VOID 0x20000000
#define IOC_OUT 0x40000000
#define IOC_IN 0x80000000
#define IOC_INOUT (IOC_IN | IOC_OUT)
#define _IO(x, y) (IOC_VOID | ((x) << 8) | (y))
#define _IOR(x, y, t) (IOC_OUT | (((long)sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y))
#define _IOW(x, y, t) (IOC_IN | (((long)sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y))
#define _IOWR(x, y, t) (IOC_INOUT | (((long)sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y))
#endif

#ifndef FIONREAD
#define FIONREAD _IOR('f', 127, unsigned long)
#endif

#ifndef FIONBIO
#define FIONBIO _IOW('f', 126, unsigned long)
#endif

int ioctl(int fd, int request, ...);

#endif /* WII_SYS_IOCTL_H */
