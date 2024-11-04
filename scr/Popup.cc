#include "Screen.h"
#include "extern.h"

struct choice {
	char *name;
	int namlen;
	int r, c;
};

static struct choice tab [3];
static cnum;

void Screen::Error (int c, int i, char *head, char *reply, char *s, ...)
{
	char buf [100];

	vsprintf (buf, s, (char *) (&s + 1));
	Popup (head, buf, 0, reply, 0, 0, c, i);
}

int Screen::Popup (char *head, char *mesg, char *mesg2,
	char *c1, char *c2, char *c3, int color, int inverse)
{
	int len, r;

	int w = strlen (mesg);
	if (mesg2) {
		len = strlen (mesg2);
		if (len > w)
			w = len;
	}
	len = strlen (head);
	if (len > w)
		w = len;
	len = 0;
	if (c1)
		len += strlen (c1);
	if (c2)
		len += strlen (c2);
	if (c3)
		len += strlen (c3);
	if (len > w)
		w = len;
	int h = 6;
	w += 10;
	r = Lines/2 - h/2;
	int c = Columns/2 - w/2;

	Box box (*this, r, c, h+1, w+1);                // save box
	Clear (r, c, h, w, color, ' ');
	DrawFrame (r, c, h, w, color);
	Put (r, c + (w-strlen(head)) / 2, head, color); // head
	if (mesg2) {
		Put (r+1, c + (w-strlen(mesg)) / 2, mesg, color);
		Put (r+2, c + (w-strlen(mesg2)) / 2, mesg2, color);
	} else
		Put (r+2, c + (w-strlen(mesg)) / 2, mesg, color);

	// Draw shadow.
	for (int i=0; i<w; ++i)
		AttrLow (r+h, c+i+1);
	for (i=0; i<h; ++i)
		AttrLow (r+i+1, c+w);

	if (c1) {
		initChoice (r+4, c+w/2, c1, c2, c3);

		int ch = menuChoice (color, inverse);
		Put (box);
		return (ch);
	} else {
		// message
		HideCursor ();
		Sync ();
		return (0);
	}
}

void Screen::initChoice (int row, int col, char *c1, char *c2, char *c3)
{
	cnum = c2 ? (c3 ? 3 : 2) : 1;
	tab[0].name = c1;
	tab[1].name = c2;
	tab[2].name = c3;
	int w = cnum-1;
	for (int i=0; i<cnum; ++i) {
		w += tab[i].namlen = strlen (tab[i].name);
		tab[i].r = row;
	}
	tab[0].c = col-w/2;
	tab[1].c = tab[0].c + tab[0].namlen + 1;
	tab[2].c = tab[1].c + tab[1].namlen + 1;
}

int Screen::menuChoice (int color, int inverse)
{
	int ch = 0;
	for (;;) {
		for (int i=0; i<cnum; ++i) {
			if (i == ch) {
				Put (tab[i].r, tab[i].c, tab[i].name, inverse);
				if (! HasInverse ()) {
					Put (tab[i].r, tab[i].c, '[', inverse);
					Put (tab[i].r, tab[i].c +
						strlen (tab[i].name) - 1, ']', inverse);
				}
			} else
				Put (tab[i].r, tab[i].c, tab[i].name, color);
		}
		HideCursor ();
		Sync ();
		switch (GetKey ()) {
		default:
			Beep ();
			continue;
		case cntrl ('['):
		case cntrl ('C'):
		case meta ('J'):        // f0
			return (-1);
		case cntrl ('M'):
		case cntrl ('J'):
			return (ch);
		case ' ':
		case cntrl ('I'):
		case meta ('r'):        // right
			if (++ch >= cnum)
				ch = 0;
			continue;
		case cntrl ('H'):
		case meta ('l'):        // left
			if (--ch < 0)
				ch = cnum-1;
			continue;
		}
	}
}

char *Screen::editString (int r, int c, int w, char *str, int cp, int color)
{
	int k;
	int firstkey = 1;
	if (cp) {
		for (cp=0; str[cp]; ++cp)
			continue;
		firstkey = 0;
	}
	for (; ; firstkey=0) {
		Clear (r, c, 1, w, color, ' ');
		Put (r, c, str, color);
		Move (r, c+cp);
		Sync ();
		int key = GetKey ();
		switch (key) {
		default:
			if (key<' ' || key>'~' && key<0300 || key>255) {
				Beep ();
				continue;
			}
			if (firstkey) {
				str[0] = key;
				str[1] = 0;
				cp = 1;
				continue;
			}
			for (k=cp; str[k]; ++k) {
				int t = key;
				key = str[k];
				str[k] = t;
			}
			str [k] = key;
			str [w] = str [k+1] = 0;
			// fall through
		case meta ('r'):        // right
			if (str [cp]) {
				++cp;
				if (cp >= w)
					cp = w-1;
			}
			continue;
		case meta ('l'):        // left
			if (--cp < 0)
				cp = 0;
			continue;
		case cntrl ('C'):
		case cntrl ('['):
		case meta ('J'):        // f0
			return (0);
		case cntrl ('M'):
		case cntrl ('J'):
			return (str);
		case cntrl ('I'):
			if (str [cp])
				while (str [++cp])
					continue;
			else
				cp = 0;
			continue;
		case meta ('h'):        // home
			cp = 0;
			continue;
		case meta ('e'):        // end
			while (str [cp])
				++cp;
			continue;
		case cntrl ('H'):               // back space
			if (cp) {
				for (k=cp--; str[k]; ++k)
					str[k-1] = str[k];
				str [k-1] = 0;
			}
			continue;
		case meta ('x'):		// delete
			if (! str [cp])
				continue;
			for (k=cp+1; str[k]; ++k)
				str[k-1] = str[k];
			str [k-1] = 0;
			continue;
		case cntrl ('Y'):               // clear line
			str [cp = 0] = 0;
			continue;
		}
	}
}

char *Screen::GetString (int w, char *str, char *head, char *mesg, int color, int inverse)
{
	int len = strlen (mesg);
	if (len > w)
		w = len;
	len = strlen (head);
	if (len > w)
		w = len;
	int h = 4;
	w += 4;
	int r = Lines/4;
	int c = (78 - w) / 2;

	Box box (*this, r, c, h+1, w+1);                // save box
	Clear (r, c, h, w, color, ' ');
	DrawFrame (r, c, h, w, color);
	Put (r, c + (w-strlen(head)) / 2, head, color); // head
	Put (r+1, c+2, mesg, color);                    // message

	// Draw shadow.
	for (int i=0; i<w; ++i)
		AttrLow (r+h, c+i+1);
	for (i=0; i<h; ++i)
		AttrLow (r+i+1, c+w);

	static char buf [129];
	strncpy (buf, str ? str : "", 80);

	str = editString (r+2, c+2, w-4, buf, 0, inverse);
	Put (box);
	return (str);
}
