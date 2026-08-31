#ifndef CURL_WII_SYS_SOCKET_H
#define CURL_WII_SYS_SOCKET_H

/* Forward to libogc's real sys/socket.h for struct sockaddr and friends.
   Avoids circular include (network.h → <sys/socket.h> → <network.h>). */
#include_next <sys/socket.h>

/* IPv6 address family — not in libogc2/network.h */
#ifndef AF_INET6
#define AF_INET6  28
#define PF_INET6  AF_INET6
#endif

#endif
