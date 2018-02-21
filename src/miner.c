#include <sys/socket.h>  /* for socket() and bind() */
#include <stdio.h>               /* printf() and fprintf() */
#include <stdlib.h>              /* for atoi() and exit() */
#include <arpa/inet.h>   /* for sockaddr_in and inet_ntoa() */
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "dataStructs.h"

#define BACKLOG 10
#define BLOCKCHAINLENGTH 200

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
			printf("Connection successful. Sending info.\n");
			write(connectionFDs[i], selfInfo, sizeof(struct minerInfo));
			printf("Info sent.\n");

			// Wait for the blockchain response
			printf("Waiting for blockchain response.\n");
			struct blockchain tempBlockchain;
			ssize_t n = read(connectionFDs[i], &tempBlockchain, sizeof(struct blockchain));
			if(n > 0)
			{
				printf("Blockchain response received. First block: [");
				for(int i = 0; i < 10; i++)
					printf("%d, ", tempBlockchain.blocks[0].coinAmounts[i]);
				printf("]\n");
			}
			else
				printf("Error in receiving blockchain response\n");

			if(n > 0)
			{
				updateClock(vectorClock, tempBlockchain.timestamp);
				vectorClock[selfInfo->identifier]++;
				printf("Timestamp received. Updated vectorClock [");
				for(int i = 0; i < 10; i++)
					printf("%d, ", vectorClock[i]);
				printf("]\n");
			}
			else
				printf("Error in receiving blockchain timestamp\n");

			// Replace our blockchain with the response if necessary
			int bClockChanged = 0;
			compareVectorClocks(currentChain->timestamp, tempBlockchain.timestamp);
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
	
	// Read the new client's data
	struct minerInfo newClient;
	ssize_t n = read(tempFD, &newClient, sizeof(struct minerInfo));

	if(n > 0)
	{
		// Save the new client's info
		connectionFDs[newClient.identifier] = tempFD;
		peers[newClient.identifier] = newClient;

		// Add new block with new miner's initial coins
		currentChain->blocks[currentChain->length].coinAmounts[newClient.identifier] = newClient.initialCoins;
		currentChain->length++;
		printf("Information received. Identifier = %d, Username = %s, Coins = %d\n"
				, newClient.identifier, newClient.username, newClient.initialCoins);

		// Doing an event, update the clock
		vectorClock[selfInfo->identifier]++;
		for(int i = 0; i < 10; i++)
			currentChain->timestamp[i] = vectorClock[i];

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
		returnBlock.coinAmounts[i] = 0;

	int i = 0;
	int j = 0;
	int k = 0;

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
		printf("Peer %d port = %d\n", identifier, htons(peerAddress.sin_port));
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
  
    // Username/ip/port
	int numOfMiners = 0;
    struct minerInfo peers[10];

	// Data defining this miner
	///////////////////////////////////////

	/* Checking if we're reading dummy data from a file */
	// If we're given an argument, treat it as a filename
	unsigned int addrLen = sizeof(struct sockaddr_in);
	if(argc == 4)
	{
		printf("Loading dummy data from given file...\n");
		// Open the file
		FILE *initializerFP;
		initializerFP = fopen(argv[3], "r");

		char fileBuffer[255];

		// Get the identifier
		fgets(fileBuffer, 100, initializerFP);
		selfInfo.identifier = fileBuffer[0] - '0';
		// Get the username
		fgets(fileBuffer, 100, initializerFP);
		strcpy(selfInfo.username, fileBuffer);
		// Get the IP
		fgets(fileBuffer, 100, initializerFP);
		bzero(&selfInfo.address, addrLen);
		selfInfo.address.sin_family = AF_INET;
		inet_pton(AF_INET, fileBuffer, &selfInfo.address.sin_addr);
		// Get the port
		fgets(fileBuffer, 100, initializerFP);
		selfInfo.address.sin_port = htons(atoi(fileBuffer));
		// Fill the first block
		fgets(fileBuffer, 100, initializerFP);
		currentChain.blocks[0] = initBlock(fileBuffer);
		currentChain.length++;
		// Set the initial coins
		selfInfo.initialCoins = currentChain.blocks[0].coinAmounts[selfInfo.identifier];
		// Fill the peers
		fgets(fileBuffer, 100, initializerFP);
		struct minerQuery tempQuery = initPeers(fileBuffer);
		numOfMiners = tempQuery.numOfMiners;
		for(int i = 0; i < 10; i++)
			peers[i] = tempQuery.minerInfos[i];
		
		fclose(initializerFP);
		printf("Data loaded. Port = %d\n", htons(selfInfo.address.sin_port));
	}
	else if(argc != 3)
	{
		printf("Unknown number of arguments\n");
		exit(1);
	}

	/* Set up the socket */
	printf("Setting up socket for listening.\n");
	fd_set fdSet;
	struct timeval timeVal;
	int socketFD;
	int max = 0;
	int reuse = 1;

	// Initialize FDs to invalid
	int connectionFDs[10];
	for(int i = 0; i < 10; i++)
		connectionFDs[i] = -1;

	if((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		DieWithError("client: socket() failed");
	setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
	if(bind(socketFD, (struct sockaddr *) &selfInfo.address, addrLen) < 0)
		DieWithError("client: bind() failed");
	if(listen(socketFD, BACKLOG) < 0)
		DieWithError("client: listen() failed");

	/* Start establishing connections */
	//### select() example code taken from https://stackoverflow.com/questions/2284428/in-c-networking-using-select-do-i-first-have-to-listen-and-accept ###
	establishConnections(&selfInfo, &currentChain, vectorClock, peers, connectionFDs);

	/* Start listening for user input, more connections, and transactions  */

	printf("Listening for input, connections, and transactions...\n");
	int running = 1;
	while(running)
	{
		FD_ZERO(&fdSet);
		FD_SET(socketFD, &fdSet);
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
		timeVal.tv_sec = 1; timeVal.tv_usec = 0;
		if(select(max, &fdSet, NULL, NULL, &timeVal) > 0)
		{
			// Something can be read
			if(FD_ISSET(socketFD, &fdSet))
			{
				// Found a listener, connect to them
				vectorClock[selfInfo.identifier]++;
				if(connectToClient(&socketFD, &selfInfo, vectorClock, &currentChain, peers, connectionFDs) == 0)
					numOfMiners++;
				else
					vectorClock[selfInfo.identifier]--;
			}

			for(int i = 0; i < 10; i++)
			{
				// Skip connections that have been invalidated
				if(connectionFDs[i] == -1)
					continue;

				if(FD_ISSET(connectionFDs[i], &fdSet))
				{
					char stringReceived[255];
					ssize_t n = read(socketFD, &stringReceived, 255);
					if(n < 1)
					{
						// Endpoint died, remove it.
						connectionFDs[i] = -1;
						numOfMiners--;

						printf("Removed %d: connection terminated\n", i);
					}
					else
					{
						stringReceived[n] = '\0';
						printf("New response len %zd: %s\n", n, stringReceived);
					}
				}
			}
		}
	}
}
