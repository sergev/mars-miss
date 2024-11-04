#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "format.h"

static double scale (double v, int sc)
{
#define ADJUSTMINUS(n) if (sc <= -n) v *= 1e-##n, sc += n
#define ADJUSTPLUS(n)  if (sc >=  n) v *= 1e+##n, sc -= n
	if (sc < 0) {
		if (sc <= -512)
			return (0);
#ifndef vax
		ADJUSTMINUS (256);
		ADJUSTMINUS (128);
		ADJUSTMINUS (64);
#endif
		ADJUSTMINUS (32);
		ADJUSTMINUS (16);
		ADJUSTMINUS (8);
		ADJUSTMINUS (4);
		ADJUSTMINUS (2);
		ADJUSTMINUS (1);
	} else if (sc > 0) {
		if (sc >= 512)
			return (0);
#ifndef vax
		ADJUSTPLUS (256);
		ADJUSTPLUS (128);
		ADJUSTPLUS (64);
#endif
		ADJUSTPLUS (32);
		ADJUSTPLUS (16);
		ADJUSTPLUS (8);
		ADJUSTPLUS (4);
		ADJUSTPLUS (2);
		ADJUSTPLUS (1);
	}
	return (v);
}

static void skipline (FILE *fd)
{
	for (;;) {
		int c = getc (fd);
		if (c == EOF)
			break;
		if (c == '\n')
			break;
	}
}

static void skipchars (FILE *fd, int n)
{
	for (; n>0; --n) {
		int c = getc (fd);
		if (c == EOF)
			break;
		if (c == '\n') {
			ungetc (c, stdin);
			break;
		}
	}
}

static int skiptext (FILE *fd, int n, char *text)
{
	for (; n>0; --n, ++text) {
		int c = getc (fd);
		if (c == EOF)
			break;
		if (c == '\n') {
			ungetc (c, stdin);
			break;
		}
		if (c != *text)
			break;
	}
	return (n==0);
}

static void readchars (FILE *fd, char *buf, int n)
{
	for (; --n>=0; ++buf) {
		int c = getc (fd);
		if (c == EOF)
			break;
		if (c == '\n') {
			ungetc (c, stdin);
			break;
		}
		*buf = c;
	}
	*buf = 0;
}

static void readfield (FILE *fd, char *buf, int n)
{
	int c;

	/* Пропускаем пробелы в начале поля */
	while ((c = getc (fd)) == ' ' || c=='\t')
		continue;
	if (c == EOF || c == '\n') {
		*buf = 0;
		return;
	}
	do *buf++ = c;
	while (--n>0 && (c = getc (fd)) != EOF && c!=' ' && c!='\t' && c!='\n');
	*buf = 0;
}

int Print (FILE *fd, int n, void **av, int *at)
{
	int ok;

	for (ok=0; ok<n; ++ok, ++at, ++av) {
		if (ok)
			putc (' ', fd);
		switch (*at) {
		case 's': fprintf (fd, "%d",   *(short*)  *av);    break;
		case 'i': fprintf (fd, "%d",   *(int*)    *av);    break;
		case 'l': fprintf (fd, "%ld",  *(long*)   *av);    break;
		case 'f': fprintf (fd, "%g",   *(float*)  *av);    break;
		case 'd': fprintf (fd, "%g",   *(double*) *av);    break;
		default:  fprintf (fd, "%.*s", -*at, (char*) *av); break;
		}
	}
	putc ('\n', fd);
	return (ok);
}

int PrintQuote (FILE *fd, int n, void **av, int *at)
{
	int ok;

	for (ok=0; ok<n; ++ok, ++at, ++av) {
		if (ok)
			putc (' ', fd);
		switch (*at) {
		case 's': fprintf (fd, "%d",   *(short*)  *av);    break;
		case 'i': fprintf (fd, "%d",   *(int*)    *av);    break;
		case 'l': fprintf (fd, "%ld",  *(long*)   *av);    break;
		case 'f': fprintf (fd, "%g",   *(float*)  *av);    break;
		case 'd': fprintf (fd, "%g",   *(double*) *av);    break;
		default:  fprintf (fd, "`%.*s'", -*at, (char*) *av); break;
		}
	}
	putc ('\n', fd);
	return (ok);
}

int PrintFormat (FILE *fd, int n, void **av, int *at)
{
	int ok, type;
	double v;
	char *fmt;
	int eolseen = 0;

	for (ok=0; ok<n; ++ok, ++at, ++av) {
		fmt = "%*.*e";
nextfmt:        switch (type = FormatNext ()) {
		default:
			goto nextfmt;
		case '*':
			++eolseen;
			if (! ok && eolseen > 1)
				return (ok);
		case '/':
			putc ('\n', fd);
			goto nextfmt;
		case 'X':
			fprintf (fd, "%*s", FormatWidth, " ");
			goto nextfmt;
		case 'H':
			fprintf (fd, "%.*s", FormatWidth, FormatText);
			goto nextfmt;
		case 'F':
			fmt = "%*.*f";
			break;
		case 'E':
		case 'D':
		case 'I':
		case 'A':
			break;
		}
		switch (*at) {
		case 's':
			fprintf (fd, "%*d", FormatWidth, *(short*) *av);
			break;
		case 'i':
			fprintf (fd, "%*d", FormatWidth,  *(int*) *av);
			break;
		case 'l':
			fprintf (fd, "%*ld", FormatWidth, *(long*) *av);
			break;
		case 'f':
			v = *(float*) *av;
			if (FormatScale)
				v = scale (v, FormatScale);
			fprintf (fd, fmt, FormatWidth, FormatPrecision, v);
			break;
		case 'd':
			v = *(double*) *av;
			if (FormatScale)
				v = scale (v, FormatScale);
			fprintf (fd, fmt, FormatWidth, FormatPrecision, v);
			break;
		default:
			fprintf (fd, "%*.*s", -FormatWidth, -*at, (char*) *av);
			break;
		}
	}
	return (ok);
}

void PrintFlush (FILE *fd)
{
	for (;;) {
		switch (FormatNext ()) {
		default:
			continue;
		case 'X':
			fprintf (fd, "%*s", FormatWidth, " ");
			continue;
		case 'H':
			fprintf (fd, "%.*s", FormatWidth, FormatText);
			continue;
		case '/':
			putc ('\n', fd);
			continue;
		case 'E':
		case 'D':
		case 'F':
		case 'I':
		case 'A':
		case '*':
			putc ('\n', fd);
			return;
		}
	}
}

int Scan (FILE *fd, int n, void **av, int *at)
{
	int ok;
	char buf [1024], *e;

	for (ok=0; ok<n; ++ok, ++at, ++av) {
		readfield (fd, buf, sizeof (buf));
		if (! *buf)
			return (ok);
		if (*at < 0) {
			int len = strlen (buf);
			strncpy (*av, buf, -*at);
			while (len < -*at)
				((char*) *av) [len++] = ' ';
			continue;
		}
		switch (*at) {
		case 's': *(short*) *av = strtol (buf, &e, 10); break;
		case 'i': *(int*) *av = strtol (buf, &e, 10);   break;
		case 'l': *(long*) *av = strtol (buf, &e, 10);  break;
		case 'f': *(float*) *av = strtod (buf, &e);     break;
		case 'd': *(double*) *av = strtod (buf, &e);    break;
		}
		if (e == buf)
			return (ok);
	}
	return (ok);
}

int ScanFormat (FILE *fd, int n, void **av, int *at)
{
	int ok, type;
	char buf [1024], *e;
	int eolseen = 0;
	double v;

	for (ok=0; ok<n; ++ok, ++at, ++av) {
nextfmt:        switch (type = FormatNext ()) {
		default:
			goto nextfmt;
		case '*':
			++eolseen;
			if (! ok && eolseen > 1)
				return (ok);
		case '/':
			skipline (fd);
			goto nextfmt;
		case 'X':
			skipchars (fd, FormatWidth);
			goto nextfmt;
		case 'H':
			if (! skiptext (fd, FormatWidth, FormatText))
				return (ok);
			goto nextfmt;
		case 'E':
		case 'D':
		case 'F':
		case 'I':
		case 'A':
			break;
		}
		readchars (fd, buf, FormatWidth);
		if (feof (fd) && !*buf)
			return (ok);
		if (*at < 0) {
			int len = strlen (buf);
			strncpy (*av, buf, -*at);
			while (len < -*at)
				((char*) *av) [len++] = ' ';
			continue;
		}
		switch (*at) {
		case 's': *(short*) *av = strtol (buf, &e, 10); break;
		case 'i': *(int*) *av = strtol (buf, &e, 10);   break;
		case 'l': *(long*) *av = strtol (buf, &e, 10);  break;
		case 'f':
			v = strtod (buf, &e);
			if (type=='F' && FormatScale)
				v = scale (v, -FormatScale);
			*(float*) *av = v;
			break;
		case 'd':
			v = strtod (buf, &e);
			if (type=='F' && FormatScale)
				v = scale (v, -FormatScale);
			*(double*) *av = v;
			break;
		}
		if (e == buf)
			return (ok);
	}
	return (ok);
}
