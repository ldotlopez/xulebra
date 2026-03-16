/*
 * score.c
 * Hall-of-Fame: writing and displaying high scores.
 *
 * Changes from previous version
 * ──────────────────────────────
 * • score_seed()    – call once at startup so rand() is properly seeded
 *                     instead of re-seeding inside apple_generate().
 * • score_write()   – caps the file at SCORE_MAX_RECORDS (100) by
 *                     truncating after sorting; unbounded growth is gone.
 * • score_show_ncurses() – new: renders the top-10 table inside a curses
 *                     WINDOW instead of printing to stdout, so it displays
 *                     correctly after the game window has been destroyed.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <curses.h>

#include "defines.h"
#include "snake.h"
#include "score.h"
#include "colors.h"

/* ── Internal helpers ───────────────────────────────────────── */

static int open_or_create_db(void)
{
    int fd = open(SCORE_DATABASE, O_RDWR);
    if (fd >= 0) return fd;
    return open(SCORE_DATABASE,
                O_RDWR | O_CREAT,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}

/* ── Public API ─────────────────────────────────────────────── */

/*
 * score_seed – seed the PRNG once at program startup.
 * Call from main() before any game mode is entered.
 * All apple placement in one_player.c, client.c, and server.c
 * benefits because they all call rand() after this.
 */
void score_seed(void)
{
    srand((unsigned int)time(NULL));
}

/*
 * score_write – insert a new entry, keep the file sorted descending,
 *               and cap at SCORE_MAX_RECORDS.
 */
int score_write(const char *login, int points)
{
    int         fd;
    off_t       file_size;
    int         n_records;
    ScoreRecord incoming, stored, to_write;

    fd = open_or_create_db();
    if (fd < 0) return -1;

    memset(&incoming, 0, sizeof(incoming));
    strncpy(incoming.login, login, sizeof(incoming.login) - 1);
    incoming.points = points;

    /* Insertion-sort pass: bubble the new entry up to its correct slot */
    while (read(fd, &stored, sizeof(ScoreRecord)) == (ssize_t)sizeof(ScoreRecord)) {
        lseek(fd, -(off_t)sizeof(ScoreRecord), SEEK_CUR);

        if (incoming.points > stored.points) {
            to_write = incoming;
            incoming = stored;
        } else {
            to_write = stored;
        }

        if (write(fd, &to_write, sizeof(ScoreRecord)) != (ssize_t)sizeof(ScoreRecord)) {
            close(fd);
            return -1;
        }
    }

    /* Append the displaced (lowest) entry */
    if (write(fd, &incoming, sizeof(ScoreRecord)) != (ssize_t)sizeof(ScoreRecord)) {
        close(fd);
        return -1;
    }

    /* Cap the file at SCORE_MAX_RECORDS to prevent unbounded growth */
    file_size = lseek(fd, 0, SEEK_END);
    n_records = (int)(file_size / (off_t)sizeof(ScoreRecord));
    if (n_records > SCORE_MAX_RECORDS) {
        ftruncate(fd, (off_t)SCORE_MAX_RECORDS * (off_t)sizeof(ScoreRecord));
    }

    close(fd);
    return 1;
}

/*
 * score_show – print the top-N scores to stdout.
 * Used after endwin() when curses is no longer active.
 */
int score_show(int limit)
{
    int         fd, count = 0;
    ScoreRecord entry;

    fd = open_or_create_db();
    if (fd < 0) return -1;

    printf("%-10s  %s\n", "Player", "Points");
    printf("%-10s  %s\n", "----------", "------");

    while (count < limit &&
           read(fd, &entry, sizeof(ScoreRecord)) == (ssize_t)sizeof(ScoreRecord)) {
        printf("%-10s  %d\n", entry.login, entry.points);
        count++;
    }

    close(fd);
    return count;
}

/*
 * score_show_ncurses – render the top-N high-score table inside a curses
 * WINDOW centred on the terminal.  Call while curses is still active
 * (i.e. before endwin()).
 *
 * The window is created, displayed with a "press any key" prompt, waits
 * for a keypress, then destroys itself — leaving the caller's windows
 * intact.
 *
 * @limit       Maximum rows to show (capped at 10).
 * @has_color   Pass gs->has_color so the table uses the project palette.
 */
void score_show_ncurses(int limit, int has_color)
{
    int         fd, count = 0, i;
    ScoreRecord entries[10];
    WINDOW     *win;
    int         win_h, win_w, win_y, win_x;

    if (limit > 10) limit = 10;

    fd = open_or_create_db();
    if (fd >= 0) {
        while (count < limit &&
               read(fd, &entries[count], sizeof(ScoreRecord))
                   == (ssize_t)sizeof(ScoreRecord))
            count++;
        close(fd);
    }

    /* Window dimensions: 2 border + 1 title + 1 blank + count rows + 1 blank + 1 prompt */
    win_h = count + 6;
    win_w = 32;
    win_y = (LINES - win_h) / 2;
    win_x = (COLS  - win_w) / 2;
    if (win_y < 0) win_y = 0;
    if (win_x < 0) win_x = 0;

    win = newwin(win_h, win_w, win_y, win_x);
    if (!win) return;

    keypad(win, TRUE);
    wtimeout(win, -1);   /* blocking — wait for key */

    if (has_color) wattron(win, COLOR_PAIR(CP_BORDER) | A_BOLD);
    wborder(win, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
    if (has_color) wattroff(win, COLOR_PAIR(CP_BORDER) | A_BOLD);

    /* Title */
    if (has_color) wattron(win, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwaddstr(win, 1, (win_w - 12) / 2, "Hall of Fame");
    if (has_color) wattroff(win, COLOR_PAIR(CP_TITLE) | A_BOLD);

    /* Column headers */
    if (has_color) wattron(win, COLOR_PAIR(CP_INFO_LABEL));
    mvwprintw(win, 2, 2, "%-3s %-10s %7s", "#", "Player", "Points");
    if (has_color) wattroff(win, COLOR_PAIR(CP_INFO_LABEL));

    /* Entries */
    for (i = 0; i < count; i++) {
        if (has_color) wattron(win, COLOR_PAIR(CP_INFO_VALUE) | (i == 0 ? A_BOLD : 0));
        mvwprintw(win, 3 + i, 2, "%-3d %-10s %7d",
                  i + 1, entries[i].login, entries[i].points);
        if (has_color) wattroff(win, COLOR_PAIR(CP_INFO_VALUE) | A_BOLD);
    }

    if (count == 0) {
        if (has_color) wattron(win, COLOR_PAIR(CP_INFO_LABEL));
        mvwaddstr(win, 3, 2, "(no scores yet)");
        if (has_color) wattroff(win, COLOR_PAIR(CP_INFO_LABEL));
    }

    /* Prompt */
    if (has_color) wattron(win, COLOR_PAIR(CP_INFO_LABEL));
    mvwaddstr(win, win_h - 2, (win_w - 17) / 2, "press any key...");
    if (has_color) wattroff(win, COLOR_PAIR(CP_INFO_LABEL));

    wnoutrefresh(win);
    doupdate();
    wgetch(win);
    delwin(win);
}
