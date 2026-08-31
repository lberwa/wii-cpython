/* spam.c -> spam.so : Beispiel einer echten Python-C-Extension, die zur
 * Laufzeit per `import spam` ueber den Wii-dlopen-Loader geladen wird.
 *
 * Baurezept (siehe wiitest/Makefile, Ziel spam.so):
 *   powerpc-eabi-gcc -fPIC -fno-plt -shared -nostdlib <Python-Includes> spam.c -o spam.so
 *
 * Die im Modul undefinierten CPython-API-Symbole (PyArg_ParseTuple,
 * PyLong_FromLong, PyModule_Create2, ...) werden ueber die Symbol-Map
 * (sd:/symbols.map) aufgeloest, die das Hauptprogramm beim Start laedt.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

static PyObject *
spam_add(PyObject *self, PyObject *args)
{
    (void)self;
    int a, b;
    if (!PyArg_ParseTuple(args, "ii", &a, &b))
        return NULL;
    return PyLong_FromLong((long)a + (long)b);
}

static PyObject *
spam_greet(PyObject *self, PyObject *args)
{
    (void)self; (void)args;
    return PyUnicode_FromString("hi from spam.so");
}

static PyMethodDef spam_methods[] = {
    {"add",   spam_add,   METH_VARARGS, "add(a, b) -> a + b"},
    {"greet", spam_greet, METH_NOARGS,  "greet() -> str"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef spam_module = {
    PyModuleDef_HEAD_INIT,
    "spam",
    "Wii dlopen Demo-Modul",
    -1,
    spam_methods,
    NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit_spam(void)
{
    return PyModule_Create(&spam_module);
}
