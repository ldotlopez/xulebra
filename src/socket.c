/*
 * socket.c
 * TCP socket helpers for the Xulebra network layer.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "socket.h"

int net_connect(const char *host, int port)
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
        memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);
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

int net_accept_one(int port)
{
    struct sockaddr_in addr;
    socklen_t          addr_len;
    int                listen_fd, conn_fd;
    int                reuse = 1;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return -1;

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
    close(listen_fd);

    if (conn_fd < 0) return -4;
    return conn_fd;
}
