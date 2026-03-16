/*
 * colors.h
 * Color pair definitions and initialisation for Xulebra.
 *
 * All color pairs are defined here so both one_player.c and client.c
 * use the same IDs without coordinating at runtime.
 *
 * Usage
 * ─────
 *   Call colors_init() once after initscr() and start_color().
 *   Then apply a pair with wattron(win, COLOR_PAIR(CP_xxx)) /
 *   wattroff(win, COLOR_PAIR(CP_xxx)), or set a window's background
 *   with wbkgd(win, COLOR_PAIR(CP_xxx)).
 *
 * Graceful degradation
 * ────────────────────
 *   Always guard with has_colors() before calling colors_init().
 *   The CP_* constants are safe to pass to COLOR_PAIR() even on
 *   monochrome terminals – they resolve to attribute 0 (normal).
 */

#ifndef COLORS_H
#define COLORS_H

#include <curses.h>

/* ── Color pair IDs ─────────────────────────────────────────── */
#define CP_DEFAULT      0   /* Terminal default (no pair)         */
#define CP_SNAKE_HEAD   1   /* Bright green on black              */
#define CP_SNAKE_BODY   2   /* Green on black                     */
#define CP_APPLE        3   /* Bright red on black                */
#define CP_APPLE_FAST   4   /* Bright yellow on black (speed-up)  */
#define CP_APPLE_SLOW   5   /* Cyan on black (slow-down)          */
#define CP_BORDER       6   /* Bright white on black              */
#define CP_INFO_LABEL   7   /* White on black                     */
#define CP_INFO_VALUE   8   /* Bright white on black              */
#define CP_OPP_SNAKE    9   /* Bright blue on black (opponent)    */
#define CP_TITLE       10   /* Bright green on black (title text) */
#define CP_GAMEOVER    11   /* Bright red on black (game over)    */

/*
 * colors_init – define all color pairs.
 * Call once, after initscr() + start_color().
 * Safe to call even when has_colors() is false; the init_pair()
 * calls simply have no visible effect on monochrome terminals.
 */
static inline void colors_init(void)
{
    /* Pair 0 is always the terminal default and cannot be changed */
    init_pair(CP_SNAKE_HEAD, COLOR_GREEN,   COLOR_BLACK);
    init_pair(CP_SNAKE_BODY, COLOR_GREEN,   COLOR_BLACK);
    init_pair(CP_APPLE,      COLOR_RED,     COLOR_BLACK);
    init_pair(CP_APPLE_FAST, COLOR_YELLOW,  COLOR_BLACK);
    init_pair(CP_APPLE_SLOW, COLOR_CYAN,    COLOR_BLACK);
    init_pair(CP_BORDER,     COLOR_WHITE,   COLOR_BLACK);
    init_pair(CP_INFO_LABEL, COLOR_WHITE,   COLOR_BLACK);
    init_pair(CP_INFO_VALUE, COLOR_WHITE,   COLOR_BLACK);
    init_pair(CP_OPP_SNAKE,  COLOR_BLUE,    COLOR_BLACK);
    init_pair(CP_TITLE,      COLOR_GREEN,   COLOR_BLACK);
    init_pair(CP_GAMEOVER,   COLOR_RED,     COLOR_BLACK);

    /* Bright variants via A_BOLD – applied at draw time, not here */
}

#endif /* COLORS_H */
