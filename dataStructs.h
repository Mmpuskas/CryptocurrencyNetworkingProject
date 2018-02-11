#include <stdio.h>  /* printf() and fprintf() */
#include <stdlib.h> /* for atoi() and exit() */
#include <string.h>
#include <arpa/inet.h> /* For sockaddr_in and inet_ntoa() */

/** This file is for writing the data structures for both the server and miners */

struct block
{
	// The change in amounts for each peer, indexed by ther identifier
	int coinAmounts[10];
};

// Used by other structs
struct minerInfo
{
	char username[20];
	struct sockaddr_in ipAddress;
	unsigned short portNumber;
};

// A miner's info about itself
// Just for planning. Will probably be moved into the program itself
struct selfInfo
{
	int identifier; // 0-9
	char username[20];
	struct sockaddr_in ipAddress;
	unsigned short portNumber;
	int vectorClock[10];

	// The blockchain. Max 200 entries, can increase if needed.
	// First entry's data is provided by server
	struct block blockchain[200];
	
	// The other miner's data. username/ip/port
	// This will all be maintained and sent by the server
	struct minerInfo peers[10];
};

// The struct used to respond to a server query
struct minerQuery
{
	int numOfMiners;
	struct minerInfo minerInfos[10];
};

struct purchaseRequest
{
	// The miner being purchased from
	int identifier;

	// The number of coins being spent
	int numOfCoins;

	// The vector clock timestamp
	int timestamp[10];
};
