/*
 * xulnet.h
 * Definiciones y estructuras comunes al servidor y al cliente
 */

/* Definiciones importantes */
#define DEFAULT_HOST	"localhost"
#define DEFAULT_PORT	2580
#define PACKET_SIZE	5 
#define LOGIN_LEN	9

#define ALTO 20
#define ANCHO 30
#define TOTAL_ANCHO 80
#define TOTAL_ALTO 25

#define ANUBIS 1

/* Definiciones de teclas */
#define UP		KEY_UP
#define DOWN		KEY_DOWN
#define LEFT		KEY_LEFT
#define RIGHT		KEY_RIGHT

#ifdef ANUBIS
#define NOBUTTON	-1
#else 
#define NOBUTTON	-2
#endif

/* Tipos de movimientos */
#define MOVE		1
#define GROW		2
#define D_MOVE		3
#define D_GROW		4
#define I_COL		5
#define I_EAT		6
#define HE_COL		7
#define HE_EAT		8
#define DRAW		9
#define I_TOUCH_HE	10
#define HE_TOUCH_ME	11
#define I_FINISH_GROW	12
#define ST_APPLE	13
#define EAT_APPLE	14
#define NEW_APPLE	15
#define PAUSE		16

/* Estructuras varias */
struct tnode
{
    int x, y;
    struct tnode *next;
};

struct tsnake
{
    struct tnode *head;
    struct tnode *tail;
    int dir;

};

