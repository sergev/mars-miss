/*
 * Генератор отчетов
 */
#include <stdio.h>
#ifndef NO_STDLIB
#include <stdlib.h>
#endif
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "report.h"

short           PageCount = 0;          /* номер страницы */
short           LineCount = 0;          /* номер строки */
short           RecCount = 0;           /* номер записи */
short           PrecCount = 0;          /* номер записи с начала страницы */

int             PageSize = 66;          /* размер страницы */
int             Ngroups = 1;            /* количество группируемых */
int             PSlevel = -1;           /* уровень страничных сумм */

boolean         Ualign = TRUE;          /* выравнивание к верху */

char*           OutputFileName;         /* имя файла или трубы */
char*           UserName;               /* имя пользователя */
char*           Password;               /* пароль */

short           Groups [MaxGroups];     /* номера группируемых полей */
text_block*     GroupHeads [MaxGroups];
text_block*     GroupTails [MaxGroups];

text_block      PageSumm = { NIL, 0, 0, };
text_block      PageHead = { NIL, 0, 0, };
text_block      PageTail = { NIL, 0, 0, };
text_block      InfoHead = { NIL, 0, 0, };
text_block      InfoTail = { NIL, 0, 0, };

text_block      DataLine = { NIL, 0, 0, };

int             DBtextlen = 0;
char            DBsaved [MaxDbOut];

char*           Builtins [Nbuiltin] = { "P", "N", "R", "DATE", "TM", };

static char     dateBuf [] = "00.00.0000";
static char     timeBuf [] = "00:00";

static desc_elem BuiltinRec [Nbuiltin] = {
	Ebuiltin, 0, 'L', DBchar, 5, 5, 0, timeBuf, NIL,
	Ebuiltin, 0, 'L', DBchar, 10, 10, 0, dateBuf, NIL,
	Ebuiltin, 0, 'I', DBshort, sizeof (RecCount),
		4, 0, (char*) &RecCount, NIL,
	Ebuiltin, 0, 'I', DBshort, sizeof (PrecCount),
		4, 0, (char*) &PrecCount, NIL,
	Ebuiltin, 0, 'I', DBshort, sizeof (PageCount),
		4, 0, (char*) &PageCount, NIL,
};

static desc_elem* ColumnTab [MaxColumns+Nbuiltin] = {
	BuiltinRec,     /* &TM - время */
	BuiltinRec+1,   /* &DATE - дата */
	BuiltinRec+2,   /* &R - номер записи */
	BuiltinRec+3,   /* &N - номер записи с начала страницы */
	BuiltinRec+4,   /* &P - номер страницы */
	NIL,
};

desc_elem **Columns = ColumnTab + Nbuiltin;

static FILE*    Output;                 /* выходной файл или труба */
static char*    InputText;              /* текст описания отчета */

extern char *optarg;
extern int optind;

static void ParmAsk (void);
extern void exit (int);

/*
 * Usage:
 * marsreport [options] -c report arg...
 * marsreport [options] reportfile arg...
 * marsreport [options] < reportfile
 * marsreport [options] - arg... < reportfile
 * Options:
 * -u user
 * -p password
 * -o rezultfile
 * -l pagelength
 */

int main (int argc, char **argv)
{
	char *iname = 0, *oname = 0;
	long tim;
	struct tm *t;

        Output = stdout;
	for (;;) {
		switch (getopt (argc, argv, "u:p:o:l:c:")) {
		case EOF:
			break;
		case 'u':
			UserName = optarg;
			continue;
		case 'p':
			Password = optarg;
			continue;
		case 'o':
			oname = optarg;
			continue;
		case 'l':
			PageSize = atoi (optarg);
			continue;
		case 'c':
			InputText = optarg;
			continue;
		default:
			fprintf (stderr, "Usage:\n");
			fprintf (stderr, "\tmarsreport [options] -c report arg...\n");
			fprintf (stderr, "\tmarsreport [options] reportfile arg...\n");
			fprintf (stderr, "\tmarsreport [options] < reportfile\n");
			fprintf (stderr, "\tmarsreport [options] - arg... < reportfile\n");
			fprintf (stderr, "Options:\n");
			fprintf (stderr, "\t-u user\n");
			fprintf (stderr, "\t-p password\n");
			fprintf (stderr, "\t-o rezultfile\n");
			fprintf (stderr, "\t-l pagelength\n");
			return (-1);
		}
		break;
	}
	argc -= optind;
	argv += optind;

	if (! InputText && argc > 0) {
		if (strcmp (*argv, "-") != 0)
			iname = *argv;
		--argc;
		++argv;
	}

	if (iname && freopen (iname, "r", stdin) != stdin) {
		fprintf (stderr, "Cannot read %s\n", iname);
		return (-1);
	}

	/* проинициализируем дату и время */
	time (&tim);
	t = localtime (&tim);
	sprintf (dateBuf, "%2d.%02d.%04d", t->tm_mday, t->tm_mon+1,
		t->tm_year+1900);
	sprintf (timeBuf, "%02d:%02d", t->tm_hour, t->tm_min);

	DBconnect ();                   /* подключаемся к СУБД */
	Translate ();                   /* чтение и трансляция запроса */
	ScanFile ();                    /* обрабатываем описания */
	ParmAsk ();                     /* запрос параметров, EXEC */

	if (oname)
		OutputFileName = oname;
	if (OutputFileName) {
		if (*OutputFileName == '|')
			Output = popen (OutputFileName+1, "w");
		else if (strcmp (OutputFileName, "-") != 0)
			Output = fopen (OutputFileName, "w");
		if (! Output) {
			fprintf (stderr, "Cannot write to %s\n", OutputFileName);
			exit (-1);
		}
	}

	FormPaper ();                   /* выводим текст отчета  */
	DBdisconn ();                   /* отлетаем от СУБД */

	if (OutputFileName && *OutputFileName == '|')
		pclose (Output);
	return (0);
}

/*
 * Выдача сообщений об ошибках
 */
void dberror ()
{
	fprintf (stderr, "Database error: %s\n", DBerror (DBinfo.errCode));
	fprintf (stderr, "*** %s\n", DBexecBuf);
	if (DBinfo.errPosition >= 0)
		fprintf (stderr, "*** %*s\n", DBinfo.errPosition, "^");
	exit (-1);
}

static void ParmAsk ()
{
	if (Mdescr->paramNumber != 0) {
		fprintf (stderr, "Parameterized queries not implemented yet\n");
		exit (-1);
	}
	if (DBexec (Mdescr) != 0)
		dberror ();
}

void PutLine (char *line)
{
	if (*line)
		fputs (line, Output);
	putc ('\n', Output);
}

int GetChar ()
{
	if (InputText) {
		if (! *InputText)
			return (-1);
		return (*InputText++);
	}
	return (getchar ());
}

int EndOfFile ()
{
	if (InputText)
		return (! *InputText);
	return (feof (stdin));
}
