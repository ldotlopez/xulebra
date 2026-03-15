/*
 * main.c
 * Entry point for Xulebra – a networked two-player snake game.
 *
 * Usage
 *   xulebra -1        Single-player mode
 *   xulebra -s        Start as server (also launches a local client child)
 *   xulebra -c        Connect as client only
 *   xulebra -h        Print this help
 *
 * Optional flag accepted by -s and -c:
 *   -p <port>         Override the default port (must be > 1024)
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

/* Forward declarations for the mode entry points defined elsewhere */
void one_player(int argc, char *argv[]);
void server_run(int argc, char *argv[]);
void client_run(int argc, char *argv[]);

/* ── Signal handling ────────────────────────────────────────── */

static void handle_sigint(int sig)
{
    /* Re-arm: POSIX does not guarantee persistent signal handlers */
    signal(SIGINT, handle_sigint);
    (void)sig;  /* suppress unused-parameter warning */
}

/* ── Help text ──────────────────────────────────────────────── */

static void print_help(void)
{
    puts("Xulebra  –  two-player networked snake");
    puts("Copyright (C) XuZo 2000-2001\n");
    puts("Usage:  xulebra <mode> [options]\n");
    puts("Modes:");
    puts("  -1          Single-player");
    puts("  -s          Server mode  (also spawns a local client)");
    puts("  -c          Client mode  (connect to a running server)");
    puts("  -h          Show this help\n");
    puts("Options (for -s and -c):");
    puts("  -p <port>   TCP port to use  (default: 2580, must be > 1024)");
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
    int fd = open(path, O_CREAT | O_EXCL | O_WRONLY,
                  S_IRUSR | S_IWUSR);
    if (fd < 0) return -1;
    close(fd);
    return 0;
}

static void lock_remove(const char *path)
{
    if (remove(path) != 0)
        fprintf(stderr, "warning: could not remove lock file %s\n", path);
}

/* ── Argument shifting ──────────────────────────────────────── */

/*
 * drop_first_arg – remove argv[1] (the mode flag) so that sub-functions
 * receive a clean argc/argv starting at their own flags.
 */
static void drop_first_arg(int *argc, char *argv[])
{
    int i;
    /* argv[argc] must remain NULL per the C standard */
    for (i = 1; i < *argc - 1; i++)
        argv[i] = argv[i + 1];
    (*argc)--;
    argv[*argc] = NULL;
}

/* ── Entry point ────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    char mode;
    char lock[256];
    int  child_status;
    pid_t pid;

    if (argc < 2) {
        fprintf(stderr, "Try: xulebra -h\n");
        return EXIT_FAILURE;
    }

    /* Parse the mode flag */
    if (argv[1][0] != '-') {
        fprintf(stderr, "Invalid option: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    mode = argv[1][1];
    switch (mode) {
    case '1': case 's': case 'c': case 'h':
        break;
    default:
        fprintf(stderr, "Unknown option: -%c\n", argv[1][1]);
        return EXIT_FAILURE;
    }

    /* Remove the mode flag; sub-functions see only their own args */
    drop_first_arg(&argc, argv);

    signal(SIGINT, handle_sigint);

    /* ── Dispatch ─────────────────────────────────────────── */
    switch (mode) {

    case '1':
        one_player(argc, argv);
        break;

    case 's':
        lock_path(lock, sizeof(lock));

        /* Refuse to start a second server instance */
        if (access(lock, F_OK) == 0) {
            fprintf(stderr,
                    "Server is already running  (lock file: %s)\n", lock);
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
            /* ── Parent: run the server ── */
            server_run(argc, argv);
            waitpid(pid, &child_status, 0);
            lock_remove(lock);
        } else {
            /* ── Child: run a local client ── */
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
