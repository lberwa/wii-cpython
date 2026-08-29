#ifndef _ARPA_INET_H
#define _ARPA_INET_H

/* Forward to libogc's real arpa/inet.h for inet_aton, inet_ntoa, etc.
   Avoids circular include (network.h → <arpa/inet.h> → <network.h>). */
#ifndef _POSIX_THREADS
#  define _POSIX_THREADS
#endif
#include_next <arpa/inet.h>

#ifndef INET_ADDRSTRLEN
#  define INET_ADDRSTRLEN 16
#endif

#endif /* _ARPA_INET_H */
