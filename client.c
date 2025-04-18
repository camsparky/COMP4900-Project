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

#define NUMTHREADS 2
#define PRIORITY0 4
#define PRIORITY1 3
#define BATCH_SIZE 50
#define BLOCK_SIZE 16

void writeAudit(struct LinkedList *list){
	FILE *fptr;
	fptr = fopen("audit.txt", "w");
	struct node *node = list->head;
	while(node){
		fprintf(fptr,"%s,%d\n", node->ballot->voterId, node->ballot->candidate);
		node = node->next;
	}
	fclose(fptr);
}

char generate_random(int l, int r) { //this will generate random number in range l and r
      char rand_num = (rand() % (r - l + 1)) + l;
      return rand_num;
}

void insertatend(struct LinkedList *list, struct Ballot *voteData){
	struct node *end = (struct node*) malloc(sizeof(struct node));
	end->ballot = voteData;

	if(list->tail){
		list->tail->next = end;
		list->tail = end;
	}
	else{
		list->head = end;
		list->tail = end;
	}
	list->total += 1;
}

char* getNewId(unsigned int curId, char A1, char A2){
	char* newId = (char*) malloc(11 * sizeof(char));
	int length = (int)((floor(log10(curId))+1)*sizeof(char));
	sprintf(newId+((10*sizeof(char)) - (length*sizeof(char))), "%d", curId);

	int i;
	for(i=(10-(length+1)); i>1; i--){
		newId[i] = '0';
	}
	newId[0] = A1;
	newId[1] = A2;
	newId[10] = 0;
	return newId;
}

void* printVotes(void *args){
	struct LinkedList *masterList = (struct LinkedList*) args;
	while(1){
		struct node *curNode = masterList->head;
		unsigned int totalVotesParty1 = 0;
		unsigned int totalVotesParty2 = 0;
		while(curNode){
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

void* votingMachine(void *args){
	struct LinkedList *masterList = (struct LinkedList*) args;
	unsigned int curVoteId = 1;
	char voteCode[2] = {generate_random(65,90), generate_random(65,90)};
	while(1){
		int curTotal=0;
		while(curTotal < BATCH_SIZE){
			char vote = generate_random(0,1);
			char* newId = getNewId(curVoteId, voteCode[0], voteCode[1]);
			struct Ballot *newBallot = (struct Ballot*) malloc(sizeof(struct Ballot));
			memcpy(newBallot->voterId, newId, sizeof(newBallot->voterId));
			newBallot->candidate = vote;
			insertatend(masterList, newBallot);
			curVoteId+=1;
			curTotal+=1;
		}
		writeAudit(masterList);
		sleep(5);
	}
}

uint8_t* encryptData(struct Ballot *balInfo){
	//Setup Symmetric Cryptography
	int ret;
	qcrypto_ctx_t *qctx = NULL;
	qcrypto_ctx_t *qkeyctx = NULL;
	qcrypto_key_t *qkey = NULL;

	//Setting up key and IV
	const uint8_t key[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
	const uint8_t ivVal[] = {0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x06 };
	const size_t keysize = sizeof(key);

	//Setting up plaintext to encrypt (Our data)
	uint8_t ptext[sizeof(struct Ballot)];
	memcpy(ptext, balInfo, sizeof(struct Ballot));
	const size_t psize = sizeof(ptext);

	//Setting up ciphertext pointer
	uint8_t ctext_cmp[BLOCK_SIZE];
	uint8_t *ctext_ptr = ctext_cmp;
	size_t csize_cmp = 0;

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
		.action = QCRYPTO_CIPHER_ENCRYPT,
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

	/* Initialize cipher encryption */
	ret = qcrypto_cipher_init(qctx, qkey, &cargs);
	if (ret != QCRYPTO_R_EOK) {
		fprintf(stderr, "qcrypto_cipher_init() failed (%d:%s)\n", ret, qcrypto_strerror(ret));
		exit(-1);
	}

	/* Cipher encryption */
	ret = qcrypto_cipher_encrypt(qctx, ptext, psize, ctext_ptr, &csize_cmp);
	if (ret != QCRYPTO_R_EOK) {
		fprintf(stderr, "qcrypto_cipher_encrypt() failed (%d:%s)\n", ret, qcrypto_strerror(ret));
		exit(-1);
	}

	/* Finalize cipher encryption */
	ret = qcrypto_cipher_final(qctx, ctext_ptr, &csize_cmp);
	if (ret != QCRYPTO_R_EOK) {
		fprintf(stderr, "qcrypto_cipher_final() failed (%d:%s)\n", ret, qcrypto_strerror(ret));
		exit(-1);
	}

	return ctext_ptr;
}


int main(int argc, char **argv)
{
	//Defining Variables
	//Server Connection Related Variables
    srand(time(0)); //Initializing RNG
	char* serverIp;
	int serverPort;
	int clientSocket;
	struct sockaddr_in serverAddress;
	char buffer[80];
	int status;

	//Thread Related Variables
	pthread_t threads[NUMTHREADS];
    pthread_attr_t attribs[NUMTHREADS];
    struct sched_param params[NUMTHREADS];
    params[0].sched_priority = PRIORITY0;
    params[1].sched_priority = PRIORITY1;


    //Data Related variables / Initializing main list
    struct LinkedList *list = (struct LinkedList*) malloc(sizeof(struct LinkedList));
    list->total=0;
    unsigned int curSentIndex=0;
    struct node* curData;


	//Checking arguments to make sure a target IP and port was supplied
    if(argc != 3){
    	printf("Supply a target IP Address and Port number to the client!");
    	exit(EXIT_FAILURE);
    }
    else{
    	serverIp = argv[1];
    	serverPort = atoi(argv[2]);
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
    pthread_create(&threads[0], &attribs[0], votingMachine, (void *)list); //Voting Machine Thread
    pthread_create(&threads[1], &attribs[1], printVotes, (void *)list); //Printing Votes Thread
    if(list->total==0){
    	sleep(1);
    	curData = list->head;
    }

    //Setup Address
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr(serverIp);
    serverAddress.sin_port = htons((unsigned short) serverPort);

    while(1){
    	if((list->total - curSentIndex)>=50){
    		//Create the client socket
			clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if(clientSocket<0){
				printf("*** CLIENT ERROR: Could not open socket");
				exit(-1);
			}
			//Connect to server
			status = connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
			if(status < 0){
				printf("*** CLIENT ERROR: Could not connect.\n");
				exit(-1);
			}
			if(curSentIndex>0){curData = curData->next;}
			for(int i=0; i<BATCH_SIZE; i++){
				uint8_t* cipherPointer = encryptData(curData->ballot);
				uint8_t cipherBlock[BLOCK_SIZE];
				memcpy(cipherBlock,cipherPointer,BLOCK_SIZE);
				send(clientSocket, cipherBlock, BLOCK_SIZE, 0);
				recv(clientSocket, buffer, 80, 0);
				if(curData->next){
					curData = curData->next;
				}
			}
			curSentIndex+=50;
			close(clientSocket);
    	}
    	sleep(5);
    }

    return EXIT_SUCCESS;


}
