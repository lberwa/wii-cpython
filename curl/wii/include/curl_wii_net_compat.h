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

#ifndef CURL_WII_POLLFD_DEFINED
#define CURL_WII_POLLFD_DEFINED
struct pollfd {
  int fd;
  short events;
  short revents;
};
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
  if(net_poll(mapped, (s32)nfds, (s32)timeout) < 0)
    return -1;
  for(i = 0; i < nfds; ++i)
    fds[i].revents = (short)mapped[i].revents;
  return 0;
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

#define socket      curl_wii_socket
#define bind        net_bind
#define listen      net_listen
#define accept      curl_wii_accept
#define connect     curl_wii_connect
#define send        net_send
#define sendto      net_sendto
#define recv        net_recv
#define recvfrom    net_recvfrom
#define read        net_read
#define write       net_write
#define close       net_close
#define select      net_select
#define getsockopt  curl_wii_getsockopt
#define setsockopt  net_setsockopt
#define getsockname net_getsockname
#define shutdown    net_shutdown
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

#endif
