#ifndef MY_TEXT_RENDERER_H
#define MY_TEXT_RENDERER_H

#include <stdint.h>
#include <gccore.h>  // unbedingt hier für GXRModeObj

// my_text_renderer.h
extern GXRModeObj* rmode;
extern void* framebuffer;


// Video initialisieren (einmal vor Textzeichnen aufrufen)
void video_init_custom(void);

// Getter für main.c Variablen
void* get_framebuffer(void);
GXRModeObj* get_rmode(void);

// Text zeichnen an (x,y) mit Farbnamen ("red", "green", "white" etc.)
void render_text(const char *text, int x, int y, const char *color_name);

// Bildschirm löschen
void clear_screen(const char *color_name);

void render_text_ohne_bild(const char *text, int x, int y, const char *color_name);
void clear_screen_ohne_bild(const char *color_name);
void render_bild(void);


// Initialisiert den Textpuffer (optional, falls du willst)
void terminal_clear(void);

// Fügt eine neue Zeile hinzu (automatisches Scrollen, wenn nötig)
void terminal_print(const char *text);

// Zeichnet den kompletten Inhalt neu auf den Bildschirm
void terminal_render(void);

// Aktiviert oder deaktiviert automatisches Scrollen
void terminal_set_autoscroll(bool enabled);

#endif

