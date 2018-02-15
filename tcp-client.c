#include        <sys/socket.h>  /* for socket() and bind() */
#include        <stdio.h>               /* printf() and fprintf() */
#include        <stdlib.h>              /* for atoi() and exit() */
#include        <arpa/inet.h>   /* for sockaddr_in and inet_ntoa() */
#include        <sys/types.h>
#include        <string.h>
#include        <unistd.h>
#include "dataStructs.h"

#define ECHOMAX 255             /* Longest string to echo */
#define BACKLOG 128

void
DieWithError(const char *errorMessage) /* External error handling function */
{
        perror(errorMessage);
        exit(1);
}

void
str_cli(FILE *fp, int sockfd)
{
	ssize_t n;
        char    sendline[ECHOMAX], recvline[ECHOMAX];

        while (fgets(sendline, ECHOMAX, fp) != NULL) {

                write(sockfd, sendline, strlen(sendline));

                if ( (n = read(sockfd, recvline, ECHOMAX)) == 0)
                        DieWithError("str_cli: server terminated prematurely");

		recvline[ n ] = '\0';
                fputs(recvline, stdout);
        }
}

void sendStruct(FILE *fp, int sockfd)
{
	int blockSize = sizeof(struct block);
	struct block blockToSend;
	ssize_t n;

	for(int i = 0; i < 10; i++)
		blockToSend.coinAmounts[i] = i + 42;

	write(sockfd, &blockToSend, blockSize);

	struct block blockReceived;
	if ( (n = read(sockfd, &blockReceived, blockSize)) == 0)
		DieWithError("str_cli: server terminated prematurely");


	printf("coinAmounts in block received from server: ");
	for(int i = 0; i < 10; i++)
		printf("%d ", blockReceived.coinAmounts[i]);
	printf("\n");
}

int
main(int argc, char **argv)
{
	int sockfd;
	struct sockaddr_in servaddr;

	if (argc != 3)
		DieWithError( "usage: tcp-client <Server-IPaddress> <Server-Port>" );
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

	connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

	//str_cli(stdin, sockfd);		/* do it all */
	sendStruct(stdin, sockfd);

	exit(0);
}
