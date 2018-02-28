#include <stdio.h>  /* printf() and fprintf() */
#include <stdlib.h> /* for atoi() and exit() */
#include <string.h>
#include <arpa/inet.h> /* For sockaddr_in and inet_ntoa() */

#define BLOCKCHAINLENGTH 200

/** This file is for writing the data structures for both the server and miners */
struct block
{
	// The change in amounts for each peer, indexed by ther identifier
	int coinAmounts[10];
};

struct blockchain
{
	int length;
	int timestamp[10];
	struct block blocks[BLOCKCHAINLENGTH];
};

struct minerInfo
{
	int identifier;
	int initialCoins;
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
	// The miner spending money
	int spenderID;

	// The miner receiving money
	int receiverID;

	// The number of coins being spent
	int amount;

	// The vector clock timestamp
	int timestamp[10];
};
