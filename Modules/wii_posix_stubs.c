/* wii_posix_stubs.c — POSIX functions referenced by CPython / stdlib C modules
 * (signal, posix, fcntl, gdbm, _ctypes, ...) that newlib/libogc do NOT provide
 * on the Wii.  These live in libpython.a so every consumer of the library gets
 * them, instead of each embedding program having to re-define them.
 *
 * NOTE: ftruncate() and fsync() are intentionally NOT stubbed here — libsysbase
 * provides real (libfat-backed) implementations; stubbing them to no-ops would
 * shadow the working versions (and in fsync's case break flushing to the SD).
 */
#ifdef WII_BUILD

#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>

/* signal masking / handlers: no job control or real signals on Wii. */
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{ (void)how; (void)set; (void)oldset; return 0; }

int sigaction(int sig, const struct sigaction *act, struct sigaction *old)
{ (void)sig; (void)act; (void)old; return 0; }

/* interval timers: not available. */
int setitimer(int which, const struct itimerval *v, struct itimerval *old)
{ (void)which; (void)v; (void)old; return 0; }

/* sysconf: return a sane page size; nothing queries anything else meaningfully. */
long sysconf(int name) { (void)name; return 4096; }

/* ownership / permissions: FAT has no Unix uid/gid/mode -> harmless no-ops. */
int fchown(int fd, uid_t uid, gid_t gid)
{ (void)fd; (void)uid; (void)gid; return 0; }

uid_t getuid(void)  { return 0; }
/* geteuid: referenced by _ctypes (.so).  Single-user system. */
uid_t geteuid(void) { return 0; }

mode_t umask(mode_t m) { (void)m; return 0; }

/* advisory locking: single process, no-op. */
int flock(int fd, int op) { (void)fd; (void)op; return 0; }

#endif /* WII_BUILD */
