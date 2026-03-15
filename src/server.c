/*
 * server.c
 * Network game server for Xulebra (two-player snake).
 *
 * Responsibilities
 *   1. Accept connections from both players.
 *   2. Exchange player names.
 *   3. Mirror packets between players, translating client-perspective
 *      message types to opponent-perspective message types where needed.
 *   4. Maintain authoritative copies of both snakes to detect cross-
 *      collisions (one snake's head entering the other's body).
 *   5. Own apple generation, so both clients always agree on apple position.
 *
 * Threading model: single-threaded, synchronous.  The server reads one
 * packet per player per frame, processes them, then writes the opponent's
 * (possibly modified) packet back to each player.
 *
 * Global-variable elimination — Context Object pattern
 * ─────────────────────────────────────────────────────
 * The original code kept two Snake structs and the apple position as
 * file-scope (global) variables.  Globals are avoided here by collecting
 * ALL mutable game state into ServerState, which is allocated on the stack
 * of server_run() — its natural owner — and passed by pointer to every
 * helper that needs it.
 *
 * Benefits:
 *   • Every function's dependencies are visible in its signature.
 *   • Helper functions are reentrant: a second call (e.g. in a test or a
 *     future multi-game server) sees completely independent state.
 *   • Lifetime is unambiguous: the struct lives exactly as long as one
 *     server session, and server_state_cleanup() releases it cleanly.
 *   • No changes to any header or external interface are required.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "socket.h"
#include "xulnet.h"
#include "structs.h"

/* ═══════════════════════════════════════════════════════════════
 * Context Object
 *
 * ServerState owns every piece of mutable state that used to live
 * at file scope.  One instance is created on the stack of server_run()
 * and a pointer to it is threaded through every helper.
 * ═══════════════════════════════════════════════════════════════ */
typedef struct {
    Snake snakes[2];          /* Authoritative server-side snake copies    */
    int   apple_x;            /* Current apple X; -1 = not yet placed      */
    int   apple_y;            /* Current apple Y; -1 = not yet placed      */
    int   player_fd[2];       /* Connected socket fds for each player      */
    char  names[2][LOGIN_LEN];/* Login names exchanged at handshake        */
} ServerState;

/*
 * server_state_init – zero-initialise *s and mark the apple as absent.
 * Must be called before any other function that accepts a ServerState *.
 */
static void server_state_init(ServerState *s)
{
    snake_init(&s->snakes[0]);
    snake_init(&s->snakes[1]);
    s->apple_x     = -1;
    s->apple_y     = -1;
    s->player_fd[0] = -1;
    s->player_fd[1] = -1;
    memset(s->names, 0, sizeof(s->names));
}

/*
 * server_state_cleanup – release all resources held by *s.
 * Safe to call even if initialisation was only partial (fd == -1 is skipped).
 */
static void server_state_cleanup(ServerState *s)
{
    int i;
    for (i = 0; i < 2; i++) {
        snake_free(&s->snakes[i]);
        if (s->player_fd[i] >= 0) {
            close(s->player_fd[i]);
            s->player_fd[i] = -1;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════
 * I/O helpers  (stateless – no ServerState needed)
 * ═══════════════════════════════════════════════════════════════ */

static int read_packet(int fd, char *buf)
{
    ssize_t n = read(fd, buf, PACKET_SIZE);
    return (n == (ssize_t)PACKET_SIZE) ? PACKET_SIZE : -1;
}

static int write_packet(int fd, const char *buf)
{
    ssize_t n = write(fd, buf, PACKET_SIZE);
    return (n == (ssize_t)PACKET_SIZE) ? PACKET_SIZE : -1;
}

/* ═══════════════════════════════════════════════════════════════
 * Apple generation  (needs ServerState for snake positions)
 * ═══════════════════════════════════════════════════════════════ */

/*
 * apple_generate – pick a random board cell not occupied by either snake
 * and store it in s->apple_x / s->apple_y.
 *
 * srandom() is seeded once per call so that rapid back-to-back calls
 * (unlikely in practice) still get different seeds via time(NULL).
 */
static void apple_generate(ServerState *s)
{
    srandom((unsigned int)time(NULL));
    do {
        s->apple_x = (int)(random() % NET_BOARD_COLS) + 1;
        s->apple_y = (int)(random() % NET_BOARD_ROWS) + 1;
    } while (snake_contains(&s->snakes[0], s->apple_x, s->apple_y) ||
             snake_contains(&s->snakes[1], s->apple_x, s->apple_y));
}

/* ═══════════════════════════════════════════════════════════════
 * Per-player packet processing
 * ═══════════════════════════════════════════════════════════════ */

/*
 * process_player_packet – update *s for player index `pl` and translate
 * the packet's message type from the sender's perspective to the
 * opponent's perspective.
 *
 * @s        Live server state (snakes, apple position).
 * @pl       Sending player index: 0 or 1.
 * @self_fd  Socket fd of the sending player  (used for direct replies).
 * @packet   PACKET_SIZE-byte buffer; modified in-place.
 *
 * Return values
 *   0   Packet was relabelled and should be forwarded to the opponent.
 *   1   Packet was consumed here (a direct reply was sent to self_fd);
 *       do NOT forward to opponent.
 *  -1   Fatal I/O error.
 */
static int process_player_packet(ServerState *s, int pl, int self_fd,
                                  char packet[PACKET_SIZE])
{
    int nx = (unsigned char)packet[1];
    int ny = (unsigned char)packet[2];

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
        /*
         * Player finished its initial growth sequence.
         * Player 0 triggers the very first apple placement; player 1
         * (who sends this message later) reuses the already-generated
         * coordinates.
         */
        if (pl == 0)
            apple_generate(s);

        packet[0] = MSG_SET_APPLE;
        packet[1] = (char)s->apple_x;
        packet[2] = (char)s->apple_y;
        if (write_packet(self_fd, packet) < 0) return -1;
        return 1;   /* consumed — do not relay to opponent */

    case MSG_ATE_APPLE:
        snake_push_head(&s->snakes[pl], nx, ny, 0);
        apple_generate(s);
        packet[0] = MSG_NEW_APPLE;
        packet[1] = (char)s->apple_x;
        packet[2] = (char)s->apple_y;
        if (write_packet(self_fd, packet) < 0) return -1;
        return 1;   /* consumed */

    default:
        return 0;   /* Unknown packet — forward as-is */
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Cross-collision detection
 * ═══════════════════════════════════════════════════════════════ */

/*
 * check_cross_collisions – compare the two new head positions against
 * each other's bodies and inject the appropriate game-over messages.
 *
 * @s     Live server state (read-only here; only the snakes are queried).
 * @pkt0  Packet from player 0 (modified in-place on event).
 * @pkt1  Packet from player 1 (modified in-place on event).
 *
 * Returns 1 if an event was injected, 0 otherwise.
 */
static int check_cross_collisions(const ServerState *s,
                                   char pkt0[PACKET_SIZE],
                                   char pkt1[PACKET_SIZE])
{
    int x0 = (unsigned char)pkt0[1], y0 = (unsigned char)pkt0[2];
    int x1 = (unsigned char)pkt1[1], y1 = (unsigned char)pkt1[2];

    if (x0 == x1 && y0 == y1) {
        pkt0[0] = MSG_DRAW;
        pkt1[0] = MSG_DRAW;
        return 1;
    }

    /* Player 1's new head entered player 0's body */
    if (snake_contains(&s->snakes[0], x1, y1)) {
        pkt0[0] = MSG_I_BIT_OPP;
        pkt1[0] = MSG_OPP_BIT_ME;
        return 1;
    }

    /* Player 0's new head entered player 1's body */
    if (snake_contains(&s->snakes[1], x0, y0)) {
        pkt1[0] = MSG_I_BIT_OPP;
        pkt0[0] = MSG_OPP_BIT_ME;
        return 1;
    }

    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Argument parsing
 * ═══════════════════════════════════════════════════════════════ */

/*
 * parse_port – scan argv for "-p <port>" and return the port number,
 *              or DEFAULT_PORT if the flag is absent.
 * Returns -1 and prints a diagnostic on any parse error.
 */
static int parse_port(int argc, char *argv[])
{
    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-') continue;
        switch (argv[i][1]) {
        case 'p':
            if (i + 1 >= argc) {
                fprintf(stderr, "server: -p requires a port number\n");
                return -1;
            }
            {
                int port = atoi(argv[++i]);
                if (port <= 1024) {
                    fprintf(stderr,
                            "server: port must be greater than 1024\n");
                    return -1;
                }
                fprintf(stderr, "server: listening on port %d\n", port);
                return port;
            }
        default:
            fprintf(stderr, "server: unknown option %s\n", argv[i]);
            return -1;
        }
    }
    return DEFAULT_PORT;
}

/* ═══════════════════════════════════════════════════════════════
 * Handshake
 * ═══════════════════════════════════════════════════════════════ */

/*
 * accept_players – accept both TCP connections, exchange login names
 * and player-index assignments.
 *
 * Populates s->player_fd[] and s->names[][].
 * Returns 0 on success, -1 on any I/O error.
 */
static int accept_players(ServerState *s, int port)
{
    int i;
    for (i = 0; i < 2; i++) {
        s->player_fd[i] = net_accept_one(port);
        if (s->player_fd[i] < 0) {
            fprintf(stderr, "server: net_accept_one failed (%d)\n",
                    s->player_fd[i]);
            return -1;
        }
        if (read(s->player_fd[i], s->names[i], LOGIN_LEN) <= 0) {
            fprintf(stderr, "server: failed to read login for player %d\n",
                    i + 1);
            return -1;
        }
        if (write(s->player_fd[i], &i, sizeof(int)) <= 0) {
            fprintf(stderr,
                    "server: failed to send player number to player %d\n",
                    i + 1);
            return -1;
        }
    }

    /* Tell each player their opponent's name */
    write(s->player_fd[0], s->names[1], LOGIN_LEN);
    write(s->player_fd[1], s->names[0], LOGIN_LEN);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Main game loop
 * ═══════════════════════════════════════════════════════════════ */

/*
 * game_loop – read, process, and relay packets until a connection drops.
 * All mutable state is accessed exclusively through *s.
 */
static void game_loop(ServerState *s)
{
    char packets[2][PACKET_SIZE];
    int  consumed[2];
    int  i;

    while (1) {
        /* Read one packet per player */
        for (i = 0; i < 2; i++) {
            if (read_packet(s->player_fd[i], packets[i]) < 0)
                return;
        }

        consumed[0] = process_player_packet(s, 0, s->player_fd[0], packets[0]);
        consumed[1] = process_player_packet(s, 1, s->player_fd[1], packets[1]);

        if (consumed[0] < 0 || consumed[1] < 0) return;

        if (!consumed[0] && !consumed[1])
            check_cross_collisions(s, packets[0], packets[1]);

        if (!consumed[0])
            if (write_packet(s->player_fd[1], packets[0]) < 0) return;

        if (!consumed[1])
            if (write_packet(s->player_fd[0], packets[1]) < 0) return;
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Public entry point
 * ═══════════════════════════════════════════════════════════════ */

/*
 * server_run – parse arguments, set up state, run the game, clean up.
 *
 * This is the only public symbol in this translation unit.
 * All state lives inside `state` on this stack frame; there are no
 * file-scope variables in this file.
 */
void server_run(int argc, char *argv[])
{
    ServerState state;
    int         port;

    port = parse_port(argc, argv);
    if (port < 0) return;

    server_state_init(&state);

    if (accept_players(&state, port) < 0) {
        server_state_cleanup(&state);
        return;
    }

    game_loop(&state);

    sleep(1);   /* Allow clients to drain their receive buffers */
    server_state_cleanup(&state);
}
