#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef RCVSIZE
        #define RCVSIZE 1024
#endif

#ifndef SEQUENCELEN
	#define SEQUENCELEN 4
#endif


int synchro(int sock, struct sockaddr_in server){
	char buffer[RCVSIZE];
	socklen_t serverLen = sizeof(server);
	int recvdSize;

	sendto(sock, "SYN", strlen("SYN"), MSG_CONFIRM, (struct sockaddr*)&server, serverLen);
	recvdSize = recvfrom(sock, buffer, RCVSIZE, MSG_WAITALL, (struct sockaddr*) &server, &serverLen);
       	buffer[recvdSize] = '\0';
	printf("RECEIVED : %s\n", buffer);
	char strbegin[12];
	printf("%s\n",buffer+7);
	int port = atoi(buffer+7);
	strncpy(strbegin, buffer, 7);
	strbegin[7]='\0';
	printf("STRBEGIN : %s\n", strbegin);
	if (strcmp(strbegin, "SYN-ACK") == 0){
		sendto(sock, "ACK", strlen("ACK"), MSG_CONFIRM, (struct sockaddr*)&server, serverLen);
		printf("SYNCHRO ACK SENT\n");
		return port;
	}
	printf("SYNCHRO : SYN-ACK NOT RECEIVED\n");
	return -1;
}

//fonction qui demande un fichier et qui acquitte les msg
char* askForFile(int sock, struct sockaddr_in server, char* filename){
        char buffer[RCVSIZE];
	socklen_t serverLen = sizeof(server);
	int recvdSize;
        char* res;
        printf("BEFORE INI STRING\n");
        char ackMsg[] = "ACK_";
        char endMsg[] = "END_";
        char getMsg[] = "GET_";
        printf("AFTER INIT STRING\n");
        int fileSize;

        //init res size with size announced by server 
        //getMsg + 4 = (char*) malloc(strlen(filename));
        strncat(getMsg, filename, strlen(filename));
        printf("AFTER SETTING GET MSG\n");
        printf("GET MESSAGE : %s\n", getMsg);
        sendto(sock, getMsg, strlen(getMsg), MSG_CONFIRM, (struct sockaddr*)&server, serverLen);
	recvfrom(sock, &fileSize, RCVSIZE, MSG_WAITALL, (struct sockaddr*) &server, &serverLen);
        fileSize = ntohl(fileSize);
        res = (char*) malloc(fileSize);
        printf("FILE SIZE RECEIVED, RES INITIALIZED \n");

        //start receiving file content
        //endMsg + 4 = (char*) malloc(strlen(filename));
        //ackMsg = "ACK_";
        //ackMsg + 4 = (char*) malloc(SEQUENCELEN);
        strncat(endMsg, filename, strlen(filename));
        printf("END MSG CONSTRUCTED: %s\n", endMsg);
        recvdSize = recvfrom(sock, buffer, 1028 , MSG_WAITALL, (struct sockaddr*) &server, &serverLen);
        //buffer[recvdSize] = '\0';
        //printf("RECEIVED FIRST CONTENT FRAG : %s\n", buffer);
        int nextFree = 0;
        while (strcmp(buffer, endMsg) != 0){
                // WARNING !!!!! check if server send same msg 
                memcpy(res + nextFree, buffer + SEQUENCELEN, recvdSize - SEQUENCELEN);
                printf("LAST CHAR OF BUFFER : %c\n",buffer[recvdSize]);
                //printf("added msg at %i position\n", nextFree);
                nextFree += (recvdSize - SEQUENCELEN);
                /*
                memcpy(res + nextFree, "ICI", 3);
                nextFree += 3;*/
                //strncat(res, buffer+SEQUENCELEN, recvdSize -  SEQUENCELEN);
                //res[i*(recvdSize - SEQUENCELEN)] = '\0'; //not exactly but check for strcat
                //printf("UPDATED RES\n");
                buffer[SEQUENCELEN] = '\0';
                //printf("SEQUENCE NUMBER : %s\n", buffer);
                ackMsg[4] = '\0';
                strncat(ackMsg, buffer, SEQUENCELEN);
                printf("Received %i bytes | ack = %s\n", recvdSize, ackMsg);
                //strncpy(ackMsg + 4, buffer, SEQUENCELEN);
                //printf("CONSTRUCTED ACK : %s\n", ackMsg);
                sendto(sock, ackMsg, strlen(ackMsg), MSG_CONFIRM, (struct sockaddr*)&server, serverLen);
                bzero(buffer, recvdSize);
                recvdSize = recvfrom(sock, buffer, 1028, MSG_WAITALL, (struct sockaddr*) &server, &serverLen);
                //buffer[recvdSize] = '\0';
                //printf("received MSG : %s\n", buffer);
        }
        printf("lastly received : %s\n", buffer);
        printf("STRCMP(%s,%s) = %i\n",buffer, endMsg, strcmp(buffer, endMsg));
        printf("ENDMSG = %s\n", endMsg);
        //printf("ALL FILE RECEIVED : res = %s\n", res);

        char fileRes[] = "client_";
        strncat(fileRes, filename, strlen(filename));
        FILE* fichRes = fopen(fileRes, "w");
        if (fichRes){
                fwrite(res, 1, fileSize, fichRes);
                //fputs(res, fichRes);
                fclose(fichRes);
        }
        return res;
}