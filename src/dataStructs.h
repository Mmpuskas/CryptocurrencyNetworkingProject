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
	struct sockaddr_in address;
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
