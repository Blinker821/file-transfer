#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "utils/gethostbyname.h"
#include "utils/networks.h"
#include "utils/safeUtil.h"
#include "utils/cpe464.h"
#include "utils/pollLib.h"
#include "pdu.h"
#include "rcopy.h"
#include "window.h"
#include "slidingWindow.h"

#define MAXBUF 80


int main (int argc, char *argv[])
{
	int socketNum = 0;	
	struct sockaddr_in6 server;	// Supports 4 and 6 but requires IPv6 struct
	int portNumber = 0;
	FILE *out;
	SlidingWindow window;
	
	portNumber = checkArgs(argc, argv);
	sendErr_init(atof(argv[5]), 1, 1, 1, 1);
	socketNum = setupUdpClientToServer(&server, argv[6], portNumber);
	setupPollSet();
	addToPollSet(socketNum);
	// setup file transfer
	if(checkFile(socketNum, &server, argc, argv) == -1)
	{
		printf("Error: file %s not found on server.\n", argv[1]);
		exit(-1);
	}
	
	//setup 'window' and outfile
	window = createWindow(atoi(argv[3]), atoi(argv[4]));
	if((out = fopen(argv[2], "w")) == NULL)
	{
		printf("Error on open for output of file: %s", argv[2]);
		exit(-1);
	}
	recvFile(socketNum, &server, out, &window);
	
	close(socketNum);

	return 0;
}

int checkArgs(int argc, char * argv[])
{

	int portNumber = 0;

	/* check command line arguments  */
	if (argc != 8)
	{
		printf("usage: %s from-filename to-filename window-size buffer-size error-percent remote-machine remote-port \n", argv[0]);
		exit(1);
	}
	
	portNumber = atoi(argv[7]);
		
	return portNumber;
}

