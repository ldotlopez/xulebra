all:
	gcc -c -I./includes -O2 src/xulebra.c -o src/xulebra.o
	gcc -c -I./includes -O2 src/xulebra-server.c -o src/xulebra-server.o
	gcc -c -I./includes -O2 src/xulebra-client.c -o src/xulebra-client.o
	gcc -c -I./includes -O2 src/score.c -o src/score.o
	gcc -I./includes -O2 -lcurses -o bin/xulebra src/main.c src/xulebra.o src/xulebra-server.o src/xulebra-client.o src/score.o
	strip bin/xulebra
clean:
	rm -rf src/*.o src/*~ bin/xulebra
