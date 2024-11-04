#include "curses.h"

int main ()
{
	initscr ();
	mvaddstr (LINES/2-3, COLS/2 - 9, "Hello, World!");
	attrset (A_REVERSE);
	mvaddstr (LINES/2-2, COLS/2 - 8, "Hello, World!");
	attrset (A_BOLD);
	mvaddstr (LINES/2-1, COLS/2 - 7, "Hello, World!");
	attrset (A_REVERSE | A_BOLD);
	mvaddstr (LINES/2+0, COLS/2 - 6, "Hello, World!");
	attrset (A_DIM);
	mvaddstr (LINES/2+1, COLS/2 - 5, "Hello, World!");
	attrset (A_REVERSE | A_DIM);
	mvaddstr (LINES/2+2, COLS/2 - 4, "Hello, World!");
	attrset (0);
	refresh ();
	getch ();
	move (LINES-1, 0);
	refresh ();
	endwin ();
	return (0);
}
