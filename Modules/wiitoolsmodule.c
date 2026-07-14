/* Wii helper module for embedded Python on Wii. */

#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "lodepng/lodepng.h"
#include "lodepng/lodepng.c"

#ifndef HW_RVL
#define HW_RVL
#endif

#include <my_text_renderer.h>
#include "../fat/include/pyfat.h"
#include <gccore.h>
#include <video.h>
#include <wiiuse/wpad.h>
#include <ogc/pad.h>
#include <gx.h>
#include <assert.h>
#include <ogc/conf.h>
#include <ogc/system.h>
#include <ogc/audio.h>
#include <ogc/cache.h>
#include <ogc/consol.h>
#include <sys/time.h>

#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include "../curl/include/curl/curl.h"
#include "../build-wii/curl/mbedtls/install-wii/include/mbedtls/platform.h"
#define NETWORK_H22
#include <network.h>

#define __XSI_VISIBLE 600
#define __POSIX_VISIBLE 200112
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
//#include <sys/socket.h>
extern char font8x8_basic[128][8];

/* ====================================================================== *
 *  Sicherheit: deterministische Vorbedingungs-Checks                     *
 *                                                                        *
 *  Aus Python darf kein harter DSI/ISI-Halt entstehen. Statt einen       *
 *  CPU-Fault abzufangen (fragil), prüfen die Wrapper VOR dem libogc-      *
 *  Aufruf, ob die Voraussetzung erfüllt ist (Subsystem initialisiert,    *
 *  Controller verbunden, gültiger Kanal/Adresse) und werfen sonst eine   *
 *  normale, in Python abfangbare Exception.                              *
 * ====================================================================== */

/* Init-Status der Subsysteme (aus Python-Sicht). */
static int g_wpad_inited  = 0;
static int g_pad_inited   = 0;
static int g_audio_inited = 0;

/* Prüft, ob [addr, addr+len) vollständig in gültigem Wii-RAM liegt
   (MEM1/MEM2, cached + uncached). 0 = ungültig. */
static int wt_addr_range_ok(u32 a, u32 len)
{
    u32 end;
    if (len == 0) len = 1;
    end = a + len;
    if (end < a) return 0;                                  /* Overflow */
    if (a >= 0x80000000u && end <= 0x81800000u) return 1;   /* MEM1 cached  (24 MB) */
    if (a >= 0xC0000000u && end <= 0xC1800000u) return 1;   /* MEM1 uncached */
    if (a >= 0x90000000u && end <= 0x94000000u) return 1;   /* MEM2 cached  (64 MB) */
    if (a >= 0xD0000000u && end <= 0xD4000000u) return 1;   /* MEM2 uncached */
    return 0;
}

/* Subsystem muss initialisiert sein, sonst RuntimeError. Rückgabe -1 = Fehler. */
static int wt_require(int inited, const char *what)
{
    if (!inited) {
        PyErr_Format(PyExc_RuntimeError,
            "%s ist nicht initialisiert — zuerst die passende *_Init-Funktion aufrufen", what);
        return -1;
    }
    return 0;
}

/* WPAD initialisiert + gültiger Kanal + Controller verbunden (Probe ok).
   Verhindert, dass libogc mit uninitialisiertem/leerem Zustand deref't (DSI).
   Rückgabe -1 = Fehler (Python-Exception gesetzt). */
static int wt_wpad_channel_ready(int chan)
{
    u32 type = 0;
    s32 pr;
    if (wt_require(g_wpad_inited, "WPAD") != 0)
        return -1;
    if (chan < 0 || chan >= WPAD_MAX_WIIMOTES) {
        PyErr_SetString(PyExc_ValueError, "chan must be 0..3");
        return -1;
    }
    /* WPAD_Probe ist selbst gefahrlos (prüft intern __wpads_inited) und meldet,
       ob auf dem Kanal ein Controller verbunden und der Handshake fertig ist. */
    pr = WPAD_Probe((s32)chan, &type);
    if (pr != WPAD_ERR_NONE) {
        PyErr_Format(PyExc_RuntimeError,
            "kein Controller auf Kanal %d (WPAD_Probe=%d)", chan, (int)pr);
        return -1;
    }
    return 0;
}

static int net_ready = 0;
static void *xfb2 = NULL;

static void *net_thread(void *arg) {
    int ret = net_init();
    if (ret >= 0)
        *((int*)arg) = 1;   // net_ready setzen
    return NULL;
}

GXRModeObj* rmode3;
void* framebuffer3;

static WPADData g_event_bufs[WPAD_MAX_WIIMOTES][8];
static PADStatus g_pad_status[PAD_CHANMAX];
static s32 g_power_button_chan = -999;
static s32 g_battery_dead_chan = -999;
static int g_net_inited = 0;
static int g_curl_inited = 0;
static int g_entropy_seeded = 0;
typedef struct wiitools_png_image {
    char *name;
    unsigned char *rgba;
    unsigned w;
    unsigned h;
    unsigned char *tex_rgba8;
    unsigned tex_w;
    unsigned tex_h;
    struct wiitools_png_image *next;
} wiitools_png_image;
static wiitools_png_image *g_png_images = NULL;
static wiitools_png_image *g_png_current = NULL;
static wiitools_png_image *g_render_target = NULL;
static int g_render_target_dirty = 0;
extern const unsigned char test_png[] __attribute__((weak));
extern const unsigned char test_png_end[] __attribute__((weak));
static int g_ycbcr_lut_ready = 0;
static int16_t g_y_r[256], g_y_g[256], g_y_b[256];
static int16_t g_cb_r[256], g_cb_g[256], g_cb_b[256];
static int16_t g_cr_r[256], g_cr_g[256], g_cr_b[256];

static unsigned u8_clamp_int(int v)
{
    if (v < 0) return 0u;
    if (v > 255) return 255u;
    return (unsigned)v;
}

int mbedtls_platform_get_entropy(psa_driver_get_entropy_flags_t flags,
                                 size_t *estimate_bits,
                                 unsigned char *output, size_t output_size)
{
    size_t i;
    unsigned long t;
    (void)flags;
    if (!g_entropy_seeded) {
        srand((unsigned)(time(NULL) ^ (time_t)clock()));
        g_entropy_seeded = 1;
    }
    t = (unsigned long)clock();
    for (i = 0; i < output_size; ++i) {
        unsigned long r = (unsigned long)rand();
        r ^= (t + (unsigned long)(i * 1103515245u));
        output[i] = (unsigned char)(r & 0xFFu);
    }
    if (estimate_bits != NULL)
        *estimate_bits = output_size * 8u;
    return 0;
}

mbedtls_ms_time_t mbedtls_ms_time(void)
{
    clock_t c = clock();
    if (c <= 0)
        return (mbedtls_ms_time_t)0;
    return (mbedtls_ms_time_t)((((unsigned long)c) * 1000u) / (unsigned long)CLOCKS_PER_SEC);
}

static void rgb_to_ycbcr(unsigned r, unsigned g, unsigned b,
                         unsigned *y, unsigned *cb, unsigned *cr)
{
    int yi = (int)g_y_r[r] + (int)g_y_g[g] + (int)g_y_b[b] + 16;
    int cbi = (int)g_cb_r[r] + (int)g_cb_g[g] + (int)g_cb_b[b] + 128;
    int cri = (int)g_cr_r[r] + (int)g_cr_g[g] + (int)g_cr_b[b] + 128;
    *y = u8_clamp_int(yi);
    *cb = u8_clamp_int(cbi);
    *cr = u8_clamp_int(cri);
}

static void png_init_ycbcr_lut(void)
{
    int i;
    if (g_ycbcr_lut_ready)
        return;
    for (i = 0; i < 256; i++) {
        g_y_r[i] = (int16_t)((66 * i + 128) >> 8);
        g_y_g[i] = (int16_t)((129 * i + 128) >> 8);
        g_y_b[i] = (int16_t)((25 * i + 128) >> 8);

        g_cb_r[i] = (int16_t)((-38 * i + 128) >> 8);
        g_cb_g[i] = (int16_t)((-74 * i + 128) >> 8);
        g_cb_b[i] = (int16_t)((112 * i + 128) >> 8);

        g_cr_r[i] = (int16_t)((112 * i + 128) >> 8);
        g_cr_g[i] = (int16_t)((-94 * i + 128) >> 8);
        g_cr_b[i] = (int16_t)((-18 * i + 128) >> 8);
    }
    g_ycbcr_lut_ready = 1;
}

static void wiitools_power_button_cb(s32 chan) { g_power_button_chan = chan; }
static void wiitools_battery_dead_cb(s32 chan) { g_battery_dead_chan = chan; }

static PyObject* py_none(void)
{
    Py_INCREF(Py_None);
    return Py_None;
}

static int wiitools_get_bytes(PyObject *obj, const char **out, Py_ssize_t *len,
                              PyObject **temp_bytes)
{
    if (obj == NULL) {
        PyErr_SetString(PyExc_TypeError, "expected bytes or str");
        return -1;
    }
    if (PyBytes_Check(obj)) {
        if (PyBytes_AsStringAndSize(obj, (char **)out, len) != 0)
            return -1;
        *temp_bytes = NULL;
        return 0;
    }
    if (PyByteArray_Check(obj)) {
        *out = PyByteArray_AsString(obj);
        if (*out == NULL)
            return -1;
        *len = PyByteArray_Size(obj);
        *temp_bytes = NULL;
        return 0;
    }
    if (PyUnicode_Check(obj)) {
        PyObject *utf8 = PyUnicode_AsUTF8String(obj);
        if (utf8 == NULL)
            return -1;
        *out = PyBytes_AS_STRING(utf8);
        *len = PyBytes_GET_SIZE(utf8);
        *temp_bytes = utf8;
        return 0;
    }
    PyErr_SetString(PyExc_TypeError, "expected bytes or str");
    return -1;
}

typedef struct wiitools_curl_buf {
    char *data;
    size_t len;
    size_t cap;
} wiitools_curl_buf;

static void wiitools_curl_buf_free(wiitools_curl_buf *b)
{
    if (b == NULL)
        return;
    if (b->data != NULL) {
        free(b->data);
        b->data = NULL;
    }
    b->len = 0;
    b->cap = 0;
}

static int wiitools_curl_buf_reserve(wiitools_curl_buf *b, size_t want)
{
    char *p;
    size_t next_cap;
    if (b->cap >= want)
        return 0;
    next_cap = b->cap ? b->cap : 1024u;
    while (next_cap < want) {
        if (next_cap > ((size_t)-1) / 2u)
            return -1;
        next_cap *= 2u;
    }
    p = (char *)realloc(b->data, next_cap);
    if (p == NULL)
        return -1;
    b->data = p;
    b->cap = next_cap;
    return 0;
}

static int wiitools_curl_buf_append(wiitools_curl_buf *b, const char *src, size_t n)
{
    size_t need;
    if (n == 0)
        return 0;
    need = b->len + n + 1u;
    if (need < b->len)
        return -1;
    if (wiitools_curl_buf_reserve(b, need) != 0)
        return -1;
    memcpy(b->data + b->len, src, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

static size_t wiitools_curl_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    wiitools_curl_buf *b = (wiitools_curl_buf *)userdata;
    size_t n = size * nmemb;
    if (b == NULL)
        return 0;
    if (wiitools_curl_buf_append(b, ptr, n) != 0)
        return 0;
    return n;
}

/* libcurl debug callback: append verbose/debug output into a buffer */
static int wiitools_curl_debug_cb(CURL *handle, curl_infotype type,
                                  char *data, size_t size, void *userdata)
{
    wiitools_curl_buf *b = (wiitools_curl_buf *)userdata;
    (void)handle;
    (void)type;
    if (b == NULL || data == NULL || size == 0)
        return 0;
    /* Append as-is (may contain binary, but verbose output is text) */
    if (wiitools_curl_buf_append(b, data, size) != 0)
        return 0;
    return 0;
}

static int wiitools_curl_ensure_runtime(void)
{
    CURLcode cc;
    if (!g_net_inited) {
        /* net_thread runs net_init() in the background; calling it again hangs.
           Caller (Python) must ensure IsNetReady() == 1 before using curl. */
        if (!net_ready) {
            PyErr_SetString(PyExc_RuntimeError, "Network not ready — call wiitools.IsNetReady() first");
            return -1;
        }
        g_net_inited = 1;
    }
    if (!g_curl_inited) {
        cc = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (cc != CURLE_OK) {
            PyErr_Format(PyExc_RuntimeError, "curl_global_init failed: %s", curl_easy_strerror(cc));
            return -1;
        }
        g_curl_inited = 1;
    }
    return 0;
}

static struct curl_slist *wiitools_curl_parse_headers(PyObject *headers_obj)
{
    struct curl_slist *headers = NULL;
    int i, n;
    if (headers_obj == NULL || headers_obj == Py_None)
        return NULL;
    if (!PyList_Check(headers_obj) && !PyTuple_Check(headers_obj)) {
        PyErr_SetString(PyExc_TypeError, "headers must be a list/tuple of str/bytes");
        return NULL;
    }
    n = PySequence_Size(headers_obj);
    for (i = 0; i < n; ++i) {
        PyObject *item = PySequence_GetItem(headers_obj, i);
        const char *line;
        Py_ssize_t line_len = 0;
        PyObject *tmp_bytes = NULL;
        if (item == NULL) {
            curl_slist_free_all(headers);
            return NULL;
        }
        if (wiitools_get_bytes(item, &line, &line_len, &tmp_bytes) != 0) {
            Py_DECREF(item);
            curl_slist_free_all(headers);
            return NULL;
        }
        headers = curl_slist_append(headers, line);
        Py_XDECREF(tmp_bytes);
        Py_DECREF(item);
        if (headers == NULL) {
            PyErr_NoMemory();
            return NULL;
        }
    }
    return headers;
}

static PyObject *wiitools_curl_build_result(CURLcode cc, const char *error_buf,
                                            long status_code, const char *effective_url,
                                            const wiitools_curl_buf *body,
                                            const wiitools_curl_buf *resp_headers)
{
    PyObject *res;
    PyObject *v;
    const char *err_text;
    res = PyDict_New();
    if (res == NULL)
        return NULL;

    v = PyLong_FromLong((long)(cc == CURLE_OK));
    if (v == NULL) goto fail;
    if (PyDict_SetItemString(res, "ok", v) != 0) { Py_DECREF(v); goto fail; }
    Py_DECREF(v);

    v = PyLong_FromLong(status_code);
    if (v == NULL) goto fail;
    if (PyDict_SetItemString(res, "status", v) != 0) { Py_DECREF(v); goto fail; }
    Py_DECREF(v);

    v = PyBytes_FromStringAndSize(body && body->data ? body->data : "", body ? (Py_ssize_t)body->len : 0);
    if (v == NULL) goto fail;
    if (PyDict_SetItemString(res, "body", v) != 0) { Py_DECREF(v); goto fail; }
    Py_DECREF(v);

    v = PyBytes_FromStringAndSize(resp_headers && resp_headers->data ? resp_headers->data : "",
                                  resp_headers ? (Py_ssize_t)resp_headers->len : 0);
    if (v == NULL) goto fail;
    if (PyDict_SetItemString(res, "headers", v) != 0) { Py_DECREF(v); goto fail; }
    Py_DECREF(v);

    err_text = "";
    if (cc != CURLE_OK) {
        if (error_buf != NULL && error_buf[0] != '\0')
            err_text = error_buf;
        else
            err_text = curl_easy_strerror(cc);
    }
    v = PyUnicode_FromString(err_text);
    if (v == NULL) goto fail;
    if (PyDict_SetItemString(res, "error", v) != 0) { Py_DECREF(v); goto fail; }
    Py_DECREF(v);

    v = PyUnicode_FromString(effective_url != NULL ? effective_url : "");
    if (v == NULL) goto fail;
    if (PyDict_SetItemString(res, "url", v) != 0) { Py_DECREF(v); goto fail; }
    Py_DECREF(v);

    /* Expose numeric libcurl error code for easier diagnosis */
    v = PyLong_FromLong((long)cc);
    if (v == NULL) goto fail;
    if (PyDict_SetItemString(res, "curl_code", v) != 0) { Py_DECREF(v); goto fail; }
    Py_DECREF(v);

    return res;
fail:
    Py_DECREF(res);
    return NULL;
}

static PyObject *wiitools_curl_perform(const char *method, const char *url,
                                       const char *data, int data_len,
                                       PyObject *headers_obj, int timeout_ms,
                                       int verify_peer, int verify_host,
                                       int follow_redirects,
                                       const char *user_agent,
                                       const char *ca_file,
                                       int verbose)
{
    CURL *easy = NULL;
    struct curl_slist *headers = NULL;
    wiitools_curl_buf body;
    wiitools_curl_buf resp_headers;
    wiitools_curl_buf debug_buf;
    CURLcode cc = CURLE_OK;
    long status_code = 0;
    char error_buf[CURL_ERROR_SIZE];
    char *effective_url = NULL;
    PyObject *ret = NULL;
    int is_post = 0;
    int is_head = 0;

    memset(&body, 0, sizeof(body));
    memset(&resp_headers, 0, sizeof(resp_headers));
    memset(&debug_buf, 0, sizeof(debug_buf));
    memset(error_buf, 0, sizeof(error_buf));

    if (wiitools_curl_ensure_runtime() != 0)
        return NULL;

    easy = curl_easy_init();
    if (easy == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "curl_easy_init failed");
        return NULL;
    }
	//curl_easy_setopt(easy, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

    headers = wiitools_curl_parse_headers(headers_obj);
    if (headers_obj != NULL && headers_obj != Py_None && headers == NULL)
        goto done;

    if (method != NULL) {
        if (strcmp(method, "POST") == 0 || strcmp(method, "post") == 0)
            is_post = 1;
        else if (strcmp(method, "HEAD") == 0 || strcmp(method, "head") == 0)
            is_head = 1;
    }

    curl_easy_setopt(easy, CURLOPT_URL, url);
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, wiitools_curl_write_cb);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, wiitools_curl_write_cb);
    curl_easy_setopt(easy, CURLOPT_HEADERDATA, &resp_headers);
    curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, error_buf);
    /* Avoid signal() usage on Wii */
    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, follow_redirects ? 1L : 0L);
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, verify_peer ? 1L : 0L);
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, verify_host ? 2L : 0L);
    if (user_agent != NULL && user_agent[0] != '\0')
        curl_easy_setopt(easy, CURLOPT_USERAGENT, user_agent);
    if (ca_file != NULL && ca_file[0] != '\0')
        curl_easy_setopt(easy, CURLOPT_CAINFO, ca_file);
    if (headers != NULL)
        curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers);

    if (verbose) {
        curl_easy_setopt(easy, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(easy, CURLOPT_DEBUGFUNCTION, wiitools_curl_debug_cb);
        curl_easy_setopt(easy, CURLOPT_DEBUGDATA, &debug_buf);
    }

    if (timeout_ms > 0) {
        /* On Wii, CURLOPT_TIMEOUT_MS does not work reliably.
           Use only CURLOPT_CONNECTTIMEOUT like the working C curl examples. */
        curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, (long)((timeout_ms + 999) / 1000));
    }

    if (is_head) {
        /* CURLOPT_NOBODY hangs on Wii libcurl; use CUSTOMREQUEST instead */
        //curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, "HEAD");
        curl_easy_setopt(easy, CURLOPT_NOBODY, 1L);
    } else if (is_post) {
        curl_easy_setopt(easy, CURLOPT_POST, 1L);
        if (data != NULL) {
            curl_easy_setopt(easy, CURLOPT_POSTFIELDS, data);
            curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, (long)data_len);
        }
    } else if (method != NULL &&
               !(strcmp(method, "GET") == 0 || strcmp(method, "get") == 0)) {
        curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, method);
        if (data != NULL) {
            curl_easy_setopt(easy, CURLOPT_POSTFIELDS, data);
            curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, (long)data_len);
        }
    }

    cc = curl_easy_perform(easy);
    curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &status_code);
    curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &effective_url);

    ret = wiitools_curl_build_result(cc, error_buf, status_code, effective_url, &body, &resp_headers);
    /* attach verbose/debug log if requested */
    if (ret != NULL && debug_buf.data != NULL) {
        PyObject *vdbg = PyBytes_FromStringAndSize(debug_buf.data, (Py_ssize_t)debug_buf.len);
        if (vdbg != NULL) {
            PyDict_SetItemString(ret, "verbose", vdbg);
            Py_DECREF(vdbg);
        }
    }

done:
    wiitools_curl_buf_free(&body);
    wiitools_curl_buf_free(&resp_headers);
    wiitools_curl_buf_free(&debug_buf);
    if (headers != NULL)
        curl_slist_free_all(headers);
    if (easy != NULL)
        curl_easy_cleanup(easy);
    return ret;
}

static PyObject *wiitools_curl_request(PyObject *self, PyObject *args, PyObject *kwargs)
{
    const char *method;
    const char *url;
    PyObject *data_obj = Py_None;
    PyObject *headers_obj = Py_None;
    int timeout_ms = 30000;
    int verify_peer = 0;
    int verify_host = 0;
    int follow_redirects = 1;
    const char *user_agent = NULL;
    const char *ca_file = NULL;
    int verbose = 0;
    const char *data = NULL;
    Py_ssize_t data_len = 0;
    PyObject *data_tmp = NULL;
    static char *kwlist[] = {
        "method", "url", "data", "headers", "timeout_ms",
        "verify_peer", "verify_host", "follow_redirects",
        "user_agent", "ca_file", "verbose", NULL
    };
    (void)self;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ss|OOiiiizzi:curl_request", kwlist,
                                     &method, &url, &data_obj, &headers_obj, &timeout_ms,
                                     &verify_peer, &verify_host, &follow_redirects,
                                     &user_agent, &ca_file, &verbose))
        return NULL;
    if (data_obj != Py_None) {
        if (wiitools_get_bytes(data_obj, &data, &data_len, &data_tmp) != 0)
            return NULL;
    }
    {
        PyObject *ret = wiitools_curl_perform(method, url, data, (int)data_len, headers_obj, timeout_ms,
                                              verify_peer, verify_host, follow_redirects, user_agent, ca_file,
                                              verbose);
        Py_XDECREF(data_tmp);
        return ret;
    }
}

static PyObject *wiitools_curl_get(PyObject *self, PyObject *args, PyObject *kwargs)
{
    const char *url;
    PyObject *headers_obj = Py_None;
    int timeout_ms = 30000;
    int verify_peer = 0;
    int verify_host = 0;
    int follow_redirects = 1;
    const char *user_agent = NULL;
    const char *ca_file = NULL;
    int verbose = 0;
    static char *kwlist[] = {
        "url", "headers", "timeout_ms", "verify_peer",
        "verify_host", "follow_redirects", "user_agent", "ca_file", "verbose", NULL
    };
    (void)self;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|Oiiiizzi:curl_get", kwlist,
                                     &url, &headers_obj, &timeout_ms, &verify_peer,
                                     &verify_host, &follow_redirects, &user_agent, &ca_file, &verbose))
        return NULL;
    return wiitools_curl_perform("GET", url, NULL, 0, headers_obj, timeout_ms,
                                 verify_peer, verify_host, follow_redirects, user_agent, ca_file,
                                 verbose);
}

static PyObject *wiitools_curl_post(PyObject *self, PyObject *args, PyObject *kwargs)
{
    const char *url;
    const char *data = NULL;
    Py_ssize_t data_len = 0;
    PyObject *data_tmp = NULL;
    PyObject *data_obj = NULL;
    PyObject *headers_obj = Py_None;
    int timeout_ms = 30000;
    int verify_peer = 0;
    int verify_host = 0;
    int follow_redirects = 1;
    const char *user_agent = NULL;
    const char *ca_file = NULL;
    int verbose = 0;
    static char *kwlist[] = {
        "url", "data", "headers", "timeout_ms", "verify_peer",
        "verify_host", "follow_redirects", "user_agent", "ca_file", "verbose", NULL
    };
    (void)self;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|Oiiiizzi:curl_post", kwlist,
                                     &url, &data_obj, &headers_obj, &timeout_ms,
                                     &verify_peer, &verify_host, &follow_redirects,
                                     &user_agent, &ca_file, &verbose))
        return NULL;
    if (data_obj == NULL) {
        PyErr_SetString(PyExc_TypeError, "data is required");
        return NULL;
    }
    if (wiitools_get_bytes(data_obj, &data, &data_len, &data_tmp) != 0)
        return NULL;
    {
        PyObject *ret = wiitools_curl_perform("POST", url, data, (int)data_len, headers_obj, timeout_ms,
                                              verify_peer, verify_host, follow_redirects, user_agent, ca_file,
                                              verbose);
        Py_XDECREF(data_tmp);
        return ret;
    }
}

static char *wiitools_strdup(const char *s)
{
    size_t n;
    char *copy;
    if (s == NULL)
        return NULL;
    n = strlen(s);
    copy = (char *)malloc(n + 1u);
    if (copy == NULL)
        return NULL;
    memcpy(copy, s, n + 1u);
    return copy;
}

static void png_unload_image_data(wiitools_png_image *img)
{
    if (img == NULL)
        return;
    if (img->tex_rgba8 != NULL) {
        free(img->tex_rgba8);
        img->tex_rgba8 = NULL;
    }
    if (img->rgba != NULL) {
        free(img->rgba);
        img->rgba = NULL;
    }
    img->tex_w = 0;
    img->tex_h = 0;
    img->w = 0;
    img->h = 0;
}

static wiitools_png_image *png_find_image(const char *name)
{
    wiitools_png_image *it = g_png_images;
    while (it != NULL) {
        if (strcmp(it->name, name) == 0)
            return it;
        it = it->next;
    }
    return NULL;
}

static wiitools_png_image *png_get_or_create_image(const char *name)
{
    wiitools_png_image *img = png_find_image(name);
    if (img != NULL)
        return img;

    img = (wiitools_png_image *)malloc(sizeof(*img));
    if (img == NULL)
        return NULL;
    memset(img, 0, sizeof(*img));
    img->name = wiitools_strdup(name);
    if (img->name == NULL) {
        free(img);
        return NULL;
    }
    img->next = g_png_images;
    g_png_images = img;
    return img;
}

static void png_remove_image(const char *name)
{
    wiitools_png_image *prev = NULL;
    wiitools_png_image *it = g_png_images;

    while (it != NULL) {
        if (strcmp(it->name, name) == 0) {
            if (prev != NULL)
                prev->next = it->next;
            else
                g_png_images = it->next;
            if (g_png_current == it)
                g_png_current = NULL;
            png_unload_image_data(it);
            if (it->name != NULL)
                free(it->name);
            free(it);
            return;
        }
        prev = it;
        it = it->next;
    }
}

static void png_clear_all_images(void)
{
    wiitools_png_image *it = g_png_images;
    while (it != NULL) {
        wiitools_png_image *next = it->next;
        png_unload_image_data(it);
        if (it->name != NULL)
            free(it->name);
        free(it);
        it = next;
    }
    g_png_images = NULL;
    g_png_current = NULL;
}

static unsigned next_pow2_u(unsigned v)
{
    unsigned p = 1;
    while (p < v && p < 4096u)
        p <<= 1;
    return p;
}

static int png_build_gx_texture(wiitools_png_image *img)
{
    unsigned tx, ty;
    unsigned tw, th;
    unsigned char *out;
    if (img == NULL || img->rgba == NULL)
        return -1;

    tw = next_pow2_u(img->w ? img->w : 1u);
    th = next_pow2_u(img->h ? img->h : 1u);
    if (tw < 4u) tw = 4u;
    if (th < 4u) th = 4u;

    out = (unsigned char *)memalign(32, (size_t)tw * (size_t)th * 4u);
    if (out == NULL)
        return -1;
    memset(out, 0, (size_t)tw * (size_t)th * 4u);

    for (ty = 0; ty < th; ty += 4u) {
        for (tx = 0; tx < tw; tx += 4u) {
            unsigned block = ((ty / 4u) * (tw / 4u) + (tx / 4u)) * 64u;
            unsigned by, bx;
            for (by = 0; by < 4u; by++) {
                for (bx = 0; bx < 4u; bx++) {
                    unsigned sx = tx + bx;
                    unsigned sy = ty + by;
                    unsigned dst = block + ((by * 4u + bx) * 2u);
                    unsigned src = (sy < img->h && sx < img->w)
                        ? ((sy * img->w + sx) * 4u)
                        : 0u;
                    unsigned char r = (sy < img->h && sx < img->w) ? img->rgba[src + 0] : 0u;
                    unsigned char g = (sy < img->h && sx < img->w) ? img->rgba[src + 1] : 0u;
                    unsigned char b = (sy < img->h && sx < img->w) ? img->rgba[src + 2] : 0u;
                    unsigned char a = (sy < img->h && sx < img->w) ? img->rgba[src + 3] : 0u;
                    out[dst + 0] = a;
                    out[dst + 1] = r;
                    out[dst + 32u + 0] = g;
                    out[dst + 32u + 1] = b;
                }
            }
        }
    }

    if (img->tex_rgba8 != NULL)
        free(img->tex_rgba8);
    img->tex_rgba8 = out;
    img->tex_w = tw;
    img->tex_h = th;
    DCStoreRange(img->tex_rgba8, (u32)((size_t)tw * (size_t)th * 4u));
    return 0;
}

/* --------------- FAT --------------- */
static PyObject* fatinit(PyObject *self, PyObject *args)
{
    int r;
    (void)self;
    if (!PyArg_ParseTuple(args, ":fatInitDefault"))
        return NULL;

    r = fatInitDefault();
    return PyLong_FromLong((long)r);
}

/* --------------- VIDEO --------------- */
static PyObject* video_waitvsync(PyObject *self, PyObject *args)
{
    (void)self;
    if (!PyArg_ParseTuple(args, ":VIDEO_WaitVSync"))
        return NULL;

    VIDEO_WaitVSync();

    return py_none();
}

static PyObject* usleep2(PyObject *self, PyObject *args) {
	int i;
    if (!PyArg_ParseTuple(args, "i:WPAD_ButtonsHeld", &i))
        return NULL;
	
	usleep(i);

	Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* file_remove(PyObject *self, PyObject *args)
{
    const char *path;
    int rc;
    (void)self;
    if (!PyArg_ParseTuple(args, "s:remove", &path))
        return NULL;
    rc = remove(path);
    return PyLong_FromLong((long)rc);
}

static PyObject* file_read(PyObject *self, PyObject *args)
{
    const char *path;
    FILE *f;
    long size;
    PyObject *bytes;
    size_t nread;
    (void)self;
    if (!PyArg_ParseTuple(args, "s:read_file", &path))
        return NULL;

    f = fopen(path, "rb");
    if (!f)
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
    }
    size = ftell(f);
    if (size < 0) {
        fclose(f);
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
    }
    rewind(f);

    bytes = PyBytes_FromStringAndSize(NULL, (Py_ssize_t)size);
    if (!bytes) { fclose(f); return NULL; }

    nread = fread(PyBytes_AS_STRING(bytes), 1, (size_t)size, f);
    fclose(f);
    if (nread != (size_t)size) {
        Py_DECREF(bytes);
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
    }
    return bytes;
}

static PyObject* file_write(PyObject *self, PyObject *args)
{
    const char *path;
    PyObject *obj;
    const char *data = NULL;
    Py_ssize_t len = 0;
    PyObject *temp = NULL;
    FILE *f;
    size_t nwritten;
    (void)self;
    if (!PyArg_ParseTuple(args, "sO:write_file", &path, &obj))
        return NULL;

    if (wiitools_get_bytes(obj, &data, &len, &temp) != 0)
        return NULL;

    f = fopen(path, "wb");
    if (!f) {
        Py_XDECREF(temp);
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
    }
    nwritten = fwrite(data, 1, (size_t)len, f);
    fclose(f);
    Py_XDECREF(temp);
    if (nwritten != (size_t)len)
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
    return PyLong_FromSsize_t((Py_ssize_t)nwritten);
}

static PyObject* terminal_init(PyObject *self, PyObject *args) {
	(void)self;
    if (!PyArg_ParseTuple(args, ":VIDEO_WaitVSync"))
        return NULL;

    video_init_custom();
	terminal_clear();

    return py_none();
}

#define FIFO_SIZE (256 * 1024)

static GXRModeObj* screenMode;
static void* frameBuffer[3];
static mqmsg_t frame = NULL;
static mqbox_t frame_draw;
static mqbox_t frame_empty;
static void* fifoBuffer = NULL;
static uint8_t colors[256 * 3] ATTRIBUTE_ALIGN(32);

static bool gfx_matrix_texture_prev = false;
static bool gfx_fog_prev = false;

static uint32_t gfx_depth_last = GX_TRUE;
static uint32_t gfx_depth_test_last = GX_TRUE;
static uint32_t gfx_depth_func_last = GX_LEQUAL;

static int gfx_screen_width = 802;
static const int gfx_width  = 802;
static const int gfx_height = 480;

static float gfx_texcoord_div = 256.0f;

/* logical screen size set via set_screen_size(); 0 = use physical */
static float g_logical_w = 0.0f;
static float g_logical_h = 0.0f;
static float g_vp_x = 0.0f;
static float g_vp_y = 0.0f;
static float g_vp_w = 0.0f;
static float g_vp_h = 0.0f;

static void get_logical_size(float *w, float *h) {
    if (g_logical_w > 0.0f) { *w = g_logical_w; *h = g_logical_h; }
    else if (screenMode) { *w = (float)screenMode->fbWidth; *h = (float)screenMode->xfbHeight; }
    else { *w = 802.0f; *h = 460.0f; }
}

static volatile mqmsg_t current_frame = NULL;
static mqmsg_t g_draw_target = NULL;
static int g_frame_dirty = 0;

/* 1, wenn wir das Rendering eines Hosts uebernommen haben (rendering_adopt).
   Dann flippt do_render_flush direkt (VIDEO_SetNextFramebuffer) statt ueber die
   frame_draw-MessageQueue, fuer die es hier keinen Consumer/Callback gibt. */
static int g_adopt_mode = 0;

static int png_get_render_target(u32 **out_fb, unsigned *out_w, unsigned *out_h)
{
    if (current_frame != NULL && screenMode != NULL) {
        if (g_draw_target == NULL) {
            mqmsg_t next = NULL;
            /* Match gfx pipeline: always draw into a free backbuffer. */
            MQ_Receive(frame_empty, &next, MQ_MSG_BLOCK);
            g_draw_target = next;
        }
        *out_fb = (u32 *)g_draw_target;
        *out_w = (unsigned)screenMode->fbWidth;
        *out_h = (unsigned)screenMode->xfbHeight;
        return 0;
    }

    *out_fb = (u32 *)get_framebuffer();
    if (*out_fb == NULL || get_rmode() == NULL)
        return -1;

    *out_w = (unsigned)get_rmode()->fbWidth;
    *out_h = (unsigned)get_rmode()->xfbHeight;
    return 0;
}

static void png_put_pixel_xfb(u32 *fb, unsigned fb_pairs_per_row, int dx, int dy,
                              unsigned r, unsigned g, unsigned b)
{
    unsigned yy, cb, cr;
    unsigned pair_x = ((unsigned)dx) & ~1u;
    unsigned pair_idx = (unsigned)dy * fb_pairs_per_row + (pair_x >> 1);
    u32 pair = fb[pair_idx];
    unsigned y1 = (pair >> 24) & 0xffu;
    unsigned p_cb = (pair >> 16) & 0xffu;
    unsigned y2 = (pair >> 8) & 0xffu;
    unsigned p_cr = pair & 0xffu;

    rgb_to_ycbcr(r, g, b, &yy, &cb, &cr);
    if (((unsigned)dx & 1u) == 0u) {
        y1 = yy;
    } else {
        y2 = yy;
    }
    p_cb = cb;
    p_cr = cr;
    fb[pair_idx] = ((u32)y1 << 24) | ((u32)p_cb << 16) | ((u32)y2 << 8) | (u32)p_cr;
}

/* ===== Software surface renderer ===== */

static void surface_blend_pixel(wiitools_png_image *surf, int x, int y,
                                 unsigned char sr, unsigned char sg,
                                 unsigned char sb, unsigned char sa)
{
    unsigned idx;
    unsigned char *dst;
    if (x < 0 || y < 0 || (unsigned)x >= surf->w || (unsigned)y >= surf->h)
        return;
    idx = ((unsigned)y * surf->w + (unsigned)x) * 4u;
    dst = surf->rgba + idx;
    if (sa == 0) return;
    if (sa == 255 || dst[3] == 0) {
        dst[0] = sr; dst[1] = sg; dst[2] = sb; dst[3] = sa;
        return;
    }
    {
        unsigned inv = 255u - (unsigned)sa;
        unsigned oa = (unsigned)sa + (unsigned)dst[3] * inv / 255u;
        if (oa > 255u) oa = 255u;
        dst[0] = (unsigned char)(((unsigned)sr * sa + (unsigned)dst[0] * (unsigned)dst[3] * inv / 255u) / oa);
        dst[1] = (unsigned char)(((unsigned)sg * sa + (unsigned)dst[1] * (unsigned)dst[3] * inv / 255u) / oa);
        dst[2] = (unsigned char)(((unsigned)sb * sa + (unsigned)dst[2] * (unsigned)dst[3] * inv / 255u) / oa);
        dst[3] = (unsigned char)oa;
    }
}

static void surface_sw_blit_img(wiitools_png_image *src,
                                 int sx, int sy, int sw2, int sh2,
                                 wiitools_png_image *dst,
                                 int dx, int dy, int dw, int dh)
{
    int tx, ty;
    unsigned sx_step_fp, sy_step_fp, sy_fp;
    if (src == NULL || src->rgba == NULL || dst == NULL || dst->rgba == NULL)
        return;
    if (sw2 <= 0 || sh2 <= 0 || dw <= 0 || dh <= 0) return;
    sx_step_fp = ((unsigned)sw2 << 16) / (unsigned)dw;
    sy_step_fp = ((unsigned)sh2 << 16) / (unsigned)dh;
    sy_fp = 0;
    for (ty = 0; ty < dh; ty++) {
        int dyi = dy + ty;
        int syi = sy + (int)(sy_fp >> 16);
        unsigned sx_fp2 = 0;
        if (dyi < 0 || (unsigned)dyi >= dst->h) { sy_fp += sy_step_fp; continue; }
        if (syi < 0 || (unsigned)syi >= src->h) { sy_fp += sy_step_fp; continue; }
        for (tx = 0; tx < dw; tx++) {
            int dxi = dx + tx;
            int sxi = sx + (int)(sx_fp2 >> 16);
            unsigned sidx;
            if (dxi < 0 || (unsigned)dxi >= dst->w) { sx_fp2 += sx_step_fp; continue; }
            if (sxi < 0 || (unsigned)sxi >= src->w) { sx_fp2 += sx_step_fp; continue; }
            sidx = ((unsigned)syi * src->w + (unsigned)sxi) * 4u;
            surface_blend_pixel(dst, dxi, dyi,
                                src->rgba[sidx], src->rgba[sidx+1],
                                src->rgba[sidx+2], src->rgba[sidx+3]);
            sx_fp2 += sx_step_fp;
        }
        sy_fp += sy_step_fp;
    }
}

static void surface_sw_draw_rect(wiitools_png_image *surf,
                                  int x, int y, int w, int h,
                                  unsigned char r, unsigned char g,
                                  unsigned char b, unsigned char a,
                                  float angle_deg)
{
    float cx = (float)x + (float)w * 0.5f;
    float cy = (float)y + (float)h * 0.5f;
    float hw = (float)w * 0.5f, hh = (float)h * 0.5f;
    float rad = angle_deg * (3.14159265358979323846f / 180.0f);
    float cs = cosf(rad), sn = sinf(rad);
    float diag = sqrtf(hw*hw + hh*hh) + 1.5f;
    int x0 = (int)(cx - diag), x1 = (int)(cx + diag) + 1;
    int y0 = (int)(cy - diag), y1 = (int)(cy + diag) + 1;
    int px, py;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)surf->w) x1 = (int)surf->w;
    if (y1 > (int)surf->h) y1 = (int)surf->h;
    for (py = y0; py < y1; py++) {
        for (px = x0; px < x1; px++) {
            float lx = ((float)px - cx) * cs + ((float)py - cy) * sn;
            float ly = -((float)px - cx) * sn + ((float)py - cy) * cs;
            if (lx >= -hw && lx <= hw && ly >= -hh && ly <= hh)
                surface_blend_pixel(surf, px, py, r, g, b, a);
        }
    }
}

static void surface_sw_draw_circle(wiitools_png_image *surf,
                                    int x, int y, int radius,
                                    unsigned char r, unsigned char g,
                                    unsigned char b, unsigned char a)
{
    float r2 = (float)radius * (float)radius;
    int x0 = x - radius, x1 = x + radius + 1;
    int y0 = y - radius, y1 = y + radius + 1;
    int px, py;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)surf->w) x1 = (int)surf->w;
    if (y1 > (int)surf->h) y1 = (int)surf->h;
    for (py = y0; py < y1; py++) {
        float dy = (float)py - (float)y;
        for (px = x0; px < x1; px++) {
            float dx = (float)px - (float)x;
            if (dx*dx + dy*dy <= r2)
                surface_blend_pixel(surf, px, py, r, g, b, a);
        }
    }
}

static void surface_sw_draw_oval(wiitools_png_image *surf,
                                  int x, int y, int w, int h,
                                  unsigned char r, unsigned char g,
                                  unsigned char b, unsigned char a,
                                  float angle_deg)
{
    float cx = (float)x + (float)w * 0.5f;
    float cy = (float)y + (float)h * 0.5f;
    float rx = (float)w * 0.5f, ry = (float)h * 0.5f;
    float rad = angle_deg * (3.14159265358979323846f / 180.0f);
    float cs = cosf(rad), sn = sinf(rad);
    float diag = (rx > ry ? rx : ry) + 1.5f;
    int x0 = (int)(cx - diag), x1 = (int)(cx + diag) + 1;
    int y0 = (int)(cy - diag), y1 = (int)(cy + diag) + 1;
    int px, py;
    if (rx <= 0.0f || ry <= 0.0f) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)surf->w) x1 = (int)surf->w;
    if (y1 > (int)surf->h) y1 = (int)surf->h;
    for (py = y0; py < y1; py++) {
        for (px = x0; px < x1; px++) {
            float ddx = (float)px - cx, ddy = (float)py - cy;
            float lx = ddx * cs + ddy * sn;
            float ly = -ddx * sn + ddy * cs;
            float ex = lx / rx, ey = ly / ry;
            if (ex*ex + ey*ey <= 1.0f)
                surface_blend_pixel(surf, px, py, r, g, b, a);
        }
    }
}

static void surface_sw_render_text(wiitools_png_image *surf,
                                    int x, int y, const char *text,
                                    int size, int shadow,
                                    unsigned char r, unsigned char g,
                                    unsigned char b, unsigned char a,
                                    float angle_deg)
{
    int scale = (size <= 0) ? 1 : size;
    int shadow_off = scale / 4;
    float rad = angle_deg * (3.14159265358979323846f / 180.0f);
    float cs = cosf(rad), sn = sinf(rad);
    size_t len = strlen(text);
    int cx, cy, i, row, col, dx2, dy2;
    if (shadow_off < 1) shadow_off = 1;

    if (shadow) {
        cx = x; cy = y;
        for (i = 0; i < (int)len; i++) {
            unsigned char ch = (unsigned char)text[i];
            if (ch == '\n') { cy += 9 * scale; cx = x; continue; }
            if (ch > 127) ch = '?';
            for (row = 0; row < 8; row++) {
                unsigned char bits = (unsigned char)font8x8_basic[ch][row];
                for (col = 0; col < 8; col++) {
                    if (!(bits & (1u << col))) continue;
                    for (dy2 = 0; dy2 < scale; dy2++) {
                        for (dx2 = 0; dx2 < scale; dx2++) {
                            float fx = (float)(cx + col*scale + shadow_off + dx2) - (float)x;
                            float fy = (float)(cy + row*scale + shadow_off + dy2) - (float)y;
                            float rx2 = fx*cs - fy*sn + (float)x;
                            float ry2 = fx*sn + fy*cs + (float)y;
                            surface_blend_pixel(surf, (int)rx2, (int)ry2, 0, 0, 0, a);
                        }
                    }
                }
            }
            cx += 8 * scale;
        }
    }

    cx = x; cy = y;
    for (i = 0; i < (int)len; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (ch == '\n') { cy += 9 * scale; cx = x; continue; }
        if (ch > 127) ch = '?';
        for (row = 0; row < 8; row++) {
            unsigned char bits = (unsigned char)font8x8_basic[ch][row];
            for (col = 0; col < 8; col++) {
                if (!(bits & (1u << col))) continue;
                for (dy2 = 0; dy2 < scale; dy2++) {
                    for (dx2 = 0; dx2 < scale; dx2++) {
                        float fx = (float)(cx + col*scale + dx2) - (float)x;
                        float fy = (float)(cy + row*scale + dy2) - (float)y;
                        float rx2 = fx*cs - fy*sn + (float)x;
                        float ry2 = fx*sn + fy*cs + (float)y;
                        surface_blend_pixel(surf, (int)rx2, (int)ry2, r, g, b, a);
                    }
                }
            }
        }
        cx += 8 * scale;
    }
}

/* ===== End software surface renderer ===== */

static int png_blit_region_scaled(wiitools_png_image *img, int sx, int sy, int sw, int sh,
                                  int dx, int dy, int dw, int dh)
{
    if (img == NULL || img->rgba == NULL)
        return -2;

    if (g_render_target != NULL) {
        surface_sw_blit_img(img, sx, sy, sw, sh, g_render_target, dx, dy, dw, dh);
        g_render_target_dirty = 1;
        return 0;
    }

    if (screenMode != NULL && current_frame != NULL && img->tex_rgba8 != NULL) {
        Mtx model;
        Mtx44 proj;
        GXTexObj texobj;
        float u0, v0, u1, v1;

        if (g_draw_target == NULL) {
            mqmsg_t next = NULL;
            MQ_Receive(frame_empty, &next, MQ_MSG_BLOCK);
            g_draw_target = next;
        }

        guMtxIdentity(model);
        GX_LoadPosMtxImm(model, GX_PNMTX0);
        { float _lw, _lh; get_logical_size(&_lw, &_lh); guOrtho(proj, 0.0f, _lh, 0.0f, _lw, -1.0f, 1.0f); }
        GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);

        GX_SetNumChans(1);
        GX_SetNumTexGens(1);
        GX_SetNumTevStages(1);
        GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
        GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
        GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
        GX_SetColorUpdate(GX_TRUE);

        GX_ClearVtxDesc();
        GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
        GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
        GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
        GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
        GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
        GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

        GX_InitTexObj(&texobj, img->tex_rgba8, (u16)img->tex_w, (u16)img->tex_h,
                      GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
        GX_InitTexObjLOD(&texobj, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
        GX_LoadTexObj(&texobj, GX_TEXMAP0);

        u0 = (float)sx / (float)img->tex_w;
        v0 = (float)sy / (float)img->tex_h;
        u1 = (float)(sx + sw) / (float)img->tex_w;
        v1 = (float)(sy + sh) / (float)img->tex_h;

        GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
        GX_Position3f32((f32)dx, (f32)dy, 0.0f);
        GX_Color4u8(255, 255, 255, 255);
        GX_TexCoord2f32(u0, v0);
        GX_Position3f32((f32)(dx + dw), (f32)dy, 0.0f);
        GX_Color4u8(255, 255, 255, 255);
        GX_TexCoord2f32(u1, v0);
        GX_Position3f32((f32)(dx + dw), (f32)(dy + dh), 0.0f);
        GX_Color4u8(255, 255, 255, 255);
        GX_TexCoord2f32(u1, v1);
        GX_Position3f32((f32)dx, (f32)(dy + dh), 0.0f);
        GX_Color4u8(255, 255, 255, 255);
        GX_TexCoord2f32(u0, v1);
        GX_End();

        /* Present is handled once in update() to avoid per-draw copy stalls. */
        g_frame_dirty = 1;
        return 0;
    }

    unsigned fbw, fbh;
    unsigned fb_pairs_per_row;
    u32 *fb;
    int ty;
    unsigned sx_step_fp;
    unsigned sy_step_fp;
    unsigned sy_fp = 0;

    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
        return 0;
    if (png_get_render_target(&fb, &fbw, &fbh) != 0)
        return -1;

    png_init_ycbcr_lut();
    fb_pairs_per_row = fbw / 2u;
    sx_step_fp = ((unsigned)sw << 16) / (unsigned)dw;
    sy_step_fp = ((unsigned)sh << 16) / (unsigned)dh;

    for (ty = 0; ty < dh; ty++) {
        int dyi = dy + ty;
        int syi;
        int tx;
        unsigned sx_fp = 0;
        if (dyi < 0 || (unsigned)dyi >= fbh)
            goto next_row;

        syi = sy + (int)(sy_fp >> 16);
        if (syi < 0 || (unsigned)syi >= img->h)
            goto next_row;

        for (tx = 0; tx < dw; tx++) {
            int dxi = dx + tx;
            int sxi;
            unsigned src;
            unsigned r, g, b, a;

            if (dxi < 0 || (unsigned)dxi >= fbw)
                goto next_px;

            sxi = sx + (int)(sx_fp >> 16);
            if (sxi < 0 || (unsigned)sxi >= img->w)
                goto next_px;

            src = ((unsigned)syi * img->w + (unsigned)sxi) * 4u;
            r = img->rgba[src + 0];
            g = img->rgba[src + 1];
            b = img->rgba[src + 2];
            a = img->rgba[src + 3];
            if (a == 0)
                goto next_px;

            png_put_pixel_xfb(fb, fb_pairs_per_row, dxi, dyi, r, g, b);
next_px:
            sx_fp += sx_step_fp;
        }
next_row:
        sy_fp += sy_step_fp;
    }

    g_frame_dirty = 1;
    return 0;
}

static int png_blit_quad(wiitools_png_image *img,
                         float x1, float y1, float x2, float y2,
                         float x3, float y3, float x4, float y4,
                         int sx, int sy, int sw, int sh)
{
    Mtx model;
    Mtx44 proj;
    GXTexObj texobj;
    float u0, v0, u1, v1;

    if (img == NULL || img->rgba == NULL)
        return -2;
    if (screenMode == NULL || current_frame == NULL || img->tex_rgba8 == NULL)
        return -1;
    if (sw <= 0 || sh <= 0)
        return 0;

    if (g_draw_target == NULL) {
        mqmsg_t next = NULL;
        MQ_Receive(frame_empty, &next, MQ_MSG_BLOCK);
        g_draw_target = next;
    }

    guMtxIdentity(model);
    GX_LoadPosMtxImm(model, GX_PNMTX0);
    { float _lw, _lh; get_logical_size(&_lw, &_lh); guOrtho(proj, 0.0f, _lh, 0.0f, _lw, -1.0f, 1.0f); }
    GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);

    GX_SetNumChans(1);
    GX_SetNumTexGens(1);
    GX_SetNumTevStages(1);
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
    GX_SetColorUpdate(GX_TRUE);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    GX_InitTexObj(&texobj, img->tex_rgba8, (u16)img->tex_w, (u16)img->tex_h,
                  GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&texobj, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GX_LoadTexObj(&texobj, GX_TEXMAP0);

    u0 = (float)sx / (float)img->tex_w;
    v0 = (float)sy / (float)img->tex_h;
    u1 = (float)(sx + sw) / (float)img->tex_w;
    v1 = (float)(sy + sh) / (float)img->tex_h;

    GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
    GX_Position3f32(x1, y1, 0.0f);
    GX_Color4u8(255, 255, 255, 255);
    GX_TexCoord2f32(u0, v0);
    GX_Position3f32(x2, y2, 0.0f);
    GX_Color4u8(255, 255, 255, 255);
    GX_TexCoord2f32(u1, v0);
    GX_Position3f32(x3, y3, 0.0f);
    GX_Color4u8(255, 255, 255, 255);
    GX_TexCoord2f32(u1, v1);
    GX_Position3f32(x4, y4, 0.0f);
    GX_Color4u8(255, 255, 255, 255);
    GX_TexCoord2f32(u0, v1);
    GX_End();

    g_frame_dirty = 1;
    return 0;
}

static void copy_buffers(u32 cnt) {
	mqmsg_t input_frame;

	if(MQ_Receive(frame_draw, &input_frame, MQ_MSG_NOBLOCK)) {
		VIDEO_SetNextFramebuffer(input_frame);
		VIDEO_Flush();

		if(current_frame)
			MQ_Send(frame_empty, current_frame, MQ_MSG_BLOCK);

		current_frame = input_frame;
	}
}

static PyObject* rendering_init(PyObject *self, PyObject *args) {
	(void)self;
    if (!PyArg_ParseTuple(args, ":rendering_init"))
        return NULL;
	
	VIDEO_Init();
	video_init_done = 1;
	screenMode = VIDEO_GetPreferredMode(NULL);
	frameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(screenMode));
	frameBuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(screenMode));
	frameBuffer[2] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(screenMode));

	MQ_Init(&frame_draw, 3);
	MQ_Init(&frame_empty, 3);

	/* frameBuffer[0] is shown first; only enqueue true backbuffers. */
	MQ_Send(frame_empty, frameBuffer[1], MQ_MSG_BLOCK);
	MQ_Send(frame_empty, frameBuffer[2], MQ_MSG_BLOCK);
	frame = frameBuffer[1];

	if(CONF_GetAspectRatio() == CONF_ASPECT_16_9) {
		screenMode->viWidth = 678;
		gfx_screen_width = 802;
	} else {
		screenMode->viWidth = 672;
		gfx_screen_width = 640;
	}

	if(VIDEO_GetCurrentTvMode() == VI_PAL
	   || VIDEO_GetCurrentTvMode() == VI_MPAL) {
		screenMode->viXOrigin = (VI_MAX_WIDTH_PAL - screenMode->viWidth) / 2;
	} else {
		screenMode->viXOrigin = (VI_MAX_WIDTH_NTSC - screenMode->viWidth) / 2;
	}

	s8 hoffset = 0;
	CONF_GetDisplayOffsetH(&hoffset);
	screenMode->viXOrigin += hoffset;

	VIDEO_Configure(screenMode);
	VIDEO_SetNextFramebuffer(frameBuffer[0]);
	current_frame = frameBuffer[0];
	g_draw_target = NULL;
	g_frame_dirty = 0;
	VIDEO_SetPreRetraceCallback(copy_buffers);
	VIDEO_SetBlack(false);
	VIDEO_Flush();

	fifoBuffer = MEM_K0_TO_K1(memalign(32, FIFO_SIZE));
	memset(fifoBuffer, 0, FIFO_SIZE);

	GX_Init(fifoBuffer, FIFO_SIZE);
	GX_SetCopyClear((GXColor) {255, 255, 255, 255}, GX_MAX_Z24);
	GX_SetViewport(0, 0, screenMode->fbWidth, screenMode->efbHeight, 0, 1);
	GX_SetDispCopyYScale(
		GX_GetYScaleFactor(screenMode->efbHeight, screenMode->xfbHeight));
	GX_SetScissor(0, 0, screenMode->fbWidth, screenMode->efbHeight);
	GX_SetDispCopySrc(0, 0, screenMode->fbWidth, screenMode->efbHeight);
	GX_SetDispCopyDst(screenMode->fbWidth, screenMode->xfbHeight);
	GX_SetCopyFilter(GX_FALSE, NULL, GX_FALSE, NULL);
	GX_SetFieldMode(screenMode->field_rendering,
					((screenMode->viHeight == 2 * screenMode->xfbHeight) ?
						 GX_ENABLE :
						 GX_DISABLE));

	GX_SetCullMode(GX_CULL_BACK);
	GX_CopyDisp(frameBuffer[0], GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	GX_InvalidateTexAll();
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	// blocks
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 8);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGB, GX_RGB8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_U8, 8);

	// entities, particles
	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	// gui, font drawing
	GX_SetVtxAttrFmt(GX_VTXFMT2, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT2, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT2, GX_VA_TEX0, GX_TEX_ST, GX_U16, 8);

	// blocks etc with direct color
	GX_SetVtxAttrFmt(GX_VTXFMT3, GX_VA_POS, GX_POS_XYZ, GX_S16, 8);
	GX_SetVtxAttrFmt(GX_VTXFMT3, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT3, GX_VA_TEX0, GX_TEX_ST, GX_U8, 8);

	GX_SetArray(GX_VA_CLR0, colors, 3 * sizeof(uint8_t));
	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);
	GX_SetTevOp(GX_TEVSTAGE0, true ? GX_MODULATE : GX_PASSCLR);
	GX_SetAlphaCompare(GX_GEQUAL, 16, GX_AOP_AND, GX_ALWAYS, 0);
	GX_SetZCompLoc(GX_FALSE);

	GX_SetTexCoordGen(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_POS, GX_TEXMTX1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

	GX_SetLineWidth(12, GX_TO_ZERO);

	GX_DrawDone();
	return py_none();
}

/* --------------- Overlay-Bridge: Host-Rendering uebernehmen --------------- */
/*
 * Wenn dieses Modul als Overlay unter einem Host (z.B. ffCavEX) laeuft, der
 * VIDEO, Framebuffer und GX bereits initialisiert hat, darf man NICHT
 * rendering_init() aufrufen (das setzt alles neu auf und kollidiert mit dem
 * Host). Stattdessen uebernimmt diese Funktion nur die Software-Globals
 * dieses Moduls auf die Werte des Hosts. Danach besteht der Check in
 * wiitools_begin_shape_draw() (screenMode/current_frame != NULL, Zeile 2118)
 * und es wird in den Back-Buffer des Hosts gezeichnet -- ohne eigenen
 * VSync/Flip.
 *
 * Aus C aufrufbar (nicht static), z.B. aus ext_main(api):
 *     extern void wiitools_adopt_host_gfx(GXRModeObj*, void*, mqbox_t);
 *     wiitools_adopt_host_gfx(host_mode, host_backbuffer, 0);
 *
 * mode        : GXRModeObj* des Hosts (Host-Global: screenMode)
 * back_buffer : aktueller Back-Buffer des Hosts, in den gezeichnet wird
 *               (Host-Global: frame). MUSS != NULL sein.
 * empty_q     : frame_empty-Queue des Hosts, oder 0 wenn der Host den Flip
 *               selbst macht (empfohlen -> kein MQ_Receive, kein Konflikt).
 */
void wiitools_adopt_host_gfx(GXRModeObj *mode, void *back_buffer, mqbox_t empty_q)
{
    screenMode    = mode;
    current_frame = (mqmsg_t)back_buffer;  /* != NULL -> Check Z.2118 besteht */
    frame_empty   = empty_q;
    /* In den Back-Buffer des Hosts zeichnen und (im Flush) direkt anzeigen. */
    g_draw_target = (mqmsg_t)back_buffer;
    g_frame_dirty = 0;
    g_adopt_mode  = 1;

    /* KEIN GX_Init und KEIN eigener FIFO! libpython ist in CavEX gelinkt -> es
       gibt genau EIN GX und EINEN GP-FIFO (den von CavEX). Ein GX_Init hier
       wuerde den geteilten GP auf einen fremden FIFO umschalten; das
       Hin-/Herschalten macht den GP-Finish-Mechanismus kaputt und CavEX'
       naechstes GX_WaitDrawDone haengt. Wir benutzen CavEX' bereits
       initialisiertes GX mit und setzen nur den GX-STATE. Kein Video/FB/VI. */
    GX_SetCopyClear((GXColor) {0, 0, 0, 255}, GX_MAX_Z24);
    GX_SetViewport(0, 0, screenMode->fbWidth, screenMode->efbHeight, 0, 1);
    GX_SetDispCopyYScale(
        GX_GetYScaleFactor(screenMode->efbHeight, screenMode->xfbHeight));
    GX_SetScissor(0, 0, screenMode->fbWidth, screenMode->efbHeight);
    GX_SetDispCopySrc(0, 0, screenMode->fbWidth, screenMode->efbHeight);
    GX_SetDispCopyDst(screenMode->fbWidth, screenMode->xfbHeight);
    GX_SetCopyFilter(GX_FALSE, NULL, GX_FALSE, NULL);
    GX_SetFieldMode(screenMode->field_rendering,
                    ((screenMode->viHeight == 2 * screenMode->xfbHeight) ?
                         GX_ENABLE : GX_DISABLE));

    GX_SetCullMode(GX_CULL_BACK);
    GX_SetDispCopyGamma(GX_GM_1_0);

    GX_InvalidateTexAll();
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 8);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGB, GX_RGB8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_U8, 8);

    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    GX_SetVtxAttrFmt(GX_VTXFMT2, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT2, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT2, GX_VA_TEX0, GX_TEX_ST, GX_U16, 8);

    GX_SetVtxAttrFmt(GX_VTXFMT3, GX_VA_POS, GX_POS_XYZ, GX_S16, 8);
    GX_SetVtxAttrFmt(GX_VTXFMT3, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT3, GX_VA_TEX0, GX_TEX_ST, GX_U8, 8);

    GX_SetArray(GX_VA_CLR0, colors, 3 * sizeof(uint8_t));
    GX_SetNumChans(1);
    GX_SetNumTexGens(1);
    GX_SetNumTevStages(1);
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetAlphaCompare(GX_GEQUAL, 16, GX_AOP_AND, GX_ALWAYS, 0);
    GX_SetZCompLoc(GX_FALSE);

    GX_SetTexCoordGen(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_POS, GX_TEXMTX1);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

    GX_SetLineWidth(12, GX_TO_ZERO);
    GX_DrawDone();
}

/* Gleiche Funktion aus Python: rendering_adopt(mode_ptr, fb_ptr[, empty_q])
 * mit den Zeigern als Integer (Pointer-Werte, die der Host uebergibt). */
static PyObject* rendering_adopt(PyObject *self, PyObject *args)
{
    unsigned long mode_ptr = 0, fb_ptr = 0, empty_ptr = 0;
    (void)self;
    if (!PyArg_ParseTuple(args, "kk|k:rendering_adopt",
                          &mode_ptr, &fb_ptr, &empty_ptr))
        return NULL;
    /* rohe Zeiger von Python: zuerst Bereich prüfen */
    if (!wt_addr_range_ok((u32)mode_ptr, sizeof(GXRModeObj)) ||
        !wt_addr_range_ok((u32)fb_ptr, 4)) {
        PyErr_SetString(PyExc_ValueError,
            "rendering_adopt: mode_ptr/fb_ptr outside valid RAM");
        return NULL;
    }
    wiitools_adopt_host_gfx((GXRModeObj*)mode_ptr, (void*)fb_ptr,
                            (mqbox_t)empty_ptr);
    return py_none();
}

/* --------------- PNG helpers --------------- */
static wiitools_png_image *png_require_current(void)
{
    if (g_png_current == NULL || g_png_current->rgba == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "no PNG selected/loaded");
        return NULL;
    }
    return g_png_current;
}

static PyObject* png_use(PyObject *self, PyObject *args)
{
    const char *name;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "s:png_use", &name))
        return NULL;
    img = png_find_image(name);
    if (img == NULL || img->rgba == NULL) {
        PyErr_Format(PyExc_KeyError, "PNG name not found: '%s'", name);
        return NULL;
    }
    g_png_current = img;
    return py_none();
}

static PyObject* png_load_named(PyObject *self, PyObject *args)
{
    const char *path;
    const char *name;
    unsigned char *encoded = NULL;
    unsigned char *decoded = NULL;
    size_t encoded_size = 0;
    unsigned w = 0, h = 0;
    unsigned err;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "ss:png_load_named", &path, &name))
        return NULL;

    err = lodepng_load_file(&encoded, &encoded_size, path);
    if (err != 0) {
        PyErr_Format(PyExc_IOError,
                     "png_load_named('%s','%s'): file read failed (err=%u: %s, errno=%d)",
                     path, name, err, lodepng_error_text(err), errno);
        return NULL;
    }
    err = lodepng_decode32(&decoded, &w, &h, encoded, encoded_size);
    free(encoded);
    if (err != 0) {
        PyErr_Format(PyExc_IOError,
                     "png_load_named('%s','%s'): decode failed (err=%u: %s, bytes=%lu)",
                     path, name, err, lodepng_error_text(err), (unsigned long)encoded_size);
        return NULL;
    }
    img = png_get_or_create_image(name);
    if (img == NULL) {
        free(decoded);
        PyErr_NoMemory();
        return NULL;
    }
    png_unload_image_data(img);
    img->rgba = decoded;
    img->w = w;
    img->h = h;
    png_build_gx_texture(img);
    g_png_current = img;
    return Py_BuildValue("(ii)", (int)img->w, (int)img->h);
}

static PyObject* png_load(PyObject *self, PyObject *args)
{
    const char *path;
    PyObject *named_args;
    PyObject *ret;
    (void)self;
    if (!PyArg_ParseTuple(args, "s:png_load", &path))
        return NULL;
    named_args = Py_BuildValue("(ss)", path, "__default__");
    if (named_args == NULL)
        return NULL;
    ret = png_load_named(self, named_args);
    Py_DECREF(named_args);
    return ret;
}

static PyObject* png_load_embedded_named(PyObject *self, PyObject *args)
{
    const char *name;
    const unsigned char *start = test_png;
    const unsigned char *end = test_png_end;
    size_t encoded_size;
    unsigned char *decoded = NULL;
    unsigned w = 0, h = 0;
    unsigned err;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "s:png_load_embedded_named", &name))
        return NULL;
    if (start == NULL || end == NULL || end <= start) {
        PyErr_SetString(PyExc_RuntimeError, "embedded test_png not linked");
        return NULL;
    }
    encoded_size = (size_t)(end - start);
    err = lodepng_decode32(&decoded, &w, &h, start, encoded_size);
    if (err != 0) {
        PyErr_Format(PyExc_IOError,
                     "png_load_embedded_named('%s'): decode failed (err=%u: %s, bytes=%lu)",
                     name, err, lodepng_error_text(err), (unsigned long)encoded_size);
        return NULL;
    }
    img = png_get_or_create_image(name);
    if (img == NULL) {
        free(decoded);
        PyErr_NoMemory();
        return NULL;
    }
    png_unload_image_data(img);
    img->rgba = decoded;
    img->w = w;
    img->h = h;
    png_build_gx_texture(img);
    g_png_current = img;
    return Py_BuildValue("(ii)", (int)img->w, (int)img->h);
}

static PyObject* png_load_embedded(PyObject *self, PyObject *args)
{
    PyObject *named_args;
    PyObject *ret;
    (void)self;
    if (!PyArg_ParseTuple(args, ":png_load_embedded"))
        return NULL;
    named_args = Py_BuildValue("(s)", "__default__");
    if (named_args == NULL)
        return NULL;
    ret = png_load_embedded_named(self, named_args);
    Py_DECREF(named_args);
    return ret;
}

static PyObject* png_info_named(PyObject *self, PyObject *args)
{
    const char *name;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "s:png_info_named", &name))
        return NULL;
    img = png_find_image(name);
    if (img == NULL || img->rgba == NULL)
        return py_none();
    return Py_BuildValue("(ii)", (int)img->w, (int)img->h);
}

static PyObject* png_info(PyObject *self, PyObject *args)
{
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, ":png_info"))
        return NULL;
    img = g_png_current;
    if (img == NULL || img->rgba == NULL)
        return py_none();
    return Py_BuildValue("(ii)", (int)img->w, (int)img->h);
}

static PyObject* png_unload_named(PyObject *self, PyObject *args)
{
    const char *name;
    (void)self;
    if (!PyArg_ParseTuple(args, "s:png_unload_named", &name))
        return NULL;
    png_remove_image(name);
    return py_none();
}

static PyObject* png_unload_all(PyObject *self, PyObject *args)
{
    (void)self;
    if (!PyArg_ParseTuple(args, ":png_unload_all"))
        return NULL;
    png_clear_all_images();
    return py_none();
}

static PyObject* png_unload(PyObject *self, PyObject *args)
{
    wiitools_png_image *img;
    char *name_copy;
    (void)self;
    if (!PyArg_ParseTuple(args, ":png_unload"))
        return NULL;
    img = g_png_current;
    if (img == NULL)
        return py_none();
    name_copy = wiitools_strdup(img->name);
    if (name_copy == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    png_remove_image(name_copy);
    free(name_copy);
    return py_none();
}

/* Encodes rgba to PNG and writes it to path.
   Returns 0 on success. On error sets a Python exception and returns -1. */
static int wiitools_png_write_file(const char *path, const unsigned char *rgba, unsigned w, unsigned h)
{
    unsigned char *buf = NULL;
    size_t buf_size = 0;
    unsigned err;
    FILE *f;
    size_t nwritten;

    err = lodepng_encode32(&buf, &buf_size, rgba, w, h);
    if (err != 0) {
        PyErr_Format(PyExc_IOError, "png encode failed: %s", lodepng_error_text(err));
        return -1;
    }

    errno = 0;
    f = fopen(path, "wb");
    if (!f) {
        PyErr_Format(PyExc_OSError, "fopen('%s','wb') failed: errno=%d (%s)",
                     path, errno, strerror(errno));
        free(buf);
        return -1;
    }
    setvbuf(f, NULL, _IONBF, 0);
    errno = 0;
    nwritten = fwrite(buf, 1, buf_size, f);
    if (nwritten != buf_size) {
        PyErr_Format(PyExc_OSError, "fwrite('%s') wrote %u/%u bytes: errno=%d (%s)",
                     path, (unsigned)nwritten, (unsigned)buf_size, errno, strerror(errno));
        fclose(f);
        free(buf);
        return -1;
    }
    if (fclose(f) != 0) {
        PyErr_Format(PyExc_OSError, "fclose('%s') failed: errno=%d (%s)",
                     path, errno, strerror(errno));
        free(buf);
        return -1;
    }
    free(buf);
    return 0;
}

static PyObject* png_save_named(PyObject *self, PyObject *args)
{
    const char *name;
    const char *path;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "ss:png_save_named", &name, &path))
        return NULL;
    img = png_find_image(name);
    if (img == NULL || img->rgba == NULL) {
        PyErr_Format(PyExc_KeyError, "PNG name not found: '%s'", name);
        return NULL;
    }
    if (wiitools_png_write_file(path, img->rgba, img->w, img->h) != 0) {
        return NULL; /* exception already set */
    }
    return PyLong_FromLong(0);
}

static PyObject* png_save(PyObject *self, PyObject *args)
{
    const char *path;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "s:png_save", &path))
        return NULL;
    img = png_require_current();
    if (img == NULL)
        return NULL;
    if (wiitools_png_write_file(path, img->rgba, img->w, img->h) != 0) {
        return NULL; /* exception already set */
    }
    return PyLong_FromLong(0);
}

static PyObject* png_show_named(PyObject *self, PyObject *args)
{
    const char *name;
    int x, y;
    int rc;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "sii:png_show_named", &name, &x, &y))
        return NULL;
    img = png_find_image(name);
    if (img == NULL || img->rgba == NULL) {
        PyErr_Format(PyExc_KeyError, "PNG name not found: '%s'", name);
        return NULL;
    }
    rc = png_blit_region_scaled(img, 0, 0, (int)img->w, (int)img->h, x, y, (int)img->w, (int)img->h);
    if (rc == -1) {
        PyErr_SetString(PyExc_RuntimeError, "framebuffer not initialized");
        return NULL;
    }
    return py_none();
}

static PyObject* png_show(PyObject *self, PyObject *args)
{
    int x, y;
    int rc;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "ii:png_show", &x, &y))
        return NULL;
    img = png_require_current();
    if (img == NULL)
        return NULL;
    rc = png_blit_region_scaled(img, 0, 0, (int)img->w, (int)img->h, x, y, (int)img->w, (int)img->h);
    if (rc == -1) {
        PyErr_SetString(PyExc_RuntimeError, "framebuffer not initialized");
        return NULL;
    }
    return py_none();
}

static PyObject* png_show_region_named(PyObject *self, PyObject *args)
{
    const char *name;
    int sx, sy, sw, sh, dx, dy;
    int rc;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "siiiiii:png_show_region_named", &name, &sx, &sy, &sw, &sh, &dx, &dy))
        return NULL;
    img = png_find_image(name);
    if (img == NULL || img->rgba == NULL) {
        PyErr_Format(PyExc_KeyError, "PNG name not found: '%s'", name);
        return NULL;
    }
    rc = png_blit_region_scaled(img, sx, sy, sw, sh, dx, dy, sw, sh);
    if (rc == -1) {
        PyErr_SetString(PyExc_RuntimeError, "framebuffer not initialized");
        return NULL;
    }
    return py_none();
}

static PyObject* png_show_region(PyObject *self, PyObject *args)
{
    int sx, sy, sw, sh, dx, dy;
    int rc;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "iiiiii:png_show_region", &sx, &sy, &sw, &sh, &dx, &dy))
        return NULL;
    img = png_require_current();
    if (img == NULL)
        return NULL;
    rc = png_blit_region_scaled(img, sx, sy, sw, sh, dx, dy, sw, sh);
    if (rc == -1) {
        PyErr_SetString(PyExc_RuntimeError, "framebuffer not initialized");
        return NULL;
    }
    return py_none();
}

static PyObject* png_show_region_scaled_named(PyObject *self, PyObject *args)
{
    const char *name;
    int sx, sy, sw, sh, dx, dy, dw, dh;
    int rc;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "siiiiiiii:png_show_region_scaled_named",
                          &name, &sx, &sy, &sw, &sh, &dx, &dy, &dw, &dh))
        return NULL;
    img = png_find_image(name);
    if (img == NULL || img->rgba == NULL) {
        PyErr_Format(PyExc_KeyError, "PNG name not found: '%s'", name);
        return NULL;
    }
    rc = png_blit_region_scaled(img, sx, sy, sw, sh, dx, dy, dw, dh);
    if (rc == -1) {
        PyErr_SetString(PyExc_RuntimeError, "framebuffer not initialized");
        return NULL;
    }
    return py_none();
}

static PyObject* png_show_region_scaled(PyObject *self, PyObject *args)
{
    int sx, sy, sw, sh, dx, dy, dw, dh;
    int rc;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "iiiiiiii:png_show_region_scaled",
                          &sx, &sy, &sw, &sh, &dx, &dy, &dw, &dh))
        return NULL;
    img = png_require_current();
    if (img == NULL)
        return NULL;
    rc = png_blit_region_scaled(img, sx, sy, sw, sh, dx, dy, dw, dh);
    if (rc == -1) {
        PyErr_SetString(PyExc_RuntimeError, "framebuffer not initialized");
        return NULL;
    }
    return py_none();
}

static PyObject* png_show_scaled_named(PyObject *self, PyObject *args)
{
    const char *name;
    int dx, dy, dw, dh;
    int rc;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "siiii:png_show_scaled_named", &name, &dx, &dy, &dw, &dh))
        return NULL;
    img = png_find_image(name);
    if (img == NULL || img->rgba == NULL) {
        PyErr_Format(PyExc_KeyError, "PNG name not found: '%s'", name);
        return NULL;
    }
    rc = png_blit_region_scaled(img, 0, 0, (int)img->w, (int)img->h, dx, dy, dw, dh);
    if (rc == -1) {
        PyErr_SetString(PyExc_RuntimeError, "framebuffer not initialized");
        return NULL;
    }
    return py_none();
}

static PyObject* png_show_scaled(PyObject *self, PyObject *args)
{
    int dx, dy, dw, dh;
    int rc;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "iiii:png_show_scaled", &dx, &dy, &dw, &dh))
        return NULL;
    img = png_require_current();
    if (img == NULL)
        return NULL;
    rc = png_blit_region_scaled(img, 0, 0, (int)img->w, (int)img->h, dx, dy, dw, dh);
    if (rc == -1) {
        PyErr_SetString(PyExc_RuntimeError, "framebuffer not initialized");
        return NULL;
    }
    return py_none();
}

static PyObject* png_show_fullscreen_named(PyObject *self, PyObject *args)
{
    const char *name;
    u32 *fb;
    unsigned fbw, fbh;
    int rc;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "s:png_show_fullscreen_named", &name))
        return NULL;
    img = png_find_image(name);
    if (img == NULL || img->rgba == NULL) {
        PyErr_Format(PyExc_KeyError, "PNG name not found: '%s'", name);
        return NULL;
    }
    if (png_get_render_target(&fb, &fbw, &fbh) != 0) {
        PyErr_SetString(PyExc_RuntimeError, "framebuffer not initialized");
        return NULL;
    }
    rc = png_blit_region_scaled(img, 0, 0, (int)img->w, (int)img->h, 0, 0, (int)fbw, (int)fbh);
    if (rc == -1) {
        PyErr_SetString(PyExc_RuntimeError, "framebuffer not initialized");
        return NULL;
    }
    return py_none();
}

static PyObject* png_show_fullscreen(PyObject *self, PyObject *args)
{
    u32 *fb;
    unsigned fbw, fbh;
    int rc;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, ":png_show_fullscreen"))
        return NULL;
    img = png_require_current();
    if (img == NULL)
        return NULL;
    if (png_get_render_target(&fb, &fbw, &fbh) != 0) {
        PyErr_SetString(PyExc_RuntimeError, "framebuffer not initialized");
        return NULL;
    }
    rc = png_blit_region_scaled(img, 0, 0, (int)img->w, (int)img->h, 0, 0, (int)fbw, (int)fbh);
    if (rc == -1) {
        PyErr_SetString(PyExc_RuntimeError, "framebuffer not initialized");
        return NULL;
    }
    return py_none();
}

/* png(screen_x, screen_y, image_x, image_y, image_w, image_h, screen_w, screen_h) */
static PyObject* png_draw(PyObject *self, PyObject *args)
{
    int dx, dy, sx, sy, sw, sh, dw, dh;
    int rc;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "iiiiiiii:png", &dx, &dy, &sx, &sy, &sw, &sh, &dw, &dh))
        return NULL;
    img = png_require_current();
    if (img == NULL)
        return NULL;
    rc = png_blit_region_scaled(img, sx, sy, sw, sh, dx, dy, dw, dh);
    if (rc == -1) {
        PyErr_SetString(PyExc_RuntimeError, "framebuffer not initialized");
        return NULL;
    }
    return py_none();
}

static PyObject* png_quad(PyObject *self, PyObject *args)
{
    float x1, y1, x2, y2, x3, y3, x4, y4;
    int sx, sy, sw, sh;
    int rc;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "ffffffffiiii:png_quad",
                          &x1, &y1, &x2, &y2, &x3, &y3, &x4, &y4,
                          &sx, &sy, &sw, &sh))
        return NULL;
    img = png_require_current();
    if (img == NULL)
        return NULL;
    rc = png_blit_quad(img, x1, y1, x2, y2, x3, y3, x4, y4, sx, sy, sw, sh);
    if (rc == -1) {
        PyErr_SetString(PyExc_RuntimeError, "png_quad requires rendering_init() and GX texture path");
        return NULL;
    }
    return py_none();
}

static PyObject* png_draw_named(PyObject *self, PyObject *args)
{
    const char *name;
    int dx, dy, sx, sy, sw, sh, dw, dh;
    int rc;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "siiiiiiii:png_named", &name, &dx, &dy, &sx, &sy, &sw, &sh, &dw, &dh))
        return NULL;
    img = png_find_image(name);
    if (img == NULL || img->rgba == NULL) {
        PyErr_Format(PyExc_KeyError, "PNG name not found: '%s'", name);
        return NULL;
    }
    rc = png_blit_region_scaled(img, sx, sy, sw, sh, dx, dy, dw, dh);
    if (rc == -1) {
        PyErr_SetString(PyExc_RuntimeError, "framebuffer not initialized");
        return NULL;
    }
    return py_none();
}

/* --------------- Shape drawing --------------- */
static int wiitools_begin_shape_draw(void)
{
    Mtx model;
    Mtx44 proj;

    if (screenMode == NULL || current_frame == NULL)
        return -1;

    if (g_draw_target == NULL) {
        mqmsg_t next = NULL;
        MQ_Receive(frame_empty, &next, MQ_MSG_BLOCK);
        g_draw_target = next;
    }

    guMtxIdentity(model);
    GX_LoadPosMtxImm(model, GX_PNMTX0);
    { float _lw, _lh; get_logical_size(&_lw, &_lh); guOrtho(proj, 0.0f, _lh, 0.0f, _lw, -1.0f, 1.0f); }
    GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);

    GX_SetNumChans(1);
    GX_SetNumTexGens(0);
    GX_SetNumTevStages(1);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
    GX_SetColorUpdate(GX_TRUE);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    return 0;
}

/*
 * gutil_prepare()
 *
 * Setzt den GX-Zustand exakt so, wie CavEX' gutil_*-Funktionen (gfx_draw_quads,
 * GX_VTXFMT2) ihn erwarten, und MUSS vor jedem gutil_text/gutil_texquad-Aufruf
 * (via c_run) laufen. Hintergrund: draw_rect/render_text nutzen
 * wiitools_begin_shape_draw(), das GX_VA_TEX0 auf GX_NONE laesst und
 * NumTexGens=0 setzt. gfx_draw_quads schreibt aber Position(S16)+Farbe(RGBA8)+
 * Texcoord(U16) fuer VTXFMT2. Ohne den passenden Vertex-Deskriptor liest der
 * Grafikprozessor die falsche Byte-Zahl pro Vertex -> FIFO-Desync -> der
 * naechste GX_DrawDone (in update()) haengt fuer immer.
 *
 * Setzt daher: vollen Deskriptor (POS/CLR0/TEX0 = DIRECT), VTXFMT2-Format wie
 * CavEX (S16 / RGBA8 / U16 mit frac 8), TEV MODULATE + TexCoordGen fuer die in
 * gutil gebundene Textur, Ortho-Projektion mit CavEX' z-Bereich (-256..256, da
 * gutil-Schatten z=-2 nutzen), Blending an, Tiefentest aus.
 */
static PyObject* gutil_prepare(PyObject *self, PyObject *args)
{
    Mtx model;
    Mtx44 proj;
    float lw, lh;
    (void)self;

    if (!PyArg_ParseTuple(args, ":gutil_prepare"))
        return NULL;

    if (screenMode == NULL || current_frame == NULL)
        return py_none();

    if (g_draw_target == NULL && frame_empty != 0) {
        mqmsg_t next = NULL;
        MQ_Receive(frame_empty, &next, MQ_MSG_BLOCK);
        g_draw_target = next;
    }

    guMtxIdentity(model);
    GX_LoadPosMtxImm(model, GX_PNMTX0);
    get_logical_size(&lw, &lh);
    guOrtho(proj, 0.0f, lh, 0.0f, lw, -256.0f, 256.0f);
    GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);

    GX_SetNumChans(1);
    GX_SetNumTexGens(1);
    GX_SetNumTevStages(1);
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
    GX_SetColorUpdate(GX_TRUE);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT2, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT2, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT2, GX_VA_TEX0, GX_TEX_ST, GX_U16, 8);

    g_frame_dirty = 1;
    return py_none();
}

static u8 wiitools_u8(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (u8)v;
}

static int wiitools_parse_rgba_tuple(PyObject *obj, u8 *r, u8 *g, u8 *b, u8 *a)
{
    long rv, gv, bv, av;
    if (!PyTuple_Check(obj) || PyTuple_Size(obj) != 4) {
        PyErr_SetString(PyExc_TypeError, "color must be a tuple (r, g, b, a)");
        return -1;
    }
    rv = PyLong_AsLong(PyTuple_GetItem(obj, 0));
    gv = PyLong_AsLong(PyTuple_GetItem(obj, 1));
    bv = PyLong_AsLong(PyTuple_GetItem(obj, 2));
    av = PyLong_AsLong(PyTuple_GetItem(obj, 3));
    if (PyErr_Occurred())
        return -1;
    *r = wiitools_u8((int)rv);
    *g = wiitools_u8((int)gv);
    *b = wiitools_u8((int)bv);
    *a = wiitools_u8((int)av);
    return 0;
}

static void wiitools_emit_quad(float x, float y, float w, float h, u8 r, u8 g, u8 b, u8 a)
{
    GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
    GX_Position3f32(x, y, 0.0f);
    GX_Color4u8(r, g, b, a);
    GX_Position3f32(x + w, y, 0.0f);
    GX_Color4u8(r, g, b, a);
    GX_Position3f32(x + w, y + h, 0.0f);
    GX_Color4u8(r, g, b, a);
    GX_Position3f32(x, y + h, 0.0f);
    GX_Color4u8(r, g, b, a);
    GX_End();
}

static void wiitools_emit_quad_rot(float x, float y, float w, float h,
                                   float ox, float oy, float cs, float sn,
                                   u8 r, u8 g, u8 b, u8 a)
{
    float vx[4], vy[4];
    int i;
    vx[0] = x;     vy[0] = y;
    vx[1] = x + w; vy[1] = y;
    vx[2] = x + w; vy[2] = y + h;
    vx[3] = x;     vy[3] = y + h;

    GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
    for (i = 0; i < 4; i++) {
        float lx = vx[i] - ox;
        float ly = vy[i] - oy;
        float rx = lx * cs - ly * sn;
        float ry = lx * sn + ly * cs;
        GX_Position3f32(ox + rx, oy + ry, 0.0f);
        GX_Color4u8(r, g, b, a);
    }
    GX_End();
}

static int wiitools_text_length_px(const char *text, int scale)
{
    int cur = 0;
    int maxw = 0;
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        if (*p == '\n') {
            if (cur > maxw) maxw = cur;
            cur = 0;
        } else {
            cur += 8 * scale;
        }
        p++;
    }
    if (cur > maxw) maxw = cur;
    return maxw;
}

static PyObject* draw_rect(PyObject *self, PyObject *args)
{
    int x, y, w, h;
    PyObject *color;
    float angle_deg;
    u8 r, g, b, a;
    float cx, cy, hw, hh;
    float rad, cs, sn;
    float lx[4], ly[4];
    int i;
    (void)self;

    if (!PyArg_ParseTuple(args, "iiiiOf:draw_rect", &x, &y, &w, &h, &color, &angle_deg))
        return NULL;
    if (wiitools_parse_rgba_tuple(color, &r, &g, &b, &a) != 0)
        return NULL;
    if (w <= 0 || h <= 0)
        return py_none();
    if (g_render_target != NULL) {
        surface_sw_draw_rect(g_render_target, x, y, w, h, r, g, b, a, angle_deg);
        g_render_target_dirty = 1;
        return py_none();
    }
    if (wiitools_begin_shape_draw() != 0) {
        PyErr_SetString(PyExc_RuntimeError, "rendering not initialized");
        return NULL;
    }

    cx = (float)x + (float)w * 0.5f;
    cy = (float)y + (float)h * 0.5f;
    hw = (float)w * 0.5f;
    hh = (float)h * 0.5f;
    rad = angle_deg * (3.14159265358979323846f / 180.0f);
    cs = cosf(rad);
    sn = sinf(rad);

    lx[0] = -hw; ly[0] = -hh;
    lx[1] =  hw; ly[1] = -hh;
    lx[2] =  hw; ly[2] =  hh;
    lx[3] = -hw; ly[3] =  hh;

    GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
    for (i = 0; i < 4; i++) {
        float rx = lx[i] * cs - ly[i] * sn;
        float ry = lx[i] * sn + ly[i] * cs;
        GX_Position3f32(cx + rx, cy + ry, 0.0f);
        GX_Color4u8(r, g, b, a);
    }
    GX_End();

    g_frame_dirty = 1;
    return py_none();
}

static PyObject* draw_circle(PyObject *self, PyObject *args)
{
    int x, y, radius;
    PyObject *color;
    u8 r, g, b, a;
    int segments = 48;
    int i;
    (void)self;

    if (!PyArg_ParseTuple(args, "iiiO:draw_circle", &x, &y, &radius, &color))
        return NULL;
    if (wiitools_parse_rgba_tuple(color, &r, &g, &b, &a) != 0)
        return NULL;
    if (radius <= 0)
        return py_none();
    if (g_render_target != NULL) {
        surface_sw_draw_circle(g_render_target, x, y, radius, r, g, b, a);
        g_render_target_dirty = 1;
        return py_none();
    }
    if (wiitools_begin_shape_draw() != 0) {
        PyErr_SetString(PyExc_RuntimeError, "rendering not initialized");
        return NULL;
    }

    GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT1, (u16)(segments + 2));
    GX_Position3f32((f32)x, (f32)y, 0.0f);
    GX_Color4u8(r, g, b, a);
    for (i = 0; i <= segments; i++) {
        float t = (float)i * (2.0f * 3.14159265358979323846f / (float)segments);
        GX_Position3f32((f32)x + cosf(t) * (float)radius, (f32)y + sinf(t) * (float)radius, 0.0f);
        GX_Color4u8(r, g, b, a);
    }
    GX_End();

    g_frame_dirty = 1;
    return py_none();
}

static PyObject* draw_oval(PyObject *self, PyObject *args)
{
    int x, y, w, h;
    PyObject *color;
    u8 r, g, b, a;
    float cx, cy, rx, ry, angle_deg;
    float rad, cs, sn;
    int segments = 64;
    int i;
    (void)self;

    if (!PyArg_ParseTuple(args, "iiiiOf:draw_oval", &x, &y, &w, &h, &color, &angle_deg))
        return NULL;
    if (wiitools_parse_rgba_tuple(color, &r, &g, &b, &a) != 0)
        return NULL;
    if (w <= 0 || h <= 0)
        return py_none();
    if (g_render_target != NULL) {
        surface_sw_draw_oval(g_render_target, x, y, w, h, r, g, b, a, angle_deg);
        g_render_target_dirty = 1;
        return py_none();
    }
    if (wiitools_begin_shape_draw() != 0) {
        PyErr_SetString(PyExc_RuntimeError, "rendering not initialized");
        return NULL;
    }

    cx = (float)x + (float)w * 0.5f;
    cy = (float)y + (float)h * 0.5f;
    rx = (float)w * 0.5f;
    ry = (float)h * 0.5f;
    rad = angle_deg * (3.14159265358979323846f / 180.0f);
    cs = cosf(rad);
    sn = sinf(rad);

    GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT1, (u16)(segments + 2));
    GX_Position3f32(cx, cy, 0.0f);
    GX_Color4u8(r, g, b, a);
    for (i = 0; i <= segments; i++) {
        float t = (float)i * (2.0f * 3.14159265358979323846f / (float)segments);
        float ex = cosf(t) * rx;
        float ey = sinf(t) * ry;
        float px = ex * cs - ey * sn;
        float py = ex * sn + ey * cs;
        GX_Position3f32(cx + px, cy + py, 0.0f);
        GX_Color4u8(r, g, b, a);
    }
    GX_End();

    g_frame_dirty = 1;
    return py_none();
}

static PyObject* render_text_py(PyObject *self, PyObject *args)
{
    int x, y;
    char *text;
    int size;
    int shadow;
    float angle_deg;
    float rad, cs, sn;
    PyObject *color;
    u8 r, g, b, a;
    int cx, cy;
    int i, row, col;
    int scale, shadow_off;
    size_t len;
    (void)self;

    if (!PyArg_ParseTuple(args, "iisiiOf:render_text", &x, &y, &text, &size, &shadow, &color, &angle_deg))
        return NULL;
    if (wiitools_parse_rgba_tuple(color, &r, &g, &b, &a) != 0)
        return NULL;
    if (g_render_target != NULL) {
        surface_sw_render_text(g_render_target, x, y, text, size, shadow, r, g, b, a, angle_deg);
        g_render_target_dirty = 1;
        return py_none();
    }
    if (wiitools_begin_shape_draw() != 0) {
        PyErr_SetString(PyExc_RuntimeError, "rendering not initialized");
        return NULL;
    }

    scale = (size <= 0) ? 1 : size;
    rad = angle_deg * (3.14159265358979323846f / 180.0f);
    cs = cosf(rad);
    sn = sinf(rad);
    shadow_off = scale / 4;
    if (shadow_off < 1) shadow_off = 1;
    len = strlen(text);
    cx = x;
    cy = y;

    if (shadow) {
        for (i = 0; i < (int)len; i++) {
            unsigned char ch = (unsigned char)text[i];
            if (ch == '\n') {
                cy += 9 * scale;
                cx = x;
                continue;
            }
            if (ch > 127) ch = '?';
            for (row = 0; row < 8; row++) {
                unsigned char bits = (unsigned char)font8x8_basic[ch][row];
                for (col = 0; col < 8; col++) {
                    if (bits & (1u << col)) {
                        wiitools_emit_quad_rot((float)(cx + col * scale + shadow_off),
                                               (float)(cy + row * scale + shadow_off),
                                               (float)scale, (float)scale,
                                               (float)x, (float)y, cs, sn,
                                               0, 0, 0, a);
                    }
                }
            }
            cx += 8 * scale;
        }
        cx = x;
        cy = y;
    }

    for (i = 0; i < (int)len; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (ch == '\n') {
            cy += 9 * scale;
            cx = x;
            continue;
        }
        if (ch > 127) ch = '?';
        for (row = 0; row < 8; row++) {
            unsigned char bits = (unsigned char)font8x8_basic[ch][row];
            for (col = 0; col < 8; col++) {
                if (bits & (1u << col)) {
                    wiitools_emit_quad_rot((float)(cx + col * scale), (float)(cy + row * scale),
                                           (float)scale, (float)scale,
                                           (float)x, (float)y, cs, sn, r, g, b, a);
                }
            }
        }
        cx += 8 * scale;
    }

    g_frame_dirty = 1;
    return py_none();
}

static PyObject* text_length_py(PyObject *self, PyObject *args)
{
    char *text;
    int size;
    int scale;
    (void)self;
    if (!PyArg_ParseTuple(args, "si:text_length", &text, &size))
        return NULL;
    scale = (size <= 0) ? 1 : size;
    return PyLong_FromLong((long)wiitools_text_length_px(text, scale));
}


/* --------------- WPADState named-tuple type --------------- */

static PyStructSequence_Field WPADState_fields[] = {
    {"WPAD_BUTTON_2",              "Wiimote button 2"},
    {"WPAD_BUTTON_1",              "Wiimote button 1"},
    {"WPAD_BUTTON_B",              "Wiimote B (trigger)"},
    {"WPAD_BUTTON_A",              "Wiimote A"},
    {"WPAD_BUTTON_MINUS",          "Wiimote minus"},
    {"WPAD_BUTTON_HOME",           "Wiimote home"},
    {"WPAD_BUTTON_LEFT",           "Wiimote d-pad left"},
    {"WPAD_BUTTON_RIGHT",          "Wiimote d-pad right"},
    {"WPAD_BUTTON_DOWN",           "Wiimote d-pad down"},
    {"WPAD_BUTTON_UP",             "Wiimote d-pad up"},
    {"WPAD_BUTTON_PLUS",           "Wiimote plus"},
    {"WPAD_NUNCHUK_BUTTON_Z",      "Nunchuk Z"},
    {"WPAD_NUNCHUK_BUTTON_C",      "Nunchuk C"},
    {"WPAD_CLASSIC_BUTTON_UP",     "Classic d-pad up"},
    {"WPAD_CLASSIC_BUTTON_LEFT",   "Classic d-pad left"},
    {"WPAD_CLASSIC_BUTTON_ZR",     "Classic ZR"},
    {"WPAD_CLASSIC_BUTTON_X",      "Classic X"},
    {"WPAD_CLASSIC_BUTTON_A",      "Classic A"},
    {"WPAD_CLASSIC_BUTTON_Y",      "Classic Y"},
    {"WPAD_CLASSIC_BUTTON_B",      "Classic B"},
    {"WPAD_CLASSIC_BUTTON_ZL",     "Classic ZL"},
    {"WPAD_CLASSIC_BUTTON_FULL_R", "Classic full R trigger"},
    {"WPAD_CLASSIC_BUTTON_PLUS",   "Classic plus"},
    {"WPAD_CLASSIC_BUTTON_HOME",   "Classic home"},
    {"WPAD_CLASSIC_BUTTON_MINUS",  "Classic minus"},
    {"WPAD_CLASSIC_BUTTON_FULL_L", "Classic full L trigger"},
    {"WPAD_CLASSIC_BUTTON_DOWN",   "Classic d-pad down"},
    {"WPAD_CLASSIC_BUTTON_RIGHT",  "Classic d-pad right"},
    {"buttons_raw",                "Raw button bitmask (int)"},
    {NULL, NULL}
};

static PyStructSequence_Desc WPADState_desc = {
    "wiitools.WPADState",
    "Snapshot of all Wiimote/Classic button states. Fields are True/False.",
    WPADState_fields,
    29
};

static PyTypeObject *WPADState_Type = NULL;

static PyObject* wpad_state_from_bits(u32 bits)
{
    PyObject *obj;
    int i = 0;
    if (WPADState_Type == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "WPADState not initialized");
        return NULL;
    }
    obj = PyStructSequence_New(WPADState_Type);
    if (obj == NULL) return NULL;
#define SETB(mask) PyStructSequence_SET_ITEM(obj, i++, PyBool_FromLong((bits & (u32)(mask)) != 0u))
    SETB(WPAD_BUTTON_2);   SETB(WPAD_BUTTON_1);
    SETB(WPAD_BUTTON_B);   SETB(WPAD_BUTTON_A);
    SETB(WPAD_BUTTON_MINUS); SETB(WPAD_BUTTON_HOME);
    SETB(WPAD_BUTTON_LEFT);  SETB(WPAD_BUTTON_RIGHT);
    SETB(WPAD_BUTTON_DOWN);  SETB(WPAD_BUTTON_UP);
    SETB(WPAD_BUTTON_PLUS);
    SETB(WPAD_NUNCHUK_BUTTON_Z); SETB(WPAD_NUNCHUK_BUTTON_C);
    SETB(WPAD_CLASSIC_BUTTON_UP);    SETB(WPAD_CLASSIC_BUTTON_LEFT);
    SETB(WPAD_CLASSIC_BUTTON_ZR);    SETB(WPAD_CLASSIC_BUTTON_X);
    SETB(WPAD_CLASSIC_BUTTON_A);     SETB(WPAD_CLASSIC_BUTTON_Y);
    SETB(WPAD_CLASSIC_BUTTON_B);     SETB(WPAD_CLASSIC_BUTTON_ZL);
    SETB(WPAD_CLASSIC_BUTTON_FULL_R); SETB(WPAD_CLASSIC_BUTTON_PLUS);
    SETB(WPAD_CLASSIC_BUTTON_HOME);  SETB(WPAD_CLASSIC_BUTTON_MINUS);
    SETB(WPAD_CLASSIC_BUTTON_FULL_L); SETB(WPAD_CLASSIC_BUTTON_DOWN);
    SETB(WPAD_CLASSIC_BUTTON_RIGHT);
#undef SETB
    PyStructSequence_SET_ITEM(obj, i++, PyLong_FromUnsignedLong((unsigned long)bits));
    return obj;
}

static u32 wpad_collect_bits(int chan, u32 (*fn)(s32))
{
    if (chan == WPAD_CHAN_ALL) {
        u32 r = 0; int k;
        for (k = 0; k < 4; k++) r |= fn((s32)k);
        return r;
    }
    return fn((s32)chan);
}

static PyObject* wpad_buttons_down_all(PyObject *self, PyObject *args)
{
    int chan;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_ButtonsDown_all", &chan)) return NULL;
    return wpad_state_from_bits(wpad_collect_bits(chan, WPAD_ButtonsDown));
}

static PyObject* wpad_buttons_up_all(PyObject *self, PyObject *args)
{
    int chan;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_ButtonsUp_all", &chan)) return NULL;
    return wpad_state_from_bits(wpad_collect_bits(chan, WPAD_ButtonsUp));
}

static PyObject* wpad_buttons_held_all(PyObject *self, PyObject *args)
{
    int chan;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_ButtonsHeld_all", &chan)) return NULL;
    return wpad_state_from_bits(wpad_collect_bits(chan, WPAD_ButtonsHeld));
}

/* --------------- WPAD (simplified) --------------- */
static PyObject* wpad_up(PyObject *self, PyObject *args)
{
    long button;
    int chan;
    int r = 0;
    (void)self;

    if (!PyArg_ParseTuple(args, "li:WPAD_ButtonsUp", &button, &chan))
        return NULL;

    if (chan == WPAD_CHAN_ALL) {
        int i;
        for (i = 0; i < 4; i++) {
            u32 p = WPAD_ButtonsUp(i);
            if (p & (u32)button) {
                r = 1;
                break;
            }
        }
    } else {
        u32 p = WPAD_ButtonsUp(chan);
        if (p & (u32)button)
            r = 1;
    }

    return PyLong_FromLong((long)r);
}

static PyObject* wpad_init(PyObject *self, PyObject *args)
{
    (void)self;
    if (!PyArg_ParseTuple(args, ":WPAD_Init"))
        return NULL;
    s32 r = WPAD_Init();
    g_wpad_inited = 1;
    WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_ALL, gfx_width, gfx_height);
    return PyLong_FromLong((long)r);
}

/* PAD (GameCube controller) wrappers */
static PyObject* pad_init(PyObject *self, PyObject *args)
{
    s32 r;
    (void)self;
    if (!PyArg_ParseTuple(args, ":PAD_Init"))
        return NULL;
    r = PAD_Init();
    g_pad_inited = 1;
    return PyLong_FromLong((long)r);
}

static PyObject* pad_sync(PyObject *self, PyObject *args)
{
    (void)self;
    if (!PyArg_ParseTuple(args, ":PAD_Sync"))
        return NULL;
    return PyLong_FromLong((long)PAD_Sync());
}

static PyObject* pad_scanpads(PyObject *self, PyObject *args)
{
    (void)self;
    if (!PyArg_ParseTuple(args, ":PAD_ScanPads"))
        return NULL;
    return PyLong_FromLong((long)PAD_ScanPads());
}

static PyObject* pad_read(PyObject *self, PyObject *args)
{
    u32 r;
    (void)self;
    if (!PyArg_ParseTuple(args, ":PAD_Read"))
        return NULL;
    r = PAD_Read(g_pad_status);
    return Py_BuildValue("iy#", (int)r, (char *)g_pad_status, (int)sizeof(g_pad_status));
}

static PyObject* pad_reset(PyObject *self, PyObject *args)
{
    unsigned long mask;
    (void)self;
    if (!PyArg_ParseTuple(args, "k:PAD_Reset", &mask))
        return NULL;
    return PyLong_FromLong((long)PAD_Reset((u32)mask));
}

static PyObject* pad_recalibrate(PyObject *self, PyObject *args)
{
    unsigned long mask;
    (void)self;
    if (!PyArg_ParseTuple(args, "k:PAD_Recalibrate", &mask))
        return NULL;
    return PyLong_FromLong((long)PAD_Recalibrate((u32)mask));
}

static PyObject* pad_clamp(PyObject *self, PyObject *args)
{
    (void)self;
    if (!PyArg_ParseTuple(args, ":PAD_Clamp"))
        return NULL;
    PAD_Clamp(g_pad_status);
    return PyBytes_FromStringAndSize((char *)g_pad_status, (Py_ssize_t)sizeof(g_pad_status));
}

static PyObject* pad_control_motor(PyObject *self, PyObject *args)
{
    int chan;
    unsigned long cmd;
    (void)self;
    if (!PyArg_ParseTuple(args, "ik:PAD_ControlMotor", &chan, &cmd))
        return NULL;
    PAD_ControlMotor((s32)chan, (u32)cmd);
    return py_none();
}

static PyObject* pad_set_spec(PyObject *self, PyObject *args)
{
    unsigned long spec;
    (void)self;
    if (!PyArg_ParseTuple(args, "k:PAD_SetSpec", &spec))
        return NULL;
    PAD_SetSpec((u32)spec);
    return py_none();
}

static PyObject* pad_buttons_up(PyObject *self, PyObject *args)
{
    long button;
    int chan;
    u16 p;
    int i;
    (void)self;
    if (!PyArg_ParseTuple(args, "li:PAD_ButtonsUp", &button, &chan))
        return NULL;
    if (chan < 0) {
        for (i = 0; i < PAD_CHANMAX; ++i) {
            p = PAD_ButtonsUp(i);
            if (p & (u16)button)
                return PyLong_FromLong(1);
        }
        return PyLong_FromLong(0);
    }
    p = PAD_ButtonsUp(chan);
    return PyLong_FromLong((p & (u16)button) ? 1 : 0);
}

static PyObject* pad_buttons_down(PyObject *self, PyObject *args)
{
    long button;
    int chan;
    u16 p;
    int i;
    (void)self;
    if (!PyArg_ParseTuple(args, "li:PAD_ButtonsDown", &button, &chan))
        return NULL;
    if (chan < 0) {
        for (i = 0; i < PAD_CHANMAX; ++i) {
            p = PAD_ButtonsDown(i);
            if (p & (u16)button)
                return PyLong_FromLong(1);
        }
        return PyLong_FromLong(0);
    }
    p = PAD_ButtonsDown(chan);
    return PyLong_FromLong((p & (u16)button) ? 1 : 0);
}

static PyObject* pad_buttons_held(PyObject *self, PyObject *args)
{
    long button;
    int chan;
    u16 p;
    int i;
    (void)self;
    if (!PyArg_ParseTuple(args, "li:PAD_ButtonsHeld", &button, &chan))
        return NULL;
    if (chan < 0) {
        for (i = 0; i < PAD_CHANMAX; ++i) {
            p = PAD_ButtonsHeld(i);
            if (p & (u16)button)
                return PyLong_FromLong(1);
        }
        return PyLong_FromLong(0);
    }
    p = PAD_ButtonsHeld(chan);
    return PyLong_FromLong((p & (u16)button) ? 1 : 0);
}

static PyObject* pad_stick_x(PyObject *self, PyObject *args)
{
    int pad;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:PAD_StickX", &pad))
        return NULL;
    return PyLong_FromLong((long)PAD_StickX(pad));
}

static PyObject* pad_stick_y(PyObject *self, PyObject *args)
{
    int pad;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:PAD_StickY", &pad))
        return NULL;
    return PyLong_FromLong((long)PAD_StickY(pad));
}

static PyObject* pad_substick_x(PyObject *self, PyObject *args)
{
    int pad;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:PAD_SubStickX", &pad))
        return NULL;
    return PyLong_FromLong((long)PAD_SubStickX(pad));
}

static PyObject* pad_substick_y(PyObject *self, PyObject *args)
{
    int pad;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:PAD_SubStickY", &pad))
        return NULL;
    return PyLong_FromLong((long)PAD_SubStickY(pad));
}

static PyObject* pad_trigger_l(PyObject *self, PyObject *args)
{
    int pad;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:PAD_TriggerL", &pad))
        return NULL;
    return PyLong_FromLong((long)PAD_TriggerL(pad));
}

static PyObject* pad_trigger_r(PyObject *self, PyObject *args)
{
    int pad;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:PAD_TriggerR", &pad))
        return NULL;
    return PyLong_FromLong((long)PAD_TriggerR(pad));
}

static PyObject* wpad_down(PyObject *self, PyObject *args)
{
    long button;
    int chan;
    int r = 0;
    (void)self;

    if (!PyArg_ParseTuple(args, "li:WPAD_ButtonsDown", &button, &chan))
        return NULL;

    if (chan == WPAD_CHAN_ALL) {
        int i;
        for (i = 0; i < 4; i++) {
            u32 p = WPAD_ButtonsDown(i);
            if (p & (u32)button) {
                r = 1;
                break;
            }
        }
    } else {
        u32 p = WPAD_ButtonsDown(chan);
        if (p & (u32)button)
            r = 1;
    }

    return PyLong_FromLong((long)r);
}

static PyObject* wpad_held(PyObject *self, PyObject *args)
{
    long button;
    int chan;
    int r = 0;
    (void)self;

    if (!PyArg_ParseTuple(args, "li:WPAD_ButtonsHeld", &button, &chan))
        return NULL;

    if (chan == WPAD_CHAN_ALL) {
        int i;
        for (i = 0; i < 4; i++) {
            u32 p = WPAD_ButtonsHeld(i);
            if (p & (u32)button) {
                r = 1;
                break;
            }
        }
    } else {
        u32 p = WPAD_ButtonsHeld(chan);
        if (p & (u32)button)
            r = 1;
    }

    return PyLong_FromLong((long)r);
}

static PyObject* wpad_control_speaker(PyObject *self, PyObject *args)
{
    int chan, enable;
    s32 r;
    (void)self;
    if (!PyArg_ParseTuple(args, "ii:WPAD_ControlSpeaker", &chan, &enable))
        return NULL;
    r = WPAD_ControlSpeaker((s32)chan, (s32)enable);
    return PyLong_FromLong((long)r);
}

static PyObject* wpad_read_event(PyObject *self, PyObject *args)
{
    int chan;
    s32 r;
    WPADData data;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_ReadEvent", &chan))
        return NULL;
    /* Nur in den libogc-Event-Pfad (__wpad_calc_data) gehen, wenn WPAD
       initialisiert und auf dem Kanal ein Controller verbunden ist. */
    if (wt_wpad_channel_ready(chan) != 0)
        return NULL;
    memset(&data, 0, sizeof(data));
    r = WPAD_ReadEvent((s32)chan, &data);
    return Py_BuildValue("(iy#)", (int)r, (const char *)&data, (int)sizeof(data));
}

static PyObject* wpad_dropped_events(PyObject *self, PyObject *args)
{
    int chan;
    s32 r;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_DroppedEvents", &chan))
        return NULL;
    r = WPAD_DroppedEvents((s32)chan);
    return PyLong_FromLong((long)r);
}

static PyObject* wpad_flush(PyObject *self, PyObject *args)
{
    int chan;
    s32 r;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_Flush", &chan))
        return NULL;
    if (wt_require(g_wpad_inited, "WPAD") != 0)
        return NULL;
    if (chan != WPAD_CHAN_ALL && wt_wpad_channel_ready(chan) != 0)
        return NULL;
    r = WPAD_Flush((s32)chan);
    return PyLong_FromLong((long)r);
}

static PyObject* wpad_read_pending(PyObject *self, PyObject *args)
{
    int chan;
    s32 r;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_ReadPending", &chan))
        return NULL;
    /* Simplified: no Python callback support; pass NULL. */
    if (wt_require(g_wpad_inited, "WPAD") != 0)
        return NULL;
    if (chan != WPAD_CHAN_ALL && wt_wpad_channel_ready(chan) != 0)
        return NULL;
    r = WPAD_ReadPending((s32)chan, NULL);
    return PyLong_FromLong((long)r);
}

static PyObject* wpad_set_data_format(PyObject *self, PyObject *args)
{
    int chan, fmt;
    s32 r;
    (void)self;
    if (!PyArg_ParseTuple(args, "ii:WPAD_SetDataFormat", &chan, &fmt))
        return NULL;
    r = WPAD_SetDataFormat((s32)chan, (s32)fmt);
    return PyLong_FromLong((long)r);
}

static PyObject* wpad_set_motion_plus(PyObject *self, PyObject *args)
{
    int chan, enable;
    s32 r;
    (void)self;
    if (!PyArg_ParseTuple(args, "ii:WPAD_SetMotionPlus", &chan, &enable))
        return NULL;
    r = WPAD_SetMotionPlus((s32)chan, (u8)enable);
    return PyLong_FromLong((long)r);
}

static PyObject* wpad_set_vres(PyObject *self, PyObject *args)
{
    int chan;
    long xres, yres;
    s32 r;
    (void)self;
    if (!PyArg_ParseTuple(args, "ill:WPAD_SetVRes", &chan, &xres, &yres))
        return NULL;
    r = WPAD_SetVRes((s32)chan, (u32)xres, (u32)yres);
    return PyLong_FromLong((long)r);
}

static PyObject* wpad_get_status(PyObject *self, PyObject *args)
{
    s32 r;
    (void)self;
    if (!PyArg_ParseTuple(args, ":WPAD_GetStatus"))
        return NULL;
    r = WPAD_GetStatus();
    return PyLong_FromLong((long)r);
}

static PyObject* wpad_probe(PyObject *self, PyObject *args)
{
    int chan;
    s32 r;
    u32 type = 0;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_Probe", &chan))
        return NULL;
    r = WPAD_Probe((s32)chan, &type);
    return Py_BuildValue("(ii)", (int)r, (int)type);
}

static PyObject* wpad_set_event_bufs(PyObject *self, PyObject *args)
{
    int chan, cnt;
    s32 r;
    (void)self;
    if (!PyArg_ParseTuple(args, "ii:WPAD_SetEventBufs", &chan, &cnt))
        return NULL;
    if (chan < 0 || chan >= WPAD_MAX_WIIMOTES) {
        PyErr_SetString(PyExc_ValueError, "chan must be 0..3");
        return NULL;
    }
    if (cnt < 1 || cnt > 8) {
        PyErr_SetString(PyExc_ValueError, "cnt must be 1..8");
        return NULL;
    }
    if (wt_require(g_wpad_inited, "WPAD") != 0)
        return NULL;
    memset(g_event_bufs[chan], 0, sizeof(g_event_bufs[chan]));
    r = WPAD_SetEventBufs((s32)chan, g_event_bufs[chan], (u32)cnt);
    return PyLong_FromLong((long)r);
}

static PyObject* wpad_disconnect(PyObject *self, PyObject *args)
{
    int chan;
    s32 r;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_Disconnect", &chan))
        return NULL;
    r = WPAD_Disconnect((s32)chan);
    return PyLong_FromLong((long)r);
}

static PyObject* wpad_is_speaker_enabled(PyObject *self, PyObject *args)
{
    int chan;
    s32 r;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_IsSpeakerEnabled", &chan))
        return NULL;
    r = WPAD_IsSpeakerEnabled((s32)chan);
    return PyLong_FromLong((long)r);
}

static PyObject* wpad_send_stream_data(PyObject *self, PyObject *args)
{
    int chan;
    const char *buf;
    int len;
    s32 r;
    (void)self;
    if (!PyArg_ParseTuple(args, "iy#:WPAD_SendStreamData", &chan, &buf, &len))
        return NULL;
    r = WPAD_SendStreamData((s32)chan, (void *)buf, (u32)len);
    return PyLong_FromLong((long)r);
}

static PyObject* wpad_shutdown(PyObject *self, PyObject *args)
{
    (void)self;
    if (!PyArg_ParseTuple(args, ":WPAD_Shutdown"))
        return NULL;
    WPAD_Shutdown();
    return py_none();
}

static PyObject* wpad_set_idle_timeout(PyObject *self, PyObject *args)
{
    long seconds;
    (void)self;
    if (!PyArg_ParseTuple(args, "l:WPAD_SetIdleTimeout", &seconds))
        return NULL;
    WPAD_SetIdleTimeout((u32)seconds);
    return py_none();
}

static PyObject* wpad_set_power_button_callback(PyObject *self, PyObject *args)
{
    int enable;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_SetPowerButtonCallback", &enable))
        return NULL;
    g_power_button_chan = -999;
    WPAD_SetPowerButtonCallback(enable ? wiitools_power_button_cb : NULL);
    return py_none();
}

static PyObject* wpad_set_battery_dead_callback(PyObject *self, PyObject *args)
{
    int enable;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_SetBatteryDeadCallback", &enable))
        return NULL;
    g_battery_dead_chan = -999;
    WPAD_SetBatteryDeadCallback(enable ? wiitools_battery_dead_cb : NULL);
    return py_none();
}

static PyObject* wpad_get_power_button_event(PyObject *self, PyObject *args)
{
    int chan;
    (void)self;
    if (!PyArg_ParseTuple(args, ":WPAD_GetPowerButtonEvent"))
        return NULL;
    chan = g_power_button_chan;
    g_power_button_chan = -999;
    return PyLong_FromLong((long)chan);
}

static PyObject* wpad_get_battery_dead_event(PyObject *self, PyObject *args)
{
    int chan;
    (void)self;
    if (!PyArg_ParseTuple(args, ":WPAD_GetBatteryDeadEvent"))
        return NULL;
    chan = g_battery_dead_chan;
    g_battery_dead_chan = -999;
    return PyLong_FromLong((long)chan);
}

static PyObject* wpad_rumble(PyObject *self, PyObject *args)
{
    int chan, status;
    s32 r;
    (void)self;
    if (!PyArg_ParseTuple(args, "ii:WPAD_Rumble", &chan, &status))
        return NULL;
    r = WPAD_Rumble((s32)chan, status);
    return PyLong_FromLong((long)r);
}

static PyObject* wpad_set_idle_thresholds(PyObject *self, PyObject *args)
{
    int chan, btns, ir, accel, js, wb, mp;
    s32 r;
    (void)self;
    if (!PyArg_ParseTuple(args, "iiiiiii:WPAD_SetIdleThresholds", &chan, &btns, &ir, &accel, &js, &wb, &mp))
        return NULL;
    r = WPAD_SetIdleThresholds((s32)chan, (s32)btns, (s32)ir, (s32)accel, (s32)js, (s32)wb, (s32)mp);
    return PyLong_FromLong((long)r);
}

static PyObject* wpad_encode_data(PyObject *self, PyObject *args)
{
    long flag;
    const char *pcm;
    int pcm_len;
    int out_len;
    int num_samples;
    u8 *enc_data;
    WPADEncStatus info;
    PyObject *ret;
    (void)self;

    if (!PyArg_ParseTuple(args, "ly#i:WPAD_EncodeData", &flag, &pcm, &pcm_len, &out_len))
        return NULL;
    if ((pcm_len % 2) != 0) {
        PyErr_SetString(PyExc_ValueError, "pcm byte length must be even (s16 samples)");
        return NULL;
    }
    if (out_len <= 0 || out_len > 8192) {
        PyErr_SetString(PyExc_ValueError, "out_len must be 1..8192");
        return NULL;
    }

    num_samples = pcm_len / 2;
    enc_data = (u8 *)malloc((size_t)out_len);
    if (enc_data == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    memset(&info, 0, sizeof(info));
    memset(enc_data, 0, (size_t)out_len);
    WPAD_EncodeData(&info, (u32)flag, (const s16 *)pcm, (s32)num_samples, enc_data);

    ret = Py_BuildValue("(y#y#)", (const char *)info.data, (int)sizeof(info.data),
                                  (const char *)enc_data, out_len);
    free(enc_data);
    return ret;
}

static PyObject* wpad_data(PyObject *self, PyObject *args)
{
    int chan;
    WPADData *data;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_Data", &chan))
        return NULL;
    if (wt_wpad_channel_ready(chan) != 0)
        return NULL;
    data = WPAD_Data(chan);
    if (data == NULL)
        return py_none();
    return Py_BuildValue("y#", (const char *)data, (int)sizeof(*data));
}

static PyObject* wpad_battery_level(PyObject *self, PyObject *args)
{
    int chan;
    u8 level;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_BatteryLevel", &chan))
        return NULL;
    level = WPAD_BatteryLevel(chan);
    return PyLong_FromLong((long)level);
}

static PyObject* wpad_ir(PyObject *self, PyObject *args)
{
    int chan;
    struct ir_t ir;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_IR", &chan))
        return NULL;
    if (wt_wpad_channel_ready(chan) != 0)
        return NULL;
    memset(&ir, 0, sizeof(ir));
    WPAD_IR(chan, &ir);
    return Py_BuildValue("y#", (const char *)&ir, (int)sizeof(ir));
}

static PyObject* wpad_orientation(PyObject *self, PyObject *args)
{
    int chan;
    struct orient_t orient;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_Orientation", &chan))
        return NULL;
    if (wt_wpad_channel_ready(chan) != 0)
        return NULL;
    memset(&orient, 0, sizeof(orient));
    WPAD_Orientation(chan, &orient);
    return Py_BuildValue("y#", (const char *)&orient, (int)sizeof(orient));
}

static PyObject* wpad_gforce(PyObject *self, PyObject *args)
{
    int chan;
    struct gforce_t gforce;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_GForce", &chan))
        return NULL;
    if (wt_wpad_channel_ready(chan) != 0)
        return NULL;
    memset(&gforce, 0, sizeof(gforce));
    WPAD_GForce(chan, &gforce);
    return Py_BuildValue("y#", (const char *)&gforce, (int)sizeof(gforce));
}

static PyObject* wpad_accel(PyObject *self, PyObject *args)
{
    int chan;
    struct vec3w_t accel;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_Accel", &chan))
        return NULL;
    if (wt_wpad_channel_ready(chan) != 0)
        return NULL;
    memset(&accel, 0, sizeof(accel));
    WPAD_Accel(chan, &accel);
    return Py_BuildValue("y#", (const char *)&accel, (int)sizeof(accel));
}

static PyObject* wpad_expansion(PyObject *self, PyObject *args)
{
    int chan;
    struct expansion_t exp;
    (void)self;
    if (!PyArg_ParseTuple(args, "i:WPAD_Expansion", &chan))
        return NULL;
    if (wt_wpad_channel_ready(chan) != 0)
        return NULL;
    memset(&exp, 0, sizeof(exp));
    WPAD_Expansion(chan, &exp);
    return Py_BuildValue("y#", (const char *)&exp, (int)sizeof(exp));
}

/* --------------- Surface API --------------- */

static PyObject* py_surface_new(PyObject *self, PyObject *args)
{
    const char *name;
    int w, h;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "sii:surface_new", &name, &w, &h))
        return NULL;
    if (w <= 0 || h <= 0) {
        PyErr_SetString(PyExc_ValueError, "width and height must be positive");
        return NULL;
    }
    img = png_get_or_create_image(name);
    if (img == NULL) { PyErr_NoMemory(); return NULL; }
    png_unload_image_data(img);
    img->rgba = (unsigned char *)calloc((size_t)(unsigned)w * (size_t)(unsigned)h * 4u, 1u);
    if (img->rgba == NULL) { PyErr_NoMemory(); return NULL; }
    img->w = (unsigned)w;
    img->h = (unsigned)h;
    return Py_BuildValue("(ii)", w, h);
}

static PyObject* py_surface_set_target(PyObject *self, PyObject *args)
{
    const char *name;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "s:surface_set_target", &name))
        return NULL;
    img = png_find_image(name);
    if (img == NULL || img->rgba == NULL) {
        PyErr_Format(PyExc_KeyError, "surface not found: '%s'", name);
        return NULL;
    }
    g_render_target = img;
    g_render_target_dirty = 0;
    return py_none();
}

static PyObject* py_surface_clear_target(PyObject *self, PyObject *args)
{
    (void)self;
    if (!PyArg_ParseTuple(args, ":surface_clear_target"))
        return NULL;
    g_render_target = NULL;
    g_render_target_dirty = 0;
    return py_none();
}

static PyObject* py_surface_fill(PyObject *self, PyObject *args)
{
    const char *name;
    PyObject *color;
    u8 r, g, b, a;
    wiitools_png_image *img;
    unsigned i, n;
    (void)self;
    if (!PyArg_ParseTuple(args, "sO:surface_fill", &name, &color))
        return NULL;
    if (wiitools_parse_rgba_tuple(color, &r, &g, &b, &a) != 0)
        return NULL;
    img = png_find_image(name);
    if (img == NULL || img->rgba == NULL) {
        PyErr_Format(PyExc_KeyError, "surface not found: '%s'", name);
        return NULL;
    }
    n = img->w * img->h;
    for (i = 0; i < n; i++) {
        img->rgba[i*4u+0] = r; img->rgba[i*4u+1] = g;
        img->rgba[i*4u+2] = b; img->rgba[i*4u+3] = a;
    }
    if (g_render_target == img) g_render_target_dirty = 1;
    return py_none();
}

static PyObject* py_surface_get_size(PyObject *self, PyObject *args)
{
    const char *name;
    wiitools_png_image *img;
    (void)self;
    if (!PyArg_ParseTuple(args, "s:surface_get_size", &name))
        return NULL;
    img = png_find_image(name);
    if (img == NULL || img->rgba == NULL)
        return py_none();
    return Py_BuildValue("(ii)", (int)img->w, (int)img->h);
}

static PyObject* py_blit(PyObject *self, PyObject *args)
{
    const char *name;
    int x, y;
    int rc;
    wiitools_png_image *img;
    wiitools_png_image *saved_target;
    (void)self;
    if (!PyArg_ParseTuple(args, "sii:blit", &name, &x, &y))
        return NULL;
    img = png_find_image(name);
    if (img == NULL || img->rgba == NULL) {
        PyErr_Format(PyExc_KeyError, "surface not found: '%s'", name);
        return NULL;
    }
    /* Always rebuild GX texture so modifications are visible */
    png_build_gx_texture(img);
    if (g_render_target == img) g_render_target_dirty = 0;
    /* blit always goes to the screen, bypass any active render target */
    saved_target = g_render_target;
    g_render_target = NULL;
    rc = png_blit_region_scaled(img, 0, 0, (int)img->w, (int)img->h,
                                x, y, (int)img->w, (int)img->h);
    g_render_target = saved_target;
    if (rc == -1) {
        PyErr_SetString(PyExc_RuntimeError, "framebuffer not initialized");
        return NULL;
    }
    return py_none();
}

static PyObject* py_surface_blit(PyObject *self, PyObject *args)
{
    const char *src_name;
    PyObject *dst_obj;
    int x, y;
    int sx = 0, sy = 0, sw2 = -1, sh2 = -1;
    wiitools_png_image *src, *dst;
    (void)self;
    /* surface_blit(src_name, dst_name_or_None, x, y[, sx, sy, sw, sh]) */
    if (!PyArg_ParseTuple(args, "sOii|iiii:surface_blit",
                          &src_name, &dst_obj, &x, &y, &sx, &sy, &sw2, &sh2))
        return NULL;
    src = png_find_image(src_name);
    if (src == NULL || src->rgba == NULL) {
        PyErr_Format(PyExc_KeyError, "source surface not found: '%s'", src_name);
        return NULL;
    }
    if (dst_obj == Py_None) {
        dst = g_render_target;
        if (dst == NULL) {
            PyErr_SetString(PyExc_RuntimeError,
                "no active render target — call surface_set_target() first");
            return NULL;
        }
    } else {
        const char *dst_name;
        if (!PyUnicode_Check(dst_obj)) {
            PyErr_SetString(PyExc_TypeError, "dst must be a surface name (str) or None");
            return NULL;
        }
        dst_name = PyUnicode_AsUTF8(dst_obj);
        if (dst_name == NULL) return NULL;
        dst = png_find_image(dst_name);
        if (dst == NULL || dst->rgba == NULL) {
            PyErr_Format(PyExc_KeyError, "destination surface not found: '%s'", dst_name);
            return NULL;
        }
    }
    if (sw2 < 0) sw2 = (int)src->w - sx;
    if (sh2 < 0) sh2 = (int)src->h - sy;
    surface_sw_blit_img(src, sx, sy, sw2, sh2, dst, x, y, sw2, sh2);
    if (g_render_target == dst) g_render_target_dirty = 1;
    return py_none();
}

/* --------------- UPDATE --------------- */

static void do_render_flush(void)
{
    if (g_frame_dirty) {
        if (g_adopt_mode && g_draw_target != NULL) {
            /* Uebernommenes Host-Rendering: EFB in den Host-Back-Buffer kopieren
               und diesen direkt anzeigen. Kein MQ/kein Callback -- den gibt es
               im Adopt-Modus nicht. g_draw_target bleibt erhalten (wir zeichnen
               jeden Frame in denselben Host-Puffer). */
            GX_CopyDisp(g_draw_target, GX_TRUE);
            GX_DrawDone();
            GX_WaitDrawDone();
            VIDEO_SetNextFramebuffer(g_draw_target);
            VIDEO_Flush();
        } else if (screenMode != NULL && current_frame != NULL) {
            if (g_draw_target != NULL) {
                GX_CopyDisp(g_draw_target, GX_TRUE);
                GX_DrawDone();
                GX_WaitDrawDone();
                MQ_Send(frame_draw, g_draw_target, MQ_MSG_BLOCK);
            }
            g_draw_target = NULL;
        } else {
            void *fb = get_framebuffer();
            if (fb != NULL) {
                VIDEO_SetNextFramebuffer(fb);
                VIDEO_Flush();
            }
        }
        g_frame_dirty = 0;
    }
    VIDEO_WaitVSync();
}

static PyObject* wpad_scan_pads(PyObject *self, PyObject *args)
{
    (void)self;
    if (!PyArg_ParseTuple(args, ":WPAD_ScanPads"))
        return NULL;
    WPAD_ScanPads();
    return py_none();
}

static PyObject* py_set_screen_size(PyObject *self, PyObject *args)
{
    int lw, lh;
    (void)self;
    if (!PyArg_ParseTuple(args, "ii:set_screen_size", &lw, &lh))
        return NULL;
    if (screenMode == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "graphics not initialized");
        return NULL;
    }
    if (lw <= 0 || lh <= 0) {
        /* reset to physical */
        g_logical_w = 0.0f; g_logical_h = 0.0f;
        GX_SetViewport(0, 0, screenMode->fbWidth, screenMode->efbHeight, 0, 1);
        GX_SetScissor(0, 0, (u32)screenMode->fbWidth, (u32)screenMode->efbHeight);
        return py_none();
    }
    float phys_w = (float)screenMode->fbWidth;
    float phys_h = (float)screenMode->xfbHeight;
    float scale = phys_w / (float)lw < phys_h / (float)lh
                  ? phys_w / (float)lw : phys_h / (float)lh;
    float vp_w = (float)lw * scale;
    float vp_h = (float)lh * scale;
    float vp_x = (phys_w - vp_w) * 0.5f;
    float vp_y = (phys_h - vp_h) * 0.5f;
    g_logical_w = (float)lw;
    g_logical_h = (float)lh;
    g_vp_x = vp_x; g_vp_y = vp_y;
    g_vp_w = vp_w; g_vp_h = vp_h;
    GX_SetViewport(vp_x, vp_y, vp_w, vp_h, 0, 1);
    GX_SetScissor((u32)vp_x, (u32)vp_y, (u32)vp_w, (u32)vp_h);
    return py_none();
}

static PyObject* render_update(PyObject *self, PyObject *args)
{
    (void)self;
    if (!PyArg_ParseTuple(args, ":render_update"))
        return NULL;
    do_render_flush();
    return py_none();
}

static PyObject* update(PyObject *self, PyObject *args)
{
    (void)self;
    if (!PyArg_ParseTuple(args, ":update"))
        return NULL;
    WPAD_ScanPads();
    PAD_ScanPads();
    do_render_flush();
    return py_none();
}

static PyObject* IsNetReady(PyObject *self, PyObject *args)
{
    (void)self;
    if (!PyArg_ParseTuple(args, ":IsNetReady"))
        return NULL;

	return PyLong_FromLong(net_ready);
}

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

/* Return primary local IPv4 address as a string, or empty string on failure.
   Technique: create a UDP socket and connect() to a public IP (no packets
   are actually sent for UDP) then call getsockname() to read the local
   address the kernel chose for that route. This avoids relying on higher
   level Python socket module which may be absent on the Wii build. */
static PyObject* get_local_ip(PyObject *self, PyObject *args)
{
    (void)self;
    if (!PyArg_ParseTuple(args, ":get_local_ip"))
        return NULL;

    int sock = -1;
    char ipbuf[INET_ADDRSTRLEN] = "";

    /* Prefer libogc helper if available: net_gethostip() */
    {
        /* net_gethostip returns a u32 (declared in network.h). Try it first. */
        unsigned long nip = (unsigned long)net_gethostip();
        if (nip != 0u) {
            struct in_addr ina;
            const char *s = NULL;

            /* First try treating value as network-order (s_addr expects network order) */
            ina.s_addr = (in_addr_t)nip;
            s = inet_ntoa(ina);
            if (s != NULL && strcmp(s, "0.0.0.0") != 0) {
                strncpy(ipbuf, s, sizeof(ipbuf) - 1);
                return PyUnicode_FromString(ipbuf);
            }

            /* Try byte-swapped (some implementations return host-order) */
            ina.s_addr = (in_addr_t)htonl((uint32_t)nip);
            s = inet_ntoa(ina);
            if (s != NULL && strcmp(s, "0.0.0.0") != 0) {
                strncpy(ipbuf, s, sizeof(ipbuf) - 1);
                return PyUnicode_FromString(ipbuf);
            }
        }
    }

	
    //sock = socket(AF_INET, SOCK_DGRAM, 0);
    //if (sock < 0) {

        /* can't open socket -> return empty string */
		/*
        return PyUnicode_FromString("");
    }

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
	*/
    //dst.sin_port = htons(53); /* DNS port; no packets actually sent */
    /* Use a well-known public IP (Cloudflare). */
    /*
	if (inet_pton(AF_INET, "1.1.1.1", &dst.sin_addr) != 1) {
        close(sock);
        return PyUnicode_FromString("");
    }*/

    /* connect() on UDP doesn't send packets but sets the default peer */
    /*if (connect(sock, (struct sockaddr *)&dst, sizeof(dst)) >= 0) {
        struct sockaddr_in local;
        socklen_t llen = sizeof(local);
        if (getsockname(sock, (struct sockaddr *)&local, &llen) == 0) {
            const char *s = inet_ntoa(local.sin_addr);
            if (s != NULL)
                strncpy(ipbuf, s, sizeof(ipbuf) - 1);
        }
    } else {*/
        /* fallback: try gethostname + gethostbyname */
        /*char hn[128] = {0};
        if (gethostname(hn, sizeof(hn)) == 0) {
            struct hostent *h = gethostbyname(hn);
            if (h && h->h_addr_list && h->h_addr_list[0]) {
                struct in_addr ina;
                memcpy(&ina, h->h_addr_list[0], sizeof(ina));
                const char *s = inet_ntoa(ina);
                if (s != NULL)
                    strncpy(ipbuf, s, sizeof(ipbuf) - 1);
            }
        }
    }
	*/

    close(sock);
    return PyUnicode_FromString(ipbuf);
}

static PyObject* init(PyObject *self, PyObject *args)
{
    (void)self;
    if (!PyArg_ParseTuple(args, ":init"))
        return NULL;
    VIDEO_Init();

	rmode3 = VIDEO_GetPreferredMode(NULL);
    framebuffer3 = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode3));


	xfb2 = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode3));

    fatInitDefault();

	console_init(xfb2,20,20,rmode3->fbWidth,rmode3->xfbHeight,rmode3->fbWidth*VI_DISPLAY_PIX_SZ);
	
    lwp_t thread;
    LWP_CreateThread(
        &thread,
        net_thread,       
        &net_ready,       
        NULL,             
        0,                
        50                
    );

    WPAD_Init();
    g_wpad_inited = 1;
    WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_ALL, gfx_width, gfx_height);
    PAD_Init();
    g_pad_inited = 1;
    return py_none();
}

static PyObject* init_network(PyObject *self, PyObject *args)
{
    (void)self;
    if (!PyArg_ParseTuple(args, ":net_init"))
        return NULL;
    
    lwp_t thread;
    LWP_CreateThread(
        &thread,
        net_thread,       
        &net_ready,       
        NULL,             
        0,                
        50                
    );

    return py_none();
}
/* Fixed 8-word prototype used by c_run().  On the PowerPC EABI the first eight
 * integer/pointer arguments are passed in GPRs r3..r10, so calling a real
 * function that takes fewer parameters through this prototype is safe: the
 * callee simply ignores the extra registers and there is no stack cleanup to
 * mismatch (register arguments need none, and we never spill past r10). */
typedef u32 (*wii_cfn8)(u32, u32, u32, u32, u32, u32, u32, u32);

/* c_run(func_addr, *args) -> int
 *
 * Call an arbitrary C function located at address `func_addr` with up to eight
 * arguments and return its result (the value in r3) as an int.
 *
 * Each argument may be:
 *   - an int  -> passed verbatim as a 32-bit word (value or address), or
 *   - a bytes / bytearray object -> a pointer to its buffer is passed (the
 *     buffer stays valid for the duration of the call).
 *
 * LIMITATIONS (no libffi): float/double arguments (which the EABI passes in
 * f1..f8), struct-by-value arguments, more than 8 arguments, and 64-bit or
 * floating-point return values are NOT handled correctly.  For those a
 * dedicated wrapper is required. */
static PyObject* c_run(PyObject *self, PyObject *args)
{
    (void)self;
    Py_ssize_t n = PyTuple_GET_SIZE(args);
    if (n < 1) {
        PyErr_SetString(PyExc_TypeError,
            "c_run(func_addr, *args): missing function address");
        return NULL;
    }
    if (n - 1 > 8) {
        PyErr_SetString(PyExc_TypeError,
            "c_run supports at most 8 arguments");
        return NULL;
    }

    unsigned long addr = PyLong_AsUnsignedLong(PyTuple_GET_ITEM(args, 0));
    if (PyErr_Occurred())
        return NULL;
    if (addr == 0) {
        PyErr_SetString(PyExc_ValueError, "c_run: NULL function address");
        return NULL;
    }
    /* Erste Verteidigung: Adresse muss in gültigem Code-RAM (MEM1/MEM2) liegen. */
    if (!wt_addr_range_ok((u32)addr, 4)) {
        PyErr_Format(PyExc_ValueError,
            "c_run: function address 0x%08lx outside valid RAM", addr);
        return NULL;
    }

    u32 a[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    for (Py_ssize_t i = 1; i < n; ++i) {
        PyObject *o = PyTuple_GET_ITEM(args, i);
        if (PyLong_Check(o)) {
            a[i - 1] = (u32)PyLong_AsUnsignedLong(o);
            if (PyErr_Occurred())
                return NULL;
        } else if (PyBytes_Check(o)) {
            a[i - 1] = (u32)(uintptr_t)PyBytes_AS_STRING(o);
        } else if (PyByteArray_Check(o)) {
            a[i - 1] = (u32)(uintptr_t)PyByteArray_AS_STRING(o);
        } else {
            PyErr_Format(PyExc_TypeError,
                "c_run: argument %zd must be int or bytes-like", i);
            return NULL;
        }
    }

    wii_cfn8 fn = (wii_cfn8)(uintptr_t)addr;
    /* Achtung: c_run ruft eine beliebige Adresse auf. Der Adressbereich wurde
       geprüft, aber ob dort gültiger Code steht, kann nicht garantiert werden —
       c_run ist prinzipbedingt "unsafe by design". */
    u32 r = fn(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
    return PyLong_FromUnsignedLong((unsigned long)r);
}

/* ====================================================================== *
 *  Zusätzliche libogc-Wrapper: CONF (read-only), SYS, AUDIO, CON, net    *
 *                                                                        *
 *  Callbacks (PAD/SYS/AUDIO/net) feuern im Interrupt-Kontext; dort darf  *
 *  kein Python laufen. Daher: ein C-Trampolin zählt nur ein Ereignis,    *
 *  Python fragt den Zähler per *_GetEvent() ab (wie WPAD-Power/Battery). *
 * ====================================================================== */

/* --- Makros für einfache Getter/Setter ------------------------------- */
#define WT_GET_L0(cfn) \
static PyObject* wt_##cfn(PyObject *self, PyObject *args) { \
    (void)self; if (!PyArg_ParseTuple(args, ":" #cfn)) return NULL; \
    return PyLong_FromLong((long)cfn()); }

#define WT_GET_UL0(cfn) \
static PyObject* wt_##cfn(PyObject *self, PyObject *args) { \
    (void)self; if (!PyArg_ParseTuple(args, ":" #cfn)) return NULL; \
    return PyLong_FromUnsignedLong((unsigned long)cfn()); }

#define WT_SET_1(cfn, ctype) \
static PyObject* wt_##cfn(PyObject *self, PyObject *args) { \
    (void)self; long v; if (!PyArg_ParseTuple(args, "l:" #cfn, &v)) return NULL; \
    cfn((ctype)v); return py_none(); }

#define WT_VOID0(cfn) \
static PyObject* wt_##cfn(PyObject *self, PyObject *args) { \
    (void)self; if (!PyArg_ParseTuple(args, ":" #cfn)) return NULL; \
    cfn(); return py_none(); }

/* ---------------------------- CONF (nur lesend) ---------------------- */
WT_GET_L0(CONF_Init)
WT_GET_L0(CONF_GetShutdownMode)
WT_GET_L0(CONF_GetIdleLedMode)
WT_GET_L0(CONF_GetProgressiveScan)
WT_GET_L0(CONF_GetEuRGB60)
WT_GET_L0(CONF_GetIRSensitivity)
WT_GET_L0(CONF_GetSensorBarPosition)
WT_GET_L0(CONF_GetPadSpeakerVolume)
WT_GET_L0(CONF_GetPadMotorMode)
WT_GET_L0(CONF_GetSoundMode)
WT_GET_L0(CONF_GetLanguage)
WT_GET_L0(CONF_GetScreenSaverMode)
WT_GET_L0(CONF_GetAspectRatio)
WT_GET_L0(CONF_GetEULA)
WT_GET_L0(CONF_GetWiiConnect24)
WT_GET_L0(CONF_GetRegion)
WT_GET_L0(CONF_GetArea)
WT_GET_L0(CONF_GetVideo)

static PyObject* wt_CONF_GetLength(PyObject *self, PyObject *args) {
    (void)self; const char *name;
    if (!PyArg_ParseTuple(args, "s:CONF_GetLength", &name)) return NULL;
    return PyLong_FromLong((long)CONF_GetLength(name));
}
static PyObject* wt_CONF_GetType(PyObject *self, PyObject *args) {
    (void)self; const char *name;
    if (!PyArg_ParseTuple(args, "s:CONF_GetType", &name)) return NULL;
    return PyLong_FromLong((long)CONF_GetType(name));
}
static PyObject* wt_CONF_Get(PyObject *self, PyObject *args) {
    (void)self; const char *name;
    if (!PyArg_ParseTuple(args, "s:CONF_Get", &name)) return NULL;
    s32 len = CONF_GetLength(name);
    if (len < 0) { PyErr_SetString(PyExc_KeyError, "CONF entry not found"); return NULL; }
    char *buf = (char *)malloc((size_t)len > 0 ? (size_t)len : 1);
    if (!buf) return PyErr_NoMemory();
    s32 r = CONF_Get(name, buf, (u32)len);
    if (r < 0) { free(buf); PyErr_SetString(PyExc_RuntimeError, "CONF_Get failed"); return NULL; }
    PyObject *b = PyBytes_FromStringAndSize(buf, r);
    free(buf);
    return b;
}
static PyObject* wt_CONF_GetCounterBias(PyObject *self, PyObject *args) {
    (void)self; u32 bias = 0;
    if (!PyArg_ParseTuple(args, ":CONF_GetCounterBias")) return NULL;
    if (CONF_GetCounterBias(&bias) < 0) { PyErr_SetString(PyExc_RuntimeError, "CONF_GetCounterBias failed"); return NULL; }
    return PyLong_FromUnsignedLong((unsigned long)bias);
}
static PyObject* wt_CONF_GetDisplayOffsetH(PyObject *self, PyObject *args) {
    (void)self; s8 off = 0;
    if (!PyArg_ParseTuple(args, ":CONF_GetDisplayOffsetH")) return NULL;
    if (CONF_GetDisplayOffsetH(&off) < 0) { PyErr_SetString(PyExc_RuntimeError, "CONF_GetDisplayOffsetH failed"); return NULL; }
    return PyLong_FromLong((long)off);
}
static PyObject* wt_CONF_GetNickName(PyObject *self, PyObject *args) {
    (void)self; u8 raw[64]; int byteorder = 1; /* UTF-16 big-endian */
    if (!PyArg_ParseTuple(args, ":CONF_GetNickName")) return NULL;
    memset(raw, 0, sizeof(raw));
    if (CONF_GetNickName(raw) < 0) { PyErr_SetString(PyExc_RuntimeError, "CONF_GetNickName failed"); return NULL; }
    /* Länge bis zum ersten 0x0000-Codepoint (max. 10 Zeichen) bestimmen */
    Py_ssize_t nbytes = 0;
    while (nbytes < 20 && !(raw[nbytes] == 0 && raw[nbytes + 1] == 0)) nbytes += 2;
    return PyUnicode_DecodeUTF16((const char *)raw, nbytes, "replace", &byteorder);
}
static PyObject* wt_CONF_GetParentalPassword(PyObject *self, PyObject *args) {
    (void)self; s8 buf[64];
    if (!PyArg_ParseTuple(args, ":CONF_GetParentalPassword")) return NULL;
    memset(buf, 0, sizeof(buf));
    if (CONF_GetParentalPassword(buf) < 0) { PyErr_SetString(PyExc_RuntimeError, "CONF_GetParentalPassword failed"); return NULL; }
    return PyUnicode_FromString((const char *)buf);
}
static PyObject* wt_CONF_GetParentalAnswer(PyObject *self, PyObject *args) {
    (void)self; s8 buf[64];
    if (!PyArg_ParseTuple(args, ":CONF_GetParentalAnswer")) return NULL;
    memset(buf, 0, sizeof(buf));
    if (CONF_GetParentalAnswer(buf) < 0) { PyErr_SetString(PyExc_RuntimeError, "CONF_GetParentalAnswer failed"); return NULL; }
    return PyUnicode_FromString((const char *)buf);
}
static PyObject* wt_CONF_GetPadDevices(PyObject *self, PyObject *args) {
    (void)self; conf_pads pads;
    if (!PyArg_ParseTuple(args, ":CONF_GetPadDevices")) return NULL;
    memset(&pads, 0, sizeof(pads));
    if (CONF_GetPadDevices(&pads) < 0) { PyErr_SetString(PyExc_RuntimeError, "CONF_GetPadDevices failed"); return NULL; }
    return PyLong_FromLong((long)pads.num_registered);
}

/* ---------------------------- SYS ------------------------------------ */
WT_GET_L0(SYS_GetHollywoodRevision)
WT_GET_L0(SYS_ResetButtonDown)
WT_GET_L0(SYS_GetCounterBias)
WT_GET_L0(SYS_GetDisplayOffsetH)
WT_GET_L0(SYS_GetEuRGB60)
WT_GET_L0(SYS_GetLanguage)
WT_GET_L0(SYS_GetProgressiveScan)
WT_GET_L0(SYS_GetSoundMode)
WT_GET_L0(SYS_GetVideoMode)
WT_GET_L0(SYS_GetGBSMode)
WT_GET_UL0(SYS_GetFontEncoding)
WT_GET_UL0(SYS_GetArena1Size)
WT_GET_UL0(SYS_GetArena2Size)
WT_SET_1(SYS_SetCounterBias, u32)
WT_SET_1(SYS_SetDisplayOffsetH, s8)
WT_SET_1(SYS_SetEuRGB60, u8)
WT_SET_1(SYS_SetLanguage, u8)
WT_SET_1(SYS_SetProgressiveScan, u8)
WT_SET_1(SYS_SetSoundMode, u8)
WT_SET_1(SYS_SetVideoMode, u8)
WT_SET_1(SYS_SetGBSMode, u16)

static PyObject* wt_SYS_Time(PyObject *self, PyObject *args) {
    (void)self; if (!PyArg_ParseTuple(args, ":SYS_Time")) return NULL;
    return PyLong_FromUnsignedLongLong((unsigned long long)SYS_Time());
}
static PyObject* wt_SYS_GetWirelessID(PyObject *self, PyObject *args) {
    (void)self; long chan;
    if (!PyArg_ParseTuple(args, "l:SYS_GetWirelessID", &chan)) return NULL;
    return PyLong_FromUnsignedLong((unsigned long)SYS_GetWirelessID((u32)chan));
}
static PyObject* wt_SYS_SetWirelessID(PyObject *self, PyObject *args) {
    (void)self; long chan, id;
    if (!PyArg_ParseTuple(args, "ll:SYS_SetWirelessID", &chan, &id)) return NULL;
    SYS_SetWirelessID((u32)chan, (u16)id); return py_none();
}
static PyObject* wt_SYS_STDIO_Report(PyObject *self, PyObject *args) {
    (void)self; int enable;
    if (!PyArg_ParseTuple(args, "p:SYS_STDIO_Report", &enable)) return NULL;
    SYS_STDIO_Report(enable ? true : false); return py_none();
}
static PyObject* wt_SYS_Report(PyObject *self, PyObject *args) {
    (void)self; const char *msg;
    if (!PyArg_ParseTuple(args, "s:SYS_Report", &msg)) return NULL;
    SYS_Report("%s", msg); return py_none();
}
static PyObject* wt_SYS_ResetSystem(PyObject *self, PyObject *args) {
    (void)self; long reset; unsigned long code = 0; int force_menu = 0;
    if (!PyArg_ParseTuple(args, "l|kp:SYS_ResetSystem", &reset, &code, &force_menu)) return NULL;
    SYS_ResetSystem((s32)reset, (u32)code, (s32)force_menu);
    return py_none();
}

/* SYS Power/Reset-Callbacks: nur Zählen im Trampolin, Python pollt */
static volatile u32 g_sys_power_events = 0;
static volatile u32 g_sys_reset_events = 0;
static volatile u32 g_sys_alarm_events = 0;
static void wt_sys_power_cb(void) { g_sys_power_events++; }
static void wt_sys_reset_cb(u32 irq, void *ctx) { (void)irq; (void)ctx; g_sys_reset_events++; }
static void wt_sys_alarm_cb(syswd_t alarm, void *cbarg) { (void)alarm; (void)cbarg; g_sys_alarm_events++; }

static PyObject* wt_SYS_SetPowerCallback(PyObject *self, PyObject *args) {
    (void)self; int enable;
    if (!PyArg_ParseTuple(args, "p:SYS_SetPowerCallback", &enable)) return NULL;
    SYS_SetPowerCallback(enable ? wt_sys_power_cb : NULL);
    return py_none();
}
static PyObject* wt_SYS_GetPowerEvent(PyObject *self, PyObject *args) {
    (void)self; if (!PyArg_ParseTuple(args, ":SYS_GetPowerEvent")) return NULL;
    u32 n = g_sys_power_events; g_sys_power_events = 0;
    return PyLong_FromUnsignedLong((unsigned long)n);
}
static PyObject* wt_SYS_SetResetCallback(PyObject *self, PyObject *args) {
    (void)self; int enable;
    if (!PyArg_ParseTuple(args, "p:SYS_SetResetCallback", &enable)) return NULL;
    SYS_SetResetCallback(enable ? wt_sys_reset_cb : NULL);
    return py_none();
}
static PyObject* wt_SYS_GetResetEvent(PyObject *self, PyObject *args) {
    (void)self; if (!PyArg_ParseTuple(args, ":SYS_GetResetEvent")) return NULL;
    u32 n = g_sys_reset_events; g_sys_reset_events = 0;
    return PyLong_FromUnsignedLong((unsigned long)n);
}

/* SYS Alarme */
static PyObject* wt_SYS_CreateAlarm(PyObject *self, PyObject *args) {
    (void)self; syswd_t h = 0;
    if (!PyArg_ParseTuple(args, ":SYS_CreateAlarm")) return NULL;
    if (SYS_CreateAlarm(&h) < 0) { PyErr_SetString(PyExc_RuntimeError, "SYS_CreateAlarm failed"); return NULL; }
    return PyLong_FromUnsignedLong((unsigned long)h);
}
static PyObject* wt_SYS_SetAlarm(PyObject *self, PyObject *args) {
    (void)self; unsigned long h; double secs;
    if (!PyArg_ParseTuple(args, "kd:SYS_SetAlarm", &h, &secs)) return NULL;
    struct timespec tp;
    tp.tv_sec = (time_t)secs;
    tp.tv_nsec = (long)((secs - (double)tp.tv_sec) * 1e9);
    s32 r = SYS_SetAlarm((syswd_t)h, &tp, wt_sys_alarm_cb, NULL);
    return PyLong_FromLong((long)r);
}
static PyObject* wt_SYS_SetPeriodicAlarm(PyObject *self, PyObject *args) {
    (void)self; unsigned long h; double start_s, period_s;
    if (!PyArg_ParseTuple(args, "kdd:SYS_SetPeriodicAlarm", &h, &start_s, &period_s)) return NULL;
    struct timespec ts, tp;
    ts.tv_sec = (time_t)start_s;  ts.tv_nsec = (long)((start_s  - (double)ts.tv_sec) * 1e9);
    tp.tv_sec = (time_t)period_s; tp.tv_nsec = (long)((period_s - (double)tp.tv_sec) * 1e9);
    s32 r = SYS_SetPeriodicAlarm((syswd_t)h, &ts, &tp, wt_sys_alarm_cb, NULL);
    return PyLong_FromLong((long)r);
}
static PyObject* wt_SYS_RemoveAlarm(PyObject *self, PyObject *args) {
    (void)self; unsigned long h;
    if (!PyArg_ParseTuple(args, "k:SYS_RemoveAlarm", &h)) return NULL;
    return PyLong_FromLong((long)SYS_RemoveAlarm((syswd_t)h));
}
static PyObject* wt_SYS_CancelAlarm(PyObject *self, PyObject *args) {
    (void)self; unsigned long h;
    if (!PyArg_ParseTuple(args, "k:SYS_CancelAlarm", &h)) return NULL;
    return PyLong_FromLong((long)SYS_CancelAlarm((syswd_t)h));
}
static PyObject* wt_SYS_GetAlarmEvent(PyObject *self, PyObject *args) {
    (void)self; if (!PyArg_ParseTuple(args, ":SYS_GetAlarmEvent")) return NULL;
    u32 n = g_sys_alarm_events; g_sys_alarm_events = 0;
    return PyLong_FromUnsignedLong((unsigned long)n);
}

/* ---------------------------- AUDIO ---------------------------------- */
/* Hinweis: Die AUDIO-*Stream*-API (SetStreamVol*, SetStreamSampleRate,
   SetStreamPlayState, RegisterStreamCallback, SetStreamTrigger,
   ResetStreamSampleCnt) sowie net_getsockopt existieren in libogc nur im
   GameCube-Build, nicht für Wii. Daher hier nur die vorhandene DMA/DSP-API. */
WT_VOID0(AUDIO_StartDMA)
WT_VOID0(AUDIO_StopDMA)
WT_SET_1(AUDIO_SetDSPSampleRate, u8)
WT_GET_UL0(AUDIO_GetDMAEnableFlag)
WT_GET_UL0(AUDIO_GetDMABytesLeft)
WT_GET_UL0(AUDIO_GetDMALength)
WT_GET_UL0(AUDIO_GetDMAStartAddr)
WT_GET_UL0(AUDIO_GetDSPSampleRate)

static PyObject* wt_AUDIO_Init(PyObject *self, PyObject *args) {
    (void)self; if (!PyArg_ParseTuple(args, ":AUDIO_Init")) return NULL;
    AUDIO_Init(NULL); g_audio_inited = 1; return py_none();
}
static PyObject* wt_AUDIO_InitDMA(PyObject *self, PyObject *args) {
    (void)self; unsigned long addr, len;
    if (!PyArg_ParseTuple(args, "kk:AUDIO_InitDMA", &addr, &len)) return NULL;
    if (wt_require(g_audio_inited, "AUDIO") != 0)
        return NULL;
    /* DMA-Quelle muss in gültigem RAM liegen (sonst spielt die Hardware Müll
       oder greift ungültig zu). 32-Byte-aligned wird von der AI-DMA erwartet. */
    if (!wt_addr_range_ok((u32)addr, (u32)len)) {
        PyErr_SetString(PyExc_ValueError, "AUDIO_InitDMA: startaddr/len outside valid RAM");
        return NULL;
    }
    if ((addr & 31u) != 0u || (len & 31u) != 0u) {
        PyErr_SetString(PyExc_ValueError, "AUDIO_InitDMA: startaddr and len must be 32-byte aligned");
        return NULL;
    }
    AUDIO_InitDMA((u32)addr, (u32)len); return py_none();
}

/* AUDIO-DMA-Callback: Trampolin zählt nur, Python pollt */
static volatile u32 g_audio_dma_events = 0;
static void wt_audio_dma_cb(void) { g_audio_dma_events++; }

static PyObject* wt_AUDIO_RegisterDMACallback(PyObject *self, PyObject *args) {
    (void)self; int enable;
    if (!PyArg_ParseTuple(args, "p:AUDIO_RegisterDMACallback", &enable)) return NULL;
    AUDIO_RegisterDMACallback(enable ? wt_audio_dma_cb : NULL); return py_none();
}
static PyObject* wt_AUDIO_GetDMAEvent(PyObject *self, PyObject *args) {
    (void)self; if (!PyArg_ParseTuple(args, ":AUDIO_GetDMAEvent")) return NULL;
    u32 n = g_audio_dma_events; g_audio_dma_events = 0;
    return PyLong_FromUnsignedLong((unsigned long)n);
}

/* Höhere Ebene: einen Sinuston per AI-DMA ausgeben (48 kHz, 16-bit Stereo).
   Blockierend: erzeugt den Puffer, startet DMA, wartet die Dauer ab, stoppt
   und gibt den Puffer wieder frei. */
static PyObject* wt_audio_play_tone(PyObject *self, PyObject *args)
{
    (void)self;
    double freq = 440.0;
    int    ms   = 500;
    long   vol  = 8000;
    if (!PyArg_ParseTuple(args, "|dil:audio_play_tone", &freq, &ms, &vol))
        return NULL;
    if (ms  < 1)     ms  = 1;
    if (ms  > 3000)  ms  = 3000;    /* Puffer-/Blockier-Obergrenze */
    if (vol < 0)     vol = 0;
    if (vol > 32767) vol = 32767;
    if (freq < 1.0)  freq = 1.0;

    const int rate = 48000;
    long frames = (long)(((long long)rate * ms) / 1000);
    long nbytes = frames * 4;               /* 2 Kanäle * 2 Byte */
    nbytes &= ~31L;                          /* auf 32 Byte abrunden (AI-DMA) */
    if (nbytes < 32)      nbytes = 32;
    if (nbytes > 1048544) nbytes = 1048544;  /* AI-DMA-Längenlimit (~1 MB) */
    frames = nbytes / 4;

    s16 *buf = (s16*)memalign(32, (size_t)nbytes);
    if (buf == NULL)
        return PyErr_NoMemory();

    double w = 2.0 * M_PI * freq / (double)rate;
    for (long i = 0; i < frames; i++) {
        s16 v = (s16)((double)vol * sin(w * (double)i));
        buf[2*i]     = v;   /* links  */
        buf[2*i + 1] = v;   /* rechts */
    }
    DCFlushRange(buf, (u32)nbytes);

    if (!g_audio_inited) { AUDIO_Init(NULL); g_audio_inited = 1; }
    AUDIO_StopDMA();
    AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
    AUDIO_InitDMA((u32)buf, (u32)nbytes);
    AUDIO_StartDMA();

    usleep((useconds_t)ms * 1000u + 20000u);   /* Wiedergabe abwarten */

    AUDIO_StopDMA();
    free(buf);
    return py_none();
}

/* ---------------------------- CON ------------------------------------ */
static PyObject* wt_CON_GetMetrics(PyObject *self, PyObject *args) {
    (void)self; int cols = 0, rows = 0;
    if (!PyArg_ParseTuple(args, ":CON_GetMetrics")) return NULL;
    CON_GetMetrics(&cols, &rows);
    return Py_BuildValue("(ii)", cols, rows);
}
static PyObject* wt_CON_GetPosition(PyObject *self, PyObject *args) {
    (void)self; int col = 0, row = 0;
    if (!PyArg_ParseTuple(args, ":CON_GetPosition")) return NULL;
    CON_GetPosition(&col, &row);
    return Py_BuildValue("(ii)", col, row);
}
static PyObject* wt_CON_EnableGecko(PyObject *self, PyObject *args) {
    (void)self; int channel, safe = 0;
    if (!PyArg_ParseTuple(args, "i|p:CON_EnableGecko", &channel, &safe)) return NULL;
    CON_EnableGecko(channel, safe);
    return py_none();
}

/* ---------------------------- PAD Sampling-Callback ------------------ */
static volatile u32 g_pad_sampling_events = 0;
static void wt_pad_sampling_cb(void) { g_pad_sampling_events++; }
static PyObject* wt_PAD_SetSamplingCallback(PyObject *self, PyObject *args) {
    (void)self; int enable;
    if (!PyArg_ParseTuple(args, "p:PAD_SetSamplingCallback", &enable)) return NULL;
    PAD_SetSamplingCallback(enable ? wt_pad_sampling_cb : NULL);
    return py_none();
}
static PyObject* wt_PAD_GetSamplingEvent(PyObject *self, PyObject *args) {
    (void)self; if (!PyArg_ParseTuple(args, ":PAD_GetSamplingEvent")) return NULL;
    u32 n = g_pad_sampling_events; g_pad_sampling_events = 0;
    return PyLong_FromUnsignedLong((unsigned long)n);
}

/* ---------------------------- Netzwerk / Sockets --------------------- */
/* Hilfsfunktionen: IP-String <-> sockaddr_in */
static PyObject* wt_ip_to_str(struct in_addr a) {
    char *s = inet_ntoa(a);
    return PyUnicode_FromString(s ? s : "");
}
static int wt_fill_sin(struct sockaddr_in *sin, const char *ip, int port) {
    memset(sin, 0, sizeof(*sin));
    sin->sin_len = sizeof(struct sockaddr_in);
    sin->sin_family = AF_INET;
    sin->sin_port = htons((u16)port);
    if (ip && ip[0]) {
        if (inet_aton(ip, &sin->sin_addr) == 0) return -1;
    } else {
        sin->sin_addr.s_addr = 0; /* INADDR_ANY */
    }
    return 0;
}

WT_GET_L0(net_get_status)
WT_VOID0(net_deinit)
WT_VOID0(net_wc24cleanup)

static PyObject* wt_net_gethostip(PyObject *self, PyObject *args) {
    (void)self; if (!PyArg_ParseTuple(args, ":net_gethostip")) return NULL;
    struct in_addr a; a.s_addr = (in_addr_t)net_gethostip();
    return wt_ip_to_str(a);
}
static PyObject* wt_net_get_mac_address(PyObject *self, PyObject *args) {
    (void)self; u8 mac[6] = {0};
    if (!PyArg_ParseTuple(args, ":net_get_mac_address")) return NULL;
    if (net_get_mac_address(mac) < 0) { PyErr_SetString(PyExc_RuntimeError, "net_get_mac_address failed"); return NULL; }
    return PyUnicode_FromFormat("%02x:%02x:%02x:%02x:%02x:%02x",
                                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
static PyObject* wt_net_gethostbyname(PyObject *self, PyObject *args) {
    (void)self; const char *name;
    if (!PyArg_ParseTuple(args, "s:net_gethostbyname", &name)) return NULL;
    struct hostent *h = net_gethostbyname(name);
    if (!h || !h->h_addr_list || !h->h_addr_list[0]) return py_none();
    struct in_addr a; memcpy(&a, h->h_addr_list[0], sizeof(a));
    return wt_ip_to_str(a);
}
static PyObject* wt_inet_addr(PyObject *self, PyObject *args) {
    (void)self; const char *cp;
    if (!PyArg_ParseTuple(args, "s:inet_addr", &cp)) return NULL;
    return PyLong_FromUnsignedLong((unsigned long)inet_addr(cp));
}
static PyObject* wt_inet_aton(PyObject *self, PyObject *args) {
    (void)self; const char *cp; struct in_addr a;
    if (!PyArg_ParseTuple(args, "s:inet_aton", &cp)) return NULL;
    if (inet_aton(cp, &a) == 0) return py_none();
    return PyLong_FromUnsignedLong((unsigned long)a.s_addr);
}
static PyObject* wt_inet_ntoa(PyObject *self, PyObject *args) {
    (void)self; unsigned long v; struct in_addr a;
    if (!PyArg_ParseTuple(args, "k:inet_ntoa", &v)) return NULL;
    a.s_addr = (in_addr_t)v;
    return wt_ip_to_str(a);
}
static PyObject* wt_if_config(PyObject *self, PyObject *args) {
    (void)self; int use_dhcp = 1, retries = 20;
    char ip[16] = "", nm[16] = "", gw[16] = "";
    if (!PyArg_ParseTuple(args, "|pi:if_config", &use_dhcp, &retries)) return NULL;
    s32 r = if_config(ip, nm, gw, use_dhcp ? true : false, retries);
    if (r < 0) return PyLong_FromLong((long)r);
    return Py_BuildValue("{s:s,s:s,s:s}", "ip", ip, "netmask", nm, "gateway", gw);
}

static PyObject* wt_net_socket(PyObject *self, PyObject *args) {
    (void)self; long domain = AF_INET, type = SOCK_STREAM, proto = 0;
    if (!PyArg_ParseTuple(args, "|lll:net_socket", &domain, &type, &proto)) return NULL;
    return PyLong_FromLong((long)net_socket((u32)domain, (u32)type, (u32)proto));
}
static PyObject* wt_net_bind(PyObject *self, PyObject *args) {
    (void)self; long s; const char *ip = ""; int port;
    if (!PyArg_ParseTuple(args, "lsi:net_bind", &s, &ip, &port)) return NULL;
    struct sockaddr_in sin;
    if (wt_fill_sin(&sin, ip, port) < 0) { PyErr_SetString(PyExc_ValueError, "invalid IP"); return NULL; }
    return PyLong_FromLong((long)net_bind((s32)s, (struct sockaddr *)&sin, sizeof(sin)));
}
static PyObject* wt_net_connect(PyObject *self, PyObject *args) {
    (void)self; long s; const char *ip; int port;
    if (!PyArg_ParseTuple(args, "lsi:net_connect", &s, &ip, &port)) return NULL;
    struct sockaddr_in sin;
    if (wt_fill_sin(&sin, ip, port) < 0) { PyErr_SetString(PyExc_ValueError, "invalid IP"); return NULL; }
    return PyLong_FromLong((long)net_connect((s32)s, (struct sockaddr *)&sin, sizeof(sin)));
}
static PyObject* wt_net_listen(PyObject *self, PyObject *args) {
    (void)self; long s, backlog = 5;
    if (!PyArg_ParseTuple(args, "l|l:net_listen", &s, &backlog)) return NULL;
    return PyLong_FromLong((long)net_listen((s32)s, (u32)backlog));
}
static PyObject* wt_net_accept(PyObject *self, PyObject *args) {
    (void)self; long s;
    if (!PyArg_ParseTuple(args, "l:net_accept", &s)) return NULL;
    struct sockaddr_in sin; socklen_t l = sizeof(sin);
    memset(&sin, 0, sizeof(sin));
    s32 fd = net_accept((s32)s, (struct sockaddr *)&sin, &l);
    if (fd < 0) return Py_BuildValue("(iOi)", (int)fd, Py_None, 0);
    PyObject *ipo = wt_ip_to_str(sin.sin_addr);
    PyObject *res = Py_BuildValue("(iOi)", (int)fd, ipo, (int)ntohs(sin.sin_port));
    Py_XDECREF(ipo);
    return res;
}
static PyObject* wt_net_close(PyObject *self, PyObject *args) {
    (void)self; long s;
    if (!PyArg_ParseTuple(args, "l:net_close", &s)) return NULL;
    return PyLong_FromLong((long)net_close((s32)s));
}
static PyObject* wt_net_shutdown(PyObject *self, PyObject *args) {
    (void)self; long s, how;
    if (!PyArg_ParseTuple(args, "ll:net_shutdown", &s, &how)) return NULL;
    return PyLong_FromLong((long)net_shutdown((s32)s, (u32)how));
}
static PyObject* wt_net_send(PyObject *self, PyObject *args) {
    (void)self; long s; Py_buffer buf; long flags = 0;
    if (!PyArg_ParseTuple(args, "ly*|l:net_send", &s, &buf, &flags)) return NULL;
    s32 r = net_send((s32)s, buf.buf, (s32)buf.len, (u32)flags);
    PyBuffer_Release(&buf);
    return PyLong_FromLong((long)r);
}
static PyObject* wt_net_write(PyObject *self, PyObject *args) {
    (void)self; long s; Py_buffer buf;
    if (!PyArg_ParseTuple(args, "ly*:net_write", &s, &buf)) return NULL;
    s32 r = net_write((s32)s, buf.buf, (s32)buf.len);
    PyBuffer_Release(&buf);
    return PyLong_FromLong((long)r);
}
static PyObject* wt_net_sendto(PyObject *self, PyObject *args) {
    (void)self; long s; Py_buffer buf; const char *ip; int port; long flags = 0;
    if (!PyArg_ParseTuple(args, "ly*si|l:net_sendto", &s, &buf, &ip, &port, &flags)) return NULL;
    struct sockaddr_in sin;
    if (wt_fill_sin(&sin, ip, port) < 0) { PyBuffer_Release(&buf); PyErr_SetString(PyExc_ValueError, "invalid IP"); return NULL; }
    s32 r = net_sendto((s32)s, buf.buf, (s32)buf.len, (u32)flags, (struct sockaddr *)&sin, sizeof(sin));
    PyBuffer_Release(&buf);
    return PyLong_FromLong((long)r);
}
static PyObject* wt_net_recv(PyObject *self, PyObject *args) {
    (void)self; long s, maxlen; long flags = 0;
    if (!PyArg_ParseTuple(args, "ll|l:net_recv", &s, &maxlen, &flags)) return NULL;
    if (maxlen < 0) maxlen = 0;
    char *b = (char *)malloc((size_t)maxlen > 0 ? (size_t)maxlen : 1);
    if (!b) return PyErr_NoMemory();
    s32 r = net_recv((s32)s, b, (s32)maxlen, (u32)flags);
    if (r < 0) { free(b); return PyLong_FromLong((long)r); }
    PyObject *o = PyBytes_FromStringAndSize(b, r);
    free(b);
    return o;
}
static PyObject* wt_net_read(PyObject *self, PyObject *args) {
    (void)self; long s, maxlen;
    if (!PyArg_ParseTuple(args, "ll:net_read", &s, &maxlen)) return NULL;
    if (maxlen < 0) maxlen = 0;
    char *b = (char *)malloc((size_t)maxlen > 0 ? (size_t)maxlen : 1);
    if (!b) return PyErr_NoMemory();
    s32 r = net_read((s32)s, b, (s32)maxlen);
    if (r < 0) { free(b); return PyLong_FromLong((long)r); }
    PyObject *o = PyBytes_FromStringAndSize(b, r);
    free(b);
    return o;
}
static PyObject* wt_net_recvfrom(PyObject *self, PyObject *args) {
    (void)self; long s, maxlen; long flags = 0;
    if (!PyArg_ParseTuple(args, "ll|l:net_recvfrom", &s, &maxlen, &flags)) return NULL;
    if (maxlen < 0) maxlen = 0;
    char *b = (char *)malloc((size_t)maxlen > 0 ? (size_t)maxlen : 1);
    if (!b) return PyErr_NoMemory();
    struct sockaddr_in sin; socklen_t l = sizeof(sin); memset(&sin, 0, sizeof(sin));
    s32 r = net_recvfrom((s32)s, b, (s32)maxlen, (u32)flags, (struct sockaddr *)&sin, &l);
    if (r < 0) { free(b); return Py_BuildValue("(OOi)", Py_None, Py_None, (int)r); }
    PyObject *data = PyBytes_FromStringAndSize(b, r);
    free(b);
    PyObject *ipo = wt_ip_to_str(sin.sin_addr);
    PyObject *res = Py_BuildValue("(OOi)", data, ipo, (int)ntohs(sin.sin_port));
    Py_XDECREF(data); Py_XDECREF(ipo);
    return res;
}
static PyObject* wt_net_getsockname(PyObject *self, PyObject *args) {
    (void)self; long s;
    if (!PyArg_ParseTuple(args, "l:net_getsockname", &s)) return NULL;
    struct sockaddr_in sin; socklen_t l = sizeof(sin); memset(&sin, 0, sizeof(sin));
    if (net_getsockname((s32)s, (struct sockaddr *)&sin, &l) < 0) return py_none();
    PyObject *ipo = wt_ip_to_str(sin.sin_addr);
    PyObject *res = Py_BuildValue("(Oi)", ipo, (int)ntohs(sin.sin_port));
    Py_XDECREF(ipo);
    return res;
}
static PyObject* wt_net_setsockopt(PyObject *self, PyObject *args) {
    (void)self; long s, level, opt, val;
    if (!PyArg_ParseTuple(args, "llll:net_setsockopt", &s, &level, &opt, &val)) return NULL;
    int v = (int)val;
    return PyLong_FromLong((long)net_setsockopt((s32)s, (u32)level, (u32)opt, &v, sizeof(v)));
}
/* net_getsockopt fehlt im Wii-libogc (nur net_setsockopt vorhanden) -> nicht gewrappt */
static PyObject* wt_net_fcntl(PyObject *self, PyObject *args) {
    (void)self; long s, cmd, flags;
    if (!PyArg_ParseTuple(args, "lll:net_fcntl", &s, &cmd, &flags)) return NULL;
    return PyLong_FromLong((long)net_fcntl((s32)s, (u32)cmd, (u32)flags));
}
static PyObject* wt_net_ioctl(PyObject *self, PyObject *args) {
    (void)self; long s, cmd; unsigned long arg = 0;
    if (!PyArg_ParseTuple(args, "ll|k:net_ioctl", &s, &cmd, &arg)) return NULL;
    u32 a = (u32)arg;
    return PyLong_FromLong((long)net_ioctl((s32)s, (u32)cmd, &a));
}
static PyObject* wt_net_poll(PyObject *self, PyObject *args) {
    (void)self; PyObject *lst; long timeout;
    if (!PyArg_ParseTuple(args, "Ol:net_poll", &lst, &timeout)) return NULL;
    PyObject *seq = PySequence_Fast(lst, "net_poll expects a list of (fd, events)");
    if (!seq) return NULL;
    Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);
    struct pollsd *sds = (struct pollsd *)calloc((size_t)(n > 0 ? n : 1), sizeof(struct pollsd));
    if (!sds) { Py_DECREF(seq); return PyErr_NoMemory(); }
    for (Py_ssize_t i = 0; i < n; i++) {
        PyObject *it = PySequence_Fast_GET_ITEM(seq, i);
        long fd = 0, ev = 0;
        if (!PyArg_ParseTuple(it, "ll", &fd, &ev)) { free(sds); Py_DECREF(seq); return NULL; }
        sds[i].socket = (s32)fd; sds[i].events = (u32)ev; sds[i].revents = 0;
    }
    s32 r = net_poll(sds, (s32)n, (s32)timeout);
    (void)r;
    PyObject *out = PyList_New(n);
    if (!out) { free(sds); Py_DECREF(seq); return NULL; }
    for (Py_ssize_t i = 0; i < n; i++)
        PyList_SET_ITEM(out, i, Py_BuildValue("(ik)", (int)sds[i].socket, (unsigned long)sds[i].revents));
    free(sds); Py_DECREF(seq);
    return out;
}

/* --- Modul-Init (net-Thread + Netzwerk) exponiert bereits: net_init --- */

static PyMethodDef wiitools_methods[] = {
    {"c_run", c_run, METH_VARARGS,
     "c_run(func_addr, *args) -> int : call C function at func_addr with up to "
     "8 int/pointer args (PPC EABI r3..r10); float/struct args unsupported"},
    {"fatInitDefault", fatinit, METH_VARARGS, "Mount SD card and USB"},
    {"VIDEO_WaitVSync", video_waitvsync, METH_VARARGS, "Wait one video frame"},
	{ "usleep", usleep2, METH_VARARGS, "usleep"},
    {"remove", file_remove, METH_VARARGS, "remove(path) -> 0 on success"},
    {"read_file", file_read, METH_VARARGS, "read_file(path) -> bytes"},
    {"write_file", file_write, METH_VARARGS, "write_file(path, data) -> bytes_written"},
    {"png_use", png_use, METH_VARARGS, "png_use(name) selects active PNG"},
    {"png_load", png_load, METH_VARARGS, "png_load(path) -> (width, height), stored as '__default__'"},
    {"png_load_named", png_load_named, METH_VARARGS, "png_load_named(path, name) -> (width, height)"},
    {"png_load_embedded", png_load_embedded, METH_VARARGS, "png_load_embedded() -> (width, height), name '__default__'"},
    {"png_load_embedded_named", png_load_embedded_named, METH_VARARGS, "png_load_embedded_named(name) -> (width, height)"},
    {"png_info", png_info, METH_VARARGS, "png_info() -> (width, height) or None for active PNG"},
    {"png_show", png_show, METH_VARARGS, "png_show(x, y) draws active PNG"},
    {"png_show_region", png_show_region, METH_VARARGS, "png_show_region(sx, sy, sw, sh, dx, dy)"},
    {"png_show_region_scaled", png_show_region_scaled, METH_VARARGS, "png_show_region_scaled(sx, sy, sw, sh, dx, dy, dw, dh)"},
    {"png_show_scaled", png_show_scaled, METH_VARARGS, "png_show_scaled(dx, dy, dw, dh)"},
    {"png_show_fullscreen", png_show_fullscreen, METH_VARARGS, "png_show_fullscreen()"},
    {"png", png_draw, METH_VARARGS, "png(screen_x, screen_y, image_x, image_y, image_w, image_h, screen_w, screen_h) active PNG"},
    {"png_quad", png_quad, METH_VARARGS, "png_quad(x1,y1,x2,y2,x3,y3,x4,y4,sx,sy,sw,sh)"},
    {"png_save", png_save, METH_VARARGS, "png_save(path) writes active PNG to file"},
    {"png_unload", png_unload, METH_VARARGS, "png_unload() unload active PNG"},
    {"png_unload_all", png_unload_all, METH_VARARGS, "png_unload_all()"},
    {"draw_rect", draw_rect, METH_VARARGS, "draw_rect(x, y, w, h, (r,g,b,a), angle_deg)"},
    {"gutil_prepare", gutil_prepare, METH_VARARGS, "gutil_prepare()  set GX state (VTXFMT2, TEX0 DIRECT, TEV MODULATE) required before CavEX gutil_* calls via c_run"},
    {"draw_circle", draw_circle, METH_VARARGS, "draw_circle(x, y, radius, (r,g,b,a))"},
    {"draw_oval", draw_oval, METH_VARARGS, "draw_oval(x, y, w, h, (r,g,b,a), angle_deg)"},
    {"render_text", render_text_py, METH_VARARGS, "render_text(x, y, text, size, shadow, (r,g,b,a), angle_deg)"},
    {"text_length", text_length_py, METH_VARARGS, "text_length(text, size) -> width_px"},
    {"surface_new", py_surface_new, METH_VARARGS, "surface_new(name, w, h) -> (w, h)  Create a blank off-screen RGBA surface"},
    {"surface_set_target", py_surface_set_target, METH_VARARGS, "surface_set_target(name)  Redirect draw_*, render_text, png_show into a surface"},
    {"surface_clear_target", py_surface_clear_target, METH_VARARGS, "surface_clear_target()  Restore rendering to the screen"},
    {"surface_fill", py_surface_fill, METH_VARARGS, "surface_fill(name, (r,g,b,a))  Fill a surface with a solid colour"},
    {"surface_get_size", py_surface_get_size, METH_VARARGS, "surface_get_size(name) -> (w, h) or None"},
    {"blit", py_blit, METH_VARARGS, "blit(name, x, y)  Draw a surface to the screen (always bypasses render target)"},
    {"surface_blit", py_surface_blit, METH_VARARGS, "surface_blit(src, dst_or_None, x, y[, sx, sy, sw, sh])  Copy one surface into another"},
    {"PAD_Init", pad_init, METH_VARARGS, "PAD_Init()"},
    {"PAD_Sync", pad_sync, METH_VARARGS, "PAD_Sync()"},
    {"PAD_ScanPads", pad_scanpads, METH_VARARGS, "PAD_ScanPads()"},
    {"PAD_Read", pad_read, METH_VARARGS, "PAD_Read() -> (ret, status_bytes)"},
    {"PAD_Reset", pad_reset, METH_VARARGS, "PAD_Reset(mask)"},
    {"PAD_Recalibrate", pad_recalibrate, METH_VARARGS, "PAD_Recalibrate(mask)"},
    {"PAD_Clamp", pad_clamp, METH_VARARGS, "PAD_Clamp() -> status_bytes"},
    {"PAD_ControlMotor", pad_control_motor, METH_VARARGS, "PAD_ControlMotor(chan, cmd)"},
    {"PAD_SetSpec", pad_set_spec, METH_VARARGS, "PAD_SetSpec(spec)"},
    {"PAD_ButtonsUp", pad_buttons_up, METH_VARARGS, "Button released? (button, chan) -> 0/1 (chan<0=any)"},
    {"PAD_ButtonsDown", pad_buttons_down, METH_VARARGS, "Button pressed? (button, chan) -> 0/1 (chan<0=any)"},
    {"PAD_ButtonsHeld", pad_buttons_held, METH_VARARGS, "Button held? (button, chan) -> 0/1 (chan<0=any)"},
    {"PAD_SubStickX", pad_substick_x, METH_VARARGS, "PAD_SubStickX(pad)"},
    {"PAD_SubStickY", pad_substick_y, METH_VARARGS, "PAD_SubStickY(pad)"},
    {"PAD_StickX", pad_stick_x, METH_VARARGS, "PAD_StickX(pad)"},
    {"PAD_StickY", pad_stick_y, METH_VARARGS, "PAD_StickY(pad)"},
    {"PAD_TriggerL", pad_trigger_l, METH_VARARGS, "PAD_TriggerL(pad)"},
    {"PAD_TriggerR", pad_trigger_r, METH_VARARGS, "PAD_TriggerR(pad)"},
    {"WPAD_Init", wpad_init, METH_VARARGS, "WPAD_Init()"},
    {"WPAD_ButtonsUp", wpad_up, METH_VARARGS, "Button released? (button, chan) -> 0/1"},
    {"WPAD_ButtonsDown", wpad_down, METH_VARARGS, "Button pressed? (button, chan) -> 0/1"},
    {"WPAD_ButtonsHeld", wpad_held, METH_VARARGS, "Button held? (button, chan) -> 0/1"},
    {"WPAD_ButtonsDown_all", wpad_buttons_down_all, METH_VARARGS, "WPAD_ButtonsDown_all(chan) -> WPADState with True/False per button"},
    {"WPAD_ButtonsUp_all",   wpad_buttons_up_all,   METH_VARARGS, "WPAD_ButtonsUp_all(chan) -> WPADState with True/False per button"},
    {"WPAD_ButtonsHeld_all", wpad_buttons_held_all, METH_VARARGS, "WPAD_ButtonsHeld_all(chan) -> WPADState with True/False per button"},

    {"WPAD_ControlSpeaker", wpad_control_speaker, METH_VARARGS, "WPAD_ControlSpeaker(chan, enable)"},
    {"WPAD_ReadEvent", wpad_read_event, METH_VARARGS, "WPAD_ReadEvent(chan) -> (ret, raw_data_bytes)"},
    {"WPAD_DroppedEvents", wpad_dropped_events, METH_VARARGS, "WPAD_DroppedEvents(chan)"},
    {"WPAD_Flush", wpad_flush, METH_VARARGS, "WPAD_Flush(chan)"},
    {"WPAD_ReadPending", wpad_read_pending, METH_VARARGS, "WPAD_ReadPending(chan), callbackless"},
    {"WPAD_SetDataFormat", wpad_set_data_format, METH_VARARGS, "WPAD_SetDataFormat(chan, fmt)"},
    {"WPAD_SetMotionPlus", wpad_set_motion_plus, METH_VARARGS, "WPAD_SetMotionPlus(chan, enable)"},
    {"WPAD_SetVRes", wpad_set_vres, METH_VARARGS, "WPAD_SetVRes(chan, xres, yres)"},
    {"WPAD_GetStatus", wpad_get_status, METH_VARARGS, "WPAD_GetStatus()"},
    {"WPAD_Probe", wpad_probe, METH_VARARGS, "WPAD_Probe(chan) -> (ret, type)"},
    {"WPAD_SetEventBufs", wpad_set_event_bufs, METH_VARARGS, "WPAD_SetEventBufs(chan, cnt) using internal buffers"},
    {"WPAD_Disconnect", wpad_disconnect, METH_VARARGS, "WPAD_Disconnect(chan)"},
    {"WPAD_IsSpeakerEnabled", wpad_is_speaker_enabled, METH_VARARGS, "WPAD_IsSpeakerEnabled(chan)"},
    {"WPAD_SendStreamData", wpad_send_stream_data, METH_VARARGS, "WPAD_SendStreamData(chan, data_bytes)"},
    {"WPAD_Shutdown", wpad_shutdown, METH_VARARGS, "WPAD_Shutdown()"},
    {"WPAD_SetIdleTimeout", wpad_set_idle_timeout, METH_VARARGS, "WPAD_SetIdleTimeout(seconds)"},
    {"WPAD_SetPowerButtonCallback", wpad_set_power_button_callback, METH_VARARGS, "Enable/disable internal power callback (0/1)"},
    {"WPAD_SetBatteryDeadCallback", wpad_set_battery_dead_callback, METH_VARARGS, "Enable/disable internal battery callback (0/1)"},
    {"WPAD_GetPowerButtonEvent", wpad_get_power_button_event, METH_VARARGS, "Get and clear last power callback chan"},
    {"WPAD_GetBatteryDeadEvent", wpad_get_battery_dead_event, METH_VARARGS, "Get and clear last battery callback chan"},
    {"WPAD_Rumble", wpad_rumble, METH_VARARGS, "WPAD_Rumble(chan, status)"},
    {"WPAD_SetIdleThresholds", wpad_set_idle_thresholds, METH_VARARGS, "WPAD_SetIdleThresholds(chan,btns,ir,accel,js,wb,mp)"},
    {"WPAD_EncodeData", wpad_encode_data, METH_VARARGS, "WPAD_EncodeData(flag, pcm_bytes, out_len) -> (status_bytes, encoded_bytes)"},
    {"WPAD_Data", wpad_data, METH_VARARGS, "WPAD_Data(chan) -> raw WPADData bytes"},
    {"WPAD_BatteryLevel", wpad_battery_level, METH_VARARGS, "WPAD_BatteryLevel(chan)"},
    {"WPAD_IR", wpad_ir, METH_VARARGS, "WPAD_IR(chan) -> raw ir_t bytes"},
    {"WPAD_Orientation", wpad_orientation, METH_VARARGS, "WPAD_Orientation(chan) -> raw orient_t bytes"},
    {"WPAD_GForce", wpad_gforce, METH_VARARGS, "WPAD_GForce(chan) -> raw gforce_t bytes"},
    {"WPAD_Accel", wpad_accel, METH_VARARGS, "WPAD_Accel(chan) -> raw vec3w_t bytes"},
    {"WPAD_Expansion", wpad_expansion, METH_VARARGS, "WPAD_Expansion(chan) -> raw expansion_t bytes"},

	{"terminal_init",  terminal_init,  METH_VARARGS, "Init Debug screen"},
	{"rendering_init", rendering_init, METH_VARARGS, "Init rendering screen"},
	{"rendering_adopt", rendering_adopt, METH_VARARGS,
	 "rendering_adopt(mode_ptr, fb_ptr[, empty_q]) -> uebernimmt das vom Host initialisierte Rendering, ohne neu zu initialisieren"},
    {"curl_request", (PyCFunction)wiitools_curl_request, METH_VARARGS | METH_KEYWORDS,
     "curl_request(method, url, data=None, headers=None, timeout_ms=30000, verify_peer=0, verify_host=0, follow_redirects=1, user_agent=None, ca_file=None) -> dict"},
    {"curl_get", (PyCFunction)wiitools_curl_get, METH_VARARGS | METH_KEYWORDS,
     "curl_get(url, headers=None, timeout_ms=30000, verify_peer=0, verify_host=0, follow_redirects=1, user_agent=None, ca_file=None) -> dict"},
    {"curl_post", (PyCFunction)wiitools_curl_post, METH_VARARGS | METH_KEYWORDS,
     "curl_post(url, data, headers=None, timeout_ms=30000, verify_peer=0, verify_host=0, follow_redirects=1, user_agent=None, ca_file=None) -> dict"},
    {"WPAD_ScanPads", wpad_scan_pads, METH_VARARGS, "WPAD_ScanPads()  Read all Wiimote button states for this frame"},
    {"set_screen_size", py_set_screen_size, METH_VARARGS,
     "set_screen_size(w, h)  Set logical resolution with letterboxing; set_screen_size(0,0) resets to physical"},
    {"render_update", render_update, METH_VARARGS, "render_update()  Flush the framebuffer and wait for VSync, without scanning pads"},
    {"update", update, METH_VARARGS, "WPAD_ScanPads + render_update combined; call once per frame"},
	{"IsNetReady", IsNetReady, METH_VARARGS, "is net ready?"},
    {"get_local_ip", get_local_ip, METH_VARARGS, "get_local_ip() -> primary local IPv4 address string or empty string"},
    {"init", init, METH_VARARGS, "init() -> initialize all (terminal_init() and other ...)"},
    {"net_init", init_network, METH_VARARGS, "net_init() -> initialize network thread"},

    /* ---- CONF (nur lesend) ---- */
    {"CONF_Init", wt_CONF_Init, METH_VARARGS, "CONF_Init() -> int"},
    {"CONF_GetLength", wt_CONF_GetLength, METH_VARARGS, "CONF_GetLength(name) -> int"},
    {"CONF_GetType", wt_CONF_GetType, METH_VARARGS, "CONF_GetType(name) -> int"},
    {"CONF_Get", wt_CONF_Get, METH_VARARGS, "CONF_Get(name) -> bytes"},
    {"CONF_GetShutdownMode", wt_CONF_GetShutdownMode, METH_VARARGS, "CONF_GetShutdownMode() -> int"},
    {"CONF_GetIdleLedMode", wt_CONF_GetIdleLedMode, METH_VARARGS, "CONF_GetIdleLedMode() -> int"},
    {"CONF_GetProgressiveScan", wt_CONF_GetProgressiveScan, METH_VARARGS, "CONF_GetProgressiveScan() -> int"},
    {"CONF_GetEuRGB60", wt_CONF_GetEuRGB60, METH_VARARGS, "CONF_GetEuRGB60() -> int"},
    {"CONF_GetIRSensitivity", wt_CONF_GetIRSensitivity, METH_VARARGS, "CONF_GetIRSensitivity() -> int"},
    {"CONF_GetSensorBarPosition", wt_CONF_GetSensorBarPosition, METH_VARARGS, "CONF_GetSensorBarPosition() -> int"},
    {"CONF_GetPadSpeakerVolume", wt_CONF_GetPadSpeakerVolume, METH_VARARGS, "CONF_GetPadSpeakerVolume() -> int"},
    {"CONF_GetPadMotorMode", wt_CONF_GetPadMotorMode, METH_VARARGS, "CONF_GetPadMotorMode() -> int"},
    {"CONF_GetSoundMode", wt_CONF_GetSoundMode, METH_VARARGS, "CONF_GetSoundMode() -> int (0=Mono,1=Stereo,2=Surround)"},
    {"CONF_GetLanguage", wt_CONF_GetLanguage, METH_VARARGS, "CONF_GetLanguage() -> int"},
    {"CONF_GetScreenSaverMode", wt_CONF_GetScreenSaverMode, METH_VARARGS, "CONF_GetScreenSaverMode() -> int"},
    {"CONF_GetAspectRatio", wt_CONF_GetAspectRatio, METH_VARARGS, "CONF_GetAspectRatio() -> int (0=4:3,1=16:9)"},
    {"CONF_GetEULA", wt_CONF_GetEULA, METH_VARARGS, "CONF_GetEULA() -> int"},
    {"CONF_GetWiiConnect24", wt_CONF_GetWiiConnect24, METH_VARARGS, "CONF_GetWiiConnect24() -> int"},
    {"CONF_GetRegion", wt_CONF_GetRegion, METH_VARARGS, "CONF_GetRegion() -> int"},
    {"CONF_GetArea", wt_CONF_GetArea, METH_VARARGS, "CONF_GetArea() -> int"},
    {"CONF_GetVideo", wt_CONF_GetVideo, METH_VARARGS, "CONF_GetVideo() -> int"},
    {"CONF_GetCounterBias", wt_CONF_GetCounterBias, METH_VARARGS, "CONF_GetCounterBias() -> int"},
    {"CONF_GetDisplayOffsetH", wt_CONF_GetDisplayOffsetH, METH_VARARGS, "CONF_GetDisplayOffsetH() -> int"},
    {"CONF_GetNickName", wt_CONF_GetNickName, METH_VARARGS, "CONF_GetNickName() -> str (Konsolen-Spitzname)"},
    {"CONF_GetParentalPassword", wt_CONF_GetParentalPassword, METH_VARARGS, "CONF_GetParentalPassword() -> str"},
    {"CONF_GetParentalAnswer", wt_CONF_GetParentalAnswer, METH_VARARGS, "CONF_GetParentalAnswer() -> str"},
    {"CONF_GetPadDevices", wt_CONF_GetPadDevices, METH_VARARGS, "CONF_GetPadDevices() -> int (Anzahl registrierter Geräte)"},

    /* ---- SYS ---- */
    {"SYS_Time", wt_SYS_Time, METH_VARARGS, "SYS_Time() -> int (Wii-Timebase-Ticks)"},
    {"SYS_ResetButtonDown", wt_SYS_ResetButtonDown, METH_VARARGS, "SYS_ResetButtonDown() -> int"},
    {"SYS_GetHollywoodRevision", wt_SYS_GetHollywoodRevision, METH_VARARGS, "SYS_GetHollywoodRevision() -> int"},
    {"SYS_GetCounterBias", wt_SYS_GetCounterBias, METH_VARARGS, "SYS_GetCounterBias() -> int"},
    {"SYS_SetCounterBias", wt_SYS_SetCounterBias, METH_VARARGS, "SYS_SetCounterBias(bias)"},
    {"SYS_GetDisplayOffsetH", wt_SYS_GetDisplayOffsetH, METH_VARARGS, "SYS_GetDisplayOffsetH() -> int"},
    {"SYS_SetDisplayOffsetH", wt_SYS_SetDisplayOffsetH, METH_VARARGS, "SYS_SetDisplayOffsetH(offset)"},
    {"SYS_GetEuRGB60", wt_SYS_GetEuRGB60, METH_VARARGS, "SYS_GetEuRGB60() -> int"},
    {"SYS_SetEuRGB60", wt_SYS_SetEuRGB60, METH_VARARGS, "SYS_SetEuRGB60(enable)"},
    {"SYS_GetLanguage", wt_SYS_GetLanguage, METH_VARARGS, "SYS_GetLanguage() -> int"},
    {"SYS_SetLanguage", wt_SYS_SetLanguage, METH_VARARGS, "SYS_SetLanguage(lang)"},
    {"SYS_GetProgressiveScan", wt_SYS_GetProgressiveScan, METH_VARARGS, "SYS_GetProgressiveScan() -> int"},
    {"SYS_SetProgressiveScan", wt_SYS_SetProgressiveScan, METH_VARARGS, "SYS_SetProgressiveScan(enable)"},
    {"SYS_GetSoundMode", wt_SYS_GetSoundMode, METH_VARARGS, "SYS_GetSoundMode() -> int"},
    {"SYS_SetSoundMode", wt_SYS_SetSoundMode, METH_VARARGS, "SYS_SetSoundMode(mode)"},
    {"SYS_GetVideoMode", wt_SYS_GetVideoMode, METH_VARARGS, "SYS_GetVideoMode() -> int"},
    {"SYS_SetVideoMode", wt_SYS_SetVideoMode, METH_VARARGS, "SYS_SetVideoMode(mode)"},
    {"SYS_GetWirelessID", wt_SYS_GetWirelessID, METH_VARARGS, "SYS_GetWirelessID(chan) -> int"},
    {"SYS_SetWirelessID", wt_SYS_SetWirelessID, METH_VARARGS, "SYS_SetWirelessID(chan, id)"},
    {"SYS_GetGBSMode", wt_SYS_GetGBSMode, METH_VARARGS, "SYS_GetGBSMode() -> int"},
    {"SYS_SetGBSMode", wt_SYS_SetGBSMode, METH_VARARGS, "SYS_SetGBSMode(mode)"},
    {"SYS_GetFontEncoding", wt_SYS_GetFontEncoding, METH_VARARGS, "SYS_GetFontEncoding() -> int"},
    {"SYS_GetArena1Size", wt_SYS_GetArena1Size, METH_VARARGS, "SYS_GetArena1Size() -> int (freier MEM1-Arena in Bytes)"},
    {"SYS_GetArena2Size", wt_SYS_GetArena2Size, METH_VARARGS, "SYS_GetArena2Size() -> int (freier MEM2-Arena in Bytes)"},
    {"SYS_STDIO_Report", wt_SYS_STDIO_Report, METH_VARARGS, "SYS_STDIO_Report(enable)"},
    {"SYS_Report", wt_SYS_Report, METH_VARARGS, "SYS_Report(msg)  Nachricht ins Systemlog"},
    {"SYS_ResetSystem", wt_SYS_ResetSystem, METH_VARARGS, "SYS_ResetSystem(reset[, code=0, force_menu=0])"},
    {"SYS_SetPowerCallback", wt_SYS_SetPowerCallback, METH_VARARGS, "SYS_SetPowerCallback(enable)  danach mit SYS_GetPowerEvent() pollen"},
    {"SYS_GetPowerEvent", wt_SYS_GetPowerEvent, METH_VARARGS, "SYS_GetPowerEvent() -> Anzahl seit letztem Abruf"},
    {"SYS_SetResetCallback", wt_SYS_SetResetCallback, METH_VARARGS, "SYS_SetResetCallback(enable)  danach mit SYS_GetResetEvent() pollen"},
    {"SYS_GetResetEvent", wt_SYS_GetResetEvent, METH_VARARGS, "SYS_GetResetEvent() -> Anzahl seit letztem Abruf"},
    {"SYS_CreateAlarm", wt_SYS_CreateAlarm, METH_VARARGS, "SYS_CreateAlarm() -> handle"},
    {"SYS_SetAlarm", wt_SYS_SetAlarm, METH_VARARGS, "SYS_SetAlarm(handle, seconds)  Einzel-Alarm; mit SYS_GetAlarmEvent() pollen"},
    {"SYS_SetPeriodicAlarm", wt_SYS_SetPeriodicAlarm, METH_VARARGS, "SYS_SetPeriodicAlarm(handle, start_s, period_s)"},
    {"SYS_RemoveAlarm", wt_SYS_RemoveAlarm, METH_VARARGS, "SYS_RemoveAlarm(handle)"},
    {"SYS_CancelAlarm", wt_SYS_CancelAlarm, METH_VARARGS, "SYS_CancelAlarm(handle)"},
    {"SYS_GetAlarmEvent", wt_SYS_GetAlarmEvent, METH_VARARGS, "SYS_GetAlarmEvent() -> Anzahl ausgelöster Alarme seit letztem Abruf"},

    /* ---- AUDIO ---- */
    {"AUDIO_Init", wt_AUDIO_Init, METH_VARARGS, "AUDIO_Init()"},
    {"AUDIO_InitDMA", wt_AUDIO_InitDMA, METH_VARARGS, "AUDIO_InitDMA(startaddr, len)"},
    {"AUDIO_StartDMA", wt_AUDIO_StartDMA, METH_VARARGS, "AUDIO_StartDMA()"},
    {"AUDIO_StopDMA", wt_AUDIO_StopDMA, METH_VARARGS, "AUDIO_StopDMA()"},
    {"AUDIO_GetDMAEnableFlag", wt_AUDIO_GetDMAEnableFlag, METH_VARARGS, "AUDIO_GetDMAEnableFlag() -> int"},
    {"AUDIO_GetDMABytesLeft", wt_AUDIO_GetDMABytesLeft, METH_VARARGS, "AUDIO_GetDMABytesLeft() -> int"},
    {"AUDIO_GetDMALength", wt_AUDIO_GetDMALength, METH_VARARGS, "AUDIO_GetDMALength() -> int"},
    {"AUDIO_GetDMAStartAddr", wt_AUDIO_GetDMAStartAddr, METH_VARARGS, "AUDIO_GetDMAStartAddr() -> int"},
    {"AUDIO_SetDSPSampleRate", wt_AUDIO_SetDSPSampleRate, METH_VARARGS, "AUDIO_SetDSPSampleRate(rate)"},
    {"AUDIO_GetDSPSampleRate", wt_AUDIO_GetDSPSampleRate, METH_VARARGS, "AUDIO_GetDSPSampleRate() -> int"},
    {"AUDIO_RegisterDMACallback", wt_AUDIO_RegisterDMACallback, METH_VARARGS, "AUDIO_RegisterDMACallback(enable)  mit AUDIO_GetDMAEvent() pollen"},
    {"AUDIO_GetDMAEvent", wt_AUDIO_GetDMAEvent, METH_VARARGS, "AUDIO_GetDMAEvent() -> Anzahl seit letztem Abruf"},
    {"audio_play_tone", wt_audio_play_tone, METH_VARARGS, "audio_play_tone(freq=440.0, ms=500, volume=8000)  Sinuston ausgeben (48kHz Stereo, blockierend)"},

    /* ---- CON (Konsole) ---- */
    {"CON_GetMetrics", wt_CON_GetMetrics, METH_VARARGS, "CON_GetMetrics() -> (cols, rows)"},
    {"CON_GetPosition", wt_CON_GetPosition, METH_VARARGS, "CON_GetPosition() -> (col, row)"},
    {"CON_EnableGecko", wt_CON_EnableGecko, METH_VARARGS, "CON_EnableGecko(channel[, safe=0])"},

    /* ---- PAD Sampling-Callback ---- */
    {"PAD_SetSamplingCallback", wt_PAD_SetSamplingCallback, METH_VARARGS, "PAD_SetSamplingCallback(enable)  mit PAD_GetSamplingEvent() pollen"},
    {"PAD_GetSamplingEvent", wt_PAD_GetSamplingEvent, METH_VARARGS, "PAD_GetSamplingEvent() -> Anzahl seit letztem Abruf"},

    /* ---- Netzwerk / Sockets ---- */
    {"net_get_status", wt_net_get_status, METH_VARARGS, "net_get_status() -> int"},
    {"net_deinit", wt_net_deinit, METH_VARARGS, "net_deinit()"},
    {"net_wc24cleanup", wt_net_wc24cleanup, METH_VARARGS, "net_wc24cleanup()"},
    {"net_gethostip", wt_net_gethostip, METH_VARARGS, "net_gethostip() -> str (eigene IPv4)"},
    {"net_get_mac_address", wt_net_get_mac_address, METH_VARARGS, "net_get_mac_address() -> 'aa:bb:cc:dd:ee:ff'"},
    {"net_gethostbyname", wt_net_gethostbyname, METH_VARARGS, "net_gethostbyname(name) -> ip_str oder None"},
    {"inet_addr", wt_inet_addr, METH_VARARGS, "inet_addr(ip_str) -> int"},
    {"inet_aton", wt_inet_aton, METH_VARARGS, "inet_aton(ip_str) -> int oder None"},
    {"inet_ntoa", wt_inet_ntoa, METH_VARARGS, "inet_ntoa(addr_int) -> ip_str"},
    {"if_config", wt_if_config, METH_VARARGS, "if_config(use_dhcp=True, retries=20) -> dict{ip,netmask,gateway} oder int<0"},
    {"net_socket", wt_net_socket, METH_VARARGS, "net_socket(domain=AF_INET, type=SOCK_STREAM, proto=0) -> fd"},
    {"net_bind", wt_net_bind, METH_VARARGS, "net_bind(fd, ip, port) -> int"},
    {"net_connect", wt_net_connect, METH_VARARGS, "net_connect(fd, ip, port) -> int"},
    {"net_listen", wt_net_listen, METH_VARARGS, "net_listen(fd, backlog=5) -> int"},
    {"net_accept", wt_net_accept, METH_VARARGS, "net_accept(fd) -> (fd, ip, port)"},
    {"net_close", wt_net_close, METH_VARARGS, "net_close(fd) -> int"},
    {"net_shutdown", wt_net_shutdown, METH_VARARGS, "net_shutdown(fd, how) -> int"},
    {"net_send", wt_net_send, METH_VARARGS, "net_send(fd, data, flags=0) -> int"},
    {"net_write", wt_net_write, METH_VARARGS, "net_write(fd, data) -> int"},
    {"net_sendto", wt_net_sendto, METH_VARARGS, "net_sendto(fd, data, ip, port, flags=0) -> int"},
    {"net_recv", wt_net_recv, METH_VARARGS, "net_recv(fd, maxlen, flags=0) -> bytes (oder int<0)"},
    {"net_read", wt_net_read, METH_VARARGS, "net_read(fd, maxlen) -> bytes (oder int<0)"},
    {"net_recvfrom", wt_net_recvfrom, METH_VARARGS, "net_recvfrom(fd, maxlen, flags=0) -> (bytes, ip, port)"},
    {"net_getsockname", wt_net_getsockname, METH_VARARGS, "net_getsockname(fd) -> (ip, port) oder None"},
    {"net_setsockopt", wt_net_setsockopt, METH_VARARGS, "net_setsockopt(fd, level, optname, int_value) -> int"},
    {"net_fcntl", wt_net_fcntl, METH_VARARGS, "net_fcntl(fd, cmd, flags) -> int"},
    {"net_ioctl", wt_net_ioctl, METH_VARARGS, "net_ioctl(fd, cmd, arg=0) -> int"},
    {"net_poll", wt_net_poll, METH_VARARGS, "net_poll([(fd, events), ...], timeout_ms) -> [(fd, revents), ...]"},

    {NULL, NULL, 0, NULL}
};

static void
wiitools_add_uint_constant(PyObject *m, const char *name, unsigned long value)
{
    PyObject *v = PyLong_FromUnsignedLong(value);
    if (v == NULL)
        return;
    PyModule_AddObject(m, (char *)name, v);
}

#define WIITOOLS_ADD_WPAD_CONST(mod, name) \
    wiitools_add_uint_constant((mod), #name, (unsigned long)(name))

static struct PyModuleDef wiitools_module = {
    PyModuleDef_HEAD_INIT,
    "wiitools",
    "Wii helper module.",
    -1,
    wiitools_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC
PyInit_wiitools(void)
{
    PyObject *m;
    PyObject *v;

    m = PyModule_Create(&wiitools_module);
    if (m == NULL)
        return NULL;

    /* Register WPADState named-tuple type */
    WPADState_Type = PyStructSequence_NewType(&WPADState_desc);
    if (WPADState_Type == NULL)
        return NULL;
    Py_INCREF(WPADState_Type);
    PyModule_AddObject(m, "WPADState", (PyObject *)WPADState_Type);

    WIITOOLS_ADD_WPAD_CONST(m, WPAD_BUTTON_2);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_BUTTON_1);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_BUTTON_B);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_BUTTON_A);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_BUTTON_MINUS);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_BUTTON_HOME);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_BUTTON_LEFT);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_BUTTON_RIGHT);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_BUTTON_DOWN);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_BUTTON_UP);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_BUTTON_PLUS);

    WIITOOLS_ADD_WPAD_CONST(m, WPAD_NUNCHUK_BUTTON_Z);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_NUNCHUK_BUTTON_C);

    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CLASSIC_BUTTON_UP);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CLASSIC_BUTTON_LEFT);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CLASSIC_BUTTON_ZR);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CLASSIC_BUTTON_X);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CLASSIC_BUTTON_A);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CLASSIC_BUTTON_Y);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CLASSIC_BUTTON_B);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CLASSIC_BUTTON_ZL);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CLASSIC_BUTTON_FULL_R);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CLASSIC_BUTTON_PLUS);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CLASSIC_BUTTON_HOME);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CLASSIC_BUTTON_MINUS);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CLASSIC_BUTTON_FULL_L);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CLASSIC_BUTTON_DOWN);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CLASSIC_BUTTON_RIGHT);

    WIITOOLS_ADD_WPAD_CONST(m, WPAD_GUITAR_HERO_3_BUTTON_STRUM_UP);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_GUITAR_HERO_3_BUTTON_YELLOW);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_GUITAR_HERO_3_BUTTON_GREEN);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_GUITAR_HERO_3_BUTTON_BLUE);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_GUITAR_HERO_3_BUTTON_RED);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_GUITAR_HERO_3_BUTTON_ORANGE);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_GUITAR_HERO_3_BUTTON_PLUS);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_GUITAR_HERO_3_BUTTON_MINUS);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_GUITAR_HERO_3_BUTTON_STRUM_DOWN);

    WIITOOLS_ADD_WPAD_CONST(m, WPAD_DATA_BUTTONS);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_THRESH_DEFAULT_BUTTONS);

    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CHAN_ALL);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CHAN_0);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CHAN_1);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CHAN_2);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_CHAN_3);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_BALANCE_BOARD);
    WIITOOLS_ADD_WPAD_CONST(m, WPAD_MAX_WIIMOTES);
    
    wiitools_add_uint_constant(m, "width",  gfx_width );
    wiitools_add_uint_constant(m, "height", gfx_height);

    /* GameCube controller button constants */
#define ADD_PAD(name) PyModule_AddIntConstant(m, #name, (int)(name))
    ADD_PAD(PAD_BUTTON_LEFT);
    ADD_PAD(PAD_BUTTON_RIGHT);
    ADD_PAD(PAD_BUTTON_DOWN);
    ADD_PAD(PAD_BUTTON_UP);
    ADD_PAD(PAD_TRIGGER_Z);
    ADD_PAD(PAD_TRIGGER_R);
    ADD_PAD(PAD_TRIGGER_L);
    ADD_PAD(PAD_BUTTON_A);
    ADD_PAD(PAD_BUTTON_B);
    ADD_PAD(PAD_BUTTON_X);
    ADD_PAD(PAD_BUTTON_Y);
    ADD_PAD(PAD_BUTTON_MENU);
    ADD_PAD(PAD_BUTTON_START);
    ADD_PAD(PAD_CHAN0);
    ADD_PAD(PAD_CHAN1);
    ADD_PAD(PAD_CHAN2);
    ADD_PAD(PAD_CHAN3);
    ADD_PAD(PAD_CHANMAX);
    PyModule_AddIntConstant(m, "PAD_CHAN_ALL", -1);
#undef ADD_PAD

    /* Socket-Konstanten (network.h) */
#define ADD_INT(name) PyModule_AddIntConstant(m, #name, (int)(name))
    ADD_INT(AF_INET);
    ADD_INT(PF_INET);
    ADD_INT(SOCK_STREAM);
    ADD_INT(SOCK_DGRAM);
    ADD_INT(IPPROTO_IP);
    ADD_INT(IPPROTO_TCP);
    ADD_INT(IPPROTO_UDP);
    ADD_INT(SOL_SOCKET);
    ADD_INT(SO_REUSEADDR);
    ADD_INT(TCP_NODELAY);
    ADD_INT(POLLIN);
    ADD_INT(POLLOUT);
    ADD_INT(FIONBIO);
    ADD_INT(O_NONBLOCK);

    /* SYS_ResetSystem-Modi (system.h) */
    ADD_INT(SYS_RESTART);
    ADD_INT(SYS_HOTRESET);
    ADD_INT(SYS_SHUTDOWN);
    ADD_INT(SYS_RETURNTOMENU);
    ADD_INT(SYS_POWEROFF);
    ADD_INT(SYS_POWEROFF_STANDBY);
    ADD_INT(SYS_POWEROFF_IDLE);
#undef ADD_INT

    v = PyUnicode_FromString("0.2");
    if (v != NULL) {
        PyModule_AddObject(m, "__version__", v);
    } else {
        PyErr_Clear();
    }
    return m;
}
