#ifndef WINDOW

#include <stdio.h>

#define TRUE            1
#define FALSE           0

#define A_REVERSE       0x1
#define A_UNDERLINE     0x2
#define A_BOLD          0x4
#define A_DIM           0x8

#define WINDOW void

#define getch curses_getch /* to avoid name conflict with msdos getch() */

extern int LINES, COLS;

int _gety (void), _getx (void);

WINDOW *initscr (void);
int move (int y, int x);
int addch (int c);
int addstr (char *str);
int attrset (int a);
int attroff (int a);
int refresh (void);
int endwin (void);
int getch (void);
int clrtobot (void);
int deleteln (void);
int erase (void);
int inch (void);
int insertln (void);

#define curscr          ((WINDOW*) 0)
#define stdscr          ((WINDOW*) 0)
#define standout()      attrset(A_REVERSE)
#define standend()      attroff(A_REVERSE)
#define echo()          0       /* void */
#define noecho()        0       /* void */
#define cbreak()        0       /* void */
#define nocbreak()      0       /* void */
#define nl()            0       /* void */
#define nonl()          0       /* void */
#define raw()           0       /* void */
#define noraw()         0       /* void */
#define setterm(name)   0       /* void */
#define scrollok(w,on)  0       /* void */
#define ttytype         "tty"
#define getyx(w,y,x)    (y=_gety(),x=_getx())   /* w should be stdscr */

#define mvaddch(y,x,c)          (move(y,x), addch(c))
#define mvaddstr(y,x,str)       (move(y,x), addstr(str))
#define mvgetch(y,x)            (move(y,x), getch())
#define mvinch(y,x)             (move(y,x), inch())

#endif
