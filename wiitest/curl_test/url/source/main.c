#define NETWORK_H22
#include <string.h>
#include <stdlib.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/video.h>
#include <ogc/pad.h>
#include <network.h>
#include <gccore.h>
#include "wiiuse/wpad.h"
#include <ogc/gx.h>
//#include <lwip/sockets.h>
#include "my_text_renderer.h"

//#define SERVER_IP "192.168.1.100"
//#define SERVER_PORT 12344
#define SERVER_IP "192.168.15.152"
#define SERVER_PORT 8000

static void *xfb = NULL;

int main() {
      
    
    video_init_custom();
    //VIDEO_Init();
    VIDEO_ClearFrameBuffer(rmode, framebuffer, COLOR_WHITE);
    terminal_print("init init");
    /*
    for (int i = 0;i<100;i++) {
    VIDEO_WaitVSync();
    }
    */
    //video_init_custom();
    WPAD_Init();
    terminal_print("wpad_init");
    /*
    for (int i = 0;i<100;i++) {
    VIDEO_WaitVSync();
    }
    */
    terminal_print("xfb");
    
    /*
    for (int i = 0;i<100;i++) {
    VIDEO_WaitVSync();
    }
    */
    
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    
    //terminal_clear();
    terminal_print("console init");
    //console_init( , 0, 0, 640, 480, 640*2);
    //console_init((PVOID),20,20,rmode->fbWidth,
    	//		rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
    
    console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
    
    terminal_print("console init ende");
    terminal_print("WLAN Initalisieren ...");
    
    // WLAN initialisieren
    if(net_init() < 0) {
        terminal_print("WLAN Init fehlgeschlagen");
        return 1;
    }
    terminal_print("WLAN initialisiert");

    int sock = net_socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        terminal_print("Socket Fehler");
        return 1;
    }


    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = SERVER_PORT; // network.h nimmt Port direkt als int
    IP4_ADDR(&server.sin_addr, 192,168,15,152);

    terminal_print("Verbinde mit Server...");
    if(net_connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        terminal_print("Verbindung fehlgeschlagen");
        net_close(sock);
        return 1;
    }
    terminal_print("Verbunden!");

    const char* msg = "Hallo vom Wii-Client!";
    int sent = net_send(sock, /*(char*)*/msg, strlen(msg), 0);
    if(sent < 0) {
        terminal_print("Senden fehlgeschlagen");
    } else {
        terminal_print("Nachricht gesendet");
    }
    
    terminal_print("send req");
    
    const char *req =
    "GET / HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "Connection: close\r\n"
    "\r\n";

net_send(sock, req, strlen(req), 0);

    char buffer[1024];
    terminal_print("empfange");
    
    int received = 0;
    WPAD_ScanPads();
    u32 pressed = WPAD_ButtonsHeld(0);
    
    
    while (received == 0 || (pressed & WPAD_BUTTON_A)) {
        WPAD_ScanPads();
        u32 pressed = WPAD_ButtonsHeld(0);
    
    	received = net_recv(sock, buffer, sizeof(buffer), 0);
    	/*
    	if (received > 0) {
    	// buffer enthält die Daten
    	terminal_print(buffer);
    	}*/
    }
	
    if (received > 0) {
    	// buffer enthält die Daten
    	terminal_print(buffer);
    } else {
     terminal_print("nichts empfangen!!!");
    }
	
    net_close(sock);
    terminal_print("Socket geschlossen");

    for (int i = 0;i<200;i++) {
    VIDEO_WaitVSync();
    }
    terminal_print("exiting ...");

    return 0;
}
