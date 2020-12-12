int synchro(int sock, struct sockaddr_in client, int port);
void intToSeqN(int x, char* res);
char **splitData(char* src, int fragSize);
int readAndSendFile(int sock, struct sockaddr_in client, char* filename, int dataSize, int seqNsize, int initAck);
int seqNToInt(char *seqNumber);
int seqNToInt(char *seqNumber);

struct model_elem{
        int seqN;
        long us;
        struct model_elem *suivant;
};

typedef struct model_elem ELEMLIST;

typedef ELEMLIST *LISTE;

void insertionListeTriee(LISTE *pliste, int ackN, long us_time);
long suppFirstOcc(LISTE *pliste, int ackN);
void printListe2(LISTE l); 