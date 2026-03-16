/*
 * snake.c
 * Snake ADT implementation.
 *
 * The list is doubly-linked so push and pop at both ends are O(1).
 * snake_contains() is necessarily O(n) — no spatial index is maintained.
 */

#include <stdlib.h>
#include "snake.h"

void snake_init(Snake *s)
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

int snake_push_head(Snake *s, int x, int y, int shape)
{
    SnakeNode *node = (SnakeNode *)malloc(sizeof(SnakeNode));
    if (!node) return -1;

    node->pos.x     = x;
    node->pos.y     = y;
    node->pos.shape = shape;
    node->next      = s->head;
    node->prev      = NULL;

    if (s->head)
        s->head->prev = node;
    s->head = node;

    if (!s->tail)
        s->tail = node;

    s->length++;
    return 0;
}

int snake_push_tail(Snake *s, int x, int y, int shape)
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

int snake_pop_tail(Snake *s)
{
    SnakeNode *old;

    if (!s->tail) return 0;

    old     = s->tail;
    s->tail = old->prev;

    if (s->tail)
        s->tail->next = NULL;
    else
        s->head = NULL;

    free(old);
    s->length--;
    return 1;
}

int snake_contains(const Snake *s, int x, int y)
{
    const SnakeNode *cur = s->head;
    while (cur) {
        if (cur->pos.x == x && cur->pos.y == y) return 1;
        cur = cur->next;
    }
    return 0;
}

void snake_free(Snake *s)
{
    SnakeNode *cur = s->head;
    while (cur) {
        SnakeNode *next = cur->next;
        free(cur);
        cur = next;
    }
    s->head   = s->tail = NULL;
    s->length = 0;
}
