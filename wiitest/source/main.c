/* test_python_embed.c */
#include <Python.h>
#include <my_text_renderer.h>
#include <stdlib.h>
#include <string.h>

#include <gccore.h>

extern const unsigned char test_py[];
extern const unsigned char test_py_end[];

//#define PNG
#include <fat2.h>
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

void wait(int ms) {
    for (int i = 0; i < ms; ++i) {
        VIDEO_WaitVSync();
    }
}

/* Minimal stdout/stderr bridge for Python 3 (PyFile_SetTerminalPrintHook was removed) */
static PyObject *wiiio_write(PyObject *self, PyObject *args) {
    (void)self;
    PyObject *obj;
    const char *data = NULL;
    Py_ssize_t len = 0;
    PyObject *tmp = NULL;

    if (!PyArg_ParseTuple(args, "O", &obj)) {
        return NULL;
    }

    if (PyUnicode_Check(obj)) {
        data = PyUnicode_AsUTF8AndSize(obj, &len);
        if (!data) return NULL;
    } else if (PyBytes_Check(obj)) {
        if (PyBytes_AsStringAndSize(obj, (char **)&data, &len) < 0) return NULL;
    } else {
        tmp = PyObject_Str(obj);
        if (!tmp) return NULL;
        data = PyUnicode_AsUTF8AndSize(tmp, &len);
        if (!data) {
            Py_DECREF(tmp);
            return NULL;
        }
    }

    if (len > 0 && data) {
        char *buf = (char *)PyMem_Malloc((size_t)len + 1);
        if (!buf) {
            Py_XDECREF(tmp);
            return PyErr_NoMemory();
        }
        memcpy(buf, data, (size_t)len);
        buf[len] = '\0';
        terminal_print(buf);
        PyMem_Free(buf);
    }

    Py_XDECREF(tmp);
    return PyLong_FromSsize_t(len);
}

static PyObject *wiiio_flush(PyObject *self, PyObject *args) {
    (void)self;
    (void)args;
    Py_RETURN_NONE;
}

static PyMethodDef WiiIOMethods[] = {
    {"write", wiiio_write, METH_VARARGS, "Write to Wii terminal"},
    {"flush", wiiio_flush, METH_VARARGS, "Flush (no-op)"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef WiiIOModule = {
    PyModuleDef_HEAD_INIT,
    "wiiio",
    NULL,
    -1,
    WiiIOMethods,
    NULL,
    NULL,
    NULL,
    NULL
};

int main(void) {
    
    char *script;
    size_t script_len;
    int rc;
    #ifdef PNG
    char msg[96];
    #endif

    video_init_custom();
    terminal_print("start");
    fatInitDefault();
    wait(100);

    #ifdef PNG
    if (!fatInitDefault()) {
        terminal_print("fatInitDefault failed before PNG export");
    }

    terminal_print("load test.png ...");
    //rc = export_png_to_sd("sd:/test.png", test_png, test_png_end);
    //snprintf(msg, sizeof(msg), "export_test_png_to_sd rc=%d", rc);
    //terminal_print(msg);
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

    terminal_print("0.1  initalise ...");
    wait(100);
    PyConfig config;
    terminal_print("0.2  before PyConfig_InitIsolatedConfig");
    wait(100);
    PyConfig_InitIsolatedConfig(&config);
    config.use_environment = 0;
    config.site_import = 0;
    config.user_site_directory = 0;
    config.install_signal_handlers = 0;
    config.use_hash_seed = 1;
    config.hash_seed = 0;
    //config.utf8_mode = 1;
    terminal_print("0.3   after PyConfig_InitIsolatedConfig");

    terminal_print("1.1");
    // Minimal filesystem/stdlib setup (match your SD layout)
    PyConfig_SetString(&config, &config.program_name, L"python");
    PyConfig_SetString(&config, &config.home, L"sd:/python");
    terminal_print("1.2");
    config.module_search_paths_set = 1;
    PyWideStringList_Append(&config.module_search_paths, L"sd:/python");
    PyWideStringList_Append(&config.module_search_paths, L"sd:/python/Lib");
    PyWideStringList_Append(&config.module_search_paths, L"sd:/python/lib/");
    terminal_print("1.3");
    PyConfig_SetString(&config, &config.filesystem_encoding, L"utf-8");
    PyConfig_SetString(&config, &config.filesystem_errors, L"surrogatepass");
    terminal_print("1.4   before Py_InitializeFromConfig");
    wait(100);
    {
        terminal_print("1.5");
        wait(100);
        PyStatus status = Py_InitializeFromConfig(&config);
        terminal_print("2");
        wait(100);
        if (PyStatus_Exception(status)) {
            terminal_print("3");
            terminal_print("3.5 Py_InitializeFromConfig failed");
            if (status.func != NULL) {
                terminal_print("4");
                terminal_print(status.func);
            }
            terminal_print("5");
            if (status.err_msg != NULL) {
                terminal_print("6");
                terminal_print(status.err_msg);
            }
            terminal_print("7");
            PyConfig_Clear(&config);
            terminal_print("8");
            return 1;
        }
        terminal_print("9");
    }
    terminal_print("10");
    PyConfig_Clear(&config);
    terminal_print("11   after Py_Initialize");
    //wait(100);
    if (!Py_IsInitialized()) {
        terminal_print("12 --- Py initalized == false: exiting ...");
        return 1;
    }
    //wait(100);
    terminal_print("13 --- Py_IsInitialized == true");
    //wait(100);
    {
        PyObject *wiiio = PyModule_Create(&WiiIOModule);
        if (wiiio) {
            PySys_SetObject("stdout", wiiio);
            PySys_SetObject("stderr", wiiio);
            Py_DECREF(wiiio);
            terminal_print("terminal hook set");
        } else {
            terminal_print("terminal hook failed");
        }
    }
    //wait(100);

    script_len = (size_t)(test_py_end - test_py);
    script = (char *)malloc(script_len + 1);
    if (script == NULL) {
        terminal_print("malloc failed: exiting ...");
        Py_Finalize();
        return 1;
    }
    //wait(100);
    memcpy(script, test_py, script_len);
    script[script_len] = '\0';

    //wait(100);
    terminal_print("run source_py/test.py ...");
    //wait(100);
    rc = PyRun_SimpleString(script);
    terminal_print("after PyRun_SimpleString");
    //wait(100);
    free(script);
    if (rc != 0) {
        terminal_print("python script returned error");
    }
    //wait(100);

    terminal_print("py_finalize: return 0: exiting ...");
    //wait(100);
    Py_Finalize();
    terminal_print("done");
    return 0;
}
