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

#define SEQUENCELEN 4

char *ackx(int x){
	char *res = (char*) malloc (15);
	strncpy(res, "000000000000", SEQUENCELEN);
	res[SEQUENCELEN] = '\0';
	for (int i=SEQUENCELEN - 1; i >= 0; i--){
		if (x >= pow(10, i)){
                    snprintf(res + SEQUENCELEN - i - 1, sizeof(res) - SEQUENCELEN + i + 1,"%d",x);
                    break;
		}
	}
	return res;
}
int main(int arc, char *argv){
	int res = 3/2;
	printf("res = %i\n", res);
	/*
	char xstr[SEQUENCELEN + 1];
	fgets(xstr, SEQUENCELEN + 10,stdin);
	while (strcmp(xstr, "done\n") != 0){
		printf("ABOUT TO CONVERT\n");
		printf("%s\n", ackx(atoi(xstr)));
		fgets(xstr, SEQUENCELEN +10, stdin);
	}
	printf("%s\n",xstr+1);

	char* pluscourt = NULL;
	pluscourt = (char*) malloc(2);
	strncpy(pluscourt, xstr, 2);
	printf("PLUS COURT : %s\n", pluscourt);
	printf("%i %i %i \n", atoi("0023"), atoi("0345"), atoi("3456"));
	char buff[1024];
	FILE* fich = fopen("birds.jpg","r");
	if (fich){
		fscanf(fich, "%s", buff);
		printf("%s\n", buff);
	}else{
		printf("CANT OPEN\n");
	}
	fclose(fich);
	//printf("STRCMP : %i\n", strcmp("END_rfc793.pdf", 162));
	char *string = (char*) malloc(15);
	memcpy(string, "prout", 5);
	memcpy(string + 5, "prout",5);
	printf("string : %s\n",string);
*/
	printf("INT : %i\n", atoi("0053"));
	return 1;
}
