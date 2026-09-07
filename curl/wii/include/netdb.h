#ifndef _WII_NETDB_H
#define _WII_NETDB_H

#if WII_LIBOGC == 1
/* sys/socket.h-Stub hat bereits socklen_t-Makro entfernt und die typedef via
   libogc's socket.h angelegt. Einfach durchleiten.
 * __thread wird temporaer weggefiltert: PPC-EABI unterstuetzt kein TLS,
 * und "extern __thread int h_errno" in libogc's netdb.h wuerde den
 * Linker-Fehler "section .tbss.h_errno mismatches non-TLS reference" ausloesen
 * wenn h_errno als regulaere int-Variable definiert ist. */
#define __thread
#include_next <netdb.h>
#undef __thread

#else /* WII_LIBOGC == 2 */
/* libogc2 hat kein netdb.h — minimale Deklarationen fuer socketmodule.c */
#include <network.h>  /* struct hostent, net_gethostbyname */
#include <netinet/in.h>

struct servent  { const char *s_name; char **s_aliases; int s_port; const char *s_proto; };
struct protoent { const char *p_name; char **p_aliases; int p_proto; };

#ifndef HOST_NOT_FOUND
#define HOST_NOT_FOUND 1
#endif
#ifndef NO_DATA
#define NO_DATA 4
#endif
#ifndef TRY_AGAIN
#define TRY_AGAIN 2
#endif
#ifndef NO_RECOVERY
#define NO_RECOVERY 3
#endif

/* EAI_* Fehlercodes */
#ifndef EAI_BADFLAGS
#define EAI_BADFLAGS  -1
#define EAI_NONAME    -2
#define EAI_AGAIN     -3
#define EAI_FAIL      -4
#define EAI_FAMILY    -6
#define EAI_SOCKTYPE  -7
#define EAI_SERVICE   -8
#define EAI_MEMORY    -10
#define EAI_OVERFLOW  -12
#endif

/* AI_* Flags fuer getaddrinfo() */
#ifndef AI_PASSIVE
#define AI_PASSIVE      0x00000001
#define AI_CANONNAME    0x00000002
#define AI_NUMERICHOST  0x00000004
#define AI_NUMERICSERV  0x00000008
#define AI_ALL          0x00000100
#define AI_ADDRCONFIG   0x00000400
#define AI_V4MAPPED     0x00000800
#endif

/* NI_* Flags fuer getnameinfo() */
#ifndef NI_NUMERICHOST
#define NI_NUMERICHOST  0x00000001
#define NI_NUMERICSERV  0x00000002
#define NI_NOFQDN       0x00000004
#define NI_NAMEREQD     0x00000008
#define NI_DGRAM        0x00000010
#define NI_MAXHOST      1025
#define NI_MAXSERV      32
#endif

/* struct addrinfo */
#ifndef _STRUCT_ADDRINFO
#define _STRUCT_ADDRINFO
struct addrinfo {
    int              ai_flags;
    int              ai_family;
    int              ai_socktype;
    int              ai_protocol;
    socklen_t        ai_addrlen;
    struct sockaddr *ai_addr;
    char            *ai_canonname;
    struct addrinfo *ai_next;
};
#endif

struct servent  *getservbyname(const char *name, const char *proto);
struct servent  *getservbyport(int port, const char *proto);
struct protoent *getprotobyname(const char *name);
int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *ai);
int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags);
const char *gai_strerror(int ecode);

#endif /* WII_LIBOGC */

#endif /* _WII_NETDB_H */
