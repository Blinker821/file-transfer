#ifndef PDU_H
#define PDU_H

#define MAX_PDU_SIZE 1407

int createPDU(uint8_t *pduBuffer, uint32_t seqNumber, uint8_t flag, uint8_t *payload, int payloadLen);
void outputPDU(uint8_t * aPDU, int pduLength);

#endif