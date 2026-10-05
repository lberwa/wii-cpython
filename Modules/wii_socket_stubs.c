/* Stubs for socket functions declared in libogc headers but not implemented. */
#ifdef WII_BUILD

#include <errno.h>
#include <stdarg.h>     /* va_list for ioctl(...) */
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
/* libogc2 also lacks struct iovec / struct msghdr (libogc1 has them in
 * sys/_iovec.h and sys/socket.h).  Define them with the same layout so the
 * sendmsg()/recvmsg() emulation below compiles. */
struct iovec {
    void  *iov_base;
    size_t iov_len;
};
struct msghdr {
    void         *msg_name;
    socklen_t     msg_namelen;
    struct iovec *msg_iov;
    int           msg_iovlen;
    void         *msg_control;
    socklen_t     msg_controllen;
    int           msg_flags;
};
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

/* sendmsg: libogc has no net_sendmsg. Emulate by sending each iovec with
   net_send (no ancillary/control data support — none needed for TCP HTTP). */
ssize_t sendmsg(int fd, const struct msghdr *msg, int flags)
{
    if (!msg) { errno = EFAULT; return -1; }
    ssize_t total = 0;
    for (size_t i = 0; i < (size_t)msg->msg_iovlen; i++) {
        const struct iovec *iov = &msg->msg_iov[i];
        if (iov->iov_len == 0) continue;
        s32 n = net_send(fd, iov->iov_base, (s32)iov->iov_len, (u32)flags);
        if (n < 0) { if (total > 0) break; errno = -n; return -1; }
        total += n;
        if ((size_t)n < iov->iov_len) break;   /* short write, stop */
    }
    return total;
}

/* recvmsg: libogc has no net_recvmsg. Emulate by receiving into each iovec
   with net_recv (no ancillary/control data). */
ssize_t recvmsg(int fd, struct msghdr *msg, int flags)
{
    if (!msg) { errno = EFAULT; return -1; }
    ssize_t total = 0;
    for (size_t i = 0; i < (size_t)msg->msg_iovlen; i++) {
        struct iovec *iov = &msg->msg_iov[i];
        if (iov->iov_len == 0) continue;
        s32 n = net_recv(fd, iov->iov_base, (s32)iov->iov_len, (u32)flags);
        if (n < 0) { if (total > 0) break; errno = -n; return -1; }
        total += n;
        if ((size_t)n < iov->iov_len) break;   /* short read, stop */
    }
    msg->msg_flags = 0;
    return total;
}

/* setsockopt wrapper: libogc's net_setsockopt supports only a subset of options
   (mainly SOL_SOCKET).  urllib3/pip set IPPROTO_TCP/TCP_NODELAY, SO_KEEPALIVE,
   etc.; if net_setsockopt rejects one, the whole connection setup aborts with a
   bogus errno ("Function not implemented").  Treat failures as success so these
   advisory options don't break higher-level code; supported options (e.g.
   SO_REUSEADDR) still take effect. */
int wii_setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen)
{
    s32 r = net_setsockopt(s, (u32)level, (u32)optname, optval, optlen);
    return r < 0 ? 0 : r;
}

/* socketpair: AF_UNIX does not work on Wii IOS. Emulate with TCP loopback.
   asyncio uses socketpair() for its self-pipe wakeup mechanism. */
int socketpair(int domain, int type, int protocol, int sv[2])
{
    (void)protocol;
    /* Strip SOCK_CLOEXEC / SOCK_NONBLOCK flag bits: CPython's socket_socketpair
       calls socketpair(family, type | SOCK_CLOEXEC, ...).  We only support
       stream sockets; the flags are harmless on Wii (no exec, blocking handled
       by the caller via setblocking). */
    int base = type;
#ifdef SOCK_CLOEXEC
    base &= ~SOCK_CLOEXEC;
#endif
#ifdef SOCK_NONBLOCK
    base &= ~SOCK_NONBLOCK;
#endif
    /* Accept AF_UNIX as well — redirect to AF_INET TCP loopback. */
    if (base != SOCK_STREAM) { errno = EPROTONOSUPPORT; return -1; }
    (void)domain;

    int listener = net_socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) { errno = -listener; return -1; }

    /* Bind the listener to INADDR_ANY (0.0.0.0): the Wii IOS/lwIP stack has no
       usable 127.0.0.1 loopback interface, so binding/connecting to 127.0.0.1
       fails.  Binding to ANY and connecting to the device's own IP keeps the
       whole exchange on the real interface, which does loop back.

       NOTE 1: libogc2's lwIP validates sockaddr_in.sin_len; a zeroed sin_len
       (from memset) makes net_bind reject the address.  Set it explicitly.
       NOTE 2: IOS/libogc net_bind() rejects port 0 (auto-select) with EINVAL,
       so we can't let the OS pick a port.  Try an explicit range of ephemeral
       ports until one binds free.  net_* return the negated errno on failure
       (libogc convention) -- pass that through so failures are diagnosable. */
    struct sockaddr_in addr;
    s32 r = -1;
    for (unsigned p = 49152; p <= 49152u + 256u; p++) {
        memset(&addr, 0, sizeof(addr));
        addr.sin_len         = sizeof(addr);
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(0x00000000UL); /* INADDR_ANY */
        addr.sin_port        = htons((u16)p);       /* explicit port (0 -> EINVAL) */
        r = net_bind(listener, (struct sockaddr *)&addr, sizeof(addr));
        if (r >= 0) break;
    }
    if (r < 0) {
        net_close(listener); errno = (r < 0 ? -r : EADDRINUSE); return -1;
    }
    socklen_t alen = sizeof(addr);
    r = net_getsockname(listener, (struct sockaddr *)&addr, &alen);
    if (r < 0) {
        net_close(listener); errno = (r < 0 ? -r : ENOTSOCK); return -1;
    }
    r = net_listen(listener, 1);
    if (r < 0) {
        net_close(listener); errno = (r < 0 ? -r : EOPNOTSUPP); return -1;
    }

    /* Connect to the device's own IP (net_gethostip) at the bound port.
       getsockname may have clobbered sin_len/sin_family -- restore them. */
    u32 hostip = net_gethostip();
    addr.sin_len    = sizeof(addr);
    addr.sin_family = AF_INET;
    if (hostip != 0) {
        addr.sin_addr.s_addr = hostip;
    } else {
        addr.sin_addr.s_addr = htonl(0x7f000001UL); /* fallback: 127.0.0.1 */
    }

    int client = net_socket(AF_INET, SOCK_STREAM, 0);
    if (client < 0) { net_close(listener); errno = -client; return -1; }

    r = net_connect(client, (struct sockaddr *)&addr, sizeof(addr));
    if (r < 0) {
        net_close(listener); net_close(client);
        errno = (r < 0 ? -r : ECONNREFUSED); return -1;
    }

    struct sockaddr_in peer;
    socklen_t plen = sizeof(peer);
    int server = net_accept(listener, (struct sockaddr *)&peer, &plen);
    net_close(listener);
    if (server < 0) { net_close(client); errno = -server; return -1; }

    sv[0] = server;  /* server end (ssock in asyncio) */
    sv[1] = client;  /* client end (csock in asyncio) */
    return 0;
}

/* getpeername: IOS IOCTL_SO_GETPEERNAME is TODO in libogc; lwip_getpeername
   is declared in libogc1 headers but not present in libogc.a.
   socketmodule.c is compiled without libogc headers so the macro
   getpeername->lwip_getpeername never fires there; the linker needs the
   plain symbol 'getpeername'. We provide it here (undef any header macro
   first), plus 'lwip_getpeername' for code compiled WITH the libogc1 headers.
   Stub returns 127.0.0.1 — correct for our socketpair() loopback connections. */
static int _wii_getpeername(int fd, struct sockaddr *addr, socklen_t *addrlen)
{
    (void)fd;
    if (!addr || !addrlen) { errno = EFAULT; return -1; }
    if (*addrlen < (socklen_t)sizeof(struct sockaddr_in)) { errno = ENOBUFS; return -1; }
    struct sockaddr_in *sin = (struct sockaddr_in *)addr;
    memset(sin, 0, sizeof(*sin));
    sin->sin_family      = AF_INET;
    sin->sin_addr.s_addr = htonl(0x7f000001UL); /* 127.0.0.1 loopback */
    sin->sin_port        = 0;
    *addrlen = sizeof(*sin);
    return 0;
}

/* Export plain 'getpeername' for callers compiled without libogc headers. */
#ifdef getpeername
#undef getpeername
#endif
int getpeername(int fd, struct sockaddr *addr, socklen_t *addrlen)
    { return _wii_getpeername(fd, addr, addrlen); }

#if WII_LIBOGC != 2
/* libogc1: also export 'lwip_getpeername' for callers compiled WITH the
   libogc1 header that expands getpeername -> lwip_getpeername. */
int lwip_getpeername(int fd, struct sockaddr *name, socklen_t *namelen)
    { return _wii_getpeername(fd, name, namelen); }
#endif

/* select -> libogc net_select (real implementation). */
int select(int nfds, fd_set *r, fd_set *w, fd_set *e, struct timeval *t)
{
    s32 ret = net_select(nfds, r, w, e, t);
    if (ret < 0) { errno = -ret; return -1; }
    return ret;
}

/* ioctl -> libogc net_ioctl. Handles FIONBIO (non-blocking) which pip/urllib3
   needs when it sets a socket timeout (internal_setblocking -> ioctl FIONBIO). */
int ioctl(int fd, int req, ...)
{
    va_list ap;
    va_start(ap, req);
    void *argp = va_arg(ap, void *);
    va_end(ap);
    s32 ret = net_ioctl(fd, (u32)req, argp);
    if (ret < 0) { errno = -ret; return -1; }
    return ret;
}

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
