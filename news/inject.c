#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mars.h"
#include "header.h"

#define TTL     (7L * 24 * 60 * 60)
#define MAXTTL  (14L * 24 * 60 * 60)
#define MINTTL  (2L * 24 * 60 * 60)

long curtime;

static char reqlogon[] = "logon news usenet";
static char reqinsert[] =
	"(id:varchar(100), from:varchar(100), subj:varchar(160),"
	"bytes:long, cred:long, expd:long, body:longdata)"
	"insert values $id,$from,$subj,$bytes,$cred,$expd,$body into art";
static char reqxref[] =
	"(id:varchar(100), group:varchar(100), expd:long)"
	"insert values $id,$group,$expd into xref";

char bufinsert [200];
char bufxref [200];
DBdescriptor descrinsert = (DBdescriptor) bufinsert;
DBdescriptor descrxref = (DBdescriptor) bufxref;

char *body;
long bodylen;

int verbose = 0;

extern char *genrfcdate (long tim, int tzm);
extern char *rfcdate (char *ctim);
extern int parserfcdate (char *ctim, int *pyear, int *pmon, int *pday,
	int *ph, int *pm, int *ps, int *ptz, int *pwday);

void quit ()
{
	exit (-1);
}

void error (char *s)
{
	fprintf (stderr, "%s\n", s);
}

static void dberror ()
{
	printf ("data base error: %d: %s\n", DBinfo.errCode,
		DBerror (DBinfo.errCode));
	DBdisconn ();
	quit ();
}

int nextword (char **p, char *gname, int gnamesz)
{
	char *g;

	for (g=gname; **p; ++(*p)) {
		if (g > gname + gnamesz-2)
			break;
		if (**p==' ' || **p=='\t')
			continue;
		if (**p==',')
			break;
		*g++ = **p;
	}
	*g = 0;
	if (**p) {
		if (**p != ',')
			return (0);
		++(*p);
	}
	return (1);
}

void xref (char *id, char *groups, long expired)
{
	char gname [80];

	while (*groups) {
		if (! nextword (&groups, gname, sizeof (gname)))
			continue;
		if (! *gname)
			continue;
		if (verbose)
			printf ("%s --> %s\n", id, gname);
		if (DBexec (descrxref, id, gname, expired) < 0)
			dberror ();
	}
}

long extracttime (char *ctim)
{
	int wday, tz;
	struct tm m;

	if (! parserfcdate (ctim, &m.tm_year, &m.tm_mon, &m.tm_mday,
	    &m.tm_hour, &m.tm_min, &m.tm_sec, &tz, &wday)) {
		fprintf (stderr, "Bad date: \"%s\"\n", ctim);
		return (0);
	}
	--m.tm_mon;
	m.tm_year -= 1900;
	m.tm_isdst = -1;
	m.tm_gmtoff = tz * 60;
	return (mktime (&m));
}

/*
 * Вспомогательная функция для засылки параметра типа longdata.
 * На входе получает адрес и длину буфера данных (buf,limit)
 * и смещение shift.  Должна переписать часть данных в буфер
 * и вернуть длину переписанной части.
 * Если shift<0 - просто вернуть размер данных.
 */
static long putbody (DBdescriptor d, short nelem, void *buf, long shift,
	unsigned limit)
{
	long len;

	if (shift < 0)
		return (bodylen);
	len = bodylen - shift;
	if (len <= 0)
		return (0);
	if (len > limit)
		len = limit;
	memcpy (buf, body+shift, len);
	return (len);
}

void inject (char *buf, long len)
{
	STRING f;
	long expired, created;

	f.beg = f.cur = (unsigned char *) buf;
	f.len = len;
	scanheader (&f);

	if (! h_from) {
		fprintf (stderr, "No \"From:\" field, article skipped\n");
		freeheader ();
		return;
	}
	if (! h_subject) {
		fprintf (stderr, "No \"Subject:\" field, article skipped\n");
		freeheader ();
		return;
	}
	if (! h_message_id) {
		fprintf (stderr, "No \"Message-ID:\" field, article skipped\n");
		freeheader ();
		return;
	}
	if (! h_date) {
		fprintf (stderr, "No \"Date:\" field, article skipped\n");
		freeheader ();
		return;
	}
	if (! h_newsgroups) {
		fprintf (stderr, "No \"Newsgroups:\" field, article skipped\n");
		freeheader ();
		return;
	}
	created = extracttime (h_date);
	if (h_expires)
		expired = extracttime (h_expires);
	else
		expired = curtime + TTL;

	if (expired > curtime + MAXTTL)
		expired = curtime + MAXTTL;
	else if (expired < curtime + MINTTL)
		expired = curtime + MINTTL;

	body = buf;
	bodylen = len;
	if (DBexec (descrinsert, h_message_id, h_from, h_subject,
	    len, created, expired, putbody) < 0) {
		if (DBinfo.errCode == ErrNotuniq) {
			fprintf (stderr, "Dup: %s\n", h_message_id);
			/* Database server deleted our request
			 * descriptor, let recompile it. */
			if (DBcompile (reqinsert, strlen (reqinsert),
			    descrinsert, sizeof (bufinsert)) < 0)
				dberror ();
			freeheader ();
			return;
		}
		dberror ();
	}

	xref (h_message_id, h_newsgroups, expired);

	if (verbose) {
		printf ("From: %s\n", h_from);
		printf ("Subject: %s\n", h_subject);
		printf ("Date: %s\n", genrfcdate (created, 0));
		printf ("Expires: %s\n", genrfcdate (expired, 0));
		printf ("Message-ID: %s\n", h_message_id);
		printf ("Size: %ld\n", len);
		printf ("\n");
	}
	freeheader ();
}

char *strlower (str)
char *str;
{
	register char *p;

	for (p=str; *p; ++p) {
		if (*p>='A' && *p<='Z')
			*p += 'a' - 'A';
		if (*p>=(char)0340 && *p<=(char)0377)
			*p -= 040;
	}
	return (str);
}

int main (int argc, char **argv)
{
	FILE *fd = stdin;
	char line [513], *buf;
	long len;
	int skipflag;

	for (--argc, ++argv; argc>0 && **argv=='-'; --argc, ++argv) {
		if (!strcmp (*argv, "-v"))
			++verbose;
		else {
usage:                  fprintf (stderr, "usage: inject [-v] < batch\n");
			quit ();
		}
	}
	if (argc)
		goto usage;

	if (DBconnect () < 0)
		dberror ();

	if (DBcompile (reqlogon, strlen (reqlogon), (void*) 0, 0) < 0)
		dberror ();

	if (DBcompile (reqinsert, strlen (reqinsert),
	    descrinsert, sizeof (bufinsert)) < 0)
		dberror ();

	if (DBcompile (reqxref, strlen (reqxref),
	    descrxref, sizeof (bufxref)) < 0)
		dberror ();

	time (&curtime);
	skipflag = 0;
	for (;;) {
		int c = getc (fd);
		if (c == EOF)
			break;
		ungetc (c, fd);
		if (c != '#') {
			len = 64L*1024 - 1;
			buf = malloc ((unsigned) len);
			len = fread (buf, 1, (unsigned) len, fd);
			if (len > 0)
				inject (buf, len);
			else
				fprintf (stderr, "Error reading article, article skipped\n");
			free (buf);
			break;
		}
		fgets (line, sizeof (line), fd);
		if (strncmp (line, "#! rnews ", 9) != 0) {
			if (! skipflag)
				fprintf (stderr, "Bad line: \"%.30s\", skipping...\n", line);
			skipflag = 1;
			continue;
		}
		skipflag = 0;
		len = atol (line + 9);
		if (len<1 || len>=64*1024) {
			fprintf (stderr, "Bad article size: %ld, article skipped\n", len);
			continue;
		}
		buf = malloc ((unsigned) len);
		if (! buf) {
			fprintf (stderr, "No memory for %ld bytes, article skipped\n", len);
			while (--len >= 0)
				getc (fd);
			continue;
		}
		if (fread (buf, 1, (unsigned) len, fd) != len) {
			fprintf (stderr, "Error reading %ld bytes, article skipped\n", len);
			free (buf);
			continue;
		}
		inject (buf, len);
		free (buf);
	}
	DBkill (descrinsert);
	DBcommit (1);
	DBdisconn ();
	return (0);
}
