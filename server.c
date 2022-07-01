/* Server side - UDP Code				    */
/* By Hugh Smith	4/1/2017	*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "server.h"
#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "cpe464.h"
#include "pollLib.h"
#include "pdu.h"
#include "window.h"

#define MAXBUF 80

void processClient(int socketNum);

int main ( int argc, char *argv[])
{ 
	int socketNum = 0;				
	int portNumber = 0, pid = 1;
	FILE *in = NULL;
	float errorRate;
	struct sockaddr_in6 client;
	SlidingWindow window;

	portNumber = checkArgs(argc, argv);
	errorRate = atof(argv[1]);

	sendErr_init(errorRate, 1, 1, 1, 1);
	setupPollSet();

	socketNum = udpServerSetup(portNumber);
	addToPollSet(socketNum);
	while(pid != 0)
	{
		pollCall(-1);
		pid = fork();
		if(pid == 0 && (in = setupClient(&socketNum, &client, &window)) == NULL)
		{
			perror("server setup return\n");
			exit(-1);
		}
	}	
	sendFile(socketNum, &client, in, &window);
	close(socketNum);
	return 0;
}

// processes initial client message and create sliding window
FILE *setupClient(int *socketNum, struct sockaddr_in6 *client, SlidingWindow *window)
{
	uint8_t buffer[MAX_PDU_SIZE], response[MAX_PDU_SIZE], status;
	uint32_t windowSize, bufferSize;
	int pdulen, newSocket;
	FILE *file;
    uint serverAddrLen = sizeof(struct sockaddr_in6);
	char *filename;	
	if ((pdulen = recvfrom(*socketNum, buffer, MAX_PDU_SIZE, 0, (struct sockaddr *) client, &serverAddrLen)) < 0)
	{
		perror("bad recv setup client to server\n");
		exit(-1);
	}
	else if(in_cksum((unsigned short *)buffer, pdulen) != 0)
	{
		//printf("corrupted packet\n");
		exit(-1);
	}
        buffer[pdulen] = 0;
	// new socket for child
	newSocket = socket(AF_INET6,SOCK_DGRAM,0);
	addToPollSet(newSocket);
	removeFromPollSet(*socketNum);
	*socketNum = newSocket;

	if(in_cksum((unsigned short *)buffer, pdulen) != 0 || buffer[6] != 7)
	{
		// uses a recursive call if the checksum is bad or not a setup packet
		return setupClient(socketNum, client, window);
	}

	filename = (char *)&buffer[15];
	if((file = fopen(filename, "r")) == NULL)
	{
		status = 0;
	}
	else
	{
		status = 1;
	}
        pdulen = createPDU(response, 0, 8, &status, 1);
	windowSize = ntohl(*((uint32_t *)&buffer[7]));
	bufferSize = ntohl(*((uint32_t *)&buffer[11]));
	*window = createWindow(windowSize, bufferSize);
	if((pdulen = stopAndWait(response, pdulen, buffer, MAX_PDU_SIZE, newSocket/*socketNum*/, client, &serverAddrLen, 1, 10)) < 0)
	{
		perror("client error\n");
		exit(-1);
	}

	return file;
}

// reads from the given file * and sends it using sliding window
int sendFile(int socketNum, struct sockaddr_in6 *client,  FILE *in, SlidingWindow *window)
{
	uint8_t pdu[MAX_PDU_SIZE], payload[MAX_PDU_SIZE], response[MAX_PDU_SIZE];
	uint32_t pduLen, payloadLen, seqNumber = 0, responseVal, srej = 0, fullFlag = 0, tmp = sizeof(struct sockaddr_in6);
	int waitTime = 0, numTimes = 1, eof = 0;
	// until EOF
	while(1)
	{
		// window is open
		if(!eof && isOpen(*window))
		{
			if((payloadLen = fread(payload, 1, window->bufferSize, in)) < 0)
			{
				perror("server read\n");
				exit(-1);
			}
			else if(payloadLen < window->bufferSize)
			{
				// EOF CASE
				eof = 1;
			}
			pduLen = createPDU(pdu, seqNumber, 3, payload, payloadLen);
			addPDU(window, seqNumber++, payload, payloadLen);
			moveCurrent(window, seqNumber);
			waitTime = 0; 
			numTimes = 1;
		}
		// window is closed
		else if(!eof)
		{
			payloadLen = getPDU(window, window->lower, payload);
			pduLen = createPDU(pdu, window->lower, 3, payload, payloadLen);
			waitTime = 1;
			numTimes = 10;
			// check for a response before resending data
			if(pollCall(1000) > 0)
			{
				fullFlag = 1;
				payloadLen = recvfrom(socketNum, response, MAX_PDU_SIZE, 0, (struct sockaddr *)client, &tmp);
				if(in_cksum((unsigned short *)response, payloadLen) != 0){fullFlag = 0;}
			}
		}
		else
		{
			pduLen = createPDU(pdu, seqNumber, 9, payload, 0);
			waitTime = 1;
			numTimes = 10;
		}
		// handles response from client
		if(fullFlag || (pduLen = stopAndWait(pdu, pduLen, response, MAX_PDU_SIZE, socketNum, client, &tmp, waitTime, numTimes)) != -1)
		{
			fullFlag = 0;
			responseVal = ntohl(((uint32_t *)&response[7])[0]);
			if(eof && responseVal >= window->current && response[6] != 6)
			{
				return 0;
			}
			else if(response[6] == 5 && responseVal > window->lower)
			{	
				slideWindow(window, responseVal);
			}
			// SREJ
			else if(response[6] == 6)
			{
				// go into stop and wait with the SREJ
				srej = responseVal;
				while(srej < window->current)
				{
					// resend data selectively until RRs match current
					payloadLen = getPDU(window, srej, payload);
					pduLen = createPDU(pdu, srej, 3, payload, payloadLen);
					if((pduLen = stopAndWait(pdu, pduLen, response, MAX_PDU_SIZE, socketNum, client, &tmp, 1, 10)) < 0)
					{
						perror("server srej response\n");
						exit(-1);
					}

					responseVal = ntohl(((uint32_t *)&response[7])[0]);
					if(response[6] == 5 && responseVal > window->lower)
					{
						srej = responseVal;
						slideWindow(window, responseVal);
					}
					// SREJ
					else if(response[6] == 6 && responseVal > srej)
					{
						srej = responseVal;
						if(responseVal - 1 > window->lower)
						{
							slideWindow(window, responseVal - 1);
						}
					}
				}
			}
		}
		else if(waitTime > 0)
		{
			// if there was no response after 10 seconds when the window was full
			perror("client failed to respond\n");
			exit(-1);
		}
	}
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 4 || argc < 2)
	{
		fprintf(stderr, "Usage %s error-rate [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 3)
	{
		portNumber = atoi(argv[2]);
	}
	
	return portNumber;
}
