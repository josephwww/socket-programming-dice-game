#compile and run on Linux
#tested in ubuntu desktop

default: server

server.o: server.c server.h
	gcc -std=gnu99 -c server.c -o server.o

server: server.o
	gcc server.o -o server

clean:
	-rm -f server.o
	-rm -f server
