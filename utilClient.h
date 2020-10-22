int synchro(int sock, struct sockaddr_in server);
char* askForFile(int sock, struct sockaddr_in server, char* filename);
int seqNToInt(char *seqNumber);
void intToSeqN(int x, char* res);