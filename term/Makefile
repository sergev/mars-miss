# For Interactive Unix 386
#CC      = g++ -g -O -fstrength-reduce -finline-functions
#LIBS    = -linet -lcposix
#CFLAGS  = -I../client -I../scr

# For Xenix 386
#CC      = g++ -O -fstrength-reduce -finline-functions #-g0
#LIBS    = -lsocket
#CFLAGS  = -I../client -I../scr

# For Ultrix
#CC      = g++ -pipe -O -fstrength-reduce -finline-functions #-g0
#CFLAGS  = -I../client -I../scr -DSPRINTF_RETURNS_INT

# For 386bsd
#CC      = g++ -pipe -O -fstrength-reduce -finline-functions #-g0
#CFLAGS  = -I../client -I../scr

# For Mac OSX
CC      = g++ -O
CFLAGS  = -I../client -I../scr
LIBS    = -ltermcap

SHELL   = /bin/sh
OBJS    = term.o
PROG    = marsterm
LIBMARS = ../client/libmars.a
LIBSCR  = ../scr/libscr.a

$(PROG): $(OBJS) $(LIBMARS) $(LIBSCR)
	$(CC) $(LDFLAGS) -o $(PROG) $(OBJS) $(LIBMARS) $(LIBSCR) $(LIBS)

$(LIBMARS):
	cd ../client; make libmars.a

$(LIBSCR):
	cd ../scr; make libscr.a

clean:
	rm -f *.[boa] $(PROG) .,*

.SUFFIXES: .cc

.cc.o:
	$(CC) $(CFLAGS) -c $<

##
term.o: ../scr/Screen.h ../client/mars.h
