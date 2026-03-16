/*
 * server.c
 * Network game server for Xulebra (two-player snake).
 *
 * Changes from previous version
 * ──────────────────────────────
 * • read_packet()     – loops until all PACKET_SIZE bytes arrive; a single
 *                       read() on TCP is not guaranteed to return a full packet.
 * • apple_generate()  – no longer calls srand(); seeding is done once at
 *                       startup in main() via score_seed().
 * • server_run()      – installs SIGPIPE handler so a dead client socket
 *                       does not kill the server process.
 * • Spectator mode    – a third optional connection receives a read-only
 *                       copy of both packet streams (enable with -v).
 * • Multi-apple       – up to MAX_APPLES apples on the board at once (-A N).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include "socket.h"
#include "xulnet.h"
#include "structs.h"
#include "defines.h"

/* ── Multi-apple cap ────────────────────────────────────────── */
#define MAX_APPLES  9

/* ── ServerState ────────────────────────────────────────────── */
typedef struct {
    Snake snakes[2];
    int   apple_x[MAX_APPLES];
    int   apple_y[MAX_APPLES];
    int   n_apples;          /* Requested number of apples on board  */
    int   player_fd[2];
    int   spectator_fd;      /* -1 if no spectator connected         */
    char  names[2][LOGIN_LEN];
    int   board_cols;
    int   board_rows;
} ServerState;

static void server_state_init(ServerState *s, int n_apples)
{
    int i;
    snake_init(&s->snakes[0]);
    snake_init(&s->snakes[1]);
    for (i = 0; i < MAX_APPLES; i++) {
        s->apple_x[i] = -1;
        s->apple_y[i] = -1;
    }
    s->n_apples      = (n_apples > 0 && n_apples <= MAX_APPLES) ? n_apples : 1;
    s->player_fd[0]  = -1;
    s->player_fd[1]  = -1;
    s->spectator_fd  = -1;
    s->board_cols    = NET_BOARD_COLS;
    s->board_rows    = NET_BOARD_ROWS;
    memset(s->names, 0, sizeof(s->names));
}

static void server_state_cleanup(ServerState *s)
{
    int i;
    for (i = 0; i < 2; i++) {
        snake_free(&s->snakes[i]);
        if (s->player_fd[i] >= 0) { close(s->player_fd[i]); s->player_fd[i] = -1; }
    }
    if (s->spectator_fd >= 0) { close(s->spectator_fd); s->spectator_fd = -1; }
}

/* ── I/O helpers ────────────────────────────────────────────── */

/*
 * read_packet – accumulate exactly PACKET_SIZE bytes.
 * A single read() on a TCP socket may return fewer bytes than requested
 * if the kernel buffer is partially full.  Loop until we have a complete
 * packet or the connection closes.
 */
static int read_packet(int fd, char *buf)
{
    int total = 0;
    while (total < PACKET_SIZE) {
        ssize_t n = read(fd, buf + total, PACKET_SIZE - total);
        if (n <= 0) return -1;   /* EOF or error */
        total += (int)n;
    }
    return PACKET_SIZE;
}

static int write_packet(int fd, const char *buf)
{
    ssize_t n = write(fd, buf, PACKET_SIZE);
    return (n == (ssize_t)PACKET_SIZE) ? PACKET_SIZE : -1;
}

/*
 * relay_to_spectator – forward a packet to the spectator if connected.
 * Write failures from the spectator socket are silently ignored — a
 * disconnected spectator must never crash the game.
 */
static void relay_to_spectator(ServerState *s, const char *pkt)
{
    if (s->spectator_fd < 0) return;
    if (write_packet(s->spectator_fd, pkt) < 0) {
        close(s->spectator_fd);
        s->spectator_fd = -1;
    }
}

/* ── Apple generation ───────────────────────────────────────── */

/*
 * apple_cell_occupied – return 1 if (x,y) is a snake body cell or an
 * already-placed apple.
 */
static int apple_cell_occupied(const ServerState *s, int x, int y)
{
    int i;
    if (snake_contains(&s->snakes[0], x, y)) return 1;
    if (snake_contains(&s->snakes[1], x, y)) return 1;
    for (i = 0; i < s->n_apples; i++) {
        if (s->apple_x[i] == x && s->apple_y[i] == y) return 1;
    }
    return 0;
}

/*
 * apple_generate_slot – place one apple in slot `idx`.
 * srandom() is NOT called here; it is seeded once at startup.
 */
static void apple_generate_slot(ServerState *s, int idx)
{
    int x, y;
    do {
        x = (int)(rand() % s->board_cols) + 1;
        y = (int)(rand() % s->board_rows) + 1;
    } while (apple_cell_occupied(s, x, y));
    s->apple_x[idx] = x;
    s->apple_y[idx] = y;
}

/* Generate all n_apples slots. */
static void apple_generate_all(ServerState *s)
{
    int i;
    for (i = 0; i < s->n_apples; i++)
        apple_generate_slot(s, i);
}

/*
 * apple_find – return the index of the apple at (x,y), or -1.
 */
static int apple_find(const ServerState *s, int x, int y)
{
    int i;
    for (i = 0; i < s->n_apples; i++) {
        if (s->apple_x[i] == x && s->apple_y[i] == y) return i;
    }
    return -1;
}

/* ── Per-player packet processing ───────────────────────────── */

static int process_player_packet(ServerState *s, int pl, int self_fd,
                                  char packet[PACKET_SIZE])
{
    int nx = (unsigned char)packet[1];
    int ny = (unsigned char)packet[2];
    int idx;

    switch ((unsigned char)packet[0]) {

    case MSG_MOVE:
        snake_push_head(&s->snakes[pl], nx, ny, 0);
        snake_pop_tail(&s->snakes[pl]);
        packet[0] = MSG_RELAY_MOVE;
        return 0;

    case MSG_GROW:
        snake_push_head(&s->snakes[pl], nx, ny, 0);
        packet[0] = MSG_RELAY_GROW;
        return 0;

    case MSG_SELF_COLLIDE:
        packet[0] = MSG_OPP_COLLIDE;
        return 0;

    case MSG_SELF_BITE:
        packet[0] = MSG_OPP_BITE;
        return 0;

    case MSG_GROW_DONE:
        if (pl == 0)
            apple_generate_all(s);
        /* Send all apple positions: first apple reuses MSG_SET_APPLE,
           additional apples use MSG_NEW_APPLE so the client draws them. */
        {
            int i;
            char pkt[PACKET_SIZE];
            memset(pkt, 0, PACKET_SIZE);
            pkt[0] = MSG_SET_APPLE;
            pkt[1] = (char)s->apple_x[0];
            pkt[2] = (char)s->apple_y[0];
            if (write_packet(self_fd, pkt) < 0) return -1;
            for (i = 1; i < s->n_apples; i++) {
                pkt[0] = MSG_NEW_APPLE;
                pkt[1] = (char)s->apple_x[i];
                pkt[2] = (char)s->apple_y[i];
                if (write_packet(self_fd, pkt) < 0) return -1;
            }
        }
        return 1;

    case MSG_ATE_APPLE:
        idx = apple_find(s, nx, ny);
        if (idx >= 0) {
            snake_push_head(&s->snakes[pl], nx, ny, 0);
            apple_generate_slot(s, idx);
            packet[0] = MSG_NEW_APPLE;
            packet[1] = (char)s->apple_x[idx];
            packet[2] = (char)s->apple_y[idx];
            if (write_packet(self_fd, packet) < 0) return -1;
        }
        return 1;

    default:
        return 0;
    }
}

/* ── Cross-collision detection ──────────────────────────────── */

static int check_cross_collisions(const ServerState *s,
                                   char pkt0[PACKET_SIZE],
                                   char pkt1[PACKET_SIZE])
{
    int x0 = (unsigned char)pkt0[1], y0 = (unsigned char)pkt0[2];
    int x1 = (unsigned char)pkt1[1], y1 = (unsigned char)pkt1[2];

    if (x0 == x1 && y0 == y1) {
        pkt0[0] = pkt1[0] = MSG_DRAW;
        return 1;
    }
    if (snake_contains(&s->snakes[0], x1, y1)) {
        pkt0[0] = MSG_I_BIT_OPP; pkt1[0] = MSG_OPP_BIT_ME;
        return 1;
    }
    if (snake_contains(&s->snakes[1], x0, y0)) {
        pkt1[0] = MSG_I_BIT_OPP; pkt0[0] = MSG_OPP_BIT_ME;
        return 1;
    }
    return 0;
}

/* ── Argument parsing ───────────────────────────────────────── */

static int parse_args(int argc, char *argv[],
                      int *port, int *cols, int *rows,
                      int *n_apples, int *spectate)
{
    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-') continue;
        switch (argv[i][1]) {
        case 'p':
            if (i + 1 >= argc) { fprintf(stderr,"server: -p needs a port\n"); return -1; }
            *port = atoi(argv[++i]);
            if (*port <= 1024) { fprintf(stderr,"server: port must be > 1024\n"); return -1; }
            fprintf(stderr, "server: port %d\n", *port);
            break;
        case 'W':
            if (i + 1 >= argc) { fprintf(stderr,"server: -W needs a count\n"); return -1; }
            *cols = atoi(argv[++i]);
            if (*cols < BOARD_COLS_MIN) { fprintf(stderr,"server: -W >= %d\n",BOARD_COLS_MIN); return -1; }
            break;
        case 'H':
            if (i + 1 >= argc) { fprintf(stderr,"server: -H needs a count\n"); return -1; }
            *rows = atoi(argv[++i]);
            if (*rows < BOARD_ROWS_MIN) { fprintf(stderr,"server: -H >= %d\n",BOARD_ROWS_MIN); return -1; }
            break;
        case 'A':
            if (i + 1 >= argc) { fprintf(stderr,"server: -A needs a count\n"); return -1; }
            *n_apples = atoi(argv[++i]);
            if (*n_apples < 1 || *n_apples > MAX_APPLES) {
                fprintf(stderr,"server: -A must be 1-%d\n", MAX_APPLES); return -1;
            }
            break;
        case 'v':
            *spectate = 1;
            fprintf(stderr, "server: spectator slot enabled\n");
            break;
        default:
            fprintf(stderr, "server: unknown option %s\n", argv[i]);
            return -1;
        }
    }
    return 0;
}

/* ── Handshake ──────────────────────────────────────────────── */

static int accept_players(ServerState *s, int port, int want_spectator)
{
    int i;
    for (i = 0; i < 2; i++) {
        s->player_fd[i] = net_accept_one(port);
        if (s->player_fd[i] < 0) {
            fprintf(stderr, "server: accept failed for player %d\n", i+1);
            return -1;
        }
        if (read(s->player_fd[i], s->names[i], LOGIN_LEN) <= 0) {
            fprintf(stderr, "server: no login from player %d\n", i+1);
            return -1;
        }
        if (write(s->player_fd[i], &i, sizeof(int)) <= 0) {
            fprintf(stderr, "server: could not send player index %d\n", i+1);
            return -1;
        }
    }
    write(s->player_fd[0], s->names[1], LOGIN_LEN);
    write(s->player_fd[1], s->names[0], LOGIN_LEN);

    if (want_spectator) {
        fprintf(stderr, "server: waiting for spectator...\n");
        s->spectator_fd = net_accept_one(port);
        if (s->spectator_fd < 0)
            fprintf(stderr, "server: spectator accept failed, continuing\n");
        else
            fprintf(stderr, "server: spectator connected\n");
    }
    return 0;
}

/* ── Game loop ──────────────────────────────────────────────── */

static void game_loop(ServerState *s)
{
    char packets[2][PACKET_SIZE];
    int  consumed[2], i;

    while (1) {
        for (i = 0; i < 2; i++) {
            if (read_packet(s->player_fd[i], packets[i]) < 0) return;
        }

        consumed[0] = process_player_packet(s, 0, s->player_fd[0], packets[0]);
        consumed[1] = process_player_packet(s, 1, s->player_fd[1], packets[1]);
        if (consumed[0] < 0 || consumed[1] < 0) return;

        if (!consumed[0] && !consumed[1])
            check_cross_collisions(s, packets[0], packets[1]);

        if (!consumed[0]) {
            if (write_packet(s->player_fd[1], packets[0]) < 0) return;
            relay_to_spectator(s, packets[0]);
        }
        if (!consumed[1]) {
            if (write_packet(s->player_fd[0], packets[1]) < 0) return;
            relay_to_spectator(s, packets[1]);
        }
    }
}

/* ── Entry point ────────────────────────────────────────────── */

void server_run(int argc, char *argv[])
{
    ServerState state;
    int port        = DEFAULT_PORT;
    int board_cols  = NET_BOARD_COLS;
    int board_rows  = NET_BOARD_ROWS;
    int n_apples    = 1;
    int want_spec   = 0;

    /* Ignore SIGPIPE: write() returns -1 instead of killing the process
       when a client socket closes unexpectedly.                         */
    signal(SIGPIPE, SIG_IGN);

    if (parse_args(argc, argv, &port, &board_cols, &board_rows,
                   &n_apples, &want_spec) < 0)
        return;

    server_state_init(&state, n_apples);
    state.board_cols = board_cols;
    state.board_rows = board_rows;

    if (accept_players(&state, port, want_spec) < 0) {
        server_state_cleanup(&state);
        return;
    }

    game_loop(&state);

    sleep(1);
    server_state_cleanup(&state);
}