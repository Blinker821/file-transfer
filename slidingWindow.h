#ifndef __SLIDING_WINDOW__
#define __SLIDING_WINDOW__

int stopAndWait(uint8_t *sending, int len, uint8_t *recieving, int recvlen, 
                int socket, const struct sockaddr_in6 *server, uint32_t *s, 
                int waitTime, int numTimes, int resend);
int checkFile(int socketNum, struct sockaddr_in6 *server, int argc, char **argv);
int recvFile(int socketNum, struct sockaddr_in6 *server,  FILE *out, SlidingWindow *window);
FILE *setupClient(int *socketNum, struct sockaddr_in6 *client, SlidingWindow *window);
int sendFile(int socketNum, struct sockaddr_in6 *client,  FILE *out, SlidingWindow *window);

#endif