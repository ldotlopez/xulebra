/*
 * structs.h
 * Estructuras para el modo de un jugador
 */

/* Estructura para los puntos */
typedef struct tcoor
{
    int x,y;		/* Posicion X Y	*/
    int form;		/* Forma de vida*/
} tcoor;

/* Estructura para un nodo de tcoor */
typedef struct tnode
{
    tcoor pos;		/* Estrutura a tcoor	*/
    struct tnode *next;	/* Puntero al siguiente */
} tnode;

/* Estructura para la lista */
typedef struct tlist
{
    tnode *head;		/* Pointer to 1st block	*/
    tnode *tail;		/* Pointer to last block*/
    int old_dir;		/* Old direction	*/
    int dir;			/* Direction of snake	*/
    int points;			/* Number of apples	*/
    int speed;			/* Speed of snake	*/
    int init_time;		/* Time init of game	*/
} tlist;

/* Struct for a point */
typedef struct trecord
{
    char login[9];
    int puntos;
} trecord;
