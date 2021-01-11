all: server2 server1


server1: server1.o utilServer1.o
	gcc server1.o utilServer1.o -o ../bin/server1_Poly -lm

server2: server2.o utilServer2.o
	gcc server2.o utilServer2.o -o ../bin/server2_Poly -lm

server3: server3.o utilServer3.o
	gcc server3.o utilServer3.o -o ../bin/server3_Poly -lm

server1.o: server1.c
	gcc -c server1.c -o server1.o

server2.o: server2.c
	gcc -c server2.c -o server2.o

server3.o: server3.c
	gcc -c server3.c -o server3.o

utilServer1.o: utilServer1.c utilServer1.h
	gcc -c utilServer1.c -o utilServer1.o 

utilServer2.o: utilServer2.c utilServer2.h
	gcc -c utilServer2.c -o utilServer2.o 

utilServer3.o: utilServer3.c utilServer3.h
	gcc -c utilServer3.c -o utilServer3.o 


clean:
	rm -r *.o ../bin/* 
