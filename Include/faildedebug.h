
/*#ifndef PYFAILDEDEBUG
#define PYFAILDEDEBUG
#include <stdio.h>
#include <stdlib.h>
#include "Python.h"

#ifdef Py_REF_DEBUG

#ifndef _Py_NegativeRefcount
void _Py_NegativeRefcount(const char *filename, int lineno, PyObject *op)
{
    // optional: debug output 
}
#endif

#ifndef _Py_INCREF_IncRefTotal
void _Py_INCREF_IncRefTotal(void)
{
}
#endif

#ifndef _Py_DECREF_DecRefTotal
void _Py_DECREF_DecRefTotal(void)
{
}
#endif

#endif

#endif*/