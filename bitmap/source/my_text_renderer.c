#include "my_text_renderer.h"
#include "bitmap_colors.h"
#include "font8x8_basic.h"

#include <gccore.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define TERM_MAX_LINES 30       // wie viele Zeilen sichtbar sind
#define TERM_MAX_WIDTH 30       // max Zeichen pro Zeile
#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8
#define MAX_LINES 30
#define MAX_LINE_LENGTH 80
#define TERM_VISIBLE_LINES 25  // Anzahl der auf dem Bildschirm sichtbaren Zeilen

static char term_buffer[TERM_MAX_LINES][TERM_MAX_WIDTH + 1];
static int total_lines = 0;  // Anzahl aktuell gespeicherter Zeilen

// Offene (noch nicht mit '\n' abgeschlossene) aktuelle Zeile für terminal_write().
static char cur_line[TERM_MAX_WIDTH + 1];
static int  cur_len = 0;
static int  term_dirty = 0;  // 1 = seit letztem Render veraendert


static char log_buffer[MAX_LINES][MAX_LINE_LENGTH];
static int line_count = 0;
static bool autoscroll = true;


static int current_line = 0;
static int current_col = 0;

/* Reentrant mutex — schützt alle Terminal-Zustandsvariablen und VIDEO_Flush.
   Reentrant weil terminal_feed() → terminal_render() beide unter demselben Lock laufen. */
static mutex_t g_term_mutex = 0;
static bool g_term_mutex_inited = false;
#define TERM_LOCK()   do { if (g_term_mutex_inited) LWP_MutexLock(g_term_mutex); } while(0)
#define TERM_UNLOCK() do { if (g_term_mutex_inited) LWP_MutexUnlock(g_term_mutex); } while(0)

// globale Variablen
GXRModeObj* rmode;
void* framebuffer;

GXRModeObj* get_rmode(void) { return rmode; }
void* get_framebuffer(void) { return framebuffer; }



static void draw_pixel(uint32_t *fb, int fb_width, int x, int y, uint32_t color) {
    GXRModeObj* rmode = get_rmode(); // Zugriff über Getter
    if (x >= 0 && y >= 0 && x < fb_width && y < rmode->xfbHeight) {
        fb[y * fb_width + x] = color;
    }
}

static void draw_char(uint32_t *fb, int fb_width, int x, int y, char c, uint32_t color) {
    if (c < 0 || c > 127) return;
    const uint8_t *glyph = font8x8_basic[(int)c];
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (glyph[row] & (1 << col)) {
                draw_pixel(fb, fb_width, x + col, y + row, color);
            }
        }
    }
}

static void draw_string(uint32_t *fb, int fb_width, int x, int y, const char *text, uint32_t color) {
    while (*text) {
        draw_char(fb, fb_width, x, y, *text, color);
        x += 8;
        text++;
    }
}


//custom variablen definieren
GXRModeObj* rmode;
void* framebuffer;
int video_init_done = 0;

void video_init_custom() {
    if (!video_init_done) {
        VIDEO_Init();
        LWP_MutexInit(&g_term_mutex, true);   // true = reentrant
        g_term_mutex_inited = true;
        video_init_done = 1;
    } else {
        /* After rendering_init() the retrace callback is still registered.
           Deregister it before reconfiguring video to avoid a crash when
           copy_buffers() fires on the next WaitVSync(). */
        VIDEO_SetPreRetraceCallback(NULL);
        VIDEO_SetPostRetraceCallback(NULL);
    }

    rmode = VIDEO_GetPreferredMode(NULL);
    void *raw = SYS_AllocateFramebuffer(rmode);
    if (raw == NULL) return;
    framebuffer = MEM_K0_TO_K1(raw);

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(framebuffer);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
}


void render_text(const char *text, int x, int y, const char *color_name) {
    uint32_t color = get_color_by_name(color_name);
    draw_string((uint32_t *)get_framebuffer(), get_rmode()->fbWidth, x, y, text, color);
    VIDEO_Flush();
    VIDEO_WaitVSync();
}


static void safe_clear_framebuffer(void *fb, GXRModeObj *rm, uint32_t color) {
    if (fb == NULL || rm == NULL) return;
    /* Direct CPU fill — avoids VIDEO DMA which conflicts with GX state */
    uint32_t *p = (uint32_t *)fb;
    uint32_t words = (VIDEO_PadFramebufferWidth(rm->fbWidth) * rm->xfbHeight * VI_DISPLAY_PIX_SZ) >> 2;
    for (uint32_t i = 0; i < words; i++) p[i] = color;
}

void clear_screen(const char *color_name) {
    safe_clear_framebuffer(framebuffer, rmode, get_color_by_name(color_name));
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

void clear_screen_ohne_bild(const char *color_name) {
    safe_clear_framebuffer(framebuffer, rmode, get_color_by_name(color_name));
}

void render_text_ohne_bild(const char *text, int x, int y, const char *color_name) {
    uint32_t color = get_color_by_name(color_name);
    draw_string((uint32_t *)get_framebuffer(), get_rmode()->fbWidth, x, y, text, color);
}

void render_bild() {
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

//terminalfunktionen

// Zeichnet den Terminalinhalt neu. WICHTIG: KEIN VIDEO_WaitVSync() mehr —
// wir zeichnen direkt in den sichtbaren Framebuffer, der ohnehin jeden Frame
// ausgegeben wird. Das war der Flaschenhals (2 VSyncs pro Zeile ~ 33 ms).
void terminal_render(void) {
    TERM_LOCK();
    if (framebuffer == NULL) { TERM_UNLOCK(); return; }
    clear_screen_ohne_bild("black");

    int extra = (cur_len > 0) ? 1 : 0;      // offene Teilzeile mitzeichnen
    int total_shown = total_lines + extra;
    int start = total_shown > TERM_VISIBLE_LINES ? total_shown - TERM_VISIBLE_LINES : 0;

    int row = 0;
    for (int i = start; i < total_lines; i++, row++)
        render_text_ohne_bild(term_buffer[i], 10, 20 + row * CHAR_HEIGHT, "white");
    if (extra)
        render_text_ohne_bild(cur_line, 10, 20 + row * CHAR_HEIGHT, "white");

    VIDEO_Flush();          // nur Flush, kein WaitVSync
    term_dirty = 0;
    TERM_UNLOCK();
}

void terminal_flush(void) {
    TERM_LOCK();
    if (term_dirty)
        terminal_render();
    TERM_UNLOCK();
}

static void terminal_add_line(const char *line) {
    if (total_lines < TERM_MAX_LINES) {
        strncpy(term_buffer[total_lines], line, TERM_MAX_WIDTH);
        term_buffer[total_lines][TERM_MAX_WIDTH] = '\0';
        total_lines++;
    } else {
        // Scrollen: alle Zeilen eine nach oben verschieben
        for (int i = 1; i < TERM_MAX_LINES; i++)
            strcpy(term_buffer[i - 1], term_buffer[i]);
        strncpy(term_buffer[TERM_MAX_LINES - 1], line, TERM_MAX_WIDTH);
        term_buffer[TERM_MAX_LINES - 1][TERM_MAX_WIDTH] = '\0';
    }
    term_dirty = 1;
    // Rendern übernimmt der Aufrufer (terminal_feed) — nicht mehr pro Zeile.
}

// Commit der offenen Teilzeile als eigene Terminalzeile (auch wenn leer).
static void terminal_commit_cur(void) {
    cur_line[cur_len] = '\0';
    terminal_add_line(cur_line);
    cur_len = 0;
    cur_line[0] = '\0';
}

// Kernroutine: Bytes anhängen, nur bei echtem '\n' umbrechen; bei Überlänge
// automatisch weiterbrechen. flush_partial=1 committet am Ende einen Rest.
static void terminal_feed(const char *text, int len, int flush_partial) {
    for (int i = 0; i < len; i++) {
        char c = text[i];
        if (c == '\n') {
            terminal_commit_cur();
        } else if (c == '\r') {
            /* ignorieren */
        } else {
            if (c == '\t') c = ' ';
            if (cur_len >= TERM_MAX_WIDTH)
                terminal_commit_cur();          // automatischer Zeilenumbruch
            cur_line[cur_len++] = c;
        }
    }
    if (flush_partial && cur_len > 0)
        terminal_commit_cur();
    term_dirty = 1;
    if (autoscroll)
        terminal_render();                      // einmal pro Aufruf, ohne VSync
}

void terminal_write(const char *text, int len) {
    if (text == NULL || len <= 0) return;
    TERM_LOCK();
    terminal_feed(text, len, 0);
    TERM_UNLOCK();
}

void terminal_clear(void) {
    TERM_LOCK();
    for (int i = 0; i < TERM_MAX_LINES; i++)
        term_buffer[i][0] = '\0';
    total_lines = 0;
    cur_len = 0;
    cur_line[0] = '\0';
    term_dirty = 1;
    if (framebuffer != NULL)
        terminal_render();
    TERM_UNLOCK();
}

void terminal_set_autoscroll(bool enabled) {
    autoscroll = enabled;
}

// 1 Aufruf = 1 abgeschlossene Ausgabe (committet einen evtl. Rest). Für C-Code,
// das ganze Meldungen übergibt (z.B. terminal_print("start")).
void terminal_print(const char *text) {
    if (text == NULL) return;
    TERM_LOCK();
    terminal_feed(text, (int)strlen(text), 1);
    TERM_UNLOCK();
}

