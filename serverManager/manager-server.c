#include	<sys/socket.h> 	/* for socket() and bind() */
#include	<stdio.h>		/* printf() and fprintf() */
#include	<stdlib.h>		/* for atoi() and exit() */
#include	<arpa/inet.h>	/* for sockaddr_in and inet_ntoa() */
#include	<sys/types.h>
#include	<string.h>
#include	<unistd.h>
#include "../src/dataStructs.h"

#define BACKLOG	128

struct minerList ml;


void
DieWithError(const char *errorMessage) /* External error handling function */
{
	perror(errorMessage);
	exit(1);
}

void PrintAndEchoStruct(int sockfd)
{
	//ml = malloc(sizeof minerList);

	int reqSize = sizeof(struct managerReq);
  ssize_t n;
	struct managerReq reqRec;
	if ( (n = read(sockfd, &reqRec, reqSize)) == 0)
		DieWithError("str_cli: server terminated prematurely");

if (reqRec.cmd == 1){//query
	int mlSize = sizeof(struct minerList);
	write(sockfd, &ml, mlSize);
}
if (reqRec.cmd == 2){//register
	int intSize = sizeof(int);
	for(int i = 0; i <10; i++){
		if (ml.minerInfos[i].active == 0)
			ml.minerInfos[i] = reqRec.newMinerInfo;
			write(sockfd, &i, intSize);
	}
	//print peers full error
}
if (reqRec.cmd == 3){//deregister
			char success[7];
			strcpy(success,"SUCCESS");
			ml.minerInfos[reqRec.ID].active = 0;
			write(sockfd, success, 7);
	//print ID not found error
}
if (reqRec.cmd == 3){//save
	//save the ml into a txt file
}
}

int
main(int argc, char **argv)
{
    int sock, connfd;                /* Socket */
    struct sockaddr_in echoServAddr; /* Local address */
    struct sockaddr_in echoClntAddr; /* Client address */
    unsigned int cliAddrLen;         /* Length of incoming message */
    unsigned short echoServPort;     /* Server port */

    if (argc != 2)         /* Test for correct number of parameters */
    {
        fprintf(stderr,"Usage: %s <TDP SERVER PORT>\n", argv[0]);
        exit(1);
    }

    echoServPort = atoi(argv[1]);  /* First arg:  local port */

    /* Create socket for sending/receiving datagrams */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        DieWithError("server: socket() failed");

    /* Construct local address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));   /* Zero out structure */
    echoServAddr.sin_family = AF_INET;                /* Internet address family */
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    echoServAddr.sin_port = htons(echoServPort);      /* Local port */

    /* Bind to the local address */
    if (bind(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
        DieWithError("server: bind() failed");

	if (listen(sock, BACKLOG) < 0 )
		DieWithError("server: listen() failed");

	cliAddrLen = sizeof(echoClntAddr);
	connfd = accept( sock, (struct sockaddr *) &echoClntAddr, &cliAddrLen );

	PrintAndEchoStruct(connfd);
	close(connfd);
}
