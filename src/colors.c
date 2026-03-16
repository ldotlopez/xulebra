/*
 * colors.c
 * Color pair initialisation for Xulebra.
 */

#include <curses.h>
#include "colors.h"

void colors_init(void)
{
    init_pair(CP_SNAKE_HEAD, COLOR_GREEN,  COLOR_BLACK);
    init_pair(CP_SNAKE_BODY, COLOR_GREEN,  COLOR_BLACK);
    init_pair(CP_APPLE,      COLOR_RED,    COLOR_BLACK);
    init_pair(CP_APPLE_FAST, COLOR_YELLOW, COLOR_BLACK);
    init_pair(CP_APPLE_SLOW, COLOR_CYAN,   COLOR_BLACK);
    init_pair(CP_BORDER,     COLOR_WHITE,  COLOR_BLACK);
    init_pair(CP_INFO_LABEL, COLOR_WHITE,  COLOR_BLACK);
    init_pair(CP_INFO_VALUE, COLOR_WHITE,  COLOR_BLACK);
    init_pair(CP_OPP_SNAKE,  COLOR_BLUE,   COLOR_BLACK);
    init_pair(CP_TITLE,      COLOR_GREEN,  COLOR_BLACK);
    init_pair(CP_GAMEOVER,   COLOR_RED,    COLOR_BLACK);
    /* Bright variants are applied at draw time via A_BOLD, not here */
}
