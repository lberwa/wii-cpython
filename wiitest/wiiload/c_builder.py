import os
import re


def _sym_for_path(path: str) -> str:
    # Must match the Makefile rule that copies SD/<relpath> to a temp file
    # named: sd_<relpath with / and . -> _ and all other non [A-Za-z0-9_] -> _>
    rel = os.path.relpath(path, "SD")
    sym = "sd_" + rel.replace("/", "_").replace(".", "_")
    sym = re.sub(r"[^A-Za-z0-9_]", "_", sym)
    return sym


paths: list[str] = []
for root, _dirs, files in os.walk("SD/"):
    for name in files:
        path = os.path.join(root, name)
        # bin2s skips empty files (no symbols), so ignore them here too.
        try:
            if os.path.getsize(path) == 0:
                continue
        except OSError:
            continue
        paths.append(path)

parts: list[str] = []

parts.append("""
#include <gccore.h>
#include "../../../fat/include/pyfat.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include "../../../bitmap/include/my_text_renderer.h"
             
""")

parts.append("""
static void print_int_value(const char *label, int value)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s%d", label, value);
    terminal_print(buf);
}

static void print_size_value(const char *label, size_t value)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s%u", label, (unsigned)value);
    terminal_print(buf);
}

static int export_file_to_sd(const char *path, const unsigned char *data, size_t size)
{
    FILE *f;
    unsigned char buffer[256];
    size_t offset = 0;

    terminal_print("before fopen");
    f = fopen(path, "wb");
    if (!f) {
        terminal_print("fopen failed:");
        terminal_print(path);
        print_int_value("fopen errno=", errno);
        return -1;
    }
    terminal_print("after fopen");

    while (offset < size) {
        size_t chunk = (size - offset > sizeof(buffer)) ? sizeof(buffer) : (size - offset);
        memcpy(buffer, data + offset, chunk);
        print_size_value("offset=", offset);
        print_size_value("chunk=", chunk);
        terminal_print("before fwrite");
        errno = 0;
        if (fwrite(buffer, 1, chunk, f) != chunk) {
            terminal_print("fwrite failed");
            print_int_value("fwrite errno=", errno);
            fclose(f);
            return -2;
        }
        terminal_print("after fwrite");
        offset += chunk;
    }

    terminal_print("before fflush");
    errno = 0;
    if (fflush(f) != 0) {
        terminal_print("fflush failed");
        print_int_value("fflush errno=", errno);
        fclose(f);
        return -3;
    }
    terminal_print("after fflush");

    terminal_print("before fclose");
    errno = 0;
    if (fclose(f) != 0) {
        terminal_print("fclose failed");
        print_int_value("fclose errno=", errno);
        return -4;
    }
    terminal_print("after fclose");

    return 0;
}

""")
    
for path in paths:
    sym = _sym_for_path(path)
    parts.append(f"extern const unsigned char {sym}[];\n")
    parts.append(f"extern const unsigned char {sym}_end[];\n\n")
    
'''parts.append("""
typedef struct {
    const unsigned char *start;
    const unsigned char *end;
    const char *path;
} FileEntry;

""")'''
    
parts.append("""
int main(/*int argc, char **argv*/)
{
    video_init_custom();

    terminal_print("Start...");
             
    if (!fatInitDefault())
    {
        terminal_print("SD init failed");
        return 1;
    }

""")

for path in paths:
    sym = _sym_for_path(path)
    parts.append("   {\n")
    parts.append(f"       const unsigned char *data = {sym};\n")
    parts.append(f"       size_t size = {sym}_end - {sym};\n\n")

    # Strip the leading "SD/" so SD/test/foo.txt becomes sd:/hello/test/foo.txt
    parts.append(f'       const char *path = "sd:/hello/{path[3:]}";\n\n')

    parts.append("""
        terminal_print("writing ... :");
        terminal_print(path);
        print_size_value("size=", size);
        if (size > 0) {
            print_int_value("byte0=", data[0]);
        }
        print_int_value("export rc=", export_file_to_sd(path, data, size));


        }
""")
        
parts.append("""
    terminal_print("Done writing files.\\n");
    terminal_print("exiting...\\n");
    return 0;
}
""")

out = "".join(parts)
out_path = "source/main.c"
try:
    with open(out_path, "r", encoding="utf-8") as existing:
        if existing.read() == out:
            raise SystemExit(0)
except FileNotFoundError:
    pass

with open(out_path, "w", encoding="utf-8") as f:
    f.write(out)
