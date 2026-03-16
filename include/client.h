/*
 * client.h
 * Network game client public interface.
 */

#ifndef CLIENT_H
#define CLIENT_H

/* Connect to the server, run the game loop, then clean up.
   argc/argv are forwarded from main() for flag parsing. */
void client_run(int argc, char *argv[]);

#endif /* CLIENT_H */
