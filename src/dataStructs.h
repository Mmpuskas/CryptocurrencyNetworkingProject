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

	// The unique identifier, starting at identifier*1000 and incrementing as more are generated
	int blockID;

	// The vector clock timestamp
	int timestamp[10];
};

struct blockchain
{
	int length;
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

struct blockMessage
{
	// 0 = request, 1 = proof
	int type;

	struct block messageBlock;
};

struct toServerMessage
{
	// 0 = query, 1 = register, 2 = deregister, 3 = save
	int type;

	// Full in register, only has username in deregister
	struct minerInfo clientInfo;

	// For save
	char fileName[20];
};

struct fromServerMessage
{
	// 0 = peers, 1 = (returnCode,identifier), 2 = returnCode
	int type;

	// For query
	struct minerQuery peers;

	// For register
	int identifier;

	// SUCCESS or FAILURE
	// For register, deregister, save
	char returnCode[8];
};
