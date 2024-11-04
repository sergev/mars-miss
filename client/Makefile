# For Interactive Unix 386
#CC      = gcc -O -fstrength-reduce -finline-functions
#CFLAGS  = -I.

# For Xenix 386
#CC      = gcc -O -fstrength-reduce -finline-functions
#CFLAGS  = -I. -DLAI_TCP

# For Ultrix
#CC      = gcc -pipe -O -fstrength-reduce -finline-functions
#CFLAGS  = -I. -DUSE_FIFO_FILES -DUSE_UNIX_DOMAIN -DNO_STDLIB -DNO_SUN_LEN

# For 386bsd
#CC      = cc -pipe -O -fstrength-reduce -finline-functions
#CFLAGS  = -I. -DUSE_UNIX_DOMAIN

# For Mac OS
CC      = cc -O
CFLAGS  = -I. -DUSE_UNIX_DOMAIN

SHELL   = /bin/sh
LIBMARS = libmars.a
OBJS    = commit.o compile.o connect.o error.o exec.o fetch.o kill.o lexec.o\
	  lfetch.o lget.o lput.o

all:    $(LIBMARS)

$(LIBMARS): $(OBJS)
	ar r $(LIBMARS) $?
	ranlib $(LIBMARS)

clean:
	rm -f *.[boa] $(ALL) .,*

$(OBJS): mars.h intern.h
