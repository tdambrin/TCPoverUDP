#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/time.h>
#include "utilServer.h"


#ifndef RCVSIZE
	#define RCVSIZE 1024
#endif

#ifndef SEQUENCELEN
	#define SEQUENCELEN 6
#endif

int main (int argc, char *argv[]) {

  // ------------------------------------- CONFIG ----------------------------------------
  int port1= 2567;
  int port2 = 2568;
  if (argc > 1){
	port1 = atoi(argv[1]);
	printf("Port = %i, %i\n",port1, port2);
  }

  struct sockaddr_in client, adresse_udp;
  int valid= 1;
  socklen_t clientLen= sizeof(client);
  char buffer[RCVSIZE];
  fd_set set;
  FD_ZERO(&set);

  //create socket
  int sock_co_udp = socket(AF_INET, SOCK_DGRAM, 0);


  //handle error
  if (sock_co_udp < 0) {
    perror("Cannot create socket\n");
    return -1;
  }

  setsockopt(sock_co_udp, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));

  memset(&client, 0, sizeof(client));
  memset(&adresse_udp, 0, sizeof(adresse_udp));


  adresse_udp.sin_family = AF_INET;
  adresse_udp.sin_port = htons(port1);
  adresse_udp.sin_addr.s_addr = htonl(INADDR_ANY); //modif 


  if (bind(sock_co_udp, (struct sockaddr*) &adresse_udp, sizeof(adresse_udp)) == -1) {
	  perror("Bind sockudp failed\n");
	  close(sock_co_udp);
	  return -1;
  }


// -------------------------------- ACCEPT CONNEXION AND HANDLE IF CHILD PROCESSES -------------------------------------
int fragSize = 1500;
int com_sockets[100]; // used to store clients' sockets descriptors
int sync; 
char* msgType = (char*) malloc(4); // on donnera pour l'instant les requetes sous la forme ABC_ ou ABC = {GET, ...}
while (1) {
	int newPort = port1 + 1;
  	sync = synchro(sock_co_udp, client, newPort);
	if (sync > 1){ //son process who created a new socket, meaning sync = descriptor of newly created socket
		close(sock_co_udp);
		int msgSock = sync;
	  	com_sockets[newPort -1 - port1] = msgSock; // add to socket table
		int msgSize;
	  	msgSize = recvfrom(msgSock, buffer, RCVSIZE, MSG_WAITALL, (struct sockaddr*)&client, &clientLen);
	  	buffer[msgSize]='\0';
		readAndSendFile(msgSock, client, buffer, fragSize - SEQUENCELEN, SEQUENCELEN, 1); //current ack must be shared between son processes

		close(msgSock);
		break; //connexion closed

	}else if (sync == 1){ // parent process -> keep accepting connexions
		printf("ONE CLIENT SUCCESSFULLY SYNCHRO\n");
	  	newPort ++;

	}else{ // synchro failed
	  	printf("COULD NOT SYNC ONE CLIENT\n");
	}
}
	close(sock_co_udp);
	return 0;

}