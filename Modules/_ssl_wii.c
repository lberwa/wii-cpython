/* _ssl stub for Wii — provides enough for "import ssl" to succeed.
 * Real SSL socket operations raise NotImplementedError.
 * RAND_bytes backed by PSA Crypto (mbedTLS 3.x).
 */

#include "Python.h"
#include "psa/crypto.h"

#define _NOT_IMPL(name)  do { \
    PySys_WriteStdout(name " not implemented\n"); \
    PyErr_SetString(PyExc_NotImplementedError, name " not implemented on Wii"); \
    return NULL; \
} while(0)

/* ---------------------------------------------------------------- exception objects */
static PyObject *PySSLErrorObject;
static PyObject *PySSLZeroReturnErrorObject;
static PyObject *PySSLWantReadErrorObject;
static PyObject *PySSLWantWriteErrorObject;
static PyObject *PySSLSyscallErrorObject;
static PyObject *PySSLEOFErrorObject;
static PyObject *PySSLCertVerificationErrorObject;

/* ================================================================ _SSLContext */

typedef struct { PyObject_HEAD } PySSLContextObject;

static PyObject *sslctx_new(PyTypeObject *t, PyObject *a, PyObject *k) {
    return t->tp_alloc(t, 0);
}

#define STUB_METHOD(cls, name) \
    static PyObject *cls##_##name(PyObject *self, PyObject *args) \
    { (void)self;(void)args; _NOT_IMPL(#cls "." #name); }

STUB_METHOD(sslctx, set_ciphers)
STUB_METHOD(sslctx, set_alpn_protocols)
STUB_METHOD(sslctx, set_npn_protocols)
STUB_METHOD(sslctx, set_servername_callback)
STUB_METHOD(sslctx, load_cert_chain)
STUB_METHOD(sslctx, load_dh_params)
STUB_METHOD(sslctx, load_verify_locations)
STUB_METHOD(sslctx, set_default_verify_paths)
STUB_METHOD(sslctx, wrap_socket)
STUB_METHOD(sslctx, _wrap_bio)
STUB_METHOD(sslctx, get_ca_certs)
STUB_METHOD(sslctx, cert_store_stats)
STUB_METHOD(sslctx, set_psk_client_callback)
STUB_METHOD(sslctx, set_psk_server_callback)

static PyMethodDef sslctx_methods[] = {
    {"set_ciphers",              sslctx_set_ciphers,              METH_VARARGS, NULL},
    {"set_alpn_protocols",       sslctx_set_alpn_protocols,       METH_VARARGS, NULL},
    {"set_npn_protocols",        sslctx_set_npn_protocols,        METH_VARARGS, NULL},
    {"set_servername_callback",  sslctx_set_servername_callback,  METH_VARARGS, NULL},
    {"load_cert_chain",          sslctx_load_cert_chain,          METH_VARARGS, NULL},
    {"load_dh_params",           sslctx_load_dh_params,           METH_VARARGS, NULL},
    {"load_verify_locations",    sslctx_load_verify_locations,    METH_VARARGS, NULL},
    {"set_default_verify_paths", sslctx_set_default_verify_paths, METH_NOARGS,  NULL},
    {"wrap_socket",              sslctx_wrap_socket,              METH_VARARGS|METH_KEYWORDS, NULL},
    {"_wrap_bio",                sslctx__wrap_bio,                METH_VARARGS|METH_KEYWORDS, NULL},
    {"get_ca_certs",             sslctx_get_ca_certs,             METH_VARARGS|METH_KEYWORDS, NULL},
    {"cert_store_stats",         sslctx_cert_store_stats,         METH_NOARGS,  NULL},
    {"set_psk_client_callback",  sslctx_set_psk_client_callback,  METH_VARARGS, NULL},
    {"set_psk_server_callback",  sslctx_set_psk_server_callback,  METH_VARARGS, NULL},
    {NULL, NULL}
};

static PyObject *sslctx_getattr(PyObject *self, PyObject *name)
{
    const char *n = PyUnicode_AsUTF8(name);
    if (!n) return NULL;
    if (!strcmp(n,"check_hostname"))  return PyLong_FromLong(0);
    if (!strcmp(n,"verify_mode"))     return PyLong_FromLong(0);
    if (!strcmp(n,"verify_flags"))    return PyLong_FromLong(0);
    if (!strcmp(n,"options"))         return PyLong_FromLong(0);
    if (!strcmp(n,"protocol"))        return PyLong_FromLong(2);
    if (!strcmp(n,"minimum_version")) return PyLong_FromLong(0x0303);
    if (!strcmp(n,"maximum_version")) return PyLong_FromLong(0x0304);
    return PyObject_GenericGetAttr(self, name);
}

static int sslctx_setattr(PyObject *self, PyObject *name, PyObject *v)
{
    (void)self;(void)name;(void)v;
    return 0;
}

static PyTypeObject PySSLContext_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "_ssl._SSLContext",
    .tp_basicsize = sizeof(PySSLContextObject),
    .tp_new       = sslctx_new,
    .tp_methods   = sslctx_methods,
    .tp_getattro  = sslctx_getattr,
    .tp_setattro  = sslctx_setattr,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
};

/* ================================================================ MemoryBIO */

typedef struct { PyObject_HEAD } PySSLMemoryBIO;

static PyObject *membio_new(PyTypeObject *t, PyObject *a, PyObject *k)
{ return t->tp_alloc(t, 0); }

STUB_METHOD(membio, read)
STUB_METHOD(membio, write)

static PyMethodDef membio_methods[] = {
    {"read",  membio_read,  METH_VARARGS, NULL},
    {"write", membio_write, METH_VARARGS, NULL},
    {NULL, NULL}
};

static PyTypeObject PySSLMemoryBIO_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "_ssl.MemoryBIO",
    .tp_basicsize = sizeof(PySSLMemoryBIO),
    .tp_new       = membio_new,
    .tp_methods   = membio_methods,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
};

/* ================================================================ SSLSession */

typedef struct { PyObject_HEAD } PySSLSession;

static PyObject *sslsession_new(PyTypeObject *t, PyObject *a, PyObject *k)
{ return t->tp_alloc(t, 0); }

static PyTypeObject PySSLSession_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "_ssl.SSLSession",
    .tp_basicsize = sizeof(PySSLSession),
    .tp_new       = sslsession_new,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
};

/* ================================================================ module functions */

static PyObject *_ssl_RAND_status(PyObject *m, PyObject *a)
{ (void)m;(void)a; return PyLong_FromLong(1); }

static PyObject *_ssl_RAND_add(PyObject *m, PyObject *a)
{ (void)m;(void)a; Py_RETURN_NONE; }

static PyObject *_ssl_RAND_bytes(PyObject *m, PyObject *args)
{
    int n;
    (void)m;
    if (!PyArg_ParseTuple(args, "i", &n)) return NULL;
    if (n < 0) { PyErr_SetString(PyExc_ValueError,"negative count"); return NULL; }
    PyObject *result = PyBytes_FromStringAndSize(NULL, n);
    if (!result) return NULL;
    psa_crypto_init();
    psa_status_t rc = psa_generate_random((uint8_t*)PyBytes_AS_STRING(result),(size_t)n);
    if (rc != PSA_SUCCESS) {
        Py_DECREF(result);
        PyErr_SetString(PyExc_OSError,"RAND_bytes: psa_generate_random failed");
        return NULL;
    }
    return result;
}

static PyObject *_ssl_txt2obj(PyObject *m, PyObject *a)
{ (void)m;(void)a; _NOT_IMPL("txt2obj"); }

static PyObject *_ssl_nid2obj(PyObject *m, PyObject *a)
{ (void)m;(void)a; _NOT_IMPL("nid2obj"); }

static PyObject *_ssl_get_sigalgs(PyObject *m, PyObject *a)
{ (void)m;(void)a; return PyList_New(0); }

static PyObject *_ssl_get_default_verify_paths(PyObject *m, PyObject *a)
{ (void)m;(void)a; return Py_BuildValue("(ssssss)","","","","","",""); }

static PyMethodDef _ssl_methods[] = {
    {"RAND_status",              _ssl_RAND_status,              METH_NOARGS,  NULL},
    {"RAND_add",                 _ssl_RAND_add,                 METH_VARARGS, NULL},
    {"RAND_bytes",               _ssl_RAND_bytes,               METH_VARARGS, NULL},
    {"txt2obj",                  _ssl_txt2obj,                  METH_VARARGS, NULL},
    {"nid2obj",                  _ssl_nid2obj,                  METH_VARARGS, NULL},
    {"get_sigalgs",              _ssl_get_sigalgs,              METH_VARARGS, NULL},
    {"get_default_verify_paths", _ssl_get_default_verify_paths, METH_NOARGS,  NULL},
    {NULL, NULL}
};

static struct PyModuleDef _sslmodule = {
    PyModuleDef_HEAD_INIT, "_ssl", NULL, -1, _ssl_methods
};

PyMODINIT_FUNC PyInit__ssl(void)
{
    PyObject *m = PyModule_Create(&_sslmodule);
    if (!m) return NULL;

#define ADD_EXC(var, name, base) \
    var = PyErr_NewException("ssl." name, base, NULL); \
    if (var) { Py_INCREF(var); PyModule_AddObject(m, name, var); }

    ADD_EXC(PySSLErrorObject,                "SSLError",                PyExc_OSError)
    ADD_EXC(PySSLZeroReturnErrorObject,      "SSLZeroReturnError",      PySSLErrorObject)
    ADD_EXC(PySSLWantReadErrorObject,        "SSLWantReadError",        PySSLErrorObject)
    ADD_EXC(PySSLWantWriteErrorObject,       "SSLWantWriteError",       PySSLErrorObject)
    ADD_EXC(PySSLSyscallErrorObject,         "SSLSyscallError",         PySSLErrorObject)
    ADD_EXC(PySSLEOFErrorObject,             "SSLEOFError",             PySSLErrorObject)
    ADD_EXC(PySSLCertVerificationErrorObject,"SSLCertVerificationError",PySSLErrorObject)
#undef ADD_EXC

    if (PyType_Ready(&PySSLContext_Type)   < 0) return NULL;
    if (PyType_Ready(&PySSLMemoryBIO_Type) < 0) return NULL;
    if (PyType_Ready(&PySSLSession_Type)   < 0) return NULL;
    Py_INCREF(&PySSLContext_Type);
    Py_INCREF(&PySSLMemoryBIO_Type);
    Py_INCREF(&PySSLSession_Type);
    PyModule_AddObject(m, "_SSLContext", (PyObject*)&PySSLContext_Type);
    PyModule_AddObject(m, "MemoryBIO",   (PyObject*)&PySSLMemoryBIO_Type);
    PyModule_AddObject(m, "SSLSession",  (PyObject*)&PySSLSession_Type);

#define AI(k,v) PyModule_AddIntConstant(m,k,v)
#define AS(k,v) PyModule_AddStringConstant(m,k,v)
    AI("OPENSSL_VERSION_NUMBER", 0x10101000L);
    AS("OPENSSL_VERSION", "mbedTLS (Wii stub)");
    PyModule_AddObject(m,"OPENSSL_VERSION_INFO", Py_BuildValue("(iiiii)",1,1,1,0,0));
    PyModule_AddObject(m,"_OPENSSL_API_VERSION",  Py_BuildValue("(iii)",1,1,1));
    AS("_DEFAULT_CIPHERS","");

    AI("HAS_SNI",0); AI("HAS_ECDH",0); AI("HAS_NPN",0); AI("HAS_ALPN",0);
    AI("HAS_SSLv2",0); AI("HAS_SSLv3",0); AI("HAS_TLSv1",0);
    AI("HAS_TLSv1_1",0); AI("HAS_TLSv1_2",0); AI("HAS_TLSv1_3",0);
    AI("HAS_PSK",0); AI("HAS_PSK_TLS13",0); AI("HAS_PHA",0);

    AI("PROTO_MINIMUM_SUPPORTED",-2); AI("PROTO_MAXIMUM_SUPPORTED",-1);
    AI("PROTO_SSLv3",0x0300); AI("PROTO_TLSv1",0x0301);
    AI("PROTO_TLSv1_1",0x0302); AI("PROTO_TLSv1_2",0x0303);
    AI("PROTO_TLSv1_3",0x0304);

    AI("HOSTFLAG_NEVER_CHECK_SUBJECT",0x02);
    AI("VERIFY_DEFAULT",0); AI("VERIFY_CRL_CHECK_LEAF",0x04);
    AI("VERIFY_CRL_CHECK_CHAIN",0x0c); AI("VERIFY_X509_STRICT",0x20);
    AI("VERIFY_ALLOW_PROXY_CERTS",0x40); AI("VERIFY_X509_PARTIAL_CHAIN",0x80000);
    AI("ENCODING_DER",1); AI("ENCODING_PEM",2);

    AI("SSL_ERROR_ZERO_RETURN",6); AI("SSL_ERROR_WANT_READ",2);
    AI("SSL_ERROR_WANT_WRITE",3); AI("SSL_ERROR_WANT_X509_LOOKUP",4);
    AI("SSL_ERROR_SYSCALL",5); AI("SSL_ERROR_SSL",1);
    AI("SSL_ERROR_WANT_CONNECT",7); AI("SSL_ERROR_EOF",8);
    AI("SSL_ERROR_INVALID_ERROR_CODE",10);

    AI("OP_ALL",0); AI("OP_NO_SSLv2",0); AI("OP_NO_SSLv3",0);
    AI("OP_NO_TLSv1",0); AI("OP_NO_TLSv1_1",0);
    AI("OP_NO_TLSv1_2",0); AI("OP_NO_TLSv1_3",0);
    AI("OP_CIPHER_SERVER_PREFERENCE",0); AI("OP_SINGLE_DH_USE",0);
    AI("OP_SINGLE_ECDH_USE",0); AI("OP_NO_COMPRESSION",0);
    AI("OP_NO_RENEGOTIATION",0); AI("OP_ENABLE_MIDDLEBOX_COMPAT",0);
    AI("OP_IGNORE_UNEXPECTED_EOF",0); AI("OP_LEGACY_SERVER_CONNECT",0);
    AI("OP_ENABLE_KTLS",0);

    AI("CERT_NONE",0); AI("CERT_OPTIONAL",1); AI("CERT_REQUIRED",2);
    AI("PROTOCOL_TLS",2); AI("PROTOCOL_TLS_CLIENT",16);
    AI("PROTOCOL_TLS_SERVER",17); AI("PROTOCOL_SSLv23",2);
    AI("ALERT_DESCRIPTION_HANDSHAKE_FAILURE",40);
    AI("ALERT_DESCRIPTION_CERTIFICATE_EXPIRED",45);
#undef AI
#undef AS

    return m;
}
