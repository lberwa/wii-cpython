#ifndef CURL_WII_NET_COMPAT_H
#define CURL_WII_NET_COMPAT_H

/*
 * devkitPPC/libogc networking is exposed as net_* APIs in <network.h>.
 * Map the BSD names curl expects to those APIs at compile time.
 */
#ifndef NETWORK_H22
#define NETWORK_H22 1
#endif
#include <unistd.h>
/* sys/features.h (via unistd.h) sets __BSD_VISIBLE=0 and __POSIX_VISIBLE=0
   when compiled with -std=c11 (no _DEFAULT_SOURCE / _POSIX_C_SOURCE defined).
   Restore both so libogc BSD/POSIX headers expose INET_ADDRSTRLEN, sockaddr_in6, etc. */
#undef __BSD_VISIBLE
#define __BSD_VISIBLE 1
#undef __POSIX_VISIBLE
#define __POSIX_VISIBLE 200112
#include <fcntl.h>
#include <errno.h>
#include <network.h>

/* libogc net_* expects host-order ports; keep byte-order conversions no-op. */
/* libogc net_* expects host-order ports; force htons/ntohs to no-op. */
#ifdef htons
#undef htons
#endif
#define htons(x) (x)
#ifdef ntohs
#undef ntohs
#endif
#define ntohs(x) (x)
#ifndef closesocket
#define closesocket net_close
#endif

/* Use libogc's struct pollfd from <poll.h> to avoid redefinition conflicts. */
#ifndef _SYS_POLL_H_
#include <poll.h>
#endif
#ifndef CURL_WII_POLLFD_DEFINED
#define CURL_WII_POLLFD_DEFINED
#endif

static int curl_wii_poll(struct pollfd *fds, unsigned int nfds, int timeout)
{
  unsigned int i;
  struct pollsd mapped[32];
  if(nfds > 32u)
    return -1;
  for(i = 0; i < nfds; ++i) {
    mapped[i].socket = (s32)fds[i].fd;
    mapped[i].events = (u32)fds[i].events;
    mapped[i].revents = 0;
  }
  int rc = net_poll(mapped, (s32)nfds, (s32)timeout);
  if(rc < 0) { errno = -rc; return -1; }   /* libogc: negated errno */
  /* POSIX poll() returns the NUMBER of fds with non-zero revents (0 on
     timeout), NOT just 0/-1.  curl is built with -DHAVE_POLL=1 (see
     build-wii/Makefile CURL_WII_COMMON_CFLAGS), so Curl_poll() takes its
     poll() branch and does `r = poll(...); if(r <= 0) return r;` -- returning 0
     unconditionally here made curl treat EVERY wait as a timeout, so no
     connection ever completed ("Connection timed out after N milliseconds").
     Returning the ready count is what lets connect/transfer proceed.
     (socketmodule.c does NOT use this: HAVE_POLL is undef in pyconfig, so its
     internal_select uses select()=net_select instead -- pip is unaffected.) */
  int ready = 0;
  for(i = 0; i < nfds; ++i) {
    fds[i].revents = (short)mapped[i].revents;
    if(mapped[i].revents)
      ready++;
  }
  return ready;
}

#define poll curl_wii_poll

static int curl_wii_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
  u32 len = addrlen ? (u32)(*addrlen) : 0u;
  int rc = net_accept((s32)s, addr, &len);
  if(addrlen)
    *addrlen = (socklen_t)len;
  return rc;
}

static int curl_wii_getsockopt(int s, int level, int optname,
                               void *optval, socklen_t *optlen)
{
  (void)s;
  (void)level;
  (void)optname;
  /* libogc headers declare net_getsockopt, but some builds do not export it.
   * Minimal fallback for curl's SO_ERROR probes: report "no pending error". */
  if(optval && optlen && *optlen >= (socklen_t)sizeof(int)) {
    *(int *)optval = 0;
    *optlen = (socklen_t)sizeof(int);
    return 0;
  }
  return -1;
}

static int curl_wii_socket(int domain, int type, int protocol)
{
  /* libogc net_socket expects protocol 0 for TCP/UDP */
  if(protocol == IPPROTO_TCP || protocol == IPPROTO_UDP)
    protocol = 0;
  int rc = net_socket((u32)domain, (u32)type, (u32)protocol);
  if(rc < 0) {
    errno = -rc;
    return -1;
  }
  return rc;
}

static int curl_wii_connect(int s, const struct sockaddr *name,
                            socklen_t namelen)
{
  int rc = net_connect((s32)s, (struct sockaddr *)name, namelen);
  if(rc < 0) {
    errno = -rc;
    return -1;
  }
  return rc;
}

/* net_* data-transfer calls have two Wii-isms that POSIX callers (CPython's
   socketmodule, libcurl) don't expect:

   1) errno convention: on failure they RETURN the negated errno and do NOT set
      errno.  Mapping them raw (#define recv net_recv) made the caller read a
      STALE errno -- e.g. the EINPROGRESS (119) left over from connect() -- so a
      normal EWOULDBLOCK on a not-yet-ready recv looked fatal (pip: "Connection
      aborted, BlockingIOError(119)").  We translate like curl_wii_connect().

   2) buffer size: net_recvfrom()/net_sendto() do net_malloc(len) from the 64 KB
      NET_HEAP_SIZE pool (libogc network_wii.c).  A len near/over 64 KB fails
      with EINVAL (seen with recv_into(bytearray(65537)) via http.client).  We
      clamp each transfer to WII_NET_MAX_XFER, well under the shared heap so
      concurrent send+recv still fit.  recv()/send() may transfer fewer bytes
      than requested per POSIX, so the caller just loops -- no data is lost. */
#define WII_NET_MAX_XFER (16 * 1024)

static ssize_t curl_wii_recv(int s, void *buf, size_t len, int flags)
{
  if(len > WII_NET_MAX_XFER) len = WII_NET_MAX_XFER;
  s32 rc = net_recv((s32)s, buf, len, (u32)flags);
  if(rc < 0) { errno = -rc; return -1; }
  return rc;
}
static ssize_t curl_wii_send(int s, const void *buf, size_t len, int flags)
{
  if(len > WII_NET_MAX_XFER) len = WII_NET_MAX_XFER;
  s32 rc = net_send((s32)s, buf, len, (u32)flags);
  if(rc < 0) { errno = -rc; return -1; }
  return rc;
}
static ssize_t curl_wii_recvfrom(int s, void *buf, size_t len, int flags,
                                 struct sockaddr *from, socklen_t *fromlen)
{
  if(len > WII_NET_MAX_XFER) len = WII_NET_MAX_XFER;
  s32 rc = net_recvfrom((s32)s, buf, len, (u32)flags, from, fromlen);
  if(rc < 0) { errno = -rc; return -1; }
  return rc;
}
static ssize_t curl_wii_sendto(int s, const void *buf, size_t len, int flags,
                               const struct sockaddr *to, socklen_t tolen)
{
  if(len > WII_NET_MAX_XFER) len = WII_NET_MAX_XFER;
  s32 rc = net_sendto((s32)s, buf, len, (u32)flags, (struct sockaddr *)to, tolen);
  if(rc < 0) { errno = -rc; return -1; }
  return rc;
}
static ssize_t curl_wii_read(int s, void *buf, size_t len)
{
  if(len > WII_NET_MAX_XFER) len = WII_NET_MAX_XFER;
  s32 rc = net_read((s32)s, buf, len);
  if(rc < 0) { errno = -rc; return -1; }
  return rc;
}
static ssize_t curl_wii_write(int s, const void *buf, size_t len)
{
  if(len > WII_NET_MAX_XFER) len = WII_NET_MAX_XFER;
  s32 rc = net_write((s32)s, buf, len);
  if(rc < 0) { errno = -rc; return -1; }
  return rc;
}

/* bind/listen/getsockname/shutdown also use the libogc -errno convention and
   were mapped raw -- same stale-errno bug (socket.bind() reported OSError(0)).
   Wrap them too. */
static int curl_wii_bind(int s, const struct sockaddr *name, socklen_t namelen)
{
  int rc = net_bind((s32)s, (struct sockaddr *)name, namelen);
  if(rc < 0) { errno = -rc; return -1; }
  return rc;
}
static int curl_wii_listen(int s, int backlog)
{
  int rc = net_listen((s32)s, backlog);
  if(rc < 0) { errno = -rc; return -1; }
  return rc;
}
static int curl_wii_getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
  int rc = net_getsockname((s32)s, name, namelen);
  if(rc < 0) { errno = -rc; return -1; }
  return rc;
}
static int curl_wii_shutdown(int s, int how)
{
  int rc = net_shutdown((s32)s, how);
  if(rc < 0) { errno = -rc; return -1; }
  return rc;
}

#define socket      curl_wii_socket
#define bind        curl_wii_bind
#define listen      curl_wii_listen
#define accept      curl_wii_accept
#define connect     curl_wii_connect
#define send        curl_wii_send
#define sendto      curl_wii_sendto
#define recv        curl_wii_recv
#define recvfrom    curl_wii_recvfrom
#define read        curl_wii_read
#define write       curl_wii_write
#define close       net_close
#define select      net_select
#define getsockopt  curl_wii_getsockopt
/* setsockopt -> tolerant wrapper (Modules/wii_socket_stubs.c): libogc rejects
   options like TCP_NODELAY which urllib3/pip set, breaking connections.  The
   wrapper ignores failures for unsupported options. */
extern int wii_setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen);
#define setsockopt  wii_setsockopt
#define getsockname curl_wii_getsockname
#define shutdown    curl_wii_shutdown
/* libcurl may use ioctlsocket(FIONBIO) to enable non-blocking I/O. libogc
   net_* doesn't reliably support it; ignore and keep blocking. */
static int curl_wii_ioctlsocket(int s, long cmd, void *argp)
{
  if(cmd == FIONBIO) {
    return 0;
  }
  return net_ioctl((s32)s, cmd, argp);
}

#define ioctlsocket curl_wii_ioctlsocket
/* libcurl tries to use non-blocking sockets; libogc net_* doesn't reliably
   support that. Keep sockets blocking by ignoring O_NONBLOCK requests. */
static int curl_wii_fcntl(int s, int cmd, int arg)
{
  /* Ignore FD_CLOEXEC and non-blocking flags on Wii/libogc. */
  if(cmd == F_SETFD || cmd == F_GETFD)
    return 0;
  if(cmd == F_SETFL) {
    if(arg & O_NONBLOCK)
      return 0;
  }
  if(cmd == F_GETFL)
    return 0;
  return net_fcntl((s32)s, cmd, arg);
}

#define fcntl       curl_wii_fcntl
#define gethostbyname net_gethostbyname

/* --- Extra bits CPython's socketmodule.c needs once HAVE_* are enabled --- */
/* libogc has no listen-backlog limit constant. */
#ifndef SOMAXCONN
#define SOMAXCONN 128
#endif
/* getpeername has no net_* equivalent (none exists in libogc1/2); socketpair is
 * only declared by libogc1.  Both are implemented in Modules/wii_socket_stubs.c
 * (getpeername -> loopback stub, socketpair -> TCP-loopback emulation).  Declare
 * them so socketmodule.c compiles; the linker resolves them from the stub .o.
 * On libogc1 <network.h> macro-maps getpeername -> lwip_getpeername, so this
 * prototype harmlessly declares that symbol instead (also provided by the stub). */
int getpeername(int s, struct sockaddr *name, socklen_t *namelen);
#ifndef socketpair
int socketpair(int domain, int type, int protocol, int sv[2]);
#endif

#endif
