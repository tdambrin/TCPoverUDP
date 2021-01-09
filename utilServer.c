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
	#define RCVSIZE 1000
#endif

#ifndef PORTLEN
    #define PORTLEN 4
#endif

#ifndef SEQUENCELEN //value used to estimate the rtt_sec : +/- importance given to the rtt_sec measured or to the rtt_sec history
	#define SEQUENCELEN 6
#endif

#ifndef ALPHA
	#define ALPHA 0.5 //WARNING /!\ discuss about the value
#endif


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

    FILE* log = fopen("log.txt", "w");
    FILE* perf = fopen("perf.txt", "a");

    // ---------------------- INIT ---------------------
    char* content;
    //float window = 1; //with slow start the window starts to 1...
    float sstresh = 100; //...and the tresholt takes a first arbitrary great value at the beginning
    char* msg = (char*) malloc(dataSize + seqNsize);
    char* response = (char*) malloc(seqNsize+3);
    char strAck[] = "ACK";
    fd_set set;
    struct timeval timeout; //time after which we consider a segment lost


    // --------------------- READ FILE ------------------
    FILE* fich = fopen(filename, "r");
    socklen_t clientLen = sizeof(client);
    int recvdSize;
    if (fich == NULL){
        perror("Cant open file\n");
        return -1;
    }
    printf("FILE OPENED: %s\n",filename);
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

    //--------------------------- SEND FILE CONTENT TO CLIENT -----------------
    int lastSeqN = filelen/dataSize + initAck ;
    float window = 5;
    int sent = -1;
    int transmitted = 0; //nb of bytes sent and acked
    int lastSent = initAck - 1; //seqN of last sent segment
    int lastTransmittedSeqN = initAck - 1; //seqN of last acked segment
    int maybeAcked; //used to store seqN answered by client
    float flightSize = 0;
    int dupAck = 0; 
    char* currentSeqN = (char *) malloc (SEQUENCELEN);
    long srtt_sec = 0; //arbitrary value, this estimator should converge to the real value of rtt_sec
    long srtt_usec = 0;
    timeout.tv_sec = srtt_sec;
    timeout.tv_usec = 0;

    struct timeval start,end,begin,stop;
    

    //----------- PERF ---------------------
    int lastDupAckRetr = -1;
    int segSent = 0;
    int computeSRTT = 0;
    int timeout_nb = 0;
    int dupack_nb = 0;
    int ignored_nb = 0;
    
    //Send window first segment
        printf("\n#lastSent: %d\n\n",lastSent);
        printf("window %f\n",window);
        strAck[3] = '\0';
        gettimeofday(&start,NULL);
        intToSeqN(initAck, currentSeqN);
        strncat(strAck, currentSeqN, seqNsize);
        msg[0] = '\0';
        strncat(msg, currentSeqN, seqNsize);
        memcpy(msg + seqNsize, content, dataSize);
        //WARNING FOR LATER : handle last message that can be shorter
        sent = sendto(sock, (char*) msg, dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
        gettimeofday(&begin,NULL);
        lastSent += 1;
        printf("\nSEG_%i SENT \n",lastSent);
        flightSize ++;
    //-------------------------------


    int lastMsgSize = filelen - (lastSeqN - initAck)*dataSize;
    printf("lastSEQN = %i\n", lastSeqN);
    int firstTime = 1;
    while (lastTransmittedSeqN < lastSeqN){
    //while (transmitted < filelen){
        FD_ZERO(&set);
        FD_SET(sock, &set);
        select(sock+1,&set,NULL,NULL,&timeout);

        //Put an arbitrary value (close to the real one) when we don't have compute yet the real one
        if (firstTime){
            timeout.tv_sec = 0;
            timeout.tv_usec = 5000;
            srtt_usec = 5000;
        }

        printf("window = %f | flightsize : %f | sstresh = %f \n\n",window,flightSize,sstresh);
        fprintf(log,"window : %f | sstresh : %f | flightsize : %f | SRTT : %ldµs\n",window,sstresh, flightSize,srtt_usec);
        if( FD_ISSET(sock,&set) ){

            recvdSize = recvfrom(sock, (char*) response, RCVSIZE, MSG_WAITALL, (struct sockaddr*)&client, &clientLen);

            response[recvdSize] = '\0';
            maybeAcked = seqNToInt(response + 3);
            response[3] = '\0';

                //Estimation du rtt_sec sur lequel vont se baser les futures estimations du rtt_sec        
	        if(firstTime){
                gettimeofday(&stop,NULL);
	            long rtt_sec = (stop.tv_sec - begin.tv_sec);
	            srtt_sec = ALPHA*srtt_sec + (1-ALPHA)*rtt_sec;
	            long rtt_usec = (rtt_sec*1000000 + stop.tv_usec - begin.tv_usec);
                srtt_usec = ALPHA*srtt_usec + (1-ALPHA)*rtt_usec;
                firstTime = 0;
                timeout.tv_sec = 0;
                timeout.tv_usec = srtt_usec;
            }
            
            //-------------------

            printf("\nACK_%i RCV \n",maybeAcked);

            if (strcmp(response, "ACK") == 0){
                if (flightSize > 0){
                        flightSize-= 1;
                    }

                
                if (maybeAcked > lastTransmittedSeqN){ //currently acked segment is the next one of the last acked segment
                      printf("normal ACK");
                      lastTransmittedSeqN = maybeAcked;
                      dupAck = 0; // /!\check

                      //******congestion avoidance
                     // window += 1/floor(window);window += 1/floor(window);
                    //*********

                      //transmit next segments (from lastSent not lastTransmitted)
                      segSent = 0;
                      while (flightSize < floor(window) && lastSent < lastSeqN ){
                        msg[0] = '\0';
                        intToSeqN(lastSent+1, currentSeqN);
                        strncat(msg, currentSeqN, seqNsize);
                            //if the message is shorter than dataSize
                        if( lastSent +1 < lastSeqN ){
                            memcpy(msg + seqNsize, content + (lastSent + 1 - initAck)*dataSize, dataSize); //WARNING : if dataSize=cste
                            sent = sendto(sock, (char*) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            printf("SEG_%i SENT \n",lastSent+1);
                        }else{
                            memcpy(msg + seqNsize, content + (lastSent + 1 - initAck)*dataSize, lastMsgSize); //WARNING : if dataSize=cste
                            sent = sendto(sock, (char*) msg,  lastMsgSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            printf("SEG_%i SENT \n",lastSent+1);
                        }

                        if (sent > -1){
                            flightSize ++;
                            lastSent += 1;
                        }
                        segSent++;
                    }

                }else if (lastTransmittedSeqN == maybeAcked) { //something went wrong with the transmission : the currently acked is not consecutive 
                    printf("dupack");
                    dupack_nb ++;
                    dupAck ++; //WARNING : not necessarly a dup ACK ? (if ack receiving order differs from ack sending order)
                    printf("Received ACK_%d for the %d time\n",maybeAcked,dupAck);

                    if (dupAck >= 3){ //consider a lost segment
                        printf("At least 3 dupAcks\n");
                        lastDupAckRetr = maybeAcked;
                        msg[0] = '\0';
                        intToSeqN(lastTransmittedSeqN + 1, currentSeqN);
                        strncat(msg, currentSeqN, seqNsize);

                            //if the message is shorter than dataSize
                        if(lastTransmittedSeqN < lastSeqN - 1){
                            memcpy(msg + seqNsize, content + (lastTransmittedSeqN - initAck + 1)*dataSize, dataSize); //WARNING : if dataSize=cste
                            sent = sendto(sock, (char*) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            printf("SEG_%i SENT \n",lastTransmittedSeqN+1);
                        }else{
                            memcpy(msg + seqNsize, content + (lastTransmittedSeqN - initAck + 1)*dataSize, lastMsgSize); //avant on avait mis lastDUpAck ici 
                            sent = sendto(sock, (char*) msg,  lastMsgSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            printf("SEG_%i SENT \n",lastTransmittedSeqN+1);
                        }

                        //WARNING FOR LATER : handle last message that can be shorter

                        if (sent > -1){
                            computeSRTT = 1;
                            flightSize ++;
                            int diff = lastTransmittedSeqN+1 - lastSent; //avant on avait mis 'lastDupack - lastSent
                            if (diff > 0){
                                //lastSent += diff; //not sure
                            }
                        }
                        
                        sstresh = ceilf(flightSize/2);
                        //window = sstresh + dupAck;
                        dupAck = 0;
                    
                    } // not yet considered as a lost segment -> keep sending
                        segSent = 0;
                        while (flightSize < floor(window) && lastSent < lastSeqN - 1){
                            msg[0] = '\0';
                            intToSeqN(lastSent + 1, currentSeqN);
                            strncat(msg, currentSeqN, seqNsize);

                                //if the message is shorter than dataSize
                            if( lastSent +1 < lastSeqN ){
                                memcpy(msg + seqNsize, content + (lastSent + 1 - initAck)*dataSize, dataSize); //WARNING : if dataSize=cste
                                sent = sendto(sock, (char*) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                                printf("SEG_%i SENT \n",lastSent + 1);
                            }else{
                                memcpy(msg + seqNsize, content + (lastSent + 1 - initAck)*dataSize, lastMsgSize/10); //WARNING : if dataSize=cste
                                sent = sendto(sock, (char*) msg,  lastMsgSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                                printf("SEG_%i SENT \n",lastSent + 1);
                            }

                            if (sent > -1){
                                computeSRTT = 1;
                                flightSize ++;
                                lastSent += 1;
                                //printf("SENT %i bytes | seqN = %i \n", sent, lastSent);
                            }
                            segSent++;
                        }
                    
                }else{
                    printf("Received an inferior ack -> ignored \n");
                    ignored_nb ++;
                    //******congestion avoidance
                     // window += 1/floor(window);
                    //*********
                    
                }
                printf("\n SRTT : \n %ld sec.\n %ld usec.\n\n",srtt_sec,srtt_usec);
                
            }
        //TIMEOUT : segment lost
        }else{
            printf("\n#TIMEOUT\n");
            fprintf(log,"#TIMEOUT\n");
            timeout_nb ++;

            msg[0] = '\0';
            intToSeqN(lastTransmittedSeqN + 1, currentSeqN);
            strncat(msg, currentSeqN, seqNsize);

            if(lastTransmittedSeqN >= lastSeqN - 1){
                printf("about to send from timeout, lastTransmitted = %i\n", lastTransmittedSeqN+1);
                memcpy(msg + seqNsize, content + (lastTransmittedSeqN + 1 - initAck)*dataSize, lastMsgSize); //WARNING : if dataSize=cste
                sent = sendto(sock, (char *) msg,  lastMsgSize + seqNsize, MSG_CONFIRM, (struct sockaddr *)&client, clientLen);
                printf("SEG_%i SENT \n",lastTransmittedSeqN+1);
            }else{
                printf("about to send from timeout, lastTransmitted = %i\n", lastTransmittedSeqN+1);
                memcpy(msg + seqNsize, content + (lastTransmittedSeqN + 1 - initAck)*dataSize, dataSize); //WARNING : if dataSize=cste
                sent = sendto(sock, (char *) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr *)&client, clientLen);
                printf("SEG_%i SENT \n",lastTransmittedSeqN+1);
            }
            
            timeout.tv_sec = 0;
            timeout.tv_usec = srtt_usec*1.3;
           // window = 4;
            sstresh = ceilf(flightSize/2);
            printf("\n SRTT : \n %ld sec.\n %ld usec.\n\n",srtt_sec,srtt_usec);
        }
    }

    //All bytes have been transmitted -> send END MSG

    /*char endMsg[] = "END_";
    strncat(endMsg, filename, strlen(filename));*/
    char endMsg[] = "FIN";
    sent = sendto(sock, (char*) endMsg, strlen(endMsg), MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
    while( sent < 0){
        sent = sendto(sock, (char *) endMsg, strlen(endMsg), MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
    }

    printf("FILE CONTENT FULLY TRANSMITTED, sent endMsg = %s\n", endMsg);
    //free(content);



    gettimeofday(&end,NULL);
    long seconds = (end.tv_sec - start.tv_sec);
    long micros = (seconds*1000000 + end.tv_usec - start.tv_usec);
    printf("\n-------------------\nPROGRAM RAN IN :\n %ld s and %ld us \nwith window = %f\n throughput = %f MB/s\nNb timeout : %d\nNb dupAck : %d\nNb ignored: %d\n SRTT : %ld µs\n-------------------\n", seconds,micros,window,
    (filelen/ ( seconds+micros*(pow(10,-6)) ) )*pow(10,-6),timeout_nb, dupack_nb, ignored_nb,srtt_usec );
    fprintf(perf,"\n-------------------\nPROGRAM RAN IN :\n %ld s and %ld us \nwith window = %f\n throughput = %f MB/s\nNb timeout : %d\nNb dupAck : %d\nNb ignored: %d\n SRTT : %ld µs\n-------------------\n", seconds,micros,window,
    (filelen/ ( seconds+micros*(pow(10,-6)) ) )*pow(10,-6),timeout_nb, dupack_nb, ignored_nb,srtt_usec );

    fclose(perf);
    fclose(log);

    //save time for comparison-----------------------------
   /* FILE* times = fopen("times.txt", "a");
    if (times){
        fprintf(times, "%f\n", seconds);
        fclose(times);
    }*/

    return 0;
}

//not used yet
char **splitData(char* src, int fragSize){
    char** res = (char**) malloc(sizeof(src));
    for( int i=0; i < strlen(src)/fragSize + 1; i++){
        strncpy(res[i], src + fragSize*i, fragSize);
    }
    return res;
}
