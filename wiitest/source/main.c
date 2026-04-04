/* test_python_embed.c */
#include <Python.h>
#include "../../bitmap/include/render_text.h"
#include <stdlib.h>
#include <string.h>

#include <gccore.h>

extern const unsigned char test_py[];
extern const unsigned char test_py_end[];

// #define FIND_FILE2
// #define FIND_FILE
//#define PNG

#include "../../fat/include/pyfat.h"
#ifdef PNG

#include <stdio.h>

/* Symbole vom bin2o/bin2s für data/test.png */
extern const unsigned char test_png[];
extern const unsigned char test_png_end[];
extern const unsigned char brew_png[];
extern const unsigned char brew_png_end[];

static int export_png_to_sd(const char *dst_path, const unsigned char *start, const unsigned char *end) {
    FILE *f;
    size_t len = (size_t)(end - start);
    size_t written;

    if (!fatInitDefault()) {
        return -1; /* SD mount failed */
    }
    terminal_print("1");

    f = fopen(dst_path, "wb");
    if (!f) {
        return -2; /* open failed */
    }

    terminal_print("2");

    written = fwrite(start, 1, len, f);
    fclose(f);

    terminal_print("3");

    if (written != len) {
        return -3; /* write failed */
    }
    terminal_print("4");
    return 0;
}
#endif

void wait(int time) {
    for (int i = 0; i < time; ++i) {
        VIDEO_WaitVSync();
    }
}


#ifdef FIND_FILE2
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include "../../fat/include/pyfat.h"      // für fatInitDefault

void list_dir(const char *path, int level) {
    DIR *dir = opendir(path);
    if (!dir) {
        //terminal_print("Kann %s nicht öffnen\n", path);
        terminal_print("Kann");
        terminal_print(path);
        terminal_print("nicht öffnen\n");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        for (int i = 0; i < level; i++) terminal_print("  "); // Einrückung
//        terminal_print("%s%s\n", entry->d_name,
//                       (entry->d_type == DT_DIR) ? "/" : "");
        terminal_print(entry->d_name);
        terminal_print((entry->d_type == DT_DIR) ? "/" : "");

        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char subpath[256];
            snprintf(subpath, sizeof(subpath), "%s/%s", path, entry->d_name);
            list_dir(subpath, level + 1);
        }
    }

    closedir(dir);
}
#endif

int main(void) {
    
    char *script;
    size_t script_len;
    int rc;
    #ifdef PNG
    char msg[96];
    #endif

    video_init_custom();
    terminal_print("start");

    #ifdef PNG
    if (!fatInitDefault()) {
        terminal_print("fatInitDefault failed before PNG export");
    }

    terminal_print("load test.png ...");
    if (rc != 0) {
        terminal_print("PNG export failed; sd:/test.png may be missing");
    }

    terminal_print("load brew.png ...");
    rc = export_png_to_sd("sd:/brew.png", brew_png, brew_png_end);
    snprintf(msg, sizeof(msg), "export_brew_png_to_sd rc=%d", rc);
    terminal_print(msg);
    if (rc != 0) {
        terminal_print("PNG export failed; sd:/brew.png may be missing");
    }
    #endif

    if (!fatInitDefault()) {
        terminal_print("fatInitDefault fehlgeschlagen!\n");
        return 1;
    }

#ifdef FIND_FILE2
    terminal_print("start 2\n");

    // Datei auf der SD-Karte erstellen / überschreiben
    FILE *f = fopen("sd:/text.txt", "w");
    if (!f) {
        terminal_print("Fehler beim Öffnen der Datei!\n");
        return 1;
    }

    // Inhalt schreiben
    const char *text = "Hallo Welt!";
    fprintf(f, "%s\n", text);

    fclose(f);
    terminal_print("Datei erfolgreich geschrieben.\n");
    terminal_print("-----------------------");

    terminal_print("Inhalt von SD-Karte:\n");
    list_dir("sd:", 0);
    terminal_print("-----------------------");

    wait(300); // 5 Sekunden warten
#endif

#ifdef FIND_FILE
    const char *file_path = "sd:/hello/test2.py";
    FILE *fp = fopen(file_path, "r");
    if (!fp) {
        terminal_print("Datei nicht gefunden: ");
        terminal_print(file_path);
        return 1;
    }

    terminal_print("Inhalt von ");
    terminal_print(file_path);
    terminal_print("--------------------\n");

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fp)) {
        terminal_print(buffer);
    }

    fclose(fp);
    terminal_print("\nFertig.\n");
#endif

    size_t count = 1;

    //Py_Initalize_Custom(NULL); 
    PyStatus status = Py_Init_Custom((const char*[]){ "sd:/", "sd:"}, &count);

    if (status._type != _PyStatus_TYPE_OK) {
        // Init ist fehlgeschlagen
        terminal_print("Init failed in:");
        terminal_print(status.func);
        terminal_print(status.err_msg);
        return status.exitcode;  // ggf. Programm beenden
    }
    
    script_len = (size_t)(test_py_end - test_py);
    script = (char *)malloc(script_len + 1);
    if (script == NULL) {
        terminal_print("malloc failed: exiting ...");
        Py_Finalize();
        return 1;
    }

    memcpy(script, test_py, script_len);
    script[script_len] = '\0';

    terminal_print("run source_py/test.py ...");
    rc = PyRun_SimpleString(script);

    free(script);
    if (rc != 0) {
        terminal_print("python script returned error");
        wait(600);
    }

    Py_Finalize();

    terminal_print("done");
    return 0;
}
