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


static char log_buffer[MAX_LINES][MAX_LINE_LENGTH];
static int line_count = 0;
static bool autoscroll = true;


static int current_line = 0;
static int current_col = 0;


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

void video_init_custom() {
    VIDEO_Init();
    
    rmode = VIDEO_GetPreferredMode(NULL);
    framebuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    
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


void clear_screen(const char *color_name) {
    VIDEO_ClearFrameBuffer(rmode, framebuffer, get_color_by_name(color_name));
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

void clear_screen_ohne_bild(const char *color_name) {
    VIDEO_ClearFrameBuffer(rmode, framebuffer, get_color_by_name(color_name));
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
void terminal_render(void) {
    clear_screen("black");

    int start = total_lines > TERM_VISIBLE_LINES ? total_lines - TERM_VISIBLE_LINES : 0;
    for (int i = start; i < total_lines; i++) {
        render_text_ohne_bild(term_buffer[i], 10, 20 + (i - start) * CHAR_HEIGHT, "white");
    }

    VIDEO_Flush();
    VIDEO_WaitVSync();
    
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

    if (autoscroll)
        terminal_render();
}

void terminal_clear(void) {
    for (int i = 0; i < TERM_MAX_LINES; i++)
        term_buffer[i][0] = '\0';
    total_lines = 0;
    clear_screen("black");
}

void terminal_set_autoscroll(bool enabled) {
    autoscroll = enabled;
}

//static bool first = true;
/*
void nochmal(const char *line) {
	terminal_print("hallo akljhklsfhaklfh + AAAAAAAA");
}
*/
void terminal_print(const char *text) {
    size_t len = strlen(text);
    size_t pos = 0;
    
    //bool first = true;
    
    while (pos < len) {
    
  //  	if (first) {
        	
        	char line[TERM_MAX_WIDTH + 1];
        	size_t chunk = (len - pos > TERM_MAX_WIDTH) ? TERM_MAX_WIDTH : len - pos;
		
	        strncpy(line, &text[pos], chunk);
	        line[chunk] = '\0';
	        
	        
	        /*
    		char line[TERM_MAX_WIDTH + 1];

    		// Kopiere nur die ersten TERM_MAX_WIDTH Zeichen
    		strncpy(line, text, TERM_MAX_WIDTH);
    		line[TERM_MAX_WIDTH] = '\0';  
*/

	        terminal_add_line(line);
		//terminal_print(line);
		
	        pos += chunk;
	        
	/*        first = false;
        
        } else {
        	char line[TERM_MAX_WIDTH + 1];
        	size_t chunk = (len - pos > TERM_MAX_WIDTH) ? TERM_MAX_WIDTH : len - pos;
		
	        strncpy(line, &text[pos], chunk);
	        line[chunk] = '\0';
	        //terminal_add_line(line);
		terminal_print(line);
		
	        pos += chunk;
	}*/	
    }
}

