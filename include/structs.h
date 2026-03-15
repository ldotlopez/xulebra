/*
 * structs.h
 * Data structures shared by all translation units.
 *
 * Naming conventions
 *   - Types:     CamelCase  (e.g. Coord, SnakeNode)
 *   - Functions: snake_case (e.g. snake_add_block)
 */

#ifndef STRUCTS_H
#define STRUCTS_H

/* ── Single cell on the board ──────────────────────────────── */
typedef struct Coord {
    int x;
    int y;
    int shape;      /* Rendering character for this cell */
} Coord;

/* ── Linked-list node holding one body segment ─────────────── */
typedef struct SnakeNode {
    Coord           pos;
    struct SnakeNode *next;
} SnakeNode;

/* ── The snake itself ───────────────────────────────────────── */
typedef struct Snake {
    SnakeNode *head;        /* Front of the body (newest segment)  */
    SnakeNode *tail;        /* Back  of the body (oldest segment)  */
    int        prev_dir;    /* Direction during the last frame     */
    int        dir;         /* Current movement direction          */
    int        score;       /* Apples eaten so far                 */
    int        speed;       /* Current speed level                 */
    int        start_time;  /* Unix timestamp when the game began  */
} Snake;

/* ── High-score record (one entry in the HOF database) ─────── */
typedef struct ScoreRecord {
    char login[9];  /* Null-terminated, 8 chars + '\0' */
    int  points;
} ScoreRecord;

/* ────────────────────────────────────────────────────────────
 * Snake operations
 * These are the only functions that should touch Snake internals.
 * ────────────────────────────────────────────────────────────*/

/*
 * snake_init – zero-initialise *s.
 * Must be called before any other snake_* function.
 */
static inline void snake_init(Snake *s)
{
    s->head      = NULL;
    s->tail      = NULL;
    s->prev_dir  = 0;
    s->dir       = 0;
    s->score     = 0;
    s->speed     = 0;
    s->start_time = 0;
}

/*
 * snake_push_head – prepend a new segment at (x, y).
 * Returns 0 on success, -1 on allocation failure.
 */
static inline int snake_push_head(Snake *s, int x, int y, int shape)
{
    SnakeNode *node = (SnakeNode *)malloc(sizeof(SnakeNode));
    if (!node) return -1;

    node->pos.x    = x;
    node->pos.y    = y;
    node->pos.shape = shape;
    node->next     = s->head;
    s->head        = node;

    if (s->tail == NULL)
        s->tail = node;   /* First node is both head and tail */

    return 0;
}

/*
 * snake_push_tail – append a new segment at (x, y) to the END of the list.
 * Used during initial placement to build the body in head-to-tail order
 * without reversing: first call = head cell, last call = tail cell.
 * Returns 0 on success, -1 on allocation failure.
 */
static inline int snake_push_tail(Snake *s, int x, int y, int shape)
{
    SnakeNode *node = (SnakeNode *)malloc(sizeof(SnakeNode));
    if (!node) return -1;

    node->pos.x     = x;
    node->pos.y     = y;
    node->pos.shape = shape;
    node->next      = NULL;

    if (s->head == NULL)
        s->head = node;          /* First node is both head and tail */
    else
        s->tail->next = node;    /* Link after current tail           */

    s->tail = node;
    return 0;
}

/*
 * snake_pop_tail – remove the last segment.
 * Returns 1 if a node was removed, 0 if the list was already empty.
 */
static inline int snake_pop_tail(Snake *s)
{
    if (s->head == NULL) return 0;

    /* Single-node case */
    if (s->head == s->tail) {
        free(s->head);
        s->head = s->tail = NULL;
        return 1;
    }

    /* Walk to the node just before tail */
    SnakeNode *cur = s->head;
    while (cur->next != s->tail)
        cur = cur->next;

    free(s->tail);
    s->tail   = cur;
    cur->next = NULL;
    return 1;
}

/*
 * snake_contains – return 1 if any segment occupies (x, y).
 */
static inline int snake_contains(const Snake *s, int x, int y)
{
    const SnakeNode *cur = s->head;
    while (cur) {
        if (cur->pos.x == x && cur->pos.y == y) return 1;
        cur = cur->next;
    }
    return 0;
}

/*
 * snake_free – release all nodes; leaves *s in an init-equivalent state.
 */
static inline void snake_free(Snake *s)
{
    SnakeNode *cur = s->head;
    while (cur) {
        SnakeNode *next = cur->next;
        free(cur);
        cur = next;
    }
    s->head = s->tail = NULL;
}

#endif /* STRUCTS_H */