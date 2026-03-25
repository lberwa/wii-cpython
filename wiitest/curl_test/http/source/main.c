/* curl_test main.c
 * Simple libcurl HTTP test for Wii: GET http://192.168.15.152:8000
 * Prints libcurl verbose/debug output and response via terminal_print.
 */

#define B1 600
#define B2 200112
#include <my_text_renderer.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef HW_RVL
#define HW_RVL
#endif
//#define _WIN32 

#ifndef NETWORK_H22
#define NETWORK_H22 1
#endif

#include <gccore.h>
#include "../../../curl/include/curl/curl.h"
#include <network.h>

#include <stdlib.h>

#define __XSI_VISIBLE 600
#define __POSIX_VISIBLE 200112
#define _GNU_SOURCE
#include <unistd.h>


#include <time.h>



/* Provide minimal mbedtls platform hooks required by the static mbedtls
   libraries used by the cross-build. The full project provides these in
   the wiitools module; the standalone curl_test must provide them too so
   the linker can resolve symbols when linking against libmbedtls.a. */

static int g_entropy_seeded = 0;

/* Simple entropy provider used only for tests. Matches symbol name used by
   mbedtls: mbedtls_platform_get_entropy(...) */
int mbedtls_platform_get_entropy(unsigned int flags,
                                 size_t *estimate_bits,
                                 unsigned char *output,
                                 size_t output_size)
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

/* Millisecond timer expected by mbedtls */
unsigned long mbedtls_ms_time(void)
{
    clock_t c = clock();
    if (c <= 0)
        return (unsigned long)0;
    return (unsigned long)((((unsigned long)c) * 1000u) / (unsigned long)CLOCKS_PER_SEC);
}


typedef struct {
    char *data;
    size_t len;
    size_t cap;
} buf_t;

/* Preconnected socket storage for opensocket callback */
static int g_pre_sock = -1;
static int g_pre_sock_last = -1;
static int g_open_cb_called = 0;
static int g_sockopt_cb_called = 0;

/* opensocket callback used by libcurl to obtain a socket we've already
   created and connected using the Wii/net API. Must be file-scope (not
   a nested function) to compile cleanly across toolchains. */
static curl_socket_t opensocket_cb(void *clientp, curlsocktype purpose, struct curl_sockaddr *address)
{
    (void)clientp; (void)purpose; (void)address;
    g_open_cb_called = 1;
    if (g_pre_sock >= 0) {
        curl_socket_t s = (curl_socket_t)g_pre_sock;
        g_pre_sock_last = g_pre_sock;
        g_pre_sock = -1; /* hand over ownership */
        return s;
    }
    return CURL_SOCKET_BAD;
}

/* Tell libcurl our socket is already connected, so it must not call connect(). */
static int sockopt_cb(void *clientp, curl_socket_t curlfd, curlsocktype purpose)
{
    (void)clientp; (void)purpose;
    g_sockopt_cb_called = 1;
    if ((int)curlfd == g_pre_sock_last) {
        return CURL_SOCKOPT_ALREADY_CONNECTED;
    }
    return CURL_SOCKOPT_OK;
}

static void buf_init(buf_t *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}
static void buf_free(buf_t *b) {
    if (b->data) free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}
static int buf_reserve(buf_t *b, size_t want) {
    if (b->cap >= want) return 0;
    size_t nc = b->cap ? b->cap * 2 : 1024;
    while (nc < want) nc *= 2;
    char *p = (char*)realloc(b->data, nc);
    if (!p) return -1;
    b->data = p; b->cap = nc; return 0;
}
static int buf_append(buf_t *b, const char *p, size_t n) {
    if (n == 0) return 0;
    if (b->len + n + 1 < b->len) return -1; /* overflow */
    if (buf_reserve(b, b->len + n + 1) != 0) return -1;
    memcpy(b->data + b->len, p, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

/* libcurl write callback */
static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    buf_t *b = (buf_t*)userdata;
    size_t n = size * nmemb;
    if (!b) return 0;
    if (buf_append(b, ptr, n) != 0) return 0;
    return n;
}

/* libcurl debug callback (verbose) */
static int debug_cb(CURL *handle, curl_infotype type, char *data, size_t size, void *userdata) {
    (void)handle; (void)type;
    buf_t *b = (buf_t*)userdata;
    if (!b || !data || size == 0) return 0;
    /* append as-is (verbose is text) */
    buf_append(b, data, size);
    return 0;
}

/* Print long text by splitting into 200-char chunks using terminal_print */
static void terminal_print_long(const char *s) {
    if (!s) return;
    size_t i = 0, n = strlen(s);
    const size_t chunk = 200;
    char tmp[256];
    while (i < n) {
        size_t take = (n - i) < chunk ? (n - i) : chunk;
        memcpy(tmp, s + i, take);
        tmp[take] = '\0';
        terminal_print(tmp);
        i += take;
    }
}

static void raw_http_get_fallback(void)
{
    int sock = net_socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        terminal_print("raw: socket create failed");
        return;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = 8000; /* libogc expects host-order port */
    IP4_ADDR(&server.sin_addr, 192,168,15,152);

    terminal_print("raw: connecting...");
    if (net_connect(sock, (struct sockaddr *)&server, sizeof(server)) != 0) {
        terminal_print("raw: connect failed");
        net_close(sock);
        return;
    }

    {
        const char *req =
            "GET /fallback HTTP/1.1\r\n"
            "Host: 192.168.15.152\r\n"
            "X-Fallback: 1\r\n"
            "Connection: close\r\n"
            "\r\n";
        net_send(sock, req, strlen(req), 0);
    }

    terminal_print("raw: receiving...");
    {
        char buffer[512];
        int received = net_recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (received > 0) {
            buffer[received] = '\0';
            terminal_print_long(buffer);
        } else {
            terminal_print("raw: recv failed");
        }
    }

    net_close(sock);
    terminal_print("raw: done");
}

int main(void) {
    CURL *easy = NULL;
    CURLcode cc = CURLE_OK;
    buf_t body, dbg;
    long status = 0;
    char errorbuf[CURL_ERROR_SIZE];

    video_init_custom();
    terminal_clear();
    terminal_print("curl_test: start");

    /* 1. Netzwerk initialisieren */
    if (net_init() < 0) {
        terminal_print("net_init failed");
        return 1;
    }/*
#undef __XSI_VISIBLE
#undef __POSIX_VISIBLE
#define __XSI_VISIBLE 600
#define __POSIX_VISIBLE 200112
#undef _SYS_UNISTD_H
#undef _UNISTD_H_
#include <unistd.h>
#include <time.h>*/

    /* WICHTIG: Warte kurz, bis das Netzwerk wirklich bereit ist */
    u32 ip = 0;
    int timeout = 0;
    while ((ip = net_gethostip()) == 0 && timeout < 50) {
        #undef write
        #undef read
#undef _SYS_UNISTD_H
#undef _UNISTD_H_
#include <unistd.h>
//#include <time.h>
        usleep(100000); // 100ms
        #define _SYS_UNISTD_H
        #define _UNISTD_H_
        #undef write
        #undef read
        timeout++;
    }

/*
#undef _SYS_FEATURES_H
#include <sys/features.h>
#undef _SYS_UNISTD_H
#undef _UNISTD_H_
#include <unistd.h>
#include <time.h>
*/


    terminal_print("Netzwerk bereit!");

    buf_init(&body);
    buf_init(&dbg);
    memset(errorbuf, 0, sizeof(errorbuf));

    terminal_print("curl: global_init...");
    /* 2. libcurl global initialisieren */
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        terminal_print("curl_global_init failed");
        return 1;
    }
    terminal_print("curl: global_init ok");

    terminal_print("curl: easy_init...");
    easy = curl_easy_init();
    if (!easy) {
        terminal_print("curl_easy_init failed");
        return 1;
    }
    terminal_print("curl: easy_init ok");

    /* Use libcurl's own connect path (no manual pre-connect). */

    /* 3. libcurl konfigurieren (Lass libcurl alles alleine machen!) */
    terminal_print("curl: setopt URL");
    curl_easy_setopt(easy, CURLOPT_URL, "http://192.168.15.152:8000/");
    {
        struct curl_slist *hdrs = NULL;
        hdrs = curl_slist_append(hdrs, "X-Fallback: 0");
        curl_easy_setopt(easy, CURLOPT_HTTPHEADER, hdrs);
    }
    terminal_print("curl: setopt WRITEFUNCTION");
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_cb);
    terminal_print("curl: setopt WRITEDATA");
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &body);
    terminal_print("curl: setopt ERRORBUFFER");
    curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, errorbuf);

    /* Debug-Ausgaben aktivieren */
    terminal_print("curl: setopt VERBOSE/DEBUG");
    curl_easy_setopt(easy, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(easy, CURLOPT_DEBUGFUNCTION, debug_cb);
    curl_easy_setopt(easy, CURLOPT_DEBUGDATA, &dbg);
    /* Avoid signal() usage in libcurl on Wii */
    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);

    /* Timeout etwas höher setzen für die Wii */
    terminal_print("curl: setopt CONNECTTIMEOUT");
    curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, 10L);

    terminal_print("Sende HTTP GET via libcurl...");
    
    /* 4. Die Anfrage ausführen */
    terminal_print("curl: perform...");
    cc = curl_easy_perform(easy);
    terminal_print("curl: perform done");

    /* Debug-Auswertung */
    terminal_print("curl: getinfo RESPONSE_CODE");
    curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &status);
    {
        char line[128];
        snprintf(line, sizeof(line), "curl rc=%d (%s)", (int)cc, curl_easy_strerror(cc));
        terminal_print(line);
        if (errorbuf[0]) {
            terminal_print("curl error buffer:");
            terminal_print_long(errorbuf);
        }
        snprintf(line, sizeof(line), "HTTP status: %ld", status);
        terminal_print(line);
        snprintf(line, sizeof(line), "body length: %u", (unsigned)(body.len));
        terminal_print(line);
    }

    if (dbg.data && dbg.len > 0) {
        terminal_print("--- libcurl verbose output ---");
        terminal_print_long(dbg.data);
        terminal_print("--- end verbose ---");
    }

    {
        char line[128];
        snprintf(line, sizeof(line), "opensocket_cb called: %d", g_open_cb_called);
        terminal_print(line);
        snprintf(line, sizeof(line), "sockopt_cb called: %d", g_sockopt_cb_called);
        terminal_print(line);
    }

    if (cc == CURLE_COULDNT_CONNECT) {
        terminal_print("curl failed, trying raw net_* fallback...");
        raw_http_get_fallback();
    }

    if (body.data && body.len > 0) {
        terminal_print("--- body (first 1024 bytes) ---");
        /* print at most 1024 chars */
        size_t limit = body.len < 1024 ? body.len : 1024;
        char *tmp = (char*)malloc(limit + 1);
        if (tmp) {
            memcpy(tmp, body.data, limit);
            tmp[limit] = '\0';
            terminal_print_long(tmp);
            free(tmp);
        }
        terminal_print("--- end body ---");
    }

    terminal_print("curl: cleanup");
    if (easy) curl_easy_cleanup(easy);
    curl_global_cleanup();
    buf_free(&body);
    buf_free(&dbg);
    terminal_print("curl_test: done");
    return 0;
}

#ifdef ajskl
int main(void) {
    CURL *easy = NULL;
    CURLcode cc = CURLE_OK;
    buf_t body, dbg;
    long status = 0;
    char errorbuf[CURL_ERROR_SIZE];

    video_init_custom();
    terminal_clear();
    terminal_print("curl_test: start");

    /* initialize Wii networking (libogc net_*) */
    if (net_init() < 0) {
        terminal_print("net_init failed");
        return 1;
    }
    terminal_print("net_init ok");

    buf_init(&body);
    buf_init(&dbg);
    memset(errorbuf, 0, sizeof(errorbuf));

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        terminal_print("curl_global_init failed");
        return 1;
    }

    easy = curl_easy_init();
    if (!easy) {
        terminal_print("curl_easy_init failed");
        curl_global_cleanup();
        return 1;
    }

    /* Diagnostic: create and pre-connect a socket, then give it to libcurl
       via CURLOPT_OPENSOCKETFUNCTION. If libcurl succeeds using that
       pre-connected socket, the issue is likely libcurl's non-blocking
       connect / poll / getsockopt interaction with the Wii compatibility
       layer. */
    {
        static int pre_sock = -1;
        struct sockaddr_in server;
        pre_sock = net_socket(AF_INET, SOCK_STREAM, 0);
        if (pre_sock < 0) {
            terminal_print("pre-socket create failed");
        } else {
            memset(&server, 0, sizeof(server));
            server.sin_family = AF_INET;
            server.sin_port = htons(8000);
            IP4_ADDR(&server.sin_addr, 192,168,15,152);
            if (net_connect(pre_sock, (struct sockaddr *)&server, sizeof(server)) != 0) {
                terminal_print("preconnect failed");
                net_close(pre_sock);
                pre_sock = -1;
            } else {
                char okmsg[128];
                snprintf(okmsg, sizeof(okmsg), "preconnected socket: %d", pre_sock);
                terminal_print(okmsg);

                /* Store the preconnected socket in the global and register the
                   file-scope opensocket callback so libcurl will receive it. */
                g_pre_sock = pre_sock;
                curl_easy_setopt(easy, CURLOPT_OPENSOCKETFUNCTION, opensocket_cb);
            }
        }
    }

    curl_easy_setopt(easy, CURLOPT_URL, "http://192.168.15.152:8000/");
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, errorbuf);

    /* verbose + debug callback */
    curl_easy_setopt(easy, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(easy, CURLOPT_DEBUGFUNCTION, debug_cb);
    curl_easy_setopt(easy, CURLOPT_DEBUGDATA, &dbg);

    /* short timeout to keep it snappy on test devices */
    curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, 5000L);

    terminal_print("curl: performing GET http://192.168.15.152:8000/");
    cc = curl_easy_perform(easy);

    curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &status);
    /* Grab OS errno and primary IP (if available) for better diagnostics */
    long os_err = 0;
    const char *primary_ip = NULL;
    if (curl_easy_getinfo(easy, CURLINFO_OS_ERRNO, &os_err) != CURLE_OK) {
        os_err = 0;
    }
    if (curl_easy_getinfo(easy, CURLINFO_PRIMARY_IP, &primary_ip) != CURLE_OK) {
        primary_ip = NULL;
    }

    /* Print summary */
    {
        char line[256];
        snprintf(line, sizeof(line), "curl rc=%d (%s)", (int)cc, curl_easy_strerror(cc));
        terminal_print(line);
        if (errorbuf[0]) {
            terminal_print("curl error buffer:");
            terminal_print_long(errorbuf);
        }
        snprintf(line, sizeof(line), "HTTP status: %ld", status);
        terminal_print(line);

        snprintf(line, sizeof(line), "body length: %u", (unsigned)(body.len));
        terminal_print(line);

        if (os_err != 0) {
            char errline[128];
            snprintf(errline, sizeof(errline), "libcurl OS errno: %ld (%s)", os_err, strerror((int)os_err));
            terminal_print(errline);
        }
        if (primary_ip && primary_ip[0]) {
            char ipline[128];
            snprintf(ipline, sizeof(ipline), "libcurl primary IP: %s", primary_ip);
            terminal_print(ipline);
        }
    }

    if (dbg.data && dbg.len > 0) {
        terminal_print("--- libcurl verbose output ---");
        terminal_print_long(dbg.data);
        terminal_print("--- end verbose ---");
    }

    if (body.data && body.len > 0) {
        terminal_print("--- body (first 1024 bytes) ---");
        /* print at most 1024 chars */
        size_t limit = body.len < 1024 ? body.len : 1024;
        char *tmp = (char*)malloc(limit + 1);
        if (tmp) {
            memcpy(tmp, body.data, limit);
            tmp[limit] = '\0';
            terminal_print_long(tmp);
            free(tmp);
        }
        terminal_print("--- end body ---");
    }

    /* cleanup */
    if (easy) curl_easy_cleanup(easy);
    curl_global_cleanup();
    buf_free(&body);
    buf_free(&dbg);

    terminal_print("curl_test: done");
    return 0;
}
#endif
