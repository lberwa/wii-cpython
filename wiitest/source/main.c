/* test_python_embed.c */
#include <Python.h>
#include "../../bitmap/include/render_text.h"
#include <stdlib.h>
#include <string.h>

#include <gccore.h>

extern const unsigned char test_py[];
extern const unsigned char test_py_end[];

// #define FIND_FILE3
// #define FIND_FILE2
// #define FIND_FILE
// #define PNG

 #define ON_DISPLAY

#ifdef ON_DISPLAY
 #define PRINT terminal_print
#else
 #define PRINT SYS_Report
#endif

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
    PRINT("1");

    f = fopen(dst_path, "wb");
    if (!f) {
        return -2; /* open failed */
    }

    PRINT("2");

    written = fwrite(start, 1, len, f);
    fclose(f);

    PRINT("3");

    if (written != len) {
        return -3; /* write failed */
    }
    PRINT("4");
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
        //PRINT("Kann %s nicht öffnen\n", path);
        PRINT("Kann");
        PRINT(path);
        PRINT("nicht öffnen\n");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        for (int i = 0; i < level; i++) PRINT("  "); // Einrückung
//        PRINT("%s%s\n", entry->d_name,
//                       (entry->d_type == DT_DIR) ? "/" : "");
        PRINT(entry->d_name);
        PRINT((entry->d_type == DT_DIR) ? "/" : "");

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

    SYS_STDIO_Report(true); // printf -> Dolphin LOG

    #ifdef PNG
    if (!fatInitDefault()) {
        PRINT("fatInitDefault failed before PNG export");
    }

    PRINT("load test.png ...");
    if (rc != 0) {
        PRINT("PNG export failed; sd:/test.png may be missing");
    }

    terminal_prPRINTint("load brew.png ...");
    rc = export_png_to_sd("sd:/brew.png", brew_png, brew_png_end);
    snprintf(msg, sizeof(msg), "export_brew_png_to_sd rc=%d", rc);
    PRINT(msg);
    if (rc != 0) {
        PRINT("PNG export failed; sd:/brew.png may be missing");
    }
    #endif

    if (!fatInitDefault()) {
        PRINT("fatInitDefault fehlgeschlagen!\n");
        return 1;
    }

#ifdef FIND_FILE2
    PRINT("start 2\n");

    // Datei auf der SD-Karte erstellen / überschreiben
    FILE *f = fopen("sd:/text.txt", "w");
    if (!f) {
        PRINT("Fehler beim Öffnen der Datei!\n");
        return 1;
    }

    // Inhalt schreiben
    const char *text = "Hallo Welt!";
    fprintf(f, "%s\n", text);

    fclose(f);
    PRINT("Datei erfolgreich geschrieben.\n");
    PRINT("-----------------------");

    PRINT("Inhalt von SD-Karte:\n");
    list_dir("sd:", 0);
    PRINT("-----------------------");

    wait(300); // 5 Sekunden warten
#endif

#ifdef FIND_FILE





FILE *fp = fopen("sd:/curl_test.py", "r");

if (!fp) {
    PRINT("Datei nicht gefunden\n");
    return 1;
}

PRINT("Inhalt:\n--------------------\n");

char line[512];
int line_num = 0;

while (fgets(line, sizeof(line), fp)) {

    char out[600];

    sprintf(out, "%d: %s", line_num, line);

    PRINT(out);

    line_num++;
}

fclose(fp);

PRINT("Fertig.\n");

wait(300); // 5 Sekunden warten

#endif

#ifdef FIND_FILE3
    // Datei öffnen
    FILE *f = fopen("sd:/curl_test.py", "r");
    if (!f)
    {
        PRINT("Fehler: Datei konnte nicht geöffnet werden\n");
        return 1;
    }

    PRINT("Inhalt von sd:/test.txt:");

    // Datei zeilenweise lesen
    char buffer[1000000];
    while (fgets(buffer, sizeof(buffer), f))
    {
        PRINT(buffer);
    }

    // Datei schließen
    fclose(f);

    PRINT("\n\nFertig.\n");
#endif

    size_t count = 1;

    //Py_Initalize_Custom(NULL, NULL); 
    PyStatus status = Py_Init_Custom((const char*[]){ "sd:/", "sd:"}, &count);

    if (status._type != _PyStatus_TYPE_OK) {
        // Init ist fehlgeschlagen
        PRINT("Init failed in:");
        PRINT(status.func);
        PRINT(status.err_msg);
        return status.exitcode;  // ggf. Programm beenden
    }
    
    script_len = (size_t)(test_py_end - test_py);
    script = (char *)malloc(script_len + 1);
    if (script == NULL) {
        PRINT("malloc failed: exiting ...");
        Py_Finalize();
        return 1;
    }

    memcpy(script, test_py, script_len);
    script[script_len] = '\0';

    PRINT("run source_py/test.py ...");
    rc = PyRun_SimpleString(script);

    free(script);
    if (rc != 0) {
        //PRINT("python script returned error rc=%d\n", rc);
        if (PyErr_Occurred()) {
            PyErr_Print();
        } else {
            PRINT("PyErr_Occurred() == false\n");
        }
        PRINT("python script returned error");
        wait(600);
    }

    Py_Finalize();

    terminal_print("done");
    return 0;
}
