# For Interactive Unix 386
#CC      = gcc -O -fstrength-reduce -finline-functions
#LIBS    = -linet -lcposix
#CFLAGS  = -I../client

# For Xenix 386
#CC      = gcc -O -fstrength-reduce -finline-functions
#LIBS    = -lsocket
#CFLAGS  = -I../client

# For Ultrix
#CC      = gcc -pipe -O -fstrength-reduce -finline-functions
#CFLAGS  = -I../client -DNO_STDLIB

# For 386bsd
#CC      = cc -pipe -O -fstrength-reduce -finline-functions
#CFLAGS  = -I../client

# For 386bsd
CC      = cc -O
CFLAGS  = -I../client

SHELL   = /bin/sh
OBJS    = main.o input.o output.o rnumb.o
PROG    = marsreport
LIBMARS = ../client/libmars.a
YFLAGS  =

all:    $(PROG)

$(PROG): $(OBJS) $(LIBMARS)
	$(CC) $(LDFLAGS) -o $(PROG) $(OBJS) $(LIBMARS) $(LIBS)

$(LIBMARS):
	cd ../client; make libmars.a

clean:
	rm -f *.o .,* *.b $(PROG)

main.o input.o output.o: report.h
