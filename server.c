#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
#include <sched.h>
#include <qcrypto/qcrypto.h>
#include <qcrypto/qcrypto_error.h>
#include <qcrypto/qcrypto_keys.h>
#include "ballot.h"
#define SERVER_PORT 6000
#define NUMTHREADS 2
#define PRIORITY0 4
#define PRIORITY1 3
#define BLOCK_SIZE 16
#define BATCH_SIZE 50

struct Ballot* decryptData(uint8_t* ctext_ptr){
	//Setup Symmetric Cryptography
	int ret;
	qcrypto_ctx_t *qctx = NULL;
	qcrypto_ctx_t *qkeyctx = NULL;
	qcrypto_key_t *qkey = NULL;

	//Setting key and IV values
	const uint8_t key[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
	const uint8_t ivVal[] = {0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x06};
	const size_t keysize = sizeof(key);

	//Setting decrypt plaintext pointer
	uint8_t ptext_cmp[BLOCK_SIZE];
	uint8_t *ptext_ptr = ptext_cmp;
	size_t psize_cmp = 0;

	/* Initialize the Qcrypto Library */
	ret = qcrypto_init(QCRYPTO_INIT_LAZY, NULL);
	if (ret != QCRYPTO_R_EOK) {
		fprintf(stderr, "qcryto_init() failed (%d:%s)\n", ret, qcrypto_strerror(ret));
		exit(-1);
	}

	/* Request symmetric keygen */
	ret = qcrypto_keygen_request("symmetric", NULL, 0, &qkeyctx);
	if (ret != QCRYPTO_R_EOK) {
	   fprintf(stderr, "qcrypto_keygen_request() failed (%d:%s)\n", ret, qcrypto_strerror(ret));
	   exit(-1);
	}

	/* Initialize cipher arguments */
	qcrypto_cipher_args_t cargs = {
		.action = QCRYPTO_CIPHER_DECRYPT,
		.iv = ivVal,
		.ivsize = 16,
		.padding = QCRYPTO_CIPHER_PADDING_PKCS7
	};

	/* Request aes-128-cbc */
	ret = qcrypto_cipher_request("aes-128-cbc", NULL, 0, &qctx);
	if (ret != QCRYPTO_R_EOK) {
		fprintf(stderr, "qcrypto_cipher_request() failed (%d:%s)\n", ret, qcrypto_strerror(ret));
		exit(-1);
	}

	/* Load key */
	ret = qcrypto_key_from_mem(qkeyctx, &qkey, key, keysize);
	if (ret != QCRYPTO_R_EOK) {
		fprintf(stderr, "qcrypto_key_from_mem() failed (%d:%s)\n", ret, qcrypto_strerror(ret));
		exit(-1);
	}

	/* Initialize cipher decryption */
	ret = qcrypto_cipher_init(qctx, qkey, &cargs);
	if (ret != QCRYPTO_R_EOK) {
		fprintf(stderr, "qcrypto_cipher_init() failed (%d:%s)\n", ret, qcrypto_strerror(ret));
		exit(-1);
	}

	/* Cipher decryption */
	ret = qcrypto_cipher_decrypt(qctx, ctext_ptr, BLOCK_SIZE, ptext_ptr, &psize_cmp);
	if (ret != QCRYPTO_R_EOK) {
		fprintf(stderr, "qcrypto_cipher_decrypt() failed (%d:%s)\n", ret, qcrypto_strerror(ret));
		exit(-1);
	}

	/* Finalize cipher decryption */
	ret = qcrypto_cipher_final(qctx, ptext_ptr, &psize_cmp);
	if (ret != QCRYPTO_R_EOK) {
		fprintf(stderr, "qcrypto_cipher_final() failed (%d:%s)\n", ret, qcrypto_strerror(ret));
		exit(-1);
	}

	struct Ballot *newBallot = (struct Ballot*) malloc(sizeof(struct Ballot));
	memcpy(newBallot, ptext_cmp, sizeof(struct Ballot));
	return newBallot;
}

void* writeAudit(void *args){
	struct LinkedList *list = (struct LinkedList*) args;
	while(1){
		FILE *fptr;
		fptr = fopen("auditServer.txt", "w");
		struct node *node = list->head;
		while(node->ballot){
			fprintf(fptr,"%s,%d\n", node->ballot->voterId, node->ballot->candidate);
			node = node->next;
		}
		fclose(fptr);
		sleep(5);
	}
}

void* printVotes(void *args){
	struct LinkedList *masterList = (struct LinkedList*) args;
	while(1){
		struct node *curNode = masterList->head;
		unsigned int totalVotesParty1 = 0;
		unsigned int totalVotesParty2 = 0;
		while(curNode->ballot){
			if(curNode->ballot->candidate==0){
				totalVotesParty1+=1;
			}
			else{
				totalVotesParty2+=1;
			}
			curNode = curNode->next;
		}
		printf("-----CURRENT RESULTS-----\n");
		printf("Party 1 Count: %d\n", totalVotesParty1);
		printf("Party 2 Count: %d\n", totalVotesParty2);
		printf("-------------------------\n");
		fflush(stdout);
		sleep(5);
	}
}

int main() {
	//Server Information related variables
    int serverSocket, clientSocket;
	struct sockaddr_in serverAddress, clientAddr;
    int status, addrSize;
    char *response = "OK";

    //Cryptography related variables
	uint8_t ctext_cmp[BLOCK_SIZE];
	uint8_t *ctext_pointer = ctext_cmp;

	//Thread Related Variables
	pthread_t threads[NUMTHREADS];
    pthread_attr_t attribs[NUMTHREADS];
    struct sched_param params[NUMTHREADS];
    params[0].sched_priority = PRIORITY0;
    params[1].sched_priority = PRIORITY1;

	//Data Structure holding voter data
	struct LinkedList *list = (struct LikedList*) malloc(sizeof(struct LinkedList));
	struct node *headNode = (struct node*) malloc(sizeof(struct node));
	list->head = headNode;
	list->tail = headNode;
	struct node *curNode = list->head;
	list->total=0;

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

    //Initializing Thread Attributes
    int i = 0;
    for (i = 0; i < NUMTHREADS; i++){ //Setting scheduling policy to be Round-robin for each thread
            pthread_attr_init(&attribs[i]);
            pthread_attr_setdetachstate(&attribs[i],PTHREAD_CREATE_DETACHED); // Don't need to wait on the thread returning because it should run forever until votes stop
            pthread_attr_setinheritsched(&attribs[i], PTHREAD_EXPLICIT_SCHED);
            pthread_attr_setschedpolicy(&attribs[i], SCHED_RR); //Using round robin scheduling
    }

    //Creating threads
    pthread_attr_setschedparam(&attribs[0], &params[0]);
    pthread_attr_setschedparam(&attribs[1], &params[1]);
    pthread_create(&threads[0], &attribs[0], printVotes, (void *)list); //Voting Machine Thread
    pthread_create(&threads[1], &attribs[1], writeAudit, (void *)list); //Printing Votes Thread

    while(1){
    	addrSize = sizeof(clientAddr);
    	clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddr, &addrSize);
    	if(clientSocket < 0){
    		printf("*** SERVER ERROR: Could not accept incoming client connection.\n");
    		exit(-1);
    	}
    	printf("SERVER: Received client connection.\n");

		// Get the message from the client
    	for(int i=0; i<BATCH_SIZE; i++){
    		recv(clientSocket, (uint8_t*)ctext_pointer, BLOCK_SIZE, 0);
    		struct Ballot *plainData = decryptData(ctext_cmp);
    		send(clientSocket, response, strlen(response),0);
    		struct node *newNode = (struct node*) malloc(sizeof(struct node));
			curNode->ballot = plainData;
			curNode->next = newNode;
			curNode = curNode->next;
			list->total+=1;
    	}

    	close(clientSocket);
    }

    close(serverSocket);
    printf("SERVER: Shutting down.\n");

    return 0;
}
