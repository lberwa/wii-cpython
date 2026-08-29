/* Support for dynamic loading of extension modules on the Nintendo Wii.
 *
 * Ersetzt dynload_shlib.c (POSIX dlopen) durch den eigenen Wii-ELF-Loader
 * (dlfcn/wii_dlfcn.c, libwiidl.a). Wird aktiv, sobald configure
 * DYNLOADFILE=dynload_wii.o setzt (dann auch HAVE_DYNAMIC_LOADING).
 *
 * CPython ruft _PyImport_FindSharedFuncptr(prefix, shortname, pathname, fp)
 * fuer jedes zu importierende C-Extension-.so auf - zuerst mit prefix
 * "PyModExport", dann "PyInit". Wir laden das .so per wii_dlopen und suchen
 * das Symbol "<prefix>_<shortname>".
 *
 * VORAUSSETZUNG: Das Hauptprogramm muss vorher die Export-Tabelle mit den
 * CPython-API- und libc-Symbolen registriert haben (wii_dl_register_exports /
 * wii_dl_set_resolver bzw. wii_dl_load_symbol_map), damit die im Modul
 * undefinierten Symbole aufgeloest werden koennen.
 */

#include "Python.h"
#include "pycore_importdl.h"      // dl_funcptr, _PyImport_DynLoadFiletab

/* Prototypen des Wii-Loaders (aus dlfcn/wii_dlfcn.h) - hier direkt deklariert,
 * um keine zusaetzlichen Include-Pfade in den Core-Build einzuschleusen. */
#define WII_RTLD_NOW 0x0002
extern void       *wii_dlopen(const char *path, int mode);
extern void       *wii_dlsym(void *handle, const char *symbol);
extern const char *wii_dlerror(void);

/* Dateiendungen, unter denen der Import-Mechanismus C-Extensions sucht.
 * Auf der Wii schlicht ".so" (Module werden von Hand als foo.so gebaut). */
const char *_PyImport_DynLoadFiletab[] = {
    ".so",
    NULL,
};

/* --- kleiner Handle-Cache -------------------------------------------------
 * findfuncptr() wird pro Modul zweimal aufgerufen (PyModExport, PyInit).
 * Ohne Cache wuerde wii_dlopen dasselbe .so zweimal laden, relozieren und
 * dessen Init-Konstruktoren doppelt ausfuehren. Wir cachen daher je Pfad
 * genau ein Handle. Handles bleiben absichtlich offen (Module leben so lange
 * wie der Interpreter - wie bei CPython ueblich). */
typedef struct {
    char *path;
    void *handle;
} wii_dl_cache_entry;

static wii_dl_cache_entry *g_cache = NULL;
static size_t g_cache_len = 0;
static size_t g_cache_cap = 0;

static void *cache_lookup(const char *path) {
    for (size_t i = 0; i < g_cache_len; i++) {
        if (strcmp(g_cache[i].path, path) == 0)
            return g_cache[i].handle;
    }
    return NULL;
}

static void cache_store(const char *path, void *handle) {
    if (g_cache_len == g_cache_cap) {
        size_t ncap = g_cache_cap ? g_cache_cap * 2 : 8;
        wii_dl_cache_entry *n = PyMem_RawRealloc(g_cache, ncap * sizeof(*n));
        if (n == NULL) return;   /* Cache ist nur Optimierung - Fehler tolerieren */
        g_cache = n;
        g_cache_cap = ncap;
    }
    size_t n = strlen(path) + 1;
    char *dup = PyMem_RawMalloc(n);
    if (dup == NULL) return;
    memcpy(dup, path, n);
    g_cache[g_cache_len].path = dup;
    g_cache[g_cache_len].handle = handle;
    g_cache_len++;
}

/* --------------------------------------------------------------------------- */

dl_funcptr
_PyImport_FindSharedFuncptr(const char *prefix,
                            const char *shortname,
                            const char *pathname, FILE *fp)
{
    (void)fp;
    char funcname[258];

    PyOS_snprintf(funcname, sizeof(funcname), "%.20s_%.200s", prefix, shortname);

    void *handle = cache_lookup(pathname);
    if (handle == NULL) {
        handle = wii_dlopen(pathname, WII_RTLD_NOW);
        if (handle == NULL) {
            const char *error = wii_dlerror();
            if (error == NULL)
                error = "unknown wii_dlopen() error";
            PyObject *error_ob = PyUnicode_DecodeLocale(error, "surrogateescape");
            if (error_ob == NULL)
                return NULL;
            PyObject *mod_name = PyUnicode_FromString(shortname);
            if (mod_name == NULL) {
                Py_DECREF(error_ob);
                return NULL;
            }
            PyObject *path = PyUnicode_DecodeFSDefault(pathname);
            if (path == NULL) {
                Py_DECREF(error_ob);
                Py_DECREF(mod_name);
                return NULL;
            }
            PyErr_SetImportError(error_ob, mod_name, path);
            Py_DECREF(error_ob);
            Py_DECREF(mod_name);
            Py_DECREF(path);
            return NULL;
        }
        cache_store(pathname, handle);
    }

    return (dl_funcptr) wii_dlsym(handle, funcname);
}
