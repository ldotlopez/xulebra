/*
 * xulnet.c (cliente)
 * By XuZo
 */

/* Inlcludes */
#include <curses.h>
#include "client.h"
#include "xulnet.h"

/* Client defines */
#define P1_START_X 1
#define P1_START_Y 1
#define P2_START_X 30 
#define P2_START_Y 20 
#define LEN 10 
#define UPDATE(x) wnoutrefresh(x); doupdate()

/* Global variables */
WINDOW *game, *info;
struct tsnake snake;
int sock, number;
char packet[PACKET_SIZE];
char xc_apple_x, xc_apple_y;
char my_login[LOGIN_LEN];
char p2_login[LOGIN_LEN];
int apples[2] = {0,0};
int port = DEFAULT_PORT;
char host[256] = DEFAULT_HOST;

/* Predeclare funtions */
void initsnake( void );
void initcurses ( void );
void initinfo ( void );
void updateinfo ( int pl );
void initsocket ( void );
void xc_add_block ( int x, int y );
struct tnode xc_del_block ( void );
void xc_draw_snake ( void );
int coor_in_list(int x, int y);
int multi_move ( int a, struct tnode *f);
void eat_fake_apple ( struct tnode f);
void moveme(struct tnode f);
void force_exit(char *reason);
void self_eat ( struct tnode f);
void xc_eat_apple (void);
void logo(void);

/* Inits snake with default values */
void initsnake( void )
{
    int i;
    struct tnode fake_apple;

    snake.head = NULL;
    snake.tail = NULL;

    if ( number == 0)
    {
        for ( i = 0; i < LEN; i++ )
        {
            fake_apple.x = i + P1_START_X; fake_apple.y = P1_START_Y;
            eat_fake_apple( fake_apple );
            read(sock,packet,PACKET_SIZE);
            mvwaddch(game,packet[2],packet[1] * 2,'+');
        }
        snake.dir = RIGHT;
    }
    else
    {
        for ( i = 0; i < LEN; i++ )
        {
            fake_apple.x = P2_START_X - i; fake_apple.y = P2_START_Y;
            eat_fake_apple( fake_apple );
            read(sock,packet,PACKET_SIZE);
            mvwaddch(game,packet[2],packet[1] * 2,'+');
        }
        snake.dir = LEFT;
    }
    xc_draw_snake();
}

void initinfo ( void )
{
    mvwaddstr(info,1,1,"Jugador 1:");
    mvwaddstr(info,3,1,"Jugador 2:");

    mvwaddstr(info,2+(number*2) ,1,my_login);
    mvwaddstr(info,4-(number*2) ,1, p2_login);

    mvwaddstr(info,6,1,"P1: 0");
    mvwaddstr(info,7,1,"P2: 0");

    UPDATE(info);
}

void updateinfo ( int pl )
{
    char lala[4];

    if ( pl == 0 )
    {
	/* He comido yo */
	if ( number == 0 )
	{
		/* Soy primer player */
		apples[0] += 1;
		sprintf(lala,"%d",apples[0]);
		mvwaddstr(info,6,5,lala);
	}
	else
	{
		/* Soy el jugador 2 */
		apples[1] += 1;
		sprintf(lala,"%d",apples[1]);
		mvwaddstr(info,7,5,lala);
	}
    }
    else
    {
	/* Se la ha comido el otro */
	if ( number == 0 )
	{
		/* El otro es el jugador 2 */
		apples[1] += 1;
		sprintf(lala,"%d",apples[1]);
		mvwaddstr(info,7,5,lala);
	}
	else
	{
		/* El otro jugador es el 1 */
		apples[0] += 1;
		sprintf(lala,"%d",apples[0]);
		mvwaddstr(info,6,5,lala);
	} 
    }
    UPDATE(info);
}

/* All for begin using curses */
void initcurses ( void )
{
    initscr();
    nonl();
    noecho();
    cbreak();
}

/* Get a socket */
void initsocket ( void )
{
    char lala[100];
    char *aux;
    sock = abre_client(host, port);
    if ( sock < 0 )
    {
        mvaddstr(7,1,"No se ha conseguido la conexion"); UPDATE(stdscr); sleep(1);
	endwin();
        return; 
    }

    aux = (char *) getenv("IRCUSER");
    if ( aux == NULL )
	   aux = (char *) getenv("LOGNAME");
    
    strncpy(my_login,aux,LOGIN_LEN);
    write(sock,my_login, LOGIN_LEN);
    sprintf(lala,"Socket: %d",sock);
    mvaddstr(8,1,lala);
    sleep(1);
    read(sock,&number,sizeof(int) );
    sprintf(lala,"Somos el jugador %d",number + 1);
    mvaddstr(9,1,lala); UPDATE(stdscr);
    read(sock,p2_login, LOGIN_LEN);
}

/* Add a block to snake */
void xc_add_block ( int x, int y )
{
    struct tnode *aux;

    /* Build node */
    aux = (struct tnode *) malloc ( sizeof(struct tnode) );
    aux->x = x; aux->y = y;
    aux->next = NULL;

    /* Si la cabeza en nula que apunte a aux */
    if ( snake.head == NULL )
        snake.head = aux;

    /* Si ya existe alguno en cola que señale el siguiente a aux */
    if ( snake.tail != NULL )
        snake.tail->next = aux;

    snake.tail = aux;
}

/* Returns the head block of the snake and frees it */
struct tnode xc_del_block ( void )
{
    struct tnode likeme;
    struct tnode *trashme;

    /* Value for return */
    likeme = *snake.head;
    /* Get the block to delete */
    trashme = snake.head;
    /* Move the head */
    snake.head = snake.head->next;
    free(trashme);
    return likeme;
}

/* Draws all snake */
void xc_draw_snake ( void )
{
    struct tnode *aux;
    aux = snake.head;
    while ( aux != NULL )
    {
        mvwaddch(game,aux->y,aux->x * 2,'*');
        aux = aux->next;
    }
    UPDATE(game);
}

/* Self explained */
int coor_in_list ( int x, int y )
{
    struct tnode *aux;
    aux = snake.head;
    while ( aux != NULL )
    {
        if ( aux->x == x )
            if ( aux->y == y )
                return 1;
        aux = aux->next;
    }
    return 0;
}

/* Calculate type of next move */
int multi_move ( int a, struct tnode *f )
{
    /* A back atempt ? */
    /* Needs to simplify */
    if ( (abs(a - snake.dir) == 1) && ( a != UP ) && ( a != LEFT ) )
        a = snake.dir;

    /* Calculate future position */
redoit:
    switch ( a )
    {
    case UP:
        f->x = snake.tail->x;
        f->y = snake.tail->y - 1;
        snake.dir = UP;
        break;
    case DOWN:
        f->x = snake.tail->x;
        f->y = snake.tail->y + 1;
        snake.dir = DOWN;
        break;
    case LEFT:
        f->x = snake.tail->x - 1;
        f->y = snake.tail->y;
        snake.dir = LEFT;
        break;
    case RIGHT:
        f->x = snake.tail->x + 1;
        f->y = snake.tail->y;
        snake.dir = RIGHT;
        break;
    case NOBUTTON:
    default:
        a = snake.dir;
        goto redoit;
    }

    /* Out table ? */
    if ( f->x > ANCHO || f->x < 1 || f->y > ALTO || f->y < 1 )
        return 1;

    /* Self eat ? */
    if ( coor_in_list(f->x, f->y) )
        return 2;

    /* Got apple? */
    if ( (xc_apple_x == f->x) && (xc_apple_y == f->y) )
        return 3;

    return 0;

}

/* Function for normal move */
void moveme(struct tnode f )
{
    struct tnode old;
    xc_add_block(f.x, f.y);
    old = xc_del_block();

    mvwaddch(game, f.y, f.x * 2, '*' );
    mvwaddch(game, old.y, old.x * 2, ' ');

    /* Built a move packet */
    packet[0] = MOVE;
    packet[1] = f.x; packet[2] = f.y;
    packet[3] = old.x; packet[4] = old.y;
    write(sock,packet,PACKET_SIZE);
}

void eat_fake_apple(struct tnode f)
{
    xc_add_block(f.x,f.y);
    mvwaddch(game, f.y, f.x * 2, '*' );

    packet[0] = GROW;
    packet[1] = f.x; packet[2] = f.y;
    write(sock,packet,PACKET_SIZE);
}

void xc_eat_apple ( void )
{
    xc_add_block(xc_apple_x,xc_apple_y);
    mvwaddch(game, xc_apple_y, xc_apple_x * 2, '*');

    packet[0] = EAT_APPLE;
    packet[1] = xc_apple_x, packet[2] = xc_apple_y;
    write(sock,packet,PACKET_SIZE);

    /* Wait the new apple */
    read(sock,packet,PACKET_SIZE);
    xc_apple_x = packet[1], xc_apple_y = packet[2];
    mvwaddch(game, xc_apple_y, xc_apple_x * 2, '@');
}

/* Funtion for do a self crash */
void i_crash_me ( struct tnode f )
{
    packet[0] = I_EAT;
    packet[1] = f.x; packet[2] = f.y;
    write(sock,packet,PACKET_SIZE);
}

/* Function for do a exit table */
void out_table (struct tnode f )
{
    mvwaddch(game, f.y, f.x * 2,'#');
    packet[0] = I_COL;
    packet[1] = f.x; packet[2] = f.y;
    write(sock,packet,PACKET_SIZE);
}

/* Exit on event with a reason */
void force_exit(char *reason)
{
    char *linea;
    mvaddstr(12,1," /-------------               ");
    mvaddstr(13,1,"| GAME OVER -->");
    mvaddstr(14,1," \\-------------               ");
    mvaddstr(13,18,reason);
    UPDATE(stdscr);
    UPDATE(game);
    endwin();
    exit(0); 
}

/* Say server i eat myself */
void self_eat ( struct tnode f)
{
    packet[0] = I_EAT;
    packet[1] = f.x;
    packet[2] = f.y;
    write(sock,packet,PACKET_SIZE);
}

void logo (void)
{
    mvaddstr(1,1,"__  __     _      _");
    mvaddstr(2,1,"\\ \\/ /   _| | ___| |__  _ __ __ _");
    mvaddstr(3,1," \\  / | | | |/ _ \\ '_ \\| '__/ _` |");
    mvaddstr(4,1," /  \\ |_| | |  __/ |_) | | | (_| |");
    mvaddstr(5,1,"/_/\\_\\__,_|_|\\___|_.__/|_|  \\__,_|");
    UPDATE(stdscr);
}

/* The main */
void xulebra_client ( int argc, char *argv[] )
{
    int key;
    struct tnode future;

    /* Ver los argumentos */
    for ( key = 1; key < argc; key++ )
    {
        if ( argv[key][0] = '-' )
        {
            switch ( argv[key][1] )
            {
            case 'h':
                if ( argv[key+1] != NULL )
                {
                    fprintf(stdout,"Usando como host %s\n",argv[key+1]);
                    key++;
                    strncpy(host,argv[key],256);
                }
                else
                {
                    fprintf(stderr,"La opcion -%c necesita conocer el host\n",argv[key][1]);
                    return; 
                }
                break;
            case 'p':
                if ( argv[key+1] != NULL )
                {
                    key++;
                    port = atoi(argv[key]);
                    if ( port < 1024 )
                    {
                        fprintf(stderr,"Usa un puerto mayor que 1024\n");
                        return; 
                    }
                    key++;
                    fprintf(stdout,"Usando %d como puerto\n",port);
                }
                else
                {
                    fprintf(stderr,"La opcion -%c necesita conocer el puerto\n",argv[key][1]);
                    return; 
                }
                break;
            default:
                fprintf(stderr,"-%c: Opcion no reconocida\n",argv[key][1]);
                break;
            }
        }
    }

    /* Iniciando las curses */
    initcurses();

    /* Iniciar ambas ventanas */
    game = newwin(ALTO + 2,(ANCHO * 2 ) + 2, 1, 0);
    info = newwin(ALTO + 2, TOTAL_ANCHO - ( ANCHO * 2 ) - 2 , 1 ,(ANCHO * 2) + 2 );

    /* Ver si estan correctas */
    if ( info == NULL || game == NULL )
    {
        fprintf(stderr, "No se han podido crear ventanas\n");
        return; 
    }

    /* Poner el logo */
    logo();

    /* Conseguir una conexion con el host */
    initsocket();
    sleep(1);

    /* Spam */
    mvaddstr(0,3,"Xulebra v0.1 By Xuzo"); UPDATE(stdscr);

    /* Poner bordes */
    wborder(game,ACS_VLINE,ACS_VLINE,ACS_HLINE,ACS_HLINE,
            ACS_ULCORNER,ACS_URCORNER,ACS_LLCORNER,ACS_LRCORNER);
    wborder(info,ACS_VLINE,ACS_VLINE,ACS_HLINE,ACS_HLINE,
            ACS_ULCORNER,ACS_URCORNER,ACS_LLCORNER,ACS_LRCORNER);

    /* Iniciar la serpiente */
    initsnake();
    initinfo();

    /* Avisar al server y conseguir la primera manzana */
    packet[0] = I_FINISH_GROW;
    write(sock,packet,PACKET_SIZE);
    read(sock,packet,PACKET_SIZE);

    /* Aqui debo manejar la recepcion de la manzana */
    xc_apple_x = packet[1]; xc_apple_y = packet[2];
    mvwaddch(game,xc_apple_y, xc_apple_x * 2 ,'@');

    keypad(game,TRUE);
    wtimeout(game,100);

    /* Actualizar pantalla */
    UPDATE(game); UPDATE(info);

    /* The game */
    for (;;)
    {
        key = wgetch(game);

        /* Analize key and do anything */
        switch ( multi_move(key,&future) )
        {
        case 0:
            moveme(future);
            break;
        case 1:
            out_table(future);
            force_exit(":( Te has estrellado");
            break;
        case 2:
            self_eat(future);
            force_exit(":( Te has moridido");
            break;
        case 3:
            xc_eat_apple();
            updateinfo(0);
            break;
        }

        /* Now listen server response */
        read(sock,packet,PACKET_SIZE);
        switch ( packet[0] )
        {
        case D_MOVE:
            mvwaddch(game,packet[2],packet[1] * 2,'+');
            mvwaddch(game,packet[4],packet[3] * 2,' ');
            break;
        case D_GROW:
            mvwaddch(game,packet[2],packet[1] * 2,'+');
            break;
        case HE_COL:
            mvwaddch(game, packet[2], packet[1] * 2,'#');
            force_exit(":) Se ha estrellado");
            break;
        case HE_EAT:
            force_exit(":) Se ha mordido");
            break;
        case HE_TOUCH_ME:
            force_exit(":) Ha chocado contra ti");
            break;
        case I_TOUCH_HE:
            force_exit(":( Has tocado contra el");
            break;
        case DRAW:
            mvwaddch(game,packet[2],packet[1] * 2,'#');
            force_exit("DRAW MATCH");
            break;
        case NEW_APPLE:
            updateinfo(1);
            mvwaddch(game,xc_apple_y, xc_apple_x * 2, '+');
            xc_apple_x = packet[1], xc_apple_y = packet[2];
            mvwaddch(game,xc_apple_y, xc_apple_x * 2, '@');
            break;
            /*
            case PAUSE:
                force_pause();
                break;
            */
        default:
            ;
        }
        UPDATE(game);
    }

    endwin();
}

