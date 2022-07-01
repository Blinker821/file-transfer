#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>

typedef struct
{
    uint8_t valid; // 0 if invalid, valid otherwise
    uint32_t seqNumber, pduLen;
    uint8_t *pdu; //will point to the array malloc'd by the table
} WindowEntry;

typedef struct
{
    uint32_t windowSize, bufferSize, lower, upper, current; // window size = number of entries, buffersize = memory of each entry
    WindowEntry *entryInfo;
    uint8_t *buffer;
} SlidingWindow;

SlidingWindow createWindow(uint32_t windowSize, uint32_t bufferSize);
void addPDU(SlidingWindow *window, uint32_t seqNumber, uint8_t *pdu, uint32_t pduLen);
int getPDU(SlidingWindow *window, uint32_t seqNumber, uint8_t *dstBuffer);
void moveCurrent(SlidingWindow *window, uint32_t newCurrent);
void slideWindow(SlidingWindow *window, uint32_t newLower);
int isOpen(SlidingWindow window);
int hasEntry(SlidingWindow window, uint32_t seqNumber);

void printEntry(WindowEntry entry);
void printTableInfo(SlidingWindow window);
void printTable(SlidingWindow window);

#endif