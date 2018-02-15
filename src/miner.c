#include <sys/socket.h>  /* for socket() and bind() */
#include <stdio.h>               /* printf() and fprintf() */
#include <stdlib.h>              /* for atoi() and exit() */
#include <arpa/inet.h>   /* for sockaddr_in and inet_ntoa() */
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include "dataStructs.h"

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
	
	while(fileBuffer[i] != '\n')
	{
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
    // Rest is sent by the server
	int numOfMiners = 0;
    struct minerInfo peers[10];

	// Data defining this miner
	///////////////////////////////////////

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
		int peerIndex = 1;
		printf("name: %s, port: %d\n", peers[peerIndex].username, htons(peers[peerIndex].address.sin_port));
		
		printf("Data loaded.\n");
	}
	else if(argc > 1)
	{
		exit(1);
	}
}
