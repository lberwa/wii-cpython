/* Wii socket compatibility for socketmodule.c
 *
 * We define struct addrinfo + getaddrinfo here (based on libogc's
 * net_gethostbyname), then set _ADDRINFO_H_INCLUDED so socketmodule.c:429's
 * unconditional #include "addrinfo.h" is skipped by addrinfo.h's own guard.
 * -DHAVE_GETADDRINFO in Setup.local causes setipaddr() to be compiled.
 */
#ifndef WII_SOCKET_COMPAT_H
#define WII_SOCKET_COMPAT_H

#ifdef WII_BUILD

/* pthread types required by pthread.h pulled in via sys/types.h */
#ifndef _POSIX_THREADS
#  define _POSIX_THREADS
#endif

/* curl/wii/include/ is in -I; maps POSIX socket names to libogc net_* */
#include "curl_wii_net_compat.h"

/* BSD u_char used in addrinfo fallback code */
#ifndef _U_CHAR_DEFINED
#  define _U_CHAR_DEFINED
typedef unsigned char u_char;
#endif

/* h_errno: libogc DNS errors arrive in errno */
#ifndef h_errno
#  define h_errno errno
#endif

/* inet_ntop: IPv4-only fallback (libogc only has inet_ntoa) */
#include <stdio.h>
static inline const char *
wii_inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    if (af == AF_INET) {
        const unsigned char *b = (const unsigned char *)src;
        snprintf(dst, (size_t)size, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
        return dst;
    }
    return NULL;
}
#define inet_ntop wii_inet_ntop

/* ----------------------------------------------------------------
 * Provide struct addrinfo + getaddrinfo/freeaddrinfo for Wii.
 * We set _ADDRINFO_H_INCLUDED so addrinfo.h's include-guard fires when
 * socketmodule.c includes it at line 429, preventing redefinitions.
 * ---------------------------------------------------------------- */
#ifndef _ADDRINFO_H_INCLUDED
#define _ADDRINFO_H_INCLUDED

#include <stdlib.h>   /* calloc, free */
#include <string.h>   /* strtoul */

#ifndef AI_PASSIVE
#  define AI_PASSIVE     0x0001
#endif
#ifndef AI_CANONNAME
#  define AI_CANONNAME   0x0002
#endif
#ifndef AI_NUMERICHOST
#  define AI_NUMERICHOST 0x0004
#endif
#ifndef AI_NUMERICSERV
#  define AI_NUMERICSERV 0x0008
#endif
#ifndef AI_ADDRCONFIG
#  define AI_ADDRCONFIG  0x0020
#endif

#ifndef EAI_NONAME
#  define EAI_NONAME  8
#endif
#ifndef EAI_AGAIN
#  define EAI_AGAIN   2
#endif
#ifndef EAI_FAIL
#  define EAI_FAIL    4
#endif
#ifndef EAI_MEMORY
#  define EAI_MEMORY 10
#endif
#ifndef EAI_FAMILY
#  define EAI_FAMILY  6
#endif
#ifndef EAI_SERVICE
#  define EAI_SERVICE 12
#endif

struct addrinfo {
    int              ai_flags;
    int              ai_family;
    int              ai_socktype;
    int              ai_protocol;
    socklen_t        ai_addrlen;
    struct sockaddr *ai_addr;
    char            *ai_canonname;
    struct addrinfo *ai_next;
};

#ifndef HAVE_SOCKADDR_STORAGE
#define HAVE_SOCKADDR_STORAGE 1
struct sockaddr_storage {
    unsigned short ss_family;
    char           __ss_pad[126];
};
#endif

static inline const char *gai_strerror(int e)
{
    switch (e) {
    case EAI_NONAME:  return "Name or service not known";
    case EAI_AGAIN:   return "Temporary DNS failure";
    case EAI_FAIL:    return "DNS failure";
    case EAI_MEMORY:  return "Out of memory";
    case EAI_FAMILY:  return "Address family not supported";
    case EAI_SERVICE: return "Service not supported";
    default:          return "Unknown error";
    }
}

static inline void freeaddrinfo(struct addrinfo *ai)
{
    while (ai) {
        struct addrinfo *n = ai->ai_next;
        free(ai->ai_addr);
        free(ai->ai_canonname);
        free(ai);
        ai = n;
    }
}

static inline int
getaddrinfo(const char *node, const char *service,
            const struct addrinfo *hints, struct addrinfo **res)
{
    struct sockaddr_in *sa;
    struct addrinfo    *ai;
    unsigned short      port = 0;

    if (service) {
        char *end;
        port = (unsigned short)strtoul(service, &end, 10);
    }

    ai = (struct addrinfo *)calloc(1, sizeof(*ai));
    sa = (struct sockaddr_in *)calloc(1, sizeof(*sa));
    if (!ai || !sa) { free(ai); free(sa); return EAI_MEMORY; }

    if (!node || (hints && (hints->ai_flags & AI_PASSIVE))) {
        sa->sin_addr.s_addr = INADDR_ANY;
    } else {
        struct in_addr tmp;
        if (inet_aton(node, &tmp)) {
            sa->sin_addr = tmp;
        } else {
            struct hostent *he = net_gethostbyname(node);
            if (!he) { free(ai); free(sa); return EAI_NONAME; }
            sa->sin_addr = *(struct in_addr *)he->h_addr;
        }
    }

    sa->sin_family = AF_INET;
    sa->sin_port   = port;

    ai->ai_family   = AF_INET;
    ai->ai_socktype = hints ? hints->ai_socktype : SOCK_STREAM;
    ai->ai_protocol = hints ? hints->ai_protocol : 0;
    ai->ai_addrlen  = (socklen_t)sizeof(struct sockaddr_in);
    ai->ai_addr     = (struct sockaddr *)sa;
    ai->ai_next     = NULL;
    *res = ai;
    return 0;
}

#endif /* _ADDRINFO_H_INCLUDED */

#endif /* WII_BUILD */
#endif /* WII_SOCKET_COMPAT_H */
