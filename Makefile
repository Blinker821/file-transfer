# udpCode makefile
# written by Hugh Smith - Feb 2021

CC = gcc
CFLAGS = -g -Wall


SRC = utils/networks.c  utils/gethostbyname.c utils/safeUtil.c 
OBJS = utils/networks.o utils/gethostbyname.o utils/safeUtil.o pdu.c utils/pollLib.c slidingWindow.c
#
#uncomment next two lines if your using sendtoErr() library
LIBS += utils/libcpe464.2.21.a -lstdc++ -ldl
CFLAGS += -D__LIBCPE464_ -no-pie

all:  rcopy server

rcopy: rcopy.c $(OBJS) 
	$(CC) $(CFLAGS) -o rcopy rcopy.c window.c $(OBJS) $(LIBS)

server: server.c $(OBJS) 
	$(CC) $(CFLAGS) -o server server.c window.c $(OBJS) $(LIBS)

window: testWindow.c window.c
	$(CC) $(CFLAGS) -o window testWindow.c pdu.c window.c $(LIBS)

%.o: %.c *.h 
	gcc -c $(CFLAGS) $< -o $@ 

cleano:
	rm -f *.o

clean:
	rm -f server rcopy

