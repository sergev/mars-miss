# For Interactive Unix 386
#CC      = gcc -O -fstrength-reduce -finline-functions
#CFLAGS  = -I../client
#LIBS    = -linet -lcposix

# For Xenix 386
#CC      = gcc -O -fstrength-reduce -finline-functions
#CFLAGS  = -I../client
#LIBS    = -lsocket

# Ultrix
#CC      = gcc -pipe -O -fstrength-reduce -finline-functions
#CFLAGS  = -I../client -DNO_STDLIB -DNO_STRDUP

# For 386bsd
CC      = cc -pipe -O -fstrength-reduce -finline-functions
CFLAGS  = -I../client

SHELL   = /bin/sh
OBJS    = gram.o exec.o format.o print.o
PROG    = marsexec
LIBMARS = ../client/libmars.a
YFLAGS  =

all:    $(PROG)

$(PROG): $(OBJS) $(LIBMARS)
	$(CC) $(LDFLAGS) -o $(PROG) $(OBJS) $(LIBMARS) $(LIBS)

$(LIBMARS):
	cd ../client; make libmars.a

clean:
	rm -f *.[boa] $(PROG) .,* y.output y.tab.[ch] testformat

testformat: format.c format.h
	$(CC) $(LDFLAGS) $(CFLAGS) -DDEBUGFORMAT -o testformat format.c

format.o exec.o: format.h
