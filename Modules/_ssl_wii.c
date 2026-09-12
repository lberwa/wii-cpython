/* _ssl for Wii — real TLS via mbedTLS 4.0 (PSA-RNG).
 * wrap_socket: client- and server-side TLS with configurable CA/cert/key.
 * MemoryBIO: real in-memory buffer with pair-BIO callbacks.
 * RAND_bytes backed by PSA Crypto.
 */

#include "Python.h"
/* Allow access to MBEDTLS_PRIVATE fields needed for sigalg extraction. */
#define MBEDTLS_ALLOW_PRIVATE_ACCESS
#include "mbedtls/ssl.h"
#include "mbedtls/ssl_ciphersuites.h"
#include "mbedtls/x509_crt.h"
#undef MBEDTLS_ALLOW_PRIVATE_ACCESS
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "mbedtls/pem.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "psa/crypto.h"
#include <network.h>   /* net_send / net_recv */
#include <string.h>    /* memcpy / memmove / strlen */
#include <ctype.h>     /* toupper */

/* ---------------------------------------------------------------- exception objects */
static PyObject *PySSLErrorObject;
static PyObject *PySSLZeroReturnErrorObject;
static PyObject *PySSLWantReadErrorObject;
static PyObject *PySSLWantWriteErrorObject;
static PyObject *PySSLSyscallErrorObject;
static PyObject *PySSLEOFErrorObject;
static PyObject *PySSLCertVerificationErrorObject;

#define _NOT_IMPL(name) do { \
    PyErr_SetString(PyExc_NotImplementedError, name " not implemented on Wii"); \
    return NULL; \
} while(0)

/* ---------------------------------------------------------------- BIO callbacks (libogc TCP) */

static int wii_ssl_send(void *ctx, const unsigned char *buf, size_t len)
{
    int fd = *(int *)ctx;
    s32 r = net_send(fd, buf, (s32)len, 0);
    return (r < 0) ? MBEDTLS_ERR_NET_SEND_FAILED : (int)r;
}

static int wii_ssl_recv(void *ctx, unsigned char *buf, size_t len)
{
    int fd = *(int *)ctx;
    s32 r = net_recv(fd, buf, (s32)len, 0);
    if (r < 0) return MBEDTLS_ERR_NET_RECV_FAILED;
    if (r == 0) return MBEDTLS_ERR_NET_CONN_RESET;
    return (int)r;
}

/* ================================================================ forward declarations */

static PyTypeObject PySSLContext_Type;
static PyTypeObject PyWiiSSL_Type;
static PyTypeObject PySSLMemoryBIO_Type;
static PyTypeObject PySSLSession_Type;

/* SSLSession forward typedef — full definition follows _WiiSSLObject */
typedef struct {
    PyObject_HEAD
    mbedtls_ssl_session session;
    int initialized;
} PySSLSession;

static PyObject *sslsession_new(PyTypeObject *t, PyObject *a, PyObject *k);
/* SNI and PSK C callbacks — defined after SSLContext methods */
static int wii_sni_callback(void *, mbedtls_ssl_context *, const unsigned char *, size_t);
static int wii_psk_server_cb(void *, mbedtls_ssl_context *, const unsigned char *, size_t);

/* ================================================================ MemoryBIO */

#define MEMBIO_CAP_INIT 4096

typedef struct {
    PyObject_HEAD
    unsigned char *buf;
    size_t         len;   /* bytes in buffer */
    size_t         cap;
    int            eof;
} PySSLMemoryBIO;

static PyObject *membio_new(PyTypeObject *t, PyObject *a, PyObject *k)
{
    (void)a; (void)k;
    PySSLMemoryBIO *self = (PySSLMemoryBIO *)t->tp_alloc(t, 0);
    if (!self) return NULL;
    self->buf = (unsigned char *)PyMem_Malloc(MEMBIO_CAP_INIT);
    if (!self->buf) { Py_DECREF(self); return PyErr_NoMemory(); }
    self->len = 0; self->cap = MEMBIO_CAP_INIT; self->eof = 0;
    return (PyObject *)self;
}

static void membio_dealloc(PyObject *self_)
{
    PySSLMemoryBIO *self = (PySSLMemoryBIO *)self_;
    PyMem_Free(self->buf);
    Py_TYPE(self)->tp_free(self);
}

/* write(data) — append bytes to buffer */
static PyObject *membio_write(PyObject *self_, PyObject *args)
{
    PySSLMemoryBIO *self = (PySSLMemoryBIO *)self_;
    Py_buffer data;
    if (!PyArg_ParseTuple(args, "y*", &data)) return NULL;
    if (self->eof) {
        PyBuffer_Release(&data);
        PyErr_SetString(PyExc_EOFError, "MemoryBIO is in EOF state");
        return NULL;
    }
    /* grow buffer if needed */
    if (self->len + (size_t)data.len > self->cap) {
        size_t newcap = self->cap;
        while (newcap < self->len + (size_t)data.len) newcap *= 2;
        unsigned char *nb = (unsigned char *)PyMem_Realloc(self->buf, newcap);
        if (!nb) { PyBuffer_Release(&data); return PyErr_NoMemory(); }
        self->buf = nb; self->cap = newcap;
    }
    memcpy(self->buf + self->len, data.buf, (size_t)data.len);
    self->len += (size_t)data.len;
    Py_ssize_t written = (Py_ssize_t)data.len;
    PyBuffer_Release(&data);
    return PyLong_FromSsize_t(written);
}

/* write_eof() — signal no more data */
static PyObject *membio_write_eof(PyObject *self_, PyObject *args)
{
    (void)args;
    ((PySSLMemoryBIO *)self_)->eof = 1;
    Py_RETURN_NONE;
}

/* read([n]) — consume up to n bytes */
static PyObject *membio_read(PyObject *self_, PyObject *args)
{
    PySSLMemoryBIO *self = (PySSLMemoryBIO *)self_;
    int n = -1;
    if (!PyArg_ParseTuple(args, "|i", &n)) return NULL;
    size_t to_read = (n < 0 || (size_t)n > self->len) ? self->len : (size_t)n;
    PyObject *result = PyBytes_FromStringAndSize((char *)self->buf, (Py_ssize_t)to_read);
    if (!result) return NULL;
    /* shift remaining data */
    memmove(self->buf, self->buf + to_read, self->len - to_read);
    self->len -= to_read;
    return result;
}

static PyObject *membio_getattr(PyObject *self_, PyObject *name)
{
    PySSLMemoryBIO *self = (PySSLMemoryBIO *)self_;
    const char *n = PyUnicode_AsUTF8(name);
    if (n && !strcmp(n, "pending")) return PyLong_FromSize_t(self->len);
    if (n && !strcmp(n, "eof"))     return PyBool_FromLong(self->eof && self->len == 0);
    return PyObject_GenericGetAttr(self_, name);
}

static PyMethodDef membio_methods[] = {
    {"read",      membio_read,      METH_VARARGS, NULL},
    {"write",     membio_write,     METH_VARARGS, NULL},
    {"write_eof", membio_write_eof, METH_NOARGS,  NULL},
    {NULL, NULL}
};

static PyTypeObject PySSLMemoryBIO_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "_ssl.MemoryBIO",
    .tp_basicsize = sizeof(PySSLMemoryBIO),
    .tp_dealloc   = membio_dealloc,
    .tp_new       = membio_new,
    .tp_methods   = membio_methods,
    .tp_getattro  = membio_getattr,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
};

/* ---------------------------------------------------------------- MemoryBIO pair BIO callbacks */

typedef struct { PySSLMemoryBIO *in; PySSLMemoryBIO *out; } MemBIOPair;

static int wii_bio_recv_raw(PySSLMemoryBIO *bio, unsigned char *buf, size_t len)
{
    if (bio->len == 0) {
        if (bio->eof) return MBEDTLS_ERR_SSL_CONN_EOF;
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    size_t to_read = (len < bio->len) ? len : bio->len;
    memcpy(buf, bio->buf, to_read);
    memmove(bio->buf, bio->buf + to_read, bio->len - to_read);
    bio->len -= to_read;
    return (int)to_read;
}

static int wii_bio_send_raw(PySSLMemoryBIO *bio, const unsigned char *buf, size_t len)
{
    if (bio->len + len > bio->cap) {
        size_t newcap = bio->cap;
        while (newcap < bio->len + len) newcap *= 2;
        unsigned char *nb = (unsigned char *)PyMem_Realloc(bio->buf, newcap);
        if (!nb) return MBEDTLS_ERR_NET_SEND_FAILED;
        bio->buf = nb; bio->cap = newcap;
    }
    memcpy(bio->buf + bio->len, buf, len);
    bio->len += len;
    return (int)len;
}

static int wii_bio_pair_send(void *ctx, const unsigned char *buf, size_t len)
{
    MemBIOPair *p = (MemBIOPair *)ctx;
    return wii_bio_send_raw(p->out, buf, len);
}

static int wii_bio_pair_recv(void *ctx, unsigned char *buf, size_t len)
{
    MemBIOPair *p = (MemBIOPair *)ctx;
    return wii_bio_recv_raw(p->in, buf, len);
}

/* ================================================================ struct definitions */

typedef struct {
    PyObject_HEAD
    mbedtls_ssl_config  conf;
    mbedtls_x509_crt    cacert;
    mbedtls_x509_crt    owncert;
    mbedtls_pk_context  ownkey;
    int verify_mode;     /* ssl.CERT_NONE/OPTIONAL/REQUIRED  →  MBEDTLS_SSL_VERIFY_* */
    int check_hostname;
    int cacert_loaded;
    int owncert_loaded;
    int conf_initialized;
    int endpoint;        /* MBEDTLS_SSL_IS_CLIENT or MBEDTLS_SSL_IS_SERVER */
    /* ALPN storage — pointers must stay valid for the lifetime of conf */
    char alpn_buf[8][32];   /* up to 8 ALPN protocol names, max 31 chars each */
    const char *alpn_list[9]; /* null-terminated list for mbedTLS */
    int alpn_count;
    /* set_ciphers — null-terminated int array for mbedtls_ssl_conf_ciphersuites */
    int ciphersuites[65];   /* up to 64 suite IDs + 0 terminator */
    /* load_dh_params → nearest FFDHE group via mbedtls_ssl_conf_groups */
    uint16_t dh_group_id;   /* 0 = not set */
    uint16_t groups_list[10]; /* EC groups + FFDHE group, null-terminated */
    /* SNI server callback */
    PyObject *sni_callback;
    /* PSK */
    unsigned char psk_buf[256];
    size_t        psk_len;
    unsigned char psk_identity_buf[128];
    size_t        psk_identity_len;
    PyObject     *psk_server_callback;  /* server-side PSK lookup callable */
} PySSLContextObject;

typedef struct {
    PyObject_HEAD
    mbedtls_ssl_context  ssl;
    int                  fd;
    PySSLContextObject  *ctx;
    int                  connected;
    int                  session_was_resumed;  /* set=True before handshake → resumed */
    /* MemoryBIO pair (used when fd == -1) */
    PySSLMemoryBIO      *incoming;
    PySSLMemoryBIO      *outgoing;
    MemBIOPair           bio_pair;
} PyWiiSSLObject;

/* ================================================================ _SSLContext helpers */

/* Initialize (or re-verify) the ssl_config for the requested endpoint.
 * Returns 0 on success, -1 on endpoint mismatch (caller sets error),
 * or a positive mbedTLS error code. */
static int sslctx_ensure_conf(PySSLContextObject *self, int server_side)
{
    if (self->conf_initialized) {
        /* Already set — check for endpoint mismatch */
        int want = server_side ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT;
        if (self->endpoint != want)
            return -1;  /* caller sets PyErr */
        return 0;
    }

    self->endpoint = server_side ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT;
    int ret = mbedtls_ssl_config_defaults(&self->conf, self->endpoint,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) return ret;

    mbedtls_ssl_conf_authmode(&self->conf,
        self->verify_mode == 0 ? MBEDTLS_SSL_VERIFY_NONE :
        self->verify_mode == 1 ? MBEDTLS_SSL_VERIFY_OPTIONAL :
                                  MBEDTLS_SSL_VERIFY_REQUIRED);

    if (self->cacert_loaded)
        mbedtls_ssl_conf_ca_chain(&self->conf, &self->cacert, NULL);
    if (self->owncert_loaded)
        mbedtls_ssl_conf_own_cert(&self->conf, &self->owncert, &self->ownkey);
    if (self->alpn_count > 0)
        mbedtls_ssl_conf_alpn_protocols(&self->conf, self->alpn_list);
    if (self->ciphersuites[0] != 0)
        mbedtls_ssl_conf_ciphersuites(&self->conf, self->ciphersuites);
    if (self->dh_group_id != 0)
        mbedtls_ssl_conf_groups(&self->conf, self->groups_list);
    if (self->psk_len > 0)
        mbedtls_ssl_conf_psk(&self->conf,
                              self->psk_buf,          self->psk_len,
                              self->psk_identity_buf, self->psk_identity_len);
    if (self->sni_callback)
        mbedtls_ssl_conf_sni(&self->conf, wii_sni_callback, self);
    if (self->psk_server_callback)
        mbedtls_ssl_conf_psk_cb(&self->conf, wii_psk_server_cb, self);

    self->conf_initialized = 1;
    return 0;
}

/* ================================================================ _SSLContext */

static PyObject *sslctx_new(PyTypeObject *t, PyObject *a, PyObject *k)
{
    (void)a; (void)k;
    PySSLContextObject *self = (PySSLContextObject *)t->tp_alloc(t, 0);
    if (!self) return NULL;

    mbedtls_ssl_config_init(&self->conf);
    mbedtls_x509_crt_init(&self->cacert);
    mbedtls_x509_crt_init(&self->owncert);
    mbedtls_pk_init(&self->ownkey);

    self->verify_mode      = 0;  /* CERT_NONE */
    self->check_hostname   = 0;
    self->cacert_loaded    = 0;
    self->owncert_loaded   = 0;
    self->conf_initialized = 0;
    self->endpoint         = MBEDTLS_SSL_IS_CLIENT;
    self->alpn_count       = 0;
    self->ciphersuites[0]  = 0;
    self->dh_group_id      = 0;
    self->groups_list[0]   = 0;
    self->sni_callback     = NULL;
    self->psk_len          = 0;
    self->psk_identity_len = 0;
    self->psk_server_callback = NULL;

    psa_crypto_init();  /* idempotent; required by mbedTLS 4.x */

    return (PyObject *)self;
}

static void sslctx_dealloc(PyObject *self_)
{
    PySSLContextObject *self = (PySSLContextObject *)self_;
    mbedtls_ssl_config_free(&self->conf);
    mbedtls_x509_crt_free(&self->cacert);
    mbedtls_x509_crt_free(&self->owncert);
    mbedtls_pk_free(&self->ownkey);
    Py_XDECREF(self->sni_callback);
    Py_XDECREF(self->psk_server_callback);
    Py_TYPE(self)->tp_free(self);
}

/* --- attribute access --- */

static PyObject *sslctx_getattr(PyObject *self_, PyObject *name)
{
    PySSLContextObject *self = (PySSLContextObject *)self_;
    const char *n = PyUnicode_AsUTF8(name);
    if (!n) return NULL;
    if (!strcmp(n, "check_hostname"))  return PyBool_FromLong(self->check_hostname);
    if (!strcmp(n, "verify_mode"))     return PyLong_FromLong(self->verify_mode);
    if (!strcmp(n, "verify_flags"))    return PyLong_FromLong(0);
    if (!strcmp(n, "options"))         return PyLong_FromLong(0);
    if (!strcmp(n, "protocol"))        return PyLong_FromLong(2);
    if (!strcmp(n, "minimum_version")) return PyLong_FromLong(0x0303);
    if (!strcmp(n, "maximum_version")) return PyLong_FromLong(0x0304);
    return PyObject_GenericGetAttr(self_, name);
}

static int sslctx_setattr(PyObject *self_, PyObject *name, PyObject *v)
{
    PySSLContextObject *self = (PySSLContextObject *)self_;
    const char *n = PyUnicode_AsUTF8(name);
    if (!n) return -1;
    if (!strcmp(n, "verify_mode")) {
        long val = PyLong_AsLong(v);
        if (val == -1 && PyErr_Occurred()) return -1;
        self->verify_mode = (int)val;
        /* If conf is already initialized, apply immediately */
        if (self->conf_initialized) {
            mbedtls_ssl_conf_authmode(&self->conf,
                val == 0 ? MBEDTLS_SSL_VERIFY_NONE :
                val == 1 ? MBEDTLS_SSL_VERIFY_OPTIONAL :
                            MBEDTLS_SSL_VERIFY_REQUIRED);
        }
        return 0;
    }
    if (!strcmp(n, "check_hostname")) {
        int b = PyObject_IsTrue(v);
        if (b < 0) return -1;
        self->check_hostname = b;
        return 0;
    }
    /* Silently accept options, minimum_version, sslsocket_class, etc. */
    return 0;
}

/* --- stub methods --- */

#define STUB_METHOD(cls, name) \
    static PyObject *cls##_##name(PyObject *s, PyObject *a) \
    { (void)s; (void)a; _NOT_IMPL(#cls "." #name); }

/* Parse the prime INTEGER byte-length from a PKCS#3 DHParameter DER blob.
 * Returns bit-length of p, or -1 on parse error. */
static int dh_der_prime_bits(const unsigned char *der, size_t len)
{
    size_t off = 0;
    /* SEQUENCE tag */
    if (off >= len || der[off++] != 0x30) return -1;
    /* SEQUENCE length — skip it */
    if (off >= len) return -1;
    if (der[off] & 0x80) { off += 1 + (der[off] & 0x7f); } else { off++; }
    /* INTEGER tag */
    if (off >= len || der[off++] != 0x02) return -1;
    /* INTEGER length */
    if (off >= len) return -1;
    size_t prime_len;
    if (der[off] & 0x80) {
        int llen = der[off++] & 0x7f;
        prime_len = 0;
        for (int i = 0; i < llen && off < len; i++, off++)
            prime_len = (prime_len << 8) | der[off];
    } else {
        prime_len = der[off++];
    }
    /* leading 0x00 = sign byte, not part of the value */
    if (off < len && der[off] == 0x00) prime_len--;
    return (int)(prime_len * 8);
}

static PyObject *sslctx_load_dh_params(PyObject *self_, PyObject *args)
{
    PySSLContextObject *self = (PySSLContextObject *)self_;
    const char *dhfile = NULL;
    if (!PyArg_ParseTuple(args, "s", &dhfile)) return NULL;

    /* Read file */
    FILE *f = fopen(dhfile, "rb");
    if (!f) { PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError,
                                                    PyUnicode_FromString(dhfile)); return NULL; }
    fseek(f, 0, SEEK_END); long fsz = ftell(f); rewind(f);
    if (fsz <= 0 || fsz > 65536) { fclose(f);
        PyErr_SetString(PyExc_ValueError, "DH params file too large or empty"); return NULL; }
    unsigned char *fbuf = (unsigned char *)PyMem_Malloc((size_t)fsz + 1);
    if (!fbuf) { fclose(f); return PyErr_NoMemory(); }
    fread(fbuf, 1, (size_t)fsz, f); fclose(f);
    fbuf[fsz] = '\0';

    /* PEM → DER */
    mbedtls_pem_context pem;
    mbedtls_pem_init(&pem);
    size_t use_len = 0;
    int ret = mbedtls_pem_read_buffer(&pem,
                  "-----BEGIN DH PARAMETERS-----",
                  "-----END DH PARAMETERS-----",
                  fbuf, NULL, 0, &use_len);
    PyMem_Free(fbuf);
    if (ret != 0) {
        mbedtls_pem_free(&pem);
        char errbuf[256]; mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        PyErr_Format(PySSLErrorObject, "load_dh_params: %s (-0x%04x)", errbuf, -ret);
        return NULL;
    }

    int bits = dh_der_prime_bits(pem.MBEDTLS_PRIVATE(buf),
                                  pem.MBEDTLS_PRIVATE(buflen));
    mbedtls_pem_free(&pem);

    /* Map prime size → nearest IANA FFDHE group (always round up for security) */
    uint16_t ffdhe;
    if      (bits <= 2048) ffdhe = MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE2048;
    else if (bits <= 3072) ffdhe = MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE3072;
    else if (bits <= 4096) ffdhe = MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE4096;
    else if (bits <= 6144) ffdhe = MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE6144;
    else                   ffdhe = MBEDTLS_SSL_IANA_TLS_GROUP_FFDHE8192;

    self->dh_group_id = ffdhe;

    /* Build groups list: preferred EC groups + FFDHE */
    uint16_t *gl = self->groups_list;
    *gl++ = MBEDTLS_SSL_IANA_TLS_GROUP_X25519;
    *gl++ = MBEDTLS_SSL_IANA_TLS_GROUP_SECP256R1;
    *gl++ = MBEDTLS_SSL_IANA_TLS_GROUP_SECP384R1;
    *gl++ = MBEDTLS_SSL_IANA_TLS_GROUP_SECP521R1;
    *gl++ = ffdhe;
    *gl   = 0;

    if (self->conf_initialized)
        mbedtls_ssl_conf_groups(&self->conf, self->groups_list);

    Py_RETURN_NONE;
}

/* NPN is obsolete (removed in mbedTLS 4.0) — silently ignore */
static PyObject *sslctx_set_npn_protocols(PyObject *s, PyObject *a)
{ (void)s; (void)a; Py_RETURN_NONE; }

/* Try to load a CA bundle from a fixed SD-card path */
static PyObject *sslctx_set_default_verify_paths(PyObject *self_, PyObject *a)
{
    (void)a;
    PySSLContextObject *self = (PySSLContextObject *)self_;
    static const char *candidates[] = {
        "sd:/ssl/cacert.pem",
        "sd:/ssl/ca-bundle.crt",
        "sd:/ssl/cert.pem",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        if (mbedtls_x509_crt_parse_file(&self->cacert, candidates[i]) == 0) {
            self->cacert_loaded = 1;
            if (self->conf_initialized)
                mbedtls_ssl_conf_ca_chain(&self->conf, &self->cacert, NULL);
            Py_RETURN_NONE;
        }
    }
    Py_RETURN_NONE;  /* no bundle found — not an error, just no verification */
}

/* get_ca_certs([binary_form=False]) */
static PyObject *sslctx_get_ca_certs(PyObject *self_, PyObject *args)
{
    PySSLContextObject *self = (PySSLContextObject *)self_;
    int binary_form = 0;
    if (!PyArg_ParseTuple(args, "|p", &binary_form)) return NULL;
    PyObject *lst = PyList_New(0);
    if (!lst) return NULL;
    const mbedtls_x509_crt *cur = self->cacert_loaded ? &self->cacert : NULL;
    while (cur && cur->raw.p && cur->raw.len > 0) {
        PyObject *item;
        if (binary_form)
            item = PyBytes_FromStringAndSize((const char *)cur->raw.p,
                                             (Py_ssize_t)cur->raw.len);
        else
            item = PyDict_New();  /* full parsing too expensive */
        if (!item) { Py_DECREF(lst); return NULL; }
        if (PyList_Append(lst, item) < 0) { Py_DECREF(item); Py_DECREF(lst); return NULL; }
        Py_DECREF(item);
        cur = cur->next;
    }
    return lst;
}

/* cert_store_stats() → {"x509_ca": N, "x509_capath": 0, "x509_crl": 0} */
static PyObject *sslctx_cert_store_stats(PyObject *self_, PyObject *a)
{
    (void)a;
    PySSLContextObject *self = (PySSLContextObject *)self_;
    int n = 0;
    const mbedtls_x509_crt *cur = self->cacert_loaded ? &self->cacert : NULL;
    while (cur && cur->raw.p && cur->raw.len > 0) { n++; cur = cur->next; }
    return Py_BuildValue("{s:i,s:i,s:i}", "x509_ca", n, "x509_capath", 0, "x509_crl", 0);
}

/* set_ciphers(cipher_string) — colon/comma-separated mbedTLS suite names;
 * tokens starting with '!' are skipped (exclusion syntax not supported). */
static PyObject *sslctx_set_ciphers(PyObject *self_, PyObject *args)
{
    PySSLContextObject *self = (PySSLContextObject *)self_;
    const char *cipher_str = NULL;
    if (!PyArg_ParseTuple(args, "s", &cipher_str)) return NULL;

    /* Copy so we can tokenize */
    char buf[1024];
    strncpy(buf, cipher_str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    int ids[65]; int count = 0;
    char *tok = strtok(buf, ":, ");
    while (tok && count < 64) {
        if (tok[0] == '!') { tok = strtok(NULL, ":, "); continue; }
        /* Try name directly */
        int id = mbedtls_ssl_get_ciphersuite_id(tok);
        if (id == 0) {
            /* Try with "TLS-" prefix */
            char prefixed[80];
            snprintf(prefixed, sizeof(prefixed), "TLS-%s", tok);
            /* uppercase it */
            for (char *p = prefixed + 4; *p; p++)
                *p = (char)toupper((unsigned char)*p);
            id = mbedtls_ssl_get_ciphersuite_id(prefixed);
        }
        if (id != 0) ids[count++] = id;
        tok = strtok(NULL, ":, ");
    }
    if (count == 0) {
        PyErr_SetString(PySSLErrorObject, "no valid ciphersuites found in string");
        return NULL;
    }
    /* Build null-terminated list in struct (persists for conf lifetime) */
    for (int i = 0; i < count; i++) self->ciphersuites[i] = ids[i];
    self->ciphersuites[count] = 0;
    if (self->conf_initialized)
        mbedtls_ssl_conf_ciphersuites(&self->conf, self->ciphersuites);
    Py_RETURN_NONE;
}

/* --- SNI server callback glue --- */
static int wii_sni_callback(void *p_sni, mbedtls_ssl_context *ssl,
                             const unsigned char *name, size_t name_len)
{
    (void)ssl;
    PySSLContextObject *ctx = (PySSLContextObject *)p_sni;
    if (!ctx->sni_callback) return 0;
    PyGILState_STATE gstate = PyGILState_Ensure();
    PyObject *hostname = PyUnicode_FromStringAndSize((const char *)name, (Py_ssize_t)name_len);
    PyObject *res = hostname ? PyObject_CallFunction(ctx->sni_callback, "O", hostname) : NULL;
    Py_XDECREF(hostname);
    int ret = (res && res != Py_None) ? 0 : -1;
    Py_XDECREF(res);
    PyGILState_Release(gstate);
    return ret;
}

static PyObject *sslctx_set_servername_callback(PyObject *self_, PyObject *args)
{
    PySSLContextObject *self = (PySSLContextObject *)self_;
    PyObject *cb = NULL;
    if (!PyArg_ParseTuple(args, "O", &cb)) return NULL;
    if (cb == Py_None) {
        Py_XDECREF(self->sni_callback);
        self->sni_callback = NULL;
        Py_RETURN_NONE;
    }
    if (!PyCallable_Check(cb)) {
        PyErr_SetString(PyExc_TypeError, "servername_callback must be callable or None");
        return NULL;
    }
    Py_INCREF(cb);
    Py_XDECREF(self->sni_callback);
    self->sni_callback = cb;
    if (self->conf_initialized)
        mbedtls_ssl_conf_sni(&self->conf, wii_sni_callback, self);
    Py_RETURN_NONE;
}

/* --- PSK client — static PSK set via callback return value --- */
static PyObject *sslctx_set_psk_client_callback(PyObject *self_, PyObject *args)
{
    PySSLContextObject *self = (PySSLContextObject *)self_;
    PyObject *cb = NULL;
    if (!PyArg_ParseTuple(args, "O", &cb)) return NULL;

    /* Call the callback immediately with hint=None to get (identity, psk) */
    PyObject *result = PyObject_CallFunction(cb, "O", Py_None);
    if (!result) return NULL;

    const char *identity = NULL; Py_buffer psk_buf = {0};
    if (!PyArg_ParseTuple(result, "sy*", &identity, &psk_buf)) {
        Py_DECREF(result); return NULL;
    }
    size_t idlen = strlen(identity);
    if (idlen > sizeof(self->psk_identity_buf) - 1 ||
        (size_t)psk_buf.len > sizeof(self->psk_buf)) {
        PyBuffer_Release(&psk_buf); Py_DECREF(result);
        PyErr_SetString(PyExc_ValueError, "PSK or identity too long");
        return NULL;
    }
    memcpy(self->psk_buf,          psk_buf.buf, (size_t)psk_buf.len);
    self->psk_len = (size_t)psk_buf.len;
    memcpy(self->psk_identity_buf, identity, idlen + 1);
    self->psk_identity_len = idlen;
    PyBuffer_Release(&psk_buf); Py_DECREF(result);

    if (self->conf_initialized)
        mbedtls_ssl_conf_psk(&self->conf,
                              self->psk_buf,          self->psk_len,
                              self->psk_identity_buf, self->psk_identity_len);
    Py_RETURN_NONE;
}

/* --- PSK server — dynamic lookup via Python callable --- */
static int wii_psk_server_cb(void *p_info, mbedtls_ssl_context *ssl,
                              const unsigned char *identity, size_t identity_len)
{
    PySSLContextObject *ctx = (PySSLContextObject *)p_info;
    if (!ctx->psk_server_callback) return -1;
    PyGILState_STATE gstate = PyGILState_Ensure();
    PyObject *id_obj = PyBytes_FromStringAndSize((const char *)identity, (Py_ssize_t)identity_len);
    PyObject *res = id_obj ? PyObject_CallFunction(ctx->psk_server_callback, "O", id_obj) : NULL;
    Py_XDECREF(id_obj);
    int ret = -1;
    if (res && res != Py_None) {
        Py_buffer psk_view = {0};
        if (PyObject_GetBuffer(res, &psk_view, PyBUF_SIMPLE) == 0) {
            if (mbedtls_ssl_set_hs_psk(ssl, (const unsigned char *)psk_view.buf,
                                        (size_t)psk_view.len) == 0)
                ret = 0;
            PyBuffer_Release(&psk_view);
        } else PyErr_Clear();
    }
    Py_XDECREF(res);
    PyGILState_Release(gstate);
    return ret;
}

static PyObject *sslctx_set_psk_server_callback(PyObject *self_, PyObject *args)
{
    PySSLContextObject *self = (PySSLContextObject *)self_;
    PyObject *cb = NULL;
    if (!PyArg_ParseTuple(args, "O", &cb)) return NULL;
    if (cb != Py_None && !PyCallable_Check(cb)) {
        PyErr_SetString(PyExc_TypeError, "psk_server_callback must be callable or None");
        return NULL;
    }
    Py_XDECREF(self->psk_server_callback);
    self->psk_server_callback = (cb == Py_None) ? NULL : (Py_INCREF(cb), cb);
    if (self->conf_initialized && self->psk_server_callback)
        mbedtls_ssl_conf_psk_cb(&self->conf, wii_psk_server_cb, self);
    Py_RETURN_NONE;
}

/* --- load_verify_locations --- */

static PyObject *sslctx_load_verify_locations(PyObject *self_, PyObject *args)
{
    PySSLContextObject *self = (PySSLContextObject *)self_;
    const char *cafile = NULL;
    Py_buffer cadata   = {0};
    int has_cadata     = 0;

    PyObject *cafile_obj = Py_None, *capath_obj = Py_None, *cadata_obj = Py_None;
    if (!PyArg_ParseTuple(args, "OOO", &cafile_obj, &capath_obj, &cadata_obj))
        return NULL;
    /* capath ignored — no filesystem scan on Wii */

    if (cafile_obj != Py_None) {
        cafile = PyUnicode_AsUTF8(cafile_obj);
        if (!cafile) return NULL;
    }
    if (cadata_obj != Py_None) {
        if (PyObject_GetBuffer(cadata_obj, &cadata, PyBUF_SIMPLE) < 0) return NULL;
        has_cadata = 1;
    }

    int ret = 0;
    if (cafile) {
        ret = mbedtls_x509_crt_parse_file(&self->cacert, cafile);
        if (ret != 0) {
            char buf[256]; mbedtls_strerror(ret, buf, sizeof(buf));
            if (has_cadata) PyBuffer_Release(&cadata);
            PyErr_Format(PySSLErrorObject,
                         "load_verify_locations file: %s (-0x%04x)", buf, -ret);
            return NULL;
        }
        self->cacert_loaded = 1;
    }
    if (has_cadata) {
        /* PEM as string: mbedTLS needs null-terminator → len+1 */
        ret = mbedtls_x509_crt_parse(&self->cacert,
                                      (const unsigned char *)cadata.buf,
                                      (size_t)cadata.len + 1);
        PyBuffer_Release(&cadata);
        if (ret != 0 && ret != MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT) {
            if (ret < 0) {
                char buf[256]; mbedtls_strerror(ret, buf, sizeof(buf));
                PyErr_Format(PySSLErrorObject,
                             "load_verify_locations data: %s (-0x%04x)", buf, -ret);
                return NULL;
            }
            /* positive ret = number of skipped certs — acceptable */
        }
        self->cacert_loaded = 1;
    }
    if (self->cacert_loaded && self->conf_initialized)
        mbedtls_ssl_conf_ca_chain(&self->conf, &self->cacert, NULL);

    Py_RETURN_NONE;
}

/* --- load_cert_chain --- */

static PyObject *sslctx_load_cert_chain(PyObject *self_, PyObject *args)
{
    PySSLContextObject *self = (PySSLContextObject *)self_;
    const char *certfile = NULL, *keyfile = NULL;
    /* password argument ignored — simple path, no encrypted-key support */
    if (!PyArg_ParseTuple(args, "s|sz", &certfile, &keyfile, NULL)) return NULL;

    int ret = mbedtls_x509_crt_parse_file(&self->owncert, certfile);
    if (ret != 0) {
        char buf[256]; mbedtls_strerror(ret, buf, sizeof(buf));
        PyErr_Format(PySSLErrorObject,
                     "load_cert_chain cert: %s (-0x%04x)", buf, -ret);
        return NULL;
    }
    if (keyfile) {
        /* mbedTLS 4.0: mbedtls_pk_parse_keyfile(ctx, path, password)
         * — no RNG callback parameter */
        ret = mbedtls_pk_parse_keyfile(&self->ownkey, keyfile, NULL);
        if (ret != 0) {
            char buf[256]; mbedtls_strerror(ret, buf, sizeof(buf));
            PyErr_Format(PySSLErrorObject,
                         "load_cert_chain key: %s (-0x%04x)", buf, -ret);
            return NULL;
        }
    }
    ret = mbedtls_ssl_conf_own_cert(&self->conf,
                                    &self->owncert,
                                    keyfile ? &self->ownkey : NULL);
    if (ret != 0) {
        char buf[256]; mbedtls_strerror(ret, buf, sizeof(buf));
        PyErr_Format(PySSLErrorObject,
                     "ssl_conf_own_cert: %s (-0x%04x)", buf, -ret);
        return NULL;
    }
    self->owncert_loaded = 1;
    Py_RETURN_NONE;
}

/* --- set_alpn_protocols --- */

static PyObject *sslctx_set_alpn_protocols(PyObject *self_, PyObject *args)
{
    PySSLContextObject *self = (PySSLContextObject *)self_;
    PyObject *protos = NULL;

    if (!PyArg_ParseTuple(args, "O", &protos)) return NULL;

    Py_ssize_t n = PySequence_Length(protos);
    if (n < 0) return NULL;
    if (n > 8) {
        PyErr_SetString(PyExc_ValueError, "max 8 ALPN protocols");
        return NULL;
    }

    for (Py_ssize_t i = 0; i < n; i++) {
        PyObject *p = PySequence_GetItem(protos, i);
        if (!p) return NULL;
        const char *s = PyUnicode_AsUTF8(p);
        Py_DECREF(p);
        if (!s) return NULL;
        size_t slen = strlen(s);
        if (slen > 31) {
            PyErr_SetString(PyExc_ValueError, "ALPN protocol name too long (max 31)");
            return NULL;
        }
        memcpy(self->alpn_buf[i], s, slen + 1);
        self->alpn_list[i] = self->alpn_buf[i];
    }
    self->alpn_list[n] = NULL;
    self->alpn_count = (int)n;

    if (self->conf_initialized) {
        int ret = mbedtls_ssl_conf_alpn_protocols(&self->conf, self->alpn_list);
        if (ret != 0) {
            char buf[256]; mbedtls_strerror(ret, buf, sizeof(buf));
            PyErr_Format(PySSLErrorObject,
                         "set_alpn_protocols: %s (-0x%04x)", buf, -ret);
            return NULL;
        }
    }
    Py_RETURN_NONE;
}

/* --- _wrap_socket — client and server TCP TLS --- */

static PyObject *sslctx_wrap_socket(PyObject *self_, PyObject *args)
{
    PySSLContextObject *self = (PySSLContextObject *)self_;
    PyObject      *sock_obj;
    int            server_side     = 0;
    const char    *server_hostname = NULL;

    if (!PyArg_ParseTuple(args, "Oiz", &sock_obj, &server_side, &server_hostname))
        return NULL;

    int confret = sslctx_ensure_conf(self, server_side);
    if (confret != 0) {
        if (confret == -1)
            PyErr_SetString(PyExc_ValueError, "SSLContext endpoint mismatch");
        else
            PyErr_Format(PyExc_OSError, "ssl_config_defaults: -0x%04x", -confret);
        return NULL;
    }

    /* Extract the raw fd from the socket object */
    PyObject *fileno_res = PyObject_CallMethod(sock_obj, "fileno", NULL);
    if (!fileno_res) return NULL;
    int fd = (int)PyLong_AsLong(fileno_res);
    Py_DECREF(fileno_res);
    if (fd < 0 || PyErr_Occurred()) {
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_OSError, "invalid socket fd");
        return NULL;
    }

    PyWiiSSLObject *sslobj = PyObject_New(PyWiiSSLObject, &PyWiiSSL_Type);
    if (!sslobj) return NULL;

    mbedtls_ssl_init(&sslobj->ssl);
    sslobj->fd                 = fd;
    sslobj->ctx                = self;
    sslobj->connected          = 0;
    sslobj->session_was_resumed= 0;
    sslobj->incoming           = NULL;
    sslobj->outgoing           = NULL;
    Py_INCREF(self);

    int ret = mbedtls_ssl_setup(&sslobj->ssl, &self->conf);
    if (ret != 0) {
        Py_DECREF(sslobj);
        PyErr_Format(PyExc_OSError, "mbedtls_ssl_setup: -0x%04x", -ret);
        return NULL;
    }
    if (server_hostname) {
        ret = mbedtls_ssl_set_hostname(&sslobj->ssl, server_hostname);
        if (ret != 0) {
            Py_DECREF(sslobj);
            PyErr_Format(PyExc_OSError, "mbedtls_ssl_set_hostname: -0x%04x", -ret);
            return NULL;
        }
    }
    mbedtls_ssl_set_bio(&sslobj->ssl, &sslobj->fd, wii_ssl_send, wii_ssl_recv, NULL);
    return (PyObject *)sslobj;
}

/* --- _wrap_bio — MemoryBIO TLS --- */

static PyObject *sslctx__wrap_bio(PyObject *self_, PyObject *args)
{
    PySSLContextObject *self = (PySSLContextObject *)self_;
    PySSLMemoryBIO *incoming, *outgoing;
    int server_side = 0;
    const char *server_hostname = NULL;

    if (!PyArg_ParseTuple(args, "O!O!iz",
                          &PySSLMemoryBIO_Type, &incoming,
                          &PySSLMemoryBIO_Type, &outgoing,
                          &server_side, &server_hostname))
        return NULL;

    int confret = sslctx_ensure_conf(self, server_side);
    if (confret != 0) {
        if (confret == -1)
            PyErr_SetString(PyExc_ValueError, "SSLContext endpoint mismatch");
        else
            PyErr_Format(PyExc_OSError, "ssl_config_defaults: -0x%04x", -confret);
        return NULL;
    }

    PyWiiSSLObject *sslobj = PyObject_New(PyWiiSSLObject, &PyWiiSSL_Type);
    if (!sslobj) return NULL;

    mbedtls_ssl_init(&sslobj->ssl);
    sslobj->fd                 = -1;
    sslobj->ctx                = self;
    sslobj->connected          = 0;
    sslobj->session_was_resumed= 0;
    sslobj->incoming           = incoming;
    sslobj->outgoing  = outgoing;
    sslobj->bio_pair.in  = incoming;
    sslobj->bio_pair.out = outgoing;
    Py_INCREF(self);
    Py_INCREF(incoming);
    Py_INCREF(outgoing);

    int ret = mbedtls_ssl_setup(&sslobj->ssl, &self->conf);
    if (ret != 0) {
        Py_DECREF(sslobj);
        PyErr_Format(PyExc_OSError, "mbedtls_ssl_setup: -0x%04x", -ret);
        return NULL;
    }
    if (server_hostname) {
        ret = mbedtls_ssl_set_hostname(&sslobj->ssl, server_hostname);
        if (ret != 0) {
            Py_DECREF(sslobj);
            PyErr_Format(PyExc_OSError, "mbedtls_ssl_set_hostname: -0x%04x", -ret);
            return NULL;
        }
    }
    mbedtls_ssl_set_bio(&sslobj->ssl, &sslobj->bio_pair,
                        wii_bio_pair_send, wii_bio_pair_recv, NULL);
    return (PyObject *)sslobj;
}

static PyMethodDef sslctx_methods[] = {
    {"set_ciphers",              sslctx_set_ciphers,             METH_VARARGS, NULL},
    {"set_alpn_protocols",       sslctx_set_alpn_protocols,      METH_VARARGS, NULL},
    {"set_npn_protocols",        sslctx_set_npn_protocols,       METH_VARARGS, NULL},
    {"set_servername_callback",  sslctx_set_servername_callback, METH_VARARGS, NULL},
    {"load_cert_chain",          sslctx_load_cert_chain,         METH_VARARGS, NULL},
    {"load_dh_params",           sslctx_load_dh_params,          METH_VARARGS, NULL},
    {"load_verify_locations",    sslctx_load_verify_locations,   METH_VARARGS, NULL},
    {"set_default_verify_paths", sslctx_set_default_verify_paths,METH_NOARGS,  NULL},
    {"_wrap_socket",             sslctx_wrap_socket,             METH_VARARGS, NULL},
    {"_wrap_bio",                sslctx__wrap_bio,               METH_VARARGS, NULL},
    {"get_ca_certs",             sslctx_get_ca_certs,            METH_VARARGS, NULL},
    {"cert_store_stats",         sslctx_cert_store_stats,        METH_NOARGS,  NULL},
    {"set_psk_client_callback",  sslctx_set_psk_client_callback, METH_VARARGS, NULL},
    {"set_psk_server_callback",  sslctx_set_psk_server_callback, METH_VARARGS, NULL},
    {NULL, NULL}
};

static PyTypeObject PySSLContext_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "_ssl._SSLContext",
    .tp_basicsize = sizeof(PySSLContextObject),
    .tp_dealloc   = sslctx_dealloc,
    .tp_new       = sslctx_new,
    .tp_methods   = sslctx_methods,
    .tp_getattro  = sslctx_getattr,
    .tp_setattro  = sslctx_setattr,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
};

/* ================================================================ _WiiSSLObject */

static void wiissl_dealloc(PyObject *self_)
{
    PyWiiSSLObject *self = (PyWiiSSLObject *)self_;
    mbedtls_ssl_free(&self->ssl);
    Py_XDECREF(self->ctx);
    Py_XDECREF(self->incoming);
    Py_XDECREF(self->outgoing);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *wiissl_do_handshake(PyObject *self_, PyObject *args)
{
    (void)args;
    PyWiiSSLObject *self = (PyWiiSSLObject *)self_;
    int ret;
    do {
        ret = mbedtls_ssl_handshake(&self->ssl);
    } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
    if (ret != 0) {
        char buf[256];
        mbedtls_strerror(ret, buf, sizeof(buf));
        PyErr_Format(PySSLErrorObject, "TLS handshake failed: %s (-0x%04x)", buf, -ret);
        return NULL;
    }
    self->connected = 1;
    Py_RETURN_NONE;
}

static PyObject *wiissl_read(PyObject *self_, PyObject *args)
{
    PyWiiSSLObject *self = (PyWiiSSLObject *)self_;
    int       n      = 1024;
    PyObject *buffer = NULL;

    if (!PyArg_ParseTuple(args, "|iO", &n, &buffer)) return NULL;
    if (n < 0) {
        PyErr_SetString(PyExc_ValueError, "read length must be non-negative");
        return NULL;
    }

    if (buffer) {
        /* Read into caller-supplied writable buffer; return byte count */
        Py_buffer view;
        if (PyObject_GetBuffer(buffer, &view, PyBUF_WRITABLE) < 0) return NULL;
        size_t to_read = ((size_t)n < (size_t)view.len) ? (size_t)n : (size_t)view.len;
        int ret;
        do {
            ret = mbedtls_ssl_read(&self->ssl, (unsigned char *)view.buf, to_read);
        } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
        PyBuffer_Release(&view);
        if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == 0)
            return PyLong_FromLong(0);
        if (ret < 0) {
            char errbuf[256];
            mbedtls_strerror(ret, errbuf, sizeof(errbuf));
            PyErr_Format(PySSLErrorObject, "SSL read: %s (-0x%04x)", errbuf, -ret);
            return NULL;
        }
        return PyLong_FromLong(ret);
    } else {
        /* Read and return a fresh bytes object */
        PyObject *result = PyBytes_FromStringAndSize(NULL, n);
        if (!result) return NULL;
        int ret;
        do {
            ret = mbedtls_ssl_read(&self->ssl,
                                   (unsigned char *)PyBytes_AS_STRING(result),
                                   (size_t)n);
        } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
        if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == 0) {
            Py_DECREF(result);
            return PyBytes_FromStringAndSize(NULL, 0);
        }
        if (ret < 0) {
            Py_DECREF(result);
            char errbuf[256];
            mbedtls_strerror(ret, errbuf, sizeof(errbuf));
            PyErr_Format(PySSLErrorObject, "SSL read: %s (-0x%04x)", errbuf, -ret);
            return NULL;
        }
        if (_PyBytes_Resize(&result, ret) < 0) return NULL;
        return result;
    }
}

static PyObject *wiissl_write(PyObject *self_, PyObject *args)
{
    PyWiiSSLObject *self = (PyWiiSSLObject *)self_;
    Py_buffer data;
    if (!PyArg_ParseTuple(args, "y*", &data)) return NULL;

    size_t written = 0;
    while (written < (size_t)data.len) {
        int ret;
        do {
            ret = mbedtls_ssl_write(&self->ssl,
                                    (const unsigned char *)data.buf + written,
                                    (size_t)data.len - written);
        } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
        if (ret < 0) {
            PyBuffer_Release(&data);
            char errbuf[256];
            mbedtls_strerror(ret, errbuf, sizeof(errbuf));
            PyErr_Format(PySSLErrorObject, "SSL write: %s (-0x%04x)", errbuf, -ret);
            return NULL;
        }
        written += (size_t)ret;
    }
    PyBuffer_Release(&data);
    return PyLong_FromSsize_t((Py_ssize_t)written);
}

static PyObject *wiissl_cipher(PyObject *self_, PyObject *args)
{
    (void)args;
    PyWiiSSLObject *self = (PyWiiSSLObject *)self_;
    const char *cs  = mbedtls_ssl_get_ciphersuite(&self->ssl);
    const char *ver = mbedtls_ssl_get_version(&self->ssl);
    return Py_BuildValue("(zzO)", cs, ver, Py_None);
}

static PyObject *wiissl_pending(PyObject *self_, PyObject *args)
{
    (void)args;
    PyWiiSSLObject *self = (PyWiiSSLObject *)self_;
    return PyLong_FromSize_t(mbedtls_ssl_get_bytes_avail(&self->ssl));
}

static PyObject *wiissl_getpeercert(PyObject *self_, PyObject *args)
{
    PyWiiSSLObject *self = (PyWiiSSLObject *)self_;
    int binary_form = 0;
    if (!PyArg_ParseTuple(args, "|p", &binary_form)) return NULL;

    if (binary_form) {
        const mbedtls_x509_crt *cert = mbedtls_ssl_get_peer_cert(&self->ssl);
        if (!cert || !cert->raw.p || cert->raw.len == 0)
            return PyBytes_FromStringAndSize(NULL, 0);
        return PyBytes_FromStringAndSize((const char *)cert->raw.p,
                                         (Py_ssize_t)cert->raw.len);
    }
    /* dict-form: return empty dict (full X.509 parsing too expensive on Wii) */
    return PyDict_New();
}

static PyObject *wiissl_shutdown(PyObject *self_, PyObject *args)
{
    (void)args;
    PyWiiSSLObject *self = (PyWiiSSLObject *)self_;
    if (self->connected)
        mbedtls_ssl_close_notify(&self->ssl);
    self->connected = 0;
    Py_RETURN_NONE;
}

static PyObject *wiissl_get_unverified_chain(PyObject *s, PyObject *a)
{ (void)s; (void)a; return PyList_New(0); }

static PyObject *wiissl_get_verified_chain(PyObject *s, PyObject *a)
{ (void)s; (void)a; return PyList_New(0); }

static PyObject *wiissl_shared_ciphers(PyObject *s, PyObject *a)
{ (void)s; (void)a; return PyList_New(0); }

static PyObject *wiissl_compression(PyObject *s, PyObject *a)
{ (void)s; (void)a; Py_RETURN_NONE; }

static PyObject *wiissl_selected_alpn_protocol(PyObject *self_, PyObject *a)
{
    (void)a;
    const char *proto = mbedtls_ssl_get_alpn_protocol(&((PyWiiSSLObject *)self_)->ssl);
    if (!proto) Py_RETURN_NONE;
    return PyUnicode_FromString(proto);
}

static PyObject *wiissl_version(PyObject *self_, PyObject *a)
{
    (void)a;
    PyWiiSSLObject *self = (PyWiiSSLObject *)self_;
    if (!self->connected) Py_RETURN_NONE;
    const char *ver = mbedtls_ssl_get_version(&self->ssl);
    if (!ver) Py_RETURN_NONE;
    return PyUnicode_FromString(ver);
}

/* Derive the key-agreement group name from the peer certificate's public key.
 * mbedTLS 4.0 exposes no direct ECDHE-group getter; the cert's key type is
 * the closest public approximation. */
static PyObject *wiissl_group(PyObject *self_, PyObject *a)
{
    (void)a;
    PyWiiSSLObject *self = (PyWiiSSLObject *)self_;
    const mbedtls_x509_crt *cert = mbedtls_ssl_get_peer_cert(&self->ssl);
    if (!cert) Py_RETURN_NONE;

    psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;
    if (mbedtls_pk_get_psa_attributes(&cert->pk, PSA_KEY_USAGE_VERIFY_HASH, &attrs) != 0)
        Py_RETURN_NONE;

    psa_key_type_t ktype = psa_get_key_type(&attrs);
    size_t         bits  = psa_get_key_bits(&attrs);
    psa_reset_key_attributes(&attrs);

    if (!PSA_KEY_TYPE_IS_ECC(ktype)) Py_RETURN_NONE;

    psa_ecc_family_t fam = PSA_KEY_TYPE_ECC_GET_FAMILY(ktype);

    if (fam == PSA_ECC_FAMILY_MONTGOMERY) {
        if (bits == 255) return PyUnicode_FromString("x25519");
        if (bits == 448) return PyUnicode_FromString("x448");
    }
    if (fam == PSA_ECC_FAMILY_SECP_R1) {
        if (bits == 256) return PyUnicode_FromString("prime256v1");
        if (bits == 384) return PyUnicode_FromString("secp384r1");
        if (bits == 521) return PyUnicode_FromString("secp521r1");
    }
    if (fam == PSA_ECC_FAMILY_SECP_K1) {
        if (bits == 256) return PyUnicode_FromString("secp256k1");
    }
    Py_RETURN_NONE;
}

/* Build a sigalg string from the peer certificate's sig_md / sig_pk fields.
 * MBEDTLS_ALLOW_PRIVATE_ACCESS is defined at the top of this file. */
static PyObject *sigalg_from_cert(const mbedtls_x509_crt *cert)
{
    if (!cert) Py_RETURN_NONE;

    mbedtls_md_type_t     md  = cert->MBEDTLS_PRIVATE(sig_md);
    mbedtls_pk_sigalg_t   pk  = cert->MBEDTLS_PRIVATE(sig_pk);

    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md);
    const char *hash = md_info ? mbedtls_md_get_name(md_info) : NULL;
    if (!hash) Py_RETURN_NONE;

    /* Lowercase hash name (SHA256 → sha256) */
    char hash_lower[16]; int i;
    for (i = 0; hash[i] && i < 15; i++)
        hash_lower[i] = (char)tolower((unsigned char)hash[i]);
    hash_lower[i] = '\0';

    char out[64];
    switch (pk) {
    case MBEDTLS_PK_SIGALG_RSA_PSS:
        snprintf(out, sizeof(out), "rsa_pss_rsae_%s", hash_lower);
        break;
    case MBEDTLS_PK_SIGALG_RSA_PKCS1V15:
        snprintf(out, sizeof(out), "rsa_pkcs1_%s", hash_lower);
        break;
    case MBEDTLS_PK_SIGALG_ECDSA:
        snprintf(out, sizeof(out), "ecdsa_%s", hash_lower);
        break;
    default:
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(out);
}

static PyObject *wiissl_client_sigalg(PyObject *self_, PyObject *a)
{
    (void)a;
    /* "client" sigalg = algorithm the remote client used; from its certificate */
    const mbedtls_x509_crt *cert =
        mbedtls_ssl_get_peer_cert(&((PyWiiSSLObject *)self_)->ssl);
    return sigalg_from_cert(cert);
}

static PyObject *wiissl_server_sigalg(PyObject *self_, PyObject *a)
{
    (void)a;
    /* "server" sigalg = algorithm the remote server used; from its certificate */
    const mbedtls_x509_crt *cert =
        mbedtls_ssl_get_peer_cert(&((PyWiiSSLObject *)self_)->ssl);
    return sigalg_from_cert(cert);
}

static PyObject *wiissl_get_channel_binding(PyObject *self_, PyObject *args)
{
    PyWiiSSLObject *self = (PyWiiSSLObject *)self_;
    const char *cb_type = "tls-unique";
    if (!PyArg_ParseTuple(args, "|s", &cb_type)) return NULL;

    if (strcmp(cb_type, "tls-unique") == 0) {
        /* RFC 5929: first Finished message bytes.
         * own_verify_data = what we sent; for the client, that's the
         * ClientFinished, which is what both sides use as tls-unique. */
        size_t len = self->ssl.MBEDTLS_PRIVATE(verify_data_len);
        if (len == 0) Py_RETURN_NONE;
        const char *data = self->ssl.MBEDTLS_PRIVATE(own_verify_data);
        return PyBytes_FromStringAndSize(data, (Py_ssize_t)len);
    }

    if (strcmp(cb_type, "tls-server-end-point") == 0) {
        /* RFC 5929: SHA-256 (or stronger) of the server certificate DER. */
        const mbedtls_x509_crt *cert = mbedtls_ssl_get_peer_cert(&self->ssl);
        if (!cert || !cert->raw.p || cert->raw.len == 0) Py_RETURN_NONE;
        unsigned char digest[32];
        mbedtls_md_context_t ctx;
        mbedtls_md_init(&ctx);
        const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
        if (!info || mbedtls_md_setup(&ctx, info, 0) != 0) {
            mbedtls_md_free(&ctx); Py_RETURN_NONE;
        }
        mbedtls_md_starts(&ctx);
        mbedtls_md_update(&ctx, cert->raw.p, cert->raw.len);
        mbedtls_md_finish(&ctx, digest);
        mbedtls_md_free(&ctx);
        return PyBytes_FromStringAndSize((char *)digest, sizeof(digest));
    }

    if (strcmp(cb_type, "tls-exporter") == 0) {
        /* RFC 9622 / TLS 1.3: use the exporter with label "EXPORTER-Channel-Binding". */
        static const char label[] = "EXPORTER-Channel-Binding";
        unsigned char out[32];
        int ret = mbedtls_ssl_export_keying_material(
                      &self->ssl, out, sizeof(out),
                      label, sizeof(label) - 1,
                      NULL, 0, 0);
        if (ret != 0) Py_RETURN_NONE;
        return PyBytes_FromStringAndSize((char *)out, sizeof(out));
    }

    PyErr_Format(PyExc_ValueError, "unsupported channel binding type: %s", cb_type);
    return NULL;
}

static PyObject *wiissl_uses_ktls_for_send(PyObject *s, PyObject *a)
{ (void)s; (void)a; Py_RETURN_FALSE; }

static PyObject *wiissl_verify_client_post_handshake(PyObject *s, PyObject *a)
{ (void)s; (void)a; _NOT_IMPL("verify_client_post_handshake"); }

/* session getter helper — saves current session into a new PySSLSession */
static PyObject *wiissl_get_session_obj(PyWiiSSLObject *self)
{
    PySSLSession *sess = (PySSLSession *)sslsession_new(&PySSLSession_Type, NULL, NULL);
    if (!sess) return NULL;
    int ret = mbedtls_ssl_get_session(&self->ssl, &sess->session);
    if (ret != 0) {
        Py_DECREF(sess);
        Py_RETURN_NONE;
    }
    return (PyObject *)sess;
}

static PyObject *wiissl_getattr(PyObject *self_, PyObject *name)
{
    PyWiiSSLObject *self = (PyWiiSSLObject *)self_;
    const char *n = PyUnicode_AsUTF8(name);
    if (n && !strcmp(n, "context")) {
        Py_INCREF(self->ctx);
        return (PyObject *)self->ctx;
    }
    if (n && !strcmp(n, "server_side"))
        return PyBool_FromLong(self->ctx && self->ctx->endpoint == MBEDTLS_SSL_IS_SERVER);
    if (n && !strcmp(n, "server_hostname"))
        Py_RETURN_NONE;  /* mbedTLS doesn't expose stored hostname */
    if (n && !strcmp(n, "session_reused"))
        return PyBool_FromLong(self->session_was_resumed);
    if (n && !strcmp(n, "session"))
        return wiissl_get_session_obj(self);
    return PyObject_GenericGetAttr(self_, name);
}

static PyMethodDef wiissl_methods[] = {
    {"do_handshake",                  wiissl_do_handshake,                 METH_NOARGS,  NULL},
    {"read",                          wiissl_read,                         METH_VARARGS, NULL},
    {"write",                         wiissl_write,                        METH_VARARGS, NULL},
    {"cipher",                        wiissl_cipher,                       METH_NOARGS,  NULL},
    {"version",                       wiissl_version,                      METH_NOARGS,  NULL},
    {"pending",                       wiissl_pending,                      METH_NOARGS,  NULL},
    {"getpeercert",                   wiissl_getpeercert,                  METH_VARARGS, NULL},
    {"get_unverified_chain",          wiissl_get_unverified_chain,         METH_NOARGS,  NULL},
    {"get_verified_chain",            wiissl_get_verified_chain,           METH_NOARGS,  NULL},
    {"shared_ciphers",                wiissl_shared_ciphers,               METH_NOARGS,  NULL},
    {"compression",                   wiissl_compression,                  METH_NOARGS,  NULL},
    {"shutdown",                      wiissl_shutdown,                     METH_NOARGS,  NULL},
    {"selected_alpn_protocol",        wiissl_selected_alpn_protocol,       METH_NOARGS,  NULL},
    {"group",                         wiissl_group,                        METH_NOARGS,  NULL},
    {"client_sigalg",                 wiissl_client_sigalg,                METH_NOARGS,  NULL},
    {"server_sigalg",                 wiissl_server_sigalg,                METH_NOARGS,  NULL},
    {"get_channel_binding",           wiissl_get_channel_binding,          METH_VARARGS, NULL},
    {"uses_ktls_for_send",            wiissl_uses_ktls_for_send,           METH_NOARGS,  NULL},
    {"verify_client_post_handshake",  wiissl_verify_client_post_handshake, METH_NOARGS,  NULL},
    {NULL, NULL}
};

static int wiissl_setattr(PyObject *self_, PyObject *name, PyObject *v)
{
    PyWiiSSLObject *self = (PyWiiSSLObject *)self_;
    const char *n = PyUnicode_AsUTF8(name);
    if (!n) return -1;
    if (!strcmp(n, "session")) {
        if (v == Py_None) return 0;
        if (!PyObject_TypeCheck(v, &PySSLSession_Type)) {
            PyErr_SetString(PyExc_TypeError, "session must be an SSLSession or None");
            return -1;
        }
        PySSLSession *sess = (PySSLSession *)v;
        int ret = mbedtls_ssl_set_session(&self->ssl, &sess->session);
        if (ret != 0) {
            PyErr_Format(PySSLErrorObject, "ssl_set_session: -0x%04x", -ret);
            return -1;
        }
        self->session_was_resumed = 1;
        return 0;
    }
    /* ignore unknown attribute sets */
    return 0;
}

static PyTypeObject PyWiiSSL_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "_ssl._SSLObject",
    .tp_basicsize = sizeof(PyWiiSSLObject),
    .tp_dealloc   = wiissl_dealloc,
    .tp_methods   = wiissl_methods,
    .tp_getattro  = wiissl_getattr,
    .tp_setattro  = wiissl_setattr,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
};

/* ================================================================ SSLSession */

static PyObject *sslsession_new(PyTypeObject *t, PyObject *a, PyObject *k)
{
    (void)a; (void)k;
    PySSLSession *self = (PySSLSession *)t->tp_alloc(t, 0);
    if (!self) return NULL;
    mbedtls_ssl_session_init(&self->session);
    self->initialized = 1;
    return (PyObject *)self;
}

static void sslsession_dealloc(PyObject *self_)
{
    PySSLSession *self = (PySSLSession *)self_;
    if (self->initialized)
        mbedtls_ssl_session_free(&self->session);
    Py_TYPE(self)->tp_free(self);
}

static PyObject *sslsession_getattr(PyObject *self_, PyObject *name)
{
    (void)self_;
    const char *n = PyUnicode_AsUTF8(name);
    if (!n) return NULL;
    /* Expose ticket presence as id_len > 0 heuristic */
    if (!strcmp(n, "has_ticket")) Py_RETURN_FALSE;
    if (!strcmp(n, "ticket_lifetime_hint")) return PyLong_FromLong(0);
    if (!strcmp(n, "id"))         return PyBytes_FromStringAndSize(NULL, 0);
    return PyObject_GenericGetAttr(self_, name);
}

static PyTypeObject PySSLSession_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "_ssl.SSLSession",
    .tp_basicsize = sizeof(PySSLSession),
    .tp_dealloc   = sslsession_dealloc,
    .tp_new       = sslsession_new,
    .tp_getattro  = sslsession_getattr,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
};

/* ================================================================ module functions */

static PyObject *_ssl_RAND_status(PyObject *m, PyObject *a)
{ (void)m; (void)a; return PyLong_FromLong(1); }

static PyObject *_ssl_RAND_add(PyObject *m, PyObject *a)
{ (void)m; (void)a; Py_RETURN_NONE; }

static PyObject *_ssl_RAND_bytes(PyObject *m, PyObject *args)
{
    int n;
    (void)m;
    if (!PyArg_ParseTuple(args, "i", &n)) return NULL;
    if (n < 0) { PyErr_SetString(PyExc_ValueError, "negative count"); return NULL; }
    PyObject *result = PyBytes_FromStringAndSize(NULL, n);
    if (!result) return NULL;
    psa_crypto_init();
    psa_status_t rc = psa_generate_random((uint8_t *)PyBytes_AS_STRING(result), (size_t)n);
    if (rc != PSA_SUCCESS) {
        Py_DECREF(result);
        PyErr_SetString(PyExc_OSError, "RAND_bytes: psa_generate_random failed");
        return NULL;
    }
    return result;
}

/* Minimal OID table for ssl.py compatibility (nid, shortname, longname, dotted_oid).
 * Matches the 4-tuple returned by the real OpenSSL _ssl.txt2obj / _ssl.nid2obj.
 * ssl.py unpacks the result into _ASN1Object(nid, shortname, longname, oid). */
typedef struct { int nid; const char *sn; const char *ln; const char *oid; } _OIDEntry;
static const _OIDEntry _oid_table[] = {
    /* Extended Key Usage (used by ssl.Purpose enum) */
    {129, "serverAuth",              "TLS Web Server Authentication",      "1.3.6.1.5.5.7.3.1"},
    {130, "clientAuth",              "TLS Web Client Authentication",      "1.3.6.1.5.5.7.3.2"},
    /* Common subject name components */
    { 13, "CN",                      "commonName",                         "2.5.4.3"},
    {  6, "C",                       "countryName",                        "2.5.4.6"},
    {  7, "L",                       "localityName",                       "2.5.4.7"},
    {  8, "ST",                      "stateOrProvinceName",                "2.5.4.8"},
    {  9, "O",                       "organizationName",                   "2.5.4.10"},
    { 11, "OU",                      "organizationalUnitName",             "2.5.4.11"},
    { 14, "emailAddress",            "emailAddress",                       "1.2.840.113549.1.9.1"},
    /* Subject Alt Name / basic constraints */
    { 85, "subjectAltName",          "X509v3 Subject Alternative Name",    "2.5.29.17"},
    { 87, "basicConstraints",        "X509v3 Basic Constraints",           "2.5.29.19"},
    {  0, NULL, NULL, NULL}
};

static PyObject *_oid_build(const _OIDEntry *e)
{
    return Py_BuildValue("(isss)", e->nid, e->sn, e->ln, e->oid);
}

/* txt2obj(txt, name=False) -> (nid, shortname, longname, oid)
 * Matches the signature of the real _ssl.txt2obj. */
static PyObject *_ssl_txt2obj(PyObject *m, PyObject *args, PyObject *kwargs)
{
    (void)m;
    static char *kwlist[] = {"txt", "name", NULL};
    const char *txt = NULL;
    int use_name = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|p", kwlist, &txt, &use_name))
        return NULL;

    for (int i = 0; _oid_table[i].sn != NULL; i++) {
        if (strcmp(txt, _oid_table[i].oid) == 0)
            return _oid_build(&_oid_table[i]);
        if (use_name && (strcmp(txt, _oid_table[i].sn) == 0 ||
                         strcmp(txt, _oid_table[i].ln) == 0))
            return _oid_build(&_oid_table[i]);
    }
    /* Unknown OID — return a generic stub so ssl.py can still import */
    return Py_BuildValue("(isss)", 0, txt, txt, txt);
}

/* nid2obj(nid) -> (nid, shortname, longname, oid)
 * Matches the signature of the real _ssl.nid2obj. */
static PyObject *_ssl_nid2obj(PyObject *m, PyObject *args)
{
    (void)m;
    int nid = 0;
    if (!PyArg_ParseTuple(args, "i", &nid))
        return NULL;

    for (int i = 0; _oid_table[i].sn != NULL; i++) {
        if (_oid_table[i].nid == nid)
            return _oid_build(&_oid_table[i]);
    }
    PyErr_Format(PyExc_ValueError, "unknown NID %d", nid);
    return NULL;
}

static PyObject *_ssl_get_sigalgs(PyObject *m, PyObject *a)
{ (void)m; (void)a; return PyList_New(0); }

static PyObject *_ssl_get_default_verify_paths(PyObject *m, PyObject *a)
{ (void)m; (void)a; return Py_BuildValue("(ssssss)", "", "", "", "", "", ""); }

static PyMethodDef _ssl_methods[] = {
    {"RAND_status",              _ssl_RAND_status,                        METH_NOARGS,                    NULL},
    {"RAND_add",                 _ssl_RAND_add,                           METH_VARARGS,                   NULL},
    {"RAND_bytes",               _ssl_RAND_bytes,                         METH_VARARGS,                   NULL},
    {"txt2obj",                  (PyCFunction)(void(*)(void))_ssl_txt2obj, METH_VARARGS | METH_KEYWORDS,   NULL},
    {"nid2obj",                  _ssl_nid2obj,                            METH_VARARGS,                   NULL},
    {"get_sigalgs",              _ssl_get_sigalgs,                        METH_VARARGS,                   NULL},
    {"get_default_verify_paths", _ssl_get_default_verify_paths,           METH_NOARGS,                    NULL},
    {NULL, NULL}
};

static struct PyModuleDef _sslmodule = {
    PyModuleDef_HEAD_INIT, "_ssl", NULL, -1, _ssl_methods
};

PyMODINIT_FUNC PyInit__ssl(void)
{
    psa_crypto_init();

    if (PyType_Ready(&PySSLContext_Type)   < 0) return NULL;
    if (PyType_Ready(&PyWiiSSL_Type)       < 0) return NULL;
    if (PyType_Ready(&PySSLMemoryBIO_Type) < 0) return NULL;
    if (PyType_Ready(&PySSLSession_Type)   < 0) return NULL;

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

    Py_INCREF(&PySSLContext_Type);
    Py_INCREF(&PyWiiSSL_Type);
    Py_INCREF(&PySSLMemoryBIO_Type);
    Py_INCREF(&PySSLSession_Type);
    PyModule_AddObject(m, "_SSLContext", (PyObject *)&PySSLContext_Type);
    PyModule_AddObject(m, "_SSLObject",  (PyObject *)&PyWiiSSL_Type);
    PyModule_AddObject(m, "MemoryBIO",   (PyObject *)&PySSLMemoryBIO_Type);
    PyModule_AddObject(m, "SSLSession",  (PyObject *)&PySSLSession_Type);

#define AI(k, v) PyModule_AddIntConstant(m, k, v)
#define AS(k, v) PyModule_AddStringConstant(m, k, v)
    AI("OPENSSL_VERSION_NUMBER", 0x10101000L);
    AS("OPENSSL_VERSION",        "mbedTLS 4.0 (Wii)");
    PyModule_AddObject(m, "OPENSSL_VERSION_INFO",
                       Py_BuildValue("(iiiii)", 1, 1, 1, 0, 0));
    PyModule_AddObject(m, "_OPENSSL_API_VERSION",
                       Py_BuildValue("(iii)", 1, 1, 1));
    AS("_DEFAULT_CIPHERS", "");

    AI("HAS_SNI",  1);  /* mbedTLS does SNI */
    AI("HAS_ECDH", 0); AI("HAS_NPN", 0); AI("HAS_ALPN", 1);
    AI("HAS_SSLv2", 0); AI("HAS_SSLv3", 0); AI("HAS_TLSv1", 0);
    AI("HAS_TLSv1_1", 0); AI("HAS_TLSv1_2", 1); AI("HAS_TLSv1_3", 1);
    AI("HAS_PSK", 0); AI("HAS_PSK_TLS13", 0); AI("HAS_PHA", 0);

    AI("PROTO_MINIMUM_SUPPORTED", -2); AI("PROTO_MAXIMUM_SUPPORTED", -1);
    AI("PROTO_SSLv3",   0x0300); AI("PROTO_TLSv1",   0x0301);
    AI("PROTO_TLSv1_1", 0x0302); AI("PROTO_TLSv1_2", 0x0303);
    AI("PROTO_TLSv1_3", 0x0304);

    AI("HOSTFLAG_NEVER_CHECK_SUBJECT", 0x02);
    AI("VERIFY_DEFAULT", 0); AI("VERIFY_CRL_CHECK_LEAF",   0x04);
    AI("VERIFY_CRL_CHECK_CHAIN", 0x0c); AI("VERIFY_X509_STRICT",     0x20);
    AI("VERIFY_ALLOW_PROXY_CERTS", 0x40); AI("VERIFY_X509_PARTIAL_CHAIN", 0x80000);
    AI("ENCODING_DER", 1); AI("ENCODING_PEM", 2);

    AI("SSL_ERROR_ZERO_RETURN", 6); AI("SSL_ERROR_WANT_READ",    2);
    AI("SSL_ERROR_WANT_WRITE",  3); AI("SSL_ERROR_WANT_X509_LOOKUP", 4);
    AI("SSL_ERROR_SYSCALL", 5); AI("SSL_ERROR_SSL", 1);
    AI("SSL_ERROR_WANT_CONNECT", 7); AI("SSL_ERROR_EOF", 8);
    AI("SSL_ERROR_INVALID_ERROR_CODE", 10);

    AI("OP_ALL", 0); AI("OP_NO_SSLv2", 0); AI("OP_NO_SSLv3", 0);
    AI("OP_NO_TLSv1", 0); AI("OP_NO_TLSv1_1", 0);
    AI("OP_NO_TLSv1_2", 0); AI("OP_NO_TLSv1_3", 0);
    AI("OP_CIPHER_SERVER_PREFERENCE", 0); AI("OP_SINGLE_DH_USE", 0);
    AI("OP_SINGLE_ECDH_USE", 0); AI("OP_NO_COMPRESSION", 0);
    AI("OP_NO_RENEGOTIATION", 0); AI("OP_ENABLE_MIDDLEBOX_COMPAT", 0);
    AI("OP_IGNORE_UNEXPECTED_EOF", 0); AI("OP_LEGACY_SERVER_CONNECT", 0);
    AI("OP_ENABLE_KTLS", 0);

    AI("CERT_NONE", 0); AI("CERT_OPTIONAL", 1); AI("CERT_REQUIRED", 2);
    AI("PROTOCOL_TLS", 2); AI("PROTOCOL_TLS_CLIENT", 16);
    AI("PROTOCOL_TLS_SERVER", 17); AI("PROTOCOL_SSLv23", 2);
    AI("ALERT_DESCRIPTION_HANDSHAKE_FAILURE", 40);
    AI("ALERT_DESCRIPTION_CERTIFICATE_EXPIRED", 45);
#undef AI
#undef AS

    return m;
}
