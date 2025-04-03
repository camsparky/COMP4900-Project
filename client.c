#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "ballot.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 6000

void readCSVFile(char* fileName, struct Ballot* ballots){
	FILE *file;

	file = fopen(fileName, "r");

	if(file == NULL){
		printf("Error opening file.\n");
		return;
	}

	int read = 0;
	int records = 0;
	 while(!feof(file)){
		read =  fscanf(file, "%d,%d", &ballots[records].voterId, &ballots[records].candidate);
		if(read == 2) records++;

		if(read != 2 && !feof(file)){
			printf("File Format incorrect.\n");
			return;
		}

		if(ferror(file)){
			printf("Error Reading File.\n");
			return;
		}
	}

	fclose(file);


//
//	printf("\n %d records read. \n\n", records);
//
//	for (int i = 0; i<records; i++){
//		printf("%d, %d\n",  ballots[i].voterId, ballots[i].candidate);
//	}
//	printf("\n");
//
//
//	return 0;
}

int main(){
	int clientSocket;
	struct sockaddr_in serverAddress;
	int status, bytesRcv;
	char inStr[80]; //Stores user input from keyboard
	char buffer[80]; //Stores user input from keyboard
	char fileName[80]; //Stores file name from user input
	char* sendValues;


	struct Ballot ballots[50];
	int numRecords = 50;
	int dataSize = numRecords * sizeof(struct Ballot);


	// Create the client socket
	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if(clientSocket < 0){
		printf("*** CLIENT ERROR: Could not open socket.\n");
		exit(-1);
	}


	//Setup Address
	memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr(SERVER_IP);
    serverAddress.sin_port = htons((unsigned short) SERVER_PORT);

    //Connect to server
    printf("Connecting.... \n");
    status = connect(clientSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
    if(status < 0){
    	printf("*** CLIENT ERROR: Could not connect.\n");
    	exit(-1);
    }
    printf("After.... \n");

    // Go into loop to communicate with server now
    while(1){

    	//Get a command from the user
    	printf("CLIENT: Enter command to send to server... \n");

    	scanf("%s %s", inStr, fileName);
    	if(strcmp(inStr, "send") == 0){
    		printf("CLIENT: Reading and Sending polling results \n");
			readCSVFile(fileName, &ballots);
			printf("CLIENT: Results have been read \n");
			ssize_t bytesSent = send(clientSocket, ballots, dataSize, 0);
			if (bytesSent == -1) {
				perror("Send failed");
				exit(1);
			} else if (bytesSent < dataSize) {
				printf("Warning: Only %ld bytes sent out of %d\n", bytesSent, dataSize);
			}
    	}else{
    		// Send command string to server
    		strcpy(buffer, inStr);
    		printf("CLIENT: Sending \"%s\" to server.\n",buffer);
    		send(clientSocket, buffer, strlen(buffer), 0);
    	}

    	//Get Response from server, should be "OK"
    	bytesRcv = recv(clientSocket, buffer, 80, 0);
    	buffer[bytesRcv] = 0;
    	printf("CLIENT: Got back response \"%s\" from server.\n",buffer);

    	if((strcmp(inStr, "done") ==0 ) || (strcmp(inStr,"stop") ==0))
    		break;
    }
    close(clientSocket);
    printf("CLIENT: Shutting down.\n");


}


