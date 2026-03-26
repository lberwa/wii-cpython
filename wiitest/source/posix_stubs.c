#include <errno.h>

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

int setgroups(int size, const int *list)
{
    (void)size;
    (void)list;
    errno = ENOSYS;
    return -1;
}
