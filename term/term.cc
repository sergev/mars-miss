//
// Интерактивный интерфейс с базой данных MARS.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#if defined(unix) || defined(__APPLE__)
        #include <sys/time.h>
#else
        #include <bios.h>
        struct timeval { unsigned long tv_sec, tv_usec; };
#endif
#include "Screen.h"
#include "mars.h"

const int MAXCOLS = 40;                 // максимальное количество колонок
const int MAXSTRING = 30;               // максимальная длина строки

Screen V;                               // Экран терминала
char Query [2000];                      // Буфер текстового запроса
int QueryLen;                           // Длина запроса
int Interrupted;                        // Флаг прерывания запроса
short *NameAddrs;                       // Адреса полей
int ColumnShift [MAXCOLS];              // Сдвиг полей на экране
int ColumnWidth [MAXCOLS];              // Ширина полей на экране
int TableWidth;                         // Ширина полученной таблицы
int statusLine;                         // Строка статуса
int messageLine;                        // Строка сообщения
char DescrBuf [400];                    // Буфер кода запроса
DBdescriptor Descr = (DBdescriptor) &DescrBuf; // Дескриптор запроса
struct timeval StartTime;               // Время запуска запроса
int VertBar = '\31';                    // Вертикальная черта

int NormalColor = 0x07;                 // Цвет основного текста
int InverseColor = 0x17;                // Цвет шапок
int PopupColor = 0x4f;                  // Цвет окна запросов
int BorderColor = 0x03;                 // Цвет рамок таблицы

void BuildHead ();
void PrintHead ();
void PrintTail ();
void PrintLine ();
void NextLine ();

int MemoryEdit (char *buf, int size, int *curs, int begline, int endline);

#if !defined(unix) && !defined(__APPLE__)
void gettimeofday (struct timeval *tv, int flag)
{
	long dostime = biostime (0, 0L);
	tv->tv_sec = dostime * 10 / 182;
	tv->tv_usec = (dostime - tv->tv_sec * 182 / 10) * 1000 / 182;
}
#endif

void SetMode (char *mode)
{
	V.Clear (statusLine, 0, 1, V.Columns, InverseColor,
		V.HasInverse () ? ' ' : '-');
	V.Print (statusLine, 1, InverseColor, " %s ", mode);
	if (DBhost) {
		char buf [80];
		sprintf (buf, " %s/%d ", DBhost, DBport);
		V.Put (statusLine, V.Columns - strlen (buf) - 1, buf, InverseColor);
	}
	V.Move (V.Lines-1, 0);
	V.Sync ();
}

void Message (char *msg=0, char *arg=0)
{
	struct timeval tv = { 0, 0, };

	V.Clear (messageLine, 0, 1, V.Columns, InverseColor,
		V.HasInverse () ? ' ' : '-' );
	if (msg) {
		V.Print (messageLine, 1, InverseColor, " %s ", msg);
		if (arg)
			V.Print (InverseColor, "%s ", arg);
		gettimeofday (&tv, 0);
		tv.tv_sec -= StartTime.tv_sec;
		tv.tv_usec -= StartTime.tv_usec;
		if (tv.tv_usec < 0) {
			tv.tv_usec += 1000000;
			--tv.tv_sec;
		}
	}
	char buf [40];
	if (tv.tv_sec >= 60)
		sprintf (buf, " %ld min %ld.%ld sec ", tv.tv_sec/60,
			tv.tv_sec%60, tv.tv_usec/1000);
	else if (tv.tv_sec > 0)
		sprintf (buf, " %ld.%ld sec ", tv.tv_sec, tv.tv_usec/1000);
	else
		sprintf (buf, " %ld msec ", tv.tv_usec/1000);
	V.Put (messageLine, V.Columns - strlen (buf) - 1, buf, InverseColor);
}

extern "C" short MyWait (short code)
{
	if (code > 0)
		Message ("Locked...");
	else if (code < 0)
		Message ("Ready");
	else
		Message ("Running...");
	V.Move (V.Lines-1, 0);
	V.Sync ();
	return (! Interrupted);
}

void Interrupt (int sig)
{
	++Interrupted;
}

void Logon ()
{
	char *username = getenv ("MARS_USER");
	if (! username)
		return;
	char *password = getenv ("MARS_PASSWORD");
	if (! password)
		return;
	sprintf (Query, "logon %s %s", username, password);
	SetMode ("Logging on");
	gettimeofday (&StartTime, 0);
	if (DBcompile (Query, strlen (Query), 0, 0) < 0) {
		V.Error (PopupColor, NormalColor, "Error", "Ok",
			"Logon error: %s", DBerror (DBinfo.errCode));
		exit (-1);
	}
	Message ("Logged on");
}

int main (int argc, char **argv)
{
#ifdef SIGQUIT
	signal (SIGQUIT, Interrupt);
#endif
	// Количество строк под редактор запроса
	statusLine = 0;                                 // Строка статуса
	messageLine = statusLine + 500/V.Columns;       // Строка сообщения
	if (messageLine < 1)
		messageLine = 1;
	if (messageLine > V.Lines/2) {
		V.Error (PopupColor, NormalColor, "Error", "Ok", "Screen too small");
		return (0);
	}

	V.Clear (messageLine, 0, 1, V.Columns, InverseColor,
		V.HasInverse () ? ' ' : '-');

	// Устанавливаем функцию ожидания
	DBwait (MyWait);

	// Подсоединяемся к базе
	SetMode ("Connect");
	DBconnect ();

	// Регистрируемся
	Logon ();

	Query [QueryLen = 0] = 0;
	for (--argc, ++argv; argc>0; --argc, ++argv) {
		strcat (Query, *argv);
		if (argc > 1)
			strcat (Query, " ");
	}
	QueryLen = strlen (Query);
	int curs = 0;

	for(;;) {
		// Редактируем запрос
		SetMode ("Input");
		Query [QueryLen] = 0;
		int key = MemoryEdit (Query, sizeof (Query), &curs,
			statusLine+1, messageLine-1);
		QueryLen = strlen (Query);

		// Анализируем действие
		switch (key) {
		default:
			V.Beep ();
			continue;
		case meta ('A'):        // справка
			// Help ();
			continue;
		case cntrl ('C'): {     // ^C - выход
			int choice = V.Popup (" Quit ",
				"Do you really want to quit ?", 0,
				" Yes ", " No ", 0, PopupColor, NormalColor);
			if (choice != 0)
				continue;
			SetMode ("Disconnect");
			DBdisconn ();
			V.Move (V.Lines-1, 0);
			V.ClearLine (NormalColor);
			V.Sync ();
			exit (0);
			break;
                }
		case cntrl ('J'):       // line feed - выполнение
		case cntrl ('M'):       // return - выполнение
			break;
		}
		// Стираем старое сообщение
		Message ();

		SetMode ("Compile");
		Interrupted = 0;
		gettimeofday (&StartTime, 0);

		// Компилируем запрос
		if (DBcompile (Query, strlen (Query), Descr, sizeof (DescrBuf)) <= 0) {
			if (DBinfo.errCode) {
				Message ("Error:", DBerror (DBinfo.errCode));
				curs = DBinfo.errPosition;
			} else
				Message ("Executed");
			continue;
		}

		// Запрос не имеет параметров
		if (Descr->paramNumber != 0) {
			Message ("Error:", "Request without parameters");
			DBkill (Descr);
			continue;
		}
		if (Descr->columnNumber >= MAXCOLS) {
			Message ("Error:", "Too many columns");
			DBkill (Descr);
			continue;
		}

		// Запрос готов, читаем
		SetMode ("Execute");

		if (Descr->columnNumber == 0) {
			// Запрос на модификацию
			DBexec (Descr);
			if (DBinfo.errCode) {
				Message ("Error:", DBerror (DBinfo.errCode));
				curs = DBinfo.errPosition;
			} else
				Message ("Succeeded");
			DBkill (Descr);
			continue;
		}

		// Выходной запрос
		V.Clear (messageLine+1, 0, V.Lines-messageLine-1, V.Columns,
			NormalColor, ' ');

		// Простановка ширин полей и их положений
		BuildHead ();

		// Печать заголовка
		V.Move (messageLine+1, 0);
		PrintHead ();
		V.Sync ();
		int Nrecords;
		if (DBexec (Descr) == 0) {
			SetMode ("Fetch");
			V.Move (messageLine+2, 0);
			for (Nrecords=0; DBlfetch(Descr)>0; ++Nrecords) {
				NextLine ();
				PrintLine ();
				V.Sync ();
			}
			// Конец таблицы
			NextLine ();
			PrintTail ();
		}

		// Ошибка при выполнении
		if (DBinfo.errCode) {
			Message ("Error:", DBerror (DBinfo.errCode));
			curs = DBinfo.errPosition;
		} else {
			char buf [80];
			sprintf (buf, "%d records", Nrecords);
			Message (buf);
		}

		// Убираем код запроса
		DBkill (Descr);
	}
}

//
// Переход на следующую строку окна данных.
// Прокрутка одну строку, если нужно.
//
void NextLine ()
{
	if (V.Row () == V.Lines-1) {
		// Прокручиваем на строку
		V.DelLine (messageLine+1, NormalColor);
		V.Move (V.Lines-2, 0);
	}
	V.Put ('\n', 0);
}

//
// Простановка ширин столбцов и их положений.
//
void BuildHead ()
{
	NameAddrs = (short*) (Descr->elements + Descr->columnNumber) +
		Descr->columnNumber;
	int shift = 2;
	for (int i=0; i<Descr->columnNumber; ++i) {
		DBelement* elem = &Descr->elements[i];
		unsigned addr = (i==0 ? 0 : NameAddrs[-i]);
		unsigned namelen = NameAddrs[-1-i] - addr;

		switch (elem->type) {
		case DBshort: elem->length = 6;  break;
		case DBlong:  elem->length = 11; break;
		case DBfloat: elem->length = 13; break;
		default:
			if (elem->length > MAXSTRING)
				elem->length = MAXSTRING;
		}
		ColumnShift[i] = shift;
		ColumnWidth[i] = elem->length;
		if (namelen > ColumnWidth[i])
			ColumnWidth[i] = namelen;
		shift += ColumnWidth[i] + 3;
	}
	TableWidth = shift - 1;
}

//
// Печать заголовка таблицы.
//
void PrintHead ()
{
	V.Put (VertBar, BorderColor);
	for (int i=0; i<Descr->columnNumber; i++) {
		unsigned addr = (i==0 ? 0 : NameAddrs[-i]);
		unsigned namelen = NameAddrs[-1-i] - addr;
		char *name = (char*) NameAddrs + addr;

		if (ColumnShift[i] + ColumnWidth[i] + 2 >= V.Columns)
			break;
		V.Print (NormalColor, " %-*.*s ", ColumnWidth[i], namelen, name);
		V.Put (VertBar, BorderColor);
	}
	V.HorLine (messageLine+2, 0, TableWidth<V.Columns ?
		TableWidth : V.Columns, BorderColor);
	V.Put (messageLine+2, 0, V.CLT, BorderColor);
	if (TableWidth-1 < V.Columns)
		V.Put (messageLine+2, TableWidth-1, V.CRT, BorderColor);
	for (int i=1; i<Descr->columnNumber && ColumnShift[i]-2<V.Columns; i++)
		V.Put (messageLine+2, ColumnShift[i]-2, V.CX, BorderColor);
}

//
// Печать завершающей рамки.
//
void PrintTail ()
{
	int r = V.Row ();
	V.HorLine (r, 0, TableWidth<V.Columns ? TableWidth : V.Columns, BorderColor);
	V.Put (r, 0, V.LLC, BorderColor);
	if (TableWidth-1 < V.Columns)
		V.Put (r, TableWidth-1, V.LRC, BorderColor);
	for (int i=1; i<Descr->columnNumber && ColumnShift[i]-2<V.Columns; i++)
		V.Put (r, ColumnShift[i]-2, V.LT, BorderColor);
}

//
// Вывод строки таблицы данных
//
void PrintLine ()
{
	V.Put (VertBar, BorderColor);
	for (int i=0; i<Descr->columnNumber; ++i) {
		DBelement* elem = &Descr->elements[i];
		short *databody = (short*) ((char*) DBbuffer + elem->shift);

		if (ColumnShift[i] + ColumnWidth[i] + 2 >= V.Columns)
			break;
		switch (elem->type) {
		case DBvarchar: {
			int len = *databody - databody[1] - sizeof(short);
			if (len > ColumnWidth[i])
				len = ColumnWidth[i];
			V.Print (NormalColor, " %-*.*s ", ColumnWidth[i], len,
				(char*)(databody+1) + databody[1]);
			}
			break;
		case DBchar:
			V.Print (NormalColor, " %-*.*s ", ColumnWidth[i], elem->length, databody);
			break;
		case DBshort:
			V.Print (NormalColor, " %*d ", ColumnWidth[i], *(short*) databody);
			break;
		case DBlong:
			V.Print (NormalColor, " %*ld ", ColumnWidth[i], *(long*) databody);
			break;
		case DBfloat:
			V.Print (NormalColor, " %*.5g ", ColumnWidth[i], *(double*) databody);
			break;
		}
		V.Put (VertBar, BorderColor);
	}
}

//
// Редактирование строки в памяти.
//
int MemoryEdit (char *buf, int size, int *posptr, int begline, int endline)
{
	int len = strlen (buf);
	int lines = endline - begline + 1;
	int maxsize = V.Columns * lines;
	int pos = 0;

	if (--size > maxsize)
		size = maxsize;
	if (len > size) {
		len = size;
		buf[len] = 0;
	}
	if (posptr)
		pos = *posptr;
	if (pos < 0)
		pos = 0;
	if (pos > len)
		pos = len;
	for (;;) {
		V.Clear (begline, 0, lines, V.Columns, NormalColor, ' ');
		V.Move (begline, 0);
		int n = 0;
		for (int r=0; n<len; ++r) {
			for (int c=0; c<V.Columns && n<len; ++c, ++n)
				V.Put (begline+r, c, buf[n], NormalColor);
		}
		for (;;) {
			V.Move (begline + pos / V.Columns, pos % V.Columns);
			V.Sync ();
			int key = V.GetKey ();
			switch (key) {
			default:
				if (key>=' ' && key<='~' || key>=0200) {
					if (pos >= size-1)
						continue;
					if (len >= size-1)
						buf[--len] = 0;
					if (len != pos) {
						for (int i=len; i>=pos; --i)
							buf[i+1] = buf[i];
					}
					buf[pos++] = key;
					++len;
					break;
				}
				V.Beep ();
				continue;
			case cntrl ('L'):       // ^L - redraw screen
			case cntrl ('R'):       // ^R - redraw screen
				V.Redraw ();
				continue;
			case cntrl ('J'):       // line feed
			case cntrl ('M'):       // return
			case cntrl ('C'):       // ^C
			case cntrl ('['):       // escape
			case meta ('A'):        // F1
			case meta ('B'):        // F2
			case meta ('C'):        // F3
			case meta ('D'):        // F4
			case meta ('E'):        // F5
			case meta ('F'):        // F6
			case meta ('G'):        // F7
			case meta ('H'):        // F8
			case meta ('I'):        // F9
			case meta ('J'):        // F10
			case meta ('K'):        // F11
			case meta ('L'):        // F12
				if (posptr)
					*posptr = pos;
				return (key);
			case cntrl ('H'):       // ^H - erase previous char
				if (! pos)
					continue;
				--pos;
				/* fall through */
			case meta ('x'):        // Del - erase char
			case cntrl ('D'):       // ^D - erase char
				if (pos == len)
					continue;
				for (int i=pos+1; i<=len; ++i)
					buf[i-1] = buf[i];
				--len;
				break;
			case cntrl ('K'):       // ^K - erase to eof
				if (pos == len)
					continue;
				len = pos;
				break;
			case cntrl ('Y'):       // ^Y - erase all
			case cntrl ('U'):       // ^U - erase all
				if (! len)
					continue;
				len = pos = 0;
				break;
			case meta ('l'):        // left
				if (pos > 0)
					--pos;
				continue;
			case meta ('r'):        // right
				if (pos < len)
					++pos;
				continue;
			case cntrl ('I'):       // tab
				if (pos+8 <= len)
					pos += 8;
				continue;
			case meta ('u'):        // up
				if (pos >= V.Columns)
					pos -= V.Columns;
				continue;
			case meta ('d'):        // down
				if (pos + V.Columns <= len)
					pos += V.Columns;
				continue;
			case meta ('h'):        // home
				pos = 0;
				continue;
			case meta ('e'):        // end
				pos = len;
				continue;
			}
			buf[len] = 0;
			break;
		}
	}
}

#ifdef EDITDEBUG
Screen V;
char buf [4000];

int main (int argc, char **argv)
{
	for (--argc, ++argv; argc>0; --argc, ++argv) {
		strcat (buf, " ");
		strcat (buf, argv [1]);
	}
	MemoryEdit (buf, sizeof (buf), V.Lines/4);
	V.Move (V.Lines-1, 0);
	V.ClearLine (NormalColor);
	V.Sync ();
	return (0);
}
#endif
