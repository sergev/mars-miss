#include "Screen.h"
#include <dos.h>
#include <io.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <stdio.h>

static unsigned char linedraw [11] = {
	0xda, 0xc2, 0xbf, 0xc3, 0xc5, 0xb4, 0xc0, 0xc1, 0xd9, 0xb3, 0xc4,
};

inline void outerr (char *str)
{
	write (2, str, strlen (str));
}

inline void set (short far *p, int val, unsigned len)
{
	while (len--)
		*p++ = val;
}

inline void move (short far *to, short far *from, unsigned len)
{
	while (len--)
		*to++ = *from++;
}

// Some ideas of dos bios interface are taken from
// sources of elvis editor, by Guntram Blohm.

Screen::Screen (int colormode, int graphmode)
{
	// Determine number of screen columns. Also set attrset according
	// to monochrome/color screen.
	union REGS regs;
	regs.h.ah = 0x0f;
	int86 (0x10, &regs, &regs);
	Columns = regs.h.ah;

	// Check for monochrome mode.
	ColorMode = (regs.h.al != 7);

	// Getting the number of rows is hard. Most screens support 25 only,
	// EGA/VGA also support 43/50 lines, and some OEM's even more.
	// Unfortunately, there is no standard bios variable for the number
	// of lines, and the bios screen memory size is always rounded up
 	// to 0x1000. So, we'll really have to cheat.
 	// When using the screen memory size, keep in mind that each character
 	// byte has an associated attribute byte.
 	//
 	// Uses: word at 40:4c contains memory size
 	//       byte at 40:84 # of rows-1 (sometimes)
	//       byte at 40:4a # of columns

	// Screen size less then 4K? then we have 25 lines only.
	if (*(int far *) (0x40004cL) <= 4096)
		Lines = 25;

	// VEGA vga uses the bios byte at 0x40:0x84 for # of rows.
	// Use that byte, if it makes sense together with memory size.
	else if ((((*(unsigned char far *) 0x40004aL * 2 *
	    (*(unsigned char far *) 0x400084L + 1)) + 0xfff) & (~0xfff)) ==
	    *(unsigned int far *) 0x40004cL)
		Lines = *(unsigned char far *) 0x400084L + 1;

	// Oh oh. Emit '\n's until screen starts scrolling.
	else {
		// Move to top of screen.
		regs.x.ax = 0x200;
		regs.x.bx = 0;
		regs.x.dx = 0;
		int86 (0x10, &regs, &regs);

		int oldline = 0;
		for (;;) {
			// Write newline to screen.
			regs.x.ax = 0xe0a;
			regs.x.bx = 0;
			int86 (0x10, &regs, &regs);

			// Read cursor position.
			regs.x.ax = 0x300;
			regs.x.bx = 0;
			int86 (0x10, &regs, &regs);
			int line = regs.h.dh;

			if (oldline == line) {
				Lines = line + 1;
				break;
			}
			oldline = line;
		}
	}
	scr = new short far* [Lines];
	if (! scr) {
		outerr ("Out of memory in Screen constructor\n");
		exit (1);
	}
	scr[0] = (short far *) 0xb8000000L;
	for (int y=1; y<Lines; ++y)
		scr[y] = scr[y-1] + Columns;

	flgs = NormalAttr = 0x0700;
	hidden = 0;
	ungetflag = 0;
	ncallb = 0;

	ULC           = '\20';
	UT            = '\21';
	URC           = '\22';
	CLT           = '\23';
	CX            = '\24';
	CRT           = '\25';
	LLC           = '\26';
	LT            = '\27';
	LRC           = '\30';
	VERT          = '\31';
	HOR           = '\32';

	Clear ();
}

Screen::~Screen ()
{
	// Set cursor position to 0,0.
	if (curx<0 || curx>=Columns)
		curx = 0;
	if (cury<0 || cury>=Lines)
		cury = 0;
	union REGS regs;
	regs.x.ax = 0x200;
	regs.x.bx = 0;
	regs.x.dx = cury << 8 | (unsigned char) curx;
	int86 (0x10, &regs, &regs);
	delete scr;
}

void Screen::Clear (int attr)
{
	if (attr)
		attr = attr<<8 & 0x7f00;
	else
		attr = flgs;
	for (int y=0; y<Lines; ++y)
		set (scr[y], ' ' | attr, Columns);
	curx = cury = 0;
}

void Screen::Put (int c, int attr)
{
	if (attr)
		attr = attr<<8 & 0x7f00;
	else
		attr = flgs;
	switch (c = (unsigned char) c) {
	case '\20': case '\21': case '\22': case '\23': case '\24': case '\25':
	case '\26': case '\27': case '\30': case '\31': case '\32':
		c = linedraw [c - ULC];
	default:
		if (curx>=0 && curx<Columns)
			scr[cury][curx] = c | attr;
		++curx;
		return;
	case '\t':
		int n;
		for (n=8-(curx&7); --n>=0; ++curx)
			if (curx>=0 && curx<Columns)
				scr[cury][curx] = ' ' | attr;
		return;
	case '\b':
		--curx;
		return;
	case '\n':
		if (++cury >= Lines)
			cury = 0;
	case '\r':
		curx = 0;
		return;
	}
}

void Screen::Sync ()
{
	// Set cursor position.
	// If it is out of screen, cursor will disappear.
	if (curx<0 || curx>=Columns || cury<0 || cury>=Lines)
		hidden = 1;
	union REGS regs;
	regs.x.ax = 0x200;
	regs.x.bx = 0;
	regs.x.dx = hidden ? -1 : cury << 8 | (unsigned char) curx;
	int86 (0x10, &regs, &regs);
}

void Screen::Beep ()
{
	sound (400);
	delay (200);
	nosound ();
}

void Screen::ClearLine (int y, int x, int attr)
{
	if (attr)
		attr = attr<<8 & 0x7f00;
	else
		attr = flgs;
	if (y < 0) {
		y = cury;
		x = curx;
	}
	if (x < 0)
		x = 0;
	if (x < Columns)
		set (scr[y] + x, ' ' | attr, Columns - x);
}

void Screen::UngetKey (int k)
{
	ungetflag = 1;
	ungetkey = k;
}

int Screen::GetKey ()
{
	if (ungetflag) {
		ungetflag = 0;
		return (ungetkey);
	}
nextkey:
	int c = getch ();
	if (! c) {
		c = getch ();
		switch (c) {
		default:	c = 0;		break;
		case ';':       c = meta ('A');	break;	// F1
		case '<':       c = meta ('B');	break;	// F2
		case '=':       c = meta ('C');	break;	// F3
		case '>':       c = meta ('D');	break;	// F4
		case '?':       c = meta ('E');	break;	// F5
		case '@':       c = meta ('F');	break;	// F6
		case 'A':       c = meta ('G');	break;	// F7
		case 'B':       c = meta ('H');	break;	// F8
		case 'C':       c = meta ('I');	break;	// F9
		case 'D':       c = meta ('J');	break;	// F10
		case 'E':       c = meta ('K');	break;	// F11
		case 'F':       c = meta ('L');	break;	// F12
		case 'K':       c = meta ('l');	break;	// left
		case 'M':       c = meta ('r');	break;	// right
		case 'H':       c = meta ('u');	break;	// up
		case 'P':       c = meta ('d');	break;	// down
		case 'G':       c = meta ('h');	break;	// home
		case 'O':       c = meta ('e');	break;	// end
		case 'Q':       c = meta ('n');	break;	// nexy page
		case 'I':       c = meta ('p');	break;	// prev page
		case 'R':       c = meta ('i');	break;	// insert
		case 'S':       c = meta ('x');	break;	// delete
		}
	}
gotkey:
	int found = 0;
	struct callBack *cb = callBackTable;
	for (int j=ncallb; --j>=0; ++cb)
		if (cb->key == c) {
			found = 1;
			(*cb->func) (c);
		}
	if (found)
		goto nextkey;
	return (c);
}

void Screen::AddHotKey (int key, void (*f) (int))
{
	if (ncallb >= sizeof (callBackTable) / sizeof (callBackTable[0]))
		return;
	callBackTable[ncallb].key = key;
	callBackTable[ncallb].func = f;
	++ncallb;
}

void Screen::DelHotKey (int key)
{
	if (ncallb <= 0)
		return;
	struct callBack *a = callBackTable;
	struct callBack *b = a;
	for (int j=ncallb; --j>=0; ++a)
		if (a->key != key)
			*b++ = *a;
	ncallb = callBackTable - b;
}

void Screen::PrintVect (int attr, char *fmt, void *vect)
{
	char buf [512];

	vsprintf (buf, fmt, vect);
	Put (buf, attr);
}

void Screen::Print (int attr, char *fmt, ...)
{
	PrintVect (attr, fmt, &fmt + 1);
}

void Screen::Print (int y, int x, int attr, char *fmt, ...)
{
	Move (y, x);
	PrintVect (attr, fmt, &fmt + 1);
}

void Screen::Clear (int y, int x, int ny, int nx, int attr, int sym)
{
	if (attr)
		attr = attr<<8 & 0x7f00;
	else
		attr = flgs;
	sym |= attr;
	for (; --ny>=0; ++y) {
		for (int i=nx; --i>=0;)
			scr[y][x+i] = sym;
	}
}

void Screen::DelLine (int y, int attr)
{
	if (y<0 || y>=Lines)
		return;
	if (attr)
		attr = attr<<8 & 0x7f00;
	else
		attr = flgs;
	for (; y<Lines-1; ++y)
		move (scr[y], scr[y+1], Columns);
	set (scr[Lines-1], ' ' | attr, Columns);
}

void Screen::InsLine (int y, int attr)
{
	if (y<0 || y>=Lines)
		return;
	if (attr)
		attr = attr<<8 & 0x7f00;
	else
		attr = flgs;
	for (int i=Lines-1; i>y; --i)
		move (scr[i], scr[i-1], Columns);
	set (scr[y], ' ' | attr, Columns);
}

void Screen::AttrLow (int y, int x)
{
	if (x>=0 && x<Columns)
		scr[y][x] &= ~0xf800;
}

void Screen::HorLine (int y, int x, int nx, int attr)
{
	if (attr)
		attr = attr<<8 & 0x7f00;
	else
		attr = flgs;
	while (--nx >= 0)
		scr[y][x++] = linedraw[10] | attr;
}

void Screen::VertLine (int x, int y, int ny, int attr)
{
	if (attr)
		attr = attr<<8 & 0x7f00;
	else
		attr = flgs;
	while (--ny >= 0)
		scr[y++][x] = linedraw[9] | attr;
}

void Screen::DrawFrame (int y, int x, int ny, int nx, int attr)
{
	HorLine (y, x+1, nx-2, attr);
	HorLine (y+ny-1, x+1, nx-2, attr);
	VertLine (x, y+1, ny-2, attr);
	VertLine (x+nx-1, y+1, ny-2, attr);
	Put (y, x, ULC, attr);
	Put (y, x+nx-1, URC, attr);
	Put (y+ny-1, x, LLC, attr);
	Put (y+ny-1, x+nx-1, LRC, attr);
}

void Screen::AttrSet (int y, int x, int ny, int nx, int attr)
{
	if (x < 0) {
		nx += x;
		x = 0;
	}
	if (x>=Columns || nx<0)
		return;
	if (attr)
		attr = attr<<8 & 0x7f00;
	else
		attr = flgs;
	for (; --ny>=0; ++y) {
		short far *p = & scr [y] [x];
		for (int xx=0; xx<nx; ++xx, ++p)
			*p = *p & 0xff | attr;
	}
}
