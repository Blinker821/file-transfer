#ifndef __SERVER__
#define __SERVER__

#include "window.h"

int checkArgs(int argc, char *argv[]);
FILE *setupClient(int *socketNum, struct sockaddr_in6 *client, SlidingWindow *window);
int sendFile(int socketNum, struct sockaddr_in6 *client,  FILE *out, SlidingWindow *window);

#endif