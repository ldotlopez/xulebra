/*
 * main.c
 * Entry point for Xulebra.
 *
 * Changes from previous version
 * ──────────────────────────────
 * • score_seed() called once here so all modules share one PRNG seed.
 * • signal(SIGPIPE, SIG_IGN) installed early; server also installs it
 *   but having it in main() protects the client path as well.
 * • Help text updated for all new flags.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

void one_player(int argc, char *argv[]);
void server_run(int argc, char *argv[]);
void client_run(int argc, char *argv[]);
void score_seed(void);

/* ── Signal handling ────────────────────────────────────────── */

static void handle_sigint(int sig)
{
    signal(SIGINT, handle_sigint);
    (void)sig;
}

/* ── Help ───────────────────────────────────────────────────── */

static void print_help(void)
{
    puts("Xulebra v0.2  –  two-player networked snake");
    puts("Copyright (C) XuZo 2000-2001\n");
    puts("Usage:  xulebra <mode> [options]\n");
    puts("Modes:");
    puts("  -1          Single-player");
    puts("  -s          Server mode  (also spawns a local client)");
    puts("  -c          Client mode  (connect to a running server)");
    puts("  -h          Show this help\n");

    puts("Options for -1 (single-player):");
    puts("  -W <cols>    Board width        (default 30, min 10)");
    puts("  -H <rows>    Board height       (default 20, min 5)");
    puts("  -S <level>   Initial speed 1-10 (default 5; 1=slow 10=fast)");
    puts("  -L <len>     Initial snake length (default 3, max 20)");
    puts("  -A <n>       Apples on board at once (default 1, max 9)");
    puts("  -N <n>       Auto speed-up every N apples (0=off, default 0)");
    puts("  -T           Wrap-around: exit wall re-enters opposite side");
    puts("  -b           Enable AI bot opponent (blue snake)");
    puts("  (in-game)    SPACE = pause   Q / ESC = quit\n");

    puts("Options for -s / -c (network):");
    puts("  -p <port>    TCP port            (default 2580, must be > 1024)");
    puts("  -W <cols>    Board width         (default 30, min 10)");
    puts("  -H <rows>    Board height        (default 20, min 5)");
    puts("  -S <level>   Initial speed 1-10  (default 5)");
    puts("  -A <n>       Apples on board     (default 1, max 9)  [-s only]");
    puts("  -v           Enable spectator slot [-s only]");
    puts("  -h <host>    Server hostname      (default localhost) [-c only]");
}

/* ── Lock-file helpers ──────────────────────────────────────── */

static void lock_path(char *buf, size_t size)
{
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, size, "%s/.xulebra.lock", home);
}

static int lock_create(const char *path)
{
    int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR);
    if (fd < 0) return -1;
    close(fd);
    return 0;
}

static void lock_remove(const char *path)
{
    if (remove(path) != 0)
        fprintf(stderr, "warning: could not remove lock %s\n", path);
}

/* ── Argument shifting ──────────────────────────────────────── */

static void drop_first_arg(int *argc, char *argv[])
{
    int i;
    for (i = 1; i < *argc - 1; i++)
        argv[i] = argv[i + 1];
    (*argc)--;
    argv[*argc] = NULL;
}

/* ── Entry point ────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    char  mode;
    char  lock[256];
    int   child_status;
    pid_t pid;

    if (argc < 2) {
        fprintf(stderr, "Try: xulebra -h\n");
        return EXIT_FAILURE;
    }

    if (argv[1][0] != '-') {
        fprintf(stderr, "Invalid option: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    mode = argv[1][1];
    switch (mode) {
    case '1': case 's': case 'c': case 'h': break;
    default:
        fprintf(stderr, "Unknown option: -%c\n", argv[1][1]);
        return EXIT_FAILURE;
    }

    drop_first_arg(&argc, argv);

    /* Seed PRNG once for all modules */
    score_seed();

    /* Ignore SIGPIPE globally; write() returns -1 on broken sockets */
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  handle_sigint);

    switch (mode) {

    case '1':
        one_player(argc, argv);
        break;

    case 's':
        lock_path(lock, sizeof(lock));
        if (access(lock, F_OK) == 0) {
            fprintf(stderr, "Server already running (lock: %s)\n", lock);
            return EXIT_FAILURE;
        }
        if (lock_create(lock) < 0) {
            perror("lock_create");
            return EXIT_FAILURE;
        }
        pid = fork();
        if (pid < 0) {
            perror("fork");
            lock_remove(lock);
            return EXIT_FAILURE;
        }
        if (pid > 0) {
            server_run(argc, argv);
            waitpid(pid, &child_status, 0);
            lock_remove(lock);
        } else {
            client_run(argc, argv);
            exit(EXIT_SUCCESS);
        }
        break;

    case 'c':
        client_run(argc, argv);
        break;

    case 'h':
        print_help();
        break;
    }

    return EXIT_SUCCESS;
}