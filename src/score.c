/*
 * Score.c
 * Functions for write and show top scores
 */

#include "includes.h"
#include "defines.h"
#include "structs.h"

/* This write and create a database */
int write_record ( char *login, int puntos )
{
    int fd, io_bytes, pos = 0;
    trecord *rec_1, *rec_2, *rec_w;

    /* For test only */
    /* printf("Parameters: [%s] [%d]\n",login,puntos); */

    rec_1 = ( void * ) malloc(sizeof(trecord));
    rec_2 = ( void * ) malloc(sizeof(trecord));
    rec_w = ( void * ) malloc(sizeof(trecord));

    /* Obtener el descriptor de fichero */
    if ( (fd = open( DATABASE ,O_RDWR)) < 0 )
    {
        if ( (fd = creat( DATABASE ,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1 )
        {
            return -1 ;
        }
    }

    /* Preparar los datos */
    strncpy(rec_1->login,login,9);
    rec_1->puntos = puntos;

    /* Buscar mi sitio */
    while ( read(fd,rec_2,sizeof(trecord)) > 0 )
    {
        /* Volver a la posicion actual */
        pos = lseek(fd,0,SEEK_CUR) - sizeof(trecord);
        lseek(fd,pos,SEEK_SET);

        /* Comparar los dos registros */
        if ( rec_1->puntos > rec_2->puntos )
        {
            *rec_w = *rec_1;
            *rec_1 = *rec_2;
        }
        else
        {
            *rec_w = *rec_2;
        }
        write(fd,rec_w,sizeof(trecord));
    }
    /* For test only */
    /* printf("Write [%s] [%d] at [%d]\n",rec_1->login,rec_1->puntos,lseek(fd,0,SEEK_CUR)); */
    write(fd,rec_1,sizeof(trecord));
    close(fd);
    return 1;
}

/* Show the database */
int see_records ( int how_much )
{
    trecord *regi;
    int fd;

    regi = ( trecord * ) malloc(sizeof(trecord));

    /* Obtener el descriptor de fichero */
    if ( (fd = open( DATABASE ,O_RDWR)) < 0 )
    {
        if ( (fd = creat( DATABASE ,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1 )
        {
            return -1 ;
        }
    }

    while ( (how_much--) && (read(fd,regi,sizeof(trecord))) )
    {
        printf("* %s: %d Points\n",regi->login,regi->puntos);
    }
}

