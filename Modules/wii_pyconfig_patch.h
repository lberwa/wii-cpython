
/* Wii/libogc BSD compatibility -- WII_BSD_COMPAT_ADDED */
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
#ifndef PF_INET
#define PF_INET  2
#endif
#ifndef PF_INET6
#define PF_INET6 30
#endif
#ifndef PF_UNSPEC
#define PF_UNSPEC 0
#endif
#ifndef IN_CLASSA_NSHIFT
#define IN_CLASSA_NSHIFT 24
#endif
#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET 127
#endif
#ifndef IN_MULTICAST
#define IN_MULTICAST(i)    (((u_long)(i) & 0xf0000000) == 0xe0000000)
#endif
#ifndef IN_EXPERIMENTAL
#define IN_EXPERIMENTAL(i) (((u_long)(i) & 0xe0000000) == 0xe0000000)
#endif
/* Disable POSIX semaphores -- sem_* not available on Wii/libogc */
#undef HAVE_SEM_TIMEDWAIT
/* Disable headers with u_short/u_int/u_long issues on Wii */
#undef HAVE_NET_IF_H
#undef HAVE_HSTRERROR

#endif /*Py_PYCONFIG_H*/
