/*
 * La Xulebra 
 * Idea original de XuZo Ta-Chan y Lancelot
 * Juego desarollado por XuZo Corp.
 * Pequeñas aportaciones como ideas y pequeños algoritmos
 */

/* Include files */
#include "includes.h"
#include "defines.h"
#include "structs.h"

/* Predeclaration of functions */
void show_title ( void );
void init_info_win ( void );
void update_info ( int type, int value );
void say_bye ( char *message );
int point_is_in_list ( tcoor xy );
tcoor put_block ( tcoor xy );
tcoor get_block ( void );
int type_of_move ( tcoor apple, int tecla, tcoor *future );
void advance_snake ( tcoor new_pos );
void eat_apple ( tcoor new_pos, tcoor *apple );
void put_food ( tcoor *xy );
void beauty_char ( tcoor new_pos );
void init_snake ( void );
void op_draw_snake ( void );
void draw_border ( void );

/* Two principal windows */
tlist player;
WINDOW *game, *info, *options;

/* Show a little title */
void show_title ( void )
{
    char logo[] =
        {
            "__  __     _      _\n"
            " \\ \\/ /   _| | ___| |__  _ __ __ _\n"
            "  \\  / | | | |/ _ \\ '_ \\| '__/ _` |\n"
            "  /  \\ |_| | |  __/ |_) | | | (_| |\n"
            " /_/\\_\\__,_|_|\\___|_.__/|_|  \\__,_|\n"
        } ;
    mvaddstr( 1, 1, logo );
}

/* Draw ALL snake */
void op_draw_snake ( void )
{
    tnode *aux;

    aux = player.head;
    /* Print a block position and move head */
    while ( aux != NULL )
    {
        mvwaddch( game, aux->pos.y, aux->pos.x * 2, '*' );
        aux = aux->next;
    }
}

/* Draw window borders */
void draw_border ( void )
{
    wborder( game, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
             ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER );
    wborder(info, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER );
}

/* Default values in info */
void init_info_win ( void )
{
    /* Name */
    mvwaddstr( info, 1, 1, "Player: " );
    mvwaddstr( info, 1, 9, getenv( "LOGNAME" ) );

    /* Apples */
    mvwaddstr( info, 3, 1, "Apples:   0" );

    /* Speed */
    mvwaddstr( info, 4, 1, "Speed:    100" );

    /* Points */
    mvwaddstr( info, 5, 1, "Points:" );

    DOUPDATE( info );
}

void update_info ( int type, int value )
{
    char tmp_string[15];

    sprintf(tmp_string,"%d   \0",value);
    mvwaddstr(info,type,11,tmp_string);
    DOUPDATE(info);
}

/* Things to do before exit game */
void say_bye ( char *message )
{
    int points;

    beep();
    endwin();
    points = (player.points * 10) - (time(NULL) - player.init_time);

    if ( points > 0 )
        write_record(getenv("LOGNAME"), points );

    see_records(10);
    fprintf(stdout, "-=-=-=-=-=-=] Game Over !!! (%s) [=-=-=-=-=-=-\n", message);
    exit(1);
}

/* Is that point in list ? */
int point_is_in_list ( tcoor xy )
{
    int found = 0;
    tnode *a;

    a = player.head;

    /* No end of list and not yet found */
    while ( (a != NULL) && (!found) )
    {
        /* Update found and pass to next block */
        found = ( (a->pos.x == xy.x) && (a->pos.y == xy.y) );
        a = a->next;
    }
    return found;
}

/* Put new points in the list */
tcoor put_block ( tcoor xy )
{
    tnode *new_node;

    /* Create and write the new block */
    new_node = ( tnode * ) malloc( sizeof(tnode));
    new_node->next = NULL;
    new_node->pos = xy;

    if ( player.head == NULL )
        player.head = new_node;

    if ( player.tail != NULL )
        player.tail->next = new_node;

    player.tail = new_node;
}

/* Return a point and erase it from list */
tcoor get_block ( void )
{
    tcoor old_node;
    tnode *aux;

    /* Get value for return, move head and destroy block */
    old_node = player.head->pos;
    aux = player.head;
    player.head = player.head->next;
    free(aux);
    return old_node;
}

/* Detect type of move for do action */
int type_of_move ( tcoor apple, int tecla, tcoor *future )
{
    tcoor param;
    tlist snak;
    int time_freeze;

    *future = player.tail->pos;
    player.old_dir = player.dir;

    /* Back key */
    switch ( tecla )
    {
    case UP:	if ( player.dir == 2 ) tecla = NOBUTTON; break;
    case DOWN:	if ( player.dir == 1 ) tecla = NOBUTTON; break;
    case LEFT:	if ( player.dir == 4 ) tecla = NOBUTTON; break;
    case RIGHT:	if ( player.dir == 3 ) tecla = NOBUTTON; break;
    }

    /* Which will be xy next block */
    switch ( tecla )
    {
    case UP:
        future->y--; player.dir = 1; break;
    case DOWN:
        future->y++; player.dir = 2; break;
    case LEFT:
        future->x--; player.dir = 3; break;
    case RIGHT:
        future->x++; player.dir = 4; break;
    case PAUSE:
        timeout(-1);
        mvwaddstr(info,7,1,"-= Paused =-");
        DOUPDATE(info);
        fflush(stdin);
        time_freeze = time(NULL);
        getch();
        player.init_time += (time(NULL) - time_freeze);
        mvwaddstr(info,7,1,"            ");
        timeout(player.speed);
    case NOBUTTON:
    default:
        switch ( player.dir )
        {
        case 1:
            future->y--; break;
        case 2:
            future->y++; break;
        case 3:
            future->x--; break;
        case 4:
            future->x++; break;
        }

    }

    /* A colision ? (1) */
    param = *future;
    if ( point_is_in_list(param) )
        return 1;

    /* Out of table ? (2) */
    if ( (future->x > ANCHO) || (future->x < 1) || (future->y > ALTO) || (future->y < 1) )
        return 2;

    /* Eat (3) */
    if ( point_is_in_list(apple) )
        return 3;

    /* Normal move (0) */
    return 0;
}

/* Avanced move snake function */
void advance_snake ( tcoor new_pos )
{
    tcoor old_xy;

    /* Put and Get Blocks */
    old_xy = get_block();
    put_block(new_pos);

    /* Erase */
    mvwaddch(game,old_xy.y, old_xy.x * 2, ' ');

    /* Print correct snake head */
    switch ( player.dir )
    {
    case 1: mvwaddch(game,new_pos.y, new_pos.x * 2, CAB_1);
        break;
    case 2:	mvwaddch(game,new_pos.y, new_pos.x * 2, CAB_2);
        break;
    case 3: mvwaddch(game,new_pos.y, new_pos.x * 2, CAB_3);
        break;
    case 4: mvwaddch(game,new_pos.y, new_pos.x * 2, CAB_4);
        break;
    }
    wmove(game,0,0);
}

/* In case of eat an apple */
void eat_apple ( tcoor new_pos, tcoor *apple )
{
    char infos[15];


    /* Grow snake, grow */
    put_block(new_pos);
    player.points++;

    /* Print */

    mvwaddch(game,apple->y, apple->x * 2, 'O');

    /* Power apple ? */
    switch ( apple->form )
    {
    case 1:
        player.speed = player.speed + DIF_SPEED;
        update_info(4, SPEED - player.speed + 100 );
        timeout( player.speed );
        beep();
        break;
    case -1:
        player.speed = player.speed - DIF_SPEED;
        if ( player.speed <= 0 )
            player.speed = MAX_SPEED;
        update_info(4,SPEED - player.speed + 100);
        timeout(player.speed);
        beep();
    }

    /* Erase apple */
    apple->x = -1;

    /* Info */
    update_info(3,player.points);
    wnoutrefresh(info);
}

void put_food ( tcoor *xy )
{
    tcoor point;

    srandom(time(NULL));
    do
    {
        point.x = random() % ANCHO + 1;
        point.y = random() % ALTO + 1;
    } while ( point_is_in_list(point) );
    mvwaddch(game,point.y, point.x * 2, '@');

    /* Special apple ? */
    if ( (random() % 100) <= PER_CENT_APPLE )
    {
        if ( (random() % 100) % PER_CENT_UP )
            point.form = -1;
        else
            point.form = 1;
    }
    else
        point.form = 0;

    *xy = point;
}

/* Draw snake in beaty ascii */
void beauty_char ( tcoor new_pos )
{
    char ori = '*' ;		/* Oriented char	*/
    int dif_x, dif_y, discr;	/* Counters		*/

    dif_x = new_pos.x - player.tail->pos.x;
    dif_y = new_pos.y - player.tail->pos.y;
    discr = (player.old_dir*100) + ((dif_x+1)*10) + (dif_y+1);

    switch ( discr )
    {
    case 101: ori = ESQ_2;
        break;
    case 110: ori = D_TUB_Y; /* Sigue eje Y- */
        break;
    case 121: ori = ESQ_1 ; /* 1 1 0 */
        break;
    case 201: ori = ESQ_4; /* 2 -1 0 */
        break;
    case 212: ori = D_TUB_Y; /* 2 0 1 */
        break;
    case 221: ori = ESQ_3; /* 2 1 0 */
        break;
    case 301: ori = D_TUB_X; /* 3 -1 0 */
        break;
    case 310: ori = ESQ_3; /* 3 0 -1 */
        break;
    case 312: ori = ESQ_1; /* 3 0 1 */
        break;
    case 410: ori = ESQ_4; /* 4 0 -1 */
        break;
    case 412: ori = ESQ_2; /* 4 0 1*/
        break;
    case 421: ori = D_TUB_X; /* 4 1 0 */ /* Sigo */
        break;
    }
    mvwaddch(game,player.tail->pos.y, player.tail->pos.x * 2,ori);
}

/* Give intial values to snake */
void init_snake ( void )
{
    int x;
    tcoor xy;

    /* NULL it ! */
    player.head = NULL;
    player.tail = NULL;

    /* Snake born here */
    xy.y = START_Y;
    for ( xy.x = START_X ; xy.x < START_LONG; xy.x++ )
        put_block(xy);

    player.dir = START_DIR;
    player.points = 0;
    player.speed = SPEED;
    player.init_time = time(NULL);
}

void one_player ( int argc, char *argv[] )
{
    /* Variables */
    tcoor food;		/* List of apples		*/
    tcoor next_pos;		/* Get the next position	*/
    int key;		/* Key preset			*/
    char infos[10];		/* For buffer info		*/

    /* Init curses */
    initscr();		/* Get standar screen		*/
    nonl();			/* No new line wait		*/
    noecho();		/* No echo print		*/
    show_title();
    getch();
    cbreak();		/* Get ^C ?			*/
    keypad(stdscr,TRUE);	/* Get specials keys		*/
    timeout(SPEED);		/* Time for new key press	*/

    /* Init sub windows */
    game = newwin(ALTO + 2,(ANCHO * 2 ) + 2, 0, 0);
    info = newwin(ALTO + 2, TOTAL_ANCHO - ( ANCHO * 2 ) - 2 , 0 ,(ANCHO * 2) + 2 );
    if ( (info == NULL) || (game == NULL) )
        say_bye("Error creating windows");
    init_info_win();

    init_snake();		/* Put some values on snake	*/
    draw_border();		/* Draw border			*/
    op_draw_snake();		/* Preview of snake		*/
    put_food(&food);	/* Put some food		*/

    /* Real game */
    for (;;)
    {
        /* Wait a time an flush stdin */
        usleep(player.speed * 1000);
        fflush( stdin );

        /* Get a key	*/
        key = getch();

        /* Get type of move: move, eat, collision */
        switch ( type_of_move(food,key,&next_pos) )
        {
            /* Normal move */
        case 0:
            beauty_char(next_pos);
            advance_snake(next_pos);
            break;
            /* Colision	*/
        case 1:
            say_bye("Eat your self");
            break;
            /* Out of table	*/
        case 2:
            say_bye("Out of table");
            break;

            /* Eat, ñam, ñam */
        case 3:
            eat_apple(next_pos,&food);
            break;
        }

        /* No food ? */
        if ( food.x < 0 )
            put_food(&food);

        /* Refresh all screens */
        update_info(5,(player.points * 10) - (time(NULL) - player.init_time));
        wnoutrefresh(game);
        doupdate();
    }
}
