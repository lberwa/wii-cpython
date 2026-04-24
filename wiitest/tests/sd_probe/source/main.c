#include <gccore.h>
#include <ogc/system.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

#include "../../../../fat/include/pyfat.h"

static void wait_vsync_frames(int frames) {
    for (int i = 0; i < frames; i++) {
        VIDEO_WaitVSync();
    }
}

static void dump_hex_ascii(const unsigned char *buf, size_t n) {
    char line[256];
    size_t pos = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = buf[i];
        pos += (size_t)snprintf(line + pos, sizeof(line) - pos, "%02X ", (unsigned)c);
        if ((i % 16) == 15 || i + 1 == n) {
            size_t pad = (16 - ((i % 16) + 1)) % 16;
            for (size_t p = 0; p < pad; p++) {
                pos += (size_t)snprintf(line + pos, sizeof(line) - pos, "   ");
            }
            pos += (size_t)snprintf(line + pos, sizeof(line) - pos, " |");
            size_t start = i - (i % 16);
            size_t end = i + 1;
            for (size_t j = start; j < end; j++) {
                unsigned char cc = buf[j];
                line[pos++] = (cc >= 32 && cc < 127) ? (char)cc : '.';
                if (pos + 2 >= sizeof(line)) {
                    break;
                }
            }
            line[pos++] = '|';
            line[pos++] = '\0';
            SYS_Report("%s\n", line);
            pos = 0;
        }
    }
}

static void probe_file(const char *path) {
    SYS_Report("---- probe: %s ----\n", path);

    struct stat st;
    errno = 0;
    if (stat(path, &st) == 0) {
        SYS_Report("stat: size=%ld ino(startCluster)=%lu mode=%o\n",
                   (long)st.st_size,
                   (unsigned long)st.st_ino,
                   (unsigned)st.st_mode);
    } else {
        SYS_Report("stat failed: errno=%d (%s)\n", errno, strerror(errno));
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        SYS_Report("open failed: errno=%d (%s)\n", errno, strerror(errno));
        return;
    }

    unsigned char head[64];
    errno = 0;
    size_t r = fread(head, 1, sizeof(head), f);
    SYS_Report(
        "read(head)=%u bytes feof=%d ferror=%d errno=%d (%s)\n",
        (unsigned)r,
        feof(f) ? 1 : 0,
        ferror(f) ? 1 : 0,
        errno,
        strerror(errno)
    );
    if (r > 0) {
        dump_hex_ascii(head, r);
    } else {
        clearerr(f);
        rewind(f);
        errno = 0;
        int c = fgetc(f);
        SYS_Report(
            "fgetc(after rewind)=%d feof=%d ferror=%d errno=%d (%s)\n",
            c,
            feof(f) ? 1 : 0,
            ferror(f) ? 1 : 0,
            errno,
            strerror(errno)
        );
    }

    fclose(f);
}

#ifdef SD_PROBE_WRITE_TEST
static void write_pattern_file(const char *path, size_t size) {
    SYS_Report("---- write: %s (%u bytes) ----\n", path, (unsigned)size);

    FILE *f = fopen(path, "wb");
    if (!f) {
        SYS_Report("open(wb) failed: errno=%d (%s)\n", errno, strerror(errno));
        return;
    }

    unsigned char buf[256];
    for (size_t i = 0; i < sizeof(buf); i++) {
        buf[i] = (unsigned char)(i ^ 0xA5);
    }

    size_t remaining = size;
    while (remaining > 0) {
        size_t chunk = remaining > sizeof(buf) ? sizeof(buf) : remaining;
        errno = 0;
        size_t w = fwrite(buf, 1, chunk, f);
        if (w != chunk) {
            SYS_Report("fwrite short: wrote=%u expected=%u feof=%d ferror=%d errno=%d (%s)\n",
                       (unsigned)w, (unsigned)chunk, feof(f) ? 1 : 0, ferror(f) ? 1 : 0,
                       errno, strerror(errno));
            break;
        }
        remaining -= chunk;
    }

    errno = 0;
    if (fflush(f) != 0) {
        SYS_Report("fflush failed: errno=%d (%s)\n", errno, strerror(errno));
    }
    fclose(f);
}
#endif

static void list_dir(const char *path) {
    SYS_Report("---- list dir: %s ----\n", path);

    DIR *dir = opendir(path);
    if (!dir) {
        SYS_Report("opendir failed: errno=%d (%s)\n", errno, strerror(errno));
        return;
    }

    int shown = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        SYS_Report("%s\n", ent->d_name);
        shown++;
        if (shown >= 80) {
            SYS_Report("(truncated)\n");
            break;
        }
    }
    closedir(dir);
}

int main(void) {
    VIDEO_Init();
    SYS_STDIO_Report(true);

    SYS_Report("[sd_probe] start\n");

    if (!fatInitDefault()) {
        SYS_Report("[sd_probe] fatInitDefault failed\n");
        wait_vsync_frames(600);
        return 1;
    }

    SYS_Report("[sd_probe] fatInitDefault ok\n");

    list_dir("sd:/");

    probe_file("sd:/curl_test.py");
    probe_file("sd:/rendering_test.py");
    probe_file("sd:/testg.py");
    probe_file("sd:/source_py/test.py");

    SYS_Report("[sd_probe] done (waiting)\n");
    wait_vsync_frames(1800);
    return 0;
}
