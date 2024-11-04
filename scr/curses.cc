#include <stdlib.h>
#include <string.h>
#if defined(unix) || defined(__APPLE__)
#   include <unistd.h>
#else
#   include <io.h>
#endif
#include "Screen.h"

static Screen *V;
static int color, attr;
static int normalColor, boldColor, dimColor;
static int inverseColor, inverseBoldColor, inverseDimColor;

static void fatal ()
{
	char *msg = "curses not initialized\n";
	write (2, msg, strlen (msg));
	exit (-1);
}

extern "C" {
#include "curses.h"

int COLS, LINES;

int _getx ()
{
	if (! V)
		fatal ();
	return (V->Col ());
}

int _gety ()
{
	if (! V)
		fatal ();
	return (V->Row ());
}

int addch (int c)
{
	if (! V)
		fatal ();
	V->Put (c, color);
	return (TRUE);
}

int addstr (char *str)
{
	if (! V)
		fatal ();
	V->Put (str, color);
	return (TRUE);
}

int clear ()
{
	if (! V)
		fatal ();
	V->Clear (color);
	return (TRUE);
}

int refresh ()
{
	if (! V)
		fatal ();
	V->Sync ();
	return (TRUE);
}

int getch ()
{
	if (! V)
		fatal ();
	return (V->GetKey ());
}

int move (int y, int x)
{
	if (! V)
		fatal ();
	V->Move (y, x);
	return (TRUE);
}

int endwin ()
{
	if (! V)
		fatal ();
	delete V;
	V = 0;
	return (TRUE);
}

WINDOW *initscr ()
{
	V = new Screen ();
	LINES = V->Lines;
	COLS = V->Columns;
	if (V->HasColors ()) {
		normalColor             = 0x1b /*0x70*/;
		boldColor               = 0x1f /*0x71*/;
		dimColor                = 0x1e /*0x7c*/;
		inverseColor            = 0x7b /*0x1b*/;
		inverseBoldColor        = 0x7f /*0x1f*/;
		inverseDimColor         = 0x7e /*0x13*/;
	} else {
		normalColor             = 0x07;
		boldColor               = 0x0f;
		dimColor                = 0x07;
		inverseColor            = 0x1f;
		inverseBoldColor        = 0x1f;
		inverseDimColor         = 0x10;
	}
	attrset (0);
	clear ();
	return (stdscr);
}

int printw (char *fmt, ...)
{
	if (! V)
		fatal ();

        va_list ap;
        va_start(ap, fmt);
	V->PrintVect (color, fmt, ap);
        va_end(ap);
	return (TRUE);
}

int attrset (int a)
{
	if (a & A_BOLD)
		color = (a & A_REVERSE) ? inverseBoldColor : boldColor;
	else if (a & A_DIM)
		color = (a & A_REVERSE) ? inverseDimColor : dimColor;
	else
		color = (a & A_REVERSE) ? inverseColor : normalColor;
	attr = a;
	return (TRUE);
}

int attroff (int a)
{
	if (a & (A_BOLD | A_DIM)) {
		attr &= ~(A_BOLD | A_DIM);
		if (a & A_REVERSE) {
			color = normalColor;
			attr &= ~A_REVERSE;
		} else
			color = (attr & A_REVERSE) ? inverseColor : normalColor;
	} else if (a & A_REVERSE) {
		attr &= ~A_REVERSE;
		if (attr & A_BOLD)
			color = boldColor;
		else if (attr & A_DIM)
			color = dimColor;
		else
			color = normalColor;
	}
	return (TRUE);
}

int erase ()
{
	if (! V)
		fatal ();
	V->Clear (0, 0, V->Lines, V->Columns, color);
	return (TRUE);
}

int deleteln ()
{
	if (! V)
		fatal ();
	V->DelLine (V->Row (), color);
	return (TRUE);
}

int insertln ()
{
	if (! V)
		fatal ();
	V->InsLine (V->Row (), color);
	return (TRUE);
}

int inch ()
{
	if (! V)
		fatal ();
	return (V->GetChar (V->Row (), V->Col ()));
}

int clrtoeol ()
{
	if (! V)
		fatal ();
	V->ClearLine (color);
	return (TRUE);
}

int clrtobot ()
{
	if (! V)
		fatal ();
	V->ClearLine (color);
	for (int r=V->Row()+1; r<V->Columns; ++r)
		V->ClearLine (r, 0, color);
	return (TRUE);
}

}; /* extern "C" */
