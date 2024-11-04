/*
 * Обработка описаний
 */
#include <stdio.h>
#ifndef NO_STDLIB
#include <stdlib.h>
#endif
#include <string.h>
#include "report.h"

static char DescrMBuf [1000];
DBdescriptor Mdescr = (DBdescriptor) DescrMBuf;
DBelement *Cdescr;

static char *scan;
static char SourceLine [MaxSource+2];
static int SourceLineNum = 0;

static void ReadBlock (text_block*, int);
static int LookField (char*, int);

extern char *getpass (char*);

/*
 * Преобразование символа в верхний регистр.
 * Только для КОИ8.
 */
static inline int UpperCase (register unsigned char c)
{
	if (c>='a' && c<='z')           c += 'A' - 'a'; /* a - z */
	else if (c>=0300 && c<0340)     c += 040;       /* ю - ъ */
	else if (c == 0243)             c = 0263;       /* ё */
	return (c);
}

/*
 * Чтение строки из входного потока в буфер SourceLine.
 * Пропускает строчные комментарии (#, %, \).
 * Удаляет \n на конце.  Возвращает -1 если поток закончился.
 * Устанавливает scan на SourceLine.
 */
static int GetLine ()
{
	do {
		int c, i = 0;
		while (i<MaxSource-1 && (c = GetChar ()) != EOF && c!='\n')
			SourceLine[i++] = c;
		SourceLine[i] = 0;
		if (c == EOF && i == 0) {
			SourceLineNum = 0;
			return (-1);
		}
		++SourceLineNum;
	} while (*SourceLine=='#' || *SourceLine=='%' || *SourceLine=='\\');
	scan = SourceLine;
	return (1);
}

static inline void SkipSpaces ()
{
	do {
		while (*scan == ' ' || *scan == '\t')
			++scan;
	} while (! *scan && GetLine () >= 0);
}

static inline int EndOfLine ()
{
	while (*scan == ' ' || *scan == '\t')
		++scan;
	return (*scan == 0);
}

/*
 * Проверка следующего символа в ключевой строке
 */
static inline void CheckSym (int sym)
{
	SkipSpaces ();
	if (*scan != sym)
		MyError (RErrSyntax);
	++scan;
}

static inline void CheckEndOfLine (void)
{
	while (*scan == ' ' || *scan == '\t')
		++scan;
	if (*scan)
		MyError (RErrSyntax);
}

/*
 * Проверка значения ключа в ключевой строке
 */
static boolean CheckKey (char *key)
{
	char *oldscan;

	SkipSpaces ();
	if (! *scan)
		return (FALSE);
	for (oldscan=scan; *key; ++key)
		if (*key == ' ')
			SkipSpaces ();
		else if (*key != UpperCase (*scan++))
			return (scan=oldscan, FALSE);
	SkipSpaces ();
	return (TRUE);
}

/*
 * Чтение числа из потока
 */
static short ReadNumber ()
{
	short value = 0;

	SkipSpaces ();
	if (*scan <'0' || *scan >'9')
		MyError (RErrSyntax);
	do {
		value = *scan++ - '0' + value*10;
		if (value >= 1000)
			MyError (RErrSyntax);
	} while (*scan >='0' && *scan <= '9');
	return (value);
}

static char *GetMemory (int sz)
{
	char *mem = malloc (sz);
	if (! sz) {
		fprintf (stderr, "Out of memory\n");
		exit (-1);
	}
	return (mem);
}

void ScanFile ()
{
	int i;

	/* Неявные значение форматов */
	for (i=0; i<Mdescr->columnNumber; ++i) {
		DBelement *bdescr = Cdescr + i;
		desc_elem *fdescr = Columns[i];

		fdescr->suplinfo = 0 ;
		fdescr->type = bdescr->type;
		fdescr->length = bdescr->length;
		fdescr->fclass = Edata;

		switch (bdescr->type) {
		case DBshort:
			fdescr->formatsign  = 'I';
			fdescr->formatwidth =  6 ;
			break;
		case DBlong:
			fdescr->formatsign  = 'I';
			fdescr->formatwidth = 11 ;
			break;
		case DBfloat:
			fdescr->formatsign  = 'G';
			fdescr->formatwidth = 13 ;
			fdescr->suplinfo    =  7 ;
			break;
		case DBchar:
			fdescr->formatsign  = 'L';
			fdescr->formatwidth = bdescr->length;
			break;
		default:
			fprintf (stderr, "Invalid format descriptor: %d\n",
				bdescr->type);
			exit (-1);
		}
	}

	GetLine ();

	if (CheckKey ("FILE")) {                /* #FILE-строка */
		char *fname;

		CheckSym ('"');
		for (fname=scan; *scan!='"'; ++scan)
			if (*scan == 0)
				MyError (RErrSyntax);
		*scan++ = 0;
		OutputFileName = malloc ((unsigned) strlen (fname) + 1);
		if (OutputFileName)
			strcpy (OutputFileName, fname);
		CheckEndOfLine ();
		GetLine ();
	}

	if (CheckKey ("FORMAT")) {               /* #FORMAT-строка */
		int spec, clen;
		char *cname;
		desc_elem *fdescr;

		CheckSym ('{');
		for (;;) {
			while (EndOfLine ())
				if (GetLine () < 0)
					MyError (RErrSyntax);
			cname = scan;
			while (*scan && *scan!=' ' && *scan!='\t' && *scan!='=')
				++scan;
			clen = scan - cname;
			CheckSym ('=');
			SkipSpaces ();

			/* установим, формат какого столбца мы меняем */
			fdescr = Columns [LookField (cname, clen)];
			fdescr->formatsign = spec = UpperCase (*scan);
			if (fdescr->type == DBchar) {
				switch (spec) {
				default:
					MyError(RErrFormat);
				case 'L': case 'R': case 'C':
					break;
				}
				++scan;
				fdescr->formatwidth = ReadNumber ();
				if (fdescr->formatwidth == 0 ||
				    fdescr->formatwidth > fdescr->length)
					MyError (RErrFormat);
			} else if (spec == '$') {
				++scan;
				fdescr->formatwidth = ReadNumber ();
				if (fdescr->formatwidth < 10)
					MyError (RErrFormat);
			} else {
				switch (spec) {
				default:
					MyError (RErrFormat);
				case 'E': case 'F': case 'G': case 'D':
				case 'I':
					break;
				}
				++scan;
				fdescr->formatwidth = ReadNumber ();
				if (spec != 'I') {
					if (*scan != '.')
						MyError (RErrFormat);
					++scan;
					fdescr->suplinfo = ReadNumber ();
					if (fdescr->suplinfo > fdescr->formatwidth)
						/* d больше w в Fw.d */
						MyError (RErrFormat);
				}
			}
			SkipSpaces ();
			if (*scan == '}')
				break;
			CheckSym (',');
		}
		CheckSym ('}');
		CheckEndOfLine ();
		GetLine ();
	}

	if (CheckKey ("ALIGN")) {       /* выравнивание полей по вертик. */
		if (CheckKey ("UP"))
			Ualign = 1;
		else if (CheckKey ("DOWN"))
			Ualign = 0;
		else
			MyError (RErrSyntax);
	}

	if (CheckKey ("PAGE SIZE"))     /* размер листа по вертикали */
		PageSize = ReadNumber ();

	/* чтение списка полей суммирования */
	if (CheckKey ("SUM")) {
		CheckSym ('{');
		for (;;) {
			desc_elem *fdescr;
			char *cname;
			int clen;

			while (EndOfLine ())
				if (GetLine () < 0)
					MyError (RErrSyntax);
			cname = scan;
			while (*scan && *scan!=',' && *scan!=' ' &&
			    *scan!='\t' && *scan!='}')
				++scan;
			clen = scan - cname;

			fdescr = Columns [LookField (cname, clen)];
			if (fdescr->fclass != Edata || fdescr->type == DBchar)
				MyError (RErrSumm);
			fdescr->fclass = Esumm;

			SkipSpaces ();
			if (*scan == '}')
				break;
			CheckSym (',');
		}
		GetLine ();
	}

	if (CheckKey ("LINE"))                  /* %LINE-строка */
		ReadBlock (&DataLine, 2);
	else {
		int shift = 0;
		textfield **flist, *fdescr;

		DataLine.lines = (textline*) GetMemory (sizeof (textline));
		DataLine.lines->next = NIL;
		flist = &DataLine.lines->fields;
		for (i=0; i<Mdescr->columnNumber; ++i) {
			fdescr = (textfield*) GetMemory (sizeof (textfield));
			fdescr->position = ++shift;
			fdescr->data = Columns[i];
			shift += fdescr->data->formatwidth + 1;
			*flist = fdescr;
			flist = &fdescr->next;
		}
		*flist = NIL;
		DataLine.lines->text = GetMemory (shift+1);
		memset (DataLine.lines->text, ' ', shift);
		DataLine.nlines = 1;
		DataLine.lines->text[shift] = 0;
	}

	/* чтение заголовка/концевика отчета */
	*GroupHeads = (text_block*) GetMemory (sizeof (text_block));
	*GroupTails = (text_block*) GetMemory (sizeof (text_block));

	(*GroupHeads)->lines = (*GroupTails)->lines = NIL;
	(*GroupHeads)->nlines = (*GroupTails)->nlines = 0;

	if (CheckKey ("REPORT HEADING"))        /* заголовок отчета */
		ReadBlock (*GroupHeads, 0);

	if (CheckKey ("PAGE SUMMARY")) {        /* страничные суммы */
		PSlevel = 0;
		ReadBlock (&PageSumm, 1);
	}

	if (CheckKey ("REPORT FOOTING"))        /* концевик отчета */
		ReadBlock (*GroupTails, 1);

	/* чтение списка полей группирования */
	while (CheckKey ("GROUP")) {
		DBelement *bdescr;
		text_block *footing;
		char *cname;
		int clen;

		if (Ngroups >= MaxGroups)
			MyError (RErrManyGroups);
		footing = (text_block*) GetMemory (sizeof (text_block));

		cname = scan;
		while (*scan && *scan!=' ' && *scan!='\t' &&
		    *scan!='(' && *scan!='{')
			++scan;
		clen = scan - cname;

		i = LookField (cname, clen);            /* ищем столбец */
		bdescr = Cdescr + i;
		Groups[Ngroups] = i;                    /* номер колонки */
		if (Columns[i]->fclass != Edata)
			MyError (RErrGroup);
		Columns[i]->fclass = Egroup;            /* поле группирования */
		GroupHeads[Ngroups] = (text_block*) GetMemory (sizeof (text_block));
		ReadBlock (GroupHeads[Ngroups], 0);

		if (CheckKey ("PAGE SUMMARY")) {        /* страничные суммы */
			if (PSlevel >= 0)
				MyError (RErrPSmult);
			PSlevel = Ngroups;
			ReadBlock (&PageSumm, 1);
		}

		/* читаем концевик группы */
		if (CheckKey ("GROUP FOOTING"))
			ReadBlock (footing, 1);
		else {
			footing->nlines = 0;
			footing->lines = NIL;
			footing->mode = Rnormal;
		}
		GroupTails[Ngroups] = footing;

		/* отведем память под хранимое значение */
		Columns[i]->svalue = GetMemory (bdescr->length);
		++Ngroups;
	}

	if (CheckKey ("INFO HEADING"))          /* заголовок информации */
		ReadBlock (&InfoHead, 0);

	if (CheckKey ("INFO FOOTING"))          /* заголовок информации */
		ReadBlock (&InfoTail, 1);

	if (CheckKey ("PAGE HEADING"))          /* заголовок страницы */
		ReadBlock (&PageHead, -1);

	if (CheckKey ("PAGE FOOTING"))          /* концевик страницы */
		ReadBlock (&PageTail, -1);

	SkipSpaces ();
	if (! EndOfFile ())
		MyError (RErrKeyword);

	/* распределим память под значения суммируемых полей */
	for (i=0; i<Mdescr->columnNumber; ++i) {
		desc_elem *fdescr = Columns[i];
		if (fdescr->fclass == Esumm)
			fdescr->svalue = GetMemory (Cdescr[i].length *
				(Ngroups + 1));
	}
}

/*
 * Чтение блока текста (заголовки, концевики)
 * mode: -1: заголовок/концевик страницы
 */
static void ReadBlock (text_block *descr, int mode)
{
	textline **llist = &descr->lines;
	textline *theLine;
	textfield **flist;
	int len;

	descr->mode = Rnormal;
	descr->nlines = 0;

	*llist = NIL;
	SkipSpaces ();
	if (*scan == '(') {                     /* задан режим вывода */
		do {                            /* цикл по параметрам */
			++scan;
			SkipSpaces ();
			if (CheckKey ("PAGE"))          /* вывод с новой страницы */
				descr->mode = Rpage;
			else if (CheckKey ("ALONE"))    /* вывод на отдельн. стр. */
				descr->mode = Ralone;
			else
				MyError (RErrKeyword);
		} while (*scan == ',');         /* цикл по списку */
		CheckSym (')');
	}
	CheckSym ('{');
	CheckEndOfLine ();

	/* цикл чтения блока текста */
	for (;;) {
		if (GetLine () < 0)
			MyError (RErrSyntax);
		if (! strcmp (SourceLine, "}"))
			break;

		theLine = (textline*) GetMemory (sizeof (textline));
		flist = &theLine->fields;
		*flist = NIL;

		/* сканируем входную строку в поисках полей */
		len = strlen (SourceLine);
		for (scan=SourceLine; *scan; ++scan) {
			int flen, number;
			textfield *fdescr;
			desc_elem *ddescr;

			if (*scan != '{')
				continue;

			/* вставка поля в текст  */
			fdescr = (textfield*) GetMemory (sizeof (textfield));

			for (flen=1; scan[flen]!='}'; ++flen)
				if (scan[flen] == 0)
					MyError (RErrSyntax);

			number = LookField (scan+1, flen-1);
			ddescr = Columns [number];

			/* не суммируемые поля - нельзя */
			if (mode != 2 && ddescr->fclass == Edata)
				MyError (RErrGrValue);

			/* для Page h/f - только встроенные функции */
			/* if (mode < 0 && ddescr->fclass != Ebuiltin)
				MyError (RErrGrValue); */

			/* для не Page h/f - нельзя #страницы */
			/* if (mode >= 0 && number == -1)
				MyError (RErrGrValue); */

			/* для heading  - нельзя суммируемые поля */
			if (mode == 0 && ddescr->fclass == Esumm)
				MyError (RErrGrValue);

			fdescr->position = scan - SourceLine;
			fdescr->data = ddescr;

			/* проверим выпирание поля за границу строки */
			memset (scan, ' ', ++flen);
			while (scan + ddescr->formatwidth > SourceLine + len) {
				SourceLine[len] = ' ';
				if (++len >= MaxSource)
					MyError (RErrBigLine);
				SourceLine[len] = 0;
			}
			*flist = fdescr;
			flist = &fdescr->next;
			*flist = NIL;
		}

		/* заносим строку в память */
		theLine->text = GetMemory (len+1);
		memcpy (theLine->text, SourceLine, len+1);
		*llist = theLine;
		llist = &theLine->next;
		*llist = NIL;
		++descr->nlines;
	}
	GetLine ();
}

static void GetLogonName (char *nbuf, int sz)
{
	register int ch;
	FILE *fd = fopen ("/dev/tty", "r");

	if (! fd) {
		fprintf (stderr, "Cannot open /dev/tty\n");
		exit (-1);
	}
	for (;;) {
		char *p = nbuf;
		printf ("Logon: ");
		while ((ch = getc (fd)) != '\n') {
			if (ch == EOF)
				exit(0);
			if (p < nbuf + sz - 1)
				*p++ = ch;
		}
		if (p > nbuf) {
			*p = '\0';
			break;
		}
	}
	fclose (fd);
}

static void InpPassw (char *pname, char *name, char *pwd)
{
	int i;

	if (pname && *pname) {
		for (i=0; *pname && *pname!=' '; ++i) {
			if (i >= Lname)
				MyError (RErrSyntax);
			*name++ = *pname++;
		}
		while (*pname==' ')
			++pname;
	}
	*name = 0;
	if (pname && *pname) {
		for (i=0; *pname && *pname!=' '; ++i) {
			if (i >= Lname)
				MyError (RErrSyntax);
			*pwd++ = *pname++;
		}
	}
	*pwd = 0;
}

/*
 * Трансляция запросов
 */
void Translate ()
{
	char *pout = DBexecBuf;
	char string = 0;                        /* флаг чтение string-а */
	int retcode;

	GetLine ();

	if (CheckKey ("LOGON")) {               /* #LOGON - строка */
		static char user [40], pwd [40];
		char Logon [80];

		InpPassw (scan, user, pwd);
		if (! UserName) {
			UserName = user;
			if (! *user)
				GetLogonName (user, 40);
		}
		if (! Password) {
			Password = pwd;
			if (! pwd)
				strncpy (pwd, getpass ("Password: "), 40);
		}
		sprintf (Logon, "Logon %s %s", UserName, Password);
		/* затираем пароль во избежание неприятностей */
		memset (Password, 'X', strlen (Password));

		/* пытаемся идентифицироваться в базе */
		if (DBcompile (Logon, strlen (Logon), NIL, 0) != 0) {
			sprintf (DBexecBuf, "Logon %s %s", UserName, Password);
			dberror ();
		}
		GetLine ();
	}

	if (! CheckKey ("QUERY"))
		MyError (RErrSyntax);
	CheckSym ('{');

	strcpy (pout, "COMPILE ");
	pout += strlen (pout);
	for (;;) {
		if (pout-DBexecBuf >= 1000)
			MyError (RErrTooLong);
		while (EndOfLine ()) {
			if (GetLine () < 0)
				MyError (RErrSyntax);
			if (! string && pout[-1] != ' ')
				*pout++ = ' ';
		}
		if (string) {                           /* строка */
			if ((*pout++ = *scan++) == string)
				string = 0;
			continue;
		}
		if (*scan == '}')
			break;
		if (*scan == '"' || *scan == '\'')
			string = *scan;
		*pout++ = *scan++;
		if (*scan == ' ' || *scan == '\t')
			*pout++ = ' ';
	}
	CheckSym ('}');
	CheckEndOfLine ();

	DBtextlen = pout - DBexecBuf;
	retcode = DBcompile (NIL, DBtextlen, Mdescr, sizeof (DescrMBuf));
	if (retcode < 0)
		dberror();
	if (Mdescr->columnNumber == 0)
		MyError (RErrNotSelect);

	Cdescr = Mdescr->elements + Mdescr->paramNumber;

	for (retcode=0; retcode<Mdescr->columnNumber; ++retcode) {
		if (Cdescr[retcode].type == DBvarchar)
			MyError (RErrVarchar);
		Columns[retcode] = (desc_elem*) GetMemory (sizeof (desc_elem));
		Columns[retcode]->value = DBsaved + Cdescr[retcode].shift;
	}
}

/*
 * Поиск столбца по имени
 * если это - поле запроса, то: номер колонки
 * если это - спец-поле, то: -1-#спецполя
 */
static int LookField (char *name, int namelen)
{
	short namesNumber = Mdescr->columnNumber+Mdescr->paramNumber;
	short* namesTab = (short*) &Mdescr->elements[namesNumber] + namesNumber;
	char* names = (char*) namesTab;
	int from = Mdescr->paramNumber;
	int till = namesNumber;
	int shift = 0;
	int n = 0;

	while (namelen > 1 && name[namelen-1] == ' ')
		--namelen;

	/* поиск по спец-именам */
	if (*name == '&') {
		for (n=0; n<Nbuiltin; ++n) {
			char* text = Builtins[n];
			if (namelen-1 == strlen (text) &&
			    !memcmp (name+1, text, namelen-1))
				return (-1-n);
		}
		MyError (RErrName);
	}

	/* поиск по именам столбцов */
	for (;;) {
		if (n >= till)
			MyError (RErrName);
		if (n >= from && namesTab[-1-n] - shift == namelen &&
		    !memcmp (names+shift, name, namelen))
			break;
		shift = namesTab [-(++n)];
	}
	return (n-from);
}

void MyError (int code)
{
	static char *myerror [] = {
		"???",
		"Invalid syntax",       "Not select",
		"Too many columns",     "Too long",
		"Invalid name",         "Too big line",
		"Bad format",           "Too many groups",
		"Bad group",            "Empty object",
		"Bad group value",      "Bad varchar",
		"Bad summ",             "Invalid keyword",
		"?High",                "?PSmult",
	};
	if (SourceLineNum > 0) {
		fprintf (stderr, "Error in line %d: %s\n",
			SourceLineNum, myerror [code]);
		if (scan) {
			fprintf (stderr, "*** %s\n", SourceLine);
			fprintf (stderr, "*** %*s\n", scan-SourceLine+1, "^");
		}
	} else
		fprintf (stderr, "Error: %s\n", myerror [code]);
	exit (-1);
}
