// Client side - UDP Code				    
// By Hugh Smith	4/1/2017		

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "pdu.h"
#include "cpe464.h"
#include "pollLib.h"
#include "rcopy.h"
#include "window.h"

#define MAXBUF 80


int main (int argc, char *argv[])
{
	int socketNum = 0;	
	struct sockaddr_in6 server;	// Supports 4 and 6 but requires IPv6 struct
	int portNumber = 0;
	FILE *out;
	SlidingWindow window;
	
	portNumber = checkArgs(argc, argv);
	sendErr_init(atof(argv[5]), 1, 1, 1, 1);
	socketNum = setupUdpClientToServer(&server, argv[6], portNumber);
	setupPollSet();
	addToPollSet(socketNum);
	// setup file transfer
	if(checkFile(socketNum, &server, argc, argv) == -1)
	{
		printf("Error: file %s not found on server.\n", argv[1]);
		exit(-1);
	}
	
	//setup 'window' and outfile
	if((out = fileSetup(argc, argv, &window)) == NULL)
	{
		printf("Error on open for output of file: %s", argv[2]);
		exit(-1);
	}
	recvFile(socketNum, &server, out, &window);
	
	close(socketNum);

	return 0;
}

int checkArgs(int argc, char * argv[])
{

	int portNumber = 0;

	/* check command line arguments  */
	if (argc != 8)
	{
		printf("usage: %s from-filename to-filename window-size buffer-size error-percent remote-machine remote-port \n", argv[0]);
		exit(1);
	}
	
	portNumber = atoi(argv[7]);
		
	return portNumber;
}

// runs setup of connection with the server, including filename request
// returns -1 on failure, others on a success
int checkFile(int socketNum, struct sockaddr_in6 *server, int argc, char **argv)
{
	uint8_t pdu[MAX_PDU_SIZE], response[MAX_PDU_SIZE], payload[110];
	int windowSize, bufferSize, nameLen, pduLen, rr = 0, i, resend = 0, pollRes;
	uint32_t serverLen = sizeof(struct sockaddr_in6);
	struct sockaddr_in6 server2 = *server;
	// Format the payload
	windowSize = atoi(argv[3]);
	bufferSize = atoi(argv[4]);
	nameLen = strlen(argv[1]);
	if (nameLen > 100)
	{
		perror("filename too long\n");
		exit(-1);
	}
	windowSize = htonl(windowSize);
	memcpy(payload, &windowSize, 4);
	bufferSize = htonl(bufferSize);
	memcpy(&payload[4], &bufferSize, 4);
	memcpy(&payload[8], argv[1], nameLen);
	pduLen = createPDU(pdu, 0, 7, payload, 8 + nameLen);
	// wait for a response up to 10 times
	for(i = 0; i < 11; i++)
	{
		if(i == 10){exit(-1);}
		if(resend){resend = 0;}
		else
		{
			pduLen = sendtoErr(socketNum, pdu, pduLen, 0, (struct sockaddr *)server, serverLen);
		}
		if((pollRes = pollCall(1000)) > 0)
		{
			pduLen = recvfrom(socketNum, response, MAX_PDU_SIZE, 0, (struct sockaddr *)&server2, &serverLen);
			if(in_cksum((unsigned short *)response, pduLen) == 0)
			{
				*server = server2;
				break;
			}
			else
			{
				i--;
				resend = 1;
			}
			
		}
	}
	// checks for correct flag and whether the server was successful
	if(response[6] != 8 && response[8] == 0)
	{
		return -1;
	}
	pduLen = createPDU(pdu, 1, 5, (uint8_t *)&rr, 4);


	sendtoErr(socketNum, pdu, pduLen, 0, (struct sockaddr *)server, serverLen);

	return 0;
}

// opens the output file and initializes the 'window' used to buffer out of order data
// returns the file pointer to outfile or NULL
FILE *fileSetup(int argc, char **argv, SlidingWindow *window)
{
	*window = createWindow(atoi(argv[3]), atoi(argv[4]));
	return fopen(argv[2], "w");
}

// recieves data and writes to disk following sliding window protocol
int recvFile(int socketNum, struct sockaddr_in6 *server, FILE *out, SlidingWindow *window)
{
	uint8_t pdu[MAX_PDU_SIZE], response[15];
	uint32_t seqNumber = 2, responseVal = 0, expectedSN = 0, recievedSN, tmp, x = sizeof(struct sockaddr_in6);
	int srejSent = 0, pollRes = 0, checksumRes = 0, waitTime = 10, numTimes = 1;
	int pduLen = createPDU(response, seqNumber++, 5, (uint8_t *)&responseVal, 4);
	// until EOF recieved
	while(1)
	{
		if(!srejSent && (pduLen = stopAndWait(response, pduLen, pdu, MAX_PDU_SIZE, socketNum, server, &x, waitTime, numTimes)) < 0)
		{
			perror("client recv\n");
			exit(-1);
		}
		else if(srejSent && (pollRes = pollCall(10000)) > 0)
		{
			pduLen = recvfrom(pollRes, pdu, MAX_PDU_SIZE, 0, (struct sockaddr *)server, &x);
			checksumRes = in_cksum((unsigned short *)pdu, pduLen);
		}
		else if(srejSent && pollRes < 0)
		{
			perror("client expect resend\n");
			exit(-1);
		}
		recievedSN = ntohl(((uint32_t *)pdu)[0]);
		if(checksumRes == 0 && pdu[6] == 3)
		{
			// expected sequence number
			if(recievedSN == expectedSN)
			{
				expectedSN++;
				if(fwrite(&pdu[7], 1, pduLen - 7, out) < 0)
				{
					perror("File write\n");
					exit(-1);
				}
				// loop to write data in window
				while(hasEntry(*window, expectedSN))
				{
					pduLen = getPDU(window, expectedSN, pdu);
					if(fwrite(pdu, pduLen, 1, out) < 0)
					{
						perror("File write\n");
						exit(-1);
					}
					expectedSN++;
				}
				moveCurrent(window, expectedSN);
				slideWindow(window, expectedSN);
				responseVal = expectedSN;
				tmp = htonl(responseVal);
				pduLen = createPDU(response, seqNumber++, 5, (uint8_t *)&tmp, 4);
				srejSent = 0;
			}
			// unexpected, > so out of order
			else if(recievedSN > expectedSN)
			{
				addPDU(window, recievedSN, &pdu[7], pduLen-7);
				responseVal = expectedSN;
				tmp = htonl(responseVal);
				pduLen = createPDU(response, seqNumber++, 6, (uint8_t *)&tmp, 4);
				if(!srejSent)
				{
					sendtoErr(socketNum, response, pduLen, 0, (struct sockaddr *)server, x);
					srejSent = 1;
				}
			}
			// unexpected, < so duplicate
			else
			{
				tmp = htonl(expectedSN);
				pduLen = createPDU(response, seqNumber++, 5, (uint8_t *)&tmp, 4);
				srejSent = 0;
			}
		}
		// initial RR is lost, respond with RR0
		else if(checksumRes == 0 && pdu[6] == 8)
		{
			pduLen = createPDU(response, seqNumber++, 5, (uint8_t *)&checksumRes, 4);
		}
		// ADD EOF CASE
		else if(pdu[6] == 9 && recievedSN == expectedSN)
		{
			pduLen = createPDU(response, seqNumber++, 10, NULL, 0);
			sendtoErr(socketNum, response, pduLen, 0, (struct sockaddr *)server, x);
			fflush(out);
			fclose(out);
			return 0;
		}
		else if(pdu[6] == 9)
		{
			tmp = htonl(expectedSN);
			pduLen = createPDU(response, seqNumber++, 6, (uint8_t *)&tmp, 4);
			srejSent = 0;
		}
	}
}
