/*
 * server.h
 * Network game server public interface.
 */

#ifndef SERVER_H
#define SERVER_H

/* Accept two players (and optionally a spectator), run the game loop,
   then clean up.  argc/argv are forwarded from main() for flag parsing. */
void server_run(int argc, char *argv[]);

#endif /* SERVER_H */
