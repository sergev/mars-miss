SHELL           = /bin/sh
CC              = cc -pipe -g
CFLAGS          = -Wall -I../mars11/client
LIBS            = ../mars11/client/libmars.a

newsinject:     inject.o rfcdate.o header.o
		$(CC) $(LDFLAGS) inject.o rfcdate.o header.o $(LIBS) -o newsinject

##
header.o: header.h
inject.o: header.h
