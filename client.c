#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "utilClient.h"

#ifndef RCVSIZE
        #define RCVSIZE 1024
#endif

int main (int argc, char *argv[]) {

// --------------------- CONFIG ----------------------
  struct sockaddr_in adresse, adresse_com;
  struct in_addr serv_addr;
  serv_addr.s_addr = inet_addr("127.0.0.1");
  int port = 5001;
  int gotServAddr;
  if (argc > 2){
          port = atoi(argv[2]);
          printf("port = %i\n",port);
          char *ip_addr = argv[1];
          printf("adress = %s\n", ip_addr);
          gotServAddr = inet_pton(AF_INET, ip_addr, &serv_addr);

  }else{
          port = atoi(argv[1]);
          gotServAddr = -1;
  }
  printf("PORT = %i\n",port);
  int valid = 1;
  char msg[RCVSIZE];
  char blanmsg[RCVSIZE];
  int server_desc;
  //create socket
  if ((server_desc = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("cannot create udp socket\n");
        return -1;
  }

  setsockopt(server_desc, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));
  memset(&adresse, 0, sizeof(adresse));

  adresse.sin_family= AF_INET;
  adresse_com.sin_family = AF_INET;
  adresse.sin_port= htons(port);
  if (gotServAddr >= 1){
        adresse.sin_addr.s_addr= serv_addr.s_addr;
	adresse_com.sin_addr.s_addr = serv_addr.s_addr;
  }else{
          adresse.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	  adresse_com.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
          //adresse.sin_addr.s_addr = htonl(INADDR_LOOPBACK); modif
  }

  int newPort;
  int newSock;

  // ------------------------- CONNECT ------------------------------
  if ((newPort = synchro(server_desc, adresse)) > 0){
	  if ((newSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		  perror("Could not create new com socket\n");
		  return -1;
	  }
	  //char *fMsg = "First message !";
	  adresse_com.sin_port = htons(newPort);
	  //sendto(newSock, fMsg, strlen(fMsg), MSG_CONFIRM, (struct sockaddr*) &adresse_com, sizeof(adresse_com));
	  //printf("First message sent %s %d\n", inet_ntoa(adresse_com.sin_addr), ntohs(adresse_com.sin_port));
  }

  // ------------------------ ASF FOR FILE (BY INPUT) -------------------
  char fichier[RCVSIZE];
  fgets(fichier, RCVSIZE, stdin);
  fichier[strlen(fichier) - 1] = '\0'; //because of \n char
  //erreur de seg
  printf("ABOUT TO ASK FOR FILE\n");
  char *contenu = askForFile(newSock, adresse_com, fichier);


close(server_desc);
return 0;
}

