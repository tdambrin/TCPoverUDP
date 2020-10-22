int synchro(int sock, struct sockaddr_in client, int port);
char *ackx(int x);
char **splitData(char* src, int fragSize);
int readAndSendFile(int sock, struct sockaddr_in client, char* filename, int dataSize, int seqNsize, int initAck);

