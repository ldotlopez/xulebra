/*
 * socket.h
 * TCP socket helpers for the Xulebra network layer.
 *
 * Return values
 * ─────────────
 *   >= 0  valid socket file descriptor
 *     -1  socket() failed
 *     -2  bind() or gethostbyname() failed
 *     -3  listen() or connect() failed
 *     -4  accept() failed  (net_accept_one only)
 */

#ifndef SOCKET_H
#define SOCKET_H

/*
 * net_connect – open a TCP connection to host:port.
 * @host  Hostname or dotted-decimal IPv4 string.
 * @port  Port number in host byte order.
 */
int net_connect(const char *host, int port);

/*
 * net_accept_one – bind to port, accept one connection, return its fd.
 * The listening socket is closed before the function returns.
 * @port  Port number in host byte order. Must be > 1023.
 */
int net_accept_one(int port);

#endif /* SOCKET_H */
