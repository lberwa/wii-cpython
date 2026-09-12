#include "my_text_renderer.h"
#include "bitmap_colors.h"
#include "font8x8_basic.h"

#include <gccore.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Terminal dimensions                                                  */
/* ------------------------------------------------------------------ */
#define TERM_COLS     36   /* chars per row; each draw_pixel unit = 2 screen px
                              (YUYV stride = fbWidth/2 = 320), x_start=10,
                              max = (320-10)/8 = 38; 36 leaves a right margin */
#define TERM_ROWS     30   /* scrollback lines                          */
#define TERM_VISIBLE  25   /* visible rows on screen                    */
#define CHAR_W         8
#define CHAR_H         8

/* ------------------------------------------------------------------ */
/* Per-character cell: text + foreground color                         */
/* ------------------------------------------------------------------ */
typedef struct { char ch; uint32_t color; } TChar;

/* ------------------------------------------------------------------ */
/* Terminal state                                                       */
/* ------------------------------------------------------------------ */
static TChar term_buf[TERM_ROWS][TERM_COLS + 1];
static int   term_len[TERM_ROWS];
static int   total_lines = 0;

/* Current (incomplete) line.  cur_col = write cursor (for \r).
   cur_len = highest position written; cur_len >= cur_col always.     */
static TChar cur_buf[TERM_COLS + 1];
static int   cur_len = 0;
static int   cur_col = 0;

static int   term_dirty = 0;
static bool  autoscroll  = true;

/* ------------------------------------------------------------------ */
/* Video state                                                          */
/* ------------------------------------------------------------------ */
GXRModeObj *rmode       = NULL;
void       *framebuffer = NULL;
int         video_init_done = 0;

static mutex_t g_term_mutex        = 0;
static bool    g_term_mutex_inited = false;
#define TERM_LOCK()   do { if (g_term_mutex_inited) LWP_MutexLock(g_term_mutex);   } while(0)
#define TERM_UNLOCK() do { if (g_term_mutex_inited) LWP_MutexUnlock(g_term_mutex); } while(0)

/* ------------------------------------------------------------------ */
/* ANSI 16-color palette — YUYV packed format:                         */
/*   bits 31-24: Y0  bits 23-16: Cb  bits 15-8: Y1  bits 7-0: Cr     */
/* (Y0==Y1 for solid single-color pixels)                              */
/* ------------------------------------------------------------------ */
#define YUYV(y, cb, cr) \
    (((uint32_t)(y) << 24) | ((uint32_t)(cb) << 16) | \
     ((uint32_t)(y) <<  8) |  (uint32_t)(cr))

/* BT.601 video-range conversions of the classic VGA 16-color palette */
static const uint32_t ansi_pal[16] = {
    YUYV( 16, 128, 128),  /*  0 Black        #000000 */
    YUYV( 60, 103, 203),  /*  1 Red          #AA0000 */
    YUYV(102,  79,  65),  /*  2 Green        #00AA00 */
    YUYV(102,  78, 171),  /*  3 Dark Yellow  #AA5500 */
    YUYV( 33, 203, 116),  /*  4 Blue         #0000AA */
    YUYV( 76, 177, 191),  /*  5 Magenta      #AA00AA */
    YUYV(118, 153,  53),  /*  6 Cyan         #00AAAA */
    YUYV(162, 128, 128),  /*  7 Silver       #AAAAAA */
    YUYV( 89, 128, 128),  /*  8 Dark Gray    #555555 */
    YUYV(133, 103, 203),  /*  9 Bright Red   #FF5555 */
    YUYV(175,  79,  65),  /* 10 Bright Green #55FF55 */
    YUYV(218,  53, 140),  /* 11 Bright Yellow#FFFF55 */
    YUYV(106, 203, 116),  /* 12 Bright Blue  #5555FF */
    YUYV(149, 177, 191),  /* 13 Br. Magenta  #FF55FF */
    YUYV(191, 153,  53),  /* 14 Bright Cyan  #55FFFF */
    YUYV(235, 128, 128),  /* 15 White        #FFFFFF */
};

#define DEFAULT_FG  ansi_pal[15]   /* White — matches old "white" color */

/* ------------------------------------------------------------------ */
/* Current text attributes                                             */
/* ------------------------------------------------------------------ */
static uint32_t cur_fg   = YUYV(235, 128, 128); /* default: white     */
static int      cur_bold = 0;

/* ------------------------------------------------------------------ */
/* ANSI CSI escape-sequence parser state                               */
/* ------------------------------------------------------------------ */
typedef enum { ANSI_NORM, ANSI_ESC, ANSI_CSI } AnsiState;
static AnsiState ansi_state = ANSI_NORM;
static int       ansi_par[8];   /* accumulated numeric parameters      */
static int       ansi_npar = 0;
static int       ansi_cur  = 0; /* parameter being built digit by digit */

/* ================================================================== */
/* Pixel / glyph drawing                                               */
/* ================================================================== */

GXRModeObj *get_rmode(void)     { return rmode;       }
void       *get_framebuffer(void) { return framebuffer; }

static void draw_pixel(uint32_t *fb, int fb_width, int x, int y, uint32_t color)
{
    GXRModeObj *rm = get_rmode();
    if (x >= 0 && y >= 0 && x < fb_width && y < rm->xfbHeight)
        fb[y * fb_width + x] = color;
}

static void draw_char(uint32_t *fb, int fb_width, int x, int y,
                      char c, uint32_t color)
{
    unsigned int idx = (unsigned char)c;
    if (idx > 127) return;
    const char *glyph = font8x8_basic[idx];
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
            if ((unsigned char)glyph[row] & (1u << col))
                draw_pixel(fb, fb_width, x + col, y + row, color);
}

static void draw_string(uint32_t *fb, int fb_width, int x, int y,
                        const char *text, uint32_t color)
{
    while (*text) {
        draw_char(fb, fb_width, x, y, *text, color);
        x += CHAR_W;
        text++;
    }
}

/* Draw a TChar row (with per-character colors).                       */
static void draw_tchar_row(const TChar *row, int len, int x, int y)
{
    uint32_t *fb   = (uint32_t *)get_framebuffer();
    int       fbw  = get_rmode()->fbWidth;
    for (int i = 0; i < len; i++, x += CHAR_W) {
        unsigned char c = (unsigned char)row[i].ch;
        if (c >= 0x20 && c < 0x7F)
            draw_char(fb, fbw, x, y, (char)c, row[i].color);
        else if (c == '\t') /* rendered as spaces — already substituted */
            draw_char(fb, fbw, x, y, ' ', row[i].color);
    }
}

/* ================================================================== */
/* Video initialisation                                                */
/* ================================================================== */

void video_init_custom(void)
{
    if (!video_init_done) {
        VIDEO_Init();
        LWP_MutexInit(&g_term_mutex, true); /* true = reentrant */
        g_term_mutex_inited = true;
        video_init_done = 1;
    } else {
        VIDEO_SetPreRetraceCallback(NULL);
        VIDEO_SetPostRetraceCallback(NULL);
    }

    rmode       = VIDEO_GetPreferredMode(NULL);
    void *raw   = SYS_AllocateFramebuffer(rmode);
    if (!raw) return;
    framebuffer = MEM_K0_TO_K1(raw);

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(framebuffer);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

/* ================================================================== */
/* Public drawing utilities (non-terminal)                            */
/* ================================================================== */

void render_text(const char *text, int x, int y, const char *color_name)
{
    uint32_t color = get_color_by_name(color_name);
    draw_string((uint32_t *)get_framebuffer(), get_rmode()->fbWidth,
                x, y, text, color);
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

void render_text_ohne_bild(const char *text, int x, int y, const char *color_name)
{
    uint32_t color = get_color_by_name(color_name);
    draw_string((uint32_t *)get_framebuffer(), get_rmode()->fbWidth,
                x, y, text, color);
}

static void safe_clear_framebuffer(void *fb, GXRModeObj *rm, uint32_t color)
{
    if (!fb || !rm) return;
    uint32_t *p     = (uint32_t *)fb;
    uint32_t  words = (VIDEO_PadFramebufferWidth(rm->fbWidth)
                       * rm->xfbHeight * VI_DISPLAY_PIX_SZ) >> 2;
    for (uint32_t i = 0; i < words; i++) p[i] = color;
}

void clear_screen(const char *color_name)
{
    safe_clear_framebuffer(framebuffer, rmode, get_color_by_name(color_name));
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

void clear_screen_ohne_bild(const char *color_name)
{
    safe_clear_framebuffer(framebuffer, rmode, get_color_by_name(color_name));
}

void render_bild(void)
{
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

/* ================================================================== */
/* Terminal rendering                                                  */
/* ================================================================== */

void terminal_render(void)
{
    TERM_LOCK();
    if (!framebuffer) { TERM_UNLOCK(); return; }
    clear_screen_ohne_bild("black");

    int extra      = (cur_len > 0) ? 1 : 0;
    int total_shown = total_lines + extra;
    int start      = (total_shown > TERM_VISIBLE) ? total_shown - TERM_VISIBLE : 0;

    int row = 0;
    for (int i = start; i < total_lines; i++, row++)
        draw_tchar_row(term_buf[i], term_len[i], 10, 20 + row * CHAR_H);
    if (extra)
        draw_tchar_row(cur_buf, cur_len, 10, 20 + row * CHAR_H);

    VIDEO_Flush();
    term_dirty = 0;
    TERM_UNLOCK();
}

void terminal_flush(void)
{
    TERM_LOCK();
    if (term_dirty) terminal_render();
    TERM_UNLOCK();
}

/* ================================================================== */
/* Internal terminal buffer operations (caller holds TERM_LOCK)        */
/* ================================================================== */

static void terminal_add_line_locked(const TChar *line, int len)
{
    if (total_lines < TERM_ROWS) {
        memcpy(term_buf[total_lines], line, sizeof(TChar) * (len + 1));
        term_len[total_lines] = len;
        total_lines++;
    } else {
        /* Scroll: shift all lines up by one. */
        for (int i = 1; i < TERM_ROWS; i++) {
            memcpy(term_buf[i - 1], term_buf[i],
                   sizeof(TChar) * (term_len[i] + 1));
            term_len[i - 1] = term_len[i];
        }
        memcpy(term_buf[TERM_ROWS - 1], line, sizeof(TChar) * (len + 1));
        term_len[TERM_ROWS - 1] = len;
    }
    term_dirty = 1;
}

/* Commit the current partial line and reset it.                       */
static void terminal_commit_cur(void)
{
    cur_buf[cur_len].ch    = '\0';
    cur_buf[cur_len].color = 0;
    terminal_add_line_locked(cur_buf, cur_len);
    cur_len = 0;
    cur_col = 0;
    memset(cur_buf, 0, sizeof(cur_buf));
}

/* Write one character to cur_buf at cur_col, advance cursor.         */
static void terminal_put(char c, uint32_t color)
{
    if (cur_col >= TERM_COLS) terminal_commit_cur();
    cur_buf[cur_col].ch    = c;
    cur_buf[cur_col].color = color;
    cur_col++;
    if (cur_col > cur_len) cur_len = cur_col;
}

static void terminal_clear_locked(void)
{
    for (int i = 0; i < TERM_ROWS; i++) {
        memset(term_buf[i], 0, sizeof(term_buf[i]));
        term_len[i] = 0;
    }
    total_lines = 0;
    cur_len     = 0;
    cur_col     = 0;
    memset(cur_buf, 0, sizeof(cur_buf));
    term_dirty  = 1;
    if (framebuffer) terminal_render();
}

void terminal_clear(void)
{
    TERM_LOCK();
    terminal_clear_locked();
    TERM_UNLOCK();
}

/* ================================================================== */
/* ANSI SGR (Set Graphic Rendition)                                    */
/* ================================================================== */

static void handle_sgr(const int *par, int npar)
{
    if (npar == 0) {              /* \033[m  — full reset */
        cur_fg   = DEFAULT_FG;
        cur_bold = 0;
        return;
    }
    for (int p = 0; p < npar; p++) {
        int code = par[p];
        if (code == 0) {
            cur_fg   = DEFAULT_FG;
            cur_bold = 0;
        } else if (code == 1) {   /* bold — switch to bright variant    */
            cur_bold = 1;
        } else if (code == 2 || code == 22) { /* faint / normal        */
            cur_bold = 0;
        } else if (code >= 30 && code <= 37) {
            int idx = code - 30;
            cur_fg  = ansi_pal[cur_bold ? idx + 8 : idx];
        } else if (code >= 90 && code <= 97) {
            cur_fg  = ansi_pal[code - 90 + 8];
        }
        /* Background (40-47, 100-107): stored but terminal bg is black,
           so we do not render it separately.                           */
    }
}

/* ================================================================== */
/* ANSI CSI dispatcher (called when a letter terminates the sequence)  */
/* ================================================================== */

static void handle_csi(char cmd, const int *par, int npar)
{
    int p0 = (npar > 0) ? par[0] : 0;
    int p1 = (npar > 1) ? par[1] : 0;

    switch (cmd) {
    case 'm':  /* SGR */
        handle_sgr(par, npar);
        break;

    case 'J':  /* Erase display */
        if (p0 == 2 || npar == 0)
            terminal_clear_locked();
        break;

    case 'K':  /* Erase line */
        if (p0 == 0) {          /* erase from cursor to end of line    */
            for (int i = cur_col; i < cur_len; i++) {
                cur_buf[i].ch    = ' ';
                cur_buf[i].color = cur_fg;
            }
            cur_len = cur_col;
        } else if (p0 == 1) {   /* erase from beginning to cursor      */
            for (int i = 0; i < cur_col; i++) {
                cur_buf[i].ch    = ' ';
                cur_buf[i].color = cur_fg;
            }
        } else if (p0 == 2) {   /* erase entire line                   */
            cur_len = 0;
            cur_col = 0;
            memset(cur_buf, 0, sizeof(cur_buf));
        }
        break;

    case 'H':  /* Cursor position  \033[row;colH  (1-based, default 1) */
    case 'f':  /* same as H                                             */
        if (p0 == 0 && p1 == 0) {
            /* \033[H = home: go to first row, first col.
               Commit cur line so we start fresh at the bottom.        */
            if (cur_len > 0) terminal_commit_cur();
        } else {
            /* Row navigation is complex in a scrolling terminal.
               Only apply column part for now.                         */
            int col = (p1 > 0 ? p1 : 1) - 1;
            if (col < 0) col = 0;
            if (col > cur_len) col = cur_len;
            cur_col = col;
        }
        break;

    case 'A':  /* Cursor up   — row navigation: commit cur line */
        if (cur_len > 0) terminal_commit_cur();
        break;

    case 'B':  /* Cursor down */
        if (cur_len > 0) terminal_commit_cur();
        break;

    case 'C':  /* Cursor forward (right) */
        { int n = (p0 > 0) ? p0 : 1;
          cur_col += n;
          if (cur_col > cur_len) cur_col = cur_len; }
        break;

    case 'D':  /* Cursor back (left) */
        { int n = (p0 > 0) ? p0 : 1;
          cur_col -= n;
          if (cur_col < 0) cur_col = 0; }
        break;

    case 's':  /* Save cursor position (simplified: no-op) */
    case 'u':  /* Restore cursor position (simplified: no-op) */
        break;

    default:
        break;
    }
}

/* ================================================================== */
/* Core feed routine — handles all escape sequences and control chars  */
/* ================================================================== */

static void terminal_feed(const char *text, int len, int flush_partial)
{
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];

        /* --- ANSI escape-sequence state machine --- */
        if (ansi_state == ANSI_ESC) {
            if (c == '[') {
                ansi_state = ANSI_CSI;
                ansi_npar  = 0;
                ansi_cur   = 0;
                memset(ansi_par, 0, sizeof(ansi_par));
            } else {
                ansi_state = ANSI_NORM; /* unknown ESC seq: ignore      */
            }
            continue;
        }
        if (ansi_state == ANSI_CSI) {
            if (c >= '0' && c <= '9') {
                ansi_cur = ansi_cur * 10 + (c - '0');
            } else if (c == ';') {
                if (ansi_npar < 8) ansi_par[ansi_npar++] = ansi_cur;
                ansi_cur = 0;
            } else {
                if (ansi_npar < 8) ansi_par[ansi_npar++] = ansi_cur;
                handle_csi((char)c, ansi_par, ansi_npar);
                ansi_state = ANSI_NORM;
                ansi_npar  = 0;
                ansi_cur   = 0;
            }
            continue;
        }

        /* --- Normal character processing --- */
        switch (c) {
        case '\033':  /* ESC — start of escape sequence                */
            ansi_state = ANSI_ESC;
            break;

        case '\n':    /* Newline: commit current line                   */
            terminal_commit_cur();
            break;

        case '\r':    /* Carriage return: move write cursor to col 0   */
            cur_col = 0;
            break;

        case '\b':    /* Backspace: move write cursor back one          */
            if (cur_col > 0) cur_col--;
            break;

        case '\t':    /* Horizontal tab: advance to next 8-col stop    */
            { int next = ((cur_col / 8) + 1) * 8;
              if (next > TERM_COLS) next = TERM_COLS;
              while (cur_col < next) terminal_put(' ', cur_fg); }
            break;

        case '\a':    /* Bell: ignore (no speaker support)             */
            break;

        case '\f':    /* Form feed: clear screen                        */
            terminal_clear_locked();
            break;

        case '\v':    /* Vertical tab: treat as newline                 */
            terminal_commit_cur();
            break;

        default:
            if (c >= 0x20 && c != 0x7F)  /* printable ASCII            */
                terminal_put((char)c, cur_fg);
            break;
        }
    }

    if (flush_partial && cur_len > 0)
        terminal_commit_cur();

    term_dirty = 1;
    if (autoscroll)
        terminal_render();
}

/* ================================================================== */
/* Public terminal API                                                 */
/* ================================================================== */

void terminal_write(const char *text, int len)
{
    if (!text || len <= 0) return;
    TERM_LOCK();
    terminal_feed(text, len, 0);
    TERM_UNLOCK();
}

/* For C callers: always commits any trailing partial line.            */
void terminal_print(const char *text)
{
    if (!text) return;
    TERM_LOCK();
    terminal_feed(text, (int)strlen(text), 1);
    TERM_UNLOCK();
}

void terminal_set_autoscroll(bool enabled)
{
    autoscroll = enabled;
}
