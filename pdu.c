#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "cpe464.h"
#include "pollLib.h"
#include "checksum.h"
#include "pdu.h"

// Formats given data into a valid pdu
int createPDU(uint8_t *pduBuffer, uint32_t seqNumber, uint8_t flag, uint8_t *payload, int payloadLen)
{
    int pduLen = 0;
    uint16_t checksumResult;
    // copy all data things
    seqNumber = htonl(seqNumber);
    memcpy(pduBuffer, &seqNumber, 4);
    pduLen += 4;
    pduBuffer[4] = 0;
    pduBuffer[5] = 0;
    pduLen += 2;
    pduBuffer[6] = flag;
    pduLen++;
    memcpy(&pduBuffer[7], payload, payloadLen);
    pduLen += payloadLen;

    // checksum
    checksumResult = in_cksum((unsigned short *)pduBuffer, pduLen);
    memcpy(&pduBuffer[4], &checksumResult, 2);

    return pduLen;
}

// prints a formatted pdu to stdout
void outputPDU(uint8_t * aPDU, int pduLength)
{
    uint16_t checksumResult = in_cksum((unsigned short *)aPDU, pduLength);
    uint32_t seqNumber;
    uint8_t flag;
    int i;
    if(checksumResult != 0)
    {
        printf("Bad checksum!\n");
        return;
    }
    memcpy(&seqNumber, aPDU, 4);
    seqNumber = ntohl(seqNumber);
    flag = aPDU[6];
    printf("Good checksum!\n");
    printf("n = %d, flag = %d, payload length = %d, payload = ", seqNumber, flag, pduLength - 7);
    //printf("\n%s\n", &aPDU[7]);
    for(i = 7; i < pduLength; i++)
    {
        if((i - 7) % 50 == 0)
        {
            printf("\nloc %d| 0x ", i - 7);
        }
        printf("%x ", aPDU[i]);
    }
    printf("\n");
}

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