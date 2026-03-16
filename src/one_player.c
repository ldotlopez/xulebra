/*
 * one_player.c
 * Single-player snake game.
 *
 * Command-line flags
 * ──────────────────
 *   -W <cols>    Board width  (default 30, min 10)
 *   -H <rows>    Board height (default 20, min 5)
 *   -S <level>   Initial speed level 1-10 (default 5)
 *   -L <len>     Initial snake length (default 3, min 1)
 *   -A <n>       Number of apples on board at once (default 1, max 9)
 *   -N <n>       Auto speed-up: raise level every N apples (0 = off)
 *   -T           Wrap-around mode: exit wall re-enters opposite side
 *   -b           Enable AI opponent (bot snake, blue)
 *
 * Speed
 * ─────
 * User-facing level 1-10.  ms = 550 - level*50.
 * Speed apples change level ±1; -N triggers an extra +1 every N apples.
 *
 * Apple types
 * ───────────
 *   shape  0   normal     – score + grow           red  '@'
 *   shape +1   speed-up   – score + grow + faster  yellow '+'
 *   shape -1   slow-down  – score + grow + slower  cyan 'o'
 *
 * Bot AI
 * ──────
 * The bot uses a two-stage decision at each frame:
 *
 *   Stage 1 – Safety filter
 *     For each of the four possible directions, compute a flood-fill
 *     from the candidate cell (excluding both snake bodies).  Directions
 *     that reach fewer cells than the bot's current length are marked
 *     unsafe and excluded from consideration unless ALL directions are
 *     unsafe (then the safest one is used as a last resort).
 *
 *   Stage 2 – Greedy apple targeting
 *     Among the safe directions, pick the one whose candidate cell has
 *     the smallest Manhattan distance to the nearest apple.
 *
 * This combination avoids most self-trapping while still pursuing food
 * aggressively.  It is intentionally beatable by a human player.
 */

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curses.h>

#include "defines.h"
#include "structs.h"
#include "colors.h"

/* Forward declarations */
int score_write(const char *login, int points);
int score_show(int limit);
void score_show_ncurses(int limit, int has_color);

/* ── Constants ──────────────────────────────────────────────── */
#define MAX_APPLES     9
#define SNAKE_LEN_MIN  1
#define SNAKE_LEN_MAX  20
#define AUTO_SPEED_OFF 0

/* ── Info-window rows ───────────────────────────────────────── */
#define INFO_ROW_PLAYER    1
#define INFO_ROW_BOT       2   /* only visible when bot is enabled */
#define INFO_ROW_APPLES    4
#define INFO_ROW_BOT_SCORE 5   /* only visible when bot is enabled */
#define INFO_ROW_SPEED     6
#define INFO_ROW_POINTS    7
#define INFO_ROW_TIME      8
#define INFO_ROW_PAUSE    10

/* ═══════════════════════════════════════════════════════════════
 * Context object
 * ═══════════════════════════════════════════════════════════════ */

typedef struct {
    Snake   snake;          /* Player snake                      */

    /* Bot */
    Snake   bot;            /* AI snake                          */
    int     bot_enabled;    /* 1 when -b is passed               */
    int     bot_alive;      /* 0 once the bot has collided       */

    /* Board */
    int     board_cols;
    int     board_rows;
    int     wrap;

    /* Apples */
    Coord   apples[MAX_APPLES];
    int     n_apples;

    /* Speed */
    int     speed_level;
    int     auto_speed_n;

    /* Init */
    int     init_len;

    /* Display */
    int     has_color;
    WINDOW *game;
    WINDOW *info;
} GameState;

/* ── Speed helpers ──────────────────────────────────────────── */

static void apply_speed(GameState *gs)
{
    if (gs->speed_level < SPEED_LEVEL_MIN) gs->speed_level = SPEED_LEVEL_MIN;
    if (gs->speed_level > SPEED_LEVEL_MAX) gs->speed_level = SPEED_LEVEL_MAX;
    gs->snake.speed = LEVEL_TO_MS(gs->speed_level);
    wtimeout(gs->game, gs->snake.speed);
}

/* ═══════════════════════════════════════════════════════════════
 * Argument parsing
 * ═══════════════════════════════════════════════════════════════ */

static int parse_args(int argc, char *argv[], GameState *gs,
                      int *board_cols, int *board_rows)
{
    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] == '\0') {
            fprintf(stderr, "one_player: unknown argument: %s\n", argv[i]);
            return -1;
        }
        switch (argv[i][1]) {
        case 'W':
            if (i+1>=argc){fprintf(stderr,"one_player: -W needs a value\n");return -1;}
            *board_cols = atoi(argv[++i]);
            if (*board_cols < BOARD_COLS_MIN){fprintf(stderr,"one_player: -W >= %d\n",BOARD_COLS_MIN);return -1;}
            break;
        case 'H':
            if (i+1>=argc){fprintf(stderr,"one_player: -H needs a value\n");return -1;}
            *board_rows = atoi(argv[++i]);
            if (*board_rows < BOARD_ROWS_MIN){fprintf(stderr,"one_player: -H >= %d\n",BOARD_ROWS_MIN);return -1;}
            break;
        case 'S':
            if (i+1>=argc){fprintf(stderr,"one_player: -S needs a value\n");return -1;}
            gs->speed_level = atoi(argv[++i]);
            if (gs->speed_level < SPEED_LEVEL_MIN || gs->speed_level > SPEED_LEVEL_MAX){
                fprintf(stderr,"one_player: -S must be %d-%d\n",SPEED_LEVEL_MIN,SPEED_LEVEL_MAX);return -1;}
            break;
        case 'L':
            if (i+1>=argc){fprintf(stderr,"one_player: -L needs a value\n");return -1;}
            gs->init_len = atoi(argv[++i]);
            if (gs->init_len < SNAKE_LEN_MIN || gs->init_len > SNAKE_LEN_MAX){
                fprintf(stderr,"one_player: -L must be %d-%d\n",SNAKE_LEN_MIN,SNAKE_LEN_MAX);return -1;}
            break;
        case 'A':
            if (i+1>=argc){fprintf(stderr,"one_player: -A needs a value\n");return -1;}
            gs->n_apples = atoi(argv[++i]);
            if (gs->n_apples < 1 || gs->n_apples > MAX_APPLES){
                fprintf(stderr,"one_player: -A must be 1-%d\n",MAX_APPLES);return -1;}
            break;
        case 'N':
            if (i+1>=argc){fprintf(stderr,"one_player: -N needs a value\n");return -1;}
            gs->auto_speed_n = atoi(argv[++i]);
            if (gs->auto_speed_n < 0){fprintf(stderr,"one_player: -N must be >= 0\n");return -1;}
            break;
        case 'T':
            gs->wrap = 1;
            break;
        case 'b':
            gs->bot_enabled = 1;
            break;
        default:
            fprintf(stderr,"one_player: unknown option -%c\n", argv[i][1]);
            return -1;
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Terminal size validation
 * ═══════════════════════════════════════════════════════════════ */

static int validate_board_fits(int *cols, int *rows)
{
    int max_cols = (COLS - 20) / 2;
    int max_rows = LINES - 2;

    if (max_cols < BOARD_COLS_MIN || max_rows < BOARD_ROWS_MIN) {
        endwin();
        fprintf(stderr, "one_player: terminal too small (need %dx%d)\n",
                BOARD_COLS_MIN*2+20, BOARD_ROWS_MIN+2);
        return -1;
    }
    if (*cols > max_cols){fprintf(stderr,"one_player: -W clamped to %d\n",max_cols);*cols=max_cols;}
    if (*rows > max_rows){fprintf(stderr,"one_player: -H clamped to %d\n",max_rows);*rows=max_rows;}
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Info sidebar
 * ═══════════════════════════════════════════════════════════════ */

static void info_label(const GameState *gs, int row, const char *s)
{
    if (gs->has_color) wattron(gs->info, COLOR_PAIR(CP_INFO_LABEL));
    mvwaddstr(gs->info, row, 1, s);
    if (gs->has_color) wattroff(gs->info, COLOR_PAIR(CP_INFO_LABEL));
}

static void info_value(const GameState *gs, int row, int col, const char *s)
{
    if (gs->has_color) wattron(gs->info, COLOR_PAIR(CP_INFO_VALUE) | A_BOLD);
    mvwaddstr(gs->info, row, col, s);
    if (gs->has_color) wattroff(gs->info, COLOR_PAIR(CP_INFO_VALUE) | A_BOLD);
}

static void info_init(const GameState *gs)
{
    char buf[20];
    const char *name = getenv("LOGNAME");
    if (!name) name = "player";

    info_label(gs, INFO_ROW_PLAYER, "Player: ");
    info_value(gs, INFO_ROW_PLAYER, 9, name);

    if (gs->bot_enabled) {
        if (gs->has_color) wattron(gs->info, COLOR_PAIR(CP_OPP_SNAKE) | A_BOLD);
        mvwaddstr(gs->info, INFO_ROW_BOT, 1, "Bot:    AI");
        if (gs->has_color) wattroff(gs->info, COLOR_PAIR(CP_OPP_SNAKE) | A_BOLD);
    }

    info_label(gs, INFO_ROW_APPLES, "Apples: ");
    info_value(gs, INFO_ROW_APPLES, 9, "0");

    if (gs->bot_enabled) {
        info_label(gs, INFO_ROW_BOT_SCORE, "BotEat: ");
        info_value(gs, INFO_ROW_BOT_SCORE, 9, "0");
    }

    info_label(gs, INFO_ROW_SPEED, "Speed:  ");
    snprintf(buf, sizeof(buf), "Level %d  ", gs->speed_level);
    info_value(gs, INFO_ROW_SPEED, 9, buf);

    info_label(gs, INFO_ROW_POINTS, "Points: ");
    info_value(gs, INFO_ROW_POINTS, 9, "0");

    info_label(gs, INFO_ROW_TIME, "Time:   ");
    info_value(gs, INFO_ROW_TIME, 9, "0s   ");

    if (gs->wrap)
        info_label(gs, INFO_ROW_PAUSE - 1, "[wrap]");

    REFRESH(gs->info);
}

static void info_update_int(const GameState *gs, int row, int col, int value)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d    ", value);
    info_value(gs, row, col, buf);
    REFRESH(gs->info);
}

static void info_update_speed(const GameState *gs)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "Level %d  ", gs->speed_level);
    info_value(gs, INFO_ROW_SPEED, 9, buf);
    REFRESH(gs->info);
}

static void info_set_pause(const GameState *gs, int paused)
{
    if (paused) {
        if (gs->has_color) wattron(gs->info, COLOR_PAIR(CP_GAMEOVER) | A_BOLD);
        mvwaddstr(gs->info, INFO_ROW_PAUSE, 1, "-= Paused =-");
        if (gs->has_color) wattroff(gs->info, COLOR_PAIR(CP_GAMEOVER) | A_BOLD);
    } else {
        mvwaddstr(gs->info, INFO_ROW_PAUSE, 1, "            ");
    }
    REFRESH(gs->info);
}

/* ═══════════════════════════════════════════════════════════════
 * Apple management
 * ═══════════════════════════════════════════════════════════════ */

/*
 * cell_occupied – returns 1 if (x,y) is blocked by either snake or an apple.
 * Used by apple placement and the bot flood-fill.
 */
static int cell_occupied(const GameState *gs, int x, int y)
{
    int i;
    if (snake_contains(&gs->snake, x, y)) return 1;
    if (gs->bot_enabled && gs->bot_alive && snake_contains(&gs->bot, x, y)) return 1;
    for (i = 0; i < gs->n_apples; i++) {
        if (gs->apples[i].x == x && gs->apples[i].y == y) return 1;
    }
    return 0;
}

static void place_apple_slot(GameState *gs, int idx)
{
    Coord p;
    do {
        p.x = (int)(rand() % gs->board_cols) + 1;
        p.y = (int)(rand() % gs->board_rows) + 1;
    } while (cell_occupied(gs, p.x, p.y));

    if ((rand() % 100) < APPLE_SPAWN_PCT)
        p.shape = ((rand() % 100) < SPEED_UP_PCT) ? 1 : -1;
    else
        p.shape = 0;

    gs->apples[idx] = p;
}

static void draw_apple(const GameState *gs, int idx)
{
    const Coord *a = &gs->apples[idx];
    int   pair;
    chtype ch;

    switch (a->shape) {
    case  1: pair = CP_APPLE_FAST; ch = '+'; break;
    case -1: pair = CP_APPLE_SLOW; ch = 'o'; break;
    default: pair = CP_APPLE;      ch = '@'; break;
    }

    if (gs->has_color) wattron(gs->game, COLOR_PAIR(pair) | A_BOLD);
    mvwaddch(gs->game, a->y, a->x * 2, ch);
    if (gs->has_color) wattroff(gs->game, COLOR_PAIR(pair) | A_BOLD);
}

static void place_all_apples(GameState *gs)
{
    int i;
    for (i = 0; i < gs->n_apples; i++) {
        place_apple_slot(gs, i);
        draw_apple(gs, i);
    }
}

static int apple_at(const GameState *gs, int x, int y)
{
    int i;
    for (i = 0; i < gs->n_apples; i++) {
        if (gs->apples[i].x == x && gs->apples[i].y == y) return i;
    }
    return -1;
}

/* ═══════════════════════════════════════════════════════════════
 * Snake rendering
 * ═══════════════════════════════════════════════════════════════ */

static char body_char(int old_dir, int dx, int dy)
{
    int d = (old_dir * 100) + ((dx + 1) * 10) + (dy + 1);
    switch (d) {
    case 101: return CHAR_CORNER_2; case 110: return CHAR_BODY_V;
    case 121: return CHAR_CORNER_1; case 201: return CHAR_CORNER_4;
    case 212: return CHAR_BODY_V;   case 221: return CHAR_CORNER_3;
    case 301: return CHAR_BODY_H;   case 310: return CHAR_CORNER_3;
    case 312: return CHAR_CORNER_1; case 410: return CHAR_CORNER_4;
    case 412: return CHAR_CORNER_2; case 421: return CHAR_BODY_H;
    default:  return '*';
    }
}

static void redraw_second_segment(GameState *gs, const Coord *new_head)
{
    const SnakeNode *second;
    int dx, dy;
    if (!gs->snake.head || !gs->snake.head->next) return;
    second = gs->snake.head->next;
    dx = new_head->x - second->pos.x;
    dy = new_head->y - second->pos.y;
    if (gs->has_color) wattron(gs->game, COLOR_PAIR(CP_SNAKE_BODY));
    mvwaddch(gs->game, second->pos.y, second->pos.x * 2,
             (chtype)body_char(gs->snake.prev_dir, dx, dy));
    if (gs->has_color) wattroff(gs->game, COLOR_PAIR(CP_SNAKE_BODY));
}

static char head_char(int dir)
{
    switch (dir) {
    case DIR_UP:   return CHAR_HEAD_UP;
    case DIR_DOWN: return CHAR_HEAD_DOWN;
    case DIR_LEFT: return CHAR_HEAD_LEFT;
    default:       return CHAR_HEAD_RIGHT;
    }
}

static void draw_head(GameState *gs, int y, int x)
{
    if (gs->has_color) wattron(gs->game, COLOR_PAIR(CP_SNAKE_HEAD) | A_BOLD);
    mvwaddch(gs->game, y, x * 2, (chtype)head_char(gs->snake.dir));
    if (gs->has_color) wattroff(gs->game, COLOR_PAIR(CP_SNAKE_HEAD) | A_BOLD);
}

/* ═══════════════════════════════════════════════════════════════
 * Wrap-around helper
 * ═══════════════════════════════════════════════════════════════ */

static void wrap_coord(const GameState *gs, Coord *c)
{
    if (!gs->wrap) return;
    if (c->x < 1)              c->x = gs->board_cols;
    if (c->x > gs->board_cols) c->x = 1;
    if (c->y < 1)              c->y = gs->board_rows;
    if (c->y > gs->board_rows) c->y = 1;
}

/* ═══════════════════════════════════════════════════════════════
 * Move classification (player)
 *   0  normal   1  self-collision   2  wall   3  ate apple
 * ═══════════════════════════════════════════════════════════════ */

static int classify_move(GameState *gs, int key, Coord *next, int *apple_idx)
{
    *next = gs->snake.head->pos;
    gs->snake.prev_dir = gs->snake.dir;

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

    wrap_coord(gs, next);

    if (next->x < 1 || next->x > gs->board_cols ||
        next->y < 1 || next->y > gs->board_rows)
        return 2;

    if (snake_contains(&gs->snake, next->x, next->y))
        return 1;

    /* Check bot body collision (player runs into bot) */
    if (gs->bot_enabled && gs->bot_alive &&
        snake_contains(&gs->bot, next->x, next->y))
        return 1;

    *apple_idx = apple_at(gs, next->x, next->y);
    if (*apple_idx >= 0) return 3;

    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Player movement
 * ═══════════════════════════════════════════════════════════════ */

static void move_normal(GameState *gs, const Coord *next)
{
    redraw_second_segment(gs, next);
    if (gs->snake.tail)
        mvwaddch(gs->game, gs->snake.tail->pos.y,
                           gs->snake.tail->pos.x * 2, ' ');
    snake_pop_tail(&gs->snake);
    snake_push_head(&gs->snake, next->x, next->y, 0);
    draw_head(gs, next->y, next->x);
    wmove(gs->game, 0, 0);
}

static void move_eat_apple(GameState *gs, const Coord *next, int idx)
{
    int shape = gs->apples[idx].shape;
    mvwaddch(gs->game, gs->apples[idx].y, gs->apples[idx].x * 2, 'O');
    snake_push_head(&gs->snake, next->x, next->y, 0);
    gs->snake.score++;

    switch (shape) {
    case 1:  gs->speed_level++; apply_speed(gs); info_update_speed(gs); beep(); break;
    case -1: gs->speed_level--; apply_speed(gs); info_update_speed(gs); beep(); break;
    default: break;
    }

    if (gs->auto_speed_n > AUTO_SPEED_OFF &&
        gs->snake.score % gs->auto_speed_n == 0) {
        gs->speed_level++;
        apply_speed(gs);
        info_update_speed(gs);
    }

    info_update_int(gs, INFO_ROW_APPLES, 9, gs->snake.score);
    place_apple_slot(gs, idx);
    draw_apple(gs, idx);
}

/* ═══════════════════════════════════════════════════════════════
 * Bot AI
 * ═══════════════════════════════════════════════════════════════ */

/*
 * bot_flood_fill – count the number of empty cells reachable from (sx,sy)
 * without crossing either snake body or the board walls.
 *
 * Uses an iterative BFS with a fixed-size visited bitmap allocated on the
 * stack (up to 100×50 = 5000 bits = 625 bytes; well within stack limits).
 * If the board is larger the bitmap degrades gracefully: cells beyond
 * FLOOD_MAX_COLS × FLOOD_MAX_ROWS are counted as reachable (conservative).
 */
#define FLOOD_MAX_COLS  100
#define FLOOD_MAX_ROWS   50
#define FLOOD_CELLS     (FLOOD_MAX_COLS * FLOOD_MAX_ROWS)

typedef struct { int x, y; } FloodCell;

static int bot_flood_fill(const GameState *gs, int sx, int sy)
{
    /* visited bitmap: 1 bit per cell, stored as bytes */
    unsigned char visited[FLOOD_CELLS / 8 + 1];
    FloodCell     queue[FLOOD_CELLS];
    int           head_q = 0, tail_q = 0;
    int           count  = 0;
    int           cols   = gs->board_cols;
    int           rows   = gs->board_rows;

    /* If the board is larger than our bitmap, return a large number —
       we can't tell if the pocket is small, so assume it's safe. */
    if (cols > FLOOD_MAX_COLS || rows > FLOOD_MAX_ROWS)
        return 9999;

    memset(visited, 0, sizeof(visited));

#define VISITED_IDX(x,y)  (((y)-1) * cols + ((x)-1))
#define IS_VISITED(x,y)   (visited[VISITED_IDX(x,y)/8] &  (1 << (VISITED_IDX(x,y)%8)))
#define SET_VISITED(x,y)  (visited[VISITED_IDX(x,y)/8] |= (1 << (VISITED_IDX(x,y)%8)))

    /* Seed */
    if (sx < 1 || sx > cols || sy < 1 || sy > rows) return 0;
    if (snake_contains(&gs->snake, sx, sy)) return 0;
    if (gs->bot_alive && snake_contains(&gs->bot, sx, sy)) return 0;

    SET_VISITED(sx, sy);
    queue[tail_q].x = sx;
    queue[tail_q].y = sy;
    tail_q++;
    count++;

    while (head_q != tail_q) {
        int cx = queue[head_q].x;
        int cy = queue[head_q].y;
        head_q++;

        /* Neighbours */
        int dx[] = {0,0,-1,1};
        int dy[] = {-1,1,0,0};
        int k;
        for (k = 0; k < 4; k++) {
            int nx = cx + dx[k];
            int ny = cy + dy[k];

            if (nx < 1 || nx > cols || ny < 1 || ny > rows) continue;
            if (IS_VISITED(nx, ny)) continue;
            if (snake_contains(&gs->snake, nx, ny)) continue;
            if (gs->bot_alive && snake_contains(&gs->bot, nx, ny)) continue;

            SET_VISITED(nx, ny);
            queue[tail_q].x = nx;
            queue[tail_q].y = ny;
            tail_q++;
            count++;
        }
    }

#undef VISITED_IDX
#undef IS_VISITED
#undef SET_VISITED

    return count;
}

/*
 * bot_choose_dir – two-stage decision:
 *
 *   1. Safety: flood-fill each candidate cell; prefer directions that lead
 *      to open space (>= bot body length).  If all directions are unsafe,
 *      fall through to the least-bad option.
 *
 *   2. Greedy: among safe directions, choose the one with minimum Manhattan
 *      distance to the nearest apple.
 */
static int bot_choose_dir(const GameState *gs)
{
    static const int dirs[4] = { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };
    int best_dir   = gs->bot.dir;
    int best_dist  = INT_MAX;
    int best_space = -1;
    int i, j;

    /* Nearest apple coordinates */
    int tgt_x = gs->apples[0].x;
    int tgt_y = gs->apples[0].y;
    int best_apple_dist = INT_MAX;

    for (j = 0; j < gs->n_apples; j++) {
        int d = abs(gs->bot.head->pos.x - gs->apples[j].x) +
                abs(gs->bot.head->pos.y - gs->apples[j].y);
        if (d < best_apple_dist) {
            best_apple_dist = d;
            tgt_x = gs->apples[j].x;
            tgt_y = gs->apples[j].y;
        }
    }

    for (i = 0; i < 4; i++) {
        int d = dirs[i];
        int nx = gs->bot.head->pos.x;
        int ny = gs->bot.head->pos.y;

        /* Reject reversal */
        if ((d == DIR_UP    && gs->bot.dir == DIR_DOWN)  ||
            (d == DIR_DOWN  && gs->bot.dir == DIR_UP)    ||
            (d == DIR_LEFT  && gs->bot.dir == DIR_RIGHT) ||
            (d == DIR_RIGHT && gs->bot.dir == DIR_LEFT))
            continue;

        /* Project one step */
        if (d == DIR_UP)    ny--;
        if (d == DIR_DOWN)  ny++;
        if (d == DIR_LEFT)  nx--;
        if (d == DIR_RIGHT) nx++;

        /* Respect wrap */
        if (gs->wrap) {
            if (nx < 1) nx = gs->board_cols;
            if (nx > gs->board_cols) nx = 1;
            if (ny < 1) ny = gs->board_rows;
            if (ny > gs->board_rows) ny = 1;
        }

        /* Hard wall / body rejection */
        if (nx < 1 || nx > gs->board_cols || ny < 1 || ny > gs->board_rows)
            continue;
        if (snake_contains(&gs->bot,   nx, ny)) continue;
        if (snake_contains(&gs->snake, nx, ny)) continue;

        /* Stage 1: flood-fill safety */
        int space = bot_flood_fill(gs, nx, ny);

        /* Stage 2: distance to target apple */
        int dist = abs(nx - tgt_x) + abs(ny - tgt_y);

        /*
         * Prefer safe directions (space >= body length).
         * Among equally safe candidates, prefer the closest to the apple.
         * If no safe candidate found yet, track the least-bad fallback
         * (largest reachable space).
         */
        int is_safe = (space >= gs->bot.length);

        if (best_space < gs->bot.length) {
            /* No safe candidate yet: take whatever has most space */
            if (space > best_space ||
                (space == best_space && dist < best_dist)) {
                best_space = space;
                best_dist  = dist;
                best_dir   = d;
            }
        } else if (is_safe) {
            /* Safe candidate: prefer closer to apple */
            if (dist < best_dist ||
                (dist == best_dist && space > best_space)) {
                best_space = space;
                best_dist  = dist;
                best_dir   = d;
            }
        }
    }

    return best_dir;
}

/*
 * bot_step – move the bot one cell.
 *
 * Returns:
 *   0  moved normally
 *   1  collided (wall, self, or player body)
 *   2  ate an apple (idx returned in *ate_idx)
 */
static int bot_step(GameState *gs, int *ate_idx)
{
    int dir = bot_choose_dir(gs);
    int nx  = gs->bot.head->pos.x;
    int ny  = gs->bot.head->pos.y;
    Coord old_tail;

    gs->bot.prev_dir = gs->bot.dir;
    gs->bot.dir      = dir;

    if (dir == DIR_UP)    ny--;
    if (dir == DIR_DOWN)  ny++;
    if (dir == DIR_LEFT)  nx--;
    if (dir == DIR_RIGHT) nx++;

    /* Wrap */
    if (gs->wrap) {
        if (nx < 1) nx = gs->board_cols;
        if (nx > gs->board_cols) nx = 1;
        if (ny < 1) ny = gs->board_rows;
        if (ny > gs->board_rows) ny = 1;
    }

    /* Wall */
    if (nx < 1 || nx > gs->board_cols || ny < 1 || ny > gs->board_rows)
        return 1;

    /* Self / player body */
    if (snake_contains(&gs->bot,   nx, ny)) return 1;
    if (snake_contains(&gs->snake, nx, ny)) return 1;

    /* Apple? */
    *ate_idx = apple_at(gs, nx, ny);
    if (*ate_idx >= 0) {
        /* Grow: push head, don't pop tail */
        snake_push_head(&gs->bot, nx, ny, 0);
        gs->bot.score++;
        return 2;
    }

    /* Normal move */
    old_tail = gs->bot.tail->pos;

    /* Erase old tail cell — but only if the player snake doesn't occupy it */
    if (!snake_contains(&gs->snake, old_tail.x, old_tail.y))
        mvwaddch(gs->game, old_tail.y, old_tail.x * 2, ' ');

    snake_pop_tail(&gs->bot);
    snake_push_head(&gs->bot, nx, ny, 0);
    return 0;
}

/* Draw the entire bot snake (used on initial placement and after pause). */
static void bot_draw_all(const GameState *gs)
{
    const SnakeNode *cur = gs->bot.head;
    int is_head = 1;
    while (cur) {
        if (gs->has_color) wattron(gs->game, COLOR_PAIR(CP_OPP_SNAKE) | A_BOLD);
        mvwaddch(gs->game, cur->pos.y, cur->pos.x * 2,
                 is_head ? (chtype)head_char(gs->bot.dir) : (chtype)'+');
        if (gs->has_color) wattroff(gs->game, COLOR_PAIR(CP_OPP_SNAKE) | A_BOLD);
        is_head = 0;
        cur = cur->next;
    }
}

/* Draw the bot's new head after a step. */
static void bot_draw_head(const GameState *gs)
{
    if (!gs->bot.head) return;
    if (gs->has_color) wattron(gs->game, COLOR_PAIR(CP_OPP_SNAKE) | A_BOLD);
    mvwaddch(gs->game, gs->bot.head->pos.y, gs->bot.head->pos.x * 2,
             (chtype)head_char(gs->bot.dir));
    if (gs->has_color) wattroff(gs->game, COLOR_PAIR(CP_OPP_SNAKE) | A_BOLD);
}

/* Place the bot starting in the bottom-right corner, facing left. */
static void bot_place_initial(GameState *gs)
{
    int x;
    int start_x = gs->board_cols;
    int start_y = gs->board_rows;
    int len     = gs->init_len;

    snake_free(&gs->bot);
    snake_init(&gs->bot);

    /*
     * Head at (board_cols, board_rows), tail extends rightward off-screen?
     * No: head at rightmost, body leftward.  push_tail right-to-left
     * identical logic to player but mirrored at bottom-right corner.
     *
     * We want: head = (start_x, start_y)  facing LEFT
     *          tail = (start_x - len + 1, start_y)
     *
     * push_tail iterates left-to-right so head ends up at the LAST push.
     * Iterate from (start_x - len + 1) to start_x, leftward first:
     */
    for (x = start_x - len + 1; x <= start_x; x++) {
        int px = x;
        /* Clamp in case board is smaller than init_len */
        if (px < 1) px = 1;
        snake_push_tail(&gs->bot, px, start_y, 0);
    }

    gs->bot.dir      = DIR_LEFT;
    gs->bot.prev_dir = DIR_LEFT;
    gs->bot.score    = 0;
    gs->bot_alive    = 1;
}

/* ═══════════════════════════════════════════════════════════════
 * Game-over
 * ═══════════════════════════════════════════════════════════════ */

static void game_over(GameState *gs, const char *reason)
{
    int points = (gs->snake.score * 10) -
                 ((int)time(NULL) - gs->snake.start_time);
    beep();

    if (points > 0) {
        const char *name = getenv("LOGNAME");
        if (!name) name = "player";
        score_write(name, points);
    }

    score_show_ncurses(10, gs->has_color);

    endwin();
    if (gs->bot_enabled)
        fprintf(stdout,
                "Player: %d apples   Bot: %d apples\n",
                gs->snake.score, gs->bot.score);
    fprintf(stdout,
            "-=-=-=-=-=-=] Game Over !!! (%s) [=-=-=-=-=-=-\n", reason);
    exit(EXIT_SUCCESS);
}

/* ═══════════════════════════════════════════════════════════════
 * Setup helpers
 * ═══════════════════════════════════════════════════════════════ */

static void show_title(int has_color)
{
    if (has_color) attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvaddstr(1, 1, "__  __     _      _");
    mvaddstr(2, 1, " \\ \\/ /   _| | ___| |__  _ __ __ _");
    mvaddstr(3, 1, "  \\  / | | | |/ _ \\ '_ \\| '__/ _` |");
    mvaddstr(4, 1, "  /  \\ |_| | |  __/ |_) | | | (_| |");
    mvaddstr(5, 1, " /_/\\_\\__,_|_|\\___|_.__/|_|  \\__,_|");
    if (has_color) attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvaddstr(7, 1, "Press any key to start...");
    refresh();
}

static void draw_borders(const GameState *gs)
{
    if (gs->has_color) {
        wattron(gs->game, COLOR_PAIR(CP_BORDER) | A_BOLD);
        wattron(gs->info, COLOR_PAIR(CP_BORDER) | A_BOLD);
    }
    wborder(gs->game, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
    wborder(gs->info, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
    if (gs->has_color) {
        wattroff(gs->game, COLOR_PAIR(CP_BORDER) | A_BOLD);
        wattroff(gs->info, COLOR_PAIR(CP_BORDER) | A_BOLD);
    }
}

static void snake_draw_all(const GameState *gs)
{
    const SnakeNode *cur = gs->snake.head;
    int is_head = 1;
    while (cur) {
        if (is_head) {
            if (gs->has_color) wattron(gs->game, COLOR_PAIR(CP_SNAKE_HEAD) | A_BOLD);
            mvwaddch(gs->game, cur->pos.y, cur->pos.x * 2,
                     (chtype)head_char(gs->snake.dir));
            if (gs->has_color) wattroff(gs->game, COLOR_PAIR(CP_SNAKE_HEAD) | A_BOLD);
            is_head = 0;
        } else {
            if (gs->has_color) wattron(gs->game, COLOR_PAIR(CP_SNAKE_BODY));
            mvwaddch(gs->game, cur->pos.y, cur->pos.x * 2, '*');
            if (gs->has_color) wattroff(gs->game, COLOR_PAIR(CP_SNAKE_BODY));
        }
        cur = cur->next;
    }
}

static void snake_place_initial(GameState *gs)
{
    int x;
    int start = BOARD_ORIGIN_X + gs->init_len - 1;
    for (x = start; x >= BOARD_ORIGIN_X; x--)
        snake_push_tail(&gs->snake, x, BOARD_ORIGIN_Y, 0);
    gs->snake.dir        = DIR_RIGHT;
    gs->snake.prev_dir   = DIR_RIGHT;
    gs->snake.score      = 0;
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
    int       apple_idx;
    int       bot_apple_idx;
    int       board_cols = BOARD_COLS;
    int       board_rows = BOARD_ROWS;

    memset(&gs, 0, sizeof(gs));
    snake_init(&gs.snake);
    snake_init(&gs.bot);

    /* Defaults */
    gs.speed_level  = SPEED_LEVEL_DEF;
    gs.n_apples     = 1;
    gs.init_len     = SNAKE_START_LEN;
    gs.auto_speed_n = AUTO_SPEED_OFF;
    gs.wrap         = 0;
    gs.bot_enabled  = 0;
    gs.bot_alive    = 0;

    if (parse_args(argc, argv, &gs, &board_cols, &board_rows) < 0)
        return;

    /* ncurses */
    initscr();
    nonl();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    gs.has_color = has_colors();
    if (gs.has_color) { start_color(); colors_init(); }

    if (validate_board_fits(&board_cols, &board_rows) < 0) { endwin(); return; }

    gs.board_cols = board_cols;
    gs.board_rows = board_rows;

    show_title(gs.has_color);
    getch();

    gs.game = newwin(gs.board_rows + 2, (gs.board_cols * 2) + 2, 0, 0);
    gs.info = newwin(gs.board_rows + 2,
                     COLS - (gs.board_cols * 2) - 2,
                     0, (gs.board_cols * 2) + 2);

    if (!gs.game || !gs.info) {
        endwin();
        fprintf(stderr, "one_player: could not create windows\n");
        return;
    }

    keypad(gs.game, TRUE);
    apply_speed(&gs);

    snake_place_initial(&gs);
    if (gs.bot_enabled) {
        bot_place_initial(&gs);
    }

    draw_borders(&gs);
    snake_draw_all(&gs);
    if (gs.bot_enabled) bot_draw_all(&gs);
    info_init(&gs);
    place_all_apples(&gs);
    REFRESH(gs.game);

    /* ── Game loop ──────────────────────────────────────────── */
    for (;;) {
        key = wgetch(gs.game);

        if (key == KEY_QUIT_Q || key == KEY_QUIT_ESC)
            game_over(&gs, "Quit by player");

        /* ── Player move ───────────────────────────────────── */
        switch (classify_move(&gs, key, &next, &apple_idx)) {
        case 0: move_normal(&gs, &next);            break;
        case 1: game_over(&gs, "You bit yourself"); break;
        case 2: game_over(&gs, "Out of bounds");    break;
        case 3: move_eat_apple(&gs, &next, apple_idx); break;
        }

        /* ── Bot move ──────────────────────────────────────── */
        if (gs.bot_enabled && gs.bot_alive) {
            int bot_result = bot_step(&gs, &bot_apple_idx);

            if (bot_result == 1) {
                /* Bot collided — erase its body, mark dead */
                SnakeNode *cur = gs.bot.head;
                while (cur) {
                    mvwaddch(gs.game, cur->pos.y, cur->pos.x * 2, ' ');
                    cur = cur->next;
                }
                snake_free(&gs.bot);
                gs.bot_alive = 0;

                /* Show "DEAD" on the info panel */
                if (gs.has_color) wattron(gs.info, COLOR_PAIR(CP_GAMEOVER) | A_BOLD);
                mvwaddstr(gs.info, INFO_ROW_BOT, 1, "Bot:    DEAD");
                if (gs.has_color) wattroff(gs.info, COLOR_PAIR(CP_GAMEOVER) | A_BOLD);
                REFRESH(gs.info);

            } else if (bot_result == 2) {
                /* Bot ate an apple: redraw the apple slot, update score */
                place_apple_slot(&gs, bot_apple_idx);
                draw_apple(&gs, bot_apple_idx);
                info_update_int(&gs, INFO_ROW_BOT_SCORE, 9, gs.bot.score);
                bot_draw_head(&gs);
            } else {
                /* Normal bot move: draw new head */
                bot_draw_head(&gs);
            }
        }

        /* ── Info updates ──────────────────────────────────── */
        info_update_int(&gs, INFO_ROW_TIME, 9,
                        (int)time(NULL) - gs.snake.start_time);
        info_update_int(&gs, INFO_ROW_POINTS, 9,
                        (gs.snake.score * 10) -
                        ((int)time(NULL) - gs.snake.start_time));

        REFRESH(gs.game);
    }
}