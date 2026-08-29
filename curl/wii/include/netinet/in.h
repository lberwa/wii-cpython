#ifndef _NETINET_IN_H
#define _NETINET_IN_H

/* Forward to libogc's real netinet/in.h for struct in_addr, AF_INET, etc.
   Avoids circular include (network.h → <netinet/in.h> → <network.h>). */
#include_next <netinet/in.h>

#ifndef IPPROTO_IP
#  define IPPROTO_IP   0
#endif
#ifndef IPPROTO_TCP
#  define IPPROTO_TCP  6
#endif
#ifndef IPPROTO_UDP
#  define IPPROTO_UDP 17
#endif

#endif /* _NETINET_IN_H */
