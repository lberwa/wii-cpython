/* Stubs for socket functions declared in libogc headers but not implemented. */
#ifdef WII_BUILD

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <network.h>    /* struct hostent, net_* API, inet_aton, net_gethostbyname */
#include <sys/socket.h> /* curl/wii/include stub: sockaddr_storage etc. */
/* netdb.h: fuer libogc2 unser Stub (struct addrinfo, EAI_*), fuer libogc1 das echte Header */
#include <netdb.h>

#if WII_LIBOGC == 2
/* libogc2 hat weder servent noch protoent */
struct servent  { const char *s_name; char **s_aliases; int s_port; const char *s_proto; };
struct protoent { const char *p_name; char **p_aliases; int p_proto; };
#endif
/* h_errno: PPC-EABI unterstuetzt kein TLS, daher immer non-TLS.
 * libogc1's netdb.h deklariert "extern __thread int h_errno" - dieses
 * __thread wird in curl/wii/include/netdb.h weggefiltert. */
int h_errno = 0;

struct servent *getservbyname(const char *name, const char *proto)
    { (void)name; (void)proto; return NULL; }

struct servent *getservbyport(int port, const char *proto)
    { (void)port; (void)proto; return NULL; }

struct hostent *gethostbyaddr(const void *addr, socklen_t len, int type)
    { (void)addr; (void)len; (void)type; h_errno = HOST_NOT_FOUND; return NULL; }

int getprotobyname_r(void) { return -1; }

struct protoent *getprotobyname(const char *name)
    { (void)name; return NULL; }

ssize_t sendmsg(int fd, const struct msghdr *msg, int flags)
    { (void)fd; (void)msg; (void)flags; errno = ENOSYS; return -1; }

ssize_t recvmsg(int fd, struct msghdr *msg, int flags)
    { (void)fd; (void)msg; (void)flags; errno = ENOSYS; return -1; }

int socketpair(int domain, int type, int protocol, int sv[2])
    { (void)domain; (void)type; (void)protocol; (void)sv; errno = ENOSYS; return -1; }

int select(int nfds, fd_set *r, fd_set *w, fd_set *e, struct timeval *t)
    { (void)nfds; (void)r; (void)w; (void)e; (void)t; errno = ENOSYS; return -1; }

int ioctl(int fd, int req, ...)
    { (void)fd; (void)req; errno = ENOSYS; return -1; }

#if WII_LIBOGC == 2
/* inet_ntop / inet_pton: libogc2 hat kein POSIX-Aequivalent; libogc1 hat sie in arpa/inet.h */
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    if (af == AF_INET) {
        const unsigned char *p = (const unsigned char *)src;
        snprintf(dst, (size_t)size, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
        return dst;
    }
    errno = EAFNOSUPPORT;
    return NULL;
}

int inet_pton(int af, const char *src, void *dst) {
    if (af == AF_INET)
        return inet_aton(src, (struct in_addr *)dst);
    errno = EAFNOSUPPORT;
    return -1;
}
#endif /* WII_LIBOGC == 2 */

/* ---------------------------------------------------------------------------
 * getaddrinfo / freeaddrinfo / getnameinfo
 * Minimale IPv4-Implementierung fuer Wii: verwendet net_gethostbyname fuer
 * Hostname-Aufloesung. Benoetigt weil weder libogc noch libogc2 diese POSIX-
 * Funktionen in der Bibliothek implementieren (nur in den Headern deklariert).
 * struct addrinfo kommt von <netdb.h> (oben), struct sockaddr_in von
 * <netinet/in.h> (via network.h fuer libogc2, oder libogc's netdb.h fuer libogc1).
 * --------------------------------------------------------------------------*/

static int wii_parse_service(const char *service) {
    if (!service || service[0] == '\0') return 0;
    char *end;
    long port = strtol(service, &end, 10);
    if (*end == '\0' && port >= 0 && port <= 65535) return (int)port;
    if (strcmp(service, "http")  == 0) return 80;
    if (strcmp(service, "https") == 0) return 443;
    if (strcmp(service, "ftp")   == 0) return 21;
    if (strcmp(service, "smtp")  == 0) return 25;
    if (strcmp(service, "ssh")   == 0) return 22;
    return -1;
}

/* struct addrinfo ist in netdb.h deklariert — muss vor getaddrinfo nutzbar sein */
int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res)
{
    if (!res) return EAI_BADFLAGS;
    *res = NULL;

    int port = wii_parse_service(service);
    if (port < 0) return EAI_SERVICE;

    struct in_addr addr;
    memset(&addr, 0, sizeof(addr));

    if (node == NULL || node[0] == '\0') {
        int passive = hints && (hints->ai_flags & AI_PASSIVE);
        addr.s_addr = passive ? 0 /* INADDR_ANY */ : htonl(0x7f000001) /* 127.0.0.1 */;
    } else if (!inet_aton(node, &addr)) {
        struct hostent *h = net_gethostbyname(node);
        if (!h || !h->h_addr_list || !h->h_addr_list[0]) {
            h_errno = HOST_NOT_FOUND;
            return EAI_NONAME;
        }
        memcpy(&addr, h->h_addr_list[0], sizeof(addr));
    }

    struct addrinfo *ai = (struct addrinfo *)calloc(1, sizeof(*ai));
    if (!ai) return EAI_MEMORY;
    struct sockaddr_in *sin = (struct sockaddr_in *)calloc(1, sizeof(*sin));
    if (!sin) { free(ai); return EAI_MEMORY; }

    sin->sin_family = AF_INET;
    sin->sin_port   = htons((unsigned short)port);
    sin->sin_addr   = addr;
    sin->sin_len    = (unsigned char)sizeof(*sin);

    ai->ai_family   = AF_INET;
    ai->ai_socktype = hints ? hints->ai_socktype : SOCK_STREAM;
    ai->ai_protocol = hints ? hints->ai_protocol : 0;
    ai->ai_addrlen  = sizeof(*sin);
    ai->ai_addr     = (struct sockaddr *)sin;
    ai->ai_next     = NULL;

    *res = ai;
    return 0;
}

void freeaddrinfo(struct addrinfo *ai) {
    while (ai) {
        struct addrinfo *next = ai->ai_next;
        free(ai->ai_addr);
        free(ai);
        ai = next;
    }
}

int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                char *host, socklen_t hostlen,
                char *serv, socklen_t servlen, int flags)
{
    (void)salen; (void)flags;
    if (!sa || sa->sa_family != AF_INET) return EAI_FAMILY;
    const struct sockaddr_in *sin = (const struct sockaddr_in *)sa;
    if (host && hostlen > 0) {
        const char *ip = inet_ntoa(sin->sin_addr);
        if (!ip) return EAI_FAIL;
        strncpy(host, ip, (size_t)hostlen - 1);
        host[hostlen - 1] = '\0';
    }
    if (serv && servlen > 0)
        snprintf(serv, (size_t)servlen, "%d", (int)ntohs(sin->sin_port));
    return 0;
}

/* EAI_* Fehlertexte */
const char *gai_strerror(int ecode) {
    switch (ecode) {
        case EAI_BADFLAGS:  return "Invalid value for ai_flags";
        case EAI_NONAME:    return "Name or service not known";
        case EAI_SERVICE:   return "Servname not supported for ai_socktype";
        case EAI_MEMORY:    return "Memory allocation failure";
        case EAI_FAMILY:    return "ai_family not supported";
        default:            return "Unknown error";
    }
}

#endif /* WII_BUILD */
