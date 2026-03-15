/*
 * score.c
 * Hall-of-Fame: writing and displaying high scores.
 *
 * The database is a flat binary file of ScoreRecord structs, kept in
 * descending order (highest score first).  A simple insertion-sort pass
 * keeps the file sorted after each new entry is added.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "defines.h"
#include "structs.h"

/* ── Internal helpers ───────────────────────────────────────── */

/*
 * open_or_create_db – open the score database for reading and writing,
 *                     creating it with appropriate permissions if absent.
 *
 * Returns a file descriptor >= 0 on success, -1 on failure.
 */
static int open_or_create_db(void)
{
    int fd = open(SCORE_DATABASE, O_RDWR);
    if (fd >= 0) return fd;

    /* File does not exist yet – create it */
    fd = open(SCORE_DATABASE,
              O_RDWR | O_CREAT,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    return fd;   /* -1 on failure, valid fd otherwise */
}

/* ── Public API ─────────────────────────────────────────────── */

/*
 * score_write – insert a new entry and keep the file sorted.
 *
 * The function performs one pass of insertion sort: it walks forward
 * through the file, and whenever the new record outscores the stored one,
 * the two are swapped.  The new record "bubbles up" to its correct
 * position, then the original (now displaced) record is written at the
 * end.
 *
 * @login   Player name (at most 8 characters, will be truncated).
 * @points  Score to record.
 *
 * Returns  1 on success, -1 on I/O error.
 */
int score_write(const char *login, int points)
{
    int         fd;
    off_t       pos;
    ScoreRecord incoming, stored, to_write;

    fd = open_or_create_db();
    if (fd < 0) return -1;

    /* Populate the new entry */
    memset(&incoming, 0, sizeof(incoming));
    strncpy(incoming.login, login, sizeof(incoming.login) - 1);
    incoming.points = points;

    /*
     * Walk through every existing record.  If `incoming` beats the
     * record at the current position, swap them so that the higher score
     * sits earlier in the file.
     */
    while (read(fd, &stored, sizeof(ScoreRecord)) == (ssize_t)sizeof(ScoreRecord))
    {
        /* Rewind to the start of the record we just read */
        pos = lseek(fd, -(off_t)sizeof(ScoreRecord), SEEK_CUR);

        if (incoming.points > stored.points) {
            /* incoming wins this slot: write it here, carry `stored` forward */
            to_write = incoming;
            incoming = stored;
        } else {
            /* stored keeps its slot; re-write it unchanged */
            to_write = stored;
        }

        if (write(fd, &to_write, sizeof(ScoreRecord)) != (ssize_t)sizeof(ScoreRecord)) {
            close(fd);
            return -1;
        }
    }

    /* Append whatever is left in `incoming` at the end of the file */
    if (write(fd, &incoming, sizeof(ScoreRecord)) != (ssize_t)sizeof(ScoreRecord)) {
        close(fd);
        return -1;
    }

    close(fd);
    return 1;
}

/*
 * score_show – print the top-N scores to stdout.
 *
 * @limit  Maximum number of entries to display.
 *
 * Returns the number of entries printed, or -1 on I/O error.
 */
int score_show(int limit)
{
    int        fd;
    int        count = 0;
    ScoreRecord entry;

    fd = open_or_create_db();
    if (fd < 0) return -1;

    printf("%-10s  %s\n", "Player", "Points");
    printf("%-10s  %s\n", "----------", "------");

    while (count < limit &&
           read(fd, &entry, sizeof(ScoreRecord)) == (ssize_t)sizeof(ScoreRecord))
    {
        printf("%-10s  %d\n", entry.login, entry.points);
        count++;
    }

    close(fd);
    return count;
}
