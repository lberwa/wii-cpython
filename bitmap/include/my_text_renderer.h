#ifndef MY_TEXT_RENDERER_H
#define MY_TEXT_RENDERER_H

#include <stdint.h>
#include <gccore.h>  // unbedingt hier für GXRModeObj

// my_text_renderer.h
extern GXRModeObj* rmode;
extern void* framebuffer;


// Video initialisieren (einmal vor Textzeichnen aufrufen)
void video_init_custom(void);
extern int video_init_done;

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

// Fügt eine neue Zeile hinzu (automatisches Scrollen, wenn nötig).
// Committet am Ende eine evtl. offene Teilzeile (1 Aufruf = 1 Zeile für C-Code).
void terminal_print(const char *text);

// Stream-Ausgabe (z.B. Python stdout): hängt Bytes an die aktuelle Zeile an
// und bricht NUR bei echtem '\n' um. Teilzeilen bleiben bis zum naechsten
// Aufruf gepuffert. Rendert am Ende ohne VSync (schnell).
void terminal_write(const char *text, int len);

// Zeichnet neu, falls seit dem letzten Render etwas geaendert wurde (ohne VSync).
void terminal_flush(void);

// Zeichnet den kompletten Inhalt neu auf den Bildschirm (ohne VSync)
void terminal_render(void);

// Aktiviert oder deaktiviert automatisches Scrollen
void terminal_set_autoscroll(bool enabled);

#endif

