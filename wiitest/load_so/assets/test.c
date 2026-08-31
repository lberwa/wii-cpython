/* test.c -> test.so
 *
 * Beispiel-Modul, das zur Laufzeit von sd:/test.so geladen wird.
 * Baurezept (siehe Makefile):
 *     powerpc-eabi-gcc -fPIC -fno-plt -shared -nostdlib test.c -o test.so
 *
 * Die externen Funktionen host_add()/host_log() liegen im Hauptprogramm
 * (source/main.c) und werden ueber die Export-Tabelle des Loaders aufgeloest.
 */

extern int  host_add(int a, int b);
extern void host_log(const char *msg);

/* konstanter Zeiger auf .rodata-String -> erzeugt eine R_PPC_RELATIVE-Reloc */
static const char *const kGreeting = "Hallo von test.so!";

/* Einstiegspunkt, den main.c per wii_dlsym("test_main") sucht. */
int test_main(int x)
{
    host_log(kGreeting);
    return host_add(x, 42);
}
