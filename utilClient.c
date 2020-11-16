#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>


#ifndef RCVSIZE
        #define RCVSIZE 1024
#endif

#ifndef SEQUENCELEN
	#define SEQUENCELEN 4
#endif

struct model_elem{
        int seqN;
        char *data;
        int dataLen;
        struct model_elem *suivant;
};

typedef struct model_elem ELEMLIST;

typedef ELEMLIST *LISTE;

ELEMLIST *new_list(int val){
        ELEMLIST *res;
        res = (ELEMLIST *) malloc(sizeof(ELEMLIST));
        res->seqN=val;
        res->suivant=NULL;
        return res;
}

void insertionListeTriee(LISTE *pliste, int val, char *data, int dataLen)	
{
       if((*pliste)==NULL){ 
                (*pliste) = (LISTE) malloc(sizeof(LISTE));
                if (pliste == NULL) {
                        fprintf(stderr, "insertionListeTriee: plus de place mémoire");
                        exit(EXIT_FAILURE);
                }
                (*pliste)->seqN = val;
                (*pliste)->data = (char *) malloc(dataLen);
                if ((*pliste)->data == NULL){
                        fprintf(stderr, "insertionListeTriee: plus de place memoire\n");
                        exit(EXIT_FAILURE);
                }
                memcpy(&((*pliste)->data), data, dataLen); 
                (*pliste)->dataLen = dataLen;
                (*pliste)->suivant = NULL;
        }else if ((*pliste)->seqN >= val){
                LISTE toSave = (LISTE) malloc(sizeof(LISTE));
                *toSave = **pliste;
                (*pliste)->seqN = val;
                memcpy(&((*pliste)->data), data, dataLen); 
                (*pliste)->dataLen = dataLen;
                (*pliste)->suivant = toSave;
                free(toSave);
        }else{
                insertionListeTriee(&((*pliste)->suivant), val, data, dataLen);
        }
  return;
}

int suppHead(LISTE *pliste){
        if ( *pliste==NULL){
                return 0;
        }
        LISTE tmp = *pliste;
        *pliste = (*pliste)->suivant;
        free(tmp);
        return 1;
}

void intToSeqN(int x, char* res){
//        char *res = (char*) malloc (SEQUENCELEN);
        strncpy(res, "00000000000000", SEQUENCELEN);
        res[SEQUENCELEN] = '\0'; 
        for (int i=SEQUENCELEN - 1; i >= 0; i--){
                if (x >= pow(10, i)){
                    snprintf(res + SEQUENCELEN - i - 1, sizeof(res) - SEQUENCELEN + i + 1,"%d",x);
                    break;
                }
        } 
//        return res;
}

int seqNToInt(char *seqNumber){
    return atoi(seqNumber);
}

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
        /* WARNINGS :
          - if ACK sent, we consider that it is acked but sender may not have received it
          - Do not write again the data of an already written seqN ()
        */
        char buffer[RCVSIZE];
	socklen_t serverLen = sizeof(server);
	int recvdSize;
        char* res;
        printf("BEFORE INI STRING\n");
        char ackMsg[] = "ACK_";
        char stopMsg[] = "END_";
        printf("AFTER INIT STRING\n");
        int fileSize;
        float rtt;
        LISTE storedMsg;

        // ----------------------- SEND GET REQUEST TO SERVER --------------------
        char getMsg[] = "GET_";
        strncat(getMsg, filename, strlen(filename));
        printf("AFTER SETTING GET MSG\n");
        printf("GET MESSAGE : %s\n", getMsg);
        sendto(sock, getMsg, strlen(getMsg), MSG_CONFIRM, (struct sockaddr*)&server, serverLen);

        // --------------------- RECEIVE FILE SIZE AND INIT RES ------------------
	recvfrom(sock, &fileSize, RCVSIZE, MSG_WAITALL, (struct sockaddr*) &server, &serverLen);
        fileSize = ntohl(fileSize);
        res = (char*) malloc(fileSize);
        printf("FILE SIZE RECEIVED, RES INITIALIZED \n");

        //start receiving file content
        //endMsg + 4 = (char*) malloc(strlen(filename));
        //ackMsg = "ACK_";
        //ackMsg + 4 = (char*) malloc(SEQUENCELEN);
        strncat(stopMsg, filename, strlen(filename));
        printf("END MSG CONSTRUCTED: %s\n", stopMsg);
        recvdSize = recvfrom(sock, buffer, 1028 , MSG_WAITALL, (struct sockaddr*) &server, &serverLen);
        //buffer[recvdSize] = '\0';
        //printf("RECEIVED FIRST CONTENT FRAG : %s\n", buffer);

        // ------------------------- RECEIVE FILE CONTENT -------------------------
        int nextFree = 0;
        int lastAcked = 0;
        int seqNReceived;
        int i = 0;
        int sent;
        int receivedBytes = 0;
        char* currentSeqN = (char*) malloc(SEQUENCELEN);
        char* receivedSeqNStr = (char*) malloc(SEQUENCELEN);

        int lostSim = 0;
        int indexLost = 20;
        //while (strcmp(buffer, stopMsg) != 0){
        while (receivedBytes < fileSize){
                /*
                memcpy(res + nextFree, "ICI", 3);
                nextFree += 3;*/
                //strncat(res, buffer+SEQUENCELEN, recvdSize -  SEQUENCELEN);
                //res[i*(recvdSize - SEQUENCELEN)] = '\0'; //not exactly but check for strcat
                //printf("UPDATED RES\n");
                //printf("Received %i bytes\n", recvdSize);
                //printf("FIRST CHAR : %c\n",buffer[4]);

                /*if (recvdSize < 100){
                        printf("END MSG RECEIVED : %s\nEND MSG LOCAL : %s\n", buffer, stopMsg);
                }*/

                //buffer[SEQUENCELEN] = '\0';
                receivedSeqNStr[0] = '\0';
                strncat(receivedSeqNStr, buffer, SEQUENCELEN);
                seqNReceived = seqNToInt(receivedSeqNStr);
                ackMsg[SEQUENCELEN] = '\0';

                // First passage to set control vars because only server has the init seqN 
                if (i == 0){
                        strncat(ackMsg, buffer, SEQUENCELEN); // add seqN received to ack msg to be sent
                        sent = sendto(sock, ackMsg, strlen(ackMsg), MSG_CONFIRM, (struct sockaddr*)&server, serverLen);
                        while (sent < 0){
                                sent = sendto(sock, ackMsg, strlen(ackMsg), MSG_CONFIRM, (struct sockaddr*)&server, serverLen);
                        }
                        printf("First | ANSWER : %s\n", ackMsg);
                        lastAcked = seqNReceived;
                        // WARNING !!!!! check if server send same msg 
                        memcpy(res + nextFree, buffer + SEQUENCELEN, recvdSize - SEQUENCELEN); // write received content to res
                        // update control vars
                        receivedBytes += recvdSize - SEQUENCELEN;
                        printf("Copied %i bytes | ", recvdSize - SEQUENCELEN);
                        printf(" at %i position\n", nextFree);
                        nextFree += (recvdSize - SEQUENCELEN);

                // Received a consecutive segment
                }else if (seqNReceived == lastAcked + 1){
                        strncat(ackMsg, buffer, SEQUENCELEN);
                        sent = sendto(sock, ackMsg, strlen(ackMsg), MSG_CONFIRM, (struct sockaddr*)&server, serverLen);
                        while (sent < 0){
                                sent = sendto(sock, ackMsg, strlen(ackMsg), MSG_CONFIRM, (struct sockaddr*)&server, serverLen);
                        }
                        printf("Consecutive | ANSWER : %s\n", ackMsg);
                        lastAcked ++;
                        // WARNING !!!!!!! check if server send same msg 
                        memcpy(res + nextFree, buffer + SEQUENCELEN, recvdSize - SEQUENCELEN);
                        //printf("added msg at %i position\n", nextFree);
                        //memcpy(res + nextFree, "HERE", 4);
                        //nextFree += 4;
                        printf("Copied %i bytes |", recvdSize - SEQUENCELEN);
                        printf(" at %i position\n", nextFree);
                        nextFree += (recvdSize - SEQUENCELEN);
                        receivedBytes += recvdSize - SEQUENCELEN;

                        if (storedMsg != NULL){
                                while (storedMsg->seqN == lastAcked + 1){
                                        ackMsg[SEQUENCELEN] = '\0';
                                        intToSeqN(lastAcked + 1, currentSeqN);
                                        strncat(ackMsg, currentSeqN, SEQUENCELEN);
                                        sent = sendto(sock, ackMsg, strlen(ackMsg), MSG_CONFIRM, (struct sockaddr*)&server, serverLen);
                                        while (sent < 0){
                                                sent = sendto(sock, ackMsg, strlen(ackMsg), MSG_CONFIRM, (struct sockaddr*)&server, serverLen);
                                        }
                                        lastAcked ++;
                                                // WARNING !!!!! check if server send same msg 
                                        memcpy(res + nextFree, storedMsg->data,  storedMsg->dataLen);
                                        //printf("added msg at %i position\n", nextFree);
                                        //memcpy(res + nextFree, "HERE", 4);
                                        //nextFree += 4;
                                        nextFree += storedMsg -> dataLen;
                                        receivedBytes += storedMsg->dataLen;
                                        suppHead(&storedMsg);
                                }
                        }
                // Received a non consecutive segment -> ack again the last acked
                }else{
                        insertionListeTriee(&storedMsg, seqNReceived, buffer, recvdSize); //store msg for later (fast restransmit)
                        intToSeqN(lastAcked, currentSeqN);
                        strncat(ackMsg, currentSeqN, SEQUENCELEN);
                        sent = sendto(sock, ackMsg, strlen(ackMsg), MSG_CONFIRM, (struct sockaddr*)&server, serverLen);
                        while (sent < 0){
                                sent = sendto(sock, ackMsg, strlen(ackMsg), MSG_CONFIRM, (struct sockaddr*)&server, serverLen);
                        }
                        printf("Non-consecutive | ANSWER : %s\n", ackMsg);
                }
                /*
                //printf("SEQUENCE NUMBER : %s\n", buffer);
                strncat(ackMsg, buffer, SEQUENCELEN);
                printf("Received %i bytes | ack = %s\n", recvdSize, ackMsg);
                //strncpy(ackMsg + 4, buffer, SEQUENCELEN);
                //printf("CONSTRUCTED ACK : %s\n", ackMsg);
                sendto(sock, ackMsg, strlen(ackMsg), MSG_CONFIRM, (struct sockaddr*)&server, serverLen);*/
                bzero(buffer, recvdSize);
                recvdSize = recvfrom(sock, buffer, 1030, MSG_WAITALL, (struct sockaddr*) &server, &serverLen); // receive next msg from server
                //buffer[recvdSize] = '\0';
                //printf("received MSG : %s\n", buffer);
                i = 1;
        }
        printf("lastly received : %s\n", buffer);
        printf("STRCMP(%s,%s) = %i\n",buffer, stopMsg, strcmp(buffer, stopMsg));
        printf("ENDMSG = %s\n", stopMsg);
        //printf("ALL FILE RECEIVED : res = %s\n", res);

        //char fileRes[] = "client_";
        //strncat(fileRes, filename, strlen(filename));

        // ------------------------- SAVE RESULT ----------------------
        char fileRes[] = "result.pdf";
        FILE* fichRes = fopen(fileRes, "w");
        if (fichRes){
                fwrite(res, 1, fileSize, fichRes);
                //fputs(res, fichRes);
                fclose(fichRes);
        }else{
                printf("Cant open file to save res, filename : %s\n", filename);
        }
        return res;
}