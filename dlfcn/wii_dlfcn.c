/* wii_dlfcn.c - siehe wii_dlfcn.h
 *
 * Ablauf von wii_dlopen():
 *   1. Datei komplett in einen temporaeren Puffer lesen.
 *   2. ELF-Header pruefen (ET_DYN, EM_PPC, 32-bit, big-endian).
 *   3. Speicherbereich fuer alle PT_LOAD-Segmente allozieren (memalign 32).
 *   4. Segmente an load_base + (p_vaddr - min_vaddr) kopieren, .bss nullen.
 *   5. PT_DYNAMIC auswerten (SYMTAB/STRTAB/RELA/JMPREL/HASH/INIT/FINI).
 *   6. Relocations anwenden (R_PPC_*), externe Symbole ueber die
 *      Export-Tabelle des Hauptprogramms aufloesen.
 *   7. Daten-Cache zurueckschreiben + Instruktions-Cache invalidieren.
 *   8. DT_INIT / DT_INIT_ARRAY ausfuehren.
 */

#include "wii_dlfcn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <elf.h>

/* DT_*-Konstanten, die devkitPPC <elf.h> nicht definiert (Standardwerte). */
#ifndef DT_INIT_ARRAY
#define DT_INIT_ARRAY      25
#endif
#ifndef DT_FINI_ARRAY
#define DT_FINI_ARRAY      26
#endif
#ifndef DT_INIT_ARRAYSZ
#define DT_INIT_ARRAYSZ    27
#endif
#ifndef DT_FINI_ARRAYSZ
#define DT_FINI_ARRAYSZ    28
#endif

/* Cache-Verwaltung: auf der Wii ueber libogc, auf dem Host als Fallback
 * ueber den GCC-Builtin (erlaubt Unit-Tests des Parsers). */
#if defined(WII_BUILD) || defined(__wii__)
#include <ogc/cache.h>
#include <malloc.h>
#define WII_MEMALIGN(a, s) memalign((a), (s))
#define WII_SYNC_CODE(p, n) do { DCFlushRange((p), (n)); ICInvalidateRange((p), (n)); } while (0)
#else
#include <stdlib.h>
static void *WII_MEMALIGN(size_t a, size_t s) {
    void *p = NULL;
    if (posix_memalign(&p, a < sizeof(void *) ? sizeof(void *) : a, s) != 0) return NULL;
    return p;
}
#define WII_SYNC_CODE(p, n) __builtin___clear_cache((char *)(p), (char *)(p) + (n))
#endif

/* ---- interner Handle-Typ -------------------------------------------------- */

typedef struct {
    void        *base;          /* alloziertes Speicherblock-Basis == Bias */
    size_t       size;          /* Groesse des Blocks */
    Elf32_Sym   *dynsym;        /* Zeiger in base */
    const char  *dynstr;        /* Zeiger in base */
    uint32_t     nsyms;         /* Anzahl dynsym-Eintraege (aus DT_HASH) */
    void       (*fini)(void);   /* DT_FINI oder NULL */
    void      (**fini_array)(void);
    size_t       fini_array_cnt;
} wii_module;

/* ---- Fehler- und Export-Zustand ------------------------------------------ */

static char g_errbuf[256];
static int  g_err_set = 0;

static const wii_dl_sym  *g_exports  = NULL;
static size_t             g_nexports = 0;
static wii_dl_resolver_t  g_resolver = NULL;

static void *read_file(const char *path, size_t *out_size);

static void set_error(const char *fmt, const char *a) {
    if (a) snprintf(g_errbuf, sizeof(g_errbuf), fmt, a);
    else   snprintf(g_errbuf, sizeof(g_errbuf), "%s", fmt);
    g_err_set = 1;
}

void wii_dl_register_exports(const wii_dl_sym *table, size_t count) {
    g_exports = table;
    g_nexports = count;
}

void wii_dl_set_resolver(wii_dl_resolver_t resolver) {
    g_resolver = resolver;
}

const char *wii_dlerror(void) {
    if (!g_err_set) return NULL;
    g_err_set = 0;
    return g_errbuf;
}

/* Loest ein externes (im Modul undefiniertes) Symbol im Hauptprogramm auf. */
static void *resolve_extern(const char *name) {
    for (size_t i = 0; i < g_nexports; i++) {
        if (strcmp(g_exports[i].name, name) == 0)
            return g_exports[i].addr;
    }
    if (g_resolver)
        return g_resolver(name);
    return NULL;
}

/* ---- Symbol-Map (nm-Ausgabe) als Fallback-Resolver ----------------------- */

typedef struct { const char *name; void *addr; } wii_map_entry;

static char          *g_map_buf   = NULL;   /* gehaltener Dateipuffer */
static wii_map_entry *g_map       = NULL;
static size_t         g_map_len   = 0;

static void *symbol_map_resolver(const char *name) {
    for (size_t i = 0; i < g_map_len; i++) {
        if (strcmp(g_map[i].name, name) == 0)
            return g_map[i].addr;
    }
    return NULL;
}

long wii_dl_load_symbol_map(const char *path) {
    size_t sz = 0;
    char *buf = (char *)read_file(path, &sz);
    if (!buf) return -1;   /* Fehlermeldung via read_file gesetzt */

    /* Erst Zeilen zaehlen, um die Tabelle einmal zu allozieren. */
    size_t lines = 0;
    for (size_t i = 0; i < sz; i++)
        if (buf[i] == '\n') lines++;

    wii_map_entry *tab = (wii_map_entry *)malloc((lines + 1) * sizeof(wii_map_entry));
    if (!tab) { free(buf); set_error("out of memory for symbol map", NULL); return -1; }

    /* Zeilen in-place parsen: "<hex-addr> <typ> <name>".
     * Zeilen ohne Adresse (z.B. "         U foo") werden uebersprungen. */
    size_t n = 0;
    char *p = buf;
    char *end = buf + sz;
    while (p < end) {
        char *nl = memchr(p, '\n', (size_t)(end - p));
        char *line_end = nl ? nl : end;
        if (nl) *nl = '\0';

        /* Adresse (Hex) parsen */
        char *q = p;
        while (q < line_end && *q == ' ') q++;
        uint32_t addr = 0;
        int have_hex = 0;
        while (q < line_end) {
            char c = *q;
            int d;
            if      (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else break;
            addr = (addr << 4) | (uint32_t)d;
            have_hex = 1;
            q++;
        }
        if (have_hex && q < line_end && *q == ' ') {
            q++;                          /* Leerzeichen nach Adresse */
            if (q < line_end) {
                /* Typ-Zeichen ueberspringen */
                q++;
                while (q < line_end && *q == ' ') q++;
                if (q < line_end) {
                    tab[n].name = q;      /* zeigt in g_map_buf */
                    tab[n].addr = (void *)(uintptr_t)addr;
                    n++;
                }
            }
        }
        p = nl ? nl + 1 : end;
    }

    /* Alte Map ersetzen. */
    if (g_map_buf) free(g_map_buf);
    if (g_map)     free(g_map);
    g_map_buf = buf;
    g_map     = tab;
    g_map_len = n;

    wii_dl_set_resolver(symbol_map_resolver);
    return (long)n;
}

/* ---- Datei einlesen ------------------------------------------------------- */

static void *read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) { set_error("cannot open %s", path); return NULL; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); set_error("seek failed on %s", path); return NULL; }
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); set_error("empty/invalid file %s", path); return NULL; }
    rewind(f);
    void *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); set_error("out of memory reading %s", path); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); set_error("short read on %s", path); return NULL;
    }
    fclose(f);
    *out_size = (size_t)sz;
    return buf;
}

/* ---- ELF-Header-Validierung ---------------------------------------------- */

static int validate_ehdr(const Elf32_Ehdr *e, size_t filesz) {
    if (filesz < sizeof(Elf32_Ehdr)) { set_error("file too small for ELF header", NULL); return -1; }
    if (memcmp(e->e_ident, "\177ELF", 4) != 0) { set_error("not an ELF file", NULL); return -1; }
    if (e->e_ident[EI_CLASS] != ELFCLASS32) { set_error("not ELF32", NULL); return -1; }
    if (e->e_ident[EI_DATA] != ELFDATA2MSB) { set_error("not big-endian ELF", NULL); return -1; }
    if (e->e_type != ET_DYN) { set_error("not a shared object (ET_DYN); build with -shared", NULL); return -1; }
    if (e->e_machine != EM_PPC) { set_error("not a PowerPC ELF", NULL); return -1; }
    return 0;
}

/* ---- eine Relocation anwenden -------------------------------------------- */

static int apply_rela(wii_module *m, const Elf32_Rela *r) {
    uint32_t type   = ELF32_R_TYPE(r->r_info);
    uint32_t symidx = ELF32_R_SYM(r->r_info);
    uint8_t *base   = (uint8_t *)m->base;
    void    *where  = base + r->r_offset;

    if (type == R_PPC_NONE)
        return 0;

    /* R_PPC_RELATIVE braucht kein Symbol: base + addend. */
    if (type == R_PPC_RELATIVE) {
        *(uint32_t *)where = (uint32_t)(uintptr_t)base + (uint32_t)r->r_addend;
        return 0;
    }

    /* Symbolwert bestimmen. */
    uint32_t symval = 0;
    if (symidx != 0) {
        const Elf32_Sym *sym = &m->dynsym[symidx];
        const char *name = m->dynstr + sym->st_name;
        if (sym->st_shndx != SHN_UNDEF) {
            /* im Modul definiert */
            symval = (uint32_t)(uintptr_t)base + sym->st_value;
        } else {
            /* extern: im Hauptprogramm nachschlagen */
            void *addr = resolve_extern(name);
            if (!addr) { set_error("unresolved symbol: %s", name); return -1; }
            symval = (uint32_t)(uintptr_t)addr;
        }
    }

    uint32_t value = symval + (uint32_t)r->r_addend;

    switch (type) {
    case R_PPC_ADDR32:
    case R_PPC_GLOB_DAT:
    case R_PPC_JMP_SLOT:   /* bei -fno-plt praktisch nicht genutzt */
        *(uint32_t *)where = value;
        break;

    case R_PPC_ADDR16_LO:
        *(uint16_t *)where = (uint16_t)(value & 0xffff);
        break;
    case R_PPC_ADDR16_HI:
        *(uint16_t *)where = (uint16_t)((value >> 16) & 0xffff);
        break;
    case R_PPC_ADDR16_HA:
        *(uint16_t *)where = (uint16_t)(((value + 0x8000) >> 16) & 0xffff);
        break;

    case R_PPC_REL32:
        *(uint32_t *)where = value - (uint32_t)(uintptr_t)where;
        break;

    case R_PPC_REL24: {   /* bl/b: 24-bit relativer Branch */
        int32_t delta = (int32_t)(value - (uint32_t)(uintptr_t)where);
        uint32_t insn = *(uint32_t *)where;
        insn = (insn & 0xfc000003u) | ((uint32_t)delta & 0x03fffffcu);
        *(uint32_t *)where = insn;
        break;
    }

    default:
        snprintf(g_errbuf, sizeof(g_errbuf), "unsupported relocation type %u", (unsigned)type);
        g_err_set = 1;
        return -1;
    }
    return 0;
}

static int apply_rela_table(wii_module *m, const Elf32_Rela *tab, size_t bytes) {
    if (!tab || bytes == 0) return 0;
    size_t n = bytes / sizeof(Elf32_Rela);
    for (size_t i = 0; i < n; i++) {
        if (apply_rela(m, &tab[i]) != 0)
            return -1;
    }
    return 0;
}

/* ---- Hauptfunktion: laden ------------------------------------------------- */

void *wii_dlopen(const char *path, int mode) {
    (void)mode;
    size_t filesz = 0;
    uint8_t *file = read_file(path, &filesz);
    if (!file) return NULL;

    const Elf32_Ehdr *eh = (const Elf32_Ehdr *)file;
    if (validate_ehdr(eh, filesz) != 0) { free(file); return NULL; }

    const Elf32_Phdr *ph = (const Elf32_Phdr *)(file + eh->e_phoff);

    /* 1) Speicherbedarf ueber alle PT_LOAD-Segmente bestimmen.
     *    ET_DYN => min_vaddr ist 0, wir laden flach ab base. */
    uint32_t min_vaddr = 0xffffffffu, max_vaddr = 0;
    int have_load = 0;
    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;
        have_load = 1;
        if (ph[i].p_vaddr < min_vaddr) min_vaddr = ph[i].p_vaddr;
        uint32_t end = ph[i].p_vaddr + ph[i].p_memsz;
        if (end > max_vaddr) max_vaddr = end;
    }
    if (!have_load) { set_error("no PT_LOAD segments", NULL); free(file); return NULL; }

    size_t span = max_vaddr - min_vaddr;
    uint8_t *base = (uint8_t *)WII_MEMALIGN(32, span);
    if (!base) { set_error("out of memory allocating module image", NULL); free(file); return NULL; }
    memset(base, 0, span);   /* deckt .bss ab */

    /* 2) Segmente kopieren. base entspricht min_vaddr. */
    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;
        memcpy(base + (ph[i].p_vaddr - min_vaddr),
               file + ph[i].p_offset,
               ph[i].p_filesz);
    }

    wii_module *m = (wii_module *)calloc(1, sizeof(wii_module));
    if (!m) { set_error("out of memory (handle)", NULL); free(base); free(file); return NULL; }
    m->base = base;
    m->size = span;

    /* 3) PT_DYNAMIC finden und auswerten. */
    const Elf32_Dyn *dyn = NULL;
    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type == PT_DYNAMIC) {
            dyn = (const Elf32_Dyn *)(base + (ph[i].p_vaddr - min_vaddr));
            break;
        }
    }
    if (!dyn) { set_error("no PT_DYNAMIC segment", NULL); free(m); free(base); free(file); return NULL; }

    const Elf32_Rela *rela = NULL;    size_t relasz = 0;
    const Elf32_Rela *jmprel = NULL;  size_t pltrelsz = 0;
    const uint32_t *hash = NULL;
    void (*init_fn)(void) = NULL;
    void (**init_array)(void) = NULL; size_t init_array_cnt = 0;

    for (const Elf32_Dyn *d = dyn; d->d_tag != DT_NULL; d++) {
        uint32_t v = d->d_un.d_val;
        switch (d->d_tag) {
        case DT_SYMTAB:        m->dynsym = (Elf32_Sym *)(base + v); break;
        case DT_STRTAB:        m->dynstr = (const char *)(base + v); break;
        case DT_HASH:          hash = (const uint32_t *)(base + v); break;
        case DT_RELA:          rela = (const Elf32_Rela *)(base + v); break;
        case DT_RELASZ:        relasz = v; break;
        case DT_JMPREL:        jmprel = (const Elf32_Rela *)(base + v); break;
        case DT_PLTRELSZ:      pltrelsz = v; break;
        case DT_INIT:          init_fn = (void (*)(void))(base + v); break;
        case DT_FINI:          m->fini = (void (*)(void))(base + v); break;
        case DT_INIT_ARRAY:    init_array = (void (**)(void))(base + v); break;
        case DT_INIT_ARRAYSZ:  init_array_cnt = v / sizeof(void *); break;
        case DT_FINI_ARRAY:    m->fini_array = (void (**)(void))(base + v); break;
        case DT_FINI_ARRAYSZ:  m->fini_array_cnt = v / sizeof(void *); break;
        default: break;
        }
    }

    if (!m->dynsym || !m->dynstr) {
        set_error("missing DT_SYMTAB/DT_STRTAB", NULL);
        free(m); free(base); free(file); return NULL;
    }
    /* Symbolanzahl aus der klassischen Hash-Tabelle: hash[1] == nchain. */
    m->nsyms = hash ? hash[1] : 0;

    /* 4) Relocations anwenden. */
    if (apply_rela_table(m, rela, relasz) != 0 ||
        apply_rela_table(m, jmprel, pltrelsz) != 0) {
        free(m); free(base); free(file); return NULL;
    }

    /* Dateipuffer wird ab hier nicht mehr gebraucht. */
    free(file);

    /* 5) Caches synchronisieren, damit die CPU den neuen Code sieht. */
    WII_SYNC_CODE(base, span);

    /* 6) Initialisierer ausfuehren: erst DT_INIT, dann DT_INIT_ARRAY. */
    if (init_fn) init_fn();
    for (size_t i = 0; i < init_array_cnt; i++)
        if (init_array[i]) init_array[i]();

    return m;
}

/* ---- Symbol-Lookup -------------------------------------------------------- */

void *wii_dlsym(void *handle, const char *symbol) {
    wii_module *m = (wii_module *)handle;
    if (!m) { set_error("wii_dlsym: NULL handle", NULL); return NULL; }
    for (uint32_t i = 0; i < m->nsyms; i++) {
        const Elf32_Sym *s = &m->dynsym[i];
        if (s->st_name == 0) continue;
        if (s->st_shndx == SHN_UNDEF) continue;
        if (strcmp(m->dynstr + s->st_name, symbol) == 0)
            return (uint8_t *)m->base + s->st_value;
    }
    set_error("wii_dlsym: symbol not found: %s", symbol);
    return NULL;
}

/* ---- Entladen ------------------------------------------------------------- */

int wii_dlclose(void *handle) {
    wii_module *m = (wii_module *)handle;
    if (!m) return -1;
    /* Fini in umgekehrter Reihenfolge, dann DT_FINI. */
    for (size_t i = m->fini_array_cnt; i > 0; i--)
        if (m->fini_array[i - 1]) m->fini_array[i - 1]();
    if (m->fini) m->fini();
    free(m->base);
    free(m);
    return 0;
}
