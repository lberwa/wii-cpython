/* test_python_embed.c */
#include <Python.h>

#include "../../bitmap/include/render_text.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <gccore.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/file.h>

extern const unsigned char test_py[];
extern const unsigned char test_py_end[];

 #define ON_DISPLAY

#ifdef ON_DISPLAY
 #define PRINT terminal_print
#else
 #define PRINT SYS_Report
#endif

#include <fat.h>

void wait(int time) {
    for (int i = 0; i < time; ++i) {
        VIDEO_WaitVSync();
    }
}


int main(void) {

    char *script;
    size_t script_len;
    int rc;

    video_init_custom();
    terminal_print("start");

    SYS_STDIO_Report(true); // printf -> Dolphin LOG

    if (!fatInitDefault()) {
        PRINT("fatInitDefault fehlgeschlagen!\n");
        return 1;
    }

    size_t count = 1;

    /* Pass RELATIVE paths only: Py_Init_Custom prepends the active device
     * ({dev}:/) and appends /lib for the import path, and resolves {dev}
     * (sd/usb) from availability + {dev}:/python/config.ini.
     *   "python"            -> import path  {dev}:/python/lib
     *   "python/symbols.map" -> symbol map  {dev}:/python/symbols.map  */
    PyStatus status = Py_Init_Custom((const char*[]){ "python" }, &count, "python/symbols.map");

    if (status._type != _PyStatus_TYPE_OK) {
        // Init ist fehlgeschlagen
        PRINT("Init failed in:");
        PRINT(status.func);
        PRINT(status.err_msg);
        return status.exitcode;  // ggf. Programm beenden
    }

    // run the script as a string, because the script is 
    // embedded in the binary, not on the SD card.
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
