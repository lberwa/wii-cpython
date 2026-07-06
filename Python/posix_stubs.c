#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>

int sched_yield(void)
{
    errno = ENOSYS;
    return -1;
}

int pause(void)
{
    errno = ENOSYS;
    return -1;
}

int fchdir(int fd)
{
    (void)fd;
    errno = ENOSYS;
    return -1;
}

int fdatasync(int fd)
{
    (void)fd;
    errno = ENOSYS;
    return -1;
}

int chroot(const char *path)
{
    (void)path;
    errno = ENOSYS;
    return -1;
}

int setgroups(int ngroups, const gid_t *grouplist)
{
    (void)ngroups;
    (void)grouplist;
    errno = ENOSYS;
    return -1;
}

/* --------------------------------------------------------------------------
 * Zusaetzliche Wii-Stubs: POSIX-Funktionen, die newlib/libogc nicht bietet,
 * aber von libpython/gdbm referenziert werden. Geben bewusst Erfolg zurueck
 * (nicht -1/ENOSYS), damit CPython normal weiterlaeuft. Frueher lagen diese
 * doppelt in jedem Programm (z.B. wiitest/main.c) -- jetzt zentral hier.
 * -------------------------------------------------------------------------- */

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{ (void)how; (void)set; (void)oldset; return 0; }

int sigaction(int sig, const struct sigaction *act, struct sigaction *old)
{ (void)sig; (void)act; (void)old; return 0; }

int setitimer(int which, const struct itimerval *value,
              struct itimerval *ovalue)
{ (void)which; (void)value; (void)ovalue; return 0; }

long sysconf(int name) { (void)name; return 4096; }

int fchown(int fd, uid_t owner, gid_t group)
{ (void)fd; (void)owner; (void)group; return 0; }

uid_t getuid(void) { return 0; }

mode_t umask(mode_t mask) { (void)mask; return 0; }

int flock(int fd, int operation) { (void)fd; (void)operation; return 0; }

int ftruncate(int fd, off_t length) { (void)fd; (void)length; return 0; }

int fsync(int fd) { (void)fd; return 0; }
