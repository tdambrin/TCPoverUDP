all: client server

utilServer.o: utilServer.c utilServer.h
	gcc -c utilServer.c -o utilServer.o 

utilClient.o: utilClient.c utilClient.h
	gcc -c utilClient.c -o utilClient.o

client: client.o utilClient.o
	gcc client.o utilClient.o -o client -lm

client.o: client.c
	gcc -c client.c -o client.o

server: server.o utilServer.o
	gcc server.o utilServer.o -o server -lm

server.o: server.c
	gcc -c server.c -o server.o

clean:
	rm -r *.o server client
