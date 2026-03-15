/*
 * client.c
 * Network client for the two-player snake game.
 *
 * All mutable state lives in ClientState, allocated on the stack of
 * client_run() — no file-scope variables are used anywhere in this file.
 *
 * Protocol summary (see xulnet.h for full message list)
 * ──────────────────────────────────────────────────────
 * Handshake
 *   client → server : login name (LOGIN_LEN bytes)
 *   server → client : player index (int: 0 or 1)
 *   server → client : opponent login name (LOGIN_LEN bytes)
 *
 * Init phase (LEN grow steps, one per segment)
 *   client → server : MSG_GROW  [x, y, -, -]
 *   server → client : MSG_RELAY_GROW [x, y, -, -]  (opponent's grow echo)
 *
 * Ready signal
 *   client → server : MSG_GROW_DONE
 *   server → client : MSG_SET_APPLE [x, y, -, -]
 *
 * Game loop (per frame)
 *   client → server : one of MSG_MOVE / MSG_SELF_COLLIDE / MSG_SELF_BITE
 *                     / MSG_ATE_APPLE
 *   server → client : relabelled opponent packet
 *
 * Bugs fixed from original
 * ────────────────────────
 * • argv[key][0] = '-'  was assignment, not comparison; caused every
 *   argument to be treated as a flag.
 * • The back-direction guard in multi_move used abs(a - snake.dir) == 1
 *   which is wrong for the KEY_* values from ncurses (they are large
 *   negative integers, not 1-4). Replaced with explicit pair checks.
 * • multi_move used goto; replaced with a do-while loop.
 * • UPDATE(x) macro expanded to two statements without braces, which is
 *   unsafe inside if/else; replaced with the REFRESH(x) macro that uses
 *   do { } while(0).
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curses.h>

#include "defines.h"
#include "xulnet.h"
#include "socket.h"

/* ── Client-specific layout constants ──────────────────────── */
#define P1_START_X   1
#define P1_START_Y   1
#define P2_START_X  30
#define P2_START_Y  20
#define INIT_LEN    10   /* segments grown during the init phase */

/* ── Simple linked list used by the client ──────────────────── */
typedef struct CNode {
    int x, y;
    struct CNode *next;
} CNode;

typedef struct {
    CNode *head;
    CNode *tail;
    int    dir;
} CSnake;

/* ═══════════════════════════════════════════════════════════════
 * Context object  –  owns ALL mutable state
 * ═══════════════════════════════════════════════════════════════ */

typedef struct {
    CSnake  snake;
    int     sock;
    int     player;             /* 0 = player 1, 1 = player 2        */
    char    my_login[LOGIN_LEN];
    char    opp_login[LOGIN_LEN];
    int     apples[2];          /* scores: [0]=P1, [1]=P2            */
    int     apple_x, apple_y;
    int     port;
    char    host[256];
    int     board_cols;         /* Playable columns (from -W)  */
    int     board_rows;         /* Playable rows    (from -H)  */
    int     init_speed;         /* Frame interval ms (from -S) */
    char    packet[PACKET_SIZE];
    WINDOW *game;
    WINDOW *info;
} ClientState;

static void client_state_init(ClientState *cs)
{
    memset(cs, 0, sizeof(*cs));
    cs->sock        = -1;
    cs->port        = DEFAULT_PORT;
    cs->board_cols  = NET_BOARD_COLS;
    cs->board_rows  = NET_BOARD_ROWS;
    cs->init_speed  = 60;
    strncpy(cs->host, DEFAULT_HOST, sizeof(cs->host) - 1);
}

/* ═══════════════════════════════════════════════════════════════
 * Snake list operations
 * ═══════════════════════════════════════════════════════════════ */

static void cs_add_block(ClientState *cs, int x, int y)
{
    CNode *n = (CNode *)malloc(sizeof(CNode));
    n->x = x; n->y = y; n->next = NULL;
    if (!cs->snake.head) cs->snake.head = n;
    if  (cs->snake.tail) cs->snake.tail->next = n;
    cs->snake.tail = n;
}

/* Remove head block and return its coordinates */
static CNode cs_pop_head(ClientState *cs)
{
    CNode val     = *cs->snake.head;
    CNode *trash  = cs->snake.head;
    cs->snake.head = cs->snake.head->next;
    if (!cs->snake.head) cs->snake.tail = NULL;
    free(trash);
    val.next = NULL;
    return val;
}

static int cs_contains(const ClientState *cs, int x, int y)
{
    const CNode *n = cs->snake.head;
    while (n) {
        if (n->x == x && n->y == y) return 1;
        n = n->next;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Drawing helpers
 * ═══════════════════════════════════════════════════════════════ */

static void draw_snake(const ClientState *cs)
{
    const CNode *n = cs->snake.head;
    while (n) {
        mvwaddch(cs->game, n->y, n->x * 2, '*');
        n = n->next;
    }
    REFRESH(cs->game);
}

static void update_info(ClientState *cs, int ate_player)
{
    /* ate_player: 0 = I ate, 1 = opponent ate */
    char buf[8];
    int  scorer;

    /* Map who ate to which absolute player slot */
    if (ate_player == 0)
        scorer = cs->player;       /* I am player cs->player */
    else
        scorer = 1 - cs->player;   /* opponent is the other slot */

    cs->apples[scorer]++;
    snprintf(buf, sizeof(buf), "%d", cs->apples[scorer]);
    mvwaddstr(cs->info, 6 + scorer, 5, buf);
    REFRESH(cs->info);
}

static void init_info(ClientState *cs)
{
    mvwaddstr(cs->info, 1, 1, "Player 1:");
    mvwaddstr(cs->info, 3, 1, "Player 2:");

    /* My login appears on my row, opponent's on theirs */
    mvwaddstr(cs->info, 2 + (cs->player * 2), 1, cs->my_login);
    mvwaddstr(cs->info, 4 - (cs->player * 2), 1, cs->opp_login);

    mvwaddstr(cs->info, 6, 1, "P1: 0");
    mvwaddstr(cs->info, 7, 1, "P2: 0");

    REFRESH(cs->info);
}

static void show_logo(void)
{
    mvaddstr(1, 1, "__  __     _      _");
    mvaddstr(2, 1, "\\ \\/ /   _| | ___| |__  _ __ __ _");
    mvaddstr(3, 1, " \\  / | | | |/ _ \\ '_ \\| '__/ _` |");
    mvaddstr(4, 1, " /  \\ |_| | |  __/ |_) | | | (_| |");
    mvaddstr(5, 1, "/_/\\_\\__,_|_|\\___|_.__/|_|  \\__,_|");
    REFRESH(stdscr);
}

/* ═══════════════════════════════════════════════════════════════
 * Game-over display
 * ═══════════════════════════════════════════════════════════════ */

static void force_exit(const char *reason)
{
    mvaddstr(12, 1, " /-------------               ");
    mvaddstr(13, 1, "| GAME OVER --> ");
    mvaddstr(14, 1, " \\-------------               ");
    mvaddstr(13, 18, reason);
    REFRESH(stdscr);
    REFRESH(curscr);
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

    /* Determine login name: prefer IRCUSER, fall back to LOGNAME */
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
    int i;
    int start_x, start_y, dir;

    if (cs->player == 0) {
        start_x = P1_START_X; start_y = P1_START_Y; dir = DIR_RIGHT;
    } else {
        start_x = P2_START_X; start_y = P2_START_Y; dir = DIR_LEFT;
    }

    cs->snake.head = cs->snake.tail = NULL;
    cs->snake.dir = dir;

    for (i = 0; i < INIT_LEN; i++) {
        int x = (dir == DIR_RIGHT) ? start_x + i : start_x - i;

        /* Add to our local list */
        cs_add_block(cs, x, start_y);

        /* Tell server about this segment */
        cs->packet[0] = MSG_GROW;
        cs->packet[1] = (char)x;
        cs->packet[2] = (char)start_y;
        write(cs->sock, cs->packet, PACKET_SIZE);

        /* Read back the opponent's corresponding grow echo and render it */
        read(cs->sock, cs->packet, PACKET_SIZE);
        mvwaddch(cs->game, cs->packet[2], cs->packet[1] * 2, '+');
    }

    draw_snake(cs);
}

/* ═══════════════════════════════════════════════════════════════
 * Move classification
 *
 * Returns  0  normal move  (new head stored in *f)
 *          1  wall collision
 *          2  self-bite
 *          3  ate apple
 * ═══════════════════════════════════════════════════════════════ */

static int classify_move(ClientState *cs, int key, CNode *f)
{
    /* Reject reversal with explicit pair checks */
    if ((key == DIR_UP    && cs->snake.dir == DIR_DOWN)  ||
        (key == DIR_DOWN  && cs->snake.dir == DIR_UP)    ||
        (key == DIR_LEFT  && cs->snake.dir == DIR_RIGHT) ||
        (key == DIR_RIGHT && cs->snake.dir == DIR_LEFT))
        key = KEY_NONE;

    /* Resolve KEY_NONE / ERR / unknown to current direction */
    if (key != DIR_UP && key != DIR_DOWN &&
        key != DIR_LEFT && key != DIR_RIGHT)
        key = cs->snake.dir;

    cs->snake.dir = key;

    f->x = cs->snake.tail->x;
    f->y = cs->snake.tail->y;

    switch (key) {
    case DIR_UP:    f->y--; break;
    case DIR_DOWN:  f->y++; break;
    case DIR_LEFT:  f->x--; break;
    default:        f->x++; break;
    }

    if (f->x < 1 || f->x > cs->board_cols ||
        f->y < 1 || f->y > cs->board_rows)
        return 1;

    if (cs_contains(cs, f->x, f->y))
        return 2;

    if (f->x == cs->apple_x && f->y == cs->apple_y)
        return 3;

    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Move actions – each sends a packet to the server
 * ═══════════════════════════════════════════════════════════════ */

static void do_move(ClientState *cs, const CNode *f)
{
    CNode old;
    cs_add_block(cs, f->x, f->y);
    old = cs_pop_head(cs);

    mvwaddch(cs->game, f->y,   f->x   * 2, '*');
    mvwaddch(cs->game, old.y,  old.x  * 2, ' ');

    cs->packet[0] = MSG_MOVE;
    cs->packet[1] = (char)f->x;   cs->packet[2] = (char)f->y;
    cs->packet[3] = (char)old.x;  cs->packet[4] = (char)old.y;
    write(cs->sock, cs->packet, PACKET_SIZE);
}

static void do_wall_collision(ClientState *cs, const CNode *f)
{
    mvwaddch(cs->game, f->y, f->x * 2, '#');
    cs->packet[0] = MSG_SELF_COLLIDE;
    cs->packet[1] = (char)f->x;
    cs->packet[2] = (char)f->y;
    write(cs->sock, cs->packet, PACKET_SIZE);
}

static void do_self_bite(ClientState *cs, const CNode *f)
{
    cs->packet[0] = MSG_SELF_BITE;
    cs->packet[1] = (char)f->x;
    cs->packet[2] = (char)f->y;
    write(cs->sock, cs->packet, PACKET_SIZE);
}

static void do_eat_apple(ClientState *cs, const CNode *f)
{
    cs_add_block(cs, f->x, f->y);
    mvwaddch(cs->game, f->y, f->x * 2, '*');

    cs->packet[0] = MSG_ATE_APPLE;
    cs->packet[1] = (char)cs->apple_x;
    cs->packet[2] = (char)cs->apple_y;
    write(cs->sock, cs->packet, PACKET_SIZE);

    /* Server replies immediately with the new apple position */
    read(cs->sock, cs->packet, PACKET_SIZE);
    cs->apple_x = (unsigned char)cs->packet[1];
    cs->apple_y = (unsigned char)cs->packet[2];
    mvwaddch(cs->game, cs->apple_y, cs->apple_x * 2, '@');
}

/* ═══════════════════════════════════════════════════════════════
 * Argument parsing
 * ═══════════════════════════════════════════════════════════════ */

static int parse_args(ClientState *cs, int argc, char *argv[])
{
    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-') continue;   /* fixed: was '=' not '==' */
        switch (argv[i][1]) {
        case 'h':
            if (i + 1 < argc) {
                fprintf(stdout, "Using host %s\n", argv[i + 1]);
                strncpy(cs->host, argv[++i], sizeof(cs->host) - 1);
            } else {
                fprintf(stderr, "client: -h requires a hostname\n");
                return -1;
            }
            break;
        case 'p':
            if (i + 1 < argc) {
                cs->port = atoi(argv[++i]);
                if (cs->port <= 1024) {
                    fprintf(stderr, "client: port must be > 1024\n");
                    return -1;
                }
                fprintf(stdout, "Using port %d\n", cs->port);
            } else {
                fprintf(stderr, "client: -p requires a port number\n");
                return -1;
            }
            break;
        case 'W':
            if (i + 1 < argc) {
                cs->board_cols = atoi(argv[++i]);
                if (cs->board_cols < 10) {
                    fprintf(stderr, "client: board width must be >= 10\n");
                    return -1;
                }
            } else {
                fprintf(stderr, "client: -W requires a column count\n");
                return -1;
            }
            break;
        case 'H':
            if (i + 1 < argc) {
                cs->board_rows = atoi(argv[++i]);
                if (cs->board_rows < 5) {
                    fprintf(stderr, "client: board height must be >= 5\n");
                    return -1;
                }
            } else {
                fprintf(stderr, "client: -H requires a row count\n");
                return -1;
            }
            break;
        case 'S':
            if (i + 1 < argc) {
                cs->init_speed = atoi(argv[++i]);
                if (cs->init_speed < 10 || cs->init_speed > 500) {
                    fprintf(stderr, "client: speed must be 10-500 ms\n");
                    return -1;
                }
            } else {
                fprintf(stderr, "client: -S requires a millisecond value\n");
                return -1;
            }
            break;
        default:
            fprintf(stderr, "client: unknown option -%c\n", argv[i][1]);
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
    int         key;
    CNode       future;

    client_state_init(&cs);

    if (parse_args(&cs, argc, argv) < 0)
        return;

    /* ncurses setup */
    initscr();
    nonl();
    noecho();
    cbreak();

    /* Create windows */
    cs.game = newwin(cs.board_rows + 2,
                     (cs.board_cols * 2) + 2, 1, 0);
    cs.info = newwin(cs.board_rows + 2,
                     COLS - (cs.board_cols * 2) - 2,
                     1, (cs.board_cols * 2) + 2);

    if (!cs.info || !cs.game) {
        endwin();
        fprintf(stderr, "client: could not create windows\n");
        return;
    }

    show_logo();

    if (connect_to_server(&cs) < 0)
        return;

    mvaddstr(0, 3, "Xulebra v0.1 By Xuzo");
    REFRESH(stdscr);

    wborder(cs.game, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
    wborder(cs.info, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);

    init_snake(&cs);
    init_info(&cs);

    /* Signal server that init phase is done; receive first apple */
    cs.packet[0] = MSG_GROW_DONE;
    write(cs.sock, cs.packet, PACKET_SIZE);
    read(cs.sock, cs.packet, PACKET_SIZE);

    cs.apple_x = (unsigned char)cs.packet[1];
    cs.apple_y = (unsigned char)cs.packet[2];
    mvwaddch(cs.game, cs.apple_y, cs.apple_x * 2, '@');

    keypad(cs.game, TRUE);
    wtimeout(cs.game, cs.init_speed);

    REFRESH(cs.game);
    REFRESH(cs.info);

    /* Game loop */
    for (;;) {
        key = wgetch(cs.game);

        switch (classify_move(&cs, key, &future)) {
        case 0:
            do_move(&cs, &future);
            break;
        case 1:
            do_wall_collision(&cs, &future);
            force_exit("You crashed into the wall");
            break;
        case 2:
            do_self_bite(&cs, &future);
            force_exit("You bit yourself");
            break;
        case 3:
            do_eat_apple(&cs, &future);
            update_info(&cs, 0);
            break;
        }

        /* Read and handle one packet from the server */
        read(cs.sock, cs.packet, PACKET_SIZE);
        switch ((unsigned char)cs.packet[0]) {

        case MSG_RELAY_MOVE:
            mvwaddch(cs.game, cs.packet[2], cs.packet[1] * 2, '+');
            mvwaddch(cs.game, cs.packet[4], cs.packet[3] * 2, ' ');
            break;

        case MSG_RELAY_GROW:
            mvwaddch(cs.game, cs.packet[2], cs.packet[1] * 2, '+');
            break;

        case MSG_OPP_COLLIDE:
            mvwaddch(cs.game, cs.packet[2], cs.packet[1] * 2, '#');
            force_exit("Opponent crashed into the wall");
            break;

        case MSG_OPP_BITE:
            force_exit("Opponent bit themselves");
            break;

        case MSG_OPP_BIT_ME:
            force_exit("Opponent crashed into you");
            break;

        case MSG_I_BIT_OPP:
            force_exit("You crashed into the opponent");
            break;

        case MSG_DRAW:
            mvwaddch(cs.game, cs.packet[2], cs.packet[1] * 2, '#');
            force_exit("Draw match");
            break;

        case MSG_NEW_APPLE:
            update_info(&cs, 1);
            mvwaddch(cs.game, cs.apple_y, cs.apple_x * 2, '+');
            cs.apple_x = (unsigned char)cs.packet[1];
            cs.apple_y = (unsigned char)cs.packet[2];
            mvwaddch(cs.game, cs.apple_y, cs.apple_x * 2, '@');
            break;

        default:
            break;
        }

        REFRESH(cs.game);
    }

    endwin();
}