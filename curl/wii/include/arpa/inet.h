#ifndef _ARPA_INET_H
#define _ARPA_INET_H

#if WII_LIBOGC == 2
/* libogc2 hat kein arpa/inet.h */
#include <network.h>
#include <netinet/in.h>
#ifndef INET_ADDRSTRLEN
#  define INET_ADDRSTRLEN 16
#endif
#ifndef _WII_INET_NP_DECL
#define _WII_INET_NP_DECL
#include <stddef.h>
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
int         inet_pton(int af, const char *src, void *dst);
#endif

#else /* WII_LIBOGC == 1 */
/* libogc hat ein vollstaendiges arpa/inet.h mit inet_ntoa/inet_aton/inet_ntop/inet_pton */
#include_next <arpa/inet.h>

#endif /* WII_LIBOGC */

#endif /* _ARPA_INET_H */
