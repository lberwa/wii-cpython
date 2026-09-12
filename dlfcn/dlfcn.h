/* dlfcn.h — POSIX shim for Wii: maps dlopen/dlsym/dlclose/dlerror
 * to the wii_dlfcn runtime loader (wii_dlfcn.h / wii_dlfcn.c). */
#ifndef _DLFCN_H
#define _DLFCN_H

#include "wii_dlfcn.h"

#ifndef RTLD_LAZY
#  define RTLD_LAZY    WII_RTLD_LAZY
#endif
#ifndef RTLD_NOW
#  define RTLD_NOW     WII_RTLD_NOW
#endif
#ifndef RTLD_LOCAL
#  define RTLD_LOCAL   WII_RTLD_LOCAL
#endif
#ifndef RTLD_GLOBAL
#  define RTLD_GLOBAL  WII_RTLD_GLOBAL
#endif

#define dlopen(path, mode)  wii_dlopen((path), (mode))
#define dlsym(handle, name) wii_dlsym((handle), (name))
#define dlclose(handle)     wii_dlclose(handle)
#define dlerror()           wii_dlerror()

#endif /* _DLFCN_H */
