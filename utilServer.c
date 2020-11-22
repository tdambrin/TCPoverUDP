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
#include <time.h>

#include "utilServer.h"

#ifndef RCVSIZE
	#define RCVSIZE 1024
#endif

#ifndef PORTLEN
    #define PORTLEN 4
#endif

#ifndef SEQUENCELEN //value used to estimate the rtt : +/- importance given to the RTT measured or to the RTT history
	#define SEQUENCELEN 6
#endif

#ifndef ALPHA
	#define ALPHA 0.6 //WARNING /!\ discuss about the value
#endif

void insertionListeTriee(LISTE *pliste, int val)	
{
       if((*pliste)==NULL){ 
                (*pliste) = (LISTE) malloc(sizeof(LISTE));
                if (pliste == NULL) {
                        fprintf(stderr, "insertionListeTriee: plus de place mémoire");
                        exit(EXIT_FAILURE);
                }
                (*pliste)->seqN = val;
                (*pliste)->suivant = NULL;
        }else if ((*pliste)->seqN >= val){
                LISTE toSave = (LISTE) malloc(sizeof(LISTE));
                *toSave = **pliste;
                (*pliste)->seqN = val;
                (*pliste)->suivant = toSave;
                //free(toSave);
        }else{
                insertionListeTriee(&((*pliste)->suivant), val);
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


void printListe(LISTE l){
    if (l == NULL){
        printf("\n");
    }else{
        printf("%i | ", l->seqN);
        printListe(l->suivant);
    }
}


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

			char portstr[PORTLEN + 1];
			snprintf(portstr, sizeof(portstr),"%d",port);
			char synack_msg[8 + PORTLEN] = "SYN-ACK";
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
                  return -1;
			}
		}
	}else{
		printf("SYNCHRONIZATION  NOT GOOD\n");			
		return -1;
	}
}


void intToSeqN(int x, char *res){
        strncpy(res, "00000000000000", SEQUENCELEN);
        res[SEQUENCELEN] = '\0'; 
        for (int i=SEQUENCELEN - 1; i >= 0; i--){
                if (x >= pow(10, i)){
                    snprintf(res + SEQUENCELEN - i - 1, sizeof(res) - SEQUENCELEN + i + 1,"%d",x);
                    break;
                }
        } 
}

int seqNToInt(char *seqNumber){
    return atoi(seqNumber);
}

//current message format : XXXX<data> with XXXX sequence number
int readAndSendFile(int sock, struct sockaddr_in client, char* filename, int dataSize, int seqNsize, int initAck){
                        //WARNING FOR LATER : handle sending of more content than existing


    // ---------------------- INIT ---------------------
    char* content;
    float window = 1; //with slow start the window starts to 1...
    float sstresh = 100; //...and the tresholt takes a first arbitrary great value at the beginning
    char* msg = (char*) malloc(dataSize + seqNsize);
    char* response = (char*) malloc(seqNsize+3);
    char strAck[] = "ACK";
    fd_set set;
    struct timeval timeout; //time after which we consider a segment lost
    /*
    FD_ZERO(&set);
    FD_SET(sock, &set);
    select(server_desc_udp+1,&descripteurs,NULL,NULL,NULL);
    */
    
    //printf("FILENAME : begin:%s:end\n", filename);

    // --------------------- READ FILE ------------------
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
    fread(content, 1, filelen, fich);
    fclose(fich);

    // -------------------- SEND FILE SIZE TO CLIENT FOR CLIENT INIT ---------------------- >> not needed for project clients
    
    /*int sizeToSend = htonl(filelen);
    sendto(sock, &sizeToSend, sizeof(htonl(filelen)), MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
    printf("FILE SIZE SENT TO CLIENT\n");*/

    //printf("CONTENT ALL READ AND STORED : %s\n", content);

    //--------------------------- SEND FILE CONTENT TO CLIENT -----------------
    int sent = -1;
    int transmitted = 0; //nb of bytes sent and acked
    int lastSent = initAck - 1; //seqN of last sent segment
    int lastTransmittedSeqN = initAck - 1; //seqN of last acked segment
    int maybeAcked; //used to store seqN answered by client
    int flightSize = 0;
    int dupAck = 0;
    char* currentSeqN = (char *) malloc (SEQUENCELEN);
    float srtt = 4; //arbitrary value, this estimator should converge to the real value of rtt
    timeout.tv_sec = srtt;
    timeout.tv_usec = 0;

    clock_t start = clock();
    clock_t begin,stop;
    //WARNING : if window greater than total nb of segments 

    //Send window first segments
    for (int i = 0; i < window; i++){
        strAck[3] = '\0';
        intToSeqN(initAck + i, currentSeqN);
        strncat(strAck, currentSeqN, seqNsize);
        msg[0] = '\0';
        strncat(msg, currentSeqN, seqNsize);
        memcpy(msg + seqNsize, content + i*dataSize, dataSize);

        //WARNING FOR LATER : handle last message that can be shorter
        sent = sendto(sock, (char*) msg, dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
        begin = clock();
        lastSent += 1;
        printf("SEG_%i SENT \n",lastSent);

        flightSize ++;
        //printf("SENT %i bytes | seqN = %i \n", sent, initAck+i);
    }

    LISTE nextAcked = NULL;
    int lastSeqN = filelen/dataSize + 1 + initAck;
    int lastMsgSize = filelen - (lastSeqN - initAck)*dataSize;
    printf("lastSeqN = %i\n", lastSeqN);
    while (lastTransmittedSeqN < lastSeqN){
    //while (transmitted < filelen){
        FD_ZERO(&set);
        FD_SET(sock, &set);
        select(sock+1,&set,NULL,NULL,&timeout);

        if( FD_ISSET(sock,&set) ){
            
            //printf("\n #WINDOW : %f\n",floor(window) );
            //receive answer from client and update var
            recvdSize = recvfrom(sock, (char*) response, RCVSIZE, MSG_WAITALL, (struct sockaddr*)&client, &clientLen);

                //Estimation du RTT sur lequel vont se baser les futures estimations du RTT
	        stop = clock();
	        float rtt = (float)(stop - begin) / CLOCKS_PER_SEC;
	        srtt = ALPHA*srtt + (1-ALPHA)*rtt;
            timeout.tv_sec = srtt;
	        //printf("[RTT : %f | SRTT : %f]\n",rtt,srtt);

            response[recvdSize] = '\0';
            maybeAcked = seqNToInt(response + 3);
            response[3] = '\0';

            printf("ACK_%i RCV \n",maybeAcked);

            if (strcmp(response, "ACK") == 0){
                flightSize --; //because a segment has been acked
                    //slowstart
                if(window < sstresh){
                    window += 1;
                    //congestion avoidance
                }else{
                    window += (1/window);
                }

                if (maybeAcked == lastTransmittedSeqN + 1){ //currently acked segment is the very next one of the last acked segment
                    lastTransmittedSeqN++;

                    if (maybeAcked == lastSeqN){
                        transmitted = filelen;
                    }else{
                        transmitted += dataSize;
                    }
                    //transmit next segments (from lastSent not lastTransmitted)
                    while (flightSize < floor(window) && lastSent < lastSeqN ){

                        printf("transmitted : %d, filelen : %ld\n",transmitted,filelen);
                        msg[0] = '\0';
                        intToSeqN(lastSent + 1, currentSeqN);
                        strncat(msg, currentSeqN, seqNsize);

                            //if the message is shorter than dataSize
                        if(filelen - (lastSent - initAck)*dataSize < dataSize){
                            memcpy(msg + seqNsize, content + (lastSent - initAck + 1)*dataSize, filelen - (lastSent - initAck)*dataSize); //WARNING : if dataSize=cste
                            sent = sendto(sock, (char*) msg,  filelen - (lastSent - initAck)*dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                       }else{
                            memcpy(msg + seqNsize, content + (lastSent - initAck + 1)*dataSize, dataSize); //WARNING : if dataSize=cste
                            sent = sendto(sock, (char*) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                        }

                        begin = clock();
                        if (sent > -1){
                            flightSize ++;
                            lastSent += 1;
                            //printf("SENT %i bytes | seqN = %i \n", sent, lastSent - 1);
                        }
                        printf("SEG_%i SENT \n",lastSent); // PROBLEM HERE ********************************
                        /*if (lastSent > 161){
                            printf("\n\n *******DEBUGG END OF TRANSMISSION********\n");
                            printf("Lastly Acked = %i | lastTransmittedseqN = %i\n", maybeAcked, lastTransmittedSeqN);
                            printf("Transmitted = %i | filelen = %li\n", transmitted, filelen);
                            printf("MSG = %s\n", msg);
                            recvdSize = recvfrom(sock, (char*) response, RCVSIZE, MSG_WAITALL, (struct sockaddr*)&client, &clientLen);
                            response[recvdSize] = '\0';
                            printf("But new msg = %s\n", response);
                        }*/

                        printf("flightsize : %d, floor(window) : %f\n",flightSize,floor(window));
                    }
                }else{
             //   }else if (maybeAcked <= lastTransmittedSeqN) { //something went wrong with the transmission : the currently acked is not consecutive	
                    if (maybeAcked <= lastTransmittedSeqN){
                        printf("Received a duplicated ACK, lastTrans = %i, received = %i\n", lastTransmittedSeqN, maybeAcked);
                        dupAck ++; //WARNING : not necessarly a dup ACK ? (if ack receiving order differs from ack sending order)
                    }else{
                        printf("Received an ACK to store, lastTrans = %i, received = %i\n", lastTransmittedSeqN, maybeAcked);
                        insertionListeTriee(&nextAcked, maybeAcked);
                        printf("LISTE : "); printListe(nextAcked);
                    }
                    if (dupAck >= 3){ //consider a lost segment
                        printf("At least 3 dupAcks\n");

                        int i = 1; // used because we send from lastTransmitted but cant update lastTransmitted after just sending (got to receive the ack too)
                        window = 1;
                        sstresh = flightSize/2;
		                msg[0] = '\0';
                        intToSeqN(lastTransmittedSeqN + 1, currentSeqN);
                        strncat(msg, currentSeqN, seqNsize);

			            if (lastTransmittedSeqN < lastSeqN - 1){
				            memcpy(msg + seqNsize, content + (lastTransmittedSeqN - initAck + 1)*dataSize, dataSize);
				            while (sendto(sock, (char*) msg, dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen) < 0){
					        printf("Could not send again for 3dup, about to try again\n");
				            }
			            }else{
				            memcpy(msg + seqNsize, content + (lastTransmittedSeqN - initAck + 1)*dataSize, lastMsgSize);
                            while (sendto(sock, (char*) msg, lastMsgSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen) < 0){
                                    printf("Could not send again for 3dup, about to try again\n");
                            }
			            }

       /*                 while (flightSize < floor(window) && transmitted < filelen){
                            msg[0] = '\0';
                            intToSeqN(lastTransmittedSeqN + i, currentSeqN);
                            strncat(msg, currentSeqN, seqNsize);

                                //if the message is shorter than dataSize
                            if(lastSent < lastSeqN - 1){
                                memcpy(msg + seqNsize, content + (lastSent - initAck + 1)*dataSize, filelen - (lastSent - initAck)*dataSize); //NING : if dataSize=cste
                                sent = sendto(sock, (char*) msg,  filelen - (lastSent - initAck)*dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                                printf("SEG_%i SENT \n",lastSent);
                            }else{
                                memcpy(msg + seqNsize, content + (lastSent - initAck + 1)*dataSize, dataSize); //WARNING : if dataSize=cste
                                sent = sendto(sock, (char*) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                                printf("SEG_%i SENT \n",lastSent);
                            }

                            //WARNING FOR LATER : handle last message that can be shorter
                            begin = clock();

                            if (sent > -1){
                                flightSize ++;
                                i++;
                                int diff = lastTransmittedSeqN + i - lastSent;
                                if (diff > 0){
                                    lastSent += diff;
                                }
                                //printf("SENT %i bytes | seqN = %i \n", sent, lastTransmittedSeqN + i - 1);
                            }
                        }*/
                        dupAck = 0;
                    }else{ // not yet considered as a lost segment -> keep sending
                        while (flightSize < floor(window) && lastSent < lastSeqN){
                            msg[0] = '\0';
                            intToSeqN(lastSent + 1, currentSeqN);
                            strncat(msg, currentSeqN, seqNsize);
                            //if the message is shorter than RCVSIZE
                            if(lastSent > lastSeqN - 1){
                                printf("about to send from dupack, lastSent = %i, lastSeqN = %i\n", lastSent, lastSeqN);
                                memcpy(msg + seqNsize, content + (lastSent - initAck + 1)*dataSize, filelen - (lastSent - initAck)*dataSize); //WARNING : if dataSize=cste
                                printf("copied msg\n");
                                sent = sendto(sock, (char*) msg,  filelen - (lastSent - initAck)*dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                                printf("sent msg\n");
                            }else{
                                memcpy(msg + seqNsize, content + (lastSent - initAck + 1)*dataSize, dataSize); //WARNING : if dataSize=cste
                                sent = sendto(sock, (char*) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            }

                            begin = clock();
                            if (sent > -1){
                                flightSize ++;
                                lastSent += 1;
                                //printf("SENT %i bytes | seqN = %i \n", sent, lastSent);
                            }
                        }
                    }
                }
                 int storedAck = -1;
                    if (nextAcked != NULL){
                        storedAck = nextAcked->seqN;
                    }
                    while (storedAck == lastTransmittedSeqN + 1){
                        lastTransmittedSeqN ++;
                        suppHead(&nextAcked);
                        if (nextAcked != NULL){
                            storedAck = nextAcked->seqN;
                        }
                    }
            }
        //TIMEOUT : segment lost
        }else{
            printf("HERE\n");
            sstresh = flightSize/2;

            //--------------------------------------A MODIFIER : ENVOYER lastTrans + 1 et pas lastSent + 1-----------------------------------
            if(lastSent > lastSeqN - 1){
                                printf("about to send from timeout, lastSent = %i\n", lastSent);
                                memcpy(msg + seqNsize, content + (lastSent - initAck + 1)*dataSize, lastMsgSize); //WARNING : if dataSize=cste
                                printf("copied msg\n");
                                sent = sendto(sock, (char*) msg,  filelen - (lastSent - initAck)*dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                                printf("sent msg\n");
            }else{
                                memcpy(msg + seqNsize, content + (lastSent - initAck + 1)*dataSize, dataSize); //WARNING : if dataSize=cste
                                sent = sendto(sock, (char*) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
            }
            sent = sendto(sock, (char*) msg,  filelen - (lastSent - initAck)*dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
            window = 1;
            timeout.tv_sec = 2; 
            timeout.tv_usec = 0;
        }
    }

    //All bytes have been transmitted -> send END MSG

    /*char endMsg[] = "END_";
    strncat(endMsg, filename, strlen(filename));*/
    char endMsg[] = "FIN";
    sendto(sock, (char*) endMsg, strlen(endMsg), MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
    printf("FILE CONTENT FULLY TRANSMITTED, sent endMsg = %s\n", endMsg);

    //free(content);

    clock_t end = clock();
    float seconds = (float)(end - start) / CLOCKS_PER_SEC;
    printf("program ran in %fs with window = %f\n", seconds, window);

    //save time for comparison-----------------------------
    FILE* times = fopen("times.txt", "a");
    if (times){
        fprintf(times, "%f\n", seconds);
        fclose(times);
    }

    return 1;
}

//not used yet
char **splitData(char* src, int fragSize){
    char** res = (char**) malloc(sizeof(src));
    for( int i=0; i < strlen(src)/fragSize + 1; i++){
        strncpy(res[i], src + fragSize*i, fragSize);
    }
    return res;
}
