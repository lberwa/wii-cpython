/* Behebt Tippfehler fd_bfits -> fd_bits in libogc/network.h FD_ISSET */
#ifdef FD_ISSET
#undef FD_ISSET
#endif
#define FD_ISSET(n,p) ((p)->fd_bits[(n)/8] & (1 << ((n) & 7)))
