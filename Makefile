SHELL   = /bin/sh
SUBDIRS = server client proto sample exec report scr term

all:
	@for dir in $(SUBDIRS); do (cd $$dir;\
		echo Making all in $$dir...; $(MAKE)); done

clean:
	rm -f .,* *.b */.,*
	@for dir in $(SUBDIRS); do (cd $$dir;\
		echo Cleaning $$dir...; $(MAKE) clean); done
