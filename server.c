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
#define BLOCK_SIZE 16
#define KEY { 0x42, 0x49, 0x47, 0x20, 0x43, 0x48, 0x55, 0x4E, 0x47, 0x55, 0x53, 0x4D, 0x45, 0x4D, 0x45, 0x0f}
#define IV {0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x06}

struct Ballot* decryptData(uint8_t* ctext_ptr){
	//Setup Symmetric Cryptography
	int ret;
	qcrypto_ctx_t *qctx = NULL;
	qcrypto_ctx_t *qkeyctx = NULL;
	qcrypto_key_t *qkey = NULL;

	//Setting key and IV values
	const uint8_t key[] = KEY;
	uint8_t ivVal[] = IV;
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

int main() {
    int serverSocket, clientSocket;
	struct sockaddr_in serverAddress, clientAddr;
    int status, addrSize, bytesRcv;
    char buffer[30];
    char *response = "OK";
	uint8_t ctext_cmp[BLOCK_SIZE];

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

		// Get the message from the client
		bytesRcv = recv(clientSocket, (uint8_t*)ctext_cmp, BLOCK_SIZE, 0);
		struct Ballot *test = decryptData(ctext_cmp);
		printf("%s",test->voterId);
		exit(0);

		printf("SERVER: Sending \"%s\" to client\n", response);
		send(clientSocket, response, strlen(response),0);

    	close(clientSocket);
    }

    close(serverSocket);
    printf("SERVER: Shutting down.\n");

    return 0;
}
