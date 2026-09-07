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

/* Bridge: newlib fd_set (64 slots) -> libogc ogc_fd_set (16 slots) */
typedef struct { uint8_t fd_bits[2]; } ogc_fd_set;

static int wii_select(int nfds, fd_set *rfds, fd_set *wfds, fd_set *efds,
                      struct timeval *tv)
{
    if (nfds > 16) { errno = EINVAL; return -1; }
    ogc_fd_set or = {{0}}, ow = {{0}}, oe = {{0}};
    for (int i = 0; i < nfds; i++) {
        if (rfds  && FD_ISSET(i, rfds))  or.fd_bits[i>>3] |= 1u << (i & 7);
        if (wfds  && FD_ISSET(i, wfds))  ow.fd_bits[i>>3] |= 1u << (i & 7);
        if (efds  && FD_ISSET(i, efds))  oe.fd_bits[i>>3] |= 1u << (i & 7);
    }
    int ret = net_select(nfds,
        rfds ? (void *)&or : NULL,
        wfds ? (void *)&ow : NULL,
        efds ? (void *)&oe : NULL, tv);
    if (ret <= 0) return ret;
    if (rfds) { FD_ZERO(rfds); for (int i=0;i<nfds;i++) if (or.fd_bits[i>>3] & (1u<<(i&7))) FD_SET(i,rfds); }
    if (wfds) { FD_ZERO(wfds); for (int i=0;i<nfds;i++) if (ow.fd_bits[i>>3] & (1u<<(i&7))) FD_SET(i,wfds); }
    if (efds) { FD_ZERO(efds); for (int i=0;i<nfds;i++) if (oe.fd_bits[i>>3] & (1u<<(i&7))) FD_SET(i,efds); }
    return ret;
}
#define select wii_select

/* poll Bridge: struct pollfd kommt bereits von poll.h (oben eingebunden) */
static int wii_poll(struct pollfd *fds, unsigned int nfds, int timeout) {
    struct pollsd mapped[32];
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
