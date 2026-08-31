#include "cpython/pthread_stubs.h"
#include <errno.h>
#include <ogc/lwp.h>
#include <ogc/mutex.h>
#include <ogc/cond.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <lwp_threads.h>
#include <my_text_renderer.h>

#define WII_PTHREAD_DEFAULT_PRIO 80
#define WII_PTHREAD_DEFAULT_STACKSIZE 0
#define WII_PTHREAD_DESTRUCTOR_ITERATIONS 4

#ifndef PTHREAD_STACK_MIN
#  define PTHREAD_STACK_MIN 0x8000
#endif

typedef struct wii_stub_tls_thread {
    pthread_t thread;
    void *values[PTHREAD_KEYS_MAX];
    struct wii_stub_tls_thread *next;
} wii_stub_tls_thread;

typedef struct {
    int in_use;
    void (*destructor)(void *);
} wii_stub_tls_key;

typedef struct {
    void *(*start_routine)(void *);
    void *arg;
} wii_stub_thread_bootstrap;

static mutex_t wii_stub_registry_lock = LWP_MUTEX_NULL;
static int wii_stub_registry_ready = 0;
static wii_stub_tls_thread *wii_stub_tls_threads = NULL;
static wii_stub_tls_key wii_stub_tls_keys[PTHREAD_KEYS_MAX];

static int
wii_stub_registry_ensure(void)
{
    if (wii_stub_registry_ready) {
        return 0;
    }
    int rc = LWP_MutexInit(&wii_stub_registry_lock, true);
    if (rc != 0) {
        return rc;
    }
    memset(wii_stub_tls_keys, 0, sizeof(wii_stub_tls_keys));
    wii_stub_registry_ready = 1;
    return 0;
}

static wii_stub_tls_thread *
wii_stub_tls_find_unlocked(pthread_t thread)
{
    wii_stub_tls_thread *entry = wii_stub_tls_threads;
    while (entry != NULL) {
        if (entry->thread == thread) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

static void
wii_stub_tls_cleanup_current(void)
{
    if (!wii_stub_registry_ready) {
        return;
    }

    pthread_t thread = pthread_self();
    wii_stub_tls_thread *entry = NULL;

    LWP_MutexLock(wii_stub_registry_lock);
    wii_stub_tls_thread **link = &wii_stub_tls_threads;
    while (*link != NULL) {
        if ((*link)->thread == thread) {
            entry = *link;
            *link = entry->next;
            entry->next = NULL;
            break;
        }
        link = &(*link)->next;
    }
    LWP_MutexUnlock(wii_stub_registry_lock);

    if (entry == NULL) {
        return;
    }

    for (int pass = 0; pass < WII_PTHREAD_DESTRUCTOR_ITERATIONS; pass++) {
        int ran_destructor = 0;
        for (pthread_key_t key = 0; key < PTHREAD_KEYS_MAX; key++) {
            void *value = entry->values[key];
            void (*destructor)(void *) = NULL;
            if (value == NULL) {
                continue;
            }

            LWP_MutexLock(wii_stub_registry_lock);
            if (wii_stub_tls_keys[key].in_use) {
                destructor = wii_stub_tls_keys[key].destructor;
            }
            LWP_MutexUnlock(wii_stub_registry_lock);

            entry->values[key] = NULL;
            if (destructor != NULL) {
                destructor(value);
                ran_destructor = 1;
            }
        }
        if (!ran_destructor) {
            break;
        }
    }

    free(entry);
}

static void *
wii_stub_thread_trampoline(void *arg)
{
    wii_stub_thread_bootstrap *bootstrap = arg;
    void *(*start_routine)(void *) = bootstrap->start_routine;
    void *routine_arg = bootstrap->arg;
    free(bootstrap);

    void *result = start_routine(routine_arg);
    wii_stub_tls_cleanup_current();
    return result;
}

// mutex
int
pthread_mutex_init(pthread_mutex_t *restrict mutex,
                   const pthread_mutexattr_t *restrict attr)
{
    if (mutex == NULL) {
        return EINVAL;
    }
    return LWP_MutexInit((mutex_t *)mutex, attr != NULL && attr->recursive);
}

int
pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    if (mutex == NULL) {
        return EINVAL;
    }
    return LWP_MutexDestroy(*mutex);
}

int
pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    if (mutex == NULL) {
        return EINVAL;
    }
    int rc = LWP_MutexTryLock(*mutex);
    if (rc == 1) {
        return EBUSY;
    }
    return rc;
}

int
pthread_mutex_lock(pthread_mutex_t *mutex)
{
    if (mutex == NULL) {
        return EINVAL;
    }
    return LWP_MutexLock(*mutex);
}

int
pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    if (mutex == NULL) {
        return EINVAL;
    }
    return LWP_MutexUnlock(*mutex);
}

// condition
int
pthread_cond_init(pthread_cond_t *restrict cond,
                  const pthread_condattr_t *restrict attr)
{
    (void)attr;
    if (cond == NULL) {
        return EINVAL;
    }
    return LWP_CondInit((cond_t *)cond);
}

PyAPI_FUNC(int) pthread_cond_destroy(pthread_cond_t *cond)
{
    if (cond == NULL) {
        return EINVAL;
    }
    return LWP_CondDestroy(*cond);
}

int
pthread_cond_wait(pthread_cond_t *restrict cond,
                  pthread_mutex_t *restrict mutex)
{
    if (cond == NULL || mutex == NULL) {
        return EINVAL;
    }
    return LWP_CondWait(*cond, *mutex);
}

int
pthread_cond_timedwait(pthread_cond_t *restrict cond,
                       pthread_mutex_t *restrict mutex,
                       const struct timespec *restrict abstime)
{
    if (cond == NULL || mutex == NULL) {
        return EINVAL;
    }
    return LWP_CondTimedWait(*cond, *mutex, abstime);
}

int
pthread_cond_signal(pthread_cond_t *cond)
{
    if (cond == NULL) {
        return EINVAL;
    }
    return LWP_CondSignal(*cond);
}

int
pthread_cond_broadcast(pthread_cond_t *cond)
{
    if (cond == NULL) {
        return EINVAL;
    }
    return LWP_CondBroadcast(*cond);
}

int
pthread_condattr_init(pthread_condattr_t *attr)
{
    if (attr == NULL) {
        return EINVAL;
    }
    memset(attr, 0, sizeof(*attr));
    attr->is_initialized = 1;
    return 0;
}

int
pthread_condattr_destroy(pthread_condattr_t *attr)
{
    if (attr == NULL) {
        return EINVAL;
    }
    memset(attr, 0, sizeof(*attr));
    return 0;
}

int
pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock_id)
{
    if (attr == NULL) {
        return EINVAL;
    }
    attr->clock = clock_id;
    return 0;
}

// pthread
int
pthread_create(pthread_t *restrict thread,
               const pthread_attr_t *restrict attr,
               void *(*start_routine)(void *),
               void *restrict arg)
{
    if (thread == NULL || start_routine == NULL) {
        return EINVAL;
    }

    wii_stub_thread_bootstrap *bootstrap = malloc(sizeof(*bootstrap));
    if (bootstrap == NULL) {
        return EAGAIN;
    }
    bootstrap->start_routine = start_routine;
    bootstrap->arg = arg;

    void *stackbase = NULL;
    u32 stacksize = WII_PTHREAD_DEFAULT_STACKSIZE;
    if (attr != NULL) {
        if (attr->stackaddr != NULL) {
            stackbase = attr->stackaddr;
        }
        if (attr->stacksize > 0) {
            stacksize = (u32)attr->stacksize;
        }
    }

    int rc = LWP_CreateThread((lwp_t *)thread, wii_stub_thread_trampoline,
                              bootstrap, stackbase, stacksize,
                              WII_PTHREAD_DEFAULT_PRIO);
    if (rc != 0) {
        free(bootstrap);
        return EAGAIN;
    }
    return 0;
}

int
pthread_detach(pthread_t thread)
{
    (void)thread;
    return 0;
}

int
pthread_join(pthread_t thread, void** value_ptr)
{
    return LWP_JoinThread((lwp_t)thread, value_ptr);
}

PyAPI_FUNC(pthread_t) pthread_self(void)
{
    return (pthread_t)LWP_GetSelf();
}

int
pthread_equal(pthread_t t1, pthread_t t2)
{
    return t1 == t2;
}

void
pthread_exit(void *retval)
{
    wii_stub_tls_cleanup_current();
    __lwp_thread_exit(retval);
    abort();
}

int
pthread_attr_init(pthread_attr_t *attr)
{
    if (attr == NULL) {
        return EINVAL;
    }
    memset(attr, 0, sizeof(*attr));
    attr->is_initialized = 1;
    attr->detachstate = PTHREAD_CREATE_JOINABLE;
    return 0;
}

int
pthread_attr_setstacksize(
    pthread_attr_t *attr, size_t stacksize)
{
    if (attr == NULL || stacksize < PTHREAD_STACK_MIN) {
        return EINVAL;
    }
    attr->stacksize = (int)stacksize;
    return 0;
}

int
pthread_attr_destroy(pthread_attr_t *attr)
{
    if (attr == NULL) {
        return EINVAL;
    }
    memset(attr, 0, sizeof(*attr));
    return 0;
}


int
pthread_key_create(pthread_key_t *key, void (*destr_function)(void *))
{
    if (!key) {
        return EINVAL;
    }
    if (wii_stub_registry_ensure() != 0) {
        return EAGAIN;
    }
    LWP_MutexLock(wii_stub_registry_lock);
    for (pthread_key_t idx = 0; idx < PTHREAD_KEYS_MAX; idx++) {
        if (!wii_stub_tls_keys[idx].in_use) {
            wii_stub_tls_keys[idx].in_use = 1;
            wii_stub_tls_keys[idx].destructor = destr_function;
            *key = idx;
            LWP_MutexUnlock(wii_stub_registry_lock);
            return 0;
        }
    }
    LWP_MutexUnlock(wii_stub_registry_lock);
    return EAGAIN;
}

int
pthread_key_delete(pthread_key_t key)
{
    if (key >= PTHREAD_KEYS_MAX || !wii_stub_registry_ready) {
        return EINVAL;
    }
    LWP_MutexLock(wii_stub_registry_lock);
    if (!wii_stub_tls_keys[key].in_use) {
        LWP_MutexUnlock(wii_stub_registry_lock);
        return EINVAL;
    }
    wii_stub_tls_keys[key].in_use = 0;
    wii_stub_tls_keys[key].destructor = NULL;
    for (wii_stub_tls_thread *entry = wii_stub_tls_threads;
         entry != NULL;
         entry = entry->next) {
        entry->values[key] = NULL;
    }
    LWP_MutexUnlock(wii_stub_registry_lock);
    return 0;
}


void *
pthread_getspecific(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX || !wii_stub_registry_ready) {
        return NULL;
    }
    LWP_MutexLock(wii_stub_registry_lock);
    if (!wii_stub_tls_keys[key].in_use) {
        LWP_MutexUnlock(wii_stub_registry_lock);
        return NULL;
    }
    wii_stub_tls_thread *entry = wii_stub_tls_find_unlocked(pthread_self());
    void *value = entry != NULL ? entry->values[key] : NULL;
    LWP_MutexUnlock(wii_stub_registry_lock);
    return value;
}

int
pthread_setspecific(pthread_key_t key, const void *value)
{
    if (key >= PTHREAD_KEYS_MAX || wii_stub_registry_ensure() != 0) {
        return EINVAL;
    }
    LWP_MutexLock(wii_stub_registry_lock);
    if (!wii_stub_tls_keys[key].in_use) {
        LWP_MutexUnlock(wii_stub_registry_lock);
        return EINVAL;
    }
    wii_stub_tls_thread *entry = wii_stub_tls_find_unlocked(pthread_self());
    if (entry == NULL) {
        entry = calloc(1, sizeof(*entry));
        if (entry == NULL) {
            LWP_MutexUnlock(wii_stub_registry_lock);
            return EAGAIN;
        }
        entry->thread = pthread_self();
        entry->next = wii_stub_tls_threads;
        wii_stub_tls_threads = entry;
    }
    entry->values[key] = (void *)value;
    LWP_MutexUnlock(wii_stub_registry_lock);
    return 0;
}

int
pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset)
{
    return sigprocmask(how, set, oldset);
}

int
pthread_kill(pthread_t thread, int sig)
{
    if (thread != pthread_self()) {
        return ESRCH;
    }
    if (raise(sig) != 0) {
        return errno;
    }
    return 0;
}

// let thread_pthread define the Python API
#include "thread_pthread.h"
