/*
 * socket.h
 * Thin wrappers around BSD socket calls for client and server roles.
 *
 * Both functions return a connected/accepted socket file descriptor
 * on success, or a negative error code on failure.
 *
 * Error codes
 *   -1  socket() failed
 *   -2  bind() / gethostbyname() failed
 *   -3  listen() / connect() failed
 *   -4  accept() failed  (server only)
 */

#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

/* ── Client ─────────────────────────────────────────────────── */

/*
 * net_connect – open a TCP connection to host:port.
 *
 * @host  Hostname or dotted-decimal IPv4 address.
 * @port  Port number (host byte order).
 *
 * Returns the socket fd on success, or a negative error code.
 */
static inline int net_connect(const char *host, int port)
{
    struct sockaddr_in addr;
    struct hostent    *he;
    int                sock;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((short)port);

    he = gethostbyname(host);
    if (he != NULL) {
        memcpy(&addr.sin_addr, he->h_addr, (size_t)he->h_length);
    } else {
        addr.sin_addr.s_addr = inet_addr(host);
        if (addr.sin_addr.s_addr == (in_addr_t)(-1)) {
            close(sock);
            return -2;
        }
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -3;
    }

    return sock;
}

/* ── Server ─────────────────────────────────────────────────── */

/*
 * net_accept_one – bind to port, wait for exactly one incoming connection,
 *                  and return the accepted socket fd.
 *
 * The listening socket is closed before this function returns so that the
 * port is not held open unnecessarily.
 *
 * @port  Port number to listen on (host byte order). Must be > 1023.
 *
 * Returns the accepted socket fd on success, or a negative error code.
 */
static inline int net_accept_one(int port)
{
    struct sockaddr_in addr;
    socklen_t          addr_len;
    int                listen_fd, conn_fd;
    int                reuse = 1;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return -1;

    /* Allow immediate reuse of the port after a restart */
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
               (const void *)&reuse, sizeof(reuse));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((short)port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(listen_fd);
        return -2;
    }

    if (listen(listen_fd, 1) < 0) {
        close(listen_fd);
        return -3;
    }

    addr_len = sizeof(addr);
    conn_fd  = accept(listen_fd, (struct sockaddr *)&addr, &addr_len);

    close(listen_fd);   /* No longer needed once we have conn_fd */

    if (conn_fd < 0) return -4;
    return conn_fd;
}

#endif /* NET_SOCKET_H */
