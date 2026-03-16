/*
 * client.c
 * Network client for the two-player snake game.
 *
 * Changes from previous version
 * ──────────────────────────────
 * • CSnake/CNode removed — the client now uses the shared Snake ADT from
 *   structs.h (snake_push_head, snake_pop_tail, snake_contains, snake_free).
 *   This eliminates ~60 lines of duplicated linked-list code.
 * • LEVEL_TO_MS / SPEED_LEVEL_* macros removed — imported from defines.h.
 * • Multi-apple support: the client handles MSG_NEW_APPLE for extra apples
 *   sent by the server on MSG_GROW_DONE and on apple regeneration.
 * • srandom() not called here — seeded once by score_seed() in main().
 * • SIGPIPE is ignored in server_run(); client write errors are still
 *   checked and result in a clean exit.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curses.h>

#include "defines.h"
#include "structs.h"
#include "xulnet.h"
#include "socket.h"
#include "colors.h"

/* ── Client-specific layout ─────────────────────────────────── */
#define P1_START_X    1
#define P1_START_Y    1
#define P2_START_X   30
#define P2_START_Y   20
#define INIT_LEN     10
#define MAX_APPLES    9

/* ═══════════════════════════════════════════════════════════════
 * Context object
 * ═══════════════════════════════════════════════════════════════ */

typedef struct {
    Snake   snake;             /* Shared ADT — no more CSnake/CNode    */
    int     sock;
    int     player;
    char    my_login[LOGIN_LEN];
    char    opp_login[LOGIN_LEN];
    int     apples[2];         /* Score counters for display           */
    int     apple_x[MAX_APPLES];
    int     apple_y[MAX_APPLES];
    int     n_apples;          /* How many apple slots are tracked     */
    int     port;
    char    host[256];
    int     board_cols;
    int     board_rows;
    int     speed_level;
    int     has_color;
    char    packet[PACKET_SIZE];
    WINDOW *game;
    WINDOW *info;
} ClientState;

static void client_state_init(ClientState *cs)
{
    int i;
    memset(cs, 0, sizeof(*cs));
    snake_init(&cs->snake);
    cs->sock        = -1;
    cs->port        = DEFAULT_PORT;
    cs->board_cols  = NET_BOARD_COLS;
    cs->board_rows  = NET_BOARD_ROWS;
    cs->speed_level = SPEED_LEVEL_DEF;
    cs->n_apples    = 0;
    for (i = 0; i < MAX_APPLES; i++) { cs->apple_x[i] = -1; cs->apple_y[i] = -1; }
    strncpy(cs->host, DEFAULT_HOST, sizeof(cs->host) - 1);
}

/* ── Apple slot helpers ─────────────────────────────────────── */

static int apple_slot_add(ClientState *cs, int x, int y)
{
    int i;
    /* Replace an empty slot first */
    for (i = 0; i < MAX_APPLES; i++) {
        if (cs->apple_x[i] < 0) {
            cs->apple_x[i] = x;
            cs->apple_y[i] = y;
            if (i >= cs->n_apples) cs->n_apples = i + 1;
            return i;
        }
    }
    return -1;   /* table full */
}

static int apple_find(const ClientState *cs, int x, int y)
{
    int i;
    for (i = 0; i < cs->n_apples; i++) {
        if (cs->apple_x[i] == x && cs->apple_y[i] == y) return i;
    }
    return -1;
}

static void apple_clear_slot(ClientState *cs, int idx)
{
    cs->apple_x[idx] = -1;
    cs->apple_y[idx] = -1;
}

/* ═══════════════════════════════════════════════════════════════
 * Colored drawing helpers
 * ═══════════════════════════════════════════════════════════════ */

static void draw_my_cell(const ClientState *cs, int y, int x,
                          chtype ch, int is_head)
{
    int pair = is_head ? CP_SNAKE_HEAD : CP_SNAKE_BODY;
    int attr = is_head ? (COLOR_PAIR(pair) | A_BOLD) : COLOR_PAIR(pair);
    if (cs->has_color) wattron(cs->game, attr);
    mvwaddch(cs->game, y, x * 2, ch);
    if (cs->has_color) wattroff(cs->game, attr);
}

static void draw_opp_cell(const ClientState *cs, int y, int x, chtype ch)
{
    if (cs->has_color) wattron(cs->game, COLOR_PAIR(CP_OPP_SNAKE) | A_BOLD);
    mvwaddch(cs->game, y, x * 2, ch);
    if (cs->has_color) wattroff(cs->game, COLOR_PAIR(CP_OPP_SNAKE) | A_BOLD);
}

static void draw_apple_cell(const ClientState *cs, int y, int x, chtype ch)
{
    if (cs->has_color) wattron(cs->game, COLOR_PAIR(CP_APPLE) | A_BOLD);
    mvwaddch(cs->game, y, x * 2, ch);
    if (cs->has_color) wattroff(cs->game, COLOR_PAIR(CP_APPLE) | A_BOLD);
}

static void draw_all_apples(const ClientState *cs)
{
    int i;
    for (i = 0; i < cs->n_apples; i++) {
        if (cs->apple_x[i] >= 0)
            draw_apple_cell(cs, cs->apple_y[i], cs->apple_x[i], '@');
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Info sidebar
 * ═══════════════════════════════════════════════════════════════ */

static void info_label(const ClientState *cs, int row, int col, const char *s)
{
    if (cs->has_color) wattron(cs->info, COLOR_PAIR(CP_INFO_LABEL));
    mvwaddstr(cs->info, row, col, s);
    if (cs->has_color) wattroff(cs->info, COLOR_PAIR(CP_INFO_LABEL));
}

static void info_value(const ClientState *cs, int row, int col, const char *s)
{
    if (cs->has_color) wattron(cs->info, COLOR_PAIR(CP_INFO_VALUE) | A_BOLD);
    mvwaddstr(cs->info, row, col, s);
    if (cs->has_color) wattroff(cs->info, COLOR_PAIR(CP_INFO_VALUE) | A_BOLD);
}

static void update_score(ClientState *cs, int ate_player)
{
    char buf[8];
    int  scorer = (ate_player == 0) ? cs->player : (1 - cs->player);
    cs->apples[scorer]++;
    snprintf(buf, sizeof(buf), "%d", cs->apples[scorer]);
    info_value(cs, 6 + scorer, 5, buf);
    REFRESH(cs->info);
}

static void init_info(ClientState *cs)
{
    info_label(cs, 1, 1, "Player 1:");
    info_label(cs, 3, 1, "Player 2:");
    info_value(cs, 2 + (cs->player * 2), 1, cs->my_login);
    info_value(cs, 4 - (cs->player * 2), 1, cs->opp_login);
    info_label(cs, 6, 1, "P1: ");
    info_value(cs, 6, 5, "0");
    info_label(cs, 7, 1, "P2: ");
    info_value(cs, 7, 5, "0");
    REFRESH(cs->info);
}

/* ═══════════════════════════════════════════════════════════════
 * Logo, borders, game-over
 * ═══════════════════════════════════════════════════════════════ */

static void show_logo(int has_color)
{
    if (has_color) attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvaddstr(1, 1, "__  __     _      _");
    mvaddstr(2, 1, "\\ \\/ /   _| | ___| |__  _ __ __ _");
    mvaddstr(3, 1, " \\  / | | | |/ _ \\ '_ \\| '__/ _` |");
    mvaddstr(4, 1, " /  \\ |_| | |  __/ |_) | | | (_| |");
    mvaddstr(5, 1, "/_/\\_\\__,_|_|\\___|_.__/|_|  \\__,_|");
    if (has_color) attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
    REFRESH(stdscr);
}

static void draw_borders(const ClientState *cs)
{
    if (cs->has_color) {
        wattron(cs->game, COLOR_PAIR(CP_BORDER) | A_BOLD);
        wattron(cs->info, COLOR_PAIR(CP_BORDER) | A_BOLD);
    }
    wborder(cs->game, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
    wborder(cs->info, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
    if (cs->has_color) {
        wattroff(cs->game, COLOR_PAIR(CP_BORDER) | A_BOLD);
        wattroff(cs->info, COLOR_PAIR(CP_BORDER) | A_BOLD);
    }
}

static void force_exit(const ClientState *cs, const char *reason)
{
    if (cs->has_color) attron(COLOR_PAIR(CP_GAMEOVER) | A_BOLD);
    mvaddstr(12, 1, " /-------------               ");
    mvaddstr(13, 1, "| GAME OVER --> ");
    mvaddstr(14, 1, " \\-------------               ");
    mvaddstr(13, 18, reason);
    if (cs->has_color) attroff(COLOR_PAIR(CP_GAMEOVER) | A_BOLD);
    REFRESH(stdscr);
    sleep(2);
    endwin();
    exit(EXIT_SUCCESS);
}

/* ═══════════════════════════════════════════════════════════════
 * Socket / handshake
 * ═══════════════════════════════════════════════════════════════ */

static int connect_to_server(ClientState *cs)
{
    char buf[64];
    const char *env;

    cs->sock = net_connect(cs->host, cs->port);
    if (cs->sock < 0) {
        mvaddstr(7, 1, "Connection failed");
        REFRESH(stdscr);
        sleep(1);
        endwin();
        return -1;
    }

    env = getenv("IRCUSER");
    if (!env) env = getenv("LOGNAME");
    if (!env) env = "player";
    strncpy(cs->my_login, env, LOGIN_LEN - 1);
    write(cs->sock, cs->my_login, LOGIN_LEN);

    snprintf(buf, sizeof(buf), "Socket: %d", cs->sock);
    mvaddstr(8, 1, buf);
    sleep(1);

    read(cs->sock, &cs->player, sizeof(int));
    snprintf(buf, sizeof(buf), "You are player %d", cs->player + 1);
    mvaddstr(9, 1, buf);
    REFRESH(stdscr);

    read(cs->sock, cs->opp_login, LOGIN_LEN);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Snake initialisation (grow phase)
 * ═══════════════════════════════════════════════════════════════ */

static void init_snake(ClientState *cs)
{
    int i, start_x, start_y, dir;

    if (cs->player == 0) {
        start_x = P1_START_X; start_y = P1_START_Y; dir = DIR_RIGHT;
    } else {
        start_x = P2_START_X; start_y = P2_START_Y; dir = DIR_LEFT;
    }

    snake_free(&cs->snake);
    snake_init(&cs->snake);
    cs->snake.dir = dir;

    for (i = 0; i < INIT_LEN; i++) {
        int x = (dir == DIR_RIGHT) ? start_x + i : start_x - i;

        /* Client: push_tail so head is the last-pushed (leading) cell */
        snake_push_tail(&cs->snake, x, start_y, 0);

        cs->packet[0] = MSG_GROW;
        cs->packet[1] = (char)x;
        cs->packet[2] = (char)start_y;
        write(cs->sock, cs->packet, PACKET_SIZE);

        read(cs->sock, cs->packet, PACKET_SIZE);
        draw_opp_cell(cs, cs->packet[2], cs->packet[1], '+');
    }

    /* Draw my snake: head = last element pushed (tail of the list).
       The client list is tail-first (oldest at head, newest at tail)
       because we used push_tail during grow.  Head is the snake's
       visual front = cs->snake.tail.                                 */
    {
        const SnakeNode *cur = cs->snake.head;
        while (cur) {
            draw_my_cell(cs, cur->pos.y, cur->pos.x, '*', cur == cs->snake.tail);
            cur = cur->next;
        }
    }
    REFRESH(cs->game);
}

/* ═══════════════════════════════════════════════════════════════
 * Move classification
 *   0 normal  1 wall  2 self-bite  3 apple
 * ═══════════════════════════════════════════════════════════════ */

static int classify_move(ClientState *cs, int key, int *nx, int *ny)
{
    int fx, fy;

    /* Reject reversal */
    if ((key == DIR_UP    && cs->snake.dir == DIR_DOWN)  ||
        (key == DIR_DOWN  && cs->snake.dir == DIR_UP)    ||
        (key == DIR_LEFT  && cs->snake.dir == DIR_RIGHT) ||
        (key == DIR_RIGHT && cs->snake.dir == DIR_LEFT))
        key = KEY_NONE;

    if (key != DIR_UP && key != DIR_DOWN &&
        key != DIR_LEFT && key != DIR_RIGHT)
        key = cs->snake.dir;

    cs->snake.dir = key;

    /* Project from the visual front = tail of the ADT list (grow used push_tail) */
    fx = cs->snake.tail->pos.x;
    fy = cs->snake.tail->pos.y;

    switch (key) {
    case DIR_UP:    fy--; break;
    case DIR_DOWN:  fy++; break;
    case DIR_LEFT:  fx--; break;
    default:        fx++; break;
    }

    *nx = fx; *ny = fy;

    if (fx < 1 || fx > cs->board_cols || fy < 1 || fy > cs->board_rows)
        return 1;

    if (snake_contains(&cs->snake, fx, fy))
        return 2;

    if (apple_find(cs, fx, fy) >= 0)
        return 3;

    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Move actions
 * ═══════════════════════════════════════════════════════════════ */

static void do_move(ClientState *cs, int fx, int fy)
{
    /* In the client list (push_tail during grow), the visual front is
       the tail.  For movement we push_tail the new head and pop_head
       (the oldest / rearmost cell).  We reuse snake_push_tail and
       implement a custom pop_head to match this orientation.         */
    int old_x = cs->snake.head->pos.x;
    int old_y = cs->snake.head->pos.y;

    /* Remove oldest segment (head of ADT list = visual rear) */
    {
        SnakeNode *old = cs->snake.head;
        cs->snake.head = old->next;
        if (cs->snake.head) cs->snake.head->prev = NULL;
        else                cs->snake.tail = NULL;
        free(old);
        cs->snake.length--;
    }

    /* Append new front */
    snake_push_tail(&cs->snake, fx, fy, 0);

    draw_my_cell(cs, fy, fx, '*', 1);   /* new head */
    mvwaddch(cs->game, old_y, old_x * 2, ' ');

    cs->packet[0] = MSG_MOVE;
    cs->packet[1] = (char)fx;   cs->packet[2] = (char)fy;
    cs->packet[3] = (char)old_x; cs->packet[4] = (char)old_y;
    write(cs->sock, cs->packet, PACKET_SIZE);
}

static void do_wall_collision(ClientState *cs, int fx, int fy)
{
    mvwaddch(cs->game, fy, fx * 2, '#');
    cs->packet[0] = MSG_SELF_COLLIDE;
    cs->packet[1] = (char)fx;
    cs->packet[2] = (char)fy;
    write(cs->sock, cs->packet, PACKET_SIZE);
}

static void do_self_bite(ClientState *cs, int fx, int fy)
{
    cs->packet[0] = MSG_SELF_BITE;
    cs->packet[1] = (char)fx;
    cs->packet[2] = (char)fy;
    write(cs->sock, cs->packet, PACKET_SIZE);
}

static void do_eat_apple(ClientState *cs, int fx, int fy)
{
    int idx = apple_find(cs, fx, fy);

    /* Grow: append without removing tail */
    snake_push_tail(&cs->snake, fx, fy, 0);
    draw_my_cell(cs, fy, fx, '*', 1);

    cs->packet[0] = MSG_ATE_APPLE;
    cs->packet[1] = (char)fx;
    cs->packet[2] = (char)fy;
    write(cs->sock, cs->packet, PACKET_SIZE);

    /* Erase old apple, record slot empty */
    if (idx >= 0) {
        apple_clear_slot(cs, idx);
    }

    /* Server replies with MSG_NEW_APPLE for the replacement */
    read(cs->sock, cs->packet, PACKET_SIZE);
    if ((unsigned char)cs->packet[0] == MSG_NEW_APPLE) {
        int nx = (unsigned char)cs->packet[1];
        int ny = (unsigned char)cs->packet[2];
        apple_slot_add(cs, nx, ny);
        draw_apple_cell(cs, ny, nx, '@');
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Argument parsing
 * ═══════════════════════════════════════════════════════════════ */

static int parse_args(ClientState *cs, int argc, char *argv[])
{
    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-') continue;
        switch (argv[i][1]) {
        case 'h':
            if (i+1<argc) { strncpy(cs->host, argv[++i], sizeof(cs->host)-1); }
            else { fprintf(stderr,"client: -h needs hostname\n"); return -1; }
            break;
        case 'p':
            if (i+1<argc) {
                cs->port = atoi(argv[++i]);
                if (cs->port <= 1024) { fprintf(stderr,"client: port > 1024\n"); return -1; }
            } else { fprintf(stderr,"client: -p needs a port\n"); return -1; }
            break;
        case 'W':
            if (i+1<argc) {
                cs->board_cols = atoi(argv[++i]);
                if (cs->board_cols < BOARD_COLS_MIN) { fprintf(stderr,"client: -W >= %d\n",BOARD_COLS_MIN); return -1; }
            } else { fprintf(stderr,"client: -W needs a value\n"); return -1; }
            break;
        case 'H':
            if (i+1<argc) {
                cs->board_rows = atoi(argv[++i]);
                if (cs->board_rows < BOARD_ROWS_MIN) { fprintf(stderr,"client: -H >= %d\n",BOARD_ROWS_MIN); return -1; }
            } else { fprintf(stderr,"client: -H needs a value\n"); return -1; }
            break;
        case 'S':
            if (i+1<argc) {
                cs->speed_level = atoi(argv[++i]);
                if (cs->speed_level < SPEED_LEVEL_MIN || cs->speed_level > SPEED_LEVEL_MAX) {
                    fprintf(stderr,"client: -S must be %d-%d\n", SPEED_LEVEL_MIN, SPEED_LEVEL_MAX);
                    return -1;
                }
            } else { fprintf(stderr,"client: -S needs a value\n"); return -1; }
            break;
        default:
            fprintf(stderr,"client: unknown option -%c\n", argv[i][1]);
            break;
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Public entry point
 * ═══════════════════════════════════════════════════════════════ */

void client_run(int argc, char *argv[])
{
    ClientState cs;
    int         key, fx, fy;

    client_state_init(&cs);

    if (parse_args(&cs, argc, argv) < 0)
        return;

    initscr();
    nonl();
    noecho();
    cbreak();

    cs.has_color = has_colors();
    if (cs.has_color) { start_color(); colors_init(); }

    cs.game = newwin(cs.board_rows + 2, (cs.board_cols * 2) + 2, 1, 0);
    cs.info = newwin(cs.board_rows + 2,
                     COLS - (cs.board_cols * 2) - 2,
                     1, (cs.board_cols * 2) + 2);

    if (!cs.info || !cs.game) {
        endwin();
        fprintf(stderr, "client: could not create windows\n");
        return;
    }

    show_logo(cs.has_color);

    if (connect_to_server(&cs) < 0) return;

    mvaddstr(0, 3, "Xulebra v0.2");
    REFRESH(stdscr);

    draw_borders(&cs);
    init_snake(&cs);
    init_info(&cs);

    /* Signal server; receive all initial apple positions */
    cs.packet[0] = MSG_GROW_DONE;
    write(cs.sock, cs.packet, PACKET_SIZE);

    /* First apple comes back as MSG_SET_APPLE; additional ones as MSG_NEW_APPLE */
    read(cs.sock, cs.packet, PACKET_SIZE);
    if ((unsigned char)cs.packet[0] == MSG_SET_APPLE) {
        int ax = (unsigned char)cs.packet[1];
        int ay = (unsigned char)cs.packet[2];
        apple_slot_add(&cs, ax, ay);
        draw_apple_cell(&cs, ay, ax, '@');
    }
    /* Read any extra apples (multi-apple mode) */
    {
        int saved_timeout;
        /* Temporarily non-blocking to drain extra apple packets */
        wtimeout(cs.game, 0);
        while (1) {
            int r = read(cs.sock, cs.packet, PACKET_SIZE);
            if (r <= 0) break;
            if ((unsigned char)cs.packet[0] == MSG_NEW_APPLE) {
                int ax = (unsigned char)cs.packet[1];
                int ay = (unsigned char)cs.packet[2];
                apple_slot_add(&cs, ax, ay);
                draw_apple_cell(&cs, ay, ax, '@');
            } else break;
        }
        (void)saved_timeout;
    }

    keypad(cs.game, TRUE);
    wtimeout(cs.game, LEVEL_TO_MS(cs.speed_level));

    REFRESH(cs.game);
    REFRESH(cs.info);

    for (;;) {
        key = wgetch(cs.game);

        /* Quit keys — notify server then exit cleanly */
        if (key == KEY_QUIT_Q || key == KEY_QUIT_ESC) {
            cs.packet[0] = MSG_SELF_COLLIDE;
            cs.packet[1] = 0; cs.packet[2] = 0;
            write(cs.sock, cs.packet, PACKET_SIZE);
            force_exit(&cs, "Quit by player");
        }

        switch (classify_move(&cs, key, &fx, &fy)) {
        case 0: do_move(&cs, fx, fy);                              break;
        case 1: do_wall_collision(&cs, fx, fy);
                force_exit(&cs, "You crashed into the wall");      break;
        case 2: do_self_bite(&cs, fx, fy);
                force_exit(&cs, "You bit yourself");               break;
        case 3: do_eat_apple(&cs, fx, fy);
                update_score(&cs, 0);                              break;
        }

        read(cs.sock, cs.packet, PACKET_SIZE);
        switch ((unsigned char)cs.packet[0]) {
        case MSG_RELAY_MOVE:
            draw_opp_cell(&cs, cs.packet[2], cs.packet[1], '+');
            mvwaddch(cs.game, cs.packet[4], cs.packet[3] * 2, ' ');
            break;
        case MSG_RELAY_GROW:
            draw_opp_cell(&cs, cs.packet[2], cs.packet[1], '+');
            break;
        case MSG_OPP_COLLIDE:
            mvwaddch(cs.game, cs.packet[2], cs.packet[1] * 2, '#');
            force_exit(&cs, "Opponent crashed into the wall");
            break;
        case MSG_OPP_BITE:
            force_exit(&cs, "Opponent bit themselves");
            break;
        case MSG_OPP_BIT_ME:
            force_exit(&cs, "Opponent crashed into you");
            break;
        case MSG_I_BIT_OPP:
            force_exit(&cs, "You crashed into the opponent");
            break;
        case MSG_DRAW:
            mvwaddch(cs.game, cs.packet[2], cs.packet[1] * 2, '#');
            force_exit(&cs, "Draw match");
            break;
        case MSG_NEW_APPLE: {
            update_score(&cs, 1);
            int ax = (unsigned char)cs.packet[1];
            int ay = (unsigned char)cs.packet[2];
            /* Clear old position if tracked, then add new */
            {
                int old_idx = apple_find(&cs, ax, ay);
                if (old_idx >= 0) apple_clear_slot(&cs, old_idx);
            }
            apple_slot_add(&cs, ax, ay);
            draw_apple_cell(&cs, ay, ax, '@');
            break;
        }
        default:
            break;
        }

        REFRESH(cs.game);
    }

    endwin();
}