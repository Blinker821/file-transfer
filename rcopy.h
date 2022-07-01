#ifndef __RCOPY__
#define __RCOPY__

#include "window.h"

// initialize connection
// transfer data
// close connection
int checkArgs(int argc, char * argv[]);
int checkFile(int socketNum, struct sockaddr_in6 *server, int argc, char **argv);
FILE *fileSetup(int argc, char **argv, SlidingWindow *window);
int recvFile(int socketNum, struct sockaddr_in6 *server,  FILE *out, SlidingWindow *window);

#endif