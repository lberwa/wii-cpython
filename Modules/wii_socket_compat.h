#ifndef WII_SOCKET_COMPAT_H
#define WII_SOCKET_COMPAT_H

/* BSD type aliases missing when __BSD_VISIBLE=0 (devkitPPC sys/features.h) */
#ifndef _U_SHORT_DEFINED
typedef unsigned short  u_short;
#define _U_SHORT_DEFINED
#endif
#ifndef _U_LONG_DEFINED
typedef unsigned long   u_long;
#define _U_LONG_DEFINED
#endif
#ifndef _U_INT_DEFINED
typedef unsigned int    u_int;
#define _U_INT_DEFINED
#endif

/* netinet/in.h BSD constants (guarded by __BSD_VISIBLE in libogc) */
#ifndef IN_CLASSA_NSHIFT
#define IN_CLASSA_NSHIFT 24
#endif
#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET 127
#endif

/* PF_* aliases (libogc uses AF_* directly) */
#ifndef PF_INET
#define PF_INET  AF_INET
#endif
#ifndef PF_INET6
#define PF_INET6 AF_INET6
#endif
#ifndef PF_UNSPEC
#define PF_UNSPEC AF_UNSPEC
#endif

/* IN_MULTICAST / IN_EXPERIMENTAL as macros matching libogc definitions */
#ifndef IN_MULTICAST
#define IN_MULTICAST(i) (((u_long)(i) & 0xf0000000) == 0xe0000000)
#endif
#ifndef IN_EXPERIMENTAL
#define IN_EXPERIMENTAL(i) (((u_long)(i) & 0xe0000000) == 0xe0000000)
#endif

#endif /* WII_SOCKET_COMPAT_H */
