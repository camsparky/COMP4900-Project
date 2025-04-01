#include <netinet/in.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define SERVER_PORT 6000

int main() {
    int serverSocket, clientSocket;
	struct sockaddr_in serverAddress, clientAddr;
    int status, addrSize, bytesRcv;
    char buffer[30];
    char *response = "OK";

    //Create the server socket
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(serverSocket<0){
    	printf("*** SERVER ERROR: Could not open socket.\n");
    	exit(-1);
    }

    //Setup the server address
    memset(&serverAddress, 0, sizeof(serverAddress)); //Zeros the struct
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons((unsigned short) SERVER_PORT);

    // Bind the server socket
    status = bind(serverSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));

    if(status < 0){
    	printf("*** SERVER ERROR: Could not bind socket.\n");
    	exit(-1);
    }

    //Set up the line-up to handle up to 5 clients in line
    status = listen(serverSocket, 5);
    if(status < 0){
    	printf("*** SERVER ERROR: Could not listen on socket.\n");
    	exit(-1);
    }

    while(1){
    	addrSize = sizeof(clientAddr);
    	clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddr, &addrSize);
    	if(clientSocket < 0){
    		printf("*** SERVER ERROR: Could not accept incoming client connection.\n");
    		exit(-1);
    	}
    	printf("SERVER: Received client connection.\n");

    	//Go into infinite loop to talk to client
    	while(1){
    		// Get the message from the client
    		bytesRcv = recv(clientSocket, buffer, sizeof(buffer), 0);
    		buffer[bytesRcv] = 0;
    		printf("SERVER: Received client request: %s\n",buffer);

    		// Respond with an "OK" Message
    		printf("SERVER: Sending \"%s\" to client\n", response);
    		send(clientSocket, response, strlen(response),0);
    		if((strcmp(buffer, "done") ==0 ) || (strcmp(buffer,"stop") ==0))
    			break;
    	}

    	// If the client said to stop, then stop the server socket
    	printf("SERVER: Closing client connection. \n");
    	close(clientSocket);

    	if (strcmp(buffer, "stop")==0)
    		break;
    }

    close(serverSocket);
    printf("SERVER: Shutting down.\n");

    return 0;
}
