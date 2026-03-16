/*
 * colors.h
 * Color pair IDs and the colors_init() declaration.
 *
 * Call colors_init() once after initscr() + start_color().
 * Guard with has_colors() — CP_* constants are safe on monochrome
 * terminals; COLOR_PAIR(CP_x) resolves to attribute 0.
 */

#ifndef COLORS_H
#define COLORS_H

#include <curses.h>

/* ── Color pair IDs ─────────────────────────────────────────── */
#define CP_DEFAULT      0
#define CP_SNAKE_HEAD   1   /* Bright green  – player head      */
#define CP_SNAKE_BODY   2   /* Green         – player body      */
#define CP_APPLE        3   /* Bright red    – normal apple     */
#define CP_APPLE_FAST   4   /* Bright yellow – speed-up apple   */
#define CP_APPLE_SLOW   5   /* Cyan          – slow-down apple  */
#define CP_BORDER       6   /* Bright white  – window borders   */
#define CP_INFO_LABEL   7   /* White         – sidebar labels   */
#define CP_INFO_VALUE   8   /* Bright white  – sidebar values   */
#define CP_OPP_SNAKE    9   /* Bright blue   – opponent / bot   */
#define CP_TITLE       10   /* Bright green  – title logo       */
#define CP_GAMEOVER    11   /* Bright red    – game-over text   */

/* Define all color pairs. Call once after initscr() + start_color(). */
void colors_init(void);

#endif /* COLORS_H */