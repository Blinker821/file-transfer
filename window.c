#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "window.h"

// Allocates space for and initializes a new sliding window object
SlidingWindow createWindow(uint32_t windowSize, uint32_t bufferSize)
{
    SlidingWindow window = {
        windowSize,
        bufferSize,
        0, windowSize, 0,
        NULL, NULL};
    window.entryInfo = (WindowEntry *)calloc(windowSize, sizeof(WindowEntry));
    window.buffer = (uint8_t *)malloc(windowSize * bufferSize);
    return window;
}

// Adds a PDU to the table, creating an entry for it and copying data into the buffer
// does nothing if sequence is out of the window
void addPDU(SlidingWindow *window, uint32_t seqNumber, uint8_t *pdu, uint32_t pduLen)
{
    int index = seqNumber % window->windowSize;
    WindowEntry *entry = &window->entryInfo[index];
    if (seqNumber > window->upper || seqNumber < window->lower)
    {
        return;
    }
    entry->valid = 1;
    entry->seqNumber = seqNumber;
    entry->pduLen = pduLen;
    entry->pdu = &window->buffer[index * window->bufferSize];
    memcpy(entry->pdu, pdu, pduLen);
}

// copies the pdu of seqNumber into dstBuffer, returns -1 if it isn't available or the length of the PDU
int getPDU(SlidingWindow *window, uint32_t seqNumber, uint8_t *dstBuffer)
{
    int index = seqNumber % window->windowSize;
    WindowEntry *entry = &window->entryInfo[index];
    if (seqNumber > window->upper || seqNumber < window->lower ||
        entry->seqNumber != seqNumber || entry->valid == 0)
    {
        return -1;
    }
    memcpy(dstBuffer, entry->pdu, entry->pduLen);
    return entry->pduLen;
}

// changes current as long as it is less than upper
void moveCurrent(SlidingWindow *window, uint32_t newCurrent)
{
    if(newCurrent > window->upper)
    {
        return;
    }
    window->current = newCurrent;
}

// moves lower and upper, invalidating all entries that are slid over
void slideWindow(SlidingWindow *window, uint32_t newLower)
{
    int i;
    for(i = window->lower; i < newLower; i++)
    {
        window->entryInfo[i % window->windowSize].valid = 0;
    }
    window->lower = newLower;
    window->upper = newLower + window->windowSize;
}

// returns whether or not the window is open, current < upper
int isOpen(SlidingWindow window)
{
    return window.upper > window.current;
}

// returns whether or not the given sequence number is in the table
int hasEntry(SlidingWindow window, uint32_t seqNumber)
{
    WindowEntry *entry = &window.entryInfo[seqNumber % window.windowSize];
    return entry->valid && entry->seqNumber == seqNumber;
}

// prints metadata for a single entry
void printEntry(WindowEntry entry)
{
    if(entry.valid)
    {
        printf("sequenceNumber: %d pduSize: %d\n", entry.seqNumber, entry.pduLen);
    }
    else
    {
        printf("not valid\n");
    }
}

// prints metadata for a whole table
void printTableInfo(SlidingWindow window)
{
    printf("Server Window - Window Size: %d, Lower: %d, Upper: %d, Current: %d\n",
    window.windowSize, window.lower, window.upper, window.current);
}

// prints full table, not buffers
void printTable(SlidingWindow window)
{
    int i; 
    printf("Window Size is: %d\n", window.windowSize);
    for(i = 0; i < window.windowSize; i++)
    {
        printf("\t%d ", i);
        printEntry(window.entryInfo[i]);
    }
}
