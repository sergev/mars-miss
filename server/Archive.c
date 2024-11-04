                   /* ============================ *\
                   ** ==         M.A.R.S.       == **
                   ** ==    Инициация системы   == **
                   \* ============================ */

#include "FilesDef"
#include "CacheDef"
#include "AgentDef"
#include "CodeDef"
#include "FSysDef"
#if defined(MISS)
#  include <stdlib.h>
#  define xconst const
#elif defined(__STDC__)
#  define xconst const
#else
#  define xconst
#endif


#define  MaxCatals 10                        /* max. число внешних кат*/
#define  CommSym   '%'

	 void      Archive(char,fileid);     /* архивация базы        */
	 void      Init (int,char**);        /* инициация системы     */
         void      ParmError(short,char*);   /* ошибка в параметрах   */

enum {PerrSyntax = 1, PerrKeyword, PerrManyCatals,
      PerrLowValue, PerrBigValue};

  extern void     Calendar (unshort,short*,short*,short*);

  extern void     OutSyncro(void);           /* вывод SyncBuffer      */
  extern void     CheckWrite(fileid,zptr,page,page*);

  /* обращения к InitSystem */
  extern startmode InitSystem(int,char**);   /* системная инициация   */
  extern void      InitParams(table*,short*);/* запрос параметров иниц*/

  /* обращения к Talk */
  extern void      SetDevice(int);           /* инициация канала связи*/

  /* дисковый ввод/вывод  */
  extern void     DiskIO(fileid,void*,unsigned,unsigned,boolean);
  extern void     SyncFlush(fileid);
  extern int      FileLine(char*,short);

  /* обращения к в/выводу нижнего уровня */
  extern void     DropFile (table);          /* стирание блоков файла */

  extern PD*      HashPrev (PD);             /* поиск предыд.в Hash   */
  extern PD       CreatePD (PD*,table,page); /* создание Page Descr   */
  extern void     ChangePD (PD,PD);          /* корр. ссылок на PD    */
  extern void     RlsePD   (PD);             /* освобождение PD       */

  extern page     ReqBlock (void);           /* запрос блока Update   */
  extern void     RlseBlock(page);           /* освоб. блока Update   */

/*extern zptr     FindCache(table,page,PD,short);/*поиск/загр.в Cache */
  extern void     OutCache (CacheElem);      /* сброс Cache-блока     */

  /* локальные функции */
  static void     Restore  (boolean);        /* восстановление по Arch*/
  static void     InpSyncro(fileid);         /* ввод SyncBuffer       */
  static void     SwapFiles(void);


  static char     archname[] = "Arc%5.10";
  static char      tabname[] = "Table%3.10";

  /* сообщение об инициализации */
  static char      offtime[] =
     "Recovering from archive %3.10, fixed at %2.10:%2.10 %2.10/%2.10/%2.10";

  static fileid    Catals[MaxCatals];        /* метки доп. каталогов  */
  static int       Ncatals = 0;              /* число доп. каталогов  */

#define SyncHead ((Scheck*)SyncBuffer)

enum {
  xTEMPFILE=0,  xSWAPFILE  , xWORK,
  xCODEFILE=xWORK ,
  xUPDATEFILE,
  xARCHIVE  ,  xDATACATL  ,
#ifndef FixWorkCat
  xWORKCATL  ,
#endif
#ifndef FixServName
  xSERVNAME  ,
#endif
#ifndef FixCache
  xCACHESIZE ,
#endif
  xWHITELOCKS,
  xAGENTS,      xTEMPSIZE,   xCODEBUFS
};
#define xFILES (xCODEFILE+1)

void Init(argc, argv) int argc; char **argv; {

  fileret   retcode;
  short     index;
  fileid    oldUpdFile;

  CacheElem cacheptr;
  zptr      cacheadr;


  startmode mode;

  /* стандартные значения */
  NbNewLocks = 200; NbAgents = 4; TempSpace=100;
  NCodeBuf = 2*MaxCodeSize+2;
#ifndef FixCache
  NbCacheBuf = 40;
#endif

  mode     = InitSystem(argc,argv);

  GetMemory(ParseSize,ParseBuf);


/*
 * Обрабатываем файл параметров
 */
  TempFile = SwapFile = CodeFile = ArchCat = 0;

  if (PrmFile != 0) {
    int      length;

    FileLine(NIL,-PrmFile);
    while ((length=FileLine(CommonBuffer,250)) >= 0)
    if(length && *CommonBuffer != CommSym) {
      static char* keywords[] =
            {"TEMPFILE"  ,"SWAPFILE"  ,"CODEFILE"  ,
             "UPDATEFILE",
             "ARCHIVE"   ,"DATACATL"  ,
#ifndef FixWorkCat
             "WORKCATL"  ,
#endif
#ifndef FixServName
             "SERVERNAME",
#endif
#ifndef FixCache
             "CACHESIZE",
#endif
             "WHITELOCKS",
             "AGENTS"    ,"TEMPSIZE"  , "CODEBUFS",
             NIL};
      char* scan = CommonBuffer;
      int   nkey = 0;
      while (*scan == ' ') ++scan;
      for(;;) {
        int   j     = 0;
        char* scan1 = scan;
        if(keywords[nkey] == NIL) ParmError(PerrKeyword,CommonBuffer);
        while(keywords[nkey][j] != 0 &&
              keywords[nkey][j] == UpperCase(*scan1)) {
          ++j; ++scan1;
        }
      if(keywords[nkey][j] == 0) {scan = scan1; break;}
        ++nkey;
      }
      while (*scan == ' ') ++scan;
      if(*scan != '=') ParmError(PerrSyntax,CommonBuffer);
      while (*++scan == ' ');

      switch (nkey) {
      case xTEMPFILE: case xSWAPFILE:
        if((retcode = _Fopen(scan,FWork,TRUE))<0) 
                                         ParmError(retcode,scan);
        *(nkey==xTEMPFILE ? &TempFile : &SwapFile) = (fileid) retcode;
      break; case xCODEFILE:
        if((retcode = _Fopen(scan,FCode,FALSE))<0) 
                                         ParmError(retcode,scan);
        CodeFile = (fileid) retcode;

      break; case xUPDATEFILE:
        if((retcode = _Fopen(scan,FUpdate,mode==RG_init))<0)
                                         ParmError(retcode,scan);
        UpdFile =  (fileid)retcode;

#ifndef FixWorkCat
      break; case xWORKCATL:                   /* имя каталога Work   */
        if((retcode = _Fopen(scan,FCatal,FALSE))<0)
                                         ParmError(retcode,scan);
        WorkCatalog = (fileid) retcode;
#endif
#ifndef FixServName
      break; case xSERVNAME:                   /* имя сервера в сети  */
        if((retcode = SetName(scan)) > 0) retcode = PerrSyntax;
        if(retcode != 0) ParmError(retcode,scan);
#endif
#ifndef FixCache
      break; case xCACHESIZE:                  /* размер Cache (блоки)*/
        if((NbCacheBuf = _convi(scan,100,10))<10)ParmError(PerrLowValue,CommonBuffer);
#  ifdef ZCache
        if(NbCacheBuf>=0x10000/BlockSize) ParmError(PerrBigValue,CommonBuffer);
#  endif
#endif
      break; case xARCHIVE:                    /* имя каталога Archive*/
        if((retcode = _Fopen(scan,FCatal,FALSE))<0)
                                         ParmError(retcode,scan);
        ArchCat = (fileid) retcode;

      break; case xDATACATL:                   /* имя каталога Data   */
        if((retcode = _Fopen(scan,FCatal,FALSE))<0) 
                  ParmError(retcode,scan);
        if(Ncatals>=MaxCatals-1) ParmError(PerrManyCatals,CommonBuffer);
        Catals[Ncatals++] = (fileid) retcode;

      break; case xWHITELOCKS:
        NbNewLocks = (short)Max(_convi(scan,100,10),20);

      break; case xAGENTS:
        NbAgents   = _convi(scan,100,10);
        if(NbAgents > sizeof(long)*8-1) ParmError(PerrBigValue,CommonBuffer);
        if(NbAgents< 1)                 ParmError(PerrLowValue,CommonBuffer);
        if(NbAgents >=NbTempFiles-10)   ParmError(PerrBigValue,CommonBuffer);

      break; case xTEMPSIZE:
        TempSpace  = _convi(scan,100,10)/TempSlice;
        if(TempSpace<10)                ParmError(PerrLowValue,CommonBuffer);

      break; case xCODEBUFS:
        NCodeBuf   = _convi(scan,100,10);
        if(NCodeBuf>=0x7FFF/CdBlockSize)ParmError(PerrBigValue,CommonBuffer);
        if(NCodeBuf< 2*MaxCodeSize+2)   ParmError(PerrLowValue,CommonBuffer);
      }
    }
    FSclose(PrmFile); PrmFile=0;
  }

/*if(NbAgents >=NbTempFiles-10) myAbort(167);*/

  /* рабочий каталог - каталог данных   */
  Catals[Ncatals++] = WorkCatalog;


/*
 * читаем шапку SYNC-файла
 */
  if(UpdFile == 0) {
    if((retcode=_Fopen(UpdFileName,FUpdate,mode==RG_init))<0)
                                            ParmError(retcode,UpdFileName);
    UpdFile = (fileid) retcode;
  }
  oldUpdFile = UpdFile;
  if(mode == RG_init) {
    MaxFiles = BgWrkFiles-1; NbLocks = 4000;
    InitParams(&MaxFiles,&NbLocks);
  } else {
    Scheck header;
    DiskIO(UpdFile,(char*)&header,sizeof(header),0,FALSE);
    MaxFiles = (table) header.nfiles; NbLocks = header.nblacks;
  }


  /* Инициализация Cache-таблицы */
#ifndef FixCache
#  ifdef ZCache
     GetZCache(NbCacheBuf*BlockSize);
#  else
     GetMemory((long)NbCacheBuf*BlockSize,CacheBase);
#  endif
#endif
  GetMemory(sizeof(struct CacheElem)*NbCacheBuf,CacheTable);
  cacheptr = CacheTable;
  cacheadr = CacheBase;
  for (;;) {
    cacheptr->shift    = cacheadr ; cacheadr += BlockSize ;
    cacheptr->table    = 0xFF;
    cacheptr->page     = 0x7777;
    cacheptr->modified = FALSE;
    cacheptr->locking  = NIL;
    cacheptr->prev     =
         (cacheptr != CacheTable ? cacheptr : CacheTable+NbCacheBuf)-1;
  if((cacheptr->next = cacheptr+1) == CacheTable + NbCacheBuf) break;
    ++cacheptr;
  }
  cacheptr->next = NIL;
  First = CacheTable;


  /* распределение памяти под Lock-таблицы */
  SyncSize        =
               sizeof(Scheck)                 +   /* контрольная зап. */
               (MaxFiles * sizeof(file_elem)) +   /* каталог базы     */
               (NbLocks*2 + 15) / 16 * 2      +   /* карта блоков     */
               sizeof(struct PD) * NbLocks    +   /* таблица Lock     */
               sizeof(Scheck);                    /* еще разок контр. */

  UpdShift        = (SyncSize + BlockSize-1) / BlockSize;
  GetMemory(SyncSize + whitesize*NbNewLocks, SyncBuffer);

  Files           = (file_elem*) (SyncBuffer+sizeof(Scheck));
  UpdMap          = (unchar*) (Files+MaxFiles);
  UpdMEnd         = UpdMap + (NbLocks*2+15)/16*2;

  BlackLocks      = (PD) UpdMEnd;
  WhiteLocks      = (PD) ((char*) (BlackLocks + NbLocks) +
                             sizeof(Scheck));
  EndLocks        = (PD) ((char*) WhiteLocks + whitesize*NbNewLocks);


/*
 * Инициация Swap-файла
 */
  if(SwapFile == 0) {
    if((retcode = _Fopen(SwpFileName,FWork,TRUE)) <0) 
                                         ParmError(retcode,SwpFileName);
    SwapFile = (fileid) retcode;
  }
  SwapSize = (MaxCodes*MaxCodeSize*NbAgents+7)/8;
  GetMemory(SwapSize,SwapMap);                 /* память под карту    */
  _fill(SwapMap,SwapSize,0);                   /* все блоки свододны  */

/*
 * Инициируем таблицу кодировок
 */
  {
    int i;
    if (CodeFile == 0 && (retcode = _Fopen(CodFileName,FCode,FALSE)) > 0)
      CodeFile = (fileid) retcode;
    if (CodeFile > 0) {
      /*читаем таблицу раскодир*/
      DiskIO(CodeFile,DecodeTable,0x100,0,FALSE);
      FSclose(CodeFile);
    } else {
      for (i=0; i<sizeof(DecodeTable); ++i)
	DecodeTable[i] = i;
    }
    DecodeTable[0]=0; DecodeTable[' ']=' ';

    _fill(CodeTable,sizeof(CodeTable),BadSym);

    for(i=0;i<sizeof(DecodeTable);++i) CodeTable[DecodeTable[i]]=(char)i;

    CodeTable[0]=0; CodeTable[' ']=' ';
  }

/*
 * ищем ARCHIVE-каталог
 */
  if(ArchCat == 0) {
    if( (retcode= _Fopen(ArchCatName,FCatal,FALSE))<0) 
                                         ParmError(retcode,ArchCatName);
    ArchCat = (fileid) retcode;
  }


/*
 * Инициация блоков кода
 */
  GetMemory(NCodeBuf*CdBlockSize,CodeBuf);
  GetMemory(NCodeBuf*sizeof(*TrnsAddr),TrnsAddr);
  Eblock = CodeBuf+(NCodeBuf-1);
  *(CdBlock*)Eblock = NIL;
  do {
    --Eblock; *(CdBlock*)Eblock = Eblock+1;
  } while (Eblock!=CodeBuf);
  Hblock = NIL;

/*
 * Инициация Temp-файла
 */
  if( TempFile == 0) {
    if((retcode = _Fopen(TmpFileName,FWork,TRUE)) <0)
                                         ParmError(retcode,TmpFileName);
    TempFile  = (fileid) retcode;
  }
  /* Инициализация Temp-таблицы */
  TempSize = (page) (GetFileLength(TempFile)/(BlockSize/SectorSize));
  for(index=0; index<NbTempFiles; ++index) {
    TempHeads[index].owner   = 0xFF;
    TempHeads[index].codeNum = 0x77;
    TempHeads[index].first   = -1;
  }
  GetMemory(TempSpace*sizeof(*TempMap),TempMap);
  for(index=1; index<TempSpace; ++index) TempMap[index-1]=index;
  TempMap[index-1] = -1; TempFree = 0;

/*
 * Создание таблиц меток файлов и каталогов
 */
  GetMemory(MaxFiles*sizeof(fileid),FileLabels);
  GetMemory(MaxFiles*sizeof(fileid),CatlLabels);

/*
 * Инициализация агентов
 */
  GetMemory(NbAgents*sizeof(struct agent),AgentTable);
  for(index=0,Agent=AgentTable; index<NbAgents; ++index, ++Agent) {
    int i;
    Agent->ident       = (unchar) index;
    Agent->status      = reading;
    Agent->waits       = NIL;
    for(i=0; i<MaxCodes; ++i) Agent->codes[i].codesize = 0;
    Agent->queueheader = FALSE;
    /* захватим сразу по 1 файлу для агента (для спасения ParseBuf) */
    TempHeads[index].owner   = Agent->ident;
    TempHeads[index].codeNum = 0xFF;
  }
  Agent = Ready = NIL;

/*
 * Разбираемся с типами инициализации
 */
  if ( mode == RG_init) {                /* чистка - инициализация */
    int i;
    /* расписываем Updates-файл */
    for(i=0;i<UpdShift;++i) DiskZIO(UpdFile,CacheBase,BlockSize,i,TRUE);

    _fill(SyncBuffer,SyncSize,0);        /* проинициализируем Updates */
    ((Scheck*)SyncBuffer)->nfiles  = MaxFiles;
    ((Scheck*)SyncBuffer)->nblacks = NbLocks;
    OutSyncro();                         /* сбросим его на диск       */
  } else {                               /* перезапуск системы        */
    InpSyncro(UpdFile);
  }
  Restore(FALSE);

  if (mode == RG_restore) {              /* восстановление по архивам */
    for (;;) {                           /* цикл по архивным файлам   */
      MVS("Restore   ",10,offtime);

    /* ищем очередной архив */
      retcode=FSfind(Fdump(archname,SyncHead->archcount),
                     ArchCat, FUpdate);
    if (retcode < 0) break;

      UpdFile = (fileid) retcode;        /* берем архивный файл       */
      InpSyncro(UpdFile);Restore(TRUE);  /* восстанавливаем его структ*/
      Archive('L',oldUpdFile);           /* раскидываем его по базе   */
    }
  }


  if (mode == RG_debug) {              /* начинаем отладку ?          */
    if((retcode=_Fopen(DebFileName,FSource,FALSE))<0) retcode = 0;
    SetDevice(retcode);
  } else {                             /* подключаемся к каналу ввода */
    SetDevice(-1);
  }
  Reply("System Ready");
}


/*
 * Копирование &Update файла в архив-каталог
 * опустошение UPDATE-файла, запись в таблицы
 * моды архивации: "A" - создание Arch-файла и модификация файлов базы
 *                 "I" - создание Arch-файла без модификации файлов
 *                 "L" - только модификация файлов ("без архивации")
 */

  static char  readTrace[] = "%p reading (%2.16) %2.16 %4.16 <- %4.16";
  static char writeTrace[] = "%p writing (%2.16) %2.16 %4.16%c-> %4.16";
  static char   archText[] = {"\4arch"};
  static char   copyText[] = {"\4copy"};


void Archive(char mode, fileid oldfile) {

  page     dumpsize = 0;
  int      retcode;
  fileid   dumpfile;

  mode = UpperCase(mode);
  switch(mode) {
  default: return;
  case 'A': case 'L': case 'I': ;
  }
  if (ArchCat == 0) mode = 'L';


  /* Вытесняем все из Cache-буферов */
  { CacheElem cscan;
    for(cscan=First; cscan != NIL ; cscan=cscan->next) {
      OutCache(cscan);                      /* пишем на диск            */
      cscan->locking = NIL;  cscan->modified = FALSE;
      cscan->table   = NoFile; cscan->page = 0x7777;
    }
  }

  if (mode == 'A' || mode == 'I') {       /* заказана архивация       */
    fileid curUpdFile;
    PD     scan;
    OutSyncro();                          /* сбросим текущий Sync-файл*/

    /* создаем файл архивации */
    retcode = FSopen(Fdump(archname,SyncHead->archcount),
                     ArchCat,FUpdate);

    if(retcode < 0) myAbort(retcode);
    dumpfile = (fileid) retcode;

    /* распишем его до UpdShift */
    CheckWrite(dumpfile,First->shift,UpdShift-1,&dumpsize);

    /* Далее преобразовываем файл:     */
    /* расчищаем карту занятых блоков; */
    _fill(UpdMap,UpdMEnd-UpdMap,0);

    /* из всех Black-Lock-ов оставляем только те, что имеют "newpage",*/
    /* их мы копируем в новый файл, подставляя новый адрес в "newpage"*/
    for(scan=BlackLocks+NbLocks; --scan >= BlackLocks;) {
      scan->deltakey = 0;                        /* сбрасываем счетчик*/
      clearlock(scan->locks);                    /* снимаем блокировки*/
      if(scan->newpage == 0) scan->table =NoFile;/* нет NEW-блока     */
      if(scan->table != NoFile) {                /* элемент нужен     */
/*      zptr index = FindCache(scan->table,scan->oldpage, scan, Sltype); */
        zptr index = First->shift;
        if(TraceFlags & TraceIO) {
          Reply(Fdump(readTrace,copyText,
                     (unsigned)((index-CacheBase)/BlockSize),
                     (unsigned)scan->table, (unsigned)scan->oldpage,
                     (unsigned)scan->newpage));
        }
        DiskZIO(UpdFile,index,BlockSize,scan->newpage,FALSE);

        /* пишем файл архивации */
        scan->newpage = ReqBlock();
        if(TraceFlags & TraceIO) {
          Reply(Fdump(writeTrace,copyText,
                     (unsigned)((index-CacheBase)/BlockSize),
                     (unsigned)scan->table, (unsigned)scan->oldpage,
                     (unsigned)' ',(unsigned)scan->newpage));
        }
        CheckWrite(dumpfile,index,scan->newpage,&dumpsize);
      }
    }

    /* сбросим теперь новую карту */
    curUpdFile = UpdFile; UpdFile = dumpfile;
    OutSyncro(); UpdFile = curUpdFile;
    FSsetEOF(dumpfile,(unlong)dumpsize * (BlockSize/SectorSize));

    /* освобождаем файл архивации */
    FSclose(dumpfile);

    /* загружаем вновь исходный Sync-файл */
    InpSyncro(UpdFile);
  }


  /* обновляем файлы базы */
  if (mode == 'A' || mode == 'L') {

    /* сотрем ненужные файлы, создадим файлы нужные */
    short xfile;
    for(xfile = 0 ; xfile < MaxFiles ; ++xfile) {
      file_elem* ptr = Files      + xfile;
      fileid*  pfile = FileLabels + xfile;
      fileid*  pcatl = CatlLabels + xfile;
      if( ptr->status & BsyFile ) {            /* непустой элемент  ? */
        if( ptr->status & DelFile) {           /* файл пора стирать ? */
#if defined(MISS)
          if( (retcode=FSdeleteID(*pfile,*pcatl)) < 0) myAbort(retcode);
          FSclose(*pfile);
#else
          FSclose(*pfile);
          if( (retcode=FSdelete(Fdump(tabname,(unsigned)xfile),*pcatl,FData)) < 0) myAbort(retcode);
#endif
          *pfile=0;
          ptr->status = 0;
                                               /* файл пора создать ? */
        } else if( ! (ptr->status & (OldFile | CreFile)) ) {
          if((retcode =
              FSopen(Fdump(tabname,(unsigned)xfile),WorkCatalog,FData))
                        <0) myAbort(retcode);
          *pfile       = (fileid) retcode;
          *pcatl       = WorkCatalog;
          ptr->size    = 0;
          ptr->status |= OldFile;
        }
      }
    }

    SwapFiles();

    /* пишем модифицированные страницы на место, уберем newpage-блоки*/
/*  for(Hashvalue=0; Hashvalue<HashSize; ++Hashvalue) {
/*    PD next;
/*    for(scan=HashTable[Hashvalue] ; scan != NIL; scan=next) {
/*      next = scan->link;
/*      if (scan<WhiteLocks && scan->newpage != 0) {
/*        PD       new;
/*        page     *oldsize = &Files[scan->table].size;
/*        zptr     index    = FindCache(scan->table,scan->oldpage,
/*                                      scan, Sltype);
/*        int      label    =  FileLabels[scan->table];

/*        /* сбрасываем блок в файл */
/*        while (*oldsize < scan->oldpage) {  /* убрать дырку ?       */
/*          DiskZIO((fileid)label,index,BlockSize,*oldsize,TRUE);
/*          ++*oldsize;
/*        }
/*        *oldsize = Max(*oldsize, scan->oldpage + 1);
/*        DiskZIO((fileid)label,index,BlockSize,scan->oldpage,TRUE);

/*        RlseBlock(scan->newpage);           /* отдаем new-блок      */
/*        scan->newpage = 0;                  /* которого теперь нет  */
/*        --Nnewpages;

/*        if (! Xlocked(scan->locks)) {       /* не блокирован кем-то */
/*          if( Nlocked(scan->locks)) {       /* блок вообще свободен */
/*            RlsePD(scan);
/*          } else if((new=CreatePD(&WhiteFree,/* можно сделать White */
/*                                   scan->table,scan->oldpage))!=NIL) {

/*            * HashPrev(scan) = next;        /* предыдущий в HASH    */
/*            new->locks = scan->locks;       /* копируем блокировки  */
/*            ChangePD(scan,new);             /* меняем в агентах     */
/*            scan->link = BlackFree; BlackFree = scan;
/*          }
/*        }
/*      }
/*    }
/*  }

/*  if (Nnewpages != 0) myAbort(160);
*/
    /* доскидываем все файлы на диск */
    for(xfile = 0 ; xfile < MaxFiles ; ++xfile) {
      file_elem* ptr = Files + xfile;
      if( ptr->status & BsyFile ) {            /* непустой элемент  ? */
        if( ptr->status & OldFile) {           /* файл существует     */
          SyncFlush(FileLabels[xfile]);        /* досбрасываем его    */
        }
      }
    }


    /* фиксируем факт архивации */
    ++((Scheck*)SyncBuffer)->archcount;
    if(oldfile != 0) UpdFile = oldfile;       /* восстановление ?     */
    OutSyncro();

  }

  /* далее чистим Cache ???? */

/*for(cscan=First; cscan != NIL ; cscan=cscan->next) {
    cscan->locking = NIL;  cscan->modified = FALSE;
    cscan->table   = 0xFF; cscan->page = 0x7777;
  } */

  Reply("Archiving done");
}

static short FillSwapPDList(short maxBlocks) {
  short nPDs = 0;
  for(Hashvalue=0; Hashvalue<HashSize; ++Hashvalue) {
    PD scan;
    for(scan=HashTable[Hashvalue] ; scan != NIL; scan=scan->link) {
      if (scan<WhiteLocks && scan->newpage != 0) {
        CacheTable[nPDs].locking = scan;
        if(++nPDs >= maxBlocks) return(nPDs);
      }
    }
  }
  return(nPDs);
}

static int mfar sortArchRead(xconst void* first,xconst void* second) {
  return(CacheTable[*(short*)first ].locking->newpage >=
         CacheTable[*(short*)second].locking->newpage ? 1 : -1);
}

static int mfar sortArchWrite(xconst void* first,xconst void* second) {
  PD pFirst  = CacheTable[*(short*)first].locking;
  PD pSecond = CacheTable[*(short*)second].locking;

  return((pFirst->table == pSecond->table ? 
         pFirst->oldpage >= pSecond->oldpage : pFirst->table >  pSecond->table) ? 1 : -1);
}


static void SwapFiles(void) {
  short*  idBuffer  = (short*)CommonBuffer;
  short   maxBlocks = Min(NbCacheBuf,sizeof(CommonBuffer)/sizeof(short));
  for(;;) {
    short nBlocks = FillSwapPDList(maxBlocks);
    short xBlock;
  if(nBlocks == 0) break;

    /* отсортируем в порядке чтения */
    for(xBlock=0;xBlock<nBlocks;++xBlock) idBuffer[xBlock]=xBlock;
    qsort((void*)idBuffer,nBlocks,sizeof(short),sortArchRead);

    /* теперь читаем эти блоки в cache */
    for(xBlock=0; xBlock<nBlocks;++xBlock) {
      CacheElem pCache = &CacheTable[idBuffer[xBlock]];
      PD        ptr    = pCache->locking;
      if(TraceFlags & TraceIO) {
        Reply(Fdump(readTrace,archText,
                     (unsigned)((pCache->shift-CacheBase)/BlockSize),
                     (unsigned)ptr->table, (unsigned)ptr->oldpage,
                     (unsigned)ptr->newpage));
      }
      DiskZIO(UpdFile,pCache->shift,BlockSize,ptr->newpage,0);
    }
    /* отсортируем в порядке записи */
    qsort((void*)idBuffer,nBlocks,sizeof(short),sortArchWrite);
    
    /* теперь пишем эти блоки в файлы */
    for(xBlock=0; xBlock<nBlocks;++xBlock) {
      CacheElem pCache = &CacheTable[idBuffer[xBlock]];
      PD        ptr    = pCache->locking;

      if(TraceFlags & TraceIO) {
        Reply(Fdump(writeTrace,archText,
                     (unsigned)((pCache->shift-CacheBase)/BlockSize),
                     (unsigned)ptr->table, (unsigned)ptr->oldpage,
                     (unsigned)0,(unsigned)0));
      }
      /* сбрасываем блок в файл */
      CheckWrite((fileid)FileLabels[ptr->table],pCache->shift,
                 ptr->oldpage,&Files[ptr->table].size);

      /* отдаем модифицированные блоки */
      RlseBlock(ptr->newpage);            /* отдаем new-блок      */
      ptr->newpage = 0;                   /* которого теперь нет  */
      --Nnewpages;
      pCache->locking = NIL;
    }
  }

  if (Nnewpages != 0) myAbort(160);

  /* уберем освободившиеся newpage-блоки*/
  for(Hashvalue=0; Hashvalue<HashSize; ++Hashvalue) {
    PD next,scan;
    for(scan=HashTable[Hashvalue] ; scan != NIL; scan=next) {
      next = scan->link;
      if (scan>=WhiteLocks) {
      } else if(scan->newpage != 0) {
        myAbort(177);
      } else if(Xlocked(scan->locks)) {       /* X-блокирован кем-то */
        /* оставляем в black-lock-ах */
      } else if(Nlocked(scan->locks)) {       /* блок вообще свободен*/
        RlsePD(scan);
      } else {                                /* S->перенесем в white*/
        PD new = CreatePD(&WhiteFree,scan->table,scan->oldpage);
        if(new != NIL) {                      /* хватило свободных   */
          * HashPrev(scan) = next;            /* предыдущий в HASH    */
          new->locks = scan->locks;           /* копируем блокировки  */
          ChangePD(scan,new);                 /* меняем в агентах     */
          scan->link = BlackFree; BlackFree = scan;
        }                                            
      }
    }
  }
}

/*
 * Обработка Sync-информации при восстановлении из архива
 */
static void Restore(cont) boolean cont; {
  PD       scan;
  short    index;
  unshort  minute = (unshort)((Timer = SyncHead->timer) % (60*24));

  /* проверим контрольную запись */
  if(_cmps(SyncBuffer+SyncSize-sizeof(Scheck),
                         SyncBuffer,sizeof(Scheck)) != 0) myAbort(161);


  /* выдадим информацию о файле (если это не свежак) */
  if(Timer != 0) {
    short year;
    short month;
    short day;

    Calendar((unshort)((unlong)Timer / (60*24)),&year,&month,&day);
    Reply(Fdump(offtime,SyncHead->archcount,
                          minute/60, minute%60, day,month,year) );
  }

  /* открываем все нужные файлы*/
  for(index=0; index<MaxFiles; ++index) {
    file_elem* ptr = &Files[index];
    int        retcode;
    if(ptr->status & OldFile) {          /* есть файл                 */
      if(cont) {                         /* он должен быть открыт     */
        if(FileLabels[index] == 0) myAbort(165);
      } else {
        int xcatal = 0;
        while ((retcode = FSfind(Fdump(tabname,(unsigned)index),
                                 Catals[xcatal],FData)) <0 &&
               ++xcatal<Ncatals);

        if(retcode<0) {                  /* файл не найден            */
          if(!(ptr->status & DelFile)) myAbort(retcode);

          FileLabels[index] = 0;         /* он и не нужен (был стерт?)*/
          CatlLabels[index] = 0;
          ptr->status = 0;

        } else {
          FileLabels[index] = (fileid) retcode;
          CatlLabels[index] = Catals[xcatal];
        }
      }
    } else {                             /* файла нет                 */
      if(cont) {
        if(FileLabels[index] != 0) myAbort(166);
      } else {
        FileLabels[index] = 0;
      }
    }
  }

  /* Строим HASH-таблицы */
  for(Hashvalue=0;Hashvalue<HashSize;Hashvalue++) {
    HashTable[Hashvalue]=0;                  /* обнулим головы списков*/
  }

  /* восстанавливаем список White-lock-ов (они все пустые) */
  WhiteFree=0;
  for(scan = WhiteLocks ; scan < EndLocks ;
      scan = (PD) ((char*) scan + whitesize) ) {
    scan->link = WhiteFree ; WhiteFree = scan;
  }

  /* восстанавливаем список Black-lock-ов    */
  /* оставляем только тех, что имеют newpage */
  BlackFree = NIL; Nnewpages = 0;
  for(scan=BlackLocks+NbLocks; --scan >= BlackLocks;) {
    page pagenum;
    if(Xlocked(scan->locks)) {                 /* был заблокирован  */
      if ( (pagenum = scan->locks.Xlock.modpage) !=0) {
        RlseBlock(pagenum);                    /* отдаем mod-блок   */
        scan->locks.Xlock.modpage = 0;
      }
    }
    scan->deltakey = 0;                        /* сбрасываем счетчик*/
    clearlock(scan->locks);                    /* снимаем блокировки*/
    if(scan->newpage == 0) scan->table=NoFile; /* нет NEW-блока     */
    if(scan->table == NoFile) {                /* свободный элемент */
      scan->link = BlackFree; BlackFree = scan;/* занесем в список  */
    } else {                                   /* элемент нужен     */
      Hashvalue = HASH(scan->table,scan->oldpage);
      scan->link = HashTable[Hashvalue]; HashTable[Hashvalue] = scan ;
      ++Nnewpages;
    }
  }

  /* Откат всех операций Create/Drop */
  for(index = 0 ; index < MaxFiles ; ++index) {
    file_elem* ptr = &Files[index];
    if(ptr->status & BsyFile) {
      clearlock(ptr->iW); clearlock(ptr->iR); clearlock(ptr->iM);
      if(ptr->status & CreFile) {
        DropFile((table)index);
      } else if(ptr->status & DrpFile) {
        ptr->status &= ~ DrpFile;
      }
    }
  }
}

/*
 * Чтение синхро-информации из файла
 */
static void InpSyncro(fileid file) {
  DiskIO(file,SyncBuffer,SyncSize,0,FALSE);
}

