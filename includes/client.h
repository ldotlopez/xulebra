/* client.h */

#include <stdio.h>
#include <strings.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

/* Devuelve el valor del socket adonde se ha conectado */
int abre_client(char *server, int port)
{
    struct sockaddr_in blah;
    struct hostent *he;
    int sock;

    if ( (sock = socket(AF_INET,SOCK_STREAM,0)) == -1)
    {
        /*
        perror("socket");
        return(-2);
        */
        return -1;
    }

    bzero( (char *) &blah, sizeof( blah ) );
    blah.sin_family=AF_INET;
    blah.sin_addr.s_addr=inet_addr( server );
    blah.sin_port=htons( port );

    if ( ( he = gethostbyname( server ) ) != NULL)
    {
        bcopy( he->h_addr, (char *) &blah.sin_addr, he->h_length );
    }
    else
    {
        if ( ( blah.sin_addr.s_addr = inet_addr( server ) ) < 0 )
        {
            /*
            perror("gethostbyname");
            return(-3);
            */
            return -2;
        }
    }

    if (connect( sock, (struct sockaddr *) &blah, 16 ) == -1 )
    {
        /*
        perror("connect");
        close(sock);
        return(-4);
        */
        close(sock);
        return -3;
    }
    return sock;
}

