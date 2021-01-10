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
	#define ALPHA 0.9 //WARNING /!\ discuss about the value
#endif

void insertionListeTriee(LISTE *pliste, int ackN, long us_time)	
{
       if((*pliste)==NULL){ 
                (*pliste) = (LISTE) malloc(sizeof(LISTE));
                if (pliste == NULL) {
                        fprintf(stderr, "insertionListeTriee: plus de place mémoire");
                        exit(EXIT_FAILURE);
                }
                (*pliste)->seqN = ackN;
                (*pliste)->us = us_time;
                (*pliste)->suivant = NULL;
        }else if ((*pliste)->seqN > ackN){
                LISTE toSave = (LISTE) malloc(sizeof(LISTE));
                *toSave = **pliste;
                (*pliste)->seqN = ackN;
                (*pliste)->us = us_time;
                (*pliste)->suivant = toSave;
                //free(toSave);
        }else{
                insertionListeTriee(&((*pliste)->suivant), ackN, us_time);
        }
  return;
}
void incrListeBis(LISTE *pliste, int seqN)	{
    //in this lists, us <=> number of time a sseg has been retransmitted
       if((*pliste)==NULL){ 
                (*pliste) = (LISTE) malloc(sizeof(LISTE));
                if (pliste == NULL) {
                        fprintf(stderr, "insertionListeTriee: plus de place mémoire");
                        exit(EXIT_FAILURE);
                }
                (*pliste)->seqN = seqN;
                (*pliste)->us = 1;
                (*pliste)->suivant = NULL;
        }else if ((*pliste)->seqN > seqN){
                LISTE toSave = (LISTE) malloc(sizeof(LISTE));
                *toSave = **pliste;
                (*pliste)->seqN = seqN;
                (*pliste)->us = 1 ;
                (*pliste)->suivant = toSave;
                //free(toSave);
        }else if ((*pliste)->seqN == seqN){
            (*pliste)->us += 1;
        }else{
                incrListeBis(&((*pliste)->suivant), seqN);
        }
  return;
}

long decrListeBis(LISTE *pliste, int seqN)	{
    //in this lists, us <=> number of time a sseg has been retransmitted
       if((*pliste)==NULL){ 
           return 0;
        }else if ((*pliste)->seqN > seqN){
            return 0;
        }else if ((*pliste)->seqN == seqN){
            (*pliste)->us -= 1;
            return (*pliste)->us;
        }else{
                return decrListeBis(&((*pliste)->suivant), seqN);
        }
}

long getRetransmitted(LISTE *pliste, int seqN){
    if((*pliste)==NULL){ 
           return 0;
        }else if ((*pliste)->seqN > seqN){
            return 0;
        }else if ((*pliste)->seqN == seqN){
            return (*pliste)->us;
        }else{
                return getRetransmitted(&((*pliste)->suivant), seqN);
        }
}
long suppFirstOcc(LISTE *pliste, int ackN){
        if ( *pliste==NULL){
                return -1;
        }
        long res = -1;
        if ((*pliste)->seqN == ackN){
            res = (*pliste)->us;
            LISTE tmp = *pliste;
            *pliste = (*pliste)->suivant;
            free(tmp);
        }else if ( (*pliste)->seqN < ackN){
            res = suppFirstOcc(&((*pliste)->suivant),ackN);
        }
        return res;
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

			char portstr[PORTLEN+1];
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
    float window = 10; //with slow start the window starts to 1...
    float sstresh = 100; //...and the tresholt takes a first arbitrary great value at the beginning
    char* msg = (char*) malloc(dataSize + seqNsize);
    char* response = (char*) malloc(seqNsize+3);
    char strAck[] = "ACK";
    fd_set set;
    struct timeval timeout, start, end, begin, stop, now, temp_tv,fSent; //time after which we consider a segment lost

    // --------------------- READ FILE ------------------
    int block_size = 50000000;
    FILE* fich = fopen(filename, "r");
    socklen_t clientLen = sizeof(client);
    int recvdSize;
    if (fich == NULL){
        perror("Cant open file\n");
        return -1;
    }
    //printf("FILE OPENED: %s\n",filename);
    fseek(fich, 0, SEEK_END);
    long filelen = ftell(fich);
    fseek(fich, 0, SEEK_SET);
    int block_needed = filelen/block_size + 1;
    if (block_needed*block_size == filelen){ //the last char of the file will be stored on the last char of a block
        block_needed -=1;
    }
    printf("about to get the blocks\n");
    char *block1 = (char*) malloc(block_size*sizeof(char));
    char *block2 = (char*) malloc(block_size*sizeof(char));
    printf("got the blocks\n");

    content = (char*) malloc(filelen);
    if (content == NULL){
        perror("Memoire epuisee\n");
        return -1;
    }
    fread(content, 1, filelen, fich);
    fseek(fich,0,SEEK_SET);
    fread(block1,1,block_size,fich);
    fread(block2,1,block_size,fich);
    FILE *output = fopen("output.pdf","w");
    fwrite(block1,block_size,1,output);
    fwrite(block2,block_size,1,output);
    int block1_is = 0;
    int block2_is = 1;
    int block_to_get = 0; //{1,2} tells which block to use for memcpy
    int starts_in_block = -1; //used to temporarly store the block number corresponding to the beginning of a packet
    int end_in_block = -1 ; //used to temporarly store the block number corresponding to the end of a packet
    int all_in_block = 0; //{0,1} to store if all desired paquet content is within the same block
    int sizeToGet = 0;
    //fclose(fich);


    //DEBUG RTT
    FILE *rttLog = fopen("rtts.txt","w");//DEL
    FILE *windowLog = fopen("windows.txt","w");//DEL
    FILE *blockLog = fopen("blocks.txt","w");//DEL

    //--------------------------- SEND FILE CONTENT TO CLIENT -----------------
    int sent = -1;
    int successiveTO = 0; // number of successive timeout passages
    int lastSent = initAck - 1; //seqN of last sent segment
    int lastTransmittedSeqN = initAck - 1; //seqN of last acked segment
    int maybeAcked; //used to store seqN answered by client
    int flightSize = 0;
    int dupAck = 0;
    LISTE sendTimes = NULL; // used to store the sending instants of packets to compute the rtt
    LISTE retransmitted = NULL;//used to store how many times each segment (for a given seqN) has been retrasnmitted
    char* currentSeqN = (char *) malloc (SEQUENCELEN); //used to temporary store a sequence number
    long srtt_sec = 0; //arbitrary value, this estimator should converge to the real value of rtt
    long srtt_usec = 12000; //rtt estimation in ms
    timeout.tv_sec = srtt_sec; //will never be used (all will be put in us)
    timeout.tv_usec = srtt_usec;

    int wentToTO = 0;//DEL (debug)

    gettimeofday(&start, NULL);
    //WARNING : if window greater than total nb of segments 

    int lastSeqN = filelen/dataSize + initAck ;
    int lastMsgSize = filelen - (lastSeqN - initAck)*dataSize;
    printf("lastSEQN = %i\n", lastSeqN);

    fprintf(blockLog,"filelen=%ld,dataSize=%i,lastSN=%i,lastMsgSize=%i\n",filelen,dataSize, lastSeqN, lastMsgSize);

    //window = lastSeqN -initAck;
    //Send window first segments
    for (int i = 0; i < window; i++){
        //printf("\n#lastSent: %d\n\n",lastSent);
        //printf("i:%d,window %f\n",i,window);
        strAck[3] = '\0';
        starts_in_block = i*dataSize/block_size;
        end_in_block = (i+1)*dataSize/block_size;
        if (block1_is == starts_in_block){
            block_to_get = 1;
        }else if (block2_is == starts_in_block){
            block_to_get = 2;
        }else{
            if (block1_is > block2_is){
                fread(block2,1,block_size,fich);
                fwrite(block2,block_size,1,output);
                block2_is += 2;
                block_to_get = 2;
            }else{
                fread(block1,1,block_size,fich);
                fwrite(block1,block_size,1,output);
                block1_is += 2;
                block_to_get = 1;            
            }
        }
        all_in_block = (starts_in_block == end_in_block);
        
        intToSeqN(initAck + i, currentSeqN);
        //strncat(strAck, currentSeqN, seqNsize);
        msg[0] = '\0';
        strncat(msg, currentSeqN, seqNsize);
        if (block_to_get == 1){
            memcpy(msg + seqNsize,block1 + (i*dataSize - starts_in_block*block_size), dataSize );
            fprintf(blockLog,"writing %ith message from position %i\n",(int) initAck + i,i*dataSize -starts_in_block*block_size);
        }else if (block_to_get == 2){
            memcpy(msg + seqNsize,block2 + (i*dataSize - starts_in_block*block_size), dataSize );
        }else{
            printf("ERROR IN MEMCPY INIT\n");
            return -1;
        }

        if (!all_in_block){
            sizeToGet = dataSize - (block_size - (i*dataSize - starts_in_block*block_size));
            if (block_to_get == 2){
                if (block1_is != end_in_block){
                    fread(block1,1,block_size,fich);
                    fwrite(block1,block_size,1,output);
                    block1_is += 2;
                }
                memcpy(msg + seqNsize + (dataSize - sizeToGet) ,block1, sizeToGet);
            }else if (block_to_get == 1){
                if (block2_is != end_in_block){
                    fread(block2,1,block_size,fich);
                    fwrite(block2,block_size,1,output);
                    block2_is += 2;
                }
                memcpy(msg + seqNsize + (dataSize - sizeToGet),block2, sizeToGet );
            }else{
                printf("ERROR IN MEMCPY INIT\n");
                return -1;
            }
        }

        if (i==0){
            gettimeofday(&fSent,NULL);
        }
        //WARNING FOR LATER : handle last message that can e shorter
        sent = sendto(sock, (char*) msg, dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
        while (sent < 0){
            sent = sendto(sock, (char*) msg, dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
        }
        fprintf(blockLog, "last chars of msg=");
        for (int l=0;l<10;l++){
            fprintf(blockLog,"%c",msg[dataSize+seqNsize-1-9+l]);
        }
        fprintf(blockLog,", supposed to be=");
        for (int l=0;l<10;l++){
            fprintf(blockLog,"%c",content[(i+1)*dataSize-1-9+l]);
        }
        fprintf(blockLog,"\n");
        fprintf(blockLog, "first chars of msg=");
        for (int l=0;l<10;l++){
            fprintf(blockLog,"%c",msg[l]);
        }
        fprintf(blockLog,", supposed to be=%s",currentSeqN);
        for (int l=0;l<10-seqNsize;l++){
            fprintf(blockLog,"%c",content[l+i*dataSize]);
        }  
        fprintf(blockLog,"\n");
        fprintf(blockLog, "sib=%i, eib=%i, btg=%i, b1i=%i, b2i=%i, stg=%i, seg=%i (INIT)\n",starts_in_block,end_in_block,block_to_get,block1_is,block2_is, sizeToGet, initAck+i);
        lastSent += 1;
        printf("\nSEG_%i SENT from init\n",lastSent);
        gettimeofday(&temp_tv,NULL);
        insertionListeTriee(&sendTimes, lastSent, temp_tv.tv_sec*1000000 + temp_tv.tv_usec);
        printListe(sendTimes);
        flightSize ++;
        //printf("SENT %i bytes | seqN = %i \n", sent, initAck+i);
    }

    fprintf(blockLog, "in file=%c",content[1494]);
    fprintf(blockLog, ", in block=%c\n",block1[1494]);

    int gotFirst = 0;

    while (lastTransmittedSeqN < lastSeqN){

    //while (transmitted < filelen){
        FD_ZERO(&set);
        FD_SET(sock, &set);
        //printf("timeout : %ld s and %ld us\n", timeout.tv_sec, timeout.tv_usec);
        select(sock+1,&set,NULL,NULL,&timeout);
        fprintf(windowLog,"%f | %f\n",window,sstresh);//DEL
        if( FD_ISSET(sock,&set) ){

            recvdSize = recvfrom(sock, (char*) response, RCVSIZE, MSG_WAITALL, (struct sockaddr*)&client, &clientLen);

            response[recvdSize] = '\0';
            maybeAcked = seqNToInt(response + 3);
            response[3] = '\0';

            if (maybeAcked == 1 && lastTransmittedSeqN == initAck -1){
                gettimeofday(&now, NULL);
                gotFirst = 1;
            }

            printf("\nACK_%i RCV \n",maybeAcked);

            if (strcmp(response, "ACK") == 0){

                //Estimation du RTT sur lequel vont se baser les futures estimations du RTT
	            gettimeofday(&stop,NULL);
                long rtt_sec, rtt_usec;
                while (sendTimes != NULL && sendTimes->seqN < maybeAcked){
                    printf("supp|");
                    suppHead(&sendTimes);
                }
                printf("asupp\n");
                long sentTime = suppFirstOcc(&sendTimes,maybeAcked);
                if (sentTime > 0){
                    printf("removed %i from list\n",maybeAcked);
                    rtt_usec = stop.tv_sec*1000000 + stop.tv_usec - sentTime;
                    long temp = srtt_usec;
                    srtt_usec = ALPHA*srtt_usec + (1-ALPHA)*rtt_usec;
                    if (srtt_usec > 2*temp){
                        srtt_usec = temp;
                    }
                }else{
                    printf("no corresponding seqN:%i in sendTimes\n", maybeAcked);
                    //printListe(sendTimes);
                }
                timeout.tv_sec = 0;
                timeout.tv_usec = 0.07*srtt_usec; //RTTUPDATE
                fprintf(rttLog,"%li\n",srtt_usec); //DEL
                printf("timeout_us=%ld\n",timeout.tv_usec);
                gettimeofday(&begin,NULL);

                if(flightSize > 0){
                   flightSize --; //because a segment has been acked
                }

                
                if (maybeAcked > lastTransmittedSeqN){ //currently acked segment is the next one of the last acked segment
                    
                    //******slowstart
                      if(window < sstresh){
                          window += maybeAcked - lastTransmittedSeqN;
                          //congestion avoidance
                      }else{
                          window += ( (maybeAcked - lastTransmittedSeqN)/floor(window) );
                      }
                    //*********

                      lastTransmittedSeqN = maybeAcked;
                      dupAck = 0; // /!\check

                      //transmit next segments (from lastSent not lastTransmitted)
                      //printf("\nflightSize : %d, window : %f\n", flightSize,window);
                    while (flightSize < floor(window) && lastSent < lastSeqN ){
  
                        //printf("transmitted : %d, filelen : %ld\n",transmitted,filelen);
                        msg[0] = '\0';
                        intToSeqN(lastSent+1, currentSeqN);
                        strncat(msg, currentSeqN, seqNsize);
                        starts_in_block = ((lastSent + 1 - initAck)*dataSize)/block_size;
                        if (lastSent + 1 == lastSeqN){
                            end_in_block = (((lastSent + 1 - initAck)*dataSize) + lastMsgSize)/block_size;
                        }else{
                            end_in_block = ((lastSent + 2 - initAck)*dataSize)/block_size;
                        }                        
                        if (block1_is == starts_in_block){
                            block_to_get = 1;
                        }else if (block2_is == starts_in_block){
                            block_to_get = 2;
                        }else{
                            if (block1_is > block2_is){
                                fread(block2,1,block_size,fich);
                                fwrite(block2,block_size,1,output);
                                block2_is += 2;
                                block_to_get = 2;
                            }else{
                                fread(block1,1,block_size,fich);
                                fwrite(block1,block_size,1,output);
                                block1_is += 2;
                                block_to_get = 1;            
                            }
                        }
                        all_in_block = (starts_in_block == end_in_block);
                        
                            //if the message is shorter than dataSize
                        if( lastSent +1 < lastSeqN ){
                            if (block_to_get == 1){
                                memcpy(msg + seqNsize,block1 + (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size), dataSize );
                            }else if (block_to_get == 2){
                                memcpy(msg + seqNsize,block2 + (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size), dataSize );
                            }else{
                                printf("ERROR IN MEMCPY FROM SUPERIOR (<lastSeq) \n");
                                return -1;
                            }
                    
                            if (!all_in_block){
                                sizeToGet = dataSize - (block_size - (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size));
                                if (block_to_get == 2){
                                    if (block1_is != end_in_block){
                                        fread(block1,1,block_size,fich);
                                        fwrite(block1,block_size,1,output);
                                        block1_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (dataSize - sizeToGet) ,block1, sizeToGet);
                                }else if (block_to_get == 1){
                                    if (block2_is != end_in_block){
                                        fread(block2,1,block_size,fich);
                                        fwrite(block2,block_size,1,output);
                                        block2_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (dataSize - sizeToGet),block2, sizeToGet );
                                }else{
                                    printf("ERROR IN MEMCPY FROM SUPERIOR (<lastSeq)\n");
                                    return -1;
                                }
                            }
                            sent = sendto(sock, (char*) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            while (sent < 0){
                                sent = sendto(sock, (char*) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            }

                            printf("SEG_%i SENT from normal\n",lastSent+1);
                        }else{
                            if (block_to_get == 1){
                                memcpy(msg + seqNsize,block1 + (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size), lastMsgSize );
                            }else if (block_to_get == 2){
                                memcpy(msg + seqNsize,block2 + (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size), lastMsgSize );
                            }else{
                                printf("ERROR IN MEMCPY INIT\n");
                                return -1;
                            }

                            if (!all_in_block){
                                sizeToGet = lastMsgSize - (block_size - (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size));
                                if (block_to_get == 2){
                                    if (block1_is != end_in_block){
                                        fread(block1,1,block_size,fich);
                                        fwrite(block1,block_size,1,output);
                                        block1_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (lastMsgSize - sizeToGet) ,block1, sizeToGet);
                                }else if (block_to_get == 1){
                                    if (block2_is != end_in_block){
                                        fread(block2,1,block_size,fich);
                                        fwrite(block2,block_size,1,output);
                                        block2_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (lastMsgSize - sizeToGet),block2, sizeToGet );
                                }else{
                                    printf("ERROR IN MEMCPY FROM SUPERIOR (=lastSeq)\n");
                                    return -1;
                                }
                            }
                            sent = sendto(sock, (char*) msg,  lastMsgSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            while (sent < 0){
                                sent = sendto(sock, (char*) msg,  lastMsgSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);

                            }
                            //printf("SEG_%i SENT \n",lastSent+1);
                            //printf("message :\n %s \n-------------------\n",msg);
                        }

                        fprintf(blockLog, "sib=%i, eib=%i, btg=%i, b1i=%i, b2i=%i, stg=%i, seg=%i (SUP)\n",starts_in_block,end_in_block,block_to_get,block1_is,block2_is, sizeToGet, lastSent+1);
                        
                        //gettimeofday(&begin,NULL);
                        if (sent > -1){
                            flightSize ++;
                            lastSent += 1;
                            gettimeofday(&temp_tv,NULL);
                            insertionListeTriee(&sendTimes, lastSent, temp_tv.tv_sec*1000000 + temp_tv.tv_usec);
                            if (lastSent < 30){
                                printListe(sendTimes);
                            }
                        }
                        //printf("\n#lastSent: %d\n\n",lastSent);
                        //printf("flightsize : %d, floor(window) : %f\n",flightSize,floor(window));
                    }
                }else if (lastTransmittedSeqN == maybeAcked) { //something went wrong with the transmission : the currently acked is not consecutive 

                    dupAck ++; //WARNING : not necessarly a dup ACK ? (if ack receiving order differs from ack sending order)
                    //printf("Received ACK_%d for the %d time\n",maybeAcked,dupAck);
                    int isNormal = (decrListeBis(&retransmitted,maybeAcked) > 0);
                    if (dupAck >= 10 && isNormal == 0){ //consider a lost segment
                        //printf("At least 3 dupAcks\n");
                        //printf("flightsize : %d, floor(window) : %f, sent: %d\n",flightSize,floor(window),sent);

                        msg[0] = '\0';
                        intToSeqN(lastTransmittedSeqN + 1, currentSeqN);
                        strncat(msg, currentSeqN, seqNsize);
                        starts_in_block = ((lastTransmittedSeqN + 1 - initAck)*dataSize)/block_size;
                        if (lastTransmittedSeqN + 1 == lastSeqN){
                            end_in_block = (((lastTransmittedSeqN + 1 - initAck)*dataSize) + lastMsgSize)/block_size;
                        }else{
                            end_in_block = ((lastTransmittedSeqN + 2 - initAck)*dataSize)/block_size;
                        }                        
                        if (block1_is == starts_in_block){
                            block_to_get = 1;
                        }else if (block2_is == starts_in_block){
                            block_to_get = 2;
                        }else{
                            if (block1_is > block2_is){
                                fread(block2,1,block_size,fich);
                                fwrite(block2,block_size,1,output);
                                block2_is += 2;
                                block_to_get = 2;
                            }else{
                                fread(block1,1,block_size,fich);
                                fwrite(block1,block_size,1,output);
                                block1_is += 2;
                                block_to_get = 1;            
                            }
                        }
                        all_in_block = (starts_in_block == end_in_block);

                            //if the message is shorter than dataSize
                        if(lastTransmittedSeqN < lastSeqN - 1){
                            if (block_to_get == 1){
                                memcpy(msg + seqNsize,block1 + (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size), dataSize );
                            }else if (block_to_get == 2){
                                memcpy(msg + seqNsize,block2 + (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size), dataSize );
                            }else{
                                printf("ERROR IN MEMCPY FROM DUP (<lastSeq) \n");
                                return -1;
                            }
                    
                            if (!all_in_block){
                                sizeToGet = dataSize - (block_size - (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size));
                                if (block_to_get == 2){
                                    if (block1_is != end_in_block){
                                        fread(block1,1,block_size,fich);
                                        fwrite(block1,block_size,1,output);
                                        block1_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (dataSize - sizeToGet) ,block1, sizeToGet);
                                }else if (block_to_get == 1){
                                    if (block2_is != end_in_block){
                                        fread(block2,1,block_size,fich);
                                        fwrite(block2,block_size,1,output);
                                        block2_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (dataSize - sizeToGet),block2, sizeToGet );
                                }else{
                                    printf("ERROR IN MEMCPY FROM DUP (<lastSeq)\n");
                                    return -1;
                                }
                            }
                            sent = sendto(sock, (char*) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            printf("SEG_%i SENT from dupack\n",lastTransmittedSeqN+1);
                        }else{

                            if (block_to_get == 1){
                                memcpy(msg + seqNsize,block1 + (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size), lastMsgSize );
                            }else if (block_to_get == 2){
                                memcpy(msg + seqNsize,block2 + (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size), lastMsgSize );
                            }else{
                                printf("ERROR IN MEMCPY DUP\n");
                                return -1;
                            }

                            if (!all_in_block){
                                sizeToGet = lastMsgSize - (block_size - (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size));
                                if (block_to_get == 2){
                                    if (block1_is != end_in_block){
                                        fread(block1,1,block_size,fich);
                                        fwrite(block1,block_size,1,output);
                                        block1_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (lastMsgSize - sizeToGet) ,block1, sizeToGet);
                                }else if (block_to_get == 1){
                                    if (block2_is != end_in_block){
                                        fread(block2,1,block_size,fich);
                                        fwrite(block2,block_size,1,output);
                                        block2_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (lastMsgSize - sizeToGet),block2, sizeToGet );
                                }else{
                                    printf("ERROR IN MEMCPY FROM DUP (=lastSeq)\n");
                                    return -1;
                                }
                            }
                            sent = sendto(sock, (char*) msg,  lastMsgSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            //printf("SEG_%i SENT \n",lastTransmittedSeqN+1);
                            //printf("message :\n %s \n-------------------\n",msg);
                        }
                        fprintf(blockLog, "sib=%i, eib=%i, btg=%i, b1i=%i, b2i=%i, stg=%i, seg=%i (DUP)\n",starts_in_block,end_in_block,block_to_get,block1_is,block2_is, sizeToGet, lastTransmittedSeqN+1);

                        //WARNING FOR LATER : handle last message that can be shorter
                        //gettimeofday(&begin,NULL);

                        if (sent > -1){
                            flightSize ++;
                            incrListeBis(&retransmitted, lastTransmittedSeqN+1);
                            /*int diff = lastTransmittedSeqN+1 - lastSent; //avant on avait mis 'lastDupack - lastSent
                            if (diff > 0){
                                lastSent += diff; //not sure
                            }*/
                            gettimeofday(&temp_tv,NULL);
                            insertionListeTriee(&sendTimes, lastTransmittedSeqN+1, temp_tv.tv_sec*1000000 + temp_tv.tv_usec);
                               //printf("SENT %i bytes | seqN = %i \n", sent, lastTransmittedSeqN + i - 1);
                        }
                        
                        sstresh = flightSize/2;
                        window = sstresh + dupAck;
                        dupAck = 0;
                    
                    }else{ // not yet considered as a lost segment -> keep sending
                        while (flightSize < floor(window) && lastSent < lastSeqN - 1){
                            msg[0] = '\0';
                            intToSeqN(lastSent + 1, currentSeqN);
                            strncat(msg, currentSeqN, seqNsize);
                            starts_in_block = ((lastSent + 1 - initAck)*dataSize)/block_size;
                            if (lastSent + 1 == lastSeqN){
                                end_in_block = (((lastSent + 1 - initAck)*dataSize) + lastMsgSize)/block_size;
                            }else{
                                end_in_block = ((lastSent + 2 - initAck)*dataSize)/block_size;
                            }                            
                            if (block1_is == starts_in_block){
                                block_to_get = 1;
                            }else if (block2_is == starts_in_block){
                                block_to_get = 2;
                            }else{
                                if (block1_is > block2_is){
                                    fread(block2,1,block_size,fich);
                                    fwrite(block2,block_size,1,output);
                                    block2_is += 2;
                                    block_to_get = 2;
                                }else{
                                    fread(block1,1,block_size,fich);
                                    fwrite(block1,block_size,1,output);
                                    block1_is += 2;
                                    block_to_get = 1;            
                                }
                            }
                            all_in_block = (starts_in_block == end_in_block);
                        if( lastSent +1 < lastSeqN ){
                            if (block_to_get == 1){
                                memcpy(msg + seqNsize,block1 + (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size), dataSize );
                            }else if (block_to_get == 2){
                                memcpy(msg + seqNsize,block2 + (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size), dataSize );
                            }else{
                                printf("ERROR IN MEMCPY FROM <DUP (<lastSeq) \n");
                                return -1;
                            }
                    
                            if (!all_in_block){
                                sizeToGet = dataSize - (block_size - (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size));
                                if (block_to_get == 2){
                                    if (block1_is != end_in_block){
                                        fread(block1,1,block_size,fich);
                                        fwrite(block1,block_size,1,output);
                                        block1_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (dataSize - sizeToGet) ,block1, sizeToGet);
                                }else if (block_to_get == 1){
                                    if (block2_is != end_in_block){
                                        fread(block2,1,block_size,fich);
                                        fwrite(block2,block_size,1,output);
                                        block2_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (dataSize - sizeToGet),block2, sizeToGet );
                                }else{
                                    printf("ERROR IN MEMCPY FROM <DUP (<lastSeq)\n");
                                    return -1;
                                }
                            }
                            //printf("before copy\n");
                            //printf("after copy\n");
                            sent = sendto(sock, (char*) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            printf("SEG_%i SENT from <dupack\n",lastSent + 1);
                       }else{

                           if (block_to_get == 1){
                                memcpy(msg + seqNsize,block1 + (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size), lastMsgSize );
                            }else if (block_to_get == 2){
                                memcpy(msg + seqNsize,block2 + (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size), lastMsgSize );
                            }else{
                                printf("ERROR IN MEMCPY FROM <DUPACK\n");
                                return -1;
                            }

                            if (!all_in_block){
                                sizeToGet = lastMsgSize - (block_size - (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size));
                                if (block_to_get == 2){
                                    if (block1_is != end_in_block){
                                        fread(block1,1,block_size,fich);
                                        fwrite(block1,block_size,1,output);
                                        block1_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (lastMsgSize - sizeToGet) ,block1, sizeToGet);
                                }else if (block_to_get == 1){
                                    if (block2_is != end_in_block){
                                        fread(block2,1,block_size,fich);
                                        fwrite(block2,block_size,1,output);
                                        block2_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (lastMsgSize - sizeToGet),block2, sizeToGet );
                                }else{
                                    printf("ERROR IN MEMCPY FROM <DUPACK (=lastSeq)\n");
                                    return -1;
                                }
                            }

                            sent = sendto(sock, (char*) msg,  lastMsgSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                        }
                        fprintf(blockLog, "sib=%i, eib=%i, btg=%i, b1i=%i, b2i=%i, stg=%i, seg=%i (<DUP)\n",starts_in_block,end_in_block,block_to_get,block1_is,block2_is, sizeToGet, lastSent+1);

                            if (sent > -1){
                                flightSize ++;
                                lastSent += 1;
                                gettimeofday(&temp_tv,NULL);
                                insertionListeTriee(&sendTimes, lastSent, temp_tv.tv_sec*1000000 + temp_tv.tv_usec);
                            }
                        }
                    }
                }else{
                    //printf("Received an inferior ack -> ignored \n");
                }
            }
            successiveTO = 0;
        //TIMEOUT : segment lost
        }else{
            
            sstresh = flightSize/2;
            //window = 1; seems to reduce performances
            wentToTO ++;//DEL
            msg[0] = '\0';

            intToSeqN(lastTransmittedSeqN + 1, currentSeqN);
            strncat(msg, currentSeqN, seqNsize);
            starts_in_block = ((lastTransmittedSeqN + 1 - initAck)*dataSize)/block_size;
            if (lastTransmittedSeqN + 1 == lastSeqN){
                end_in_block = (((lastTransmittedSeqN + 1 - initAck)*dataSize) + lastMsgSize)/block_size;
            }else{
                end_in_block = ((lastTransmittedSeqN + 2 - initAck)*dataSize)/block_size;
            }
            if (block1_is == starts_in_block){
                block_to_get = 1;
            }else if (block2_is == starts_in_block){
                block_to_get = 2;
            }else{
                if (block1_is > block2_is){
                    fread(block2,1,block_size,fich);
                    fwrite(block2,block_size,1,output);
                    block2_is += 2;
                    block_to_get = 2;
                }else{
                    fread(block1,1,block_size,fich);
                    fwrite(block1,block_size,1,output);
                    block1_is += 2;
                    block_to_get = 1;            
                }
            }
            all_in_block = (starts_in_block == end_in_block);
            int alreadyRT = getRetransmitted(&retransmitted,lastTransmittedSeqN+1);

            if(lastTransmittedSeqN >= lastSeqN - 1){
                if (block_to_get == 1){
                    memcpy(msg + seqNsize,block1 + (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size), lastMsgSize );
                }else if (block_to_get == 2){
                    memcpy(msg + seqNsize,block2 + (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size), lastMsgSize );
                }else{
                    printf("ERROR IN MEMCPY FROM DUP (<lastSeq) \n");
                    return -1;
                }
        
                if (!all_in_block){
                    sizeToGet = lastMsgSize - (block_size - (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size));
                    if (block_to_get == 2){
                        if (block1_is != end_in_block){
                            fread(block1,1,block_size,fich);
                            fwrite(block1,block_size,1,output);
                            block1_is += 2;
                        }
                        memcpy(msg + seqNsize + (lastMsgSize - sizeToGet) ,block1, sizeToGet);
                    }else if (block_to_get == 1){
                        if (block2_is != end_in_block){
                            fread(block2,1,block_size,fich);
                            fwrite(block2,block_size,1,output);
                            block2_is += 2;
                        }
                        memcpy(msg + seqNsize + (lastMsgSize - sizeToGet),block2, sizeToGet );
                    }else{
                        printf("ERROR IN MEMCPY FROM DUP (<lastSeq)\n");
                        return -1;
                    }
                }
                sent = sendto(sock, (char *) msg,  lastMsgSize + seqNsize, MSG_CONFIRM, (struct sockaddr *)&client, clientLen);
                //printf("SEG_%i SENT from timeout\n",lastTransmittedSeqN+1);
                //printf("message :\n %s \n-------------------\n",msg);
            }else{

                if (block_to_get == 1){
                    memcpy(msg + seqNsize,block1 + (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size), dataSize );
                }else if (block_to_get == 2){
                    memcpy(msg + seqNsize,block2 + (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size), dataSize );
                }else{
                    printf("ERROR IN MEMCPY DUP\n");
                    return -1;
                }

                if (!all_in_block){
                    sizeToGet = dataSize - (block_size - (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size));
                    if (block_to_get == 2){
                        if (block1_is != end_in_block){
                            fread(block1,1,block_size,fich);
                            fwrite(block1,block_size,1,output);
                            block1_is += 2;
                        }
                        memcpy(msg + seqNsize + (dataSize - sizeToGet) ,block1, sizeToGet);
                    }else if (block_to_get == 1){
                        if (block2_is != end_in_block){
                            fread(block2,1,block_size,fich);
                            fwrite(block2,block_size,1,output);
                            block2_is += 2;
                        }
                        memcpy(msg + seqNsize + (dataSize - sizeToGet),block2, sizeToGet );
                    }else{
                        printf("ERROR IN MEMCPY FROM DUP (=lastSeq)\n");
                        return -1;
                    }
                }
                //printf("about to send from timeout, lastTransmitted = %i\n", lastTransmittedSeqN+1);
                //printf("copied msg\n");
                sent = sendto(sock, (char *) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr *)&client, clientLen);
                printf("SEG_%i SENT from timeout with rtt = %lds and %ldus\n",lastTransmittedSeqN+1, srtt_sec,srtt_usec);
            }
            fprintf(blockLog, "sib=%i, eib=%i, btg=%i, b1i=%i, b2i=%i, stg=%i, seg=%i (TO)\n",starts_in_block,end_in_block,block_to_get,block1_is,block2_is, sizeToGet, lastTransmittedSeqN+1);
            if (sent > -1){
                incrListeBis(&retransmitted, lastTransmittedSeqN+1);
            }
            gettimeofday(&temp_tv,NULL);
            insertionListeTriee(&sendTimes, lastTransmittedSeqN+1, temp_tv.tv_sec*1000000 + temp_tv.tv_usec);
            if (lastTransmittedSeqN > 750){
                printListe(sendTimes);
            }
            
            successiveTO++;


            if (successiveTO >= 20){
                srtt_sec = srtt_sec*1.4;
                srtt_usec = srtt_usec*1.4;
                printf("increased to\n");
                successiveTO = 0;
            }
            timeout.tv_sec = 0;
            timeout.tv_usec = 0.07*srtt_usec; //RTTUPDATE
        }
    }

    //All bytes have been transmitted -> send END MSG

    /*char endMsg[] = "END_";
    strncat(endMsg, filename, strlen(filename));*/
    char endMsg[] = "FIN";
    sent = sendto(sock, (char*) endMsg, strlen(endMsg), MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
    while( sent < 0){
        sent = sendto(sock, (char*) endMsg, strlen(endMsg), MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
    }
    printf("FILE CONTENT FULLY TRANSMITTED, sent endMsg = %s\n", endMsg);

    free(content);

    gettimeofday(&end,NULL);
    float execTime = (float)(end.tv_sec*1000000 + end.tv_usec - start.tv_sec*1000000 - start.tv_usec)/1000000;
    
    printf("program ran in %f s  <=> %f Mo/s\n", execTime, filelen/(execTime*1000000));
     
    double fRTT = ((now.tv_sec - fSent.tv_sec)*1000000 + now.tv_usec - fSent.tv_usec);
    printf("First rtt = %fus, now.tv_usec = %li, gotFirst = %i\n",fRTT, now.tv_usec, gotFirst);
    printf("timeouts = %i\n", wentToTO);//DEL
    fclose(rttLog);//DEL
    fclose(windowLog);//DEL
    fclose(fich);
    fclose(blockLog);
    /*sleep(1);
    printf("About to read file\n");
    FILE* recept = fopen("copy_rfc793.pdf", "r");
    char* rContent = (char*) malloc(filelen);
    if (rContent == NULL){
        printf("No more memory\n");
    }else{
        printf("opened and init rContent done¡\n");
    }
    fread(rContent, 1, filelen, recept);
    fclose(recept);
    printf("file closed\n");
    printf("Comparing received and sent\n");
    for (long i = 0; i<filelen; i++){
        if (content[i] != rContent[i]){
            printf("\n\nNOT SAME, i=%li <=> content[i]=%c & rContent[i]=%c,\n\n", i, content[i], rContent[i]);
            break;
        }
    }
    printf("Compared\n");*/
    


    //save time for comparison-----------------------------
    FILE* times = fopen("times.txt", "a");
    if (times){
        fprintf(times, "%f\n", execTime);
        fclose(times);
    }

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
