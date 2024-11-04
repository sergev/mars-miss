#include "Screen.h"
#include "extern.h"

Box::Box (Screen &scr, int by, int bx, int bny, int bnx)
{
	y = by;
	x = bx;
	ny = bny;
	nx = bnx;
	mem = new short [ny * nx];
	for (int yy=0; yy<ny; ++yy) {
		ScreenPtr p = &scr.scr[yy+y][x];
		short *q = &mem[yy*nx];
		for (int xx=0; xx<nx; ++xx)
			*q++ = *p++;
	}
}

void Screen::Put (Box &box, int attr)
{
	if (attr)
		attr = attr<<8 & 0x7f00;
	for (int yy=0; yy<box.ny; ++yy) {
		short *q = & box.mem [yy * box.nx];
		for (int xx=0; xx<box.nx; ++xx)
			pokeChar (box.y+yy, box.x+xx, attr ?
				(*q++ & 0377) | attr : *q++);
	}
}
