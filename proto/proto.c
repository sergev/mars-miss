#define FD_SETSIZE 32
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef USE_UNIX_DOMAIN
#include <sys/un.h>
#else
#undef AF_UNIX
#endif

int socktcpip (char *host, int portnum);
int sockunix (int portnum);
int sockfifo (int port, int *fifo);
void prstring (char *str, int len);

int main (int argc, char **argv)
{
	int sock, fifo;
	char *host, *port;
	int portnum;

	if (argc>3 || argc>1 && **argv=='-') {
		printf ("Usage: marsdebug [host [port]]\n");
		exit (2);
	}
	host = argc>1 ? argv[1] : "unix";
	port = argc>2 ? argv[2] : 0;

	if (port)
		portnum = atoi (port);
	else {
		struct servent *sv;

		/* Search MARS entry in /etc/services */
		sv = getservbyname ("mars", "tcp");
		if (! sv) {
			fprintf (stderr, "mars: unknown service\n");
			exit (-1);
		}
		portnum = ntohs (sv->s_port);
	}

	fifo = -1;
	if (!strcmp (host, "unix"))
		sock = sockunix (portnum);
	else if (*host)
		sock = socktcpip (host, portnum);
	else
		sock = sockfifo (portnum, &fifo);

	for (;;) {
		char buf [1024];
		fd_set rcv;
		int len;

		FD_ZERO (&rcv);
		FD_SET (0, &rcv);
		FD_SET (sock, &rcv);

		if (select (FD_SETSIZE, &rcv, 0, 0, 0) < 0) {
			perror ("select");
			exit (1);
		}
		if (FD_ISSET (sock, &rcv)) {
			len = read (sock, buf, sizeof (buf));
			if (len < 0) {
				perror ("reading from socket");
				exit (1);
			}
			if (len == 0) {
				printf ("Disconnected.\n");
				exit (1);
			}
			prstring (buf, len);
			putchar ('\n');
			continue;
		}
		if (FD_ISSET (0, &rcv)) {
			len = read (0, buf+2, sizeof(buf)-2);
			if (len < 0) {
				perror ("reading from stdin");
				exit (1);
			}
			if (len == 0) {
				printf ("Exit.\n");
				exit (0);
			}
			if (buf[len+1] == '\n')
				--len;
			if (len) {
				buf[0] = len>>8;
				buf[1] = len;
				write (fifo>=0 ? fifo : sock, buf, len+2);
			}
		}
	}
}

int socktcpip (char *host, int portnum)
{
	int sock;
	struct sockaddr_in server;
	struct hostent *hp;
	struct hostent hbuf;
	struct in_addr hbufaddr;

	if (*host>='0' && *host<='9' &&
	    (hbufaddr.s_addr = inet_addr (host)) != INADDR_NONE) {
		/* raw ip address */
		hp = &hbuf;
#ifdef h_addr
		{
		static char *alist [1];
		hp->h_addr_list = alist;
		}
#endif
		hp->h_name = host;
		hp->h_addrtype = AF_INET;
		hp->h_addr = (void*) &hbufaddr;
		hp->h_length = sizeof (struct in_addr);
		hp->h_aliases = 0;
	} else {
		hp = gethostbyname (host);
		if (! hp) {
			fprintf (stderr, "%s: unknown host\n", host);
			exit (2);
		}
	}

	/* Create socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror ("opening tcp socket");
		exit (1);
	}

	/* Connect socket using name specified by command line. */
	server.sin_family = hp->h_addrtype;
	memcpy (&server.sin_addr, hp->h_addr, hp->h_length);
	server.sin_port = htons (portnum);

	if (connect (sock, (struct sockaddr *) &server, sizeof (server)) < 0) {
		perror ("connecting tcp socket");
		exit (1);
	}
	printf ("Connected to %s:%d.\n", inet_ntoa (server.sin_addr),
		ntohs (server.sin_port));
	return (sock);
}

int sockunix (int portnum)
{
#ifdef AF_UNIX
	int sock;
	struct sockaddr_un server;

	/* Create socket */
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		perror ("opening unix socket");
		exit (1);
	}

	/* Connect socket using name specified by command line. */
	server.sun_family = AF_INET;
	sprintf (server.sun_path, "/tmp/.mars%d", portnum);
#ifndef NO_SUN_LEN
	server.sun_len = SUN_LEN (&server);
#endif
	if (connect (sock, (struct sockaddr *) &server, sizeof (server)) < 0) {
		perror ("connecting unix socket");
		exit (1);
	}
	printf ("Connected to %s.\n", server.sun_path);
	return (sock);
#else
	printf ("mars: UNIX domain not supported\n");
	exit (-1);
#endif
}

int sockfifo (int port, int *fifo)
{
	char fifoname [32];
	int serv, sock;
	unsigned char portnum;

	sprintf (fifoname, "/tmp/.mars%df", port);
printf ("a\n");

	/* Подсоединяемся к серверу */
	if ((serv = open (fifoname, 1)) < 0) {
		perror ("opening server fifo");
		exit (-1);
	}
printf ("b\n");

	/* Находим свободный номер FIFO-порта */
	for (portnum=1; portnum>0; ++portnum) {
		sprintf (fifoname, "/tmp/.mars%dr", portnum);
		if (mkfifo (fifoname, 0600) >= 0)
			break;
	}
printf ("c\n");
	if (! portnum) {
		fprintf (stderr, "out of fifos\n");
		exit (-1);
	}
printf ("d\n");
	if ((sock = open (fifoname, 0)) < 0) {
		perror ("opening client fifo");
		exit (-1);
	}
printf ("e\n");
	sprintf (fifoname, "/tmp/.mars%dt", portnum);
	unlink (fifoname);
printf ("f\n");
	if (mkfifo (fifoname, 0600) < 0) {
		perror ("creating client fifo");
		exit (-1);
	}
printf ("g\n");
	if ((*fifo = open (fifoname, 1)) < 0) {
		perror ("opening client fifo");
		exit (-1);
	}
printf ("h\n");

	/* Сообщаем серверу наш номер порта */
	if (write (serv, &portnum, 1) != 1) {
		perror ("writing to server fifo");
		exit (-1);
	}
printf ("i\n");
	if (read (sock, &portnum, 1) != 1) {
		perror ("reading from fifo");
		exit (-1);
	}
printf ("j\n");
	unlink (fifoname);
	printf ("Connected to FIFO %s.\n", fifoname);
	return (sock);
}

void prstring (char *str, int len)
{
	int c;

	while (len--) {
		c = (unsigned char) *str++;
		if (c >= ' ' && c <= '~' || c>=0200) {
			putchar (c);
			continue;
		}
		putchar ('\\');
		printf ("%o", c);
	}
}

#ifdef NO_MKFIFO
int mkfifo (char *name, int mode) {
  return (mknod (name, S_IFIFO | mode, 0));
}
#endif
