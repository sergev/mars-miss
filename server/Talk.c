                   /* ============================= *\
                   ** ==         M.A.R.S.        == **
                   ** == Диалог с пользователями == **
                   ** ==   Выдача трассировок    == **
                   ** ==  Файловый  ввод/вывод   == **
                   \* ============================= */

#if defined(unix)
#define FD_SETSIZE      (sizeof (long) * 16)    /* используем 64 дескриптора */
#endif

#include "FilesDef"
#include "AgentDef"

         void     SetDevice(int);            /* инициация канала связи*/
         void     Reply0   (agent,char*);    /* трассировочные выдачи */
         void     Talk     (void);           /* диалог с пользов-лями */
         void     DiskIO   (fileid,char*,unsigned,unsigned,boolean);
         void     SyncFlush(fileid);         /* сброс Upd-файла       */
         int      FileLine (char*,short);    /* строчный в/вывод      */

  extern void     Show     (char);           /* просмотр таблиц       */

                       /* ====================== *\
                       ** =  UNIX environment  = **
                       \* ====================== */

#if defined(unix)
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#ifndef NO_FCNTL
# ifdef FCNTL
#  include <fcntl.h>
# else
#  include <sys/fcntl.h>
# endif
# ifndef O_NDELAY
#  define O_NDELAY O_NONBLOCK
# endif
# ifndef O_NDELAY
#  define O_NDELAY FNDELAY
# endif
#endif
#include <sys/errno.h>
#ifdef USE_NET_ERRNO
# include <net/errno.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef USE_UNIX_DOMAIN
# include <sys/un.h>
#else
# undef AF_UNIX
#endif

#define TimeWait        10              /* цикл опроса при простое, 10 секунд */

int fileinput;                          /* режим ввода с терминала */
int TCPsocket = -1;                     /* гнездо сервера TCP/IP */
int TCPport = 0;                        /* наш номер порта */
int UNIXsocket = -1;                    /* гнездо сервера UNIX */
char UNIXport [28];                     /* наше имя порта UNIX */
int FIFOdescr = -1;                     /* именованная труба */
char FIFOpath [28];                     /* наше имя трубы */
int FIFOnew;                            /* FIFO на передачу для подсоединения */
fd_set PORTmask;                        /* маска гнезд */
fd_set PORTready;                       /* маска гнезд, готовых к передаче */
fd_set PORTmask0 = {0};                 /* нулевая маска */
int fd2agent [FD_SETSIZE];              /* дескриптор гнезда -> номер агента */
int agent2fd [FD_SETSIZE];              /* номер агента -> дескриптор гнезда */
int agent2fifo [FD_SETSIZE];            /* номер агента -> FIFO на передачу */

extern int errno;

static void prstring (char *str, int len) {
  while (len--) {
    int c = (unsigned char) *str++;
    if (c >= ' ' && c <= '~' || c>=0200) {
      putchar (c);
      continue;
    }
    putchar ('\\');
    printf ("%o", c);
  }
}

/*
 * Чтение записи из потока.
 */
static int readn (int fd, char *buf, int len)
{
  int rlen, datalen, rez;
  unchar lenbuf [2];

  /* Читаем и вычисляем длину записи */
  rez = read (fd, lenbuf, 2);
  if (rez <= 0)
    return (rez);
  if (rez == 1) {
    rez = read (fd, lenbuf+1, 1);
    if (rez <= 0)
      return (rez);
  }
  datalen = lenbuf[0]<<8 | lenbuf[1];

  /* Слишком длинная запись */
  if (datalen > len) {
badpacket:
    errno = EMSGSIZE;
    return (-1);
  }

  /* Дочитываем остаток */
  for (rez=0; rez<datalen; rez+=rlen) {
    rlen = read (fd, buf+rez, datalen-rez);
    if (rlen <= 0)
      goto badpacket;
  }
  return (rez);
}

int Send (int c, char* buf, int len) {
  int fd;

  if (fileinput) {
    printf ("Send: [");
    prstring (buf, len);
    printf ("]\n");
    return (0);
  }
  if (agent2fifo [c] >= 0)
    fd = agent2fifo [c];
  else
    fd = agent2fd [c];
  if (fd < 0)
    return (-1);
  if (write (fd, buf, len) != len) {
    if (TraceFlags & TraceOutput)
      printf ("MARS: sending to client %d: %s\n", c, strerror (errno));
    return (-1);
  }
  return (len);
}

int Receive (int c, char* buf, int len) {
  if (fileinput) {
    printf ("Recv: ");
    fflush (stdout);
    if (! fgets (buf, len, stdin)) {
      printf ("<EOF>\n");
      fclose (stdin);
      if (fopen ("/dev/tty", "r") != stdin)
	printf ("MARS: reopening stdin: %s\n", strerror (errno));
      return (-1);
    }
    len = strlen (buf);
    if (len>0 && buf[len-1]=='\n')
      buf[--len] = 0;
    return (len);
  }
  if (agent2fd [c] < 0)
    return (-1);
  len = readn (agent2fd [c], buf, len);
  if (len <= 0) {
    if (len < 0 && (TraceFlags & TraceInput))
      printf ("MARS: receiving from client %d: %s\n", c, strerror (errno));
    return (-1);
  }
  return (len);
}

/*
 * Отработка нового соединения по FIFO.
 */
int AcceptFifo (int d)
{
#ifdef USE_FIFO_FILES
  unsigned char portnum;
  char portname [32];
  int s, r;

  /* Зачитываем первый байт - некий номер порта, сообщаемый клиентом */
  errno = EMSGSIZE;
  if (read (d, &portnum, 1) != 1)
    return (-1);

  /* Открываем новые fifo по номеру порта */
  sprintf (portname, "/tmp/.mars%dt", portnum);
  s = open (portname, 0 | O_NDELAY);
  if (s < 0)
    return (-1);
  sprintf (portname, "/tmp/.mars%dr", portnum);
  r = open (portname, 1 | O_NDELAY);
  if (r < 0)
    return (-1);

  /* Говорим клиенту "привет" */
  write (r, &portnum, 1);
  FIFOnew = r;
  return (s);
#else /* USE_FIFO_FILES */
  return (-1);
#endif /* USE_FIFO_FILES */
}

/*
 * Ожидание ввода по всем соединениям.
 */
int WaitForInput () {
  struct timeval timo;
  int s, a;
  char *netaddr;

  if (fileinput)
    return (0);

  /*
   * Если в очереди есть клиенты, ждущие обработки,
   * то ждать ввода не нужно, просто опрашиваем готовность.
   */
  timo.tv_sec = Ready ? 0 : TimeWait;
  timo.tv_usec = 0;
  for (;;) {
    /* есть готовые клиенты от предыдущего вызова */
    if (memcmp (&PORTready, &PORTmask0, sizeof (fd_set)) != 0) {
      for (s=0; s<FD_SETSIZE; ++s)
	if (FD_ISSET (s, &PORTready)) {
	  FD_CLR (s, &PORTready);
	  a = fd2agent [s];
	  if (a < 0) {
	    if (TraceFlags & TraceInput)
	      printf ("MARS: unexpected input from socket %d\n", s);
	    continue;
	  }
	  return (a);
	}
    }

    /* опрашиваем готовность всех наших гнезд */
    PORTready = PORTmask;
    if (select (FD_SETSIZE, &PORTready, (void *) 0, (void *) 0, &timo) <= 0)
      return (-1);

    /* есть запросы на подключение новых клиентов */
    FIFOnew = -1;
    if (TCPsocket >= 0 && FD_ISSET (TCPsocket, &PORTready)) {
      struct sockaddr_in client;
      int length = sizeof (client);

      FD_CLR (TCPsocket, &PORTready);
      s = accept (TCPsocket, (struct sockaddr *) &client, &length);
      if (s < 0) {
	if (TraceFlags)
	  printf ("MARS: accepting tcp connection: %s\n", strerror (errno));
	continue;
      }
      netaddr = inet_ntoa (client.sin_addr);
#ifdef AF_UNIX
    } else if (UNIXsocket >= 0 && FD_ISSET (UNIXsocket, &PORTready)) {
      struct sockaddr_un client;
      int length = sizeof (client);

      FD_CLR (UNIXsocket, &PORTready);
      s = accept (UNIXsocket, (struct sockaddr *) &client, &length);
      if (s < 0) {
	if (TraceFlags)
	  printf ("MARS: accepting unix connection: %s\n", strerror (errno));
	continue;
      }
      netaddr = "unix domain";
#endif
    } else if (FIFOdescr >= 0 && FD_ISSET (FIFOdescr, &PORTready)) {
      FD_CLR (FIFOdescr, &PORTready);
      s = AcceptFifo (FIFOdescr);
      if (s < 0) {
	if (TraceFlags)
	  printf ("MARS: accepting fifo connection: %s\n", strerror (errno));
	continue;
      }
      netaddr = "fifo";
    } else
      continue;

    if (s >= FD_SETSIZE) {
      if (TraceFlags)
	printf ("MARS: out of socket descriptors (%d)\n", s);
      close (s);
      continue;
    }

    /* ищем свободный номер агента */
    for (a=0; a<FD_SETSIZE; ++a)
      if (agent2fd[a] < 0)
	break;
    if (a >= FD_SETSIZE) {
      if (TraceFlags)
	printf ("MARS: out of agent descriptors\n");
      close (s);
      continue;
    }

    /* добавляем нового клиента */
    FD_SET (s, &PORTmask);
    fd2agent [s] = a;
    agent2fd [a] = s;
    agent2fifo [a] = FIFOnew;

    if (TraceFlags)
      printf ("MARS: new agent %d from %s connected\n", a, netaddr);
  }
}

/*
 * Освобождение канала связи.
 */
void FreeChannel (int c) {
  int fd;

  if (fileinput) {
    printf ("MARS: debug channel %d disconnected\n", c);
    return;
  }

  fd = agent2fd [c];
  if (fd >= 0) {
    close (fd);
    FD_CLR (fd, &PORTmask);
    agent2fd [c] = fd2agent [fd] = -1;
    if (TraceFlags)
      printf ("MARS: agent %d disconnected\n", c);
  }
}

/*
 * Задание номера порта.
 */
int SetName (char *val) {
  struct servent *sv;

  if (*val>='0' && *val<='9') {
    TCPport = strtol (val, (char**) 0, 0);
    return (0);
  }
  sv = getservbyname (val, "tcp");              /* ищем в /etc/services */
  if (! sv) {
    printf ("MARS: cannot find '%s' in /etc/services\n", val);
    return (-1);
  }
  TCPport = ntohs (sv->s_port);
  return (0);
}

/*
 * Диалог с пользователями.
 *
 * Если OutAgent>=0, передать сообщение этому абоненту и сбросить OutAgent.
 * OutBuffer указывает на буфер данных, 0-й элемент (short) содержит
 * длину массива (если >2short).
 */
void Talk() {
  int inAgent, len;
  struct timeval tv;
  struct timezone tz;

  gettimeofday (&tv, &tz);
  SliceTimer = tv.tv_sec % 3600;

  /*
   * Время в минутах от 1 января 1990 года.
   */
  Timer = tv.tv_sec/60 - tz.tz_minuteswest - 7305L * 24 * 60;

  /*
   * Вывод ответа.
   */
  if (OutAgent >= 0) {
    /* ответ есть */
    len = *OutBuffer;
    if (len < (int) sizeof (short))
      len = 2 * sizeof (short);
    if (TraceFlags & TraceOutput) {
      printf ("MARS: to %d [%d] '", OutAgent, len);
      prstring ((char *) OutBuffer, len);
      printf ("'\n");
    }
    if (Send (OutAgent, (char*) OutBuffer, len) < 0) {
      AgentTable[OutAgent].inpbuf = "$";           /* "отключение без подтв."*/
      AgentTable[OutAgent].waittime = SliceTimer;  /* время поступления запр.*/
      AgentTable[OutAgent].status = ready;
      Inqueue (&Ready, &AgentTable[OutAgent]);
      FreeChannel (OutAgent);
    }
    OutAgent = -1;                              /* ответ выведен */
  }

  /*
   * Ожидание запроса.
   */
  inAgent = WaitForInput ();

  if (inAgent < 0) {                            /* нет ввода */
    CommonBuffer[0] = ' ';                      /* пустой цикл */
    CommonBuffer[1] = 0;
    return;
  }
  if (AgentTable[inAgent].status != reading) {
    if (TraceFlags & TraceInput)
      Reply0 (&AgentTable[inAgent], "Agent busy");
    return;
  }

  /*
   * Ввод запроса.
   */
  len = Receive (inAgent, CommonBuffer+1, MaxInput+1);
  AgentTable[inAgent].queue = Ready;
  Ready = &AgentTable[inAgent];                 /* встаем в голову очереди */
  Ready->status = ready;                        /* чтоб сразу взять данные */
  Ready->waittime = SliceTimer;                 /* время поступления запр. */

  if (len<0 || len==MaxInput+1) {               /* клиент отпал */
    Ready->inpbuf = "$";                        /* или переполнение буфера */
    return;
  }
  Ready->inpbuf = CommonBuffer+1;               /* буфер принятых данных */
  CommonBuffer[0] = inAgent + '0';              /* добавим код канала */
  CommonBuffer[len+1] = ';';                    /* и ограничитель */
  CommonBuffer[len+2] =  0 ;
  CommonBuffer[1] = UpperCase (CommonBuffer[1]);

  if (TraceFlags & TraceInput) {
    printf ("MARS: from %d [%d] '", inAgent, len);
    prstring (CommonBuffer+1, len);
    printf ("'\n");
  }
}

/*
 * Инициация канала связи.
 */
void SetDevice (int label) {
  if (label >= 0) {
    /* ввод из файла */
    FileLine (NIL, -label);
    fileinput = 1;
  } else {
    /* сетевая требуха */
    int i;

    fileinput = 0;

    if (! TCPport) {
      /* Search MARS entry in /etc/services */
      struct servent *sv = getservbyname ("mars", "tcp");
      if (! sv) {
	printf ("MARS: cannot find MARS in /etc/services\n");
	myAbort (0);
      }
      TCPport = ntohs (sv->s_port);
    }

#ifdef AF_INET
    {
      struct sockaddr_in server;
      /* Create socket */
      if ((TCPsocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	printf ("MARS: creating tcp socket: %s\n", strerror (errno));
	myAbort (0);
      }
      /* Name socket using wildcards */
      server.sin_family = AF_INET;
      server.sin_addr.s_addr = INADDR_ANY;
      server.sin_port = htons (TCPport);
      if (bind (TCPsocket, (struct sockaddr *) &server, sizeof (server))) {
	printf ("MARS: binding socket: %s\n", strerror (errno));
	myAbort (0);
      }
      /* Start accepting connections */
      listen (TCPsocket, 5);
      if (TraceFlags)
	printf ("MARS: Listen on tcp port %d\n", TCPport);
    }
#endif /* AF_INET */

#ifdef AF_UNIX
    {
      struct sockaddr_un server;
      /* Create socket */
      if ((UNIXsocket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
	printf ("MARS: creating unix socket: %s\n", strerror (errno));
	myAbort (0);
      }
      /* Name socket using wildcards */
      server.sun_family = AF_UNIX;
      sprintf (UNIXport, "/tmp/.mars%d", TCPport);
      strcpy (server.sun_path, UNIXport);
#ifndef NO_SUN_LEN
      server.sun_len = SUN_LEN (&server);
#endif
      if (bind (UNIXsocket, (struct sockaddr *) &server, sizeof (server))) {
	printf ("MARS: binding socket: %s\n", strerror (errno));
	myAbort (0);
      }
      /* Start accepting connections */
      listen (UNIXsocket, 5);
      if (TraceFlags)
	printf ("MARS: Listen on unix port %s\n", UNIXport);
    }
#endif /* AF_UNIX */

#ifdef USE_FIFO_FILES
    /* Create fifo */
    sprintf (FIFOpath, "/tmp/.mars%df", TCPport);
    if (mkfifo (FIFOpath, 0622) < 0) {
      printf ("MARS: creating FIFO: %s\n", strerror (errno));
      myAbort (0);
    }
    if ((FIFOdescr = open (FIFOpath, 0 | O_NDELAY)) < 0) {
      printf ("MARS: opening fifo %s: %s\n", FIFOpath, strerror (errno));
      unlink (FIFOpath);
      myAbort (0);
    }
    if (TraceFlags)
      printf ("MARS: Listen on fifo %s\n", FIFOpath);
#endif /* USE_FIFO_FILES */

    FD_ZERO (&PORTready);
    FD_ZERO (&PORTmask);
    if (TCPsocket >= 0)
      FD_SET (TCPsocket, &PORTmask);
    if (UNIXsocket >= 0)
      FD_SET (UNIXsocket, &PORTmask);
    if (FIFOdescr >= 0)
      FD_SET (FIFOdescr, &PORTmask);
    for (i=0; i<FD_SETSIZE; ++i)
      agent2fd [i] = fd2agent [i] = -1;
  }
}

/*
 * Строчный ввод/вывод.
 *
 * FileLine (NIL, -fd) - установка файла
 * FileLine (buf, sizeof (buf)) - чтение строки
 */
int FileLine (char* line, short len) {
  static FILE *fd;
  if (len < 0) {
    if (fd)
      fclose (fd);
    fd = fdopen (-len, "r");
    return (0);
  }
  if (! fd || ! fgets (line, len, fd))
    return (-1);
  len = strlen (line);
  if (len>0 && line[len-1]=='\n')
    line[--len] = 0;
  return (len);
}

/*
 * Трассировочная диагностика.
 */
void Reply0 (agent who, char* text) {
  if (! TraceFlags)
    return;
  if (who == NIL)
    printf ("MARS: Sys: %s\n", text);
  else if (who == (agent) -1)
    printf ("MARS: Inp: %s\n", text);
  else
    printf ("MARS: %d: %s\n", (unsigned char) who->ident, text);
}

/*
 * Сброс буферов файла.
 */
void SyncFlush (fileid label) {
#ifdef NO_FSYNC
  sync ();
#else
  fsync (label);
#endif
}

/*
 * Файловый ввод/вывод.
 */
void DiskIO (fileid label, char* bufaddr, unsigned bufleng, unsigned blocknum, int wr) {
  if (lseek (label, (long)blocknum*bufleng, 0) < 0)
    myAbort (-1);
  if (wr) {
    if (write (label, bufaddr, bufleng) != bufleng)
      myAbort (-1);
  } else
    if (read (label, bufaddr, bufleng) != bufleng)
      myAbort (-1);
}

/*
 * Диагностика о фатальной ошибке и выход.
 */
void myAbort (int code) {
  if (code < 0)
    printf ("MARS: %s\n", strerror (errno));
  else if (code)
    printf ("MARS: fatal error: %d\n", code);
  if (TCPsocket >= 0)
    close (TCPsocket);
  if (UNIXsocket >= 0) {
    close (UNIXsocket);
    unlink (UNIXport);
  }
  if (FIFOdescr >= 0)
    close (FIFOdescr);
  if (*FIFOpath)
    unlink (FIFOpath);
  exit (-1);
}

/*
 * Перестановка порядка байтов.
 */
#ifdef i386
static inline unsigned swaps (unsigned v) {
  asm ("xchgb %%ah,%%al" : "=a" (v) : "a" (v));
  return v;
}

static inline unsigned long swapl (unsigned long v) {
  asm ("xchgb %%al,%%ah; ror $16,%%eax; xchgb %%al,%%ah" : "=a" (v) : "a" (v));
  return v;
}

static inline double swapd (double v) {
  asm ("xchg %%eax,%%edx; "
    "xchgb %%al,%%ah; ror $16,%%eax; xchgb %%al,%%ah; "
    "xchgb %%dl,%%dh; ror $16,%%edx; xchgb %%dl,%%dh"
    : "=r" (v) : "r" (v));
  return v;
}
#else
#define swaps ntohs
#define swapl ntohl
double swapd (double v);
#endif

void SwapBytes (char* data, int length) {
  switch (length) {
  case 0: return;
  case sizeof (short):  *(short*)data  = swaps (*(short*)data);  break;
  case sizeof (long):   *(long*)data   = swapl (*(long*)data);   break;
  case 2*sizeof (long): *(double*)data = swapd (*(double*)data); break;
  default: printf ("MARS: bad SwapBytes\n");
  }
}

#ifdef vax
static double swapd (double v) {
  *(short*)&v  = swaps (*(short*)&v);
  *(1 + (short*)&v) = swaps (*(1 + (short*)&v));
  *(1 + (long*)&v) = swapl (*(1 + (long*)&v));
  return (v);
}
#endif
/*
 * Инвертирование плавающего числа.
 */
void InvFloat (real* data) {
  register unsigned long* ptr = (unsigned long*) data;
  register length = sizeof (real) / sizeof (unsigned long);
  while (length--)
    *ptr++ ^= ~ (unsigned long) 0;
}

#ifdef NO_MKFIFO
int mkfifo (char *name, int mode) {
  return (mknod (name, S_IFIFO | mode, 0));
}
#endif
                       /* ====================== *\
                       ** =  MISS environment  = **
                       \* ====================== */

#elif defined(MISS)
#include <SysIO>
#include <FileIO>
#include <Display>
#include <DeviceIO>
#include <TimeSystem>
#include <SysCalls>
#include <FileSystem>


#define DeviceClass 'S'                   /* класс устройства         */
#define DeviceNumber 0                    /* номер устройства         */
#define TimeWait     10                   /* цикл опроса при простое  */

  static short    fileinput = 0;          /* ввод из файла            */
  static unshort  Device;

/*
 * Установка канала ввода/вывода: файл (экран) или у-во
 */
void SetDevice(label) int label; {
  int retcode;
  if(label > 0) {
    FileLine(NIL,-label);
    fileinput=1;
  } else if (label < 0) {
    if( (retcode = TakeDev(DeviceClass,DeviceNumber))<0)
      _abort_(retcode);
    Device = _DevIdn; fileinput = 0xFF;
  }
}

void FreeChannel(who) int who; {who=0;}

/*
 * Трассировочные выдачи
 */
void Reply0(from,text) agent from; char* text; {
  static char header[] =
             {TA,RN,DL,HO,CU, SC,FI_INVRS,0, "  ", num="XXX:",SC,0,0,0};
  short inpsym;
  MVS((from ? _conv(from->ident,0x83,16) : "Sys"),3,header+num);
  dpc(header); dpc(text);
  if ((inpsym =dpm())>=0) OperSymbol = inpsym;
  if (OperSymbol == ' ') {                /* приостановим выдачу      */
    OperSymbol = dpq('_');
  }
}

/*
 * Обмен информацией с абонентами
 */
void Talk() {
  static short srvbuf[2];
  static Device_block CB      =
        { NIL          , 0             , {i= {0,0}} };
  static Device_block CBfirst =
        { (char*)srvbuf, sizeof(srvbuf), {i= {0,0}} };

  short  symbol;
  int    retcode;

  static char prompt[] =
      {TA,RN,DL,HO,CU,SC,FI_HIGHL,0,"Input:",SC,0,0,0};

  Timer = (long)(GetDayNum()+58)*(60*24)+GetMinutes();
  SliceTimer = GetDecSecs()/10+GetMinutes()%(TimerCycle/60)*60;

  /* обработка команд оператора */
  if ((symbol = dpm())>=0) OperSymbol = symbol;

  if (OperSymbol >=0) {
    switch(OperSymbol) {
    case DEL:                               /* отключились            */
      TraceFlags = 0;
    break; case ' ':
    break; case FinSym:
      if(fileinput == 0xFF) FreeDev(Device);
      _exit_(0);
    default:
      Show((char) OperSymbol); OperSymbol = -1;
    }
  }

  /* идет работа с каналом ? */
  if(fileinput == 0xFF) {                   /* работаем с каналом     */

    /* вывод ответа */
    if (OutAgent >= 0) {                    /* ответ есть             */
      srvbuf[0]= OutAgent;
      srvbuf[1]= CB.buflen=
         (*OutBuffer>=(short)sizeof(short)? *OutBuffer:sizeof(short)*2);
    } else {                                /* отвечать нечего        */
      srvbuf[0]=(Ready==NIL && !(TraceFlags & TraceCommands) ? -2 : -1);
                                            /* ждать / не ждать ответа*/
    }
    CBfirst.s.i.supl1 = TimeWait;

    if ((retcode = DeviceIO(&CBfirst,Device,1)) <0) _abort_(retcode);

    if(OutAgent>=0) {                       /* выводим сам ответ      */
      CB.bufadr = (char*) OutBuffer;
      retcode = DeviceIO(&CB,Device,1);
      if (retcode < 0) {                    /* ошибка передачи        */
        static char abortcomm[] = {'$',0};  /* "отключение без подтв."*/
        agent aborted = AgentTable + OutAgent;
        MVS((char*)&initial, sizeof(jmp_buf), (char*) &aborted->pentry);
        aborted->waittime = SliceTimer;      /*время поступления запр.*/
        aborted->inpbuf = abortcomm;
        aborted->status = ready; Inqueue(&Ready,aborted);
      }
    }
    OutAgent = -1;                          /* ответ выведен          */

    /* ввод запроса */
    if((retcode = DeviceIO(&CBfirst,Device,0))<0) _abort_(retcode);

    if( *srvbuf >=0) {                          /* есть ответ         */
      boolean overflow = srvbuf[1] > MaxInput;  /* длина блока велика?*/
      CB.bufadr = CommonBuffer+1;
      CB.buflen = (overflow ? 1 : srvbuf[1]);
      retcode   = DeviceIO(&CB,Device,0);       /* читаем данные      */
      if(!overflow && retcode < 0) _abort_(retcode);

      CommonBuffer[0] = (char) *srvbuf+'0';     /* добавим код канала */
      if(overflow) CommonBuffer[1]='$';
      CommonBuffer[CB.buflen+1] = ';';          /* и ограничитель     */
      CommonBuffer[CB.buflen+2] =  0 ;
      if(TraceFlags & TraceInput) {
        dpc(prompt);
        dpc(CommonBuffer[1] == ' ' ? CommonBuffer :
        Fdump("%c%c%c%( %4.16)",CommonBuffer[0],CommonBuffer[1],CommonBuffer[2],
               (unsigned)CB.buflen-2,(short*)(CommonBuffer+3)));
      }
    } else {
      *CommonBuffer = ' ';                      /* иначе - пустой цикл*/
      CommonBuffer[1] = 0;
      if ((TraceFlags & TraceCommands) && Ready == NIL) Delay(3,TRUE);
    }
  } else {
    if(TraceFlags & TraceInput) dpc(prompt);
    if(fileinput !=0) {
      if(FileLine(CommonBuffer,MaxInput-0x100) <0) {
        fileinput=0;
      } else {
        if(TraceFlags & TraceInput) dpc(CommonBuffer);
      }
    }
    /* пошел ввод с экрана */
    if(!fileinput) {
      char sizeh = GetScreenSize().h;
      int size;
      int comm;
      size = MaxInput /\ (sizeh-7);
      if(! (TraceFlags & TraceInput)) dpc(prompt);
      comm = Xdpr(CommonBuffer,(unchar)size,0,0); CommonBuffer[size]=0;
    }
  }

  if (*CommonBuffer>='0' && *CommonBuffer<'0'+NbAgents) {
    agent reader = &AgentTable[*CommonBuffer-'0'];
    if (reader->status != reading) {
      Reply0(reader,"Agent busy");
    } else {
      CommonBuffer[1]  = UpperCase(CommonBuffer[1]);
      reader->inpbuf   = CommonBuffer+1;
      reader->queue    = Ready; Ready=reader;/*встаем в голову очереди*/
      reader->status   = ready;              /*чтоб сразу взять данные*/
      reader->waittime = SliceTimer;         /*время поступления запр.*/
    }
  } else if (*CommonBuffer != ' ' && *CommonBuffer != '%' ) {
    Reply0(NIL,"Invalid identifier");
  }

}

/*
 * Файловый ввод/вывод
 */
void DiskIO(label,bufaddr,bufleng,blocknum,dir)
   fileid label; char* bufaddr; unsigned bufleng,blocknum; boolean dir;{
  IOl_block  CB;
  IOl_block* ptrCB = &CB;
  int        retcode;

  CB.bufadr = bufaddr;
  CB.buflen = bufleng;
#if   defined(R_11)
  CB.sector = (unlong) blocknum * (bufleng/SectorSize);
#elif defined(i_86)
  CB.sector = (unlong) blocknum * (bufleng/GetSectorSize(label));
#else
#error "Unknown processor"
#endif
  CB.label  = (filelabel) label;
  CB.label  = (filelabel) label;
  CB.dir    = (dir & 0x7F  ? 3 : 2);

#ifdef ZCache
  if (dir & 0x80) {                          /* ввод/вывод под Z      */
#if defined(R_11)
    *(unsigned*) &ptrCB |= 1;
#elif defined(i_86)
    CB.dir   |= 0x10;
#else
#error "Unknown processor"
#endif
  }
#endif
  if((retcode = _io_(ptrCB)) != (int) ptrCB) _abort_(retcode);
}

void SyncFlush(label) fileid label; {
  static long MaxSize = 0x7FFFFF;
  SetEOF(label,MaxSize);
}


#define  EOFsymbol  0x00
#define  EOLsymbol  0x0A
#define  SkipSymbol 0x0D
  static unshort FileBlock=0x100;
  static fileid  label;
  static unshort sector;
  static unshort pbuf;

int FileLine(addr,parm) char* addr; short parm; {
  int retcode = 0;
  if(parm<0) {
    label = (fileid) -parm; sector=0;
#ifdef i_86
    FileBlock=GetSectorSize(label);
#endif i_86
    pbuf=FileBlock;
  } else {
    char*   FLineBuffer = CommonBuffer+sizeof(CommonBuffer)-FileBlock;
    for(;;) {
      boolean endflag = FALSE;
      if(pbuf >= FileBlock) {
        DiskIO(label,FLineBuffer,FileBlock,sector,0); sector++; pbuf=0;
      }
    if(retcode>=parm) break;
      switch(FLineBuffer[pbuf]) {
      case EOFsymbol:
        return(-1);
      case EOLsymbol:
        endflag = TRUE;
      break; case SkipSymbol:
      break; default:
        *addr++ = FLineBuffer[pbuf]; ++retcode;
      }
      ++pbuf;
    if(endflag) break;

    }
    *addr = 0;
  }
  return(retcode);
}

#if defined(i_86)
#include <i86/register.h>

long setbit(register long value:rAX:rDX, register unsigned nbit :rCX) {
  pragma BUSY(rAX,rDX,rCX);
  if ( nbit<15 ) {
    dx |= 0x4000u >> nbit;
  } else {
    nbit -= 15;
    ax |= 0x8000u >> nbit;
  }
  pragma FREE(rAX,rDX,rCX);
  return(value);
}

long clrbit(register long value:rAX:rDX, register unsigned nbit :rCX) {
  pragma BUSY(rAX,rDX,rCX);
  if ( nbit<15 ) {
    dx &= ~(0x4000u >> nbit);
  } else {
    nbit -= 15;
    ax &= ~(0x8000u >> nbit);
  }
  pragma FREE(rAX,rDX,rCX);
  return(value);
}

boolean tstbit(register long value:rAX:rDX,register unsigned nbit :rCX){
  pragma BUSY(rAX,rDX,rCX);
  if ( nbit<15 ) {
    ax = dx & (0x4000u >> nbit);
  } else {
    nbit -= 15;
    ax = ax & (0x8000u >> nbit);
  }
  pragma FREE(rAX,rDX,rCX);
  return( ax );
}


#pragma OFF(STACK_CHECK)

void SwapBytes(register char* pdata:rSI, register unshort length:rCX) {
  register char* pedata:rDI;
  register char  temp  :rAL;
  if ( length>1 ) {
  pragma BUSY(rSI,rDI,rCX);
    pedata= pdata+length-1;  length /= 2;
    loop:
      temp= *pdata;  asm xchg al,*pedata;  *pdata = al;
      ++pdata;  --pedata;
    asm loop loop;
  pragma FREE(rSI,rDI,rCX);
  }
}

void ZSwapBytes(register unshort pdata :rSI,
                register unshort length:rCX) {
  register char _es * pedata:rDI;
  register char       temp  :rAL;
  if ( length>1 ) {
  pragma BUSY(rSI,rDI,rCX);
    pedata= (char _es *)pdata+length-1;  length /= 2;
    loop:
      temp= *(char _es *)pdata;  asm xchg al,*pedata;
      *(char _es *)pdata = al;
      ++pdata;  --pedata;
    asm loop loop;
  pragma FREE(rSI,rDI,rCX);
  }
}

void InvFloat(register real *pdata:rSI)  {
  register short      length:rCX;

  length = sizeof(real)/sizeof(unsigned);

  loop:
    *(unsigned *)pdata = ~ *(unsigned *)pdata;
    ++(unsigned *)pdata;
  asm loop loop;
}

void ZInvFloat(register unshort pdata:rSI)  {
  register short      length:rCX;

  length = sizeof(real)/sizeof(unsigned);

  loop:
    *(unsigned _es *)pdata = ~ *(unsigned _es *)pdata;
    ++(unsigned _es *)pdata;
  asm loop loop;
}
#pragma RESTORE(STACK_CHECK);
#endif

                    /* =========================== *\
                    ** =  Macintosh environment  = **
                    \* =========================== */

#elif defined(macintosh)

# define __MYTYPES__
# include "MacDefines"

         char*      RcvBuffer;

         boolean    EndFlag = FALSE;
  static boolean    debug   = TRUE;
  extern boolean    IsSleeping,wasWakedUp;

         void       quit(void);

  extern void       ATinit(void);
  extern void       ATloop(short*,short*);
  extern void       ATclose(void);
  extern void       CloseFiles(void);
 
  extern void       MacProcess(void);
  extern void       MacTrace(char*,char*);


void Reply0(who,text) char* text; agent who; {
  MacTrace(who == NIL       ? "Sys" :
           who == (agent)-1 ? "Inp" : _conv(who->ident,0x83,16),text);
}


void Talk() {
  agent       reader = NIL;
  short       itemnum;
  static      char DebugBuffer[200];
  WindowPtr   theWindow;
  Rect        Box;
  Handle      Item;
  boolean     debinput = FALSE;
  unsigned long systime;
  short       RcvSize;

  GetDateTime(&systime);
  Timer      = systime/60+((long)1460*(24*60));
  SliceTimer = systime%(unsigned short)3600;


  wasWakedUp = FALSE; IsSleeping = TRUE;
  if (! debug) {
    short  who;
    ATloop(&who,&RcvSize);
    if(who >= 0) {reader = AgentTable+who; IsSleeping = FALSE;}
  }
  MacProcess();
  IsSleeping = FALSE;
  if(EndFlag) quit();

  if (reader != NIL) {
    if(TraceFlags & TraceInput) {
      Reply0((agent) -1,RcvBuffer[0] == ' ' ? RcvBuffer :
             Fdump("%c%c%( %4.16)",RcvBuffer[0],Max(RcvBuffer[1],' '),
                   (unsigned)(RcvSize-2),(short*)(RcvBuffer+2)));
    }
    if (reader->status != reading) {
      Reply0(reader,"Agent busy");
    } else {
      reader->inpbuf   = RcvBuffer;
      *RcvBuffer       = UpperCase(*RcvBuffer);
      reader->queue    = Ready; Ready = reader;
      reader->status   = ready;
      reader->waittime = SliceTimer;
    }
  }

}

void SetDevice(i) int i; {
  if(i<0) {
    OutAgent  = -1;
    SetCursor(*GetCursor(watchCursor));
    ATinit();
    InitCursor();
    debug     = FALSE;
  }
}

void DiskIO(label,bufaddr,bufleng,blocknum,dir)
       fileid label; char* bufaddr; unsigned bufleng,blocknum; int dir;{
  long count  = bufleng;
  int retcode = SetFPos(label,fsFromStart,(long)blocknum*bufleng);
  if(retcode !=0) myAbort(retcode);

  retcode = (dir ? FSWrite(label,&count,bufaddr)
                 : FSRead (label,&count,bufaddr) );
  if(retcode !=0) myAbort(retcode);
}

void SyncFlush(label) fileid label; {
  short volid;
  int   retcode;
  GetVRefNum(label,&volid);
  if( (retcode = FlushVol(NIL,volid)) != 0) myAbort(volid);
}

void myAbort(code) int code; {
  StringHandle theString;
  if(code >= 0) {
    ParamText("\pFatal MARS",C2P(Fdump("%3.10",(unsigned)code)),"",NIL);
  } else if((theString = GetString(-code)) != NIL) {
    ParamText("\pSystem",*theString,"\p:",NIL);
  } else {
    ParamText("\pSystem",C2P(Fdump("-%4.10",(unsigned)(-code))),"",NIL);
  }
  InitCursor();
  Alert(AbortID,NIL);
  quit();
}

unchar* C2P(addr) char* addr; {
  static Str255 buffer;
  *buffer = (unchar) ((char*)_cps(addr,0xFF,0)-addr);
  memmove(buffer+1,addr,*buffer);
  return(buffer);
}

#define EOL 0x0D
int FileLine(buffer,comm) char* buffer; short comm; {
  static short label;
  static long  filesize = 0;
  static long  pos      = 0;
  int    retcode;
  if(comm<0) {
    label = -comm;
    GetEOF(label,&filesize); pos = retcode = 0;
  } else if (pos >= filesize) {
    retcode = -1;
  } else {
    long count = comm-1;
    retcode    = -1;
    SetFPos(label,fsFromStart,pos);
    FSRead(label,&count,buffer);
    while(++retcode<count && buffer[retcode] != EOL);
    buffer[retcode]=0;
    pos += retcode + (retcode<count);
    while(--count>=0) if(buffer[count] == '\t') buffer[count]=' ';
  }
  return(retcode);
}

void InvFloat(ptr) real* ptr; {
  int    i = sizeof(real)/sizeof(short);
  short* p = (short*)ptr;
  while(--i>=0) *p++ ^= ~0;
}


                       /* ======================= *\
                       ** =  MSDOS environment  = **
                       \* ======================= */
#elif defined(MSDOS)
#include        <conio.h>
#include        <dos.h>
#include        <windows.h>
#include        <dde.h>
#include        <errno.h>
#include        <stdlib.h>
#include        "mars.h"
#include        "marsdefs.h"

  extern HWND     hTableWnd,
                  hInputDlg,
                  hOptionsDlg,
                  hTraceWnd;
  extern HANDLE   hMarsTask;
  extern HANDLE   hInst;
  HWND            hAgents     [MaxAgents];  ???
  extern HANDLE   hTraceMap;

  extern short    xchTable, ychTable;
  extern short    xchTrace, ychTrace;

  void            _lstrcpy (LPSTR, LPSTR) ;
  void            _lmemcpy (LPSTR, LPSTR, WORD) ;
  int             _lstrlen (LPSTR);

  int             errno ;
  extern HANDLE   hCache;
  LPSTR           lpCache;

  static char     RegKeys[] = { 'A', 'C', 'D', 'W', 'L', 'P' };

  long FAR PASCAL _llseek(int, long, int);
  int  FAR PASCAL _lread (int, char far *, unsigned);
  int  FAR PASCAL _lwrite(int, char far *, unsigned);
  int  FAR PASCAL _lflush(int);

void DiskIO(label,bufaddr,bufleng,blocknum,dir)
       fileid label; char* bufaddr; unsigned bufleng,blocknum; int dir;
{
  char far *lpbuf;

  long retcode =
      _llseek((int)label,(long)((long)blocknum*(long)bufleng),(int)0);

  if(retcode < 0) myAbort((int)retcode);

  if (dir & 0x80) {                               /* ???? */
    lpbuf = lpCache + (unsigned)bufaddr;
    dir &= 1;
  } else {
    lpbuf = (char far *)bufaddr;
  }

  (long) retcode =
     (dir ?(int) _lwrite(label, lpbuf, bufleng)
          :(int) _lread (label, lpbuf, bufleng) );
  if((int)retcode < 0) myAbort(errno);
?? Иван, а может еще -
  if(retcode != lpbuf) myAbort(...);
/*return((int)retcode);*/
}


/*
 * Отладочная выдача    (windows version)
 */
void Reply0(who,text) char* text; agent who; {
  int             i ;
  char            messagebuf  [200] ;
  HANDLE          hMemory;
  unsigned        lmsg;

  if ( hTraceWnd ) {
  LPSTR          lps;
  TRACEMAP FAR * lpMap = (void far *) GlobalLock(hTraceMap);
    lmsg = sprintf (messagebuf,
        "%s:%s", (who ==         0 ? "Sys" :
                  who == (agent)-1 ? "Inp" : _conv(who->ident,0x83,16)),
        text) ;
  /*
    if (who != (agent)-1 && hInputDlg)
        SetDlgItemText (hInputDlg, IDD_IREPLY, messagebuf) ;
   */

    if ( lpMap->hMem[lpMap->ptr] != NULL )
        GlobalFree(lpMap->hMem[lpMap->ptr]);
    else
        ++lpMap->Nelems;

    hMemory = lpMap->hMem[lpMap->ptr] =
        GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, lmsg+1);
    lps = GlobalLock(hMemory);
    *lps = (char)lmsg;
    _lmemcpy(lps+1, messagebuf, lmsg);
    if ( ++lpMap->ptr>=MaxTraceLine )   lpMap->ptr = 0;
    GlobalUnlock( hTraceMap );

    ScrollWindow( hTraceWnd, 0, -ychTrace, NULL, NULL );
    UpdateWindow( hTraceWnd );
  }
}


  extern jmp_buf    abortjump;
void MyAbort(code) int code; {
  static char errtext[] = "Aborted with code = %5.10";

  Reply(Fdump(errtext,(unsigned)code));
  longjmp(abortjump,code);
}

/*      Windows version         */

void Talk() {
  MSG       msg;
  BOOL      done;

  if (OutAgent >= 0) {
    WORD    MsgLeng = *OutBuffer<sizeof(short) ? sizeof(short)*2 : *OutBuffer;
    WORD    hiReply = NULL,
            loReply = NULL;

    if (*OutBuffer > 2) {
    LPSTR   lpBuffer;
    HANDLE  hMemory;
        hMemory =
            GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, *OutBuffer);
        if (hMemory) {
            lpBuffer = GlobalLock (hMemory) ;
            _lmemcpy((LPSTR)lpBuffer, (LPSTR)OutBuffer, *OutBuffer);
            GlobalUnlock (hMemory) ;
        }
        hiReply = hMemory;
    } else if (*OutBuffer<0) {
        loReply = OutBuffer[0];   hiReply = OutBuffer[1];
    }
    PostAppMessage(hAgents[OutAgent], WM_DDE_ACK, hMarsTask,
        MAKELONG(loReply, hiReply)) ;
    OutAgent = -1;
  }

  /* Timer = (long)(_date()+58)*(60*24)+_time(); */

  GlobalUnlock(hCache);

  /* ... */
  for (;;) {
    done = FALSE;
    while ( !done && PeekMessage (&msg, NULL, NULL, NULL, PM_REMOVE) )
    {
      switch ( msg.message ) {
      case WM_DDE_EXECUTE:
        if ( msg.wParam == NULL ) {       /* console input */
          done = TRUE;   msg.wParam = ~0;
        } else if ( (int)(msg.wParam=SearchAgent(msg.wParam)) >= 0 ) {
          done = TRUE;
        }
      break;  case WM_DDE_TERMINATE:
        if ( (int)(msg.wParam=SearchAgent(msg.wParam)) >= 0 )
            hAgents[msg.wParam] = NULL ;
      break;  case WM_QUIT:
        myAbort(0x8000);
      break;  default:
        if ( (hInputDlg  ==NULL || !IsDialogMessage(hInputDlg  , &msg)) &&
             (hOptionsDlg==NULL || !IsDialogMessage(hOptionsDlg, &msg)) )
        {
          TranslateMessage(&msg);   DispatchMessage(&msg);
        }
      }
    }
  if ( done || Ready )  break;            /* there are active agents */
    /* close all files */
    { extern  int         TempFile,
                          UpdFile,
                          SwapFile;
      close(TempFile);    TempFile  = -1;
      close(UpdFile);    UpdFile  = -1;
      close(SwapFile);   SwapFile = -1;
    }
    WaitMessage();
  }
  /*
   *    Message from our agent
   *
   *    wParam         - Agent number
   *    HIWORD(lParam) - command handle
   *    LOWORD(lParam) - size of command handle
   */
  /* reopen all files, if it's neccessary */
  if ( TempFile == -1 )
    { extern  OFSTRUCT    ofTmpFile,
                          ofUpdFile,
                          ofSwpFile;
      extern  int         TempFile,
                          UpdFile,
                          SwapFile;
      TempFile = OpenFile( (LPSTR)NULL, &ofTmpFile, OF_REOPEN | OF_READWRITE );
      UpdFile  = OpenFile( (LPSTR)NULL, &ofUpdFile, OF_REOPEN | OF_READWRITE );
      SwapFile = OpenFile( (LPSTR)NULL, &ofSwpFile, OF_REOPEN | OF_READWRITE );
    }
   lpCache = GlobalLock(hCache);
   if ( done ) {
     if ( msg.wParam == ~0 ) {     /* console */
     int      nCopy;
       nCopy = GetDlgItemText(hInputDlg, ID_INPEDIT,
           (LPSTR)CommonBuffer, sizeof(CommonBuffer)-1 );
       CommonBuffer[nCopy] = 0;
     } else {
     BOOL     overflow = LOWORD(msg.lParam) >= MaxInput-4;
     LPSTR    lpCommand;
       lpCommand = GlobalLock (HIWORD(msg.lParam)) ;
       CommonBuffer[0] = (char) msg.wParam + '0';
       _lmemcpy(CommonBuffer+1, lpCommand, min(LOWORD(msg.lParam), MaxInput-4));
       GlobalUnlock(HIWORD(msg.lParam));     GlobalFree(HIWORD(msg.lParam));
       CommonBuffer[LOWORD(msg.lParam)+1] = ';';
       CommonBuffer[LOWORD(msg.lParam)+2] =  0 ;
       if (overflow)   CommonBuffer[1] = '$';
     }
     if ( TraceFlags & TraceInput )
         Reply0( (agent)-1, CommonBuffer );
   } else {
     CommonBuffer[0]=' ';  CommonBuffer[1]=0;
   }

  /* Timer = dbtime () ; */

  if (*CommonBuffer>='0' && *CommonBuffer<'0'+NbAgents) {
    agent reader = &AgentTable[*CommonBuffer-'0'];
    reader->privil = 0xFF;
    if (reader->status != reading) {
      Reply0(reader,"Agent busy");
    } else {
      reader->inpbuf= CommonBuffer+1;
      reader->queue = Ready;   Ready = reader;
      reader->status= ready;
    }
  } else if (*CommonBuffer != ' ' && *CommonBuffer != '%' ) {
    Reply0(NIL,"Error identifier");
  }
}

void SyncFlush(label) fileid label; {
}


#define  FileBlock  0x100
#define  EOFsymbol  0x00
#define  EOLsymbol  0x0A
#define  SkipSymbol 0x0D
#define  TA         0x09

  static fileid  label;
  static unshort sector;
  static unshort pbuf;
  static unshort lbuf;

int FileLine(addr,parm) char* addr; short parm; {
  char*   FLineBuffer = CommonBuffer+sizeof(CommonBuffer)-FileBlock;
  int retcode;

  retcode = 0;
  if(parm<0) {
    label = (fileid) -parm; sector=0; pbuf=FileBlock;  lbuf=FileBlock;
  } else {
    for(;;) {
      boolean endflag;
      if (pbuf>=lbuf) {
        if (lbuf<FileBlock) {
          return ( retcode ? retcode : -1 );
        } else {
          _llseek((int)label,(long)(FileBlock*sector),(int)0);
          lbuf = (int) _lread (label, (far char*)FLineBuffer,FileBlock);
          ++sector;  pbuf = 0;
        }
      }
      endflag = FALSE;
    if(retcode>=parm) break;
      switch(FLineBuffer[pbuf]) {
      case EOFsymbol:
        return(-1);
      case EOLsymbol:
        endflag = TRUE;
      break; case SkipSymbol:
      break; case TA:
        *addr++ = ' '; ++retcode;
      break; default:
        *addr++ = FLineBuffer[pbuf]; ++retcode;
      }
      ++pbuf;
    if(endflag) break;

    }
    *addr = 0;
  }
  return(retcode);
}

void _lstrcpy (LPSTR dst, LPSTR src) {
    while (*dst++ = *src++);
}

int  _lstrlen (LPSTR src) {
    int   leng=0;
    while (*src++)  ++leng;
    return(leng);
}

void _lmemcpy (LPSTR dst, LPSTR src, WORD leng) {
    while (leng--)
        dst [leng] = src [leng] ;
}

int  SearchAgent (HWND hWnd) {
  int  i ;
  for (i=0; i<sizeof (hAgents) / sizeof (HWND); i++)
      if (hAgents[i]==hWnd)
          return i ;
  return -1 ;
}

void SetDevice(i) int i; {}


#else
# error "Unknown Operating System"
#endif


                      /* ========================= *\
                      ** = non-MISS environment  = **
                      \* ========================= */
#if ! defined(MISS)

char* _conv(data,leng,radix) unsigned data; int leng,radix; {
  static char buffer[34] = " ";
  char* ptr = &buffer[sizeof(buffer)-1];
  int i = leng & 0x1F;
  while(--i>=0) {
    int digit = data % radix; data /= radix;
    *--ptr = (digit <10 ? digit : digit + ('A'-'9'-1)) +'0';
  if(data == 0) break;
  }
  while(--i>=0) *--ptr = (leng & 0x80 ? '0' : ' ');
  return(ptr);
}

unsigned _convi(text,leng,radix) char* text; int leng, radix; {
  unsigned value = 0,i,l;
  leng &= 0x7f;
  for(i=0; i<leng; ++i) {
    l = *text==' ' ? 0 : *text<='9' ? *text-'0' : *text-'A'+10;
  if ( *text<' ' || (*text==' ' && value) || l>=radix)  break;
    value = value*radix + l;
    ++text;
  }
  return(value);
}

unsigned Min(a,b) unsigned a,b; { return(a>=b ? b : a);}
unsigned Max(a,b) unsigned a,b; { return(a <b ? b : a);}
void TRS(a,b,c) unchar* a; unsigned b; unchar* c; {
  register unchar* pa = a;
  while(b-- != 0) {*pa = c[*pa]; ++pa;}
}

#ifndef unix
boolean tstbit(long data, unshort bit) {return(data &  (0x40000000>>bit))!=0l;}
long    clrbit(long data, unshort bit) {return(data & ~(0x40000000>>bit));}
long    setbit(long data, unshort bit) {return(data |  (0x40000000>>bit));}
#endif
#endif
