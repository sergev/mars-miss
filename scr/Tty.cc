extern "C" {
	#ifdef TERMIOS
	    #include <sys/termios.h>
	    #include <sys/ioctl.h>
	    #define TERMIO
	    #define termio termios
	    #define TCGETA TIOCGETA
	    #define TCSETA TIOCSETA
	    #define TCSETAW TIOCSETAW
	    #define OLCUC 0
	    #define IUCLC 0
	    #define OCRNL 0
	    #define XCASE 0
	#else
	    #ifdef TERMIO
		#include <termio.h>
	    #else
		#include <sgtty.h>
	    #endif
	#endif
};

#include "Screen.h"

const int CHANNEL = 2;                  // output file descriptor
const int NOCHAR = 0;

struct TtyPrivate {
	#if defined (TERMIO) || defined (TERMIOS)
	struct termio oldtio, newtio;
	#else
	struct sgttyb tty;
	int ttyflags;
	#ifdef TIOCSETC
	struct tchars oldtchars, newtchars;
	#endif
	#endif

	#ifdef NEEDLITOUT
	int oldlocal, newlocal;
	#endif

	#ifdef TIOCSLTC
	struct ltchars oldchars, newchars;
	#endif
};

void Screen::SetTty ()
{
	if (! ttydata)
		ttydata = new TtyPrivate;
#ifdef TERMIO
	if (ioctl (CHANNEL, TCGETA, (char *) &ttydata->oldtio) < 0)
		return;
	if (ttydata->oldtio.c_oflag & OLCUC)
		UpperCaseMode = 1;      // uppercase on output
	ttydata->newtio = ttydata->oldtio;
	ttydata->newtio.c_iflag &= ~(INLCR | ICRNL | IGNCR | ISTRIP | IUCLC);
	ttydata->newtio.c_oflag &= ~(OLCUC | OCRNL);
	ttydata->newtio.c_lflag &= ~(ECHO | ICANON | XCASE);
	ttydata->newtio.c_cc [VMIN] = 1;         // break input after each character
	ttydata->newtio.c_cc [VTIME] = 1;        // timeout is 100 msecs
	ttydata->newtio.c_cc [VINTR] = NOCHAR;
	ttydata->newtio.c_cc [VQUIT] = NOCHAR;
#ifdef VSWTCH
	ttydata->newtio.c_cc [VSWTCH] = NOCHAR;
#endif
#ifdef VDSUSP
	ttydata->newtio.c_cc [VDSUSP] = NOCHAR;
#endif
#ifdef VLNEXT
	ttydata->newtio.c_cc [VLNEXT] = NOCHAR;
#endif
#ifdef VDISCARD
	ttydata->newtio.c_cc [VDISCARD] = NOCHAR;
#endif
	ioctl (CHANNEL, TCSETAW, (char *) &ttydata->newtio);
#else
	if (gtty (CHANNEL, &ttydata->tty) < 0)
		return;
	if (ttydata->tty.sg_flags & LCASE)
		UpperCaseMode = 1;      // uppercase on output
	ttydata->ttyflags = ttydata->tty.sg_flags;
	ttydata->tty.sg_flags &= ~(XTABS | ECHO | CRMOD | LCASE);
#   ifdef CBREAK
	ttydata->tty.sg_flags |= CBREAK;
#   endif
#   ifdef NEEDLITOUT
	ttydata->tty.sg_flags |= LITOUT;
#   endif
	stty (CHANNEL, &ttydata->tty);
#   ifdef TIOCSETC
	ioctl (CHANNEL, TIOCGETC, (char *) &ttydata->oldtchars);
	ttydata->newtchars = ttydata->oldtchars;
	ttydata->newtchars.t_intrc = NOCHAR;
	ttydata->newtchars.t_quitc = NOCHAR;
	ttydata->newtchars.t_eofc = NOCHAR;
	ttydata->newtchars.t_brkc = NOCHAR;
	ioctl (CHANNEL, TIOCSETC, (char *) &ttydata->newtchars);
#   endif
#endif // TERMIO
#ifdef NEEDLITOUT
	ioctl (CHANNEL, TIOCLGET, (char *) &ttydata->oldlocal);
	ttydata->newlocal = ttydata->oldlocal | LLITOUT;
	ioctl (CHANNEL, TIOCLSET, (char *) &ttydata->newlocal);
#endif
#ifdef TIOCSLTC
	ioctl (CHANNEL, TIOCGLTC, (char *) &ttydata->oldchars);
	ttydata->newchars = ttydata->oldchars;
	ttydata->newchars.t_lnextc = NOCHAR;
	ttydata->newchars.t_rprntc = NOCHAR;
	ttydata->newchars.t_dsuspc = NOCHAR;
	ttydata->newchars.t_flushc = NOCHAR;
	ioctl (CHANNEL, TIOCSLTC, (char *) &ttydata->newchars);
#endif
}

void Screen::ResetTty ()
{
	if (! ttydata)
		return;
#ifdef TERMIO
	ioctl (CHANNEL, TCSETA, (char *) &ttydata->oldtio);
#else
	ttydata->tty.sg_flags = ttydata->ttyflags;
	stty (CHANNEL, &ttydata->tty);
#   ifdef TIOCSETC
	ioctl (CHANNEL, TIOCSETC, (char *) &ttydata->oldtchars);
#   endif
#endif
#ifdef NEEDLITOUT
	ioctl (CHANNEL, TIOCLSET, (char *) &ttydata->oldlocal);
#endif
#ifdef TIOCSLTC
	ioctl (CHANNEL, TIOCSLTC, (char *) &ttydata->oldchars);
#endif
}

void Screen::FlushTtyInput ()
{
	if (! ttydata)
		return;
#ifdef TCFLSH
	ioctl (CHANNEL, TCFLSH, 0);
#else
#   ifdef TIOCFLUSH
	int p = 1;

	ioctl (CHANNEL, TIOCFLUSH, (char *) &p);
#   endif
#endif
}
