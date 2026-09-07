/* netinet/in.h stub fuer Wii */
#ifndef _NETINET_IN_H
#define _NETINET_IN_H

#if WII_LIBOGC == 2
/* libogc2 hat kein netinet/in.h — Socket-Typen kommen aus network.h */
#include <network.h>

#ifndef IPPROTO_IP
#  define IPPROTO_IP   0
#endif
#ifndef IPPROTO_TCP
#  define IPPROTO_TCP  6
#endif
#ifndef IPPROTO_UDP
#  define IPPROTO_UDP 17
#endif

/* IPv6 extensions */
#ifndef IPPROTO_IPV6
#define IPPROTO_IPV6   41
#endif
#ifndef IPV6_V6ONLY
#define IPV6_V6ONLY    27
#endif
#ifndef IPV6_UNICAST_HOPS
#define IPV6_UNICAST_HOPS 4
#endif
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

#ifndef _STRUCT_IN6_ADDR
#define _STRUCT_IN6_ADDR
struct in6_addr {
    union {
        uint8_t  s6_addr[16];
        uint16_t s6_addr16[8];
        uint32_t s6_addr32[4];
    };
};
extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;
#define IN6ADDR_ANY_INIT      { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } } }
#define IN6ADDR_LOOPBACK_INIT { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 } } }
#endif

#ifndef _STRUCT_SOCKADDR_IN6
#define _STRUCT_SOCKADDR_IN6
struct sockaddr_in6 {
    uint8_t         sin6_len;
    uint8_t         sin6_family;
    uint16_t        sin6_port;
    uint32_t        sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t        sin6_scope_id;
};
#endif

#define IN6_IS_ADDR_UNSPECIFIED(a) \
    (((const uint32_t *)(a))[0] == 0 && \
     ((const uint32_t *)(a))[1] == 0 && \
     ((const uint32_t *)(a))[2] == 0 && \
     ((const uint32_t *)(a))[3] == 0)
#define IN6_IS_ADDR_LOOPBACK(a) \
    (((const uint32_t *)(a))[0] == 0 && \
     ((const uint32_t *)(a))[1] == 0 && \
     ((const uint32_t *)(a))[2] == 0 && \
     ((const uint32_t *)(a))[3] == htonl(1))

#else /* WII_LIBOGC == 1 */
/* libogc hat ein vollstaendiges netinet/in.h */
#include_next <netinet/in.h>

#endif /* WII_LIBOGC */

#endif /* _NETINET_IN_H */
