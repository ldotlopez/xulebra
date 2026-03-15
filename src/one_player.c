/*
 * one_player.c
 * Single-player snake game.
 *
 * All mutable state lives in GameState, allocated on the stack of
 * one_player() and passed by pointer to every helper — no file-scope
 * variables are used anywhere in this file.
 *
 * Layout
 * ──────
 *  stdscr  : title screen only (before the game starts)
 *  game    : bordered playfield, BOARD_ROWS+2 tall, (BOARD_COLS*2)+2 wide
 *            (columns are doubled so the board looks square on a typical
 *             80-column terminal)
 *  info    : sidebar to the right of the board, same height
 *
 * Speed
 * ─────
 * The original used usleep() + blocking getch(), which is unreliable.
 * We use wtimeout(game, ms) so wgetch() returns ERR after the frame
 * interval and the snake keeps moving even when no key is pressed.
 * Speed is in milliseconds; lower = faster.
 *
 * Apple types  (apple.shape field)
 * ─────────────────────────────────
 *   0   normal apple  – score + grow
 *  +1   speed-up apple – score + grow + faster
 *  -1   slow-down apple – score + grow + slower
 *
 * Bugs fixed from original
 * ────────────────────────
 * • type_of_move checked point_is_in_list(apple) instead of comparing
 *   the future position against the apple position — always returned
 *   "eat" when the apple happened to be anywhere in the snake body.
 * • PAUSE case fell through into NOBUTTON without a break.
 * • sprintf(buf, "%d   \0", v) — the embedded \0 is redundant; removed.
 * • fflush(stdin) is undefined behaviour; removed.
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curses.h>

#include "defines.h"
#include "structs.h"

/* Forward declarations for score functions defined in score.c */
int score_write(const char *login, int points);
int score_show(int limit);

/* ── Private constants ──────────────────────────────────────── */
#define START_X           BOARD_ORIGIN_X
#define START_Y           BOARD_ORIGIN_Y
#define SPEED_DEFAULT_MS  60
#define SPEED_MAX_MS      10
#define SPEED_STEP_MS     15

/* Info-window row assignments */
#define INFO_ROW_PLAYER   1
#define INFO_ROW_APPLES   3
#define INFO_ROW_SPEED    4
#define INFO_ROW_POINTS   5
#define INFO_ROW_PAUSE    7

/* ═══════════════════════════════════════════════════════════════
 * Context object
 * ═══════════════════════════════════════════════════════════════ */

typedef struct {
    Snake   snake;
    Coord   apple;      /* apple.shape: 0=normal +1=fast -1=slow */
    WINDOW *game;
    WINDOW *info;
} GameState;

/* ═══════════════════════════════════════════════════════════════
 * Info sidebar
 * ═══════════════════════════════════════════════════════════════ */

static void info_init(const GameState *gs)
{
    const char *name = getenv("LOGNAME");
    if (!name) name = "player";

    mvwaddstr(gs->info, INFO_ROW_PLAYER, 1, "Player:  ");
    mvwaddstr(gs->info, INFO_ROW_PLAYER, 9, name);
    mvwaddstr(gs->info, INFO_ROW_APPLES, 1, "Apples:  0");
    mvwaddstr(gs->info, INFO_ROW_SPEED,  1, "Speed:   100");
    mvwaddstr(gs->info, INFO_ROW_POINTS, 1, "Points:  0");
    REFRESH(gs->info);
}

static void info_update_row(const GameState *gs, int row, int value)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d    ", value);
    mvwaddstr(gs->info, row, 11, buf);
    REFRESH(gs->info);
}

static void info_set_pause(const GameState *gs, int paused)
{
    mvwaddstr(gs->info, INFO_ROW_PAUSE, 1,
              paused ? "-= Paused =-" : "            ");
    REFRESH(gs->info);
}

/* ═══════════════════════════════════════════════════════════════
 * Body-segment character
 *
 * Preserves the original discriminant encoding exactly:
 *   discriminant = (old_dir * 100) + ((dx+1) * 10) + (dy+1)
 * where dx/dy are the delta between the new head and the current tail.
 * ═══════════════════════════════════════════════════════════════ */

static char body_char(int old_dir, int dx, int dy)
{
    int d = (old_dir * 100) + ((dx + 1) * 10) + (dy + 1);
    switch (d) {
    case 101: return CHAR_CORNER_2;
    case 110: return CHAR_BODY_V;
    case 121: return CHAR_CORNER_1;
    case 201: return CHAR_CORNER_4;
    case 212: return CHAR_BODY_V;
    case 221: return CHAR_CORNER_3;
    case 301: return CHAR_BODY_H;
    case 310: return CHAR_CORNER_3;
    case 312: return CHAR_CORNER_1;
    case 410: return CHAR_CORNER_4;
    case 412: return CHAR_CORNER_2;
    case 421: return CHAR_BODY_H;
    default:  return '*';
    }
}

/*
 * redraw_second_segment – repaint the node immediately behind the current
 * head with the correct body character reflecting the turn just made.
 *
 * When we prepend a new head and pop the tail, the second node becomes
 * the visual body segment just behind the new head.  We update it before
 * the head moves so we can still read the current head position.
 *
 * The discriminant formula is:   (prev_dir*100) + ((dx+1)*10) + (dy+1)
 * where dx/dy is the delta from the SECOND node to the new head position.
 * This matches the original beauty_char() encoding exactly.
 */
static void redraw_second_segment(GameState *gs, const Coord *new_head)
{
    const SnakeNode *second;
    int dx, dy;

    if (!gs->snake.head || !gs->snake.head->next) return;
    second = gs->snake.head->next;

    dx = new_head->x - second->pos.x;
    dy = new_head->y - second->pos.y;
    mvwaddch(gs->game,
             second->pos.y,
             second->pos.x * 2,
             (chtype)body_char(gs->snake.prev_dir, dx, dy));
}

/* ═══════════════════════════════════════════════════════════════
 * Head character
 * ═══════════════════════════════════════════════════════════════ */

static char head_char(int dir)
{
    switch (dir) {
    case DIR_UP:    return CHAR_HEAD_UP;
    case DIR_DOWN:  return CHAR_HEAD_DOWN;
    case DIR_LEFT:  return CHAR_HEAD_LEFT;
    default:        return CHAR_HEAD_RIGHT;
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Apple
 * ═══════════════════════════════════════════════════════════════ */

static void place_apple(GameState *gs)
{
    Coord p;
    srandom((unsigned int)time(NULL));
    do {
        p.x = (int)(random() % BOARD_COLS) + 1;
        p.y = (int)(random() % BOARD_ROWS) + 1;
    } while (snake_contains(&gs->snake, p.x, p.y));

    if ((random() % 100) < APPLE_SPAWN_PCT)
        p.shape = ((random() % 100) < SPEED_UP_PCT) ? 1 : -1;
    else
        p.shape = 0;

    gs->apple = p;
    mvwaddch(gs->game, p.y, p.x * 2, '@');
}

/* ═══════════════════════════════════════════════════════════════
 * Move classification
 *
 * Returns  0 normal move
 *          1 self-collision
 *          2 wall collision
 *          3 ate apple
 * ═══════════════════════════════════════════════════════════════ */

static int classify_move(GameState *gs, int key, Coord *next)
{
    *next = gs->snake.head->pos;   /* project forward from the leading segment */
    gs->snake.prev_dir = gs->snake.dir;

    /* Reject reversal */
    if ((key == DIR_UP    && gs->snake.dir == DIR_DOWN)  ||
        (key == DIR_DOWN  && gs->snake.dir == DIR_UP)    ||
        (key == DIR_LEFT  && gs->snake.dir == DIR_RIGHT) ||
        (key == DIR_RIGHT && gs->snake.dir == DIR_LEFT))
        key = KEY_NONE;

    switch (key) {
    case DIR_UP:    next->y--; gs->snake.dir = DIR_UP;    break;
    case DIR_DOWN:  next->y++; gs->snake.dir = DIR_DOWN;  break;
    case DIR_LEFT:  next->x--; gs->snake.dir = DIR_LEFT;  break;
    case DIR_RIGHT: next->x++; gs->snake.dir = DIR_RIGHT; break;

    case KEY_PAUSE: {
        int frozen_at = (int)time(NULL);
        wtimeout(gs->game, -1);
        info_set_pause(gs, 1);
        wgetch(gs->game);
        gs->snake.start_time += (int)time(NULL) - frozen_at;
        info_set_pause(gs, 0);
        wtimeout(gs->game, gs->snake.speed);
        /* Fall through: continue in current direction this frame */
    }
    /* FALLTHROUGH */
    default:
        switch (gs->snake.dir) {
        case DIR_UP:    next->y--; break;
        case DIR_DOWN:  next->y++; break;
        case DIR_LEFT:  next->x--; break;
        default:        next->x++; break;
        }
        break;
    }

    if (next->x < 1 || next->x > BOARD_COLS ||
        next->y < 1 || next->y > BOARD_ROWS)
        return 2;

    if (snake_contains(&gs->snake, next->x, next->y))
        return 1;

    if (next->x == gs->apple.x && next->y == gs->apple.y)
        return 3;

    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Movement actions
 * ═══════════════════════════════════════════════════════════════ */

static void move_normal(GameState *gs, const Coord *next)
{
    /*
     * Order matters:
     * 1. Redraw the second segment (now behind head) with corner/straight
     *    char – must happen while head still points to the current cell.
     * 2. Erase the tail cell.
     * 3. Pop tail, push new head.
     * 4. Draw the new head character.
     */
    redraw_second_segment(gs, next);

    if (gs->snake.tail)
        mvwaddch(gs->game, gs->snake.tail->pos.y,
                           gs->snake.tail->pos.x * 2, ' ');

    snake_pop_tail(&gs->snake);
    snake_push_head(&gs->snake, next->x, next->y, 0);

    mvwaddch(gs->game, next->y, next->x * 2,
             (chtype)head_char(gs->snake.dir));
    wmove(gs->game, 0, 0);   /* park cursor away from snake */
}

static void move_eat_apple(GameState *gs, const Coord *next)
{
    mvwaddch(gs->game, gs->apple.y, gs->apple.x * 2, 'O');

    /* Grow: add head without removing tail */
    snake_push_head(&gs->snake, next->x, next->y, 0);
    gs->snake.score++;

    switch (gs->apple.shape) {
    case 1:
        gs->snake.speed += SPEED_STEP_MS;
        info_update_row(gs, INFO_ROW_SPEED,
                        SPEED_DEFAULT_MS - gs->snake.speed + 100);
        wtimeout(gs->game, gs->snake.speed);
        beep();
        break;
    case -1:
        gs->snake.speed -= SPEED_STEP_MS;
        if (gs->snake.speed < SPEED_MAX_MS)
            gs->snake.speed = SPEED_MAX_MS;
        info_update_row(gs, INFO_ROW_SPEED,
                        SPEED_DEFAULT_MS - gs->snake.speed + 100);
        wtimeout(gs->game, gs->snake.speed);
        beep();
        break;
    default:
        break;
    }

    info_update_row(gs, INFO_ROW_APPLES, gs->snake.score);
    gs->apple.x = -1;   /* signal caller to place a new apple */
}

/* ═══════════════════════════════════════════════════════════════
 * Game-over
 * ═══════════════════════════════════════════════════════════════ */

static void game_over(GameState *gs, const char *reason)
{
    int points = (gs->snake.score * 10) -
                 ((int)time(NULL) - gs->snake.start_time);

    beep();
    endwin();

    if (points > 0) {
        const char *name = getenv("LOGNAME");
        if (!name) name = "player";
        score_write(name, points);
    }

    score_show(10);
    fprintf(stdout,
            "-=-=-=-=-=-=] Game Over !!! (%s) [=-=-=-=-=-=-\n", reason);
    exit(EXIT_SUCCESS);
}

/* ═══════════════════════════════════════════════════════════════
 * Setup helpers
 * ═══════════════════════════════════════════════════════════════ */

static void show_title(void)
{
    mvaddstr(1, 1, "__  __     _      _");
    mvaddstr(2, 1, " \\ \\/ /   _| | ___| |__  _ __ __ _");
    mvaddstr(3, 1, "  \\  / | | | |/ _ \\ '_ \\| '__/ _` |");
    mvaddstr(4, 1, "  /  \\ |_| | |  __/ |_) | | | (_| |");
    mvaddstr(5, 1, " /_/\\_\\__,_|_|\\___|_.__/|_|  \\__,_|");
    refresh();
}

static void draw_borders(const GameState *gs)
{
    wborder(gs->game, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
    wborder(gs->info, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
}

static void snake_draw_all(const GameState *gs)
{
    const SnakeNode *cur = gs->snake.head;
    while (cur) {
        mvwaddch(gs->game, cur->pos.y, cur->pos.x * 2, '*');
        cur = cur->next;
    }
}

static void snake_place_initial(GameState *gs)
{
    int x;
    /*
     * The snake starts moving RIGHT so the rightmost segment is the head.
     * snake_push_tail() appends, so we iterate left-to-right:
     * first call places the tail (lowest x), last call places the head.
     * After the loop:
     *   head -> (START_X + SNAKE_START_LEN - 1, START_Y)  leading cell
     *   tail -> (START_X,                        START_Y)  trailing cell
     *
     * We do NOT use snake_push_head() here because that prepends —
     * iterating in any order with push_head produces a reversed list.
     */
    /*
     * snake_push_head() prepends  → head = visual front (newest).
     * snake_push_tail() appends   → tail = last appended.
     *
     * We want: head = rightmost cell (x=3, leading edge moving RIGHT)
     *          tail = leftmost cell  (x=1, trailing edge)
     *
     * Using push_tail and iterating RIGHT-TO-LEFT:
     *   push_tail(3) → [3]          head=(3) tail=(3)
     *   push_tail(2) → [3]→[2]      head=(3) tail=(2)
     *   push_tail(1) → [3]→[2]→[1] head=(3) tail=(1)  ✓
     */
    for (x = START_X + SNAKE_START_LEN - 1; x >= START_X; x--)
        snake_push_tail(&gs->snake, x, START_Y, 0);

    gs->snake.dir        = DIR_RIGHT;
    gs->snake.prev_dir   = DIR_RIGHT;
    gs->snake.score      = 0;
    gs->snake.speed      = SPEED_DEFAULT_MS;
    gs->snake.start_time = (int)time(NULL);
}

/* ═══════════════════════════════════════════════════════════════
 * Public entry point
 * ═══════════════════════════════════════════════════════════════ */

void one_player(int argc, char *argv[])
{
    GameState gs;
    int       key;
    Coord     next;

    (void)argc; (void)argv;

    memset(&gs, 0, sizeof(gs));
    snake_init(&gs.snake);

    /* ncurses setup */
    initscr();
    nonl();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    show_title();
    getch();   /* wait for keypress before starting */

    /* Create sub-windows */
    gs.game = newwin(BOARD_ROWS + 2, (BOARD_COLS * 2) + 2, 0, 0);
    gs.info = newwin(BOARD_ROWS + 2,
                     SCREEN_COLS - (BOARD_COLS * 2) - 2,
                     0, (BOARD_COLS * 2) + 2);

    if (!gs.game || !gs.info)
        game_over(&gs, "Error creating windows");

    keypad(gs.game, TRUE);
    wtimeout(gs.game, SPEED_DEFAULT_MS);

    snake_place_initial(&gs);
    draw_borders(&gs);
    snake_draw_all(&gs);
    info_init(&gs);
    place_apple(&gs);
    REFRESH(gs.game);

    /* Game loop */
    for (;;) {
        key = wgetch(gs.game);

        switch (classify_move(&gs, key, &next)) {
        case 0: move_normal(&gs, &next);                  break;
        case 1: game_over(&gs, "You bit yourself");       break;
        case 2: game_over(&gs, "Out of bounds");          break;
        case 3: move_eat_apple(&gs, &next);               break;
        }

        if (gs.apple.x < 0)
            place_apple(&gs);

        info_update_row(&gs, INFO_ROW_POINTS,
                        (gs.snake.score * 10) -
                        ((int)time(NULL) - gs.snake.start_time));
        REFRESH(gs.game);
    }
}