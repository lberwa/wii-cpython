#include "bitmap_colors.h"
#include <string.h>  // strcmp

uint32_t get_color_by_name(const char *name) {
    if (strcmp(name, "black") == 0)       return COLOR_BLACK;
    if (strcmp(name, "maroon") == 0)      return COLOR_MAROON;
    if (strcmp(name, "green") == 0)       return COLOR_GREEN;
    if (strcmp(name, "olive") == 0)       return COLOR_OLIVE;
    if (strcmp(name, "navy") == 0)        return COLOR_NAVY;
    if (strcmp(name, "purple") == 0)      return COLOR_PURPLE;
    if (strcmp(name, "teal") == 0)        return COLOR_TEAL;
    if (strcmp(name, "gray") == 0)        return COLOR_GRAY;
    if (strcmp(name, "grey") == 0)        return COLOR_GRAY;
    if (strcmp(name, "silver") == 0)      return COLOR_SILVER;
    if (strcmp(name, "red") == 0)         return COLOR_RED;
    if (strcmp(name, "lime") == 0)        return COLOR_LIME;
    if (strcmp(name, "yellow") == 0)      return COLOR_YELLOW;
    if (strcmp(name, "blue") == 0)        return COLOR_BLUE;
    if (strcmp(name, "fuchsia") == 0)     return COLOR_FUCHSIA;
    if (strcmp(name, "magenta") == 0)     return COLOR_FUCHSIA;
    if (strcmp(name, "aqua") == 0)        return COLOR_AQUA;
    if (strcmp(name, "cyan") == 0)        return COLOR_AQUA;
    if (strcmp(name, "white") == 0)       return COLOR_WHITE;
    if (strcmp(name, "moneygreen") == 0)  return COLOR_MONEYGREEN;
    if (strcmp(name, "skyblue") == 0)     return COLOR_SKYBLUE;
    if (strcmp(name, "cream") == 0)       return COLOR_CREAM;
    if (strcmp(name, "medgray") == 0)     return COLOR_MEDGRAY;

    // fallback
    return COLOR_WHITE;
}

