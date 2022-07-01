#ifndef PDU_H
#define PDU_H

#define MAX_PDU_SIZE 1407

int createPDU(uint8_t *pduBuffer, uint32_t seqNumber, uint8_t flag, uint8_t *payload, int payloadLen);
void outputPDU(uint8_t * aPDU, int pduLength);
int stopAndWait(uint8_t *sending, int len, uint8_t *recieving, int recvlen, 
                int socket, const struct sockaddr_in6 *server, uint32_t *s, int waitTime, int numTimes);

#endif