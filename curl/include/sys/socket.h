#ifndef CURL_INCLUDE_SYS_SOCKET_H
#define CURL_INCLUDE_SYS_SOCKET_H

/* Forward to the real libogc sys/socket.h so struct sockaddr and friends
   are defined before any Wii-specific network.h is pulled in.
   #include_next skips this file and continues the search path (finds
   libogc/include/sys/socket.h at the next position). */
#include_next <sys/socket.h>

#endif
