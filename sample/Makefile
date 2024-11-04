# For Interactive Unix 386
#CC      = gcc -O -fstrength-reduce -finline-functions
#LIBS    = -linet -lcposix

# For Xenix 386
#CC      = gcc -O -fstrength-reduce -finline-functions
#LIBS    = -lsocket

# For Ultrix
#CC      = gcc -pipe -O -fstrength-reduce -finline-functions

# For 386bsd
CC      = cc -pipe -O -fstrength-reduce -finline-functions

SHELL   = /bin/sh
CFLAGS  = -I../client
OBJS    = sample.o
PROG    = marssample
LIBMARS = ../client/libmars.a

all:    $(PROG)

$(PROG): $(OBJS) $(LIBMARS)
	$(CC) $(LDFLAGS) -o $(PROG) $(OBJS) $(LIBMARS) $(LIBS)

$(LIBMARS):
	cd ../client; make libmars.a

clean:
	rm -f *.[boa] $(PROG) .,*
