/* server.h */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>

int abre_server ( int puerto )
{
    struct sockaddr_in addr;
    struct hostent *host;
    int sockListen;
    int addrLen;
    int sock;
    char ch;
    short port;
    int val1;
    struct linger val2;

    port = puerto;

    /* Se supone k esto limpia la zona de memoria */
    memset(&addr, 0, sizeof(addr));

    /* Asigna el tipo de socket a addr */
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    /* Abre un socket */
    sockListen = socket(AF_INET, SOCK_STREAM, 0);

    /* Checkeo */
    if (sockListen < 0)
    {
        return -1;
        /*
        perror("socket");
        exit(1);
        */
    }
    val1 = 1;

    /* Operacion de servidor */
    setsockopt(sockListen, SOL_SOCKET, SO_REUSEADDR, (void *)&val1, sizeof(val1));

    /* Pedir servicio, abrir servidor */
    if (bind(sockListen, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        /*
        perror("bind");
        exit(1);
        */
        return -2;
    }

    if (listen(sockListen, 1) < 0)
    {
        /*
        perror("listen");
        exit(1);
        */
        return -3;
    }

    addrLen = sizeof(addr);
    sock = accept(sockListen, (struct sockaddr *)&addr,(size_t * )  &addrLen);

    close(sockListen);

    return sock;
}
