# For Interactive Unix 386
#CC      = gcc -O -fstrength-reduce -finline-functions
#CFLAGS  = -I../client
#LIBS    = -linet -lcposix

# For Xenix 386
#CC      = gcc -O -fstrength-reduce -finline-functions
#CFLAGS  = -I../client -DLAI_TCP -DNO_MKFIFO
#LIBS    = -lsocket

# For Ultrix
#CC      = gcc -pipe -O -fstrength-reduce -finline-functions
#CFLAGS  = -I../client -DUSE_UNIX_DOMAIN -DNO_SUN_LEN

# For 386bsd
CC      = cc -pipe -O -fstrength-reduce -finline-functions
CFLAGS  = -I../client -DUSE_UNIX_DOMAIN

SHELL   = /bin/sh
OBJS    = proto.o
PROG    = marsproto

all:    $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROG) $(OBJS) $(LIBS)

clean:
	rm -f *.[boa] $(PROG) .,*
