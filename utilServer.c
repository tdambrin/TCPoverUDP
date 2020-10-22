#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/time.h>
#include <math.h>
#include "utilServer.h"

#ifndef RCVSIZE
	#define RCVSIZE 1024
#endif

#ifndef SEQUENCELEN
	#define SEQUENCELEN 4
#endif

/*Problems so far :
 * - create the com socket after a SYN even if synchro fail at the end
 */
int synchro(int sock, struct sockaddr_in client, int port){
	char buffer[RCVSIZE];
	socklen_t clientLen = sizeof(client);
	int recvdSize = recvfrom(sock, (char*) buffer, RCVSIZE, MSG_WAITALL, (struct sockaddr*)&client, &clientLen);
        buffer[recvdSize] = '\0';
        printf("%s\n", buffer);
        if (strcmp(buffer, "SYN") == 0){ //SYN INITIATED BY CLIENT
		int fork_res = fork();
		if (fork_res == 0){ //son process

			// -> CREATION OF THE NEW COM SOCKET
			struct sockaddr_in adresse_com;
			int valid= 1;
			int newSock = socket(AF_INET, SOCK_DGRAM, 0);

			memset(&adresse_com, 0, sizeof(adresse_com));
			adresse_com.sin_family = AF_INET;
			adresse_com.sin_addr.s_addr = htonl(INADDR_ANY);
			adresse_com.sin_port = htons(port);

			if (newSock < 0){
				perror("CANT CREATE COM SOCKET \n");
				return -1;
			}
			setsockopt(newSock, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));
			if (bind(newSock, (struct sockaddr*) &adresse_com, sizeof(adresse_com)) == -1) {
				perror("Bind newsock failed\n");
				close(newSock);
				return -1;
	                }
			return newSock;
		}else{//parent process

			char portstr[SEQUENCELEN + 1];
			snprintf(portstr, sizeof(portstr),"%d",port);
			char synack_msg[8 + SEQUENCELEN] = "SYN-ACK";
			strcat(synack_msg, portstr);
	        sendto(sock, (char*) synack_msg, strlen(synack_msg), MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
			printf("SYNACK MESSAGE SENT : %s\n", synack_msg);
        	      	recvdSize = recvfrom(sock, (char*) buffer, RCVSIZE, MSG_WAITALL, (struct sockaddr*)&client, &clientLen);
              		buffer[recvdSize] = '\0';
			if (strcmp(buffer, "ACK") == 0){	     
			  	printf("SYNCHRONIZATION OK\n");
			    	return 1;;
		        }else{
			  	printf("SYNCHRO STARTED BUT NOT FINISHED\n");
			}
		}
	}else{
		printf("SYNCHRONIZATION  NOT GOOD\n");			
		return -1;
	}
}


void intToSeqN(int x, char *res){
        //char *res = (char*) malloc (SEQUENCELEN); // attention normalement malloc terminé à la fin de la fonction
        strncpy(res, "00000000000000", SEQUENCELEN);
        res[SEQUENCELEN] = '\0'; 
        for (int i=SEQUENCELEN - 1; i >= 0; i--){
                if (x >= pow(10, i)){
                    snprintf(res + SEQUENCELEN - i - 1, sizeof(res) - SEQUENCELEN + i + 1,"%d",x);
                    break;
                }
        } 
        //return res;
}

int seqNToInt(char *seqNumber){
    return atoi(seqNumber);
}

//current message format : XXXX<data> with XXXX sequence number
int readAndSendFile(int sock, struct sockaddr_in client, char* filename, int dataSize, int seqNsize, int initAck){
    char* content;
    int window = 1;
    char* msg = (char*) malloc(dataSize + seqNsize);
    char* response = (char*) malloc(seqNsize+4);
    char strAck[] = "ACK_";
    printf("FILENAME : begin:%s:end\n", filename);
    FILE* fich = fopen(filename, "r");
    socklen_t clientLen = sizeof(client);
    int recvdSize;
    if (fich == NULL){
        perror("Cant open file\n");
        return -1;
    }
    printf("FILE OPENED\n");
    fseek(fich, 0, SEEK_END);
    long filelen = ftell(fich);
    fseek(fich, 0, SEEK_SET);
    content = (char*) malloc(filelen);
    if (content == NULL){
        perror("Memoire epuisee\n");
        return -1;
    }
    int sizeToSend = htonl(filelen);
    sendto(sock, &sizeToSend, sizeof(htonl(filelen)), MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
    printf("FILE SIZE SENT TO CLIENT\n");
    /*
    for (int i=0; i < filelen/dataSize; i++){
        fgets(content[i], dataSize, fich);
        printf("Content %i read\n",i);
    }*/
    fread(content, 1, filelen, fich);
    int sent = -1;
    //printf("CONTENT ALL READ AND STORED : %s\n", content);

//------------------------------------
    //SLIDING WINDOW
    int transmitted = 0;
    int lastSent = initAck - 1;
    int lastTransmittedSeqN = initAck - 1;
    int maybeAcked;
    int flightSize = 0;
    int dupAck = 0;
    char* currentSeqN = (char *) malloc (SEQUENCELEN);

    char* proof = (char*) malloc(filelen);
    int nextFree = 0;

    for (int i = 0; i < window; i++){
        strAck[4] = '\0';
        intToSeqN(initAck + i, currentSeqN);
        strncat(strAck, currentSeqN, seqNsize);
        msg[0] = '\0';
        strncat(msg, currentSeqN, seqNsize);
        memcpy(msg + seqNsize, content + i*dataSize, dataSize);

        //WARNING FOR LATER : handle last message that can be shorter
        sent = sendto(sock, (char*) msg, dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);

                memcpy(proof+nextFree, msg + SEQUENCELEN, sent - SEQUENCELEN);
                nextFree += sent - SEQUENCELEN;


        lastSent += 1;
        flightSize ++;
        printf("SENT %i bytes | seqN = %i \n", sent, initAck+i);
    }
    while (transmitted < filelen){
            recvdSize = recvfrom(sock, (char*) response, RCVSIZE, MSG_WAITALL, (struct sockaddr*)&client, &clientLen);
            response[recvdSize] = '\0';
            maybeAcked = seqNToInt(response + 4);
            response[4] = '\0';
            if (strcmp(response, "ACK_") == 0){
                flightSize --;
                if (maybeAcked == lastTransmittedSeqN + 1){
                    lastTransmittedSeqN++;
                    transmitted += dataSize;
                    int i=1;
                    while (flightSize < window){
                        /*strAck[4] = '\0';
                        strncat(strAck, intToSeqN(initAck + lastSent + 1), seqNsize);*/
                        msg[0] = '\0';
                        intToSeqN(lastSent + 1, currentSeqN);
                        strncat(msg, currentSeqN, seqNsize);
                        memcpy(msg + seqNsize, content + (lastSent - initAck + 1)*dataSize, dataSize);
                        //WARNING FOR LATER : handle last message that can be shorter
                        sent = sendto(sock, (char*) msg, dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                        if (sent > -1){
                            flightSize ++;
                            lastSent += 1;
                            i++;
                            printf("SENT %i bytes | seqN = %i \n", sent, lastSent - 1);

                                    memcpy(proof+nextFree, msg + SEQUENCELEN, sent - SEQUENCELEN);
                                    nextFree += sent - SEQUENCELEN;
                        }
                    }
                }else{
                    printf("Received a duplicated ACK\n");
                    dupAck ++;
                    if (dupAck >= 3){
                        int i = 1;
                        while (flightSize < window){
                            /*strAck[4] = '\0';
                            strncat(strAck, intToSeqN(lastTransmittedSeqN + i), seqNsize);*/
                            msg[0] = '\0';
                            intToSeqN(lastTransmittedSeqN + i, currentSeqN);
                            strncat(msg, currentSeqN, seqNsize);
                            memcpy(msg + seqNsize, content + (lastTransmittedSeqN - initAck + i)*dataSize, dataSize);
                            //WARNING FOR LATER : handle last message that can be shorter
                            sent = sendto(sock, (char*) msg, dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            if (sent > -1){
                                flightSize ++;
                                i++;
                                int diff = lastTransmittedSeqN + i - lastSent;
                                if (diff > 0){
                                    lastSent += diff;
                                }
                                printf("SENT %i bytes | seqN = %i \n", sent, lastTransmittedSeqN + i - 1);
                            }
                        }
                    }else{
                        //keep going
                        while (flightSize < window){
                            msg[0] = '\0';
                            intToSeqN(lastSent + 1, currentSeqN);
                            strncat(msg, currentSeqN, seqNsize);
                            memcpy(msg + seqNsize, content + (lastSent - initAck + 1)*dataSize, dataSize);
                            //WARNING FOR LATER : handle last message that can be shorter
                            sent = sendto(sock, (char*) msg, dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            if (sent > -1){
                                flightSize ++;
                                lastSent += 1;
                                printf("SENT %i bytes | seqN = %i \n", sent, lastSent);
                            }
                        }
                    }
                }
            }
    }
//------------------------------------
    /*for (int i=0; i <= filelen/dataSize; i++){
        strAck[4] = '\0';
        strncat(strAck, intToSeqN(initAck+i), seqNsize);
        msg[0] = '\0';
        strncat(msg, intToSeqN(initAck + i), seqNsize);
        memcpy(msg + seqNsize, content + i*dataSize, dataSize);
        //WARNING FOR LATER : handle last message that can be shorter
        sent = sendto(sock, (char*) msg, dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
        printf("SENT %i bytes | seqN = %i \n", sent, initAck+i);
        //WARNING : add timeout (select)
        recvdSize = recvfrom(sock, (char*) response, RCVSIZE, MSG_WAITALL, (struct sockaddr*)&client, &clientLen);
        response[recvdSize] = '\0';
        //printf("RECEIVED ACK : %s\n", response);
        //printf("STRACK TO HAVE : %s\n", strAck);
        while (strcmp(response, strAck) != 0){
            printf("PACKET DIDNT ACKED\n");
            sendto(sock, (char*) msg, sizeof(msg), MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
            recvdSize = recvfrom(sock, (char*) response, RCVSIZE, MSG_WAITALL, (struct sockaddr*)&client, &clientLen);
            response[recvdSize] = '\0';
        }
    }*/

   char endMsg[] = "END_";
    //endMsg + 4 = (char*) malloc(strlen(filename));
    strncat(endMsg, filename, strlen(filename));
    printf("ENDMSG AT FIRST : %s\n", endMsg);
    printf("BEFORE SENDING: %s\n", endMsg);
    sendto(sock, (char*) endMsg, strlen(endMsg), MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
    printf("SENT END MSG : %s\n", endMsg);
    printf("FILE CONTENT FULLY TRANSMITTED\n");
    printf("CONTENT == PROOF : %i\n", memcmp(content, proof, filelen));
    printf("END OF PROOF : %s\n", proof + (filelen - 20));
    
    fclose(fich);
    free(content);
    return 1;
}

char **splitData(char* src, int fragSize){
    char** res = (char**) malloc(sizeof(src));
    for( int i=0; i < strlen(src)/fragSize + 1; i++){
        strncpy(res[i], src + fragSize*i, fragSize);
    }
    return res;
}
