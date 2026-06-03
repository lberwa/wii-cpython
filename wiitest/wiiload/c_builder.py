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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include "../../../bitmap/include/my_text_renderer.h"
             
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

    terminal_print("Start...\\n");
             
    for (int i=0; i<100; i++) {
        VIDEO_WaitVSync();
    }
             
    if (!fatInitDefault())
    {
        terminal_print("SD init failed\\n");
        return 1;
    }

""")

for path in paths:
    sym = _sym_for_path(path)
    parts.append("   {\n")
    parts.append(f"       const unsigned char *data = {sym};\n")
    parts.append(f"       size_t size = {sym}_end - {sym};\n\n")

    # Strip the leading "SD/" so SD/test/foo.txt becomes sd:/test/foo.txt
    parts.append(f'       const char *path = "sd:/{path[3:]}";\n\n')

    parts.append("""
        FILE *f = fopen(path, "w");

if (!f) {
    terminal_print("Failed to write:");
    terminal_print(path);
} else {
    terminal_print("writing ... :");
    terminal_print(path);

    fwrite(data, 1, size, f);

    terminal_print("jj");

    terminal_print("before fflush");
fflush(f);
terminal_print("after fflush");

terminal_print("before fclose");
fclose(f);
terminal_print("after fclose");

        }\n\n\n
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
