/*
 * structs.h
 * Data structures and the Snake ADT.
 *
 * Key change from previous version
 * ─────────────────────────────────
 * SnakeNode now carries a `prev` pointer, making the list doubly-linked.
 * This reduces snake_pop_tail() from O(n) to O(1) — no more full-list
 * walk every frame to find the node before the tail.
 */

#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdlib.h>

/* ── Single cell on the board ──────────────────────────────── */
typedef struct Coord {
    int x;
    int y;
    int shape;
} Coord;

/* ── Doubly-linked node ─────────────────────────────────────── */
typedef struct SnakeNode {
    Coord            pos;
    struct SnakeNode *next;
    struct SnakeNode *prev;   /* NEW: O(1) tail removal */
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

/* ══════════════════════════════════════════════════════════════
 * Snake ADT — the only code that touches Snake internals
 * ══════════════════════════════════════════════════════════════ */

static inline void snake_init(Snake *s)
{
    s->head       = NULL;
    s->tail       = NULL;
    s->prev_dir   = 0;
    s->dir        = 0;
    s->score      = 0;
    s->speed      = 0;
    s->length     = 0;
    s->start_time = 0;
}

/*
 * snake_push_head – prepend a segment at (x,y).  O(1).
 * Used every frame for movement: the new head is the next position.
 */
static inline int snake_push_head(Snake *s, int x, int y, int shape)
{
    SnakeNode *node = (SnakeNode *)malloc(sizeof(SnakeNode));
    if (!node) return -1;

    node->pos.x    = x;
    node->pos.y    = y;
    node->pos.shape = shape;
    node->next     = s->head;
    node->prev     = NULL;

    if (s->head)
        s->head->prev = node;
    s->head = node;

    if (!s->tail)
        s->tail = node;

    s->length++;
    return 0;
}

/*
 * snake_push_tail – append a segment at (x,y).  O(1).
 * Used during initial snake placement (head-to-tail order).
 */
static inline int snake_push_tail(Snake *s, int x, int y, int shape)
{
    SnakeNode *node = (SnakeNode *)malloc(sizeof(SnakeNode));
    if (!node) return -1;

    node->pos.x     = x;
    node->pos.y     = y;
    node->pos.shape = shape;
    node->next      = NULL;
    node->prev      = s->tail;

    if (s->tail)
        s->tail->next = node;
    else
        s->head = node;

    s->tail = node;
    s->length++;
    return 0;
}

/*
 * snake_pop_tail – remove the trailing segment.  O(1).
 * Previously O(n) because the singly-linked list required a full walk.
 */
static inline int snake_pop_tail(Snake *s)
{
    SnakeNode *old;

    if (!s->tail) return 0;

    old = s->tail;
    s->tail = old->prev;

    if (s->tail)
        s->tail->next = NULL;
    else
        s->head = NULL;   /* list is now empty */

    free(old);
    s->length--;
    return 1;
}

/* snake_contains – O(n) membership test. */
static inline int snake_contains(const Snake *s, int x, int y)
{
    const SnakeNode *cur = s->head;
    while (cur) {
        if (cur->pos.x == x && cur->pos.y == y) return 1;
        cur = cur->next;
    }
    return 0;
}

/* snake_free – release all nodes. */
static inline void snake_free(Snake *s)
{
    SnakeNode *cur = s->head;
    while (cur) {
        SnakeNode *next = cur->next;
        free(cur);
        cur = next;
    }
    s->head = s->tail = NULL;
    s->length = 0;
}

#endif /* STRUCTS_H */
