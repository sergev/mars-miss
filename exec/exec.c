#include <stdio.h>
#ifndef NO_STDLIB
#include <stdlib.h>
#endif
#include <stdarg.h>
#include <string.h>
#include "mars.h"
#include "format.h"

#define MAXQUERIES      20
#define MAXARGS         128
#define MAXVARCHAR      256
#define LONGSZ          ((int) sizeof (long))
#define SHORTSZ         ((int) sizeof (short))

typedef struct {
	short end;
	short beg;
} VarCharDescr;

char DescrBuf [MAXQUERIES*400];         /* Буфер для компилированных кодов */
DBdescriptor Descr [MAXQUERIES];        /* Дескрипторы кодов */
int CompiledDescr [MAXQUERIES];         /* Признак компилируемого запроса */
int nDescr;                             /* Количество занятых дескрипторов */

void *Args [MAXARGS];                   /* Аргументы для вв./выв. по формату */
int ArgTypes [MAXARGS];                 /* Типы аргументов для вв./выв. */

char *VarBuf;                           /* Буфер строк для ввода varchar */
int VarBufSize;                         /* Размер буфера */

int debug = 0;
int cflag = 0;
char *progname;

void GetData (char *filename, char *format, DBdescriptor d, char **list);
void PutData (char *filename, char *format, DBdescriptor d, char *query);
void WithTable (char **list);

extern int Scan (FILE *fd, int n, void **av, int *at);
extern int Print (FILE *fd, int n, void **av, int *at);
extern int PrintQuote (FILE *fd, int n, void **av, int *at);
extern int ScanFormat (FILE *fd, int n, void **av, int *at);
extern int PrintFormat (FILE *fd, int n, void **av, int *at);
extern void PrintFlush (FILE *fd);

extern int yyparse (void);
extern void yyinit (FILE *fd);
extern void yyinitstr (char *ptr);

static inline char *VarCharBeg (VarCharDescr *x)
{
	return ((char*) &x->beg + x->beg);
}

static inline char *VarCharEnd (VarCharDescr *x)
{
	return ((char*) &x->end + x->end);
}

static inline int VarCharLen (VarCharDescr *x)
{
	return (x->end - x->beg - SHORTSZ);
}

void Usage ()
{
	printf ("Usage:\n");
	printf ("\tmarsexec [-v] -c 'query'\n");
	printf ("or\n");
	printf ("\tmarsexec [-v] file\n");
}

void setvar (int n, char *val)
{
	char *string = malloc (strlen (val) + 8);

	sprintf (string, "%d=%s", n, val);
	putenv (string);
}

/*
 * Вызов:
 *      marsexec [-v] -c 'запрос'
 * или
 *      marsexec [-v] файл арг1 арг2 ...
 */
int main (int argc, char **argv)
{
	progname = *argv++;
	--argc;
	for (; argc>0 && **argv=='-' && !cflag; --argc, ++argv) {
		char *p = *argv + 1;

		for (; *p; ++p)
			switch (*p) {
			case 'h':
				Usage ();
				return (0);
			case 'v':
				++debug;
				break;
			case 'c':
				++cflag;
				break;
			}
	}
	setvar (0, progname);
	yyinit (stdin);
	if (cflag) {
		char *query;

		for (query=0; argc>0; --argc, ++argv) {
			int len = strlen (*argv) + 3;
			if (query) {
				query = realloc (query, strlen (query) + len);
				strcat (query, " ");
			} else {
				query = malloc (len);
				*query = 0;
			}
			strcat (query, *argv);
		}
		strcat (query, ";");
		yyinitstr (query);
	} else {
		char *infile = 0;
		int n;

		if (argc > 0) {
			infile = *argv;
			--argc;
			++argv;
		}
		for (n=1; argc>0; --argc, ++argv, ++n)
			setvar (n, *argv);
		if (infile) {
			FILE *fd = fopen (infile, "r");
			if (! fd) {
				fprintf (stderr, "marsexec: cannot read %s\n", infile);
				return (-1);
			}
			yyinit (fd);
		}
	}
	return (yyparse ());
}

void fatal (char *s, ...)
{
	va_list ap;

	va_start (ap, s);
	fprintf (stderr, "fatal error: ");
	vfprintf (stderr, s, ap);
	fprintf (stderr, "\n");
	va_end (ap);
	exit (-1);
}

void fataldb (char *s)
{
	fprintf (stderr, "database error: %s\n", DBerror (DBinfo.errCode));
	fprintf (stderr, "*** %s\n", s);
	if (DBinfo.errPosition >= 0)
		fprintf (stderr, "***%*s^\n", 1+DBinfo.errPosition, " ");
	exit (-1);
}

void Connect ()
{
	DBconnect ();
}

void Disconnect ()
{
	DBdisconn ();
}

void InputParams (DBdescriptor d)
{
	fatal ("not implemented yet");
	/* nDescr = 1; GetData (0, 0, descr, &query); */
}

/*
 * Выполнение простого запроса.
 */
void Execute (char *query)
{
	DBdescriptor descr = (DBdescriptor) DescrBuf;
	int reply;

	if (debug)
		printf ("%s;\n", query);

	/* Компилируем запрос */
	reply = DBcompile (query, strlen (query), descr, sizeof (DescrBuf));
	if (reply < 0)
		fataldb (query);

	/* Если запрос был интерпретируемым, -- все пучком */
	if (reply == 0)
		return;

	/* Есть выходные данные? Так нельзя */
	if (descr->columnNumber != 0)
		fatal ("Illegal retrieve operation: %s", query);

	if (descr->paramNumber != 0) {
		/* Нужны параметры, зачитываем */
		InputParams (descr);
		reply = DBlexec (descr);
	} else
		/* Без параметров, выполняем как есть */
		reply = DBexec (descr);

	if (reply)
		fataldb (query);

	DBkill (descr);
}

/*
 * Выгрузка данных из базы в файл.
 */
void DoWrite (char *filename, char *format, char *query)
{
	DBdescriptor descr = (DBdescriptor) DescrBuf;
	int reply;

	if (debug) {
		printf ("PUT ");
		if (filename)
			printf ("'%s'", filename);
		else
			printf ("-");
		if (format)
			printf (" USING '%s'", format);
		printf ("\n\t%s;\n", query);
	}

	/* Компилируем запрос */
	reply = DBcompile (query, strlen (query), descr, sizeof (DescrBuf));
	if (reply < 0)
		fataldb (query);

	/* Запрос данных без выходных параметров? Странно */
	if (reply == 0 || descr->columnNumber == 0)
		fatal ("Illegal insert operation: %s", query);

	if (descr->paramNumber != 0) {
		/* Нужны параметры, зачитываем */
		InputParams (descr);
		reply = DBlexec (descr);
	} else
		/* Без параметров, выполняем как есть */
		reply = DBexec (descr);

	if (reply)
		fataldb (query);

	/* Зачитываем данные и пишем их в файл */
	PutData (filename, format, descr, query);

	DBkill (descr);
}

/*
 * Загрузка данных из файла в базу.
 */
void DoRead (char *filename, char *format, char **list)
{
	DBdescriptor first;
	long *freeptr, freesize;
	int n;

	if (debug) {
		printf ("WITH ");
		if (filename)
			printf ("'%s'", filename);
		else
			printf ("-");
		if (format)
			printf (" USING '%s'", format);
		printf (" {\n");
		for (n=0; list[n]; ++n)
			printf ("\t%s;\n", list[n]);
		printf ("}\n");
	}

	/* Компилируем запросы */
	freeptr = (long*) DescrBuf;
	freesize = sizeof (DescrBuf);
	for (nDescr=0; list[nDescr]; ++nDescr) {
		Descr[nDescr] = (DBdescriptor) freeptr;
		CompiledDescr[nDescr] = DBcompile (list[nDescr],
			strlen (list[nDescr]), Descr[nDescr], freesize);
		if (CompiledDescr[nDescr] < 0)
			fataldb (list[nDescr]);
		freeptr += 1 + Descr[nDescr]->descrLength / LONGSZ;
		freesize -= Descr[nDescr]->descrLength;
	}

	/* проверка соответствия типов параметров
	 * всех параметризованных запросов */
	first = 0;
	for (n=0; n<nDescr; ++n) {
		DBdescriptor d = Descr[n];
		int l;

		if (CompiledDescr[n] == 0)
			fatal ("Illegal immediate operation: %s", list[n]);
		if (d->columnNumber != 0)
			fatal ("Illegal retrieve operation: %s", list[n]);
		if (d->paramNumber == 0)
			continue;
		if (! first) {
			first = d;
			continue;
		}
		if (first->paramNumber != d->paramNumber)
			fatal ("Bad number of parameters: %s", list[n]);
		for (l=0; l<first->paramNumber; ++l)
			if (first->elements[l].type != d->elements[l].type ||
			    first->elements[l].length != d->elements[l].length)
				fatal ("Bad type of parameter %d: %s",
					l, list[n]);
	}
	if (! first)
		fatal ("Illegal insert operation: %s", list[n]);

	/* Считываем данные из файла в базу */
	GetData (filename, format, first, list);

	for (n=0; n<nDescr; ++n)
		DBkill (Descr[n]);
}

void DoForAll (char *query, char **list)
{
	long *freeptr, freesize;
	int reply, n;

	if (debug) {
		printf ("FORALL %s; {\n", query, *list);
		for (n=0; list[n]; ++n)
			printf ("\t%s;\n", list[n]);
		printf ("}\n");
	}

	/* Компилируем запросы */
	freeptr = (long*) DescrBuf;
	freesize = sizeof (DescrBuf);
	for (nDescr=0; list[nDescr]; ++nDescr) {
		Descr[nDescr] = (DBdescriptor) freeptr;
		CompiledDescr[nDescr] = DBcompile (list[nDescr],
			strlen (list[nDescr]), Descr[nDescr], freesize);
		if (CompiledDescr[nDescr] < 0)
			fataldb (list[nDescr]);
		freeptr += 1 + Descr[nDescr]->descrLength / LONGSZ;
		freesize -= Descr[nDescr]->descrLength;
	}

	if (CompiledDescr[0]==0 || Descr[0]->columnNumber==0)
		fatal ("Illegal select operation: %s", list[0]);

	/* Проверка соответствия типов параметров остальных
	 * запросов типу параметров первого запроса */
	for (n=1; n<nDescr; ++n) {
		DBdescriptor d = Descr[n];
		int l;

		if (CompiledDescr[n] == 0)
			fatal ("Illegal immediate operation: %s", list[n]);
		if (d->columnNumber != 0)
			fatal ("Illegal retrieve operation: %s", list[n]);
		if (d->paramNumber == 0)
			continue;
		if (d->paramNumber != Descr[0]->columnNumber)
			fatal ("Bad number of parameters: %s", list[n]);
		for (l=0; l<Descr[0]->columnNumber; ++l)
			if (d->elements[l].type !=
			    Descr[0]->elements[l+Descr[0]->paramNumber].type ||
			    d->elements[l].length !=
			    Descr[0]->elements[l+Descr[0]->paramNumber].length)
				fatal ("Bad type of parameter %d: %s",
					l, list[n]);
	}

	if (Descr[0]->paramNumber != 0) {
		/* Нужны параметры, зачитываем */
		InputParams (Descr[0]);
		reply = DBlexec (Descr[0]);
	} else
		/* Без параметров, выполняем как есть */
		reply = DBexec (Descr[0]);

	if (reply)
		fataldb (query);

	WithTable (list);

	for (n=0; n<nDescr; ++n)
		DBkill (Descr[n]);
}

void WithTable (char **list)
{
	int n, reply;

	while ((reply = DBlfetch (Descr[0])) != 0) {
		memcpy (DBexecBuf, DBbuffer, *(short*) DBbuffer);
		for (n=1; n<nDescr; ++n)
			if (reply = DBlexec (Descr[n]))
				fataldb (list[n]);
	}
}

void GetData (char *filename, char *format, DBdescriptor d, char **list)
{
	DBelement* scan;
	int reply, n, nvar;
	FILE *fd;

	if (filename) {
		fd = fopen (filename, "r");
		if (! fd)
			fatal ("cannot read %s", filename);
	} else
		fd = stdin;

	if (format)
		FormatInit (format);

	((short*) DBexecBuf)[d->nVars] = d->fixparmleng;

	/* Составляем вектора адресов и типов параметров */
	nvar = 0;
	scan = d->elements;
	for (n=0; n<d->paramNumber; ++n, ++scan) {
		Args[n] = DBexecBuf + scan->shift;
		switch (scan->type) {
		case DBshort:   ArgTypes[n] = 's';              break;
		case DBlong:    ArgTypes[n] = 'l';              break;
		case DBfloat:   ArgTypes[n] = 'd';              break;
		case DBchar:    ArgTypes[n] = - scan->length;   break;
		case DBvarchar: ++nvar;                         break;
		}
	}
	if (nvar) {
		if (! VarBuf || VarBufSize<nvar*MAXVARCHAR) {
			if (VarBuf)
				free (VarBuf);
			VarBufSize = nvar * MAXVARCHAR;
			VarBuf = malloc (VarBufSize);
			if (! VarBuf)
				fatal ("no memory for varchar");

		}
		/* Переменные varchar вводим в буфер VarBuf */
		scan = d->elements;
		nvar = 0;
		for (n=0; n<d->paramNumber; ++n, ++scan) {
			Args[n] = DBexecBuf + scan->shift;
			if (scan->type == DBvarchar) {
				Args[n] = VarBuf + nvar * MAXVARCHAR;
				ArgTypes[n] = -MAXVARCHAR;
				++nvar;
			}
		}
	}
	for (;;) {
		/* Вводим числа по формату, с проверкой типа и размера */
		if (format) {
			reply = ScanFormat (fd, d->paramNumber, Args, ArgTypes);
			if (!reply && feof (fd))
				break;
			if (reply != d->paramNumber)
				fatal ("format input: %s", format);
		} else {
			reply = Scan (fd, d->paramNumber, Args, ArgTypes);
			if (!reply && feof (fd))
				break;
			if (reply != d->paramNumber)
				fatal ("input");
		}

		/* Корректируем поля типа varchar */
		if (nvar) {
			scan = d->elements;
			for (n=0; n<d->paramNumber; ++n, ++scan) {
				if (scan->type == DBvarchar) {
					VarCharDescr *vc = (VarCharDescr*)
						(DBexecBuf + scan->shift);
					char *str = Args[n];
					int len = MAXVARCHAR;

					while (len && str[len-1]==' ')
						--len;
					memcpy (VarCharBeg (vc), str, len);
					vc->end = vc->beg + len + SHORTSZ;
				}
			}
		}
		if (debug)
			PrintQuote (stdout, d->paramNumber, Args, ArgTypes);
		for (n=0; n<nDescr; ++n)
			if (DBlexec (Descr[n]) != 0)
				fataldb (list[n]);
	}
	if (fd != stdin)
		fclose (fd);
}

void PutData (char *filename, char *format, DBdescriptor d, char *query)
{
	DBelement* scan;
	int reply, n;
	FILE *fd;

	if (filename) {
		fd = fopen (filename, "w");
		if (! fd)
			fatal ("cannot write %s", filename);
	} else
		fd = stdout;

	if (format)
		FormatInit (format);

	while ((reply = DBlfetch (d)) != 0) {
		if (reply < 0)
			fataldb (query);

		/* Составляем вектора адресов и типов параметров */
		scan = d->elements + d->paramNumber;
		for (n=0; n<d->columnNumber; ++n, ++scan) {
			Args[n] = DBbuffer + scan->shift;
			switch (scan->type) {
			case DBshort:
				ArgTypes[n] = 's';
				break;
			case DBlong:
				ArgTypes[n] = 'l';
				break;
			case DBfloat:
				ArgTypes[n] = 'd';
				break;
			case DBvarchar:
				ArgTypes[n] = - VarCharLen (Args[n]);
				Args[n] = VarCharBeg (Args[n]);
				break;
			case DBchar:
				ArgTypes[n] = - scan->length;
			}
		}

		/* Корректируем поля типа varchar */
		scan = d->elements + d->paramNumber;
		for (n=0; n<d->columnNumber; ++n, ++scan) {
			if (scan->type == DBvarchar || scan->type == DBchar) {
				char *str = Args[n];
				int maxlen = - ArgTypes[n];

				while (maxlen && str[maxlen-1]==' ')
					--maxlen;
				ArgTypes[n] = - maxlen;
			}
		}

		/* Выводим числа по формату, с проверкой типа и размера */
		if (format) {
			reply = PrintFormat (fd, d->columnNumber, Args, ArgTypes);
			if (reply != d->columnNumber)
				fatal ("format output: %s", format);
		} else {
			reply = Print (fd, d->columnNumber, Args, ArgTypes);
			if (reply != d->columnNumber)
				fatal ("output");
		}
	}
	if (format)
		PrintFlush (fd);
	if (fd != stdout)
		fclose (fd);
}
