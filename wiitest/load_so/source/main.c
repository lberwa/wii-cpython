/* load_so - Demo: laedt und startet sd:/test.so ueber den Wii-dlopen-Loader.
 *
 * Ablauf:
 *   1. Video/Konsole initialisieren.
 *   2. SD-Karte mounten (fatInitDefault).
 *   3. Export-Tabelle registrieren (host_add/host_log), die das geladene
 *      Modul aufrufen darf.
 *   4. wii_dlopen("sd:/test.so") -> wii_dlsym("test_main") -> aufrufen.
 *   5. Ergebnis anzeigen, auf HOME warten.
 */

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>

#include "wii_dlfcn.h"

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

/* ---- Symbole, die dem Modul zur Verfuegung gestellt werden ---------------- */

static int host_add(int a, int b)
{
    return a + b;
}

static void host_log(const char *msg)
{
    printf("  [test.so] %s\n", msg);
}

static const wii_dl_sym g_exports[] = {
    { "host_add", (void *)host_add },
    { "host_log", (void *)host_log },
};

/* --------------------------------------------------------------------------- */

static void video_init(void)
{
    VIDEO_Init();
    WPAD_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight,
                 rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();
}

int main(void)
{
    video_init();

    printf("\n\n  === wii_dlopen Demo: sd:/test.so ===\n\n");

    if (!fatInitDefault()) {
        printf("  FEHLER: fatInitDefault() fehlgeschlagen (keine SD?)\n");
    } else {
        wii_dl_register_exports(g_exports,
                                sizeof(g_exports) / sizeof(g_exports[0]));

        printf("  Lade sd:/test.so ...\n");
        void *h = wii_dlopen("sd:/test.so", WII_RTLD_NOW);
        if (!h) {
            printf("  wii_dlopen FEHLER: %s\n", wii_dlerror());
        } else {
            printf("  geladen. Suche Symbol 'test_main' ...\n");
            int (*test_main)(int) = (int (*)(int))wii_dlsym(h, "test_main");
            if (!test_main) {
                printf("  wii_dlsym FEHLER: %s\n", wii_dlerror());
            } else {
                int r = test_main(100);
                printf("  test_main(100) = %d  (erwartet: 142)\n", r);
                printf("  --> %s\n", (r == 142) ? "OK" : "FALSCH");
            }
            wii_dlclose(h);
            printf("  Modul entladen.\n");
        }
    }

    printf("\n  HOME druecken zum Beenden.\n");
    while (1) {
        WPAD_ScanPads();
        if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME)
            break;
        VIDEO_WaitVSync();
    }
    return 0;
}
