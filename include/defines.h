/*
 * defines.h
 * Constants for the Snake game.
 */

#ifndef DEFINES_H
#define DEFINES_H

#include <curses.h>

/* ── Movement keys ─────────────────────────────────────────── */
#define DIR_UP      KEY_UP
#define DIR_DOWN    KEY_DOWN
#define DIR_LEFT    KEY_LEFT
#define DIR_RIGHT   KEY_RIGHT
#define KEY_PAUSE   ' '

/* ANUBIS platform: getch() returns -1 when no key is pressed.
   All other platforms return -2 via the halfdelay timeout. */
#ifdef ANUBIS
#  define KEY_NONE  (-1)
#else
#  define KEY_NONE  (-2)
#endif

/* ── Board geometry ────────────────────────────────────────── */
#define BOARD_ROWS      20      /* Playable rows inside the border   */
#define BOARD_COLS      30      /* Playable columns inside the border */
#define BOARD_ORIGIN_X   1      /* Top-left corner X of the board    */
#define BOARD_ORIGIN_Y   1      /* Top-left corner Y of the board    */
#define SCREEN_COLS     80
#define SCREEN_ROWS     25

/* ── Snake startup parameters ──────────────────────────────── */
#define SNAKE_START_LEN  3
#define SNAKE_START_DIR  DIR_RIGHT

/* ── Timing ────────────────────────────────────────────────── */
#define SPEED_DEFAULT   60      /* Centiseconds between frames (halfdelay units) */
#define SPEED_MAX       10      /* Fastest allowed speed value                   */
#define SPEED_STEP      15      /* How much speed increases per apple            */

/* ── Apple spawn probabilities ─────────────────────────────── */
#define APPLE_SPAWN_PCT  20     /* Chance (%) of a new apple each tick           */
#define SPEED_UP_PCT      2     /* Chance (%) of a speed-up apple               */

/* ── Rendering characters ──────────────────────────────────── */
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

/* ── Persistence ────────────────────────────────────────────── */
#define SCORE_DATABASE  "/tmp/.xulebra.hof"

/* ── ncurses helper ─────────────────────────────────────────── */
/* Marks a window dirty and flushes all pending updates in one call. */
#define REFRESH(win)    do { wnoutrefresh(win); doupdate(); } while (0)

#endif /* DEFINES_H */
