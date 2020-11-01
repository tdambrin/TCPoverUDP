#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	#define SEQUENCELEN 4
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
//  FD_SET(server_desc, &set);
//  FD_SET(sock_udp, &set);

  //handle error
  if (sock_co_udp < 0) {
    perror("Cannot create socket\n");
    return -1;
  }

//  setsockopt(server_desc, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));
  setsockopt(sock_co_udp, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));

 // memset(&adresse, 0, sizeof(adresse));
  memset(&client, 0, sizeof(client));
  memset(&adresse_udp, 0, sizeof(adresse_udp));

/*  adresse.sin_family= AF_INET;
  adresse.sin_port= htons(port1);
  adresse.sin_addr.s_addr= htonl(INADDR_ANY);*/

  adresse_udp.sin_family = AF_INET;
  adresse_udp.sin_port = htons(port1);
  adresse_udp.sin_addr.s_addr = htonl(INADDR_ANY); //modif 

  //initialize socket
  /*if (bind(server_desc, (struct sockaddr*) &adresse, sizeof(adresse)) == -1) {
    perror("Bind failed\n");
    close(server_desc);
    return -1;
  }*/

  if (bind(sock_co_udp, (struct sockaddr*) &adresse_udp, sizeof(adresse_udp)) == -1) {
	  perror("Bind sockudp failed\n");
	  close(sock_co_udp);
	  return -1;
  }


  //listen to incoming clients
  /*if (listen(server_desc, 0) < 0) {
    printf("Listen failed\n");
    return -1;
  }*/

//  printf("Listen done\n");

// -------------------------------- ACCEPT CONNEXION AND HANDLE IF CHILD PROCESSES -------------------------------------
int fragSize = SEQUENCELEN + 1024;
int com_sockets[100]; // used to store clients' sockets descriptors
int sync; 
char* msgType = (char*) malloc(4); // on donnera pour l'instant les requetes sous la forme ABC_ ou ABC = {GET, ...}
while (1) {
	int newPort = port1 + 1;
  /*  printf("Accepting\n");
    select(sock_udp + 1, &set, NULL, NULL, 0);
    printf("SELECT DONE\n");*/
  	sync = synchro(sock_co_udp, client, newPort);
	if (sync > 1){ //son process who created a new socket, meaning sync = descriptor of newly created socket
		close(sock_co_udp);
		int msgSock = sync;
	  	com_sockets[newPort -1 - port1] = msgSock; // add to socket table
		int msgSize;
	  	msgSize = recvfrom(msgSock, buffer, RCVSIZE, MSG_WAITALL, (struct sockaddr*)&client, &clientLen);
	  	buffer[msgSize]='\0';
		printf("FMsg received , size received : %s, %i\n",buffer, msgSize);

		msgSize = recvfrom(msgSock, buffer, RCVSIZE, MSG_WAITALL, (struct sockaddr*)&client, &clientLen);
		buffer[msgSize]='\0';
		printf("BUFFER : %s\n",buffer);
		strncpy(msgType, buffer, 4);
		printf("MSGTYPE :%s\n", msgType);

		// case to handle client requests

		// GET FILE
		if (strcmp(msgType, "GET_") == 0){
			printf("ABOUT TO READ & SEND\n");
			readAndSendFile(msgSock, client, buffer + 4, fragSize - SEQUENCELEN, SEQUENCELEN, 10); //current ack must be shared between son processes
		}

		close(msgSock);
		break; //connexion closed

	}else if (sync == 1){ // parent process -> keep accepting connexions
		printf("ONE CLIENT SUCCESSFULLY SYNCHRO\n");
	  	newPort ++;

	}else{ // synchro failed
	  	printf("COULD NOT SYNC ONE CLIENT\n");
	}


    /*if (FD_ISSET( server_desc, &set) == 1){
	printf("TCP ACCEPTED\n");
    	int client_desc = accept(server_desc, (struct sockaddr*)&client, &alen);

    	printf("Value of accept is:%d\n", client_desc);
    
    	int fork_return = fork();
    	printf("FORK = %i\n",fork_return);
    
    	if (fork_return == 0){ //son process running
		close(server_desc);
		printf("ACCEPT SOCK = %i\n",server_desc);
	    	int msgSize = read(client_desc,buffer,RCVSIZE);
		printf("ACCEPT RETURN = %i\n",client_desc);
		while (msgSize > 0) {
		      write(client_desc,buffer,msgSize);
		      printf("%s",buffer);
		      memset(buffer,0,RCVSIZE);
		      msgSize = read(client_desc,buffer,RCVSIZE);
		}
		close(client_desc);
		exit(EXIT_SUCCESS);
    	}else{
		FD_SET(sock_udp, &set);
		close(client_desc);
	}
    }else{ //UDP received
	printf("UDP ACCEPTED\n");
        int recvd = recvfrom(sock_udp,(char *)buffer, RCVSIZE, MSG_WAITALL, (struct sockaddr*)&client, &alen);
	buffer[recvd] = '\0';
	printf("UDP : %s\n",buffer);
	FD_SET(server_desc, &set);
    }
  }
*/
}
	close(sock_co_udp);
	return 0;

}
