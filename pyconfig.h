/* Minimal pyconfig.h stub for cross-build without configure (like 2.0).
 * Copy from config.guess or minimal defs. Errors expected later. */
#ifndef Py_PYCONFIG_H
#define Py_PYCONFIG_H

/* Enable POSIX features for proper fileno()/fdopen() declarations */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define __MISC_VISIBLE 1

#define HAVE_CONFIG_H
#undef _POSIX_C_SOURCE
#define PY_VERSION "3.15.0a7"
#define Py_USING_UNICODE
#define SIZEOF_INT 4
#define SIZEOF_LONG 4
#define SIZEOF_VOID_P 4
#define SIZEOF_TIME_T 4
#define Py_DEBUG 0
#define WITH_THREAD 0
#define Py_TRACE_REFS 0
#define Py_REF_DEBUG 0
#define Py_BUILD_CORE
#define CO_MAXBLOCKS 80
#define WITH_PYMALLOC 1
#define Py_NO_ENABLE_SHARED 1
#define USE_STATIC_LIBS

/* Wii/devkitpro - FULLDEFS */
#define MACHDEP "wii"
/* #define Py_ssize_t long */
/* #define ssize_t long */
#define SIZEOF_SIZE_T 4
#define SIZEOF_PY_SSIZE_T 4
#define SIZEOF_WCHAR_T 4
#define ALIGNOF_SIZE_T 4
#define SIZEOF_LONG_LONG 8
#define HAVE_LONG_LONG 1
#define HAVE_USABLE_WCHAR_T 1
/* #define Py_hash_t long */
#define WITH_THREAD 0
#define USE_COMPUTED_GOTOS 0
#define STACKLESS 0
#define Py_USING_UNICODE 1
#define Py_BUILD_CORE_BUILTIN 1
#define Py_BUILD_CORE_MODULE 1
#define HAVE_CLOCK_GETTIME 0
#define HAVE_LIBMPDEC 0
#define Py_NO_ENABLE_SHARED 1
#define HAVE_STD_ATOMIC 1
#define HAVE_UNISTD_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_FCNTL_H 1
#define HAVE_DIRENT_H 1
typedef unsigned pthread_key_t;
#define NATIVE_TSS_KEY_T pthread_key_t
#define HAVE_PTHREAD_H 1
#define _POSIX_THREADS 1
#define SIZEOF_PTHREAD_T 4

/* End */
#endif
