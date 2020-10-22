int synchro(int sock, struct sockaddr_in client, int port);
void intToSeqN(int x, char* res);
char **splitData(char* src, int fragSize);
int readAndSendFile(int sock, struct sockaddr_in client, char* filename, int dataSize, int seqNsize, int initAck);
int seqNToInt(char *seqNumber);