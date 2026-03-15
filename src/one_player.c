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
 *  game    : bordered playfield, board_rows+2 tall, (board_cols*2)+2 wide
 *            (columns are doubled so the board looks square on a typical
 *             80-column terminal)
 *  info    : sidebar to the right of the board, same height
 *
 * Speed
 * ─────
 * We use wtimeout(game, ms) so wgetch() returns ERR after the frame
 * interval and the snake keeps moving even when no key is pressed.
 * Speed is in milliseconds; lower = faster.
 *
 * Command-line flags (passed after -1)
 * ─────────────────────────────────────
 *   -W <cols>   Board width  in cells   (default 30, min 10, max terminal width/2 - 2)
 *   -H <rows>   Board height in cells   (default 20, min 5,  max terminal height - 4)
 *   -S <ms>     Initial frame interval  (default 60, min 10, max 500)
 *
 * Apple types  (apple.shape field)
 * ─────────────────────────────────
 *   0   normal apple  – score + grow
 *  +1   speed-up apple – score + grow + faster
 *  -1   slow-down apple – score + grow + slower
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

/* ── Limits ─────────────────────────────────────────────────── */
#define SPEED_MIN_MS      10    /* Fastest allowed frame interval   */
#define SPEED_MAX_MS     500    /* Slowest allowed frame interval   */
#define SPEED_STEP_MS     15    /* Change per speed apple           */
#define BOARD_COLS_MIN    10
#define BOARD_ROWS_MIN     5

/* Info-window row assignments */
#define INFO_ROW_PLAYER    1
#define INFO_ROW_APPLES    3
#define INFO_ROW_SPEED     4
#define INFO_ROW_POINTS    5
#define INFO_ROW_PAUSE     7

/* ═══════════════════════════════════════════════════════════════
 * Context object
 * ═══════════════════════════════════════════════════════════════ */

typedef struct {
    Snake   snake;
    Coord   apple;          /* apple.shape: 0=normal +1=fast -1=slow */
    int     board_cols;     /* Playable columns (runtime, from -W)   */
    int     board_rows;     /* Playable rows    (runtime, from -H)   */
    WINDOW *game;
    WINDOW *info;
} GameState;

/* ═══════════════════════════════════════════════════════════════
 * Argument parsing
 * ═══════════════════════════════════════════════════════════════ */

/*
 * parse_args – fill *cols, *rows, *speed_ms from argv.
 * Outputs are pre-initialised to defaults by the caller before this runs.
 * Returns 0 on success, -1 on any validation error.
 */
static int parse_args(int argc, char *argv[],
                      int *cols, int *rows, int *speed_ms)
{
    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] == '\0') {
            fprintf(stderr, "one_player: unknown argument: %s\n", argv[i]);
            return -1;
        }
        switch (argv[i][1]) {
        case 'W':
            if (i + 1 >= argc) {
                fprintf(stderr, "one_player: -W requires a column count\n");
                return -1;
            }
            *cols = atoi(argv[++i]);
            if (*cols < BOARD_COLS_MIN) {
                fprintf(stderr,
                        "one_player: board width must be >= %d\n",
                        BOARD_COLS_MIN);
                return -1;
            }
            break;
        case 'H':
            if (i + 1 >= argc) {
                fprintf(stderr, "one_player: -H requires a row count\n");
                return -1;
            }
            *rows = atoi(argv[++i]);
            if (*rows < BOARD_ROWS_MIN) {
                fprintf(stderr,
                        "one_player: board height must be >= %d\n",
                        BOARD_ROWS_MIN);
                return -1;
            }
            break;
        case 'S':
            if (i + 1 >= argc) {
                fprintf(stderr, "one_player: -S requires a millisecond value\n");
                return -1;
            }
            *speed_ms = atoi(argv[++i]);
            if (*speed_ms < SPEED_MIN_MS || *speed_ms > SPEED_MAX_MS) {
                fprintf(stderr,
                        "one_player: speed must be between %d and %d ms\n",
                        SPEED_MIN_MS, SPEED_MAX_MS);
                return -1;
            }
            break;
        default:
            fprintf(stderr, "one_player: unknown option -%c\n", argv[i][1]);
            return -1;
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Terminal size validation
 * ═══════════════════════════════════════════════════════════════ */

/*
 * validate_board_fits – check that the requested board fits the current
 * terminal.  Must be called after initscr() so LINES/COLS are known.
 * Adjusts *cols / *rows downward if they exceed the available space,
 * printing a warning.  Returns -1 if the terminal is too small even
 * for the minimum board size.
 */
static int validate_board_fits(int *cols, int *rows)
{
    /* game window needs (board_cols*2)+2 columns and board_rows+2 rows.
       info sidebar needs at least 18 columns.
       Total: (board_cols*2) + 2 + 18 columns, board_rows + 2 rows. */
    int max_cols = (COLS   - 20) / 2;   /* leave 18 cols for info + 2 border */
    int max_rows =  LINES  -  2;        /* leave 2 rows for border            */

    if (max_cols < BOARD_COLS_MIN || max_rows < BOARD_ROWS_MIN) {
        fprintf(stderr,
                "one_player: terminal too small "
                "(need at least %d x %d)\n",
                BOARD_COLS_MIN * 2 + 20,
                BOARD_ROWS_MIN + 2);
        return -1;
    }

    if (*cols > max_cols) {
        fprintf(stderr,
                "one_player: -W %d exceeds terminal width, clamping to %d\n",
                *cols, max_cols);
        *cols = max_cols;
    }
    if (*rows > max_rows) {
        fprintf(stderr,
                "one_player: -H %d exceeds terminal height, clamping to %d\n",
                *rows, max_rows);
        *rows = max_rows;
    }
    return 0;
}

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
    mvwaddstr(gs->info, INFO_ROW_SPEED,  1, "Speed:   ");
    /* Display current speed as a percentage: 100% = slowest (SPEED_MAX_MS) */
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d ms   ", gs->snake.speed);
        mvwaddstr(gs->info, INFO_ROW_SPEED, 10, buf);
    }
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

static void info_update_speed(const GameState *gs)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d ms   ", gs->snake.speed);
    mvwaddstr(gs->info, INFO_ROW_SPEED, 10, buf);
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
 * where dx/dy are the delta between the new head and the second segment.
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
 * redraw_second_segment – repaint the node immediately behind the head
 * with the correct corner/straight character before the head advances.
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
        p.x = (int)(random() % gs->board_cols) + 1;
        p.y = (int)(random() % gs->board_rows) + 1;
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
    *next = gs->snake.head->pos;
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

    /* Use runtime board dimensions, not compile-time constants */
    if (next->x < 1 || next->x > gs->board_cols ||
        next->y < 1 || next->y > gs->board_rows)
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
    redraw_second_segment(gs, next);

    if (gs->snake.tail)
        mvwaddch(gs->game, gs->snake.tail->pos.y,
                           gs->snake.tail->pos.x * 2, ' ');

    snake_pop_tail(&gs->snake);
    snake_push_head(&gs->snake, next->x, next->y, 0);

    mvwaddch(gs->game, next->y, next->x * 2,
             (chtype)head_char(gs->snake.dir));
    wmove(gs->game, 0, 0);
}

static void move_eat_apple(GameState *gs, const Coord *next)
{
    mvwaddch(gs->game, gs->apple.y, gs->apple.x * 2, 'O');

    snake_push_head(&gs->snake, next->x, next->y, 0);
    gs->snake.score++;

    switch (gs->apple.shape) {
    case 1:
        gs->snake.speed += SPEED_STEP_MS;
        if (gs->snake.speed > SPEED_MAX_MS)
            gs->snake.speed = SPEED_MAX_MS;
        info_update_speed(gs);
        wtimeout(gs->game, gs->snake.speed);
        beep();
        break;
    case -1:
        gs->snake.speed -= SPEED_STEP_MS;
        if (gs->snake.speed < SPEED_MIN_MS)
            gs->snake.speed = SPEED_MIN_MS;
        info_update_speed(gs);
        wtimeout(gs->game, gs->snake.speed);
        beep();
        break;
    default:
        break;
    }

    info_update_row(gs, INFO_ROW_APPLES, gs->snake.score);
    gs->apple.x = -1;
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
     * snake_push_tail() appends; iterating RIGHT-TO-LEFT means the
     * first push places the head (highest x, leading edge) and the
     * last push places the tail (lowest x, trailing edge):
     *   push_tail(3) → [3]           head=(3) tail=(3)
     *   push_tail(2) → [3]→[2]       head=(3) tail=(2)
     *   push_tail(1) → [3]→[2]→[1]  head=(3) tail=(1)  ✓
     */
    for (x = BOARD_ORIGIN_X + SNAKE_START_LEN - 1; x >= BOARD_ORIGIN_X; x--)
        snake_push_tail(&gs->snake, x, BOARD_ORIGIN_Y, 0);

    gs->snake.dir        = DIR_RIGHT;
    gs->snake.prev_dir   = DIR_RIGHT;
    gs->snake.score      = 0;
    /* speed is already set from parsed args before this is called */
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
    int       init_speed = 60;          /* defaults */
    int       board_cols = BOARD_COLS;
    int       board_rows = BOARD_ROWS;

    memset(&gs, 0, sizeof(gs));
    snake_init(&gs.snake);

    /* Parse flags before initscr so errors go to a clean terminal */
    if (parse_args(argc, argv, &board_cols, &board_rows, &init_speed) < 0)
        return;

    /* ncurses setup */
    initscr();
    nonl();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    /* Clamp board to actual terminal size (needs LINES/COLS from initscr) */
    if (validate_board_fits(&board_cols, &board_rows) < 0) {
        endwin();
        return;
    }

    gs.board_cols    = board_cols;
    gs.board_rows    = board_rows;
    gs.snake.speed   = init_speed;

    show_title();
    getch();

    /* Create sub-windows using runtime dimensions */
    gs.game = newwin(gs.board_rows + 2,
                     (gs.board_cols * 2) + 2, 0, 0);
    gs.info = newwin(gs.board_rows + 2,
                     COLS - (gs.board_cols * 2) - 2,
                     0, (gs.board_cols * 2) + 2);

    if (!gs.game || !gs.info)
        game_over(&gs, "Error creating windows");

    keypad(gs.game, TRUE);
    wtimeout(gs.game, gs.snake.speed);

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