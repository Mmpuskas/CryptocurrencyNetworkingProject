#include <sys/socket.h>  /* for socket() and bind() */
#include <stdio.h>               /* printf() and fprintf() */
#include <stdlib.h>              /* for atoi() and exit() */
#include <arpa/inet.h>   /* for sockaddr_in and inet_ntoa() */
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "dataStructs.h"

#define BACKLOG 10
#define BLOCKCHAINLENGTH 200
#define STDIN 0

int checkUniqueID(struct blockchain* currentChain, int ID)
{
	int isUnique = 1;
	for(int i = 0; i < currentChain->length; i++)
		if(currentChain->blocks[i].blockID == ID)
			isUnique = 0;

	return isUnique;
}

void broadcastProof(struct minerInfo* selfInfo, struct minerInfo* peers, int* connectionFDs, struct block* waitingTransaction)
{
	struct blockMessage message;
	message.type = 1;
	message.messageBlock = *waitingTransaction;

	// For each miner in our list, write the transaction
	for(int i = 0; i < 10; i++)
	{
		// Skip ourselves and removed peers
		if(i == selfInfo->identifier || peers[i].identifier == -1)
			continue;

		printf("Broadcasting proof to %s (%d)\n", peers[i].username, i);
		write(connectionFDs[i], &message, sizeof(struct blockMessage));
	}
}

void addBlock(struct blockchain* currentChain, struct block* waitingTransaction)
{
	// Add the new block to the chain
	currentChain->blocks[currentChain->length].blockID = waitingTransaction->blockID;
	for(int i = 0; i < 10; i++)
	{
		currentChain->blocks[currentChain->length].coinAmounts[i] = waitingTransaction->coinAmounts[i];
		currentChain->blocks[currentChain->length].timestamp[i] = waitingTransaction->timestamp[i];
	}
	currentChain->length++;

	// Print the updated block
	printf("Blockchain updated with new transaction:\n\tblockID = %d\n\tblock = [", currentChain->blocks[currentChain->length - 1].blockID);
	for(int i = 0; i < 10; i++)
		printf("%d, ", currentChain->blocks[currentChain->length - 1].coinAmounts[i]);
	printf("]\n\ttimestamp = [");
	for(int i = 0; i < 10; i++)
		printf("%d, ", currentChain->blocks[currentChain->length - 1].timestamp[i]);
	printf("]\n");
}

int validateBlock(struct blockchain* currentChain, struct block* waitingTransaction)
{
	int invalidCode = 0;

	// Get the starting totals
	int coinTotals[10];
	for(int i = 0; i < 10; i++)
		coinTotals[i] = currentChain->blocks[0].coinAmounts[i];

	// Check the existing blocks
	for(int i = 1; i < currentChain->length; i++)
	{
		// Check the timestamp at this iteration
		int isBigger = 0;
		for(int j = 0; j < 10; j++)
			if(currentChain->blocks[i - 1].timestamp[j] < currentChain->blocks[i].timestamp[j])
				isBigger = 1;

		if(!isBigger)
		{
				invalidCode = 1;
				break;
		}

		// Add the coins from this iteration
		for(int j = 0; j < 10; j++)
			coinTotals[j] += currentChain->blocks[i].coinAmounts[j];
		// Check if the coin total is valid
		for(int j = 0; j < 10; j++)
		{
			if(coinTotals[j] < 0)
			{
				invalidCode = 2;
				break;
			}
		}

		if(invalidCode != 0)
			break;
	}

	// Check against the new block
	int isBigger = 0;
	for(int i = 0; i < 10; i++)
		if(currentChain->blocks[currentChain->length - 1].timestamp[i] < waitingTransaction->timestamp[i])
			isBigger = 1;

	if(!isBigger)
		invalidCode = 1;

	// Add the coins from the new block
	for(int k = 0; k < 10; k++)
		coinTotals[k] += waitingTransaction->coinAmounts[k];
	// Check if the coin total is valid
	for(int k = 0; k < 10; k++)
	{
		if(coinTotals[k] < 0)
		{
			invalidCode = 2;
			break;
		}
	}

	return invalidCode;
}

// Example from https://stackoverflow.com/questions/5281779/c-how-to-test-easily-if-it-is-prime-number
// Returns 0 if not prime, 1 if prime
int isPrime(int num)
{
	if(num <= 1)
		return 0;

	if(num % 2 == 0 && num > 2)
		return 0;

	for(int i = 3; i < num / 2; i += 2)
	{
		if(num % i == 0)
			return 0;
	}

	return 1;
}

void printClock(int* vectorClock)
{
	printf("Current clock: [");
	for(int i = 0; i < 9; i++)
		printf("%d, ", vectorClock[i]);
	printf("%d]\n", vectorClock[9]);
}

void broadcastTransaction(struct minerInfo* selfInfo, struct minerInfo* peers, int* connectionFDs, struct block* waitingTransaction)
{
	struct blockMessage message;
	message.type = 0;
	message.messageBlock = *waitingTransaction;

	// For each miner in our list, write the transaction
	for(int i = 0; i < 10; i++)
	{
		// Skip ourselves and removed peers
		if(i == selfInfo->identifier || peers[i].identifier == -1)
			continue;

		printf("Broadcasting transaction to %s (%d)\n", peers[i].username, i);

		write(connectionFDs[i], &message, sizeof(struct blockMessage));
	}
}

void parseTransfer(char inputBuffer[10], struct minerInfo* selfInfo, int* vectorClock, struct blockchain* currentChain, struct minerInfo* peers, int* connectionFDs, struct block** waitingTransaction)
{
	int bufferIndex = 0;
	// Break off the identifier
	int inputIdentifier = inputBuffer[bufferIndex] - '0';
	bufferIndex++;

	if(3 < inputIdentifier || inputIdentifier < 0)
	{
		printf("INPUT ERROR: Identifier must be a number 0-3. Given input: %d\n", inputIdentifier);
		return;
	}
	else
	{
		// Break off the transaction amount
		char transactionBuffer[10];
		int transactionIndex = 0;
		while(inputBuffer[bufferIndex] != '\0')
		{
			transactionBuffer[transactionIndex] = inputBuffer[bufferIndex];
			bufferIndex++;
			transactionIndex++;
		}
		transactionBuffer[transactionIndex] = '\0';
		
		char* endPtr;
		int transactionAmount = strtol(transactionBuffer, &endPtr, 10);

		// Test if error or actually 0
		if(transactionAmount == 0)
		{
			if(errno == ERANGE)
			{
				printf("INPUT ERROR: Value out of range\n");
				errno = 0;
				return;
			}
			if(endPtr == &inputBuffer[0] || endPtr != '\0')
			{
				printf("INPUT ERROR: Invalid value, integers only.\n");
				return;
			}
		}

		printf("Transfer command received. Identifier = %d, Amount = %d\n", inputIdentifier, transactionAmount);

		// Setup the transaction to be broadcast and processed
		if(*waitingTransaction == NULL)
		{
			*waitingTransaction = malloc(sizeof(struct block));
			for(int i = 0; i < 10; i++)
			{
				(*waitingTransaction)->coinAmounts[i] = 0;
				(*waitingTransaction)->timestamp[i] = 0;
			}
			(*waitingTransaction)->coinAmounts[inputIdentifier] = transactionAmount;
			(*waitingTransaction)->coinAmounts[selfInfo->identifier] = -transactionAmount;
			(*waitingTransaction)->blockID = (selfInfo->identifier * 1000) + currentChain->length;
			// Increment the vector clock and set the transaction's timestamp
			vectorClock[selfInfo->identifier]++; // Event successful
			for(int i = 0; i < 10; i++)
				(*waitingTransaction)->timestamp[i] = vectorClock[i];

			broadcastTransaction(selfInfo, peers, connectionFDs, *waitingTransaction);
		}
		else
			printf("ERROR: Can't send transaction while still processing one.\n");
	}
}

void parseCommand(struct minerInfo* selfInfo, int* vectorClock, struct blockchain* currentChain, struct minerInfo* peers, int* connectionFDs, struct block** waitingTransaction)
{
	// Check what command we got
	char inputBuffer[20];
	fgets(inputBuffer, 20, stdin);

	// Break off the command
	char command[10];
	int bufferIndex = 0;
	while(inputBuffer[bufferIndex] != ' ' && inputBuffer[bufferIndex] != '\n')
	{
		command[bufferIndex] = inputBuffer[bufferIndex];
		bufferIndex++;
	}
	command[bufferIndex] = '\0';

	// Call the appropriate command function
	if(strcmp(command, "transfer") == 0)
	{
		// Get the rest of the command
		bufferIndex++;
		char restOfLine[10];
		int len = strlen(&inputBuffer[bufferIndex]);
		strncpy(restOfLine, &inputBuffer[bufferIndex], len);
		restOfLine[len] = '\0';

		parseTransfer(restOfLine, selfInfo, vectorClock, currentChain, peers, connectionFDs, waitingTransaction);
	}
	else if(strcmp(command, "clock") == 0)
	{
		printClock(vectorClock);
	}
	else
	{
		printf("INPUT ERROR: Unknown command\n");
	}
}

// Returns 0 if no value in One is less that the parallel value in Two, 1 if change
int compareVectorClocks(int* clockOne, int* clockTwo)
{
	int returnVal = 0;
	for(int i = 0; i < 10; i++)
		if(clockOne[i] < clockTwo[i])
			returnVal = 1;

	return returnVal;
}

void updateClock(int* vectorClock, int* timestamp)
{
	for(int i = 0; i < 10; i++)
		if(vectorClock[i] < timestamp[i])
			vectorClock[i] = timestamp[i];
}

int establishConnections(struct minerInfo* selfInfo, struct blockchain* currentChain, int* vectorClock, struct minerInfo* peers, int* connectionFDs)
{
	printf("\nEstablishing connections.\n");
	
	// For each miner in our list, establish a connection
	unsigned int addrLen = sizeof(struct sockaddr_in);
	for(int i = 0; i < 10; i++)
	{
		// Skip ourselves
		if(i == selfInfo->identifier || peers[i].identifier == -1)
			continue;

		// Setup the socket
		printf("\nAttempting connection to: %d\n", htons(peers[i].address.sin_port));
		connectionFDs[i] = socket(AF_INET, SOCK_STREAM, 0);

		// Try to connect
		int result = connect(connectionFDs[i], (struct sockaddr *) &(peers[i].address), addrLen);
		if(result == -1)
			printf("Errno = %s\n", strerror(errno));
		else
		{
			// On a successful connection, send your info
			vectorClock[selfInfo->identifier]++; // Event successful
			printf("Connection successful. Sending info.\n");
			write(connectionFDs[i], selfInfo, sizeof(struct minerInfo));
			printf("Info sent.\n");

			// Wait for the blockchain response
			printf("Waiting for blockchain response.\n");
			struct blockchain tempBlockchain;
			ssize_t n = read(connectionFDs[i], &tempBlockchain, sizeof(struct blockchain));
			if(n > 0)
			{
				printf("Blockchain response received. Newest block: [");
				for(int i = 0; i < 10; i++)
					printf("%d, ", tempBlockchain.blocks[tempBlockchain.length - 1].coinAmounts[i]);
				printf("]\n");
			}
			else
				printf("Error in receiving blockchain response\n");

			if(n > 0)
			{
				updateClock(vectorClock, tempBlockchain.blocks[tempBlockchain.length - 1].timestamp);
				vectorClock[selfInfo->identifier]++; // Event successful
				printf("Timestamp received. Updated vectorClock [");
				for(int i = 0; i < 10; i++)
					printf("%d, ", vectorClock[i]);
				printf("]\n");
			}
			else
				printf("Error in receiving blockchain timestamp\n");

			// Replace our blockchain with the response if necessary
			int bClockChanged = 0;
			compareVectorClocks(currentChain->blocks[currentChain->length - 1].timestamp, tempBlockchain.blocks[tempBlockchain.length - 1].timestamp);
			if(tempBlockchain.length > currentChain->length || bClockChanged == 1)
			{
				printf("Found longer blockchain or later clock. Replacing blockchain.\n");
				for(int i = 0; i < BLOCKCHAINLENGTH; i++)
					currentChain->blocks[i] = tempBlockchain.blocks[i];
				currentChain->length = tempBlockchain.length;
			}
		}
	}

	return 1;
}

int connectToClient(int* socketFD, struct minerInfo* selfInfo, int* vectorClock, struct blockchain* currentChain, struct minerInfo* peers, int* connectionFDs)
{
	printf("\nNew connection found. Attempting to connect.\n");
	unsigned int addrLen = sizeof(struct sockaddr_in);
	struct sockaddr_in connectedAddress;

	// Accept the connection
	int tempFD = accept(*socketFD, (struct sockaddr *) &connectedAddress, &addrLen);
	char clientName[20];
	printf("Connected to: %s:%d\n", inet_ntop(AF_INET, &connectedAddress.sin_addr, clientName, sizeof(clientName)), htons(connectedAddress.sin_port));
	vectorClock[selfInfo->identifier]++; // Event successful
	
	// Read the new client's data
	struct minerInfo newClient;
	ssize_t n = read(tempFD, &newClient, sizeof(struct minerInfo));

	if(n > 0)
	{
		// Save the new client's info
		connectionFDs[newClient.identifier] = tempFD;
		peers[newClient.identifier] = newClient;
		printf("Information received. Identifier = %d, Username = %s, Coins = %d\n"
				, newClient.identifier, newClient.username, newClient.initialCoins);

		// Doing an event, update the clock
		vectorClock[selfInfo->identifier]++; // Event Successful
		vectorClock[newClient.identifier]++; // Sync client clock

		// Add new block with new miner's initial coins
		currentChain->blocks[currentChain->length].coinAmounts[newClient.identifier] = newClient.initialCoins;
		currentChain->blocks[currentChain->length].blockID = (selfInfo->identifier * 1000) + currentChain->length;
		for(int i = 0; i < 10; i++)
			currentChain->blocks[currentChain->length].timestamp[i] = vectorClock[i];
		currentChain->length++;

		// Print the updated block
		printf("Blockchain updated with new miner's info:\n\tblockID = %d block = [", currentChain->blocks[currentChain->length - 1].blockID);
		for(int i = 0; i < 10; i++)
			printf("%d, ", currentChain->blocks[currentChain->length - 1].coinAmounts[i]);
		printf("]\n\ttimestamp = [");
		for(int i = 0; i < 10; i++)
			printf("%d, ", currentChain->blocks[currentChain->length - 1].timestamp[i]);
		printf("]\n");

		// Send back our blockchain
		printf("Sending blockchain response\n");
		write(connectionFDs[newClient.identifier], currentChain, sizeof(struct blockchain));
		printf("Blockchain response sent.\n");

		return 0;
	}
	else
	{
		printf("Error receiving info from new client.\n");
		return -1;
	}
}

void DieWithError(const char *errorMessage)
{
	perror(errorMessage);
	exit(1);
}

struct block initBlock(char fileBuffer[255])
{
	struct block returnBlock;
	for(int i = 0; i < 10; i++)
	{
		returnBlock.coinAmounts[i] = 0;
		returnBlock.timestamp[i] = 0;
	}

	int i = 0;
	int j = 0;
	int k = 0;
	//hello there
	char coinBuffer[5];
	memset(coinBuffer, 0, 5);

	// While we have more numbers
	while(fileBuffer[i] != '\n')
	{
		// If we've hit the end of a number
		if(fileBuffer[i] == ' ')
		{
			// Add the number to the coinAmounts
			coinBuffer[j] = '\0';
			returnBlock.coinAmounts[k] = atoi(coinBuffer);
			k++;
			memset(coinBuffer, 0, 5);
			j = 0;
		}

		coinBuffer[j] = fileBuffer[i];

		i++;
		j++;
	}

	return returnBlock;
}

struct minerQuery initPeers(char fileBuffer[255])
{
	struct minerQuery peerQuery;

	int i = 0;
	int j = 0;
	int numOfMiners = 1;

	// Init id's to -1
	for(int i = 0; i < 10; i++)
		peerQuery.minerInfos[i].identifier = -1;
	
	while(fileBuffer[i] != '\n')
	{
		numOfMiners++;
		int identifier = -1;
		char username[20];
		char ipAddress[20];
		char portNumber[20];

		// Get the identifier
		identifier = fileBuffer[i] - '0';
		i += 2;

		// Get the username
		while(fileBuffer[i] != ' ')
		{
			username[j] = fileBuffer[i];
			i++;
			j++;
		}
		username[j] = '\0';

		i++;
		j = 0;

		// Get the ipAddress
		while(fileBuffer[i] != ' ')
		{
			ipAddress[j] = fileBuffer[i];
			i++;
			j++;
		}
		ipAddress[j+1] = '\0';

		i++;
		j = 0;

		// Get the port number
		while(fileBuffer[i] != ',')
		{
			portNumber[j] = fileBuffer[i];
			i++;
			j++;
		}
		portNumber[j+1] = '\0';

		i++;
		j = 0;

		/* Save the data into peerQuery */
		// Identifier
		peerQuery.minerInfos[identifier].identifier = identifier;
		// Username
		strcpy(peerQuery.minerInfos[identifier].username, username);
		// Address
		struct sockaddr_in peerAddress;
		bzero(&peerAddress, sizeof(peerAddress));
		peerAddress.sin_family = AF_INET;
		inet_pton(AF_INET, ipAddress, &peerAddress.sin_addr);
		peerAddress.sin_port = htons(atoi(portNumber));
		printf("Peer %s(%d) Address = %s:%d\n", username, identifier, ipAddress, htons(peerAddress.sin_port));
		peerQuery.minerInfos[identifier].address = peerAddress;
	}

	peerQuery.numOfMiners = numOfMiners;

	return peerQuery;
}

/** This file is for writing the client and server sections of the miner. */
int main(int argc, char **argv)
{
	///////////////////////////////////////
	// Data defining this miner
	
	struct minerInfo selfInfo;
    int vectorClock[10];
	for(int i = 0; i < 10; i++)
		vectorClock[i] = 0;

    // The blockchain. Max 200 entries, can increase if needed.
    // First entry's data is provided by server
	struct blockchain currentChain;
	currentChain.length = 0;
	for(int i = 0; i < 200; i++)
	{
		currentChain.blocks[i].blockID = -1;
		for(int j = 0; j < 10; j++)
		{
			currentChain.blocks[i].coinAmounts[j] = 0;
			currentChain.blocks[i].timestamp[j] = 0;
		}
	}

	// Data defining this miner
	///////////////////////////////////////
	
	struct block* waitingTransaction = NULL;
  
	int numOfMiners = 0;
    struct minerInfo peers[10];

	// Initialize FDs to invalid
	int connectionFDs[10];
	for(int i = 0; i < 10; i++)
		connectionFDs[i] = -1;

	// Socket info
	fd_set fdSet;
	struct timeval timeVal;
	int socketFD;
	int max = 0;
	int reuse = 1;

	int serverFD = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serverAddress;

	/* Set up the socket */
	unsigned int addrLen = sizeof(struct sockaddr_in);
	printf("Setting up socket for listening.\n");
	if((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		DieWithError("client: socket() failed");
	setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
	if(bind(socketFD, (struct sockaddr *) &selfInfo.address, addrLen) < 0)
		DieWithError("client: bind() failed");
	if(listen(socketFD, BACKLOG) < 0)
		DieWithError("client: listen() failed");

	/* Checking if we're reading dummy data from a file */
	// If we're given an argument, treat it as a filename
	if(argc == 2)
	{
		printf("Loading dummy data from given file...\n");
		// Open the file
		FILE *initializerFP;
		initializerFP = fopen(argv[1], "r");

		char fileBuffer[255];

		// Get the identifier
		fgets(fileBuffer, 255, initializerFP);
		selfInfo.identifier = fileBuffer[0] - '0';
		// Get the username
		fgets(fileBuffer, 255, initializerFP);
		fileBuffer[strcspn(fileBuffer, "\r\n")] = '\0';
		strcpy(selfInfo.username, fileBuffer);
		// Get the IP
		fgets(fileBuffer, 255, initializerFP);
		bzero(&selfInfo.address, addrLen);
		selfInfo.address.sin_family = AF_INET;
		inet_pton(AF_INET, fileBuffer, &selfInfo.address.sin_addr);
		// Get the port
		fgets(fileBuffer, 255, initializerFP);
		selfInfo.address.sin_port = htons(atoi(fileBuffer));
		// Fill the first block
		fgets(fileBuffer, 255, initializerFP);
		currentChain.blocks[0] = initBlock(fileBuffer);
		currentChain.length++;
		// Set the initial coins
		selfInfo.initialCoins = currentChain.blocks[0].coinAmounts[selfInfo.identifier];
		// Fill the peers
		fgets(fileBuffer, 255, initializerFP);
		struct minerQuery tempQuery = initPeers(fileBuffer);
		numOfMiners = tempQuery.numOfMiners;
		for(int i = 0; i < 10; i++)
			peers[i] = tempQuery.minerInfos[i];
		
		fclose(initializerFP);
		printf("Data loaded. Username = %s Port = %d\n", selfInfo.username, htons(selfInfo.address.sin_port));
	}
	else if(argc == 7) // miner <Username> <Coins> <Self-IP> <Self-port> <Server-IP> <Server-port>
	{
		/** Setup self info */
		// Username
		strcpy(selfInfo.username, argv[1]);
		// Coins
		selfInfo.initialCoins = atoi(argv[2]);
		// IP
		bzero(&selfInfo.address, addrLen);
		selfInfo.address.sin_family = AF_INET;
		inet_pton(AF_INET, argv[3], &selfInfo.address.sin_addr);
		// Port
		selfInfo.address.sin_port = htons(atoi(argv[4]));
		
		// Setup the server info
		bzero(&serverAddress, addrLen);
		serverAddress.sin_family = AF_INET;
		inet_pton(AF_INET, argv[5], &serverAddress.sin_addr);
		serverAddress.sin_port = htons(atoi(argv[6]));
		printf("Data loaded. Username = %s Coins = %d Port = %d\n", selfInfo.username, selfInfo.initialCoins, htons(selfInfo.address.sin_port));

		printf("Connecting to server.\n");
		connect(serverFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
		printf("Connection successful.\n");

		// Register
		struct toServerMessage toMessage;
		toMessage.type = 1;
		memcpy(&toMessage.clientInfo, &selfInfo, sizeof(struct toServerMessage));

		printf("Registering with server.\n");
		write(serverFD, &toMessage, sizeof(struct toServerMessage));

		struct fromServerMessage fromMessage;
		ssize_t n = read(serverFD, &fromMessage, sizeof(struct fromServerMessage));
		if(n > 0)
		{
			if(fromMessage.type == 1 && strcmp(fromMessage.returnCode, "SUCCESS") == 0)
			{
				printf("Registration Successful.\n");
				selfInfo.identifier = fromMessage.identifier;
			}
			else
			{
				printf("Registration failed. Exiting.\n");
				exit(1);
			}
		}

		// Query
		printf("Querying server.\n");
		toMessage.type = 0;
		write(serverFD, &toMessage, sizeof(struct toServerMessage));

		n = read(serverFD, &fromMessage, sizeof(struct fromServerMessage));
		if(n > 0)
		{
			if(fromMessage.type == 0)
			{
				// Save peers and set the first block
				memcpy(&peers, &fromMessage.peers.minerInfos, (sizeof(struct minerInfo) * 10));
				for(int i = 0; i < 10; i++)
				{
					currentChain.blocks[0].coinAmounts[i] = fromMessage.peers.minerInfos[i].initialCoins;
					currentChain.blocks[0].timestamp[i] = 0;
				}
				currentChain.length++;
				currentChain.blocks[0].blockID++;

				printf("Blockchain updated with initial coins:\n\tblockID = %d\n\tblock = [", currentChain.blocks[currentChain.length - 1].blockID);
				for(int i = 0; i < 10; i++)
					printf("%d, ", currentChain.blocks[currentChain.length - 1].coinAmounts[i]);
				printf("]\n\ttimestamp = [");
				for(int i = 0; i < 10; i++)
					printf("%d, ", currentChain.blocks[currentChain.length - 1].timestamp[i]);
				printf("]\n");
			}
			printf("Query response received. NumOfMiners = %d\n", fromMessage.peers.numOfMiners);
		}
	}
	else
	{
		printf("Unknown number of arguments\n");
		exit(1);
	}

	/* Start establishing connections */
	//### select() example code taken from https://stackoverflow.com/questions/2284428/in-c-networking-using-select-do-i-first-have-to-listen-and-accept ###
	establishConnections(&selfInfo, &currentChain, vectorClock, peers, connectionFDs);

	/* Start listening for user input, more connections, and transactions  */
	printf("Listening for input, connections, and transactions...\n");
	int running = 1;
	int validationStep = 0; // 0 = verify coins, 1 = verify timestamps
	int largeInt = rand() + 25000; // 25000 - 57767
	srand(time(NULL));
	int randIterations = rand() % 50;
	int primeIteration = 0;
	while(running)
	{
		FD_ZERO(&fdSet);
		FD_SET(socketFD, &fdSet);
		FD_SET(STDIN, &fdSet);
		if(socketFD >= max) 
			max = socketFD + 1;

		// Add existing connections to the fdSet
		for(int i = 0; i < 10; i++)
		{
			FD_SET(connectionFDs[i], &fdSet);
			if(connectionFDs[i] >= max) 
				max = connectionFDs[i] + 1;
		}

		// Check for readable connections
		timeVal.tv_sec = 0; timeVal.tv_usec = 50000;
		if(select(max, &fdSet, NULL, NULL, &timeVal) > 0)
		{
			// Something can be read from the socket
			if(FD_ISSET(socketFD, &fdSet))
			{
				// Found a listener, connect to them
				if(connectToClient(&socketFD, &selfInfo, vectorClock, &currentChain, peers, connectionFDs) == 0)
					numOfMiners++;
				else
					vectorClock[selfInfo.identifier]--;
			}

			// Run through the connections in the socket to check for things to read
			for(int i = 0; i < 10; i++)
			{
				// Skip connections that have been invalidated
				if(connectionFDs[i] == -1)
					continue;

				if(FD_ISSET(connectionFDs[i], &fdSet))
				{
					struct blockMessage incomingTransaction;
					ssize_t n = read(connectionFDs[i], &incomingTransaction, sizeof(struct blockMessage));
					if(n < 1)
					{
						// Endpoint died, remove it.
						connectionFDs[i] = -1;
						numOfMiners--;

						printf("Removed %d: connection terminated. Err = %s\n", i, strerror(errno));
					}
					else
					{
						if(incomingTransaction.type == 0)
						{
							printf("Received purchase request.\n");
							// Save the incoming transaction to be processed
							waitingTransaction = malloc(sizeof(struct block));
							for(int i = 0; i < 10; i++)
							{
								waitingTransaction->coinAmounts[i] = 0;
								waitingTransaction->timestamp[i] = 0;
							}
							waitingTransaction->blockID = incomingTransaction.messageBlock.blockID;
							for(int i = 0; i < 10; i++)
								waitingTransaction->coinAmounts[i] = incomingTransaction.messageBlock.coinAmounts[i];
							for(int i = 0; i < 10; i++)
								waitingTransaction->timestamp[i] = incomingTransaction.messageBlock.timestamp[i];

							// Update clock
							vectorClock[selfInfo.identifier]++;
							updateClock(vectorClock, waitingTransaction->timestamp);

							// Print the received block
							printf("Request is ready to be processed. Info:\n\tblockID = %d\n\tblock = [", waitingTransaction->blockID);
							for(int i = 0; i < 10; i++)
								printf("%d, ", waitingTransaction->coinAmounts[i]);
							printf("]\n\ttimestamp = [");
							for(int i = 0; i < 10; i++)
								printf("%d, ", waitingTransaction->timestamp[i]);
							printf("]\n");
						}
						else if(incomingTransaction.type == 1)
						{
							printf("Received proof of work. Checking for validity.\n");
							int invalidCode = validateBlock(&currentChain, &incomingTransaction.messageBlock);

							if(invalidCode == 0)
							{
								if(checkUniqueID(&currentChain, incomingTransaction.messageBlock.blockID))
								{
									printf("Received proof is valid. Adding to blockchain.\n");
									addBlock(&currentChain, &incomingTransaction.messageBlock);
									validationStep = 0;
									primeIteration = 0;
									free(waitingTransaction);
									waitingTransaction = NULL;
								}
								else
									invalidCode = 3;
							}
							else
							{
								if(invalidCode == 1)
									printf("Received proof is invalid. Old timestamp is newer than new timestamp. Continuing with own processing.\n");
								else if (invalidCode == 2)
									printf("Received proof is invalid. Coins resulted in negative funds. Continuing with own processing.\n");
								else
									printf("Received proof is invalid. Duplicate ID. Continuing with own processing.\n");
							}
						}
						else
							printf("ERROR: Received message of unexpected format.\n");
					}
				}
			}

			// Something can be read from stdin
			if(FD_ISSET(STDIN, &fdSet))
			{
				parseCommand(&selfInfo, vectorClock, &currentChain, peers, connectionFDs, &waitingTransaction);
			}
		}

		// Something is waiting to be computed
		int invalidCode = 0;
		if(waitingTransaction != NULL)
		{
			if(validationStep == 0) // Re-compute the blockchain
			{
				printf("Re-computing blockchain with new block to verify timestamps and coin amounts.\n");
				invalidCode = validateBlock(&currentChain, waitingTransaction);
				randIterations = (rand() % 40) + 10;

				// Done checking, ready for processing
				if(invalidCode == 0)
				{
					printf("Timestamp and coin amounts valid.\n");
					validationStep = 1;
				}
			}
			else if(validationStep == 1)
			{
				// Process one iteration of the primes, then pass control so we can check for incoming proofs
				if(primeIteration < randIterations)
				{
					printf("Processing primes. Iteration = %d, End at %d\n", primeIteration, randIterations - 1);
					isPrime(largeInt);
					primeIteration++;
				}
				else
				{
					printf("Done processing primes.\n");
					primeIteration = 0;
					validationStep = 2;
				}
			}
			else if(validationStep == 2)
			{
				printf("New block is validated. Adding block to chain and broadcasting success.\n");
				broadcastProof(&selfInfo, peers, connectionFDs, waitingTransaction);
				addBlock(&currentChain, waitingTransaction);
				validationStep = 0;

				// Reset waitingTransaction
				free(waitingTransaction);
				waitingTransaction = NULL;
			}

			if(invalidCode != 0)
			{
				if(invalidCode == 1)
					printf("Transaction invalid. Old timestamp is newer than new timestamp. Nullifying transaction\n");
				else
					printf("Transaction invalid. Coins resulted in negative funds. Nullifying transaction\n");

				free(waitingTransaction);
				waitingTransaction = NULL;
				validationStep = 0;
				invalidCode = 0;
			}
		}
	}
}
