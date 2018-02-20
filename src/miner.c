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

void handleInput(int socketFDs[], int identifier, int numOfMiners)
{
	while(1)
	{
		for(int i = 0; i < numOfMiners; i++)
		{
			// Skip over ourselves
			if(i == identifier)
				continue;

			char message[10] = "Hello ";
			message[5] = identifier + '0';
			write(socketFDs[i], message, 10);

			char received[10];
			read(socketFDs[i], &received, 10);
			printf("%s\n", received);
		}
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
		// Username
		strcpy(peerQuery.minerInfos[identifier].username, username);
		// Address
		struct sockaddr_in peerAddress;
		bzero(&peerAddress, sizeof(peerAddress));
		peerAddress.sin_family = AF_INET;
		inet_pton(AF_INET, ipAddress, &peerAddress.sin_addr);
		peerAddress.sin_port = htons(atoi(portNumber));
		printf("%d\n", atoi(portNumber));
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
	
    int identifier; // 0-9
    char username[20];
    struct sockaddr_in localAddress;
    int vectorClock[10];
	for(int i = 0; i < 10; i++)
		vectorClock[i] = 0;

    // The blockchain. Max 200 entries, can increase if needed.
    // First entry's data is provided by server
    struct block blockchain[200];
  
    // Username/ip/port
	int numOfMiners = 0;
    struct minerInfo peers[10];

	// Data defining this miner
	///////////////////////////////////////

	/* Checking if we're reading dummy data from a file */
	// If we're given an argument, treat it as a filename
	if(argc == 4)
	{
		printf("Loading dummy data from given file...\n");
		// Open the file
		FILE *initializerFP;
		initializerFP = fopen(argv[3], "r");

		char fileBuffer[255];

		// Get the identifier
		fgets(fileBuffer, 100, initializerFP);
		identifier = fileBuffer[0] - '0';
		// Get the username
		fgets(fileBuffer, 100, initializerFP);
		strcpy(username, fileBuffer);
		// Get the IP
		fgets(fileBuffer, 100, initializerFP);
		bzero(&localAddress, sizeof(localAddress));
		localAddress.sin_family = AF_INET;
		inet_pton(AF_INET, fileBuffer, &localAddress.sin_addr);
		// Get the port
		fgets(fileBuffer, 100, initializerFP);
		localAddress.sin_port = htons(atoi(fileBuffer));
		// Fill the first block
		fgets(fileBuffer, 100, initializerFP);
		blockchain[0] = initBlock(fileBuffer);
		// Fill the peers
		fgets(fileBuffer, 100, initializerFP);
		struct minerQuery tempQuery = initPeers(fileBuffer);
		numOfMiners = tempQuery.numOfMiners;
		for(int i = 0; i < 10; i++)
			peers[i] = tempQuery.minerInfos[i];
		
		fclose(initializerFP);
		printf("Data loaded. Port = %d\n", htons(localAddress.sin_port));
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
	int connectionFDs[10];
	int max = 0;
	int reuse = 1;
	unsigned int addrLen = sizeof(struct sockaddr_in);

	if((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		DieWithError("client: socket() failed");
	setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
	if(bind(socketFD, (struct sockaddr *) &localAddress, addrLen) < 0)
		DieWithError("client: socket() failed");
	if(listen(socketFD, BACKLOG) < 0)
		DieWithError("client: socket() failed");

	/* Start establishing connections */
	printf("Establishing connections.\n");

	// For each miner in our list, establish a connection
	int numConnections = 0;
	for(int i = 0; i < numOfMiners; i++)
	{
		// Skip ourselves
		if(i == identifier)
			continue;

		printf("Attempting connection to: %d\n", htons(peers[i].address.sin_port));
		connectionFDs[i] = socket(AF_INET, SOCK_STREAM, 0);
		int result = connect(connectionFDs[i], (struct sockaddr *) &(peers[i].address), addrLen);
		if(result == -1)
			printf("Errno = %s\n", strerror(errno));
		else
		{
			numConnections++;
			printf("Connection successful\n");
		}
	}

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
		for(int i = 0; i < numConnections; i++)
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
				// Found a listener
				printf("New connection found. Attempting to connect.\n");
				struct sockaddr_in connectedAddress;
				connectionFDs[numConnections] = accept(socketFD, (struct sockaddr *) &connectedAddress, &addrLen);
				printf("Connected to: %d\n", htons(connectedAddress.sin_port));

				numConnections++;
			}

			for(int i = 0; i < numConnections; i++)
			{
				if(FD_ISSET(connectionFDs[i], &fdSet))
				{
					printf("Got data to handle\n");
				}
			}
		}
	}
}
