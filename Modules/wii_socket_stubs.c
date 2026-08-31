/* Stubs for socket functions declared in libogc headers but not implemented. */
#ifdef WII_BUILD

#include <errno.h>
#include <stddef.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>

/* libogc declares h_errno as __thread but never provides storage */
__thread int h_errno = 0;

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

#endif /* WII_BUILD */
