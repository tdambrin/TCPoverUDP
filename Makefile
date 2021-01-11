all: server

utilServer.o: utilServer2.c utilServe2r.h
	gcc -c utilServer2.c -o utilServer2.o 

server: server.o utilServer2.o
	gcc server2.o utilServer2.o -o server -lm

server.o: server2.c
	gcc -c server2.c -o server2.o

clean:
	rm -r *.o server 
