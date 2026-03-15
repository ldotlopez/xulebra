/*
 * xulnet.h
 * Network protocol definitions shared by the server and client.
 *
 * Packet layout (PACKET_SIZE bytes):
 *   [0] – message type  (one of the MSG_* constants below)
 *   [1] – x coordinate  (where applicable)
 *   [2] – y coordinate  (where applicable)
 *   [3] – reserved / extra data
 *   [4] – reserved / extra data
 */

#ifndef XULNET_H
#define XULNET_H

#include <curses.h>   /* KEY_UP / KEY_DOWN / KEY_LEFT / KEY_RIGHT */

/* ── Connection defaults ────────────────────────────────────── */
#define DEFAULT_HOST    "localhost"
#define DEFAULT_PORT    2580

/* ── Protocol sizes ─────────────────────────────────────────── */
#define PACKET_SIZE     5
#define LOGIN_LEN       9       /* 8 printable chars + '\0' */

/* ── Board geometry (must match defines.h) ──────────────────── */
#define NET_BOARD_ROWS  20
#define NET_BOARD_COLS  30
#define NET_SCREEN_COLS 80
#define NET_SCREEN_ROWS 25

/* ── Platform-specific "no key pressed" sentinel ────────────── */
#ifdef ANUBIS
#  define KEY_NONE  (-1)
#else
#  define KEY_NONE  (-2)
#endif

/* ── Direction keys (re-export from curses) ─────────────────── */
#define DIR_UP      KEY_UP
#define DIR_DOWN    KEY_DOWN
#define DIR_LEFT    KEY_LEFT
#define DIR_RIGHT   KEY_RIGHT

/* ────────────────────────────────────────────────────────────
 * Message types
 *
 * Client → Server:
 *   MSG_MOVE          Normal movement step (new head, old tail supplied)
 *   MSG_GROW          Growth step (only new head)
 *   MSG_SELF_COLLIDE  Client detected wall/border collision
 *   MSG_SELF_BITE     Client detected self-bite
 *   MSG_GROW_DONE     Snake finished its initial grow sequence
 *   MSG_ATE_APPLE     Client reports eating the apple at (x, y)
 *   MSG_PAUSE         Client requests a pause toggle
 *
 * Server → Client (relayed / generated):
 *   MSG_RELAY_MOVE    Opponent moved (relayed D_MOVE)
 *   MSG_RELAY_GROW    Opponent grew   (relayed D_GROW)
 *   MSG_OPP_COLLIDE   Opponent collided with wall
 *   MSG_OPP_BITE      Opponent bit itself
 *   MSG_DRAW          Both snakes moved to the same cell → tie
 *   MSG_I_BIT_OPP     Recipient's head entered opponent's body
 *   MSG_OPP_BIT_ME    Opponent's head entered recipient's body
 *   MSG_SET_APPLE     First apple placement (x, y)
 *   MSG_NEW_APPLE     Replacement apple after one was eaten (x, y)
 * ────────────────────────────────────────────────────────────*/

/* Client → Server */
#define MSG_MOVE         1
#define MSG_GROW         2
#define MSG_RELAY_MOVE   3
#define MSG_RELAY_GROW   4
#define MSG_SELF_COLLIDE 5
#define MSG_SELF_BITE    6
#define MSG_OPP_COLLIDE  7
#define MSG_OPP_BITE     8
#define MSG_DRAW         9
#define MSG_I_BIT_OPP    10
#define MSG_OPP_BIT_ME   11
#define MSG_GROW_DONE    12
#define MSG_SET_APPLE    13
#define MSG_ATE_APPLE    14
#define MSG_NEW_APPLE    15
#define MSG_PAUSE        16

#endif /* XULNET_H */
