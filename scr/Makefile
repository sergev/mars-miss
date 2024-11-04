SHELL   = /bin/sh
LD      = g++ #-g0
SRCS    = Screen.cc Tty.cc Key.cc Box.cc Popup.cc Menu.cc curses.c Termgoto.cc $(CAPSRC)
OBJS    = Screen.o  Tty.o  Key.o  Box.o  Popup.o  Menu.o  curses.o Termgoto.o  $(CAPOBJ)
CAPSRC  = Termcap.cc
CAPOBJ  = Termcap.o
LIB     = libscr.a

# For Interactive Unix 386
#-------------------------
#CC      = gcc -DISC -O -fstrength-reduce -finline-functions #-g0
#CPLUS   = g++ -DISC -O -fstrength-reduce -finline-functions #-g0
#CFLAGS  = -DTERMIO

# For Xenix 386
#--------------
#CC      = gcc -O -fstrength-reduce -finline-functions #-g0
#CPLUS   = g++ -O -fstrength-reduce -finline-functions #-g0
#CFLAGS  = -DTERMIO

# For Ultrix
#-----------
#CC      = gcc -pipe -O -fstrength-reduce -finline-functions #-g0
#CPLUS   = g++ -pipe -O -fstrength-reduce -finline-functions #-g0
#CFLAGS  =

# For 386bsd
#-----------
#CPLUS   = g++ -pipe -Wall -g0
#CC      = gcc -pipe -Wall -g
CPLUS   = g++ -pipe -Wall -O -fstrength-reduce -finline-functions
CC      = gcc -pipe -Wall -O -fstrength-reduce -finline-functions
CFLAGS  = -DTERMIOS

# For Mac OSX
#------------
CPLUS   = c++ -pipe -Wall -O
CC      = cc -pipe -Wall -O
CFLAGS  = -DTERMIOS

# Use standard libtermcap, instead of internal termcap library
#-------------------------------------------------------------
CAPSRC  = Termlib.cc
CAPOBJ  = Termlib.o
LIBS    = -ltermcap

$(LIB): $(OBJS)
	[ "$?" != "" ] && ar r $(LIB) $? && ranlib $(LIB)

tests:  hello hv hellcurs

hello:  hello.o $(LIB)
	$(CPLUS) $(CFLAGS) hello.o -o hello $(LIB) $(LIBS)

hellcurs: hellcurs.o $(LIB)
	$(CPLUS) $(CFLAGS) hellcurs.o -o hellcurs $(LIB) $(LIBS)

hv:     hv.o $(LIB)
	$(CPLUS) $(CFLAGS) hv.o -o hv $(LIB) $(LIBS)

depend:
	mkdep $(CFLAGS) *.cc &&\
	sed '/^###$$/,$$d' Makefile > Makefile~ &&\
	echo '###' >> Makefile~ &&\
	cat .depend >> Makefile~ &&\
	rm .depend &&\
	mv Makefile~ Makefile

clean:
	rm -f *.o *.b *.a .,* core a.out hello hv hellcurs

.SUFFIXES: .cc

.cc.o:
	$(CPLUS) $(CFLAGS) -c $<

###
Box.o: Box.cc Screen.h extern.h
Key.o: Key.cc Screen.h Termcap.h extern.h
Menu.o: Menu.cc Screen.h Menu.h extern.h
Popup.o: Popup.cc Screen.h extern.h
Screen.o: Screen.cc Screen.h Termcap.h extern.h
Termcap.o: Termcap.cc Screen.h Termcap.h extern.h
Tty.o: Tty.cc Screen.h
hv.o: hv.cc Screen.h
hello.o: hello.cc Screen.h
curses.o: curses.h Screen.h
hellcurs.o: curses.h
