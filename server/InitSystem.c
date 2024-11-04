                   /* ============================= *\
                   ** ==         M.A.R.S.        == **
                   ** ==    Системная инициация  == **
                   \* ============================= */

#include "FilesDef"
#include "CacheDef"

	 startmode InitSystem(int,char**);
         void      InitParams(table*,short*);
         void      ParmError (short, char*);

                       /* ====================== *\
                       ** =  MISS environment  = **
                       \* ====================== */
#if defined(MISS)
#define  MaxMes 40
#include <Display>
#include <SysErrors>
#include <SysUtility>
#include <StartOpers>
#include <FileSystem>
#include <ActRecord>
#include <Passwords>
#include <TimeSystem>
#include <SysConv>
#include <SysFilesOp>
#include <SysFiles>

#define CacheSegment ('D'*0x100+'B')    /* имя сегмента CASH          */
#define DelayTime   100                 /* AutoLog -> ждем 10 сек.    */
#if defined(R_11)
#  pragma SYSCALL(_regime_ = 0x22);
#elif defined(i_86)
   extern void _regime_(register char: rAL) : 0xA2;
#endif

startmode InitSystem(code, argv) int code; char **argv; {
  startmode mode = RG_start;
  int       retcode;

  theTimeZone = GetTimeZone();

  /* производим системную инициализацию */
  if (*GDIFF->owncatal.processor.c != 0) {   /* есть медиатор         */
    static Fname WorkName = {"DATA      "};
    FileItem WorkItem;
    fileid label;
    static char logpref[] =
              {CD,TA,DL,HO,CU,EL,SC,FI_INVRS+FI_HIGHL+FI_BLINK,1,0};
    static char logrec [] =
              {"MARS    (",basename="0123456789) ",
                 treg="started",0,"with code =",tcode='+',[MaxMes]0,0};
    *(Fname*)(logrec+basename) = GetFileItem(OwnCatal)->name;

    if (*GDIFF->envir) {                     /* останов системы       */
      if(code != 0) {
        static Fname syserrors = {NameSysMessage};
        MVS("aborted ",8,logrec+treg);
        /* достаем код сообщения */
	if(code <0 && GetString(~code,syserrors) == 0) {
          MVS(AZC,MaxMes,logrec+tcode);      /* переносим текст сообщ.*/
        } else {                             /* выводим номер сообщ.  */
          if (code < 0) {code = -code; logrec[tcode]='-';}
          MVS(_conv(code,0x85,10),5,logrec+tcode+1);
        }
      } else {                               /* нормальное окончание  */
        MVS("finished",9,logrec+treg);
      }
      SysLog(logrec);                        /* выводим log-запись    */
      dpc(logpref); dpc(logrec); dpq('!');
      SayBye();                              /* прощаемся с Системой  */
    }
    *GDIFF->envir = TRUE;                    /* флаг "запущены"       */
    SysLog(logrec);
    /* создаем рабочий каталог */
    _fill((char*) &WorkItem,sizeof(FileItem),0);
    WorkItem.name   = WorkName;
    WorkItem.type   = FCatal;
    WorkItem.p      = _OwnPassword;
    WorkItem.access = FilAccNo;
    if( (retcode=OpenFile(&WorkItem,OwnCatal))<0) myAbort(retcode);
    label = (filelabel) retcode;
    if( (retcode =    LockFile(label))<0 ||
        (retcode = ChngWorkCat(label))<0  ) myAbort(retcode);
  }

  if( (retcode = TuneSegment(CacheSegment))<0  ) myAbort(retcode);
  NbCacheBuf = _SegmentSize/BlockSize;
  if(NbCacheBuf < 10) myAbort(ErrMemory);
  _regime_(0x0C);

  /* запрос режима запуска */
  if (GetScreenSize().v != 0) {
    static char initdpc[] =
       { ER, SC, FI_HIGHL, 0, TA, SP, 0, 0xFF, SM, 34/2, CL,
         "MultiAccess Relational data System",0};
    static char* initnames[] =
       {
         "   Start  ", "   Debug  ",
         "   Command", "   Restore",
         "   Init   ",
/*       "   Check  "          */
       };

    for(;;) {
      static char ONhighl [] = {SC,FI_HIGHL+FI_INVRS,1,0};
      static char OFFhighl[] = {SC,FI_INVRS         ,0,0};
      int nmodes = sizeof(initnames)/sizeof(char*);
      dpsize screensize;
      screensize = GetScreenSize();

      dpc(initdpc);
      dpc(OFFhighl);
      for(mode=0; mode<nmodes; ++mode) {
        initnames[mode][0] = SP;
        initnames[mode][1] = (char) ((screensize.v-nmodes)/2 + mode);
        initnames[mode][2] = (char) ((screensize.h-8)/2);
        dpc(initnames[mode]);
      }

      mode = RG_start;
      for (;;) {
        char symbol;
        dpc(ONhighl); dpc(initnames[mode]);

        SetCursorPos(initnames[mode][1],initnames[mode][2]-1);
        symbol = dpq(0);

        dpc(OFFhighl); dpc(initnames[mode]);

      if (symbol == ET) break;

        switch (symbol) {
        case CU:
          if (--mode<0) mode += nmodes;
        break; case CD:
          if (++mode>=nmodes) mode = 0;
        break; case DEL: case FinSym:
          _exit_(0);
        default:
          dpo(BL);
        }
      }
    if (mode != RG_command) break;
      StartProgram(Kernel,0,StackCall);
    }
    dpo(ER); dpc(initnames[mode]+3);
  } else {                               /* запуск без дисплея ->     */
    Delay(DelayTime,TRUE);               /* подождем монтажа дисков   */
    mode = RG_start;
  }

  PrmFile = 0;
  if((retcode=_Fopen("$PARAMETERS",'S',FALSE))>=0) {
    PrmFile = (fileid)retcode;
  }

  return(mode);
}

/*
 * Ввод параметров создаваемой базы данных
 */
void InitParams(pFiles,pLocks) table* pFiles; short* pLocks; {
  unsigned newvalue;
  static char AskFiles[] =
   {SP,2,0, SC,0,0, "Max. number of database  files:", SC,FI_HIGHL,0,0};
  static char AskLocks[] =
   {SP,3,0, SC,0,0, "Max. number of modified blocks:", SC,FI_HIGHL,0,0};

  dpc(AskFiles);                        /* читаем число файлов в базе */
  for(;;) {
    while(!_numin(&newvalue,3,10));
  if(newvalue>=5 && newvalue<= *pFiles) break;
    dpo(BL);
  }
  *pFiles = (table) newvalue;

  dpc(AskLocks);                        /* читаем число black-lock-ов */
  for(;;) {
    while(!_numin(&newvalue,4,10));
  if(newvalue>=10 &&
     newvalue<(unsigned)(0x10000/sizeof(struct PD))) break;
    dpo(BL);
  }
  *pLocks = (short) newvalue;
}

void ParmError(code,text) short code; char* text; {
  static char header [] = {HO,CD,CD,SC,FI_HIGHL,0,BL,0};
  static char trailer[] = {LF,SC,FI_HIGHL,0,0};
  if (code>=0) {
    static Fname errors = {"MARS      "};
    GetString(code+1000,errors);
  } else {
    static Fname errors = {NameSysMessage};
    GetString(~code,errors);
  }
  dpc(header); dpc(AZC); dpc(trailer); dpc(text);
  _abort_(ErrParams);
}

                    /* =========================== *\
                    ** =  Macintosh environment  = **
                    \* =========================== */

#elif defined(macintosh)
#include  "CodeDef"
#define   __MYTYPES__
#include  "MacDefines"
#include  <Script.h>

         void       quit(void);
         short      WorkCatalog;       /* WorkDir, из которой запус-ли*/

  extern void       ATclose(void);     /* окончание работы с AppleTalk*/
  extern void       FileEdit(short);   /* редактирование файла        */
  extern void       InitMac (void);

startmode InitSystem(code, argv) int code; char **argv; {
  startmode regime;
  int       retcode;
# pragma    unused(code);
# pragma    unused(argv);

  /* инициируем Mac-овские финтифлюшки */
  InitMac();
  { MachineLocation theLocation;
    ReadLocation(&theLocation);
    theTimeZone = ((long)(theLocation.gmtFlags.gmtDelta & 0xFFFFFF)<<8>>8) / 60;
  }



/*PlayDemo(void);*/


  /* определяем свой каталог */
  {
    short   nfiles,what;
    AppFile theAppFile;
    CountAppFiles(&what,&nfiles);
    if(nfiles==1) GetAppFiles(1,&theAppFile);
    if(nfiles==1 && what == appOpen && theAppFile.fType=='TEXT') {
      WorkCatalog = theAppFile.vRefNum;
      if((retcode = FSOpen(theAppFile.fName,WorkCatalog,&PrmFile))
             < 0) myAbort(retcode);
      ClrAppFiles(1);
    } else {
      Point      where   = {100,100};
      SFTypeList theList = {'TEXT',0,0,0};
      SFReply    theReply;
      SFGetFile(where,"\pConfiguration",NIL,1,theList,NIL,&theReply);
      if(!theReply.good) ExitToShell();
      WorkCatalog = theReply.vRefNum;
      if((retcode = FSOpen(theReply.fName,WorkCatalog,&PrmFile))
             < 0) myAbort(retcode);
      what = -1;
    }

    if(what != appOpen) {                         /* "Manual" режим запуска ? */
      short     theItem;
      DialogPtr thePDialog;
      Rect      theBox;
      short     theType;

      ControlHandle normCtrl,restCtrl,initCtrl;
      thePDialog = GetNewDialog(DparmID,NIL,(WindowPtr)-1);
      GetDItem(thePDialog,DitNormal, &theType,(Handle*)&normCtrl,&theBox);
      GetDItem(thePDialog,DitRestore,&theType,(Handle*)&restCtrl,&theBox);
      GetDItem(thePDialog,DitInit,   &theType,(Handle*)&initCtrl,&theBox);
      SetCtlValue(normCtrl,TRUE);

      regime = RG_start;
      do {
        for(;;) {
          ModalDialog(NIL,&theItem);
        if(theItem == DitStart) break;
          switch(theItem) {
          case DitNormal:                          /* "Normal" regime        */
            SetCtlValue(normCtrl,TRUE);
            SetCtlValue(restCtrl,FALSE);
            SetCtlValue(initCtrl,FALSE);
            regime = RG_start;
          break; case DitInit:                     /* "Init"   regime        */
            SetCtlValue(normCtrl,FALSE);
            SetCtlValue(restCtrl,FALSE);
            SetCtlValue(initCtrl,TRUE);
            regime = RG_init;
          break; case DitRestore:                  /* "Restore" regime       */
            SetCtlValue(normCtrl,FALSE);
            SetCtlValue(restCtrl,TRUE);
            SetCtlValue(initCtrl,FALSE);
            regime = RG_restore;
          break; case DitEdit:                     /* "Edit" command          */
            HideWindow(thePDialog);
            FileEdit(PrmFile);
            ShowWindow(thePDialog);
            SelectWindow(thePDialog);
          break; default:
            FSClose(PrmFile);
            ExitToShell();
          }
        }

/*     { KeyMap theMap;
          GetKeys(&theMap);
          if(theMap[1] & 0x0000100) {
            regime = RG_debug;
            InputDialog = GetNewDialog(DinputID,NIL,(WindowPtr)-1);
            break;
          }
        }
*/
      } while(regime != RG_start &&
              StopAlert(regime == RG_init ? FinitID : FrestID,NIL) != 1);
      DisposDialog(thePDialog);
    } else {
      regime = RG_start;
    }
  }
  OpenTrace();
  return(regime);
}

  /* выход: закрытие всех файлов */
void quit(void) {
  int xfile;

  /* закроем служебные файлы */
  FSClose(UpdFile); FSClose(TempFile); FSClose(SwapFile); FSClose(CodeFile);
  /* закроем файлы таблиц    */
  for(xfile=0; xfile < MaxFiles; ++xfile) {
    if (Files[xfile].status & OldFile) FSClose(FileLabels[xfile]);
  }
  /* закроем AppleTalk */
  ATclose();
  CloseMac();
  ExitToShell();
}

/*
 * Ввод параметров создаваемой базы данных
 */
   static pascal short LimitFilter(DialogPtr,EventRecord*,short*);
   static        long  GetValue   (DialogPtr,short);

static pascal short LimitFilter(theDialog,theEvent,theItem)
     DialogPtr theDialog; EventRecord* theEvent; short* theItem; {
   short  retcode = FALSE;
   unchar code;
   switch(theEvent->what) {
   case keyDown: case autoKey:
     code = theEvent->message & 0xFF;
     if((theEvent->modifiers & cmdKey) || (code != '\t' && code != '\b' &&
      (code < '0' || code > '9'))) theEvent->what = nullEvent;
   }
   return(retcode);
}

static long GetValue(theDialog,theNum) DialogPtr theDialog; short theNum; {
  Rect   theBox; short theType;
  Handle theItem;
  Str255 theText;
  long   value;
  GetDItem(theDialog,theNum,&theType,&theItem,&theBox);
  GetIText(theItem,theText);
  StringToNum(theText,&value);
  return(value);
}

void InitParams(pFiles, pLocks) table* pFiles; short* pLocks; {
  DialogPtr     theDialog = GetNewDialog(DlimitsID,NIL,(WindowPtr)-1);
  Rect          theBox; short theType;
  ControlHandle theOK;
  short         theItem;
  boolean       enable = TRUE;
  GetDItem(theDialog,LitOK,&theType,(Handle*)&theOK,&theBox);

  for(;;) {
    long value;
    boolean newenable = TRUE;
    ModalDialog(LimitFilter,&theItem);
    if(theItem == LitQuit) quit();
  if(theItem == LitOK) break;
    newenable &= (value = GetValue(theDialog,LitFiles)) >= 2  && value <= *pFiles;
    newenable &= (value = GetValue(theDialog,LitLocks)) >= 10 && value <= *pLocks;
    if(enable != newenable) {
      HiliteControl(theOK,(enable = newenable) ? 0x00 : 0xFF);
    }
  }
  *pFiles = GetValue(theDialog,LitFiles);
  *pLocks = GetValue(theDialog,LitLocks);
  DisposDialog(theDialog);
}

/*
 * Выдача сообщений об ошибках в файле параметров
 */
void ParmError(code,line) short code; char* line; {
  Str255 errText;
  if(code < 0) {
    StringHandle sysMess = GetString(-code);
    if(sysMess != NIL) {
      memmove(errText,*sysMess,**sysMess+1);
    } else {
      NumToString((long)code,errText);
    }
  } else {
    GetIndString(errText,ParmMessID,code);
  }
  ParamText(C2P(line),errText,NIL,NIL);
  Alert(ParmErrID,NIL);
  quit();
}

                       /* ======================= *\
                       ** =  MSDOS environment  = **
                       \* ======================= */

#elif defined(MSDOS)

#include <windows.h>
#include <setjmp.h>

#define CacheSize 0xff00

  HANDLE    hCache;
  char far *lpCache;

  extern HWND         hTableWnd;
  extern HANDLE       hInst;
  extern jmp_buf      initial;
  BOOL FAR PASCAL     RegimeDlgProc(int, int, int, long);
  extern jmp_buf      abortjump;

startmode InitSystem(code, argv) int code; char **argv; {
  startmode mode   = RG_start;
  int       retcode;

  NbCacheBuf = (unsigned)CacheSize/BlockSize;


  hCache  = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT,
                (unsigned long)NbCacheBuf*BlockSize);
  if ( hCache==NULL )
      longjmp(abortjump,-1);

  lpCache = GlobalLock(hCache);

  switch (code) {
  case SW_MINIMIZE:
  case SW_SHOWMINIMIZED:
  case SW_SHOWMINNOACTIVE:
    break;
  default:
    if ( !IsIconic(hTableWnd) ) {
    FARPROC lpRegimeDlg;
      lpRegimeDlg = MakeProcInstance(RegimeDlgProc, hInst);
      mode = DialogBox(hInst, "Regime", hTableWnd, lpRegimeDlg);
      FreeProcInstance(lpRegimeDlg);
    }
  }

  return(mode);
}

		       /* ====================== *\
		       ** =  UNIX environment  = **
		       \* ====================== */

#elif defined(unix) || defined(__APPLE__)
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

extern int errno;

short WorkCatalog;                      /* WorkDir, из которой запус-ли */
char *WorkCatName = 0;                  /* полное имя WorkDir */
static int autoConfirmFlag = 0;         /* не спрашивать подтверждение */
static table paramFiles;                /* флаг -files */
static short paramLocks;                /* флаг -locks */

static char Ident[] = "MultiAccess Relational data System server\n\
Prereleased, unsupported version\n\
Copyright (C) 1992, 1993 Vladimir Butenko\n\
Portions Copyright (C) 1993 Serge Vakulenko\n";

/*
 * Запрос подтверждения да/нет.
 * Если нет, то останов.
 */
static void AskUser (char *msg) {
	int yes = -1;
	char reply [80];

	if (autoConfirmFlag)
		return;
	if (isatty (0)) {
		printf ("\n%s\n", msg);
		do {
			printf ("Do you really want to proceed? (yes/no) ");
			fflush (stdout);
			if (! fgets (reply, sizeof (reply), stdin))
				break;
			if (*reply=='y' || *reply=='Y')
				yes = 1;
			else if (*reply=='n' || *reply=='N')
				yes = 0;
		} while (yes < 0);
	}
	if (yes == 1) {
		printf ("\nConfirmed.\n");
		return;
	}
	printf ("\nNot confirmed, server halted.\n");
	exit (-1);
}

static void Usage (char *progname) {
  printf ("%s\nUsage:\n", Ident);
  printf ("\t%s [+] [+io] [+code] [+sort] [+output] [+input] [+swap] [+locks]\n",
    progname);
  printf ("\t\t[+cache] [-init | -restore | -debug] [-yes]\n");
  printf ("\t\t[-files #] [-locks #] [workdir]\n");
  printf ("Options:\n");
  printf ("\t-init\t\tinitialize database\n");
  printf ("\t-restore\trestore database from archive\n");
  printf ("\t-debug\t\tstart in debug mode\n");
  printf ("\t-yes\t\tconfirm action\n");
  printf ("\t-nodaemon\tdon't fork\n");
  printf ("\t-files #\tset the number of files\n");
  printf ("\t-locks #\tset the number of locks\n");
  printf ("\t+\t\ttrace all\n");
  printf ("\t+io\t\ttrace file input/output\n");
  printf ("\t+code\t\ttrace code compiler\n");
  printf ("\t+sort\t\ttrace sorting and merging\n");
  printf ("\t+output\t\ttrace replies to agents\n");
  printf ("\t+input\t\ttrace queries from agents\n");
  printf ("\t+swap\t\ttrace memory allocation\n");
  printf ("\t+locks\t\ttrace locking/unlocking\n");
  printf ("\t+cache\t\ttrace caching\n");
  printf ("\n");
  printf ("Workdir specifies tne name of working directory.\n");
  exit (-1);
}

static void quit () {
  if (TraceFlags)
    printf ("MARS: signal catched, exiting\n");
  myAbort (0);
}

/*
 * Разбор строки параметров, установка режима, флагов,
 * рабочего каталога, файла параметров.
 *
 * Вызов:
 *      server [+] [+io] [+code] [+sort] [+output] [+input] [+swap] [+locks]
 *              [+cache]  [-init | -restore | -debug] [-yes]
 *              [-files #] [-locks #] [workdir]
 * Флаги:
 *      -init           режим инициализации
 *      -restore        режим восстановления
 *      -debug          отладочный режим
 *      -yes            не спрашивать подтверждение
 *      -nodaemon       не ветвиться
 *      -files #        установка количества файлов
 *      -locks #        установка количества блокировок
 *      +               трассировка всего
 *      +io             трассировка ввода/вывода файлов
 *      +code           трассировка машинного кода запросов
 *      +sort           трассировка сортировки, слияния
 *      +output         трассировка ответов пользователю
 *      +input          трассировка запросов от пользователя
 *      +swap           трассировка памяти под коды запросов
 *      +locks          трассировка блокировки/разблокировки
 *      +cache          трассировка обращений к Cache
 *
 * Параметр workdir задает имя рабочего каталога.
 * По умолчанию /usr/lib/mars или /usr/local/lib/mars.
 * В этом каталоге открываем файл parameters, и все
 * остальные параметры считываем оттуда.
 */
startmode InitSystem (int argc, char **argv) {
  startmode mode = RG_start;
  char *progname;
  char buf [512];
  struct timezone tz;
  int forkFlag = 1;

  progname = strrchr (*argv, '/');
  if (progname)
    ++progname;
  else
    progname = *argv;

  /*
   * Определение режима запуска,
   * установка режимов трассировки,
   * отмена запроса подтверждения.
   */
  TraceFlags = 0;
  for (--argc, ++argv; argc>0 && (**argv=='-' || **argv=='+'); --argc, ++argv) {
    if (! strcmp (*argv, "-init"))         mode = RG_init;
    else if (! strcmp (*argv, "-restore")) mode = RG_restore;
    else if (! strcmp (*argv, "-debug"))   mode = RG_debug;
    else if (! strcmp (*argv, "-yes"))     autoConfirmFlag = 1;
    else if (! strcmp (*argv, "-nodaemon")) forkFlag = 0;
    else if (! strcmp (*argv, "-files")) {
	--argc, ++argv;
	if (argc <= 0)
		Usage (progname);
	paramFiles = strtol (*argv, (char**)0, 0);
    } else if (! strcmp (*argv, "-locks")) {
	--argc, ++argv;
	if (argc <= 0)
		Usage (progname);
	paramLocks = strtol (*argv, (char**)0, 0);
    } else if (! strcmp (*argv, "+"))     TraceFlags = ~0;
    else if (! strcmp (*argv, "+io"))     TraceFlags |= TraceIO;
    else if (! strcmp (*argv, "+code"))   TraceFlags |= TraceCode;
    else if (! strcmp (*argv, "+sort"))   TraceFlags |= TraceSort;
    else if (! strcmp (*argv, "+output")) TraceFlags |= TraceOutput;
    else if (! strcmp (*argv, "+input"))  TraceFlags |= TraceInput;
    else if (! strcmp (*argv, "+swap"))   TraceFlags |= TraceSwap;
    else if (! strcmp (*argv, "+locks"))  TraceFlags |= TraceLocks;
    else if (! strcmp (*argv, "+cache"))  TraceFlags |= TraceCache;
    else Usage (progname);
  }
  if (argc > 1)
    Usage (progname);

  /* определение WorkCatName */
  if (argc > 0)
    WorkCatName = *argv;

  switch (mode) {
  case RG_init:
    printf ("%s\nInitializing database\n", Ident);
    AskUser ("ALL DATA IN DATABASE WILL BE LOST!");
    break;
  case RG_debug:
    printf ("%s\nStarting in debug mode\n", Ident);
    AskUser ("This action could damage your database.");
    break;
  case RG_restore:
    printf ("%s\nRestoring database\n", Ident);
    AskUser ("This action is dangerous.");
    break;
  default:
    if (TraceFlags)
      printf ("%s\n", Ident);
    break;
  }

  if (! WorkCatName)
    WorkCatName = "/usr/lib/mars";
  if (access (WorkCatName, 0) < 0)
    WorkCatName = "/usr/local/lib/mars";

  WorkCatalog = open (WorkCatName, 0);
  if (WorkCatalog < 0 || chdir (WorkCatName) < 0) {
    printf ("MARS: %s: %s\n", WorkCatName, strerror (errno));
    exit (-1);
  }

  if (forkFlag) {
    int pid = fork ();
    if (pid < 0) {
      printf ("%s: fork: %s\n", WorkCatName, strerror (errno));
      exit (-1);
    }
    /* родитель -- возврат */
    if (pid)
      exit (0);
    /* потомок */
    /* перехватываем фатальные сигналы */
    signal (SIGINT, quit);
    signal (SIGQUIT, quit);
    signal (SIGTERM, quit);
    signal (SIGPIPE, SIG_IGN);
    /* отцепляемся от терминала */
    setpgid (0, getpid ());
  }

  strcpy (buf, WorkCatName);
  strcat (buf, "/parameters");
  PrmFile = open (buf, 0);
  if (PrmFile < 0)
    PrmFile = 0;

  /* смещение на восток в минутах от Гринвича */
  gettimeofday ((struct timeval*) 0, &tz);
  theTimeZone = - tz.tz_minuteswest;

  /* Всем разрешаем писать в создаваемые нами файлы (для fifo) */
  umask (0);

  return (mode);
}

/*
 * Установка количества файлов и блокировок
 */
void InitParams (table *pFiles, short *pLocks) {
  if (paramFiles)
    *pFiles = paramFiles;
  if (paramLocks)
    *pLocks = paramLocks;
  if (TraceFlags)
    printf ("MARS: files=%d, locks=%d\n", *pFiles, *pLocks);
}

/*
 * Выдача сообщений об ошибках в файле параметров
 */
void ParmError (short code, char* line) {
  if (code < 0 && *line=='/')
    printf ("MARS: %s: %s\n", line, strerror (errno));
  else if (code < 0)
    printf ("MARS: %s/%s: %s\n", WorkCatName, line, strerror (errno));
  else
    printf ("MARS: parameters error %d in line '%s'\n", code, line);
  exit (-1);
}

#ifdef NO_MEMMOVE
/*
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 * sizeof(word) MUST BE A POWER OF TWO
 * SO THAT wmask BELOW IS ALL ONES
 */
typedef	int word;		/* "word" used for optimal copy speed */

#define	wsize	sizeof(word)
#define	wmask	(wsize - 1)

/*
 * Copy a block of memory, handling overlap.
 * This is the routine that actually implements
 * (the portable versions of) bcopy, memcpy, and memmove.
 */
/*
 * Macros: loop-t-times; and loop-t-times, t>0
 */
#define	TLOOP(s) if (t) TLOOP1(s)
#define	TLOOP1(s) do { s; } while (--t)

void *memmove (void *dst0, const void *src0, register unsigned length) {
  register char *dst = dst0;
  register const char *src = src0;
  register unsigned t;

  if (length == 0 || dst == src)          /* nothing to do */
    return (dst0);

  if ((unsigned long)dst < (unsigned long)src) {
    /*
     * Copy forward.
     */
    t = (int)src;   /* only need low bits */
    if ((t | (int)dst) & wmask) {
      /*
       * Try to align operands.  This cannot be done
       * unless the low bits match.
       */
      if ((t ^ (int)dst) & wmask || length < wsize)
	t = length;
      else
	t = wsize - (t & wmask);
      length -= t;
      TLOOP1(*dst++ = *src++);
    }
    /*
     * Copy whole words, then mop up any trailing bytes.
     */
    t = length / wsize;
    TLOOP(*(word *)dst = *(word *)src; src += wsize; dst += wsize);
    t = length & wmask;
    TLOOP(*dst++ = *src++);
  } else {
    /*
     * Copy backwards.  Otherwise essentially the same.
     * Alignment works as before, except that it takes
     * (t&wmask) bytes to align, not wsize-(t&wmask).
     */
    src += length;
    dst += length;
    t = (int)src;
    if ((t | (int)dst) & wmask) {
      if ((t ^ (int)dst) & wmask || length <= wsize)
	t = length;
      else
	t &= wmask;
      length -= t;
      TLOOP1(*--dst = *--src);
    }
    t = length / wsize;
    TLOOP(src -= wsize; dst -= wsize; *(word *)dst = *(word *)src);
    t = length & wmask;
    TLOOP(*--dst = *--src);
  }
  return (dst0);
}
#endif

#else
#error "System initiation not supported"
#endif
