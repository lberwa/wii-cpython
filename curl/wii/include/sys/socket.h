#ifndef CURL_WII_SYS_SOCKET_H
#define CURL_WII_SYS_SOCKET_H

#if WII_LIBOGC == 2
/* libogc2 hat kein sys/socket.h — alle Socket-Typen kommen aus network.h */
#include <network.h>

#ifndef AF_INET6
#define AF_INET6  28
#define PF_INET6  AF_INET6
#endif

#ifndef __u_char_defined
typedef unsigned char   u_char;
#define __u_char_defined
#endif
#ifndef __u_short_defined
typedef unsigned short  u_short;
#define __u_short_defined
#endif

/* h_errno: libogc2 hat keine Deklaration */
extern int h_errno;
#ifndef HOST_NOT_FOUND
#define HOST_NOT_FOUND 1
#endif

/* sockaddr_storage: immer definieren (libogc2 hat es nicht; HAVE_SOCKADDR_STORAGE
   kommt von pyconfig.h und darf den Guard nicht steuern) */
#ifndef _WII_SOCKADDR_STORAGE_DEFINED
#define _WII_SOCKADDR_STORAGE_DEFINED
#define _SS_MAXSIZE     128
#define _SS_ALIGNSIZE   (sizeof(long long))
#define _SS_PAD1SIZE    (_SS_ALIGNSIZE - sizeof(u_char) * 2)
#define _SS_PAD2SIZE    (_SS_MAXSIZE - sizeof(u_char) * 2 - \
                         _SS_PAD1SIZE - _SS_ALIGNSIZE)
struct sockaddr_storage {
    unsigned short  ss_family;
    char            __ss_pad1[_SS_PAD1SIZE];
    long long       __ss_align;
    char            __ss_pad2[_SS_PAD2SIZE];
};
#endif

#else /* WII_LIBOGC == 1 */
/* libogc hat eine vollstaendige sys/socket.h.
   pyconfig.h definiert socklen_t als Makro -> typedef-Konflikt.
   Makro entfernen OHNE _SOCKLEN_T_DECLARED zu setzen, damit libogc's socket.h
   den typedef selbst anlegt (mit dem korrekten __socklen_t-Typ = unsigned int). */
#ifdef socklen_t
#undef socklen_t
#endif
#include_next <sys/socket.h>
#ifndef HOST_NOT_FOUND
#define HOST_NOT_FOUND 1
#endif

#endif /* WII_LIBOGC */

#endif /* CURL_WII_SYS_SOCKET_H */
