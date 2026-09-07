#ifndef _WII_POLL_H
#define _WII_POLL_H

/* Gemeinsamer poll-Stub fuer libogc und libogc2.
 * Verhindert, dass libogc's sys/poll.h struct pollfd und int poll() neu
 * definiert — was mit dem statischen wii_poll-Bridge in selectmodule_wii.c
 * und curl_wii_net_compat.h konfligieren wuerde. */

typedef unsigned int nfds_t;

#ifndef CURL_WII_POLLFD_DEFINED
#define CURL_WII_POLLFD_DEFINED
struct pollfd {
    int   fd;
    short events;
    short revents;
};
#endif

/* BSD poll-Konstanten — selbe Werte in libogc und libogc2 */
#ifndef POLLRDNORM
#define POLLRDNORM  0x0001
#endif
#ifndef POLLRDBAND
#define POLLRDBAND  0x0002
#endif
#ifndef POLLPRI
#define POLLPRI     0x0004
#endif
#ifndef POLLWRNORM
#define POLLWRNORM  0x0008
#endif
#ifndef POLLWRBAND
#define POLLWRBAND  0x0010
#endif
#ifndef POLLIN
#define POLLIN  (POLLRDNORM|POLLRDBAND)  /* 0x0003 */
#endif
#ifndef POLLOUT
#define POLLOUT POLLWRNORM               /* 0x0008 */
#endif
#ifndef POLLERR
#define POLLERR  0x0020
#endif
#ifndef POLLHUP
#define POLLHUP  0x0040
#endif
#ifndef POLLNVAL
#define POLLNVAL 0x0080
#endif

/* int poll() nur deklarieren wenn nicht schon per Makro umgeleitet
   (wii_poll/curl_wii_poll-Bridge macht eine externe Deklaration unnoetig
    und der statische Linker wuerde static-vs-extern-Konflikt melden). */
#ifndef poll
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
#endif

/* Verhindert, dass libogc's sys/poll.h spaeter struct pollfd/int poll()
   neu definiert und mit dem Bridge-Code konfligiert. */
#ifndef _SYS_POLL_H_
#define _SYS_POLL_H_
#endif

#endif /* _WII_POLL_H */
