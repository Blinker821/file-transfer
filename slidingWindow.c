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

#include "utils/gethostbyname.h"
#include "utils/networks.h"
#include "utils/safeUtil.h"
#include "utils/cpe464.h"
#include "utils/pollLib.h"
#include "pdu.h"
#include "rcopy.h"
#include "window.h"
#include "slidingWindow.h"

// sends the given packet in a stop and wait manner, polling for waitTime each sending
// copies the response into recieving
// returns the pdu length on a success, defined as recieving any packet with a valid checksum
// returns -1 on an error or if nothing valid is recieved in numTimes * waitTime
int stopAndWait(uint8_t *sending, int len, uint8_t *recieving, int recvlen, 
                int socket, const struct sockaddr_in6 *server, uint32_t *s, int waitTime, int numTimes)
{
    int pollResult, pduLen = 0, resend = 0, i;
    for(i = 0; i < numTimes; i++)
    {
        if(!resend && sendtoErr(socket, sending, len, 0, (struct sockaddr *)server, *s) < 0)
        {
            return -1;
        }
        else if(resend)
        {
            resend = 0;
        }
        if((pollResult = pollCall(waitTime * 1000)) > 0)
        {
            if((pduLen = recvfrom(pollResult, recieving, recvlen, 0, (struct sockaddr *)server, s)) < 0)
            {
                perror("bad recv stop and wait\n");
                exit(-1);
            }
            // check checksum
            if(in_cksum((unsigned short *)recieving, pduLen) == 0)
            {
                return pduLen;
            }
            // otherwise treat as lost data and continue
            i--;
            resend = 1;
        }
    }
    return -1;
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
	// REPLACE with modified stopAndWait
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
	if((pdulen = stopAndWait(response, pdulen, buffer, MAX_PDU_SIZE, newSocket, client, &serverAddrLen, 1, 10)) < 0)
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
