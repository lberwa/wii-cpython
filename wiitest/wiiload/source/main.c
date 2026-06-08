
#include <gccore.h>
#include "../../../fat/include/pyfat.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include "../../../bitmap/include/my_text_renderer.h"
             

static void print_int_value(const char *label, int value)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s%d", label, value);
    terminal_print(buf);
}

static void print_size_value(const char *label, size_t value)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s%u", label, (unsigned)value);
    terminal_print(buf);
}

static int export_file_to_sd(const char *path, const unsigned char *data, size_t size)
{
    FILE *f;
    unsigned char buffer[256];
    size_t offset = 0;

    terminal_print("before fopen");
    f = fopen(path, "wb");
    if (!f) {
        terminal_print("fopen failed:");
        terminal_print(path);
        print_int_value("fopen errno=", errno);
        return -1;
    }
    terminal_print("after fopen");

    while (offset < size) {
        size_t chunk = (size - offset > sizeof(buffer)) ? sizeof(buffer) : (size - offset);
        memcpy(buffer, data + offset, chunk);
        print_size_value("offset=", offset);
        print_size_value("chunk=", chunk);
        terminal_print("before fwrite");
        errno = 0;
        if (fwrite(buffer, 1, chunk, f) != chunk) {
            terminal_print("fwrite failed");
            print_int_value("fwrite errno=", errno);
            fclose(f);
            return -2;
        }
        terminal_print("after fwrite");
        offset += chunk;
    }

    terminal_print("before fflush");
    errno = 0;
    if (fflush(f) != 0) {
        terminal_print("fflush failed");
        print_int_value("fflush errno=", errno);
        fclose(f);
        return -3;
    }
    terminal_print("after fflush");

    terminal_print("before fclose");
    errno = 0;
    if (fclose(f) != 0) {
        terminal_print("fclose failed");
        print_int_value("fclose errno=", errno);
        return -4;
    }
    terminal_print("after fclose");

    return 0;
}

extern const unsigned char sd_test3_dat[];
extern const unsigned char sd_test3_dat_end[];


int main(/*int argc, char **argv*/)
{
    video_init_custom();

    terminal_print("Start...");
             
    if (!fatInitDefault())
    {
        terminal_print("SD init failed");
        return 1;
    }

   {
       const unsigned char *data = sd_test3_dat;
       size_t size = sd_test3_dat_end - sd_test3_dat;

       const char *path = "sd:/hello/test3.dat";


        terminal_print("writing ... :");
        terminal_print(path);
        print_size_value("size=", size);
        if (size > 0) {
            print_int_value("byte0=", data[0]);
        }
        print_int_value("export rc=", export_file_to_sd(path, data, size));


        }

    terminal_print("Done writing files.\n");
    terminal_print("exiting...\n");
    return 0;
}
