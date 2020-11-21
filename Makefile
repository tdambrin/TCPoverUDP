all: server

utilServer.o: utilServer.c utilServer.h
	gcc -c utilServer.c -o utilServer.o 

server: server.o utilServer.o
	gcc server.o utilServer.o -o server -lm

server.o: server.c
	gcc -c server.c -o server.o

clean:
	rm -r *.o server 
