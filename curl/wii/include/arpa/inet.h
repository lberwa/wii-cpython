/* arpa/inet.h stub for Wii — inet functions provided by network.h */
#ifndef _ARPA_INET_H
#define _ARPA_INET_H
#ifndef _POSIX_THREADS
#  define _POSIX_THREADS
#endif
#include <network.h>
#include <netinet/in.h>
/* inet_aton / inet_ntoa declared in network.h */
#ifndef INET_ADDRSTRLEN
#  define INET_ADDRSTRLEN 16
#endif
/* inet_ntop / inet_pton: libogc2 implements only AF_INET.
 * CPython provides its own IPv6 fallback when HAVE_INET_PTON is not defined. */
#ifndef _WII_INET_NP_DECL
#define _WII_INET_NP_DECL
#include <stddef.h>
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
int         inet_pton(int af, const char *src, void *dst);
#endif
#endif /* _ARPA_INET_H */
