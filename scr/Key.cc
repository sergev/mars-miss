extern "C" {
	#include <setjmp.h>
	#include <signal.h>
};

#include "Screen.h"
#include "Termcap.h"
#include "extern.h"

#define MAXCALLB 10

const int NKEY = 100;

struct callBack {
	int key;
	void (*func) (int);
};

struct KeyPrivate {
	int keyback;
	struct Keytab *keymap;
	struct callBack callBackTable[MAXCALLB];
	int ncallb;
};

static jmp_buf wakeup;

static void badkey ()
{
	longjmp (wakeup, -1);
}

// Compare keys. Used in call to qsort.
// Return -1, 0, 1 iff a is less, equal or greater than b.
// First, check if there is ->str.
// Then compare lengths, then strings.
// If equal, check ->tcap field.
static int compkeys (struct Keytab *a, struct Keytab *b)
{
	int cmp;

	if (! a->str) {
		if (! b->str)
			return (0);
		return (1);
	}
	if (! b->str)
		return (-1);
	cmp = strcmp (a->str, b->str);
	if (cmp)
		return (cmp);
	if (! a->tcap) {
		if (! b->tcap)
			return (0);
		return (1);
	}
	if (! b->tcap)
		return (-1);
	return (0);
}

void Screen::InitKey (struct Keytab *map)
{
	struct Captab tab [NKEY];
	struct Keytab *kp;
	struct Captab *t;
	static struct Keytab nulltab = { 0 };

	if (! keydata)
		keydata = new KeyPrivate;
	keydata->keyback = 0;
	keydata->ncallb = 0;
	keydata->keymap = map ? map : &nulltab;
	for (t=tab, kp=keydata->keymap; kp->val && t<tab+NKEY-1; ++kp, ++t) {
		if (! kp->tcap)
			continue;
		t->tname[0] = kp->tcap[0];
		t->tname[1] = kp->tcap[1];
		t->ttype = CAPSTR;
		t->tdef = 0;
		t->tc = 0;
		t->ti = 0;
		t->ts = &kp->str;
	}
	kp->val = 0;
	t->tname[0] = 0;
	GetCap (tab);
	qsort ((char *) keydata->keymap, (unsigned) (kp - keydata->keymap), sizeof (keydata->keymap[0]), compkeys);
#ifdef notdef
	{
		struct Keytab *p;
		for (p=keydata->keymap; p<kp; ++p)
			if (kp->str)
				printf ("'\\%03o%s' -> %x\n", p->str[0], p->str+1, p->val);
	}
#endif
}

int Screen::InputChar ()
{
	char c;

	while (read (0, &c, 1) != 1)
		if (! isatty (0))
			exit (0);
	return ((unsigned char) c);
}

int Screen::inputKey (struct Keytab *kp)
{
	int match, c;
	struct Keytab *lp;

	for (match=1; ; ++match) {
		if (! kp->str [match])
			return (kp->val);
		c = InputChar ();
		if (kp->str [match] == c)
			continue;
		lp = kp;
		do {
			++kp;
			if (! kp->str)
unknown:                        longjmp (wakeup, 1);
		} while (kp->str [match] != c);
		if (lp->str [match-1] != kp->str [match-1])
			goto unknown;
#ifdef HARDKEYS
		if (match>1 && strncmp (lp->str, kp->str, match-1))
			goto unknown;
#endif
	}
}

int Screen::GetKey ()
{
	unsigned oldalarm = 0;
	int c, j;

	if (! keydata)
		return (0);
	if (keydata->keyback) {
		c = keydata->keyback;
		keydata->keyback = 0;
		return (c);
	}
	Flush ();
nextkey:
	c = InputChar ();
	for (struct Keytab *kp=keydata->keymap; kp->str; ++kp)
		if ((char) c == kp->str[0])
			break;
	if (! kp->str) {
#ifdef DOC
		if (c == cntrl ('_')) {
			_prscreen ();
			goto nextkey;
		}
#endif
#ifdef CYRILLIC
		// Handle cyrillic input.  ^N turns on cyrillic input mode,
		// after that all input characters in range ' '...'~' must be
		// recoded by special cyrillic input table.  It is specified
		// by Ct termcap descriptor.  ^O turns off cyrillic input mode.
		if (c == cntrl ('N')) {
			cyrinput = 1;
			goto nextkey;
		}
		if (c == cntrl ('O')) {
			cyrinput = 0;
			goto nextkey;
		}
		if (cyrinput && c>=' ' && c<='~')
			c = CyrInputTable [c - ' '];
#endif
	} else if (! kp->str [1]) {
		c = kp->val;
	} else if (j = setjmp (wakeup)) {       // look for escape sequence
		// time out or unknown escape sequence
		alarm (oldalarm);
		FlushTtyInput ();
		if (j > 0)
			goto nextkey;
	} else {
		signal (SIGALRM, badkey);
		oldalarm = alarm (2);
		c = inputKey (kp);
		alarm (oldalarm);
	}

	int found = 0;
	struct callBack *cb = keydata->callBackTable;
	for (j=keydata->ncallb; --j>=0; ++cb)
		if (cb->key == c) {
			found = 1;
			(*cb->func) (c);
		}
	if (found) {
		Flush ();
		goto nextkey;
	}
	return (c);
}

void Screen::UngetKey (int key)
{
	if (keydata)
		keydata->keyback = key;
}

void Screen::AddHotKey (int key, void (*f) (int))
{
	if (! keydata || keydata->ncallb >= MAXCALLB)
		return;
	keydata->callBackTable[keydata->ncallb].key = key;
	keydata->callBackTable[keydata->ncallb].func = f;
	++keydata->ncallb;
}

void Screen::DelHotKey (int key)
{
	if (! keydata || keydata->ncallb <= 0)
		return;
	struct callBack *a = keydata->callBackTable;
	struct callBack *b = a;
	for (int j=keydata->ncallb; --j>=0; ++a)
		if (a->key != key)
			*b++ = *a;
	keydata->ncallb = keydata->callBackTable - b;
}
