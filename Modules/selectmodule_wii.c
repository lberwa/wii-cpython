/* Wii-Wrapper fuer selectmodule.c
 *
 * Problem: newlib (Python.h) definiert fd_set mit FD_SETSIZE=64.
 * libogc's net_select() erwartet eine eigene fd_set mit FD_SETSIZE=16.
 * Direkte Kompatibilitaet ist nicht moeglich -> Bridge-Funktionen.
 */

/* Python.h zuerst - definiert newlib fd_set/FD_* (FD_SETSIZE=64) */
#include "Python.h"

/* NETWORK_H22 aktiviert net_select/net_poll in libogc's network.h */
#ifndef NETWORK_H22
#define NETWORK_H22 1
#endif
#include <network.h>

/* poll.h vorab einbinden, BEVOR selectmodule.c es via HAVE_POLL_H erneut
 * einbindet und BEVOR "#define poll wii_poll" gesetzt wird.
 *
 * Warum: libogc1 hat ein echtes <poll.h> das struct pollfd und int poll()
 * deklariert. Wuerde selectmodule.c's #include <poll.h> nach dem
 * "#define poll wii_poll" eingebunden, wuerde der Praeprozessor
 * "int poll(...)" zu "int wii_poll(...)" expandieren -> Typkonflikt mit
 * der bereits definierten "static int wii_poll(...)".
 * Durch fruehes Einbinden ist der Include-Guard (_SYS_POLL_H_ oder
 * _WII_POLL_H) gesetzt, sodass der spaetere Include ein No-Op ist. */
#if defined(HAVE_POLL_H)
#  include <poll.h>
#elif defined(HAVE_SYS_POLL_H)
#  include <sys/poll.h>
#endif

#include <unistd.h>   /* usleep() for the empty-set timeout case */

/* select() bridge implemented on top of net_poll().
 *
 * Why not net_select(): on libogc2 net_select() rejects every call -- it
 * returns a negated errno (< 0) without touching any fd_set, so the Python
 * select module saw OSError(0, 'Error').  net_poll() on the other hand works
 * on any socket fd; socketmodule.c's internal_select() already prefers poll()
 * over select() for exactly this reason (and the bundled wii_poll() below is
 * the proven path).  So we translate the fd_sets to a pollsd array, call
 * net_poll(), and translate the revents back.
 *
 * Bonus: this drops the old FD_SETSIZE=16 limit of the net_select bridge;
 * net_poll() is not bound to a 16-slot ogc_fd_set. */
static int wii_select(int nfds, fd_set *rfds, fd_set *wfds, fd_set *efds,
                      struct timeval *tv)
{
    if (nfds < 0 || nfds > FD_SETSIZE) { errno = EINVAL; return -1; }

    struct pollsd psd[FD_SETSIZE];
    int slot[FD_SETSIZE];   /* fd -> index into psd[], or -1 if not watched */
    int n = 0;

    for (int fd = 0; fd < nfds; fd++) {
        u32 ev = 0;
        if (rfds && FD_ISSET(fd, rfds)) ev |= POLLIN;
        if (wfds && FD_ISSET(fd, wfds)) ev |= POLLOUT;
        if (efds && FD_ISSET(fd, efds)) ev |= POLLPRI;
        slot[fd] = -1;
        if (!ev) continue;
        psd[n].socket  = fd;
        psd[n].events  = ev;
        psd[n].revents = 0;
        slot[fd] = n;
        n++;
    }

    /* net_poll() timeout is in milliseconds; < 0 means wait forever. */
    int ms;
    if (tv == NULL) {
        ms = -1;
    } else {
        long long msec = (long long)tv->tv_sec * 1000
                       + (tv->tv_usec + 999) / 1000;   /* round us up to ms */
        ms = (msec > 0x7fffffffLL) ? 0x7fffffff : (int)msec;
    }

    if (n == 0) {
        /* No fds to watch: net_poll() rejects nfds==0, so emulate select()'s
           pure-timeout behaviour with a sleep. */
        if (ms > 0) usleep((useconds_t)ms * 1000);
        if (rfds) FD_ZERO(rfds);
        if (wfds) FD_ZERO(wfds);
        if (efds) FD_ZERO(efds);
        return 0;
    }

    int ret = net_poll(psd, n, ms);
    if (ret < 0) { errno = -ret; return -1; }   /* libogc: negated errno */

    /* Rebuild the fd_sets from revents.  Standard select()-over-poll mapping:
       read  = POLLIN|POLLHUP|POLLERR, write = POLLOUT|POLLERR, except = POLLPRI.
       select() counts every set bit, so a fd ready in two sets counts twice. */
    if (rfds) FD_ZERO(rfds);
    if (wfds) FD_ZERO(wfds);
    if (efds) FD_ZERO(efds);

    int count = 0;
    for (int fd = 0; fd < nfds; fd++) {
        int k = slot[fd];
        if (k < 0) continue;
        u32 re = (u32)psd[k].revents;
        if (rfds && (re & (POLLIN | POLLHUP | POLLERR))) { FD_SET(fd, rfds); count++; }
        if (wfds && (re & (POLLOUT | POLLERR)))          { FD_SET(fd, wfds); count++; }
        if (efds && (re & POLLPRI))                      { FD_SET(fd, efds); count++; }
    }
    return count;
}
#define select wii_select

/* poll Bridge: struct pollfd kommt bereits von poll.h (oben eingebunden) */
static int wii_poll(struct pollfd *fds, unsigned int nfds, int timeout) {
    struct pollsd mapped[32];
    if (nfds == 0) return 0;   /* IOS rejects nfds=0; empty set has no events */
    if (nfds > 32u) return -1;
    for (unsigned int i = 0; i < nfds; i++) {
        mapped[i].socket  = (s32)fds[i].fd;
        mapped[i].events  = (u32)fds[i].events;
        mapped[i].revents = 0;
    }
    if (net_poll(mapped, (s32)nfds, (s32)timeout) < 0) return -1;
    for (unsigned int i = 0; i < nfds; i++)
        fds[i].revents = (short)mapped[i].revents;
    return 0;
}
#define poll wii_poll

#include "selectmodule.c"
