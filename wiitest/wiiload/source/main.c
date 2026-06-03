
#include <gccore.h>
#include "../../../fat/include/pyfat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include "../../../bitmap/include/my_text_renderer.h"
             
extern const unsigned char sd_test3_dat[];
extern const unsigned char sd_test3_dat_end[];


int main(/*int argc, char **argv*/)
{
    video_init_custom();

    terminal_print("Start...\n");
             
    for (int i=0; i<100; i++) {
        VIDEO_WaitVSync();
    }
             
    if (!fatInitDefault())
    {
        terminal_print("SD init failed\n");
        return 1;
    }

   {
       const unsigned char *data = sd_test3_dat;
       size_t size = sd_test3_dat_end - sd_test3_dat;

       const char *path = "sd:/test3.dat";


        FILE *f = fopen(path, "w");

if (!f) {
    terminal_print("Failed to write:");
    terminal_print(path);
} else {
    terminal_print("writing ... :");
    terminal_print(path);

    fwrite(data, 1, size, f);

    terminal_print("jj");

    terminal_print("before fflush");
fflush(f);
terminal_print("after fflush");

terminal_print("before fclose");
fclose(f);
terminal_print("after fclose");

        }



        }

    terminal_print("Done writing files.\n");
    terminal_print("exiting...\n");
    return 0;
}
