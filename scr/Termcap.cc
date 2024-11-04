// #define DEBUG

#include "Screen.h"
#include "Termcap.h"
#include "extern.h"

const int BUFSIZE       = 2048;         // max length of termcap entry
const int MAXHOP        = 32;           // max number of tc= indirections

static char SHARETERMCAP []     = "/usr/share/misc/termcap";
static char TERMCAP []          = "/etc/termcap";
static char MYTERMCAP []        = "/usr/lib/deco/termcap";
static char MYLCLTERMCAP []     = "/usr/local/lib/deco/termcap";

static char *tname;                     // terminal name
static char *tcapfile;                  // termcap file name
static char *tbuf;                      // terminal entry buffer
static char *envtermcap;                // global variable TERMCAP
static int hopcount;                    // detect infinite loops

static char *tskip (char *);
static char *tdecode (char *, char **);
static int tgetent (char *, char *, char *);
static int tnchktc ();
static int tnamatch (char *);

int Screen::InitCap (char *bp)
{
	if (tbuf)
		return (1);
	if (! (tname = getenv ("TERM")))
		tname = "unknown";
	if (! envtermcap)
		envtermcap = getenv ("TERMCAP");
	if (envtermcap && *envtermcap=='/') {
		tcapfile = envtermcap;
		envtermcap = 0;
	}
	if (! envtermcap)
		envtermcap = "";
	if (! tcapfile || access (tcapfile, 0) < 0)
		tcapfile = SHARETERMCAP;
	if (access (tcapfile, 0) < 0)
		tcapfile = TERMCAP;
	return (tgetent (bp, tname, MYLCLTERMCAP) ||
	    tgetent (bp, tname, MYTERMCAP) ||
	    tgetent (bp, tname, tcapfile));
}

static int tgetent (char *bp, char *name, char *termcap)
{
	int c;
	int tf = -1;
	char *cp = envtermcap;
	tbuf = bp;

	// TERMCAP can have one of two things in it. It can be the
	// name of a file to use instead of /etc/termcap. In this
	// case it must start with a "/". Or it can be an entry to
	// use so we don't have to read the file. In this case it
	// has to already have the newlines crunched out.
	if (cp && *cp) {
		envtermcap = "";
		tbuf = cp;
		c = tnamatch (name);
		tbuf = bp;
		if (c) {
			strcpy (bp, cp);
			return (tnchktc ());
		}
	}
	if (tf < 0)
		tf = open (termcap, 0);
	if (tf < 0)
		return (0);
	char *ibuf = new char [BUFSIZE];
	int i = 0;
	int cnt = 0;
	for (;;) {
		cp = bp;
		for (;;) {
			if (i == cnt) {
				cnt = read (tf, ibuf, BUFSIZE);
				if (cnt <= 0) {
					close (tf);
					delete ibuf;
					return (0);
				}
				i = 0;
			}
			c = ibuf[i++];
			if (c == '\n') {
				if (cp > bp && cp[-1] == '\\'){
					cp--;
					continue;
				}
				break;
			}
			if (cp >= bp+BUFSIZE) {
				outerr ("Termcap entry too long\n");
				break;
			} else
				*cp++ = c;
		}
		*cp = 0;

		// The real work for the match.
		if (tnamatch (name)) {
			close (tf);
			delete ibuf;
			return (tnchktc ());
		}
	}
}

static int tnchktc ()
{
	register char *p, *q;
	char tcname [16];       // name of similar terminal
	char *tcbuf;
	char *holdtbuf = tbuf;
	int l;

	p = tbuf + strlen(tbuf) - 2;    // before the last colon
	while (*--p != ':')
		if (p<tbuf) {
			outerr ("Bad termcap entry\n");
			return (0);
		}
	p++;
	// p now points to beginning of last field
	if (p[0] != 't' || p[1] != 'c')
		return (1);
	strcpy (tcname,p+3);
	q = tcname;
	while (*q && *q != ':')
		q++;
	*q = 0;
	if (++hopcount > MAXHOP) {
		outerr ("Infinite tc= loop\n");
		return (0);
	}
	tcbuf = new char [BUFSIZE];
	if (! tgetent (tcbuf, tcname, tcapfile)) {
		hopcount = 0;           // unwind recursion
		delete tcbuf;
		return (0);
	}
	for (q=tcbuf; *q != ':'; q++);
	l = p - holdtbuf + strlen(q);
	if (l > BUFSIZE) {
		outerr ("Termcap entry too long\n");
		q [BUFSIZE - (p-tbuf)] = 0;
	}
	strcpy (p, q+1);
	tbuf = holdtbuf;
	hopcount = 0;                   // unwind recursion
	delete tcbuf;
	return (1);
}

static int tnamatch (char *np)
{
	register char *Np, *Bp;

	Bp = tbuf;
	if (*Bp == '#')
		return (0);
	for (;;) {
		for (Np = np; *Np && *Bp == *Np; Bp++, Np++)
			continue;
		if (*Np == 0 && (*Bp == '|' || *Bp == ':' || *Bp == 0))
			return (1);
		while (*Bp && *Bp != ':' && *Bp != '|')
			Bp++;
		if (*Bp == 0 || *Bp == ':')
			return (0);
		Bp++;
	}
}

static char *tskip (char *bp)
{
	while (*bp && *bp != ':')
		bp++;
	if (*bp == ':')
		bp++;
	return (bp);
}

void Screen::GetCap (struct Captab *t)
{
	register char *bp;
	register struct Captab *p;
	register i, base;
	char name [2];
	char *area, *begarea;

	if (! tbuf)
		return;
	bp = tbuf;
	area = begarea = new char [BUFSIZE];
	for (;;) {
		bp = tskip(bp);
		if (! bp[0] || ! bp[1])
			break;
		if (bp[0] == ':' || bp[1] == ':')
			continue;
		name[0] = *bp++;
		name[1] = *bp++;
		for (p=t; p->tname[0]; ++p)
			if (p->tname[0] == name[0] && p->tname[1] == name[1])
				break;
		if (! p->tname[0] || p->tdef)
			continue;
		p->tdef = 1;
		if (*bp == '@')
			continue;
		switch (p->ttype) {
		case CAPNUM:
			if (*bp != '#')
				continue;
			bp++;
			base = 10;
			if (*bp == '0')
				base = 8;
			i = 0;
			while (*bp>='0' && *bp<='9')
				i = i * base, i += *bp++ - '0';
			*(p->ti) = i;
			break;
		case CAPFLG:
			if (*bp && *bp != ':')
				continue;
			*(p->tc) = 1;
			break;
		case CAPSTR:
			if (*bp != '=')
				continue;
			bp++;
			*(p->ts) = tdecode (bp, &area);
			break;
		}
	}
	bp = new char [area - begarea];
	memcpy (bp, begarea, area - begarea);
	for (p=t; p->tname[0]; ++p)
		if (p->ttype == CAPSTR && *(p->ts))
			*(p->ts) += bp - begarea;
	delete begarea;
#ifdef DEBUG
	for (p=t; p->tname[0]; ++p) {
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

static char *tdecode (char *str, char **area)
{
	register char *cp;
	register int c;
	register char *dp;
	int i;

	cp = *area;
	while ((c = *str++) && c != ':') {
		switch (c) {

		case '^':
			c = *str++ & 037;
			break;

		case '\\':
			dp = "E\033^^\\\\::n\nr\rt\tb\bf\f";
			c = *str++;
nextc:
			if (*dp++ == c) {
				c = *dp++;
				break;
			}
			dp++;
			if (*dp)
				goto nextc;
			if (c>='0' && c<='9') {
				c -= '0', i = 2;
				do
					c <<= 3, c |= *str++ - '0';
				while (--i && *str>='0' && *str<='9');
			}
			break;
		}
		*cp++ = c;
	}
	*cp++ = 0;
	str = *area;
	*area = cp;
	return (str);
}
