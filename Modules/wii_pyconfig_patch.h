
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

/* Socket functions genuinely missing on libogc/libogc2 -> keep disabled. */
#undef HAVE_SENDMSG
#undef HAVE_RECVMSG
#undef HAVE_GETSERVBY
#undef HAVE_GETPROTOBYNAME
#undef HAVE_GETHOSTBYADDR
#undef HAVE_SELECT
#undef HAVE_H_ERRNO

/* Socket functions that ARE available on the Wii but which autoconf cannot
 * detect in the cross-build (it can't link a test program against libogc), so
 * they end up #undef in the generated pyconfig.h.  Force-enable them here so
 * socketmodule.c exposes the corresponding methods (bind/connect/setsockopt/
 * socketpair/...).  Backing implementations:
 *   bind/listen/sendto/recvfrom/setsockopt/getsockname/shutdown -> net_* (libogc,
 *     mapped via curl/wii/include/curl_wii_net_compat.h, present in libogc1+2)
 *   connect/accept        -> curl_wii_* (static inline in the compat header)
 *   getpeername/socketpair -> Modules/wii_socket_stubs.c
 * Without these, asyncio (socketpair -> bind fallback) and pip (setsockopt)
 * fail with "'socket' object has no attribute 'bind'/'setsockopt'". */
#define HAVE_BIND 1
#define HAVE_CONNECT 1
#define HAVE_LISTEN 1
#define HAVE_ACCEPT 1
#define HAVE_SETSOCKOPT 1
#define HAVE_GETSOCKNAME 1
#define HAVE_GETPEERNAME 1
#define HAVE_RECVFROM 1
#define HAVE_SENDTO 1
#define HAVE_SHUTDOWN 1
#define HAVE_SOCKETPAIR 1
/* getaddrinfo/getnameinfo: wii_socket_stubs.c liefert Implementierungen.
   HAVE_GETADDRINFO kommt via -DHAVE_GETADDRINFO auf der Kommandozeile. */

/* ioctl not available on Wii -- _Py_set_blocking stubs out without it */
#undef HAVE_IOCTL

/* struct sockaddr_storage: libogc/libogc2 bietet es an (via socket.h oder Stub) */
#define HAVE_SOCKADDR_STORAGE 1
/* sin6_len fehlt in libogc's struct sockaddr_in6; HAVE_SOCKADDR_SA_LEN unterdrücken */
#undef HAVE_SOCKADDR_SA_LEN
/* struct addrinfo: libogc/libogc2 stellen es bereit (via netdb.h stub oder direkt) */
#define HAVE_ADDRINFO 1

/* uuid: libuuid liegt in uuid/install-wii/include/uuid/uuid.h */
#undef HAVE_UUID_H
#define HAVE_UUID_UUID_H 1

#endif /*Py_PYCONFIG_H*/
