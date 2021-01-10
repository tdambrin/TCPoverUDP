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

#include "utilServer2.h"

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
int readAndSendFile(int sock, struct sockaddr_in client, char* filename, int dataSize, int seqNsize, int initAck, int my_window){
                        //WARNING FOR LATER : handle sending of more content than existing

    FILE* log = fopen("log.txt", "w");
    FILE* perf = fopen("perf.txt", "a");

    // ---------------------- INIT ---------------------
    char* content;
    //float window = 1; //with slow start the window starts to 1...
    float sstresh = 8; //...and the tresholt takes a first arbitrary great value at the beginning
    char* msg = (char*) malloc(dataSize + seqNsize);
    char* response = (char*) malloc(seqNsize+3);
    fd_set set;
    struct timeval timeout; //time after which we consider a segment lost


        // --------------------- READ FILE ------------------
    int block_size = 50000000;
    FILE* fich = fopen(filename, "r");
    socklen_t clientLen = sizeof(client);
    int recvdSize;
    if (fich == NULL){
        perror("Cant open file\n");
        return -1;
    }
    fseek(fich, 0, SEEK_END);
    long filelen = ftell(fich);
    fseek(fich, 0, SEEK_SET);
    int block_needed = filelen/block_size + 1;
    if (block_needed*block_size == filelen){ //the last char of the file will be stored on the last char of a block
        block_needed -=1;
    }
    char *block1 = (char*) malloc(block_size*sizeof(char));
    char *block2 = (char*) malloc(block_size*sizeof(char));
    fread(block1,1,block_size,fich);
    fread(block2,1,block_size,fich);
    int block1_is = 0;
    int block2_is = 1;
    int block_to_get = 0; //{1,2} tells which block to use for memcpy
    int starts_in_block = -1; //used to temporarly store the block number corresponding to the beginning of a packet
    int end_in_block = -1 ; //used to temporarly store the block number corresponding to the end of a packet
    int all_in_block = 0; //{0,1} to store if all desired paquet content is within the same block
    int sizeToGet = 0;

    //--------------------------- SEND FILE CONTENT TO CLIENT -----------------
    int lastSeqN = filelen/dataSize + initAck ;
    float min_window = my_window;
    float window = min_window;
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
    int srtt_raw = 5000;





//########## TRANSMISSION ##########
    
    //Send the first segment and measure the fix value of the RTT----------------------

        starts_in_block = 0;
        end_in_block = dataSize/block_size;
        if (block1_is == starts_in_block){
            block_to_get = 1;
        }else if (block2_is == starts_in_block){
            block_to_get = 2;
        }else{
            if (block1_is > block2_is){
                fread(block2,1,block_size,fich);
                block2_is += 2;
                block_to_get = 2;
            }else{
                fread(block1,1,block_size,fich);
                block1_is += 2;
                block_to_get = 1;            
            }
        }
        all_in_block = (starts_in_block == end_in_block);

        printf("\n#lastSent: %d\n\n",lastSent);
        printf("window %f\n",window);
        gettimeofday(&start,NULL);
        intToSeqN(initAck, currentSeqN);
        msg[0] = '\0';
        strncat(msg, currentSeqN, seqNsize);

        if (block_to_get == 1){
            memcpy(msg + seqNsize,block1, dataSize );
        }else if (block_to_get == 2){
            memcpy(msg + seqNsize,block2, dataSize );
        }else{
            perror("ERROR IN MEMCPY INIT\n");
            return -1;
        }

        sent = sendto(sock, (char*) msg, dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
        gettimeofday(&begin,NULL);
        lastSent += 1;
        printf("\nSEG_%i SENT \n",lastSent);
        flightSize ++;
    //-------------------------------


    int lastMsgSize = filelen - (lastSeqN - initAck)*dataSize;
    printf("lastSEQN = %i\n", lastSeqN);
    int firstTime = 1;
    
    //Send all the segments
    while (lastTransmittedSeqN < lastSeqN){
        FD_ZERO(&set);
        FD_SET(sock, &set);
        select(sock+1,&set,NULL,NULL,&timeout);

        //Put an arbitrary value (close to the real one) when we don't have compute yet the real one
        if (firstTime){
            timeout.tv_sec = 0;
            timeout.tv_usec = srtt_raw;
            srtt_usec = srtt_raw;
        }

        printf("window = %f | flightsize : %f | sstresh = %f \n\n",window,flightSize,sstresh);
        fprintf(log,"window : %f | sstresh : %f | flightsize : %f | SRTT : %ldµs\n",window,sstresh, flightSize,srtt_usec);
        if( FD_ISSET(sock,&set) ){

            recvdSize = recvfrom(sock, (char*) response, RCVSIZE, MSG_WAITALL, (struct sockaddr*)&client, &clientLen);

            response[recvdSize] = '\0';
            maybeAcked = seqNToInt(response + 3);
            response[3] = '\0';
       
	        if(firstTime){
                gettimeofday(&stop,NULL);
	            long srtt_sec = (stop.tv_sec - begin.tv_sec);
	            long srtt_usec = (srtt_sec*1000000 + stop.tv_usec - begin.tv_usec);
                firstTime = 0;

                //The RTT measured is inconsistent
                if(srtt_usec > 2*srtt_raw){
                    srtt_usec = srtt_raw;
                }
                timeout.tv_sec = 0;
                timeout.tv_usec = srtt_usec;
            }
            
            //-------------------

            printf("\nACK_%i RCV \n",maybeAcked);

            if (strcmp(response, "ACK") == 0){
                if (flightSize > 0){
                        flightSize-= 1;
                    }


                //Currently acked segment is the next one of the last acked segment
                if (maybeAcked > lastTransmittedSeqN){ 
                      printf("normal ACK");
                      lastTransmittedSeqN = maybeAcked;
                      dupAck = 0; // /!\check

                      //******congestion avoidance ou slowstart
                     //window = window < sstresh ? window*2 : window + 1;
                    //*********

                      //transmit next segments (from lastSent not lastTransmitted)
                      segSent = 0;
                      while (flightSize < floor(window) && lastSent < lastSeqN ){
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
                                block2_is += 2;
                                block_to_get = 2;
                            }else{
                                fread(block1,1,block_size,fich);
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
                                perror("ERROR IN MEMCPY FROM SUPERIOR (<lastSeq) \n");
                                return -1;
                            }
                    
                            if (!all_in_block){
                                sizeToGet = dataSize - (block_size - (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size));
                                if (block_to_get == 2){
                                    if (block1_is != end_in_block){
                                        fread(block1,1,block_size,fich);
                                        block1_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (dataSize - sizeToGet) ,block1, sizeToGet);
                                }else if (block_to_get == 1){
                                    if (block2_is != end_in_block){
                                        fread(block2,1,block_size,fich);
                                        block2_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (dataSize - sizeToGet),block2, sizeToGet );
                                }else{
                                    perror("ERROR IN MEMCPY FROM SUPERIOR (<lastSeq)\n");
                                    return -1;
                                }
                            }                            
                            sent = sendto(sock, (char*) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            printf("SEG_%i SENT \n",lastSent+1);
                        }else{
                            if (block_to_get == 1){
                                memcpy(msg + seqNsize,block1 + (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size), lastMsgSize );
                            }else if (block_to_get == 2){
                                memcpy(msg + seqNsize,block2 + (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size), lastMsgSize );
                            }else{
                                perror("ERROR IN MEMCPY SUPERIOR\n");
                                return -1;
                            }

                            if (!all_in_block){
                                sizeToGet = lastMsgSize - (block_size - (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size));
                                if (block_to_get == 2){
                                    if (block1_is != end_in_block){
                                        fread(block1,1,block_size,fich);
                                        block1_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (lastMsgSize - sizeToGet) ,block1, sizeToGet);
                                }else if (block_to_get == 1){
                                    if (block2_is != end_in_block){
                                        fread(block2,1,block_size,fich);
                                        block2_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (lastMsgSize - sizeToGet),block2, sizeToGet );
                                }else{
                                    perror("ERROR IN MEMCPY FROM SUPERIOR (=lastSeq)\n");
                                    return -1;
                                }
                            }                     
                            sent = sendto(sock, (char*) msg,  lastMsgSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            printf("SEG_%i SENT \n",lastSent+1);
                        }

                        if (sent > -1){
                            flightSize ++;
                            lastSent += 1;
                        }
                        segSent++;
                    }

                //Something went wrong with the transmission : the currently acked is not greater
                }else if (lastTransmittedSeqN == maybeAcked) {  
                    printf("dupack");
                    dupack_nb ++;
                    dupAck ++; //WARNING : not necessarly a dup ACK ? (if ack receiving order differs from ack sending order)
                    printf("Received ACK_%d for the %d time\n",maybeAcked,dupAck);

                    //We received 3 times the same ACK, the segment is considered as lost [FastRetransmit]
                    if (dupAck >= 3){ 
                        printf("At least 3 dupAcks\n");
                        lastDupAckRetr = maybeAcked;
                        msg[0] = '\0';
                        intToSeqN(lastTransmittedSeqN + 1, currentSeqN);
                        strncat(msg, currentSeqN, seqNsize);

                        //Find in which block the begining of the segment is
                        starts_in_block = ((lastTransmittedSeqN + 1 - initAck)*dataSize)/block_size;
                        
                        //Find in which block the end of the segment is ...
                        if (lastTransmittedSeqN + 1 == lastSeqN){
                            end_in_block = (((lastTransmittedSeqN + 1 - initAck)*dataSize) + lastMsgSize)/block_size;
                        }else{
                            end_in_block = ((lastTransmittedSeqN + 2 - initAck)*dataSize)/block_size;
                        }      

                        //..and in which block we have to start to take data                  
                        if (block1_is == starts_in_block){
                            block_to_get = 1;
                        }else if (block2_is == starts_in_block){
                            block_to_get = 2;
                        }else{
                            if (block1_is > block2_is){
                                fread(block2,1,block_size,fich);
                                block2_is += 2;
                                block_to_get = 2;
                            }else{
                                fread(block1,1,block_size,fich);
                                block1_is += 2;
                                block_to_get = 1;            
                            }
                        }

                        //Is the segment we have to send in one block or in two blocks ?
                        all_in_block = (starts_in_block == end_in_block);

                        //We send the last segment, the data is smaller
                        if(lastTransmittedSeqN < lastSeqN - 1){
                            if (block_to_get == 1){
                                memcpy(msg + seqNsize,block1 + (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size), dataSize );
                            }else if (block_to_get == 2){
                                memcpy(msg + seqNsize,block2 + (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size), dataSize );
                            }else{
                                perror("ERROR IN MEMCPY FROM DUP (<lastSeq) \n");
                                return -1;
                            }

                            //The segment is on two different blocks
                            if (!all_in_block){
                                sizeToGet = dataSize - (block_size - (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size));
                                if (block_to_get == 2){
                                    if (block1_is != end_in_block){
                                        fread(block1,1,block_size,fich);
                                        block1_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (dataSize - sizeToGet) ,block1, sizeToGet);
                                }else if (block_to_get == 1){
                                    if (block2_is != end_in_block){
                                        fread(block2,1,block_size,fich);
                                        block2_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (dataSize - sizeToGet),block2, sizeToGet );
                                }else{
                                    perror("ERROR IN MEMCPY FROM DUP (<lastSeq)\n");
                                    return -1;
                                }
                            }                            
                            sent = sendto(sock, (char*) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            printf("SEG_%i SENT \n",lastTransmittedSeqN+1);

                        //The segment which we want to send is not the last one
                        }else{
                            //Where does the segment start ?
                            if (block_to_get == 1){
                                memcpy(msg + seqNsize,block1 + (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size), lastMsgSize );
                            }else if (block_to_get == 2){
                                memcpy(msg + seqNsize,block2 + (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size), lastMsgSize );
                            }else{
                                perror("ERROR IN MEMCPY DUP\n");
                                return -1;
                            }

                            //The segment is on two different blocks
                            if (!all_in_block){
                                sizeToGet = lastMsgSize - (block_size - (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size));
                                if (block_to_get == 2){
                                    if (block1_is != end_in_block){
                                        fread(block1,1,block_size,fich);
                                        block1_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (lastMsgSize - sizeToGet) ,block1, sizeToGet);
                                }else if (block_to_get == 1){
                                    if (block2_is != end_in_block){
                                        fread(block2,1,block_size,fich);
                                        block2_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (lastMsgSize - sizeToGet),block2, sizeToGet );
                                }else{
                                    perror("ERROR IN MEMCPY FROM DUP (=lastSeq)\n");
                                    return -1;
                                }
                            }                            
                            sent = sendto(sock, (char*) msg,  lastMsgSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            printf("SEG_%i SENT \n",lastTransmittedSeqN+1);
                        }

                        if (sent > -1){
                            computeSRTT = 1;
                            flightSize ++;
                            int diff = lastTransmittedSeqN+1 - lastSent; //avant on avait mis 'lastDupack - lastSent
                            if (diff > 0){
                                lastSent += diff; //not sure
                            }
                        }
                        
                        sstresh = ceilf(flightSize/2);
                        window = min_window;
                        dupAck = 0;
                    
                    } 
                        segSent = 0;
                    //#NOT YET CONSIDERED AS A LOST SEGMENT -> KEEP SENDING
                        while (flightSize < floor(window) && lastSent < lastSeqN - 1){
                            msg[0] = '\0';
                            intToSeqN(lastSent + 1, currentSeqN);
                            strncat(msg, currentSeqN, seqNsize);

                            //Find in which block the begining of the segment is
                            starts_in_block = ((lastSent + 1 - initAck)*dataSize)/block_size;

                            //Find in which block the end of the segment is ...
                            if (lastSent + 1 == lastSeqN){
                                end_in_block = (((lastSent + 1 - initAck)*dataSize) + lastMsgSize)/block_size;
                            }else{
                                end_in_block = ((lastSent + 2 - initAck)*dataSize)/block_size;
                            }     
                            //..and in which block we have to start to take data                       
                            if (block1_is == starts_in_block){
                                block_to_get = 1;
                            }else if (block2_is == starts_in_block){
                                block_to_get = 2;
                            }else{
                                if (block1_is > block2_is){
                                    fread(block2,1,block_size,fich);
                                    block2_is += 2;
                                    block_to_get = 2;
                                }else{
                                    fread(block1,1,block_size,fich);
                                    block1_is += 2;
                                    block_to_get = 1;            
                                }
                            }

                            //Is the segment we have to send in one block or in two blocks ?
                            all_in_block = (starts_in_block == end_in_block);

                            //We send the last segment, the data is smaller
                            if( lastSent +1 < lastSeqN ){
                                if (block_to_get == 1){
                                    memcpy(msg + seqNsize,block1 + (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size), dataSize );
                                }else if (block_to_get == 2){
                                    memcpy(msg + seqNsize,block2 + (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size), dataSize );
                                }else{
                                    perror("ERROR IN MEMCPY FROM <DUP (<lastSeq) \n");
                                    return -1;
                                }

                            //The segment is on two different blocks
                            if (!all_in_block){
                                sizeToGet = dataSize - (block_size - (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size)); //size of the data in the next block
                                if (block_to_get == 2){
                                    if (block1_is != end_in_block){
                                        fread(block1,1,block_size,fich);
                                        block1_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (dataSize - sizeToGet) ,block1, sizeToGet);
                                }else if (block_to_get == 1){
                                    if (block2_is != end_in_block){
                                        fread(block2,1,block_size,fich);
                                        block2_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (dataSize - sizeToGet),block2, sizeToGet );
                                }else{
                                    perror("ERROR IN MEMCPY FROM <DUP (<lastSeq)\n");
                                    return -1;
                                }
                            }
                            sent = sendto(sock, (char*) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr*)&client, clientLen);
                            printf("SEG_%i SENT from <dupack\n",lastSent + 1);
                       
                       //The segment which we want to send is not the last one
                       }else{
                           //Where does the segment start ?
                           if (block_to_get == 1){
                                memcpy(msg + seqNsize,block1 + (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size), lastMsgSize );
                            }else if (block_to_get == 2){
                                memcpy(msg + seqNsize,block2 + (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size), lastMsgSize );
                            }else{
                                printf("ERROR IN MEMCPY FROM <DUPACK\n");
                                return -1;
                            }

                            //The segment is on two different blocks
                            if (!all_in_block){
                                sizeToGet = lastMsgSize - (block_size - (((lastSent + 1 - initAck)*dataSize) - starts_in_block*block_size)); //size of the data in the next block
                                if (block_to_get == 2){
                                    if (block1_is != end_in_block){
                                        fread(block1,1,block_size,fich);
                                        block1_is += 2;
                                    }
                                    memcpy(msg + seqNsize + (lastMsgSize - sizeToGet) ,block1, sizeToGet);
                                }else if (block_to_get == 1){
                                    if (block2_is != end_in_block){
                                        fread(block2,1,block_size,fich);
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

                            if (sent > -1){
                                computeSRTT = 1;
                                flightSize ++;
                                lastSent += 1;
                            }
                            segSent++;
                        }
                    
                }else{
                    printf("Received an inferior ack -> ignored \n");
                    ignored_nb ++;
                    //******congestion avoidance ou slowstart
                     //window = window < sstresh ? window*2 : window + 1;
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

            //Find in which block the end of the segment is ...
            if (lastTransmittedSeqN + 1 == lastSeqN){
                end_in_block = (((lastTransmittedSeqN + 1 - initAck)*dataSize) + lastMsgSize)/block_size;
            }else{
                end_in_block = ((lastTransmittedSeqN + 2 - initAck)*dataSize)/block_size;
            }
            //..and in which block we have to start to take data
            if (block1_is == starts_in_block){
                block_to_get = 1;
            }else if (block2_is == starts_in_block){
                block_to_get = 2;
            }else{
                if (block1_is > block2_is){
                    fread(block2,1,block_size,fich);
                    block2_is += 2;
                    block_to_get = 2;
                }else{
                    fread(block1,1,block_size,fich);
                    block1_is += 2;
                    block_to_get = 1;            
                }
            }

            //Is the segment we have to send in one block or in two blocks ?
            all_in_block = (starts_in_block == end_in_block);

            //We send the last segment, the data is smaller, let's check in wich block(s) the segment is 
            if(lastTransmittedSeqN >= lastSeqN - 1){
                //Where does the segment start ?
                if (block_to_get == 1){
                    memcpy(msg + seqNsize,block1 + (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size), lastMsgSize );
                }else if (block_to_get == 2){
                    memcpy(msg + seqNsize,block2 + (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size), lastMsgSize );
                }else{
                    printf("ERROR IN MEMCPY FROM DUP (<lastSeq) \n");
                    return -1;
                }
        
                //The segment is on two different block
                if (!all_in_block){
                    sizeToGet = lastMsgSize - (block_size - (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size)); //size of the data in the next block
                    if (block_to_get == 2){
                        if (block1_is != end_in_block){
                            fread(block1,1,block_size,fich);
                            block1_is += 2;
                        }
                        memcpy(msg + seqNsize + (lastMsgSize - sizeToGet) ,block1, sizeToGet);
                    }else if (block_to_get == 1){
                        if (block2_is != end_in_block){
                            fread(block2,1,block_size,fich);
                            block2_is += 2;
                        }
                        memcpy(msg + seqNsize + (lastMsgSize - sizeToGet),block2, sizeToGet );
                    }else{
                        printf("ERROR IN MEMCPY FROM DUP (<lastSeq)\n");
                        return -1;
                    }
                }
                sent = sendto(sock, (char *) msg,  lastMsgSize + seqNsize, MSG_CONFIRM, (struct sockaddr *)&client, clientLen);

            //The segment which we want to send is not the last one, let's check in wich block(s) the segment is   
            }else{

                if (block_to_get == 1){
                    memcpy(msg + seqNsize,block1 + (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size), dataSize );
                }else if (block_to_get == 2){
                    memcpy(msg + seqNsize,block2 + (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size), dataSize );
                }else{
                    printf("ERROR IN MEMCPY DUP\n");
                    return -1;
                }

                //The segment is on two different blocks
                if (!all_in_block){
                    sizeToGet = dataSize - (block_size - (((lastTransmittedSeqN + 1 - initAck)*dataSize) - starts_in_block*block_size)); //size of the data in the next block
                    if (block_to_get == 2){
                        if (block1_is != end_in_block){
                            fread(block1,1,block_size,fich);
                            block1_is += 2;
                        }
                        memcpy(msg + seqNsize + (dataSize - sizeToGet) ,block1, sizeToGet);
                    }else if (block_to_get == 1){
                        if (block2_is != end_in_block){
                            fread(block2,1,block_size,fich);
                            block2_is += 2;
                        }
                        memcpy(msg + seqNsize + (dataSize - sizeToGet),block2, sizeToGet );
                    }else{
                        printf("ERROR IN MEMCPY FROM DUP (=lastSeq)\n");
                        return -1;
                    }
                }
                
                sent = sendto(sock, (char *) msg,  dataSize + seqNsize, MSG_CONFIRM, (struct sockaddr *)&client, clientLen);
                printf("SEG_%i SENT from timeout with rtt = %lds and %ldus\n",lastTransmittedSeqN+1, srtt_sec,srtt_usec);
            }
            
            timeout.tv_sec = 0;
            timeout.tv_usec = srtt_usec*1.3;
            //window = min_window;
            //sstresh = ceilf(flightSize/2);
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
