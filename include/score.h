/*
 * score.h
 * Hall-of-Fame public interface.
 */

#ifndef SCORE_H
#define SCORE_H

/* Seed the PRNG once at startup. Call from main() before any game mode. */
void score_seed(void);

/* Insert a score entry, keep the file sorted, cap at SCORE_MAX_RECORDS.
   Returns 1 on success, -1 on I/O error. */
int  score_write(const char *login, int points);

/* Print the top N scores to stdout (use after endwin()).
   Returns the number of entries printed, -1 on error. */
int  score_show(int limit);

/* Render the top N scores in a centred ncurses overlay window.
   Call while curses is still active (before endwin()).
   Waits for a keypress then destroys the window. */
void score_show_ncurses(int limit, int has_color);

#endif /* SCORE_H */
