#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/access.h>
#define VERSION_FILE "/tmp/xulebra/version"

void control_c ( void )
{
	signal ( SIGINT, control_c);
}

void help(void)
{
	printf("Xulebra by XuZo '00-'01\n"
	"Version 0.1 testing\n"
	"Opciones:\n"
	"        -1 Modo un solo jugador\n"
	"        -s Modo servidor\n"
	"        -c Modo cliente\n"
	"        -u Actualizar la xulebra (no implementada)\n"
	"Comunicar fallos a xuzo :)\n");
}

main ( int argc, char *argv[] )
{
	int i;
	char mode;
	int fd;
	char lock_file[32];	

	/* Error en los argumentos */
	if ( argc < 2 )
	{
		printf("Prueba xulebra -h\n"); exit(1);
	}
	
	/* Seleccionar subfuncion */
	if ( argv[1][0] == '-' )
	switch ( argv[1][1] )
		{
			case '1':
				mode = '1';
				break;
			case 's':
				mode = 's';
				break;
			case 'c':
				mode = 'c';
				break;
			case 'h':
				mode = 'h';
				break;
			default:
				printf("Opcion incorrecta\n");
				exit(1);
		}
	
	/* Eliminar el primer argumento */
	for ( i = 1; i < argc - 1; i++ )
		argv[i] = argv[i+1];
	argc--;
	free(argv[argc]);
	argv[argc] = NULL;

	signal ( SIGINT, control_c);

	/* Imprimir los argumentos k me kedan */
	switch ( mode )
	{
		case '1': one_player(argc, argv);
			  break;
		case 's':
			/* Ver si ya se esta ejecutando */ 
			sprintf(lock_file,"%s/.xulebra.lock",getenv("HOME"));
			if ( !access(lock_file,R_OK) ) 
			{
				fprintf(stderr,"%s existe\n",lock_file);
				exit(1);
			}
			
			/* Ahora lo creo */
			fd = creat(lock_file, S_IRUSR | S_IWUSR );
			close(fd);
			if ( fork() )
			{
				/* Padre */
				xulebra_server(argc,argv);
				wait(&i);
				/* Cuando el servidor acabe eliminar el lock */
				if ( remove(lock_file) != 0 )
					fprintf(stderr,"??!! El archivo %s no existia!\n",lock_file);
			}
			else
			{
				/* Hijo */
				xulebra_client(argc,argv);
				exit(0);
			}
			break;
		case 'c': xulebra_client(argc,argv);
			  break;
		case 'h': help();
			  break;
	}
}
