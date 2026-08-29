#ifndef CURL_WII_SYS_SOCKET_H
#define CURL_WII_SYS_SOCKET_H

#ifndef NETWORK_H22
#define NETWORK_H22 1
#endif
#include <network.h>

/* IPv6 address family — not in libogc2/network.h */
#ifndef AF_INET6
#define AF_INET6  28
#define PF_INET6  AF_INET6
#endif

#endif
