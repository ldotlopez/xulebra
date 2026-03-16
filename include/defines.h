/*
 * defines.h
 * Shared constants for the Snake game.
 */

#ifndef DEFINES_H
#define DEFINES_H

#include <curses.h>

/* ── Direction keys ─────────────────────────────────────────── */
#define DIR_UP      KEY_UP
#define DIR_DOWN    KEY_DOWN
#define DIR_LEFT    KEY_LEFT
#define DIR_RIGHT   KEY_RIGHT
#define KEY_PAUSE   ' '
#define KEY_QUIT_Q  'q'
#define KEY_QUIT_ESC 27    /* ASCII ESC */

#ifdef ANUBIS
#  define KEY_NONE  (-1)
#else
#  define KEY_NONE  (-2)
#endif

/* ── Default board geometry ─────────────────────────────────── */
#define BOARD_ROWS       20
#define BOARD_COLS       30
#define BOARD_ORIGIN_X    1
#define BOARD_ORIGIN_Y    1
#define SCREEN_COLS      80
#define SCREEN_ROWS      25
#define BOARD_COLS_MIN   10
#define BOARD_ROWS_MIN    5

/* ── Snake startup ──────────────────────────────────────────── */
#define SNAKE_START_LEN   3
#define SNAKE_START_DIR   DIR_RIGHT

/* ── Speed levels (shared by one_player.c and client.c) ─────── */
#define SPEED_LEVEL_MIN   1
#define SPEED_LEVEL_MAX  10
#define SPEED_LEVEL_DEF   5
/*
 * Convert a 1-10 level to a frame interval in milliseconds.
 * Level 1 → 500 ms (slowest)   Level 10 → 50 ms (fastest)
 */
#define LEVEL_TO_MS(lvl)  (550 - (lvl) * 50)

/* ── Apple spawn probabilities ──────────────────────────────── */
#define APPLE_SPAWN_PCT  20
#define SPEED_UP_PCT      2

/* ── Score database ─────────────────────────────────────────── */
#define SCORE_DATABASE    "/tmp/.xulebra.hof"
#define SCORE_MAX_RECORDS 100   /* Hard cap; file is truncated after each write */

/* ── Rendering characters ───────────────────────────────────── */
#define CHAR_BODY_H     '-'
#define CHAR_BODY_V     '|'
#define CHAR_CORNER_1   '/'
#define CHAR_CORNER_2   '\\'
#define CHAR_CORNER_3   '\\'
#define CHAR_CORNER_4   '/'
#define CHAR_HEAD_UP    '^'
#define CHAR_HEAD_DOWN  'v'
#define CHAR_HEAD_LEFT  '<'
#define CHAR_HEAD_RIGHT '>'

/* ── ncurses flush helper ───────────────────────────────────── */
#define REFRESH(win)  do { wnoutrefresh(win); doupdate(); } while (0)

#endif /* DEFINES_H */
