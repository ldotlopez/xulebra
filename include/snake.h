/*
 * snake.h
 * Core data types and the Snake ADT public interface.
 *
 * The doubly-linked SnakeNode gives O(1) push and pop at both ends.
 * All functions that operate on Snake are defined in snake.c and
 * declared here — no inline bodies in headers.
 */

#ifndef SNAKE_H
#define SNAKE_H

/* ── Single cell on the board ──────────────────────────────── */
typedef struct Coord {
    int x;
    int y;
    int shape;
} Coord;

/* ── Doubly-linked body segment ─────────────────────────────── */
typedef struct SnakeNode {
    Coord            pos;
    struct SnakeNode *next;
    struct SnakeNode *prev;
} SnakeNode;

/* ── The snake ──────────────────────────────────────────────── */
typedef struct Snake {
    SnakeNode *head;        /* Newest segment (visual front)  */
    SnakeNode *tail;        /* Oldest segment (visual rear)   */
    int        prev_dir;
    int        dir;
    int        score;
    int        speed;       /* Current frame interval in ms   */
    int        length;      /* Current number of segments     */
    int        start_time;
} Snake;

/* ── High-score record ──────────────────────────────────────── */
typedef struct ScoreRecord {
    char login[9];
    int  points;
} ScoreRecord;

/* ── Snake ADT ──────────────────────────────────────────────── */

/* Zero-initialise *s. Must be called before any other snake_ function. */
void snake_init(Snake *s);

/* Prepend a segment at (x,y). O(1). Returns 0 on success, -1 on OOM. */
int  snake_push_head(Snake *s, int x, int y, int shape);

/* Append a segment at (x,y). O(1). Returns 0 on success, -1 on OOM. */
int  snake_push_tail(Snake *s, int x, int y, int shape);

/* Remove the trailing segment. O(1). Returns 1 if removed, 0 if empty. */
int  snake_pop_tail(Snake *s);

/* Return 1 if any segment occupies (x,y). O(n). */
int  snake_contains(const Snake *s, int x, int y);

/* Release all nodes; leaves *s in an init-equivalent state. */
void snake_free(Snake *s);

#endif /* SNAKE_H */
