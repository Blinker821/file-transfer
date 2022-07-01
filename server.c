#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "utils/gethostbyname.h"
#include "utils/networks.h"
#include "utils/safeUtil.h"
#include "utils/cpe464.h"
#include "utils/pollLib.h"
#include "server.h"
#include "pdu.h"
#include "window.h"
#include "slidingWindow.h"

#define MAXBUF 80

void processClient(int socketNum);

int main ( int argc, char *argv[])
{ 
	int socketNum = 0;				
	int portNumber = 0, pid = 1;
	FILE *in = NULL;
	float errorRate;
	struct sockaddr_in6 client;
	SlidingWindow window;

	portNumber = checkArgs(argc, argv);
	errorRate = atof(argv[1]);

	sendErr_init(errorRate, 1, 1, 1, 1);
	setupPollSet();

	socketNum = udpServerSetup(portNumber);
	addToPollSet(socketNum);
	while(pid != 0)
	{
		pollCall(-1);
		pid = fork();
		if(pid == 0 && (in = setupClient(&socketNum, &client, &window)) == NULL)
		{
			perror("server setup return\n");
			exit(-1);
		}
	}	
	sendFile(socketNum, &client, in, &window);
	close(socketNum);
	return 0;
}



int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 4 || argc < 2)
	{
		fprintf(stderr, "Usage %s error-rate [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 3)
	{
		portNumber = atoi(argv[2]);
	}
	
	return portNumber;
}
