#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_IP "192.168.2.118"
#define SERVER_PORT 6000

int main(){
	int clientSocket;
	struct sockaddr_in serverAddress;
	int status, bytesRcv;
	char inStr[80]; //Stores user input from keyboard
	char buffer[80]; //Stores user input from keyboard

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
    status = connect(clientSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
    if(status < 0){
    	printf("*** CLIENT ERROR: Could not connect.\n");
    	exit(-1);
    }

    // Go into loop to communicate with server now
    while(1){
    	//Get a command from the user
    	printf("CLIENT: Enter command to send to server...");
    	scanf("%s", inStr);

    	// Send command string to server
    	strcpy(buffer, inStr);
    	printf("CLIENT: Sending \"%s\" to server.\n",buffer);
    	send(clientSocket, buffer, strlen(buffer), 0);

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
