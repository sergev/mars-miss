#include <stdio.h>
#include <unistd.h>
#ifndef NO_STDLIB
#include <stdlib.h>
#endif
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#ifdef USE_UNIX_DOMAIN
#include <sys/un.h>
#define HOSTDFLT "unix"
#else
#undef AF_UNIX
#define HOSTDFLT "localhost"
#endif
#include <arpa/inet.h>
#include "mars.h"
#include "intern.h"

char _DBoutput [MaxDBOutput+6];         /* буфер вывода */
DBtrap _DBwaitproc = 0;                 /* функция ожидания */

char *DBhost = 0;                       /* имя машины сервера */
int DBport = 0;                         /* номер порта сервера */
int DBsocket = -1;                      /* гнездо соединения */
int DBfifo = -1;                        /* FIFO на передачу */
struct DBinfo DBinfo;                   /* результат запроса */
char DBbuffer [MaxDBInput+4];           /* буфер ввода */
char *DBexecBuf = _DBoutput+4;          /* буфер запроса */

/*
 * Чтение записи из потока.
 */
static int readn (int fd, char *buf, int len)
{
	int rlen, datalen, rez;

	/* Читаем, сколько есть */
	rlen = read (fd, buf, len);
	if (rlen < sizeof (short)) {
		for (rez=rlen; rez<sizeof(short); rez+=rlen) {
			rlen = read (fd, buf+rez, sizeof(short)-rez);
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
		rlen = read (fd, buf+rez, datalen-rez);
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
	if (write (DBfifo>=0 ? DBfifo : DBsocket, out, len+2) != len+2) {
		perror ("mars: writing to socket");
		return (-1);
	}
	DBinfo.errCode = 0;

	/* принимаем ответ */
	len = readn (DBsocket, DBbuffer, MaxDBInput);
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
	if (DBsocket >= 0)
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
	if (! DBport) {
		struct servent *sv = getservbyname ("mars", "tcp");
		if (! sv) {
			fprintf (stderr, "mars: DBconnect: unknown service\n");
			return (-1);
		}
		DBport = ntohs (sv->s_port);
	}

	if (!strcmp (DBhost, "unix")) {
#ifdef AF_UNIX
		struct sockaddr_un server;

		/* создаем гнездо */
		DBsocket = socket (AF_UNIX, SOCK_STREAM, 0);
		if (DBsocket < 0) {
			perror ("mars: opening unix socket");
			return (-1);
		}

		/* устанавливаем соединение */
		server.sun_family = AF_UNIX;
		sprintf (server.sun_path, "/tmp/.mars%d", DBport);
#ifndef NO_SUN_LEN
		server.sun_len = SUN_LEN (&server);
#endif
		if (connect (DBsocket, (struct sockaddr*) &server,
		    sizeof (server)) < 0) {
			perror ("mars: connecting unix socket");
			return (-1);
		}
#else
		fprintf (stderr, "mars: UNIX domain not supported\n");
		return (-1);
#endif
	} else if (*DBhost) {
#ifdef AF_INET
		struct sockaddr_in server;
		struct hostent *hp;
		struct hostent hbuf;
		struct in_addr hbufaddr;

		/* создаем гнездо */
		DBsocket = socket (AF_INET, SOCK_STREAM, 0);
		if (DBsocket < 0) {
			perror ("mars: opening tcp socket");
			return (-1);
		}

		if (*DBhost>='0' && *DBhost<='9' &&
		    (hbufaddr.s_addr = inet_addr (DBhost)) != INADDR_NONE) {
			/* raw ip address */
			hp = &hbuf;
#ifdef h_addr
			{
			static char *alist [1];
			hp->h_addr_list = alist;
			}
#endif
			hp->h_name = DBhost;
			hp->h_addrtype = AF_INET;
			hp->h_addr = (void*) &hbufaddr;
			hp->h_length = sizeof (struct in_addr);
			hp->h_aliases = 0;
		} else {
			hp = gethostbyname (DBhost);
			if (! hp) {
				fprintf (stderr, "mars: DBconnect: unknown host '%s'\n",
					DBhost);
				return (-1);
			}
		}

		/* устанавливаем соединение */
		server.sin_family = hp->h_addrtype;
		memcpy (&server.sin_addr, hp->h_addr, hp->h_length);
		server.sin_port = htons (DBport);

		if (connect (DBsocket, (struct sockaddr*) &server,
		    sizeof (server)) < 0) {
			perror ("mars: connecting tcp socket");
			return (-1);
		}
#else
		fprintf (stderr, "mars: Internet domain not supported\n");
		return (-1);
#endif
	} else {
#ifdef USE_FIFO_FILES
		char fifoname [32];
		int serv;
		unsigned char portnum;

		sprintf (fifoname, "/tmp/.mars%df", DBport);

		/* Подсоединяемся к серверу */
		if ((serv = open (fifoname, 1)) < 0) {
			perror ("mars: opening server fifo");
			return (-1);
		}

		/* Находим свободный номер FIFO-порта */
		for (portnum=1; portnum>0; ++portnum) {
			sprintf (fifoname, "/tmp/.mars%dr", portnum);
			if (mkfifo (fifoname, 0600) >= 0)
				break;
		}
		if (! portnum) {
			fprintf (stderr, "mars: DBconnect: out of fifos\n");
			return (-1);
		}
		if ((DBsocket = open (fifoname, 0)) < 0) {
			perror ("mars: opening client fifo");
			return (-1);
		}
		sprintf (fifoname, "/tmp/.mars%dt", portnum);
		unlink (fifoname);
		if (mkfifo (fifoname, 0600) < 0) {
			perror ("mars: creating client fifo");
			return (-1);
		}
		if ((DBfifo = open (fifoname, 1)) < 0) {
			perror ("mars: opening client fifo");
			return (-1);
		}

		/* Сообщаем серверу наш номер порта */
		if (write (serv, &portnum, 1) != 1) {
			perror ("mars: writing to server fifo");
			return (-1);
		}
		if (read (DBsocket, &portnum, 1) != 1) {
			perror ("mars: reading from fifo");
			return (-1);
		}
		unlink (fifoname);
#else
		fprintf (stderr, "mars: FIFO communication not supported\n");
		return (-1);
#endif
	}
	return (_DBio ('G', 0, 1));     /* connect */
}

/*
 * Отсоединение от базы данных, если были подключены.
 */
void DBdisconn ()
{
	if (DBsocket >= 0) {
		close (DBsocket);
		close (DBfifo);
		DBsocket = -1;
	}
}

#ifdef NO_MKFIFO
int mkfifo (char *name, int mode) {
  return (mknod (name, S_IFIFO | mode, 0));
}
#endif
