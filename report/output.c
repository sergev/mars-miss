/*
 * Формирование отчета
 */
#include <stdio.h>
#include <string.h>
#include "report.h"

static char     OutLine [LineStep*MaxHigh];
static int      OutHigh;                        /* высота записи */

static void     WriteRecord (void);
static void     SetGroups (int);
static void     SummValues (void);
static int      VCount (void);
static void     FormField (char*, desc_elem*, char*);
static int      FormChar (desc_elem*,char*);
static void     FormSlice (char*, int, desc_elem*, char*);
static void     CheckPage (int, boolean);
static void     WriteBlock (text_block*, int);
static int      FetchLine (void);

extern int      LtoText (long, int*, char*);

void FormPaper ()
{
	int basegroup;

	/* зачитываем первую запись отчета */
	if (FetchLine() == 0)
		MyError (RErrEmpty);
	memcpy (DBsaved, DBbuffer, *(short*) DBbuffer);

	basegroup = 0;
	do {
		int size, level, elevel;

		/* высота 1 записи группы */
		OutHigh = VCount();

		/* занесем новые значения  */
		SetGroups (basegroup);

		/* подсчитаем длину выводимых заголовков */
		size = InfoTail.nlines;
		for (level=basegroup; level<Ngroups; ++level) {
			size += GroupHeads[level]->nlines;
			/* проверим, не на новой ли странице ее выводить */
			if (GroupHeads[level]->mode == Rpage &&
			    LineCount > PageHead.nlines)
				size += PageSize;
		}

		/* заголовки и 1 порция данных */
		CheckPage (size+OutHigh, FALSE);

		/* выведем все заголовки */
		for (level=basegroup; level<Ngroups; ++level)
			WriteBlock (GroupHeads[level], level);

		/* зачитаем следующую запись */
		while ((basegroup = FetchLine ()) >= Ngroups) {
			/* это идет текущая группа */
			CheckPage (OutHigh + InfoTail.nlines, TRUE);
			SummValues ();          /* внесли предыдущ в сум. */
			WriteRecord ();         /* вывели предыдущую */
			OutHigh = VCount ();    /* подсчитаем длину новой */
		}

		/* подсчитаем длины концевиков */
		size = 0;
		level = Ngroups;
		while (--level>=basegroup && GroupTails[level]->mode != Rpage)
			size += GroupTails[level]->nlines;

		CheckPage (size+OutHigh, TRUE); /* место под концы и посл */
		elevel = level;

		SummValues ();                  /* внесли последн. в сум. */
		WriteRecord ();                 /* вывели пoследнюю */
		for (level=Ngroups; --level>=basegroup; ) {
			if (level == PSlevel && PrecCount != 0) {
				WriteBlock (&PageSumm, Ngroups);
				PrecCount = 0;
			}
			if (level == elevel)
				CheckPage (PageSize, FALSE);
			WriteBlock (GroupTails[level], level);
		}
	} while (basegroup != 0);               /* конец таблицы */

	WriteBlock (&PageTail, 0);
}

/*
 * Проверка перехода на новую страницу
 */
static void CheckPage (int size, boolean info)
{
	if (LineCount + size + PageSumm.nlines + PageTail.nlines > PageSize) {
		if (info)                       /* идет группа -> концовка */
			WriteBlock (&InfoTail, Ngroups-1);
		if (PrecCount)                  /* выводим сумму по странице */
			WriteBlock (&PageSumm, Ngroups);
		WriteBlock (&PageTail, 0);      /* выводим концевик страницы */
		SetGroups (Ngroups);
		LineCount = 0;
	}
	if (LineCount == 0) {                   /* выводим заголовок страницы */
		++PageCount;
		PrecCount = 0;
		WriteBlock (&PageHead, 0);
		if (info)                       /* идет группа -> заголовок */
			WriteBlock (&InfoHead, Ngroups-1);
	}
}

/*
 * Чтение очередной строки, обнаружение прерываний.
 * Код возврата: 0 -> конец файла, 1,2 -> конец соотв. группы.
 */
static int FetchLine ()
{
	static boolean first = TRUE;
	int level, retcode;

	retcode = DBlfetch (Mdescr);
	if (retcode < 0)
		dberror ();     /* сообщение об ошибке из СУБД */
	if (retcode == 0)
		return (0);     /* таблица кончилась */

	/* для первой строки на этом все кончится */
	if (first) {
		first = FALSE;
		return (1);
	}

	/* проверим группирование */
	level = 0;
	while (++level < Ngroups) {
		desc_elem *gdescr = Columns [Groups [level]];

		if (memcmp (gdescr->value - DBsaved + DBbuffer,
		    gdescr->svalue, gdescr->length))
			break;
	}
	return (level);
}

/*
 * Занесение новых значений группирования
 */
static void SetGroups (int level)
{
	int i, last;
	desc_elem *fdescr;

	/* Обнулим суммируемые значения.
	 * Сумму по странице обнуляем, если конец страницы
	 * или конец ее группы. */
	last = Ngroups + (level == Ngroups || level <= PSlevel);
	for (i=0; i<Mdescr->columnNumber; ++i) {
		fdescr = Columns[i];

		if (fdescr->fclass == Esumm) {
			memset (fdescr->svalue + fdescr->length*level, 0,
				fdescr->length * (last-level));
		}
	}
	if (level < 1)
		level = 1;
	for (; level<Ngroups; ++level) {        /* заполним новыми значениями */
		fdescr = Columns [Groups [level]];
		memcpy (fdescr->svalue, fdescr->value, fdescr->length);
	}
}

/*
 * Суммирование суммируемых полей
 */
static void SummValues ()
{
	int i, j;
	desc_elem *fdescr;
	char *data;

	/* цикл по всем выходным полям */
	for (i=0; i<Mdescr->columnNumber; ++i) {
		fdescr = Columns[i];
		if (fdescr->fclass != Esumm)
			continue;
		data = fdescr->svalue;
		for (j=0; j<Ngroups+1; ++j) {   /* прибавляем ко всем част. */
			switch (fdescr->type) {
			case DBshort:
				*(short*) data += *(short*) fdescr->value;
				break;
			case DBlong:
				*(long*) data += *(long*) fdescr->value;
				break;
			case DBfloat:
				*(double*) data += *(double*) fdescr->value;
				break;
			}
			data += fdescr->length;
		}
	}
}

/*
 * Формирование строки в многострочном поле
 * выводимое данное задается (addr,length), описатель формат - fdescr
 * адрес выводного поля - result
 */
static void FormSlice (char *addr, int len, desc_elem *fdescr, char *result)
{
	short fwidth = fdescr->formatwidth;     /* ширина поля */

	while (len != 0 && addr[len-1] == ' ')
		--len;

	memset (result, ' ', fwidth);
	switch (fdescr->formatsign) {           /* выравнивание */
	case 'L':                               /* к левому краю */
		memcpy (result, addr, len);
		break;
	case 'R':                               /* к правому краю */
		memcpy (result + fwidth-len, addr, len);
		break;
	case 'C':                               /* по центру */
		memcpy (result + (fwidth-len)/2, addr, len);
		break;
	}
}

/*
 * Формирование текстового поля
 * Выход: высота поля
 */
static int FormChar (desc_elem *fdescr, char *result)
{
	char *addr = fdescr->value;
	unsigned len = fdescr->length;
	short wfield = fdescr->formatwidth;
	int nlines, divide;

	/* обрезаем концевые пробелы */
	while (len != 0 && addr[len-1] == ' ')
		--len;

	/* пока не влезает в строку  */
	for (nlines=1; len>wfield; ++nlines) {
		for (divide=wfield; divide != 0 && addr[divide-1] != ' '; )
			--divide;
		if (divide == 0 || divide < wfield*2/3)
			divide = wfield;
		if(result != NIL) {             /* пора заносить текст */
			FormSlice (addr, divide, fdescr, result);
			result += LineStep;
		}
		addr += divide;
		len -= divide;
	}
	if (result != NIL)                      /* пора заносить текст */
		FormSlice (addr, len, fdescr, result);
	return (nlines);
}

/*
 * Запись данных в выходную строку
 */
static void FormField (char* dataptr, desc_elem* fdescr, char* buff)
{
	char textbuf [200], *ptr;

	if (fdescr->type == DBchar) {
		memcpy (buff, dataptr, fdescr->formatwidth);
		return;
	}
	if (fdescr->formatsign == '$') {
		long value;
		int ltext, mode = 0;

		switch (fdescr->type) {
		case DBshort:   value = *(short*) dataptr;      break;
		case DBfloat:   value = *(double*) dataptr;     break;
		default:        value = *(long*) dataptr;       break;
		}
		ptr = textbuf + LtoText (value/100, &mode, textbuf);
		sprintf (ptr, " руб. %ld коп.", value%100);
		for (ltext=strlen(textbuf); ltext<fdescr->formatwidth; ++ltext)
			textbuf[ltext] = ' ';
		memcpy (buff, textbuf, fdescr->formatwidth);
		return;
	}
	switch (fdescr->formatsign) {
	case 'F':               ptr = "%*.*f";  break;
	case 'E': case 'D':     ptr = "%*.*e";  break;
	default:                ptr = "%*.*g";  break;
	}
	switch (fdescr->type) {
	case DBshort:
		sprintf (textbuf, "%*d", fdescr->formatwidth,
			*(short*) dataptr);
		break;
	case DBlong:
		sprintf (textbuf, "%*ld", fdescr->formatwidth,
			*(long*) dataptr);
		break;
	case DBfloat:
		sprintf (textbuf, ptr, fdescr->formatwidth,
			fdescr->suplinfo, *(double*) dataptr);
		break;
	}
	memcpy (buff, textbuf, fdescr->formatwidth);
}

/*
 * Вывод блока текста
 */
static void WriteBlock (text_block *descr, int level)
{
	textline *lscan;
	textfield *fscan;

	if (descr == &PageTail) {       /* хвостовик страницы в конец ! */
		while (LineCount+PageTail.nlines != PageSize &&
		    PageTail.nlines != 0) {
			PutLine ("");
			++LineCount;
		}
	}
	for (lscan=descr->lines; lscan; lscan=lscan->next) {
		/* заносим поля в заголовки */
		for (fscan=lscan->fields; fscan; fscan=fscan->next) {
			desc_elem *ddescr = fscan->data;
			char *dataptr;

			switch (ddescr->fclass) {
			case Esumm:
				dataptr = ddescr->svalue + level * ddescr->length;
				break;
			case Egroup:
				dataptr = ddescr->svalue;
				break;
			default:
				dataptr = ddescr->value;
				break;
			}
			FormField (dataptr, ddescr, lscan->text + fscan->position);
		}
		PutLine (lscan->text);
	}
	LineCount += descr->nlines;
}

/*
 * Выдача инф. строки
 */
static void WriteRecord ()
{
	textline *lscan;
	textline *llscan;
	char *outscan = OutLine;
	char *first;
	int nlines  = 0;

	++PrecCount;
	++RecCount;

	if (! Ualign) {
		while (nlines < OutHigh - DataLine.nlines) {
			memcpy (outscan, DataLine.lines->text, LineStep);
			outscan += LineStep;
			++nlines;
		}
	}

	first = outscan;

	/* формируем фикс. часть записи */
	for (lscan=DataLine.lines; lscan; lscan=lscan->next) {
		llscan = lscan;
		memcpy (outscan, lscan->text, LineStep);
		outscan += LineStep;
		++nlines;
	}

	if (Ualign) {
		while (nlines < OutHigh) {
			memcpy (outscan, llscan->text, LineStep);
			outscan += LineStep;
			++nlines;
		}
	}

	/* теперь занесем все поля */
	outscan = first;
	for (lscan=DataLine.lines; lscan; lscan=lscan->next) {
		textfield *fscan;
		for (fscan=lscan->fields; fscan; fscan=fscan->next) {
			desc_elem *ddescr = fscan->data;

			if (ddescr->type != DBchar)
				FormField (ddescr->value, ddescr, outscan + fscan->position);
			else if (Ualign)
				FormChar (ddescr, outscan + fscan->position);
			else
				FormChar (ddescr, outscan + fscan->position -
					  (ddescr->suplinfo - 1) * LineStep);
		}
		outscan += LineStep;
	}

	/* выведем получившееся в файл */
	for(nlines=0; nlines<OutHigh; ++nlines)
		PutLine (OutLine + nlines*LineStep);
	LineCount += OutHigh;

	/* теперь спасаем текущую строку */
	memcpy (DBsaved, DBbuffer, *(short*) DBbuffer);
}

/*
 * Подсчет высоты считанной записи данных
 * для всех строковых данных заполняется поле suplinfo: высота поля - 1
 */
static int VCount ()
{
	textline *lscan;
	int delta = 0;
	int vshift = 0;

	/* проходим по всем строкам записи */
	for (lscan=DataLine.lines; lscan; lscan=lscan->next) {
		textfield *fscan;

		/* проходим по всем полям строки */
		for (fscan=lscan->fields; fscan; fscan=fscan->next) {
			desc_elem *ddescr = fscan->data;

			if (ddescr->type == DBchar) {
				int theHigh = FormChar (ddescr,NIL);
				ddescr->suplinfo = theHigh;
				if (Ualign)
					theHigh -= DataLine.nlines - vshift;
				else
					theHigh -= vshift + 1;
				if (delta < theHigh)
					delta = theHigh;
			}
		}
		++vshift;
	}
	delta += DataLine.nlines;
	if (delta >= MaxHigh)
		MyError (RErrHigh);
	return (delta);
}
