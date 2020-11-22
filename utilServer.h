struct model_elem{
        int seqN;
        struct model_elem *suivant;
};

typedef struct model_elem ELEMLIST;

typedef ELEMLIST *LISTE;

int synchro(int sock, struct sockaddr_in client, int port);
void intToSeqN(int x, char* res);
char **splitData(char* src, int fragSize);
int readAndSendFile(int sock, struct sockaddr_in client, char* filename, int dataSize, int seqNsize, int initAck);
int seqNToInt(char *seqNumber);
void insertionListeTriee(LISTE *pliste, int val);
int suppHead(LISTE *pliste);
void printListe2(LISTE l);