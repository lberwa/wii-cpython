/* Minimal pyconfig.h stub for cross-build without configure (like 2.0).
 * Copy from config.guess or minimal defs. Errors expected later. */
#ifndef Py_PYCONFIG_H
#define Py_PYCONFIG_H

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
#define NATIVE_TSS_KEY_T pthread_key_t
#define HAVE_PTHREAD_H 1

/* End */
#endif
