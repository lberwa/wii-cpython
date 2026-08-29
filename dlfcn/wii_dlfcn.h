/* wii_dlfcn.h - Minimaler Laufzeit-Loader fuer PowerPC-ELF-Shared-Objects (.so)
 * auf der Nintendo Wii (devkitPPC / libogc, bare-metal, kein OS-Dynamic-Linker).
 *
 * Ersetzt die glibc-dlfcn-API (dlopen/dlsym/dlclose/dlerror), deren echte
 * Implementierung im Linux-Runtime-Linker (ld.so) steckt und auf der Wii
 * fehlt. Die PPC-Relocation-Mathematik ist an glibc
 * sysdeps/powerpc/powerpc32/dl-machine.h angelehnt; Cache-Handling nutzt
 * libogc <ogc/cache.h>.
 *
 * Module MUESSEN gebaut werden mit:
 *     powerpc-eabi-gcc -fPIC -fno-plt -shared -nostdlib ...
 * (-fno-plt vermeidet PPC-PLT-Stubs; externe Referenzen werden dann zu
 *  einfachen R_PPC_ADDR32/GLOB_DAT-Relocations in die GOT.)
 *
 * Da eine .dol-Datei zur Laufzeit KEINE Symboltabelle hat, muss das
 * Hauptprogramm die Symbole, die geladene Module aufrufen duerfen
 * (z.B. CPython-API, libc), vorher per wii_dl_register_exports() /
 * wii_dl_set_resolver() bekanntmachen.
 */

#ifndef WII_DLFCN_H
#define WII_DLFCN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Modi fuer wii_dlopen() - Bindung erfolgt immer sofort (eager),
 * die Flags werden zur Quellcode-Kompatibilitaet akzeptiert. */
#define WII_RTLD_LAZY    0x0001
#define WII_RTLD_NOW     0x0002
#define WII_RTLD_LOCAL   0x0000
#define WII_RTLD_GLOBAL  0x0100

/* Eintrag in der Export-Tabelle des Hauptprogramms: Name -> Adresse. */
typedef struct {
    const char *name;
    void       *addr;
} wii_dl_sym;

/* Optionaler Fallback-Resolver fuer Symbole, die nicht in der
 * registrierten Tabelle stehen. Gibt Adresse oder NULL zurueck. */
typedef void *(*wii_dl_resolver_t)(const char *name);

/* Registriert die statische Export-Tabelle des Hauptprogramms.
 * Der Zeiger wird gespeichert (nicht kopiert) - Tabelle muss leben bleiben. */
void wii_dl_register_exports(const wii_dl_sym *table, size_t count);

/* Setzt einen Fallback-Resolver (wird nach der Tabelle befragt). */
void wii_dl_set_resolver(wii_dl_resolver_t resolver);

/* Laedt eine Symbol-Map (nm-Ausgabe: "<hex-addr> <typ> <name>" pro Zeile) und
 * registriert sie als Fallback-Resolver. Das ist das Wii-Aequivalent zu
 * --export-dynamic: da das .dol zur Laufzeit keine Symboltabelle hat, wird die
 * Map aus dem verlinkten .elf per `powerpc-eabi-nm` erzeugt und mitgeliefert.
 * Die Datei wird im Speicher gehalten (Namen zeigen hinein) - nicht freigeben.
 * Gibt die Anzahl geladener Symbole zurueck, oder -1 bei Fehler. */
long wii_dl_load_symbol_map(const char *path);

/* Laedt ein PPC-ELF-Shared-Object von 'path' (SD/FAT), fuehrt Relocations
 * und Init-Funktionen aus und gibt ein Handle zurueck (NULL bei Fehler,
 * Details via wii_dlerror()). */
void *wii_dlopen(const char *path, int mode);

/* Loest ein Symbol aus einem geladenen Modul auf. NULL wenn nicht gefunden. */
void *wii_dlsym(void *handle, const char *symbol);

/* Fuehrt Fini-Funktionen aus und gibt den Modulspeicher frei.
 * Gibt 0 bei Erfolg zurueck. */
int wii_dlclose(void *handle);

/* Letzte Fehlermeldung (oder NULL wenn seit dem letzten Aufruf keiner). */
const char *wii_dlerror(void);

#ifdef __cplusplus
}
#endif

#endif /* WII_DLFCN_H */
