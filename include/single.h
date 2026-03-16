/*
 * single.h
 * Single-player game mode public interface.
 */

#ifndef SINGLE_H
#define SINGLE_H

/* Run the single-player snake game (entry point for -1 mode).
   argc/argv are forwarded from main() for flag parsing. */
void single(int argc, char *argv[]);

#endif /* SINGLE_H */