#include <stdio.h>
#ifndef NO_STDLIB
#include <stdlib.h>
#endif
#include <stdarg.h>
#include <string.h>
#include <tcp.h>
#include "mars.h"
#include "intern.h"

#define HOSTDFLT "localhost"
#define PORTDFLT 3836

char _DBoutput [MaxDBOutput+6];         /* буфер вывода */
DBtrap _DBwaitproc = 0;                 /* функция ожидания */

char *DBhost = 0;                       /* имя машины сервера */
int DBport = 0;                         /* номер порта сервера */
tcp_Socket DBsockbuf;                   /* буфер для гнезда */
tcp_Socket *DBsocket;			/* гнездо соединения */
struct DBinfo DBinfo;                   /* результат запроса */
char DBbuffer [MaxDBInput+4];           /* буфер ввода */
char *DBexecBuf = _DBoutput+4;          /* буфер запроса */
int DBrunflag;				/* флаг инициализации */

static int readsock (char *buf, int len)
{
	int status;

	sock_wait_input (DBsocket, 30, NULL, &status);
	return (sock_fastread (DBsocket, buf, len));

sock_err:
	switch (status) {
	case 1:		/* foreign host closed connection */
		fprintf (stderr, "mars: DBconnect: foreigh host closed connection\n");
		break;
	case -1:	/* timeout */
		fprintf (stderr, "mars: DBconnect: %s\n", sockerr (DBsocket));
		break;
	}
	DBsocket = 0;
	return (-1);
}

/*
 * Чтение записи из потока.
 */
static int readn (char *buf, int len)
{
	int rlen, datalen, rez;

	/* Читаем, сколько есть */
	rlen = readsock (buf, len);
	if (rlen < sizeof (short)) {
		for (rez=rlen; rez<sizeof(short); rez+=rlen) {
			rlen = readsock (buf+rez, sizeof(short)-rez);
			if (rlen <= 0)
				return (-1);
		}
		rlen = rez;
	}

	/* Вычисляем длину записи */
	datalen = *(short*) buf;
	if (datalen < (int) sizeof (short))
		datalen = 2 * sizeof (short);

	/* Все прочитали */
	if (rlen == datalen)
		return (datalen);

	/* Слишком длинная запись */
	if (datalen > len)
		return (-1);

	/* Дочитываем остаток */
	for (rez=rlen; rez<datalen; rez+=rlen) {
		rlen = readsock (buf+rez, datalen-rez);
		if (rlen <= 0)
			return (-1);
	}
	return (rez);
}

/*
 * Собственно посылка сообщения серверу и первичная обработка ответа.
 * Параметры A и B задают первые два байта запроса,
 * остальное должно находится в буфере DBexecBuf.
 * Параметр LEN определяет общую длину пакета, от 1 до MaxDBOutput.
 *
 * Если запрос выполняется долго и установлен указатель на функцию
 * DBwaitproc, эта функция будет периодически вызыватся с аргументом
 * "работаю" или "заблокирован".  Пока эта функция будет возвращать
 * ненулевое значение, выполнение запроса будет продолжаться.
 * Нулевое значение функции DBwaitproc вызовет прерывание выполнения запроса.
 */
int _DBio (int a, int b, int len)
{
	int waitflag = 0;               /* ждем окончания запроса */
	char *out;                      /* данные для передачи */
	int reply;                      /* код ответа */

	_DBoutput [0] = len >> 8;
	_DBoutput [1] = len;
	_DBoutput [2] = a;
	_DBoutput [3] = b;
	out = _DBoutput;
again:
	/* передаем запрос */
	if (sock_write (DBsocket, out, len+2) != len+2) {
		perror ("mars: writing to socket");
		return (-1);
	}
	DBinfo.errCode = 0;

	/* принимаем ответ */
	len = readn (DBbuffer, MaxDBInput);
	if (len < 0) {
		perror ("mars: reading from socket");
		return (-1);
	}
	if (len == 0) {
		fprintf (stderr, "mars: DBio: server closed connection\n");
		return (-1);
	}
	reply = *(short*) DBbuffer;
	if (reply) {
		/* ответ получен */
		if (waitflag && _DBwaitproc)
			(*_DBwaitproc) (-1);
		if (reply < 0) {
			DBinfo.errCode = -reply;
			DBinfo.errPosition = *((short*) DBbuffer + 1) - 2;
			return (reply);
		} else if (reply == SHORTSZ)
			return (0);
		return (reply);
	}

	/* принят ответ "работаем" */
	waitflag = 1;
	if (_DBwaitproc && (*_DBwaitproc)(*((short*) DBbuffer + 1)) == 0)
		out = "\0\1I";              /* прервать */
	else
		out = "\0\1P";              /* продолжать */
	len = 1;
	goto again;
}

/*
 * Установка процедуры ожидания.
 * Возвращает старое значение.
 */
DBtrap DBwait (DBtrap proc)
{
	DBtrap old = _DBwaitproc;

	_DBwaitproc = proc;
	return (old);
}

/*
 * Подсоединение к базе данных.
 * Переменная DBhost задает имя хоста,
 * переменная DBport - номер порта сервера.
 * Если порт не задан, он берется из /etc/services
 * по имени "mars" и типу "tcp".
 */
short DBconnect ()
{
	if (! DBrunflag) {
		sock_init ();
		DBrunflag = 1;
	}
	if (DBsocket)
		DBdisconn ();
	DBinfo.errCode = 0;             /* код ошибки */
	DBinfo.errPosition = 0;         /* ее позиция в тексте или -1 */
	DBinfo.nrecords = 0;            /* счетчик модифицированных */

	/* определяем адрес хоста */
	if (! DBhost)
		DBhost = getenv ("MARS_HOST");
	if (! DBhost)
		DBhost = HOSTDFLT;

	/* определяем номер порта сервера, поиск в /etc/services */
	if (! DBport) {
		char *port = getenv ("MARS_PORT");
		if (port)
			DBport = atoi (port);
	}
	if (! DBport)
		DBport = PORTDFLT;

	if (!strcmp (DBhost, "unix")) {
		fprintf (stderr, "mars: UNIX domain not supported\n");
		return (-1);
	} else if (*DBhost) {
		long hostaddr;
		int status;

		hostaddr = resolve (DBhost);
		if (! hostaddr) {
			fprintf (stderr, "mars: DBconnect: unknown host '%s'\n",
				DBhost);
			return (-1);
		}
		DBsocket = &DBsockbuf;
		if (! tcp_open (DBsocket, 0, hostaddr, DBport, NULL)) {
			fprintf (stderr, "mars: cannot connect to '%s' port %d\n",
				DBhost, DBport);
			DBsocket = 0;
			return (-1);
		}
		sock_wait_established (DBsocket, sock_delay, NULL, &status);
		status = 0;
sock_err:	switch (status) {
		case 1:		/* foreign host closed connection */
			fprintf (stderr, "mars: DBconnect: foreigh host closed connection\n");
			DBsocket = 0;
			return (-1);
		case -1:	/* timeout */
			fprintf (stderr, "mars: DBconnect: %s\n", sockerr (DBsocket));
			DBsocket = 0;
			return (-1);
		}
	} else {
		fprintf (stderr, "mars: FIFO communication not supported\n");
		return (-1);
	}
	return (_DBio ('G', 0, 1));     /* connect */
}

/*
 * Отсоединение от базы данных, если были подключены.
 */
void DBdisconn ()
{
	if (DBsocket) {
		sock_close (DBsocket);
		sock_wait_closed (DBsocket, 30, NULL, NULL);
sock_err:	DBsocket = 0;
	}
}
