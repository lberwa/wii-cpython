/* netinet/in.h stub for Wii — all types provided by network.h */
#ifndef _NETINET_IN_H
#define _NETINET_IN_H
#include <network.h>
/* IPPROTO_* and AF_INET are defined in network.h */
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
