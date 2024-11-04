// #define DEBUG

#include "Screen.h"
#include "Termcap.h"
#include "extern.h"

const int BUFSIZE       = 2048;         // max length of termcap entry

static const char *tname;               // terminal name
static char *tbuf;                      // terminal entry buffer

extern "C" {
	int tgetent (char *bp, const char *tname);
	int tgetnum (char *name);
	int tgetflag (char *name);
	char *tgetstr (char *name, char **area);
};

int Screen::InitCap (char *bp)
{
	if (tbuf)
		return (1);
	if (! (tname = getenv ("TERM")))
		tname = "unknown";
	if (tgetent (bp, tname) <= 0)
		return (0);
	tbuf = bp;
	return (1);
}

void Screen::GetCap (struct Captab *t)
{
	char *begarea = new char [BUFSIZE];
	char *area = begarea;

	for (struct Captab *p=t; p->tname[0]; ++p)
		switch (p->ttype) {
		case CAPNUM:    *(p->ti) = tgetnum (p->tname);  break;
		case CAPFLG:    *(p->tc) = tgetflag (p->tname); break;
		case CAPSTR:    *(p->ts) = tgetstr (p->tname, &area); break;
		}

	char *bp = new char [area - begarea];
	memcpy (bp, begarea, area - begarea);
	for (struct Captab *p=t; p->tname[0]; ++p)
		if (p->ttype == CAPSTR && *(p->ts))
			*(p->ts) += bp - begarea;
	delete[] begarea;
#ifdef DEBUG
	for (struct Captab *p=t; p->tname[0]; ++p) {
		printf ("%c%c", p->tname[0], p->tname[1]);
		switch (p->ttype) {
		case CAPNUM:
			printf ("#%d\n", *(p->ti));
			break;
		case CAPFLG:
			printf (" %s\n", *(p->tc) ? "on" : "off");
			break;
		case CAPSTR:
			if (*(p->ts))
				printf ("='%s'\n", *(p->ts));
			else
				printf ("=NULL\n");
			break;
		}
	}
#endif
}
