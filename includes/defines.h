/*
 * defines.h
 * Defines for work snake
 */

/* Move Keys */
#define UP	KEY_UP
#define DOWN	KEY_DOWN
#define LEFT	KEY_LEFT
#define RIGHT	KEY_RIGHT
#ifdef ANUBIS
#define NOBUTTON        -1
#else
#define NOBUTTON        -2
#endif
#define PAUSE 32

/* Default values */
#define ALTO 20
#define ANCHO 30
#define START_X 1
#define START_Y 1
#define START_LONG 3
#define START_DIR 4
#define TOTAL_ANCHO 80
#define TOTAL_ALTO 25
#define SPEED 60
#define MAX_SPEED 10
#define PER_CENT_APPLE 20
#define PER_CENT_UP 2
#define DIF_SPEED 15

/* Snake chars */
#define D_TUB_X '-' 
#define D_TUB_Y '|'
#define ESQ_1 '/'
#define ESQ_2 '\\'
#define ESQ_3 '\\'
#define ESQ_4 '/'
#define CAB_1 '^'
#define CAB_2 'v'
#define CAB_3 '<'
#define CAB_4 '>'
#define DATABASE "/tmp/.xulebra.hof"

#define DOUPDATE(x) wnoutrefresh(x); doupdate();

