/*
 * xn-server.c
 * Servidor para juego en red de la xulebra
 * Los comentarios de C++ (//) son usados para el debug
 */

/* Includes necesarios */
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "server.h"
#include "xulnet.h"

struct tsnake snakes[2];
char apple_x = -1, apple_y = -1;

/* Add a block to snake */
void add_block ( int pl, int x, int y )
{
    struct tnode *aux;

    aux = (struct tnode *) malloc(sizeof(struct tnode));
    aux->x = x; aux->y = y;
    aux->next = NULL;

    /* Get a sure head */
    if ( snakes[pl].head == NULL )
        snakes[pl].head = aux;
    /* Correct tail */
    if ( snakes[pl].tail != NULL )
        snakes[pl].tail->next = aux;
    /* Add aux */
    snakes[pl].tail = aux;

    // fprintf(stderr,"PL (%d): added block [%d,%d]\n",pl+1,x,y);
}

/* Delete a block from a snake */
void del_block ( int pl )
{
    struct tnode *trashme;

    // fprintf(stderr,"PL (%d): deleted block [%d,%d]\n",pl+1,snakes[pl].head->x, snakes[pl].head->y);

    trashme = snakes[pl].head;
    snakes[pl].head = snakes[pl].head->next;
    free(trashme);

}

/* Lista los bloques de la serpiente */
void list_blocks(int pl)
{
    struct tnode *aux;
    aux = snakes[pl].head;
    // fprintf(stderr,"PL (%d) list: ",pl+1);
    while ( aux != NULL )
    {
        // fprintf(stderr,"[%d,%d] ",aux->x,aux->y);
        aux = aux->next;
    }
    // fprintf(stderr,"\n");
}

/* Self explained */
int xs_point_is_in_list(int pl, int x, int y)
{
    struct tnode *aux;
    aux = snakes[pl].head;
    while ( aux != NULL )
    {
        if ( aux->x == x )
            if ( aux->y == y )
                return 1;
        aux = aux->next;
    }
    return 0;
}

void generate_apple ( void )
{
    srandom(time(NULL));
    do
    {
        apple_x = ( random() % ANCHO ) + 1;
        apple_y = ( random() % ALTO ) + 1;
    }
    while ( (xs_point_is_in_list(0,apple_x,apple_y) ||
             xs_point_is_in_list(1,apple_x,apple_y)) );

    // fprintf(stderr,"Server: Apple generated [%d, %d]\n",apple_x, apple_y);
}

void xulebra_server ( int argc, char *argv[] )
{
    /* Constantes tontas */
    int nums[2];

    /* Variables varias */
    int pls[2], i, j, online = 1;
    char packets[2][PACKET_SIZE];
    char names[2][LOGIN_LEN];
    int port = DEFAULT_PORT;

    /* Mirar mis argumentos */
    for ( i = 1; i < argc; i++ )
    {
        if ( argv[i][0] == '-' )
        {
            switch ( argv[i][1] )
            {
            case 'p':
                if ( argv[i+1] != NULL )
                {
                    i++;
                    port = atoi(argv[i]);
                    if ( port < 1024 )
                    {
                        fprintf(stderr,"Usa un puerto mayor que 1024\n");
                        return; 
                    }
                    fprintf(stderr,"Usando puerto %d para conexiones\n",port);
                }
                else
                {
                    fprintf(stderr,"La opcion -%c necesita conocer el puerto\n",argv[i][1]);
                    return; 
                }
                break;
            default:
                fprintf(stderr,"%s: Opcion no reconocida\n",argv[i]);
                return; 
                break;
            }
        }
    }

    /* Asignar numeros de jugador */
    nums[0] = 0; nums[1] = 1;

    /* Cojer ambos jugadores */
    for ( i = 0; i <= 1 ; i++ )
    {
        // fprintf(stderr,"Waiting for player %d...",i+1); fflush(stdout);
        pls[i] = abre_server(port);
        read(pls[i],names[i],LOGIN_LEN);
        write(pls[i],&nums[i],sizeof(int));
        // fprintf(stderr,"Ok. (%s)\n",names[i]);
        snakes[0].head == NULL;
        snakes[0].tail == NULL;
    }

    write(pls[0],names[1],LOGIN_LEN);
    write(pls[1],names[0],LOGIN_LEN);

    /* Empezar a leer packetes y mostrar actividad */
    while ( online )
    {
        /* Get players packet */
        for ( i = 0; i <= 1; i++ )
            online = read(pls[i],packets[i],PACKET_SIZE) && online;

        /* Antes de nada modificar las estructuras de las serpientes
           y cambiar las etiquetas */

        /* Serpiente 1 */
        switch ( packets[0][0] )
        {
            /* La serpiente se mueve sin mas */
        case MOVE:
            /* Put debug */
            // fprintf(stderr,"PL (1): Move New [%d,%d] Del [%d,%d]\n",packets[0][1],packets[0][2],packets[0][3],packets[0][4]);

            /* Add-Del blocks */
            add_block(0,packets[0][1],packets[0][2]);
            del_block(0);

            /* Change label */
            packets[0][0] = D_MOVE;

            break;

            /* La serpiente esta naciendo */
        case GROW:
            /* Put debug */
            // fprintf(stderr,"PL (1): Grow New [%d,%d]\n",packets[0][1],packets[0][2]);

            /* Add block */
            add_block(0,packets[0][1],packets[0][2]);

            /* Change label */
            packets[0][0] = D_GROW;

            break;

            /* La serpiente ha colisionado con la pared */
        case I_COL:
            /* Put debug */
            // fprintf(stderr,"PL (1): Colision [%d,%d]\n",packets[0][1],packets[0][2]);

            /* Change label */
            packets[0][0] = HE_COL;

            break;

            /* La serpiente se ha mordido */
        case I_EAT:
            /* Put debug */
            // fprintf(stderr,"PL (1): Self eat [%d,%d]\n",packets[0][1],packets[0][2]);

            /* Change label */
            packets[0][0] = HE_EAT;

            break;

        case I_FINISH_GROW:
            /* Put debug */
            generate_apple();
            // fprintf(stderr,"Server: First apple generate [%d,%d]\n",apple_x, apple_y);
            // fprintf(stderr,"PL (1): Finish grow\n");
            packets[0][0] = ST_APPLE;
            packets[0][1] = apple_x;
            packets[0][2] = apple_y;
            break;

        case EAT_APPLE:
            /* Debug */
            // fprintf(stderr,"PL (1): Eat apple [%d,%d]\n",packets[0][1],packets[0][2]);

            /* Add block */
            add_block(0,packets[0][1],packets[0][2]);

            /* Generate apple */
            generate_apple();

            /* Say to PL 1 new apple pos */
            packets[0][0] = NEW_APPLE; packets[0][1] = apple_x; packets[0][2] = apple_y;
            write(pls[0], packets[0], PACKET_SIZE);

            break;

            /* Paquete desconocido */
        default:
            // fprintf(stderr,"PL (1): Unknow %d\n",packets[0][0]);
        }

        /* Serpiente 2 */
        switch ( packets[1][0] )
        {
            /* La serpiente se mueve sin mas */
        case MOVE:
            /* Put debug */
            // fprintf(stderr,"PL (2): Move New [%d,%d] Del [%d,%d]\n",packets[1][1],packets[1][2],packets[0][3],packets[0][4]);

            /* Add-Del blocks */
            add_block(1,packets[1][1],packets[1][2]);
            del_block(1);

            /* Change label */
            packets[1][0] = D_MOVE;

            break;

            /* La serpiente esta naciendo */
        case GROW:
            /* Put debug */
            // fprintf(stderr,"PL (2): Grow New [%d,%d]\n",packets[1][1],packets[1][2]);

            /* Add block */
            add_block(1,packets[1][1],packets[1][2]);

            /* Change label */
            packets[1][0] = D_GROW;

            break;

            /* La serpiente ha colisionado con la pared */
        case I_COL:
            /* Put debug */
            // fprintf(stderr,"PL (2): Colision [%d,%d]\n",packets[1][1],packets[1][2]);

            /* Change label */
            packets[1][0] = HE_COL;

            break;

            /* La serpiente se ha mordido */
        case I_EAT:
            /* Put debug */
            // fprintf(stderr,"PL (2): Self eat [%d,%d]\n",packets[1][1],packets[1][2]);

            /* Change label */
            packets[1][0] = HE_EAT;

            break;

            /* La serpiente ha terminado de crecer */
        case I_FINISH_GROW:
            /* Put debug */
            // fprintf(stderr,"PL (2): End grow\n");
            packets[1][0] = ST_APPLE;
            packets[1][1] = apple_x;
            packets[1][2] = apple_y;
            break;

        case EAT_APPLE:
            /* Debug */
            // fprintf(stderr,"PL (1): Eat apple [%d,%d]\n",packets[1][1],packets[1][2]);

            /* Add block */
            add_block(1,packets[1][1],packets[1][2]);

            /* Generate apple */
            generate_apple();

            /* Say to PL 2 new apple pos */
            packets[1][0] = NEW_APPLE; packets[1][1] = apple_x; packets[1][2] = apple_y;
            write(pls[1], packets[1], PACKET_SIZE);

            break;

            /* Paquete desconocido */
        default:
            // fprintf(stderr,"PL (2): Unknow %d\n",packets[1][0]);
        }

        /* Ahora compruebo las interfereccias entre las serpientes */

        /* Puede ser que ambas serpientes se kieran poner en la misma posicion */
        if (	packets[0][1] == packets[1][1] &&
                packets[0][2] == packets[1][2] )
        {
            packets[0][0] = DRAW;
            packets[1][0] = DRAW;
        }

        /* Que una muerda a la otra */
        else if ( xs_point_is_in_list(0,packets[1][1],packets[1][2]) )
        {
            /* El segundo jugador muerde al primero */
            packets[0][0] = I_TOUCH_HE;
            packets[1][0] = HE_TOUCH_ME;
        }
        else if ( xs_point_is_in_list(1,packets[0][1],packets[0][2]) )
        {
            /* El primer jugador muerde al segundo */
            packets[1][0] = I_TOUCH_HE;
            packets[0][0] = HE_TOUCH_ME;
        }

        /* Debug y enviar paketes */
        list_blocks(0); list_blocks(1);
        write(pls[0],packets[1],PACKET_SIZE);
        write(pls[1],packets[0],PACKET_SIZE);
    }

    /* Parar el server */
    // fprintf(stdout,"Shuting down..."); fflush(stdout);
    sleep(1);
    // fprintf(stdout,"Ok.\n");
}

