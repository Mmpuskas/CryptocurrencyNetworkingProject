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

void DieWithError(const char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

int getNumMiners(int* connectionFDs)
{
	int numOfMiners = 0;

	for(int i = 0; i < 10; i++)
		if(connectionFDs[i] != -1)
			numOfMiners++;

	return numOfMiners;
}

int getOpenID(int* connectionFDs)
{
	for(int i = 0; i < 10; i++)
		if(connectionFDs[i] == -1)
			return i;

	return -1;
}

void connectToClient(int* socketFD, struct minerInfo* clients, int* connectionFDs)
{
    printf("\nNew connection found. Attempting to connect.\n");
    unsigned int addrLen = sizeof(struct sockaddr_in);
    struct sockaddr_in connectedAddress;

    // Accept the connection
    int tempFD = accept(*socketFD, (struct sockaddr *) &connectedAddress, &addrLen);
    char clientName[20];
    printf("Connected to: %s:%d\n", inet_ntop(AF_INET, &connectedAddress.sin_addr, clientName, sizeof(clientName)), htons(connectedAddress.sin_port));

	// Give the client a local ID and save their info at it
	int identifier = getOpenID(connectionFDs);
	clients[identifier].identifier = identifier;
	clients[identifier].address = connectedAddress;
	connectionFDs[identifier] = tempFD;

	printf("Client info saved at ID %d\n", identifier);
}

int main(void)
{
	// Set up address (port hardcoded to 41499)
	struct sockaddr_in address;
    unsigned int addrLen = sizeof(struct sockaddr_in);
    memset(&address, 0, addrLen);   /* Zero out structure */
    address.sin_family = AF_INET;                /* Internet address family */
    address.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    address.sin_port = htons(41499);      /* Local port */

	struct minerInfo clients[10];
    for(int i = 0; i < 10; i++)
	{
		clients[i].identifier = -1;
		clients[i].initialCoins = 0;
	}

    // Initialize FDs to invalid
    int connectionFDs[10];
    for(int i = 0; i < 10; i++)
        connectionFDs[i] = -1;

	// Socket stuff
	fd_set fdSet;
	struct timeval timeVal;
	int socketFD; // Listening socket
	int max = 0;
	int reuse = 1;

	// Setup listener
    if((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        DieWithError("client: socket() failed");
    setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
    if(bind(socketFD, (struct sockaddr *) &address, addrLen) < 0)
        DieWithError("client: bind() failed");
    if(listen(socketFD, BACKLOG) < 0)
        DieWithError("client: listen() failed");

	printf("Server online. Listening for connections.\n");

	// Select stuff
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
        timeVal.tv_sec = 0; timeVal.tv_usec = 50000;
        if(select(max, &fdSet, NULL, NULL, &timeVal) > 0)
        {
            // Something can be read from the socket
            if(FD_ISSET(socketFD, &fdSet))
            {
                // Found a listener, connect to them
                connectToClient(&socketFD, clients, connectionFDs);
            }

            // Run through the connections in the socket to check for things to read
            for(int i = 0; i < 10; i++)
            {
                // Skip connections that have been invalidated
                if(connectionFDs[i] == -1)
                    continue;

                if(FD_ISSET(connectionFDs[i], &fdSet))
                {
                    struct toServerMessage toMessage;
                    ssize_t n = read(connectionFDs[i], &toMessage, sizeof(struct toServerMessage));
                    if(n < 1)
                    {
 						// Endpoint died, remove it.
                        connectionFDs[i] = -1;

                        printf("Removed %d: connection terminated. Err = %s\n", i, strerror(errno));
					}
					else
					{
						if(toMessage.type == 0)
						{
							// Query
							printf("Received query message.\n");
							printf("Sending following active miners:\n");
							for(int j = 0; j < 10; j++)
							{
								// Skip connections that have been invalidated
								if(connectionFDs[j] == -1)
									continue;

								struct in_addr ipAddr = clients[i].address.sin_addr;
								char ipStr[INET_ADDRSTRLEN];
								inet_ntop(AF_INET, &ipAddr, ipStr, INET_ADDRSTRLEN);
								printf("Name: %s, Coins: %d, Address: %s:%d\n", clients[j].username, clients[j].initialCoins, ipStr, clients[j].address.sin_port);
							}

							// Send the response
							struct fromServerMessage fromMessage;
							fromMessage.type = 0; // Query response
							fromMessage.peers.numOfMiners = getNumMiners(connectionFDs);
							memcpy(&fromMessage.peers.minerInfos, &clients, (sizeof(struct minerInfo) * 10));

							write(connectionFDs[i], &fromMessage, sizeof(struct fromServerMessage));
							printf("Query response sent.\n");
						}
						else if(toMessage.type == 1)
						{
							// Register
							printf("Received register message.\n");
							
							// Save the client info
							memcpy(&clients[i], &toMessage.clientInfo, sizeof(struct minerInfo));
							clients[i].identifier = i;
							printf("Saved client info. Username = %s (%d), Coins = %d, Port = %d\n", clients[i].username, clients[i].identifier, clients[i].initialCoins, htons(clients[i].address.sin_port));

							// Send the response
							struct fromServerMessage fromMessage;
							fromMessage.type = 1; // Register response
							fromMessage.identifier = i;
							strcpy(fromMessage.returnCode, "SUCCESS");

							write(connectionFDs[i], &fromMessage, sizeof(struct fromServerMessage));
							printf("Registration successful. Response sent.\n");
						}
						else if(toMessage.type == 2)
						{
							// Deregister
							printf("Received save message.\n");
						}
						else if(toMessage.type == 3)
						{
							// Save
							printf("Received save message.\n");
						}
					}
				}
			}
		}
	}

	return 0;
}
