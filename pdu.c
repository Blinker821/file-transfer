#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils/cpe464.h"
#include "utils/pollLib.h"
#include "utils/checksum.h"
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
