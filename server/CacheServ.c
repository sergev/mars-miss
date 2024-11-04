                    /* ========================== *\
                    ** ==       M.A.R.S.       == **
                    ** ==  Система в/вывода и  == **
                    ** ==      блокировок      == **
                    \* ========================== */

#define  CacheMain
#include "FilesDef"
#include "AgentDef"
#include "CodeDef"
#include "CacheDef"
#include "DataDef"


  /* блокировка элемента */
         boolean  LockElem(lockblock*,short,boolean);

  /* окончание транзакции */
         void     Commit  (boolean);
         boolean  UpdInfo (int);

  /* обнаружение deadlock-ов */
         boolean  DeadLock(void);

  /* операции с блоками данных */
         zptr     GetPage(page,table,short); /* доступ к блоку        */
         zptr     GetPins(page,table,short); /* доступ к блоку для Ins*/
         zptr     GetMapPage(page,table);    /* загрузка Map(без блок)*/

         void     SetModified(short,short);  /* отметка блока: модиф-н*/
         page     FindUsed(page,table,short);/* поиск для Insert      */

         void     SetTail(zptr,boolean);     /* установ в хвост Cache */

  /* операции с файлами данных      */
         boolean  LockTable(table,short);    /* блокировка таблицы    */
         void     DropFile (table);          /* стирание блоков файла */

  /* операции с временными файлами  */
         table    ReqTemp (unchar);          /* выделение temp-файла  */
         void     RlseTemp(table);           /* освобожд. temp-файла  */
         void     RlseAllTemps(unchar);      /* осв. всех temp-f. кода*/

  /* операции с блоками Update-файла*/
         page     ReqBlock (void);           /* запрос блока Update   */
         void     RlseBlock(page);           /* освоб. блока Update   */

  /* oперации с Page Descriptor-ами */
         PD*      HashPrev (PD);             /* поиск предыд.в Hash   */
         PD       CreatePD (PD*,table,page); /* создание Page Descr   */
         void     ChangePD (PD,PD);          /* корр. ссылок на PD    */
         void     RlsePD   (PD);             /* освобождение PD       */

  /* операции с Cache               */
          zptr     FindCache(table,page,PD,short);/*поиск/загр.в Cache*/
          void     OutCache (CacheElem);        /* сброс Cache-блока  */

          void     CheckWrite(fileid,zptr,page,page*);


  extern void     DiskIO     (fileid,void*,unsigned,unsigned,boolean);

  extern void     Unlock     (lockblock*);   /* разблокировка агентов */
  extern void     ExecError  (int);          /* ошибка с rollback-ом  */
  extern void     ReleaseCode(unshort);      /* освоб. кодов (Commit) */
  extern void     UpdFree    (PD);           /* обновл. room  -- " -- */
  extern void     OutSyncro  (void);         /* сброс синхро-информац.*/


  static lockmask getmask (lockblock*);      /* проверка, кто занял   */

  static void     FreeCache(table);          /* выброс табл. из Cache */
  static zptr     SetHead (CacheElem);       /* установ в гол. Cache  */

  static void     InpCache(CacheElem);       /* чтение блока          */

  static PD       LockPage(table,page,short);/* блокировка блока      */

  static PD       HashLook(table,page);      /* поиск в Hash-таблице  */

/*
 * Принцип блокировки файлов:
 * iR - любое обращение (Xlock, если Lock exclusive или расширение lock)
 *      ставится в Directory, LinkTree, LockTable, Create, GetCatalog,
 *                 Rights, Executor
 * iM - возможны модификации (Xlock, при Lock excl или расширение lock)
 *      попытка захватить его в X-mode при S-lock файла
 *      при S-lock предварительно проверяется чистота iW
 * iW - ненужность white-lock-ов: S-lock при Lock in share,
 *                                X-lock при Lock in exclus, расширении
 *      предварительно проверяется чистота iM (т.е. никто не модиф-ет)
 * +====+====+====+===================================================+
 * | iR | iM | iW |                                                   |
 * +====+====+====+===================================================+
 * | S  | -  | -  | можно читать файл                                 |
 * | S  | S  | -  | можно modify файл                                 |
 * | S  | X  | -  | ??                                                |
 * | S  | -  | S  | Lock in share mode (iM проверен)                  |
 * | S  | S  | S  | Lock in share mode,можно modify файл              |
 * | S  | X  | S  | ??                                                |
 * | S  | -  | X  | ??                                                |
 * | S  | S  | X  | ??                                                |
 * | S  | X  | X  | расширен lock, newmap, modLong: LockTable(Xltype) |
 * | X  | X  | X  | Lock in exclusive mode                            |
 * +====+====+====+===================================================+
 */




/*
 * Загрузка страницы для вставки - проверка, что не заблокирована
 * и не переполнена
 */
zptr GetPins(page pageadr, table tablenum, short length) {
  PD  locking  = LockPage(tablenum,pageadr,Xltype);
  if (locking == (PD) 1) {                   /* заблокировали         */
    Agent->waits = NIL; return((zptr) 1);
  }
  if (Xlocked(locking->locks) &&
      BlockSize - (locking->locks.Xlock.newspace+1)*AlignBuf < length) {
    return((zptr)1);                         /* мы ее уже переполнили */
  }
  return(FindCache(tablenum,pageadr,locking,Xltype));
}

/*
 * Загрузка страницы с блокировкой
 * выход: Z-адрес страницы или 1 -> заблокирована,
 *                             2 -> конец таблицы (только при Scanflag)
 */
zptr GetPage(page pageadr, table tablenum, short locktype) {
   PD        locking;

/*
 * оптимизация (временно упрощена)
 */
/*{ CacheElem ptr    = First;
/*  int       qCount = QuickCount;
/*  while(ptr != NIL && --qCount>=0) {
/*    if(ptr->page == pageadr || ptr->table == tablenum) {
/*      boolean useIt = FALSE;
/*      if(ptr->table >= BgWrkFiles) {
/*        useIt = TRUE;
/*      } else if(ptr->locking != NIL) {
/*      }
/*      break;
/*    }
/*  }
/*}
*/

/*
 * блокировка страницы
 */
   locking=LockPage(tablenum,pageadr,locktype);
   if( locking== (PD) 1) return((zptr)1);     /* заблокировали        */
   if( locking== (PD) 2) return((zptr)2);     /* конец таблицы        */
   return( FindCache(tablenum,pageadr,locking,locktype));

}

/*
 * Загрузка страницы с картой свободного места (без блокировок)
 */
zptr GetMapPage(page pageadr, table tablenum) {
   zptr result;
   PD locking=LockPage(tablenum,pageadr,Sltype);

   if( locking== (PD) 1) return((zptr)1);     /* заблокировали        */
   result = FindCache(tablenum,pageadr,locking,Sltype);

   if(locking!=NIL && !Xlocked(locking->locks)) {/* если S-lock, то   */
     locking->locks.Slock = clrbit(locking->locks.Slock,Agent->ident);
     RlsePD(locking);                         /* пробуем освободить   */
   }

   return(result);
}

/*
 * Поиск блока в CACHE-е, загрузка, если отсутствует
 */
zptr FindCache(table tablenum, page pageadr, PD locking, short locktype) {
   CacheElem  ptr = First;
   while (ptr && (pageadr != ptr->page || tablenum != ptr->table)) {
     ptr=ptr->next;
   }

   if(ptr== NIL) {                           /* блока нет             */
     ptr= First->prev;                       /* адрес самого старого  */
     OutCache(ptr);                          /* вытесняем его         */
     ptr->locking=locking;
     ptr->page   = pageadr; ptr->table = tablenum;
     if(locktype<0) {                        /* новые блоки не читаем */
       ZDATA(ptr->shift,page) = ptr->page;
     } else {                                /* а старые зачтем       */
       InpCache(ptr);
     }
     ptr->modified = FALSE;
     Agent->resources += IOResource;
   } else {
     ptr->locking=locking;
     Agent->resources += CacheResource;
   }

   if( TraceFlags & TraceCache) {            /* протоколирование Cache*/
      Reply(Fdump("Cache (%2.16): %2.16 %4.16 (%c)",
                  (unsigned)((ptr->shift-CacheBase)/BlockSize),
                  (unsigned) ptr->table, (unsigned)ptr->page,
                  (unsigned)(locktype<0 ? 'N' : locktype ? 'X' : 'S')));
   }

   return(SetHead(ptr));
}

/*
 * Отметка текущего блока как модифицированого
 */
void SetModified(short delta, short clastdelta) {
  First->modified = TRUE;

  if(First->table >= BgWrkFiles) return;

  /* а можем ли мы его модифиц-ть?*/
  if(First->locking ==NIL || !Xlocked(First->locking->locks))myAbort(91);

  if (delta) {                        /* занесем delta числа записей  */
    First->locking->locks.Xlock.decrflag = (
      (delta +=
        (1-First->locking->locks.Xlock.decrflag*2) *
         First->locking->deltakey)>=0 ?
        0 :
        ((delta = -delta), 1)
    );
    if(delta >= 0x100) myAbort(213);
    First->locking->deltakey = (unchar) delta;
  }

  if(Files[First->table].kind == tableKind) {      /* это таблица     */
    First->locking->locks.Xlock.newspace =
      (BlockSize - ZDATA(First->shift,data_header).freesize) /
      AlignBuf - 1;
  } else if(Files[First->table].kind == ldataKind) {/* это таблица     */
    ;
  } else if (clastdelta) {                         /* это индекс      */
    short olddelta = First->locking->locks.Xlock.newspace;
    clastdelta += clastdelta + (olddelta & 1 ? 1 - olddelta: olddelta);
    if(clastdelta<0) clastdelta = 1 - clastdelta;
    if(clastdelta>=0x200) myAbort(214);
    First->locking->locks.Xlock.newspace = clastdelta;
  }
}

/*
 * Ведение 2-направленного списка для LRU-алгоритма:
 * вставка в голову списка
 */
static zptr SetHead(ptr) CacheElem ptr; {
  CacheElem last;
  if(ptr != First) {                             /* он не был первым */
    ((ptr->next) ? ptr->next : First)->prev = ptr->prev;
    last = First->prev;                          /* будет перед нами */
    ptr->prev->next=ptr->next;
    ptr->prev=last ; First->prev=ptr;
    ptr->next=First;
    First = ptr;
  }
  return(ptr->shift);
}

/*
 * Установ Cache-буфера в конец LRU
 */
void SetTail(shift,free) zptr shift; boolean free; {
  CacheElem ptr = &CacheTable[(shift-CacheBase)/BlockSize];
  SetHead(ptr);
  First = ptr->next; ptr->next= NIL; ptr->prev->next=ptr;
  if (free) ptr->modified=FALSE;

  if( TraceFlags & TraceCache) {            /* протоколирование Cache*/
    Reply(Fdump("Cache (%2.16): in tail (%c)",
                (unsigned)((shift-CacheBase)/BlockSize),
                (unsigned)(free ? 'C' : 'S') ));
  }
}

/*
 * Удаление блоков рабочего файла
 */
/*
void FreeTemp(tablenum,maxpage,minpage)
          table tablenum; page maxpage, minpage; {
*/
/*short   index;
  for(index=0; index<HashSize; ++index) {
    PD* ptr = &HashTable[index];
    while ( *ptr != 0 ) {
      PD xfree = *ptr;
      if( xfree->oldpage < maxpage && xfree->oldpage >= minpage &&
                  xfree->table == tablenum) {
        if (xfree->locks.Xlock.modpage != 0) {
          RlseBlock( xfree->locks.Xlock.modpage);
        }
        clearlock(xfree->locks);
        *ptr =  xfree->link;
        xfree->link = BlackFree; BlackFree = xfree;
      } else {
        ptr = &(*ptr)->link;
      }
    }
  }
*/
  /* освобождаем из CACHE */
/*
  FreeCache(tablenum,maxpage,minpage);
}
*/

/*
 * Выброс из CASH блоков таблицы с номерами от minpage до maxpage
 */

/* static void FreeCache(tablenum,maxpage,minpage)
                 table tablenum; page maxpage, minpage; {*/

static void FreeCache(table tablenum) {
  CacheElem scan,next;
  for (scan = First; scan != NIL; scan=next) {
    next = scan->next;
/*  if (scan->page < maxpage && scan->page >= minpage &&
                    scan->table == tablenum) { */

    if (scan->table == tablenum) {
      scan->locking = NIL;    scan->modified =FALSE;
      scan->page    = 0x7777; scan->table = 0xFF;
      SetHead(scan);
      First = scan->next; scan->next= NIL; scan->prev->next=scan;

      if( TraceFlags & TraceCache) {        /* протоколирование Cache*/
        Reply(Fdump("Cache (%2.16): clear",
                    (unsigned)((scan->shift-CacheBase)/BlockSize) ));
      }
    }
  }
}


/*
 * Поиск в LOCK-таблице
 */
static PD HashLook(table tablenum, page pageadr) {
  PD scan;
  Hashvalue = HASH(tablenum,pageadr);
  for(scan = HashTable[Hashvalue] ; scan != NIL; scan=scan->link) {
    if(scan->table==tablenum && scan->oldpage==pageadr) return(scan);
  }
  return(NIL);
}

/*
 * Поиск в HASH предыдущего
 */
PD* HashPrev(descr) PD descr; {
  PD* scan;
  Hashvalue = HASH(descr->table,descr->oldpage);
  for(scan= &HashTable[Hashvalue]; *scan != descr;scan= &(*scan)->link);
  return(scan);
}

void CheckWrite(fileid label, zptr bufaddr, page pagenum, page* pSize) {
  while(*pSize < pagenum) {
    DiskZIO(label,bufaddr,BlockSize,*pSize,TRUE);
    ++*pSize;
  }
  DiskZIO(label,bufaddr,BlockSize,pagenum,TRUE);
  if(pagenum >= *pSize) *pSize = pagenum + 1;
}

/*
 * Запись блока
 */
void OutCache(ptr) CacheElem ptr; {

  if(ptr->modified) {                     /* блок действительно мод-ан*/
    zptr   buffaddr = ptr->shift;         /* адрес выводимого блока   */
    page   pageadr;
    fileid label;                         /* метка файла вывода       */

    if(ptr->table >= BgWrkFiles ) {       /* вывод в Temp-файл        */
      short* scan  = &TempHeads[ptr->table-BgWrkFiles].first;
      short  index = ptr->page/TempSlice;
      while(index != 0 && *scan >= 0) {
        scan = TempMap + *scan; --index;
      }

      /* автоматическое расширение файла */
      if(*scan<0) {                       /* блок не распределен      */
        for(;;) {                         /* даем память ему и всем до*/
          if (TempFree<0) ExecError(ErrTempSpace);
          *scan = TempFree;               /* заносим адрес свободного */
          TempFree = TempMap[TempFree]; TempMap[*scan] = -1;
        if(index == 0) break;             /* все,что надо,распределили*/
          --index; scan = TempMap + *scan;
        }
      }
      label   = TempFile;
      pageadr = *scan * TempSlice + ptr->page%TempSlice;

/*    /* избегание "дырок" в TempFile */
/*    while (TempSize < pageadr) {
/*      DiskZIO(TempFile,buffaddr,BlockSize,TempSize,TRUE);
/*      ++TempSize;
/*    }
/*    if(pageadr>=TempSize) TempSize = pageadr + 1;
*/
    } else {                              /* вывод не в рабочий файл  */
      page* mod = &ptr->locking->locks.Xlock.modpage;
      pageadr = (*mod==0 ? (*mod=ReqBlock()) : *mod);
      if( ZDATA(buffaddr,page) != ptr->page) myAbort(51);
      if (! Xlocked(ptr->locking->locks)) myAbort(92);
      label = UpdFile;
    }

    if( TraceFlags & TraceIO) {           /* протоколирование записи */
      Reply(Fdump("writing  (%2.16): %2.16 %4.16 -> %4.16",
                  (unsigned)((ptr->shift-CacheBase)/BlockSize),
                  (unsigned) ptr->table, (unsigned)ptr->page,
                  (unsigned) pageadr));
    }
    if(label == TempFile) {
      CheckWrite(TempFile,buffaddr,pageadr,&TempSize);
    } else {
      DiskZIO(label,buffaddr,BlockSize,pageadr,TRUE);
    }
    ptr->modified = FALSE;
  }
}

/*
 * Чтение блока
 */
static void InpCache(ptr) CacheElem ptr; {
  page   pageadr = ptr->page;
  fileid label;
  if (ptr->table >= BgWrkFiles) {
    short* scan = &TempHeads[ptr->table-BgWrkFiles].first;
    short index = pageadr / TempSlice;
    while (index != 0 && *scan >=0) {--index; scan = TempMap + *scan; }
    if(*scan <0) myAbort(121);
    label   = TempFile;
    pageadr = *scan * TempSlice + pageadr%TempSlice;

  } else {
    PD    descr;
    label = FileLabels[ptr->table];
    if( (descr = ptr->locking) != NIL){ /* есть элемент блокировки    */
      page newpage;
      if(Xlocked(descr->locks) &&       /* блок модифицируется        */
           (newpage=descr->locks.Xlock.modpage) !=0 ) {
        pageadr = newpage;
        label   = UpdFile;
      } else if (descr<WhiteLocks &&    /* уже модифицирован          */
                    (newpage=descr->newpage) !=0 ) {
        pageadr = newpage;
        label   = UpdFile;
      }
    }
  }

  if( TraceFlags & TraceIO) {            /*   протоколирование чтения */
    Reply(Fdump("reading  (%2.16): %2.16 %4.16%c<- %4.16",
                (unsigned) ((ptr->shift-CacheBase)/BlockSize),
                (unsigned) ptr->table,(unsigned)ptr->page,
                (unsigned) (label == UpdFile || label == TempFile ? ' ' : 0),
                (unsigned) pageadr));
  }

  DiskZIO(label,ptr->shift,BlockSize,pageadr,0);
  if( ZDATA(ptr->shift,page) != ptr->page) myAbort(50);
}


/*
 * Commit/Rollback
 */
void Commit(type) boolean type; {
  CacheElem   scan;
  PD         ptr;
  short      index;
  lockblock* lock;
  boolean    modified = FALSE;               /* были изменения в базе */

/*
 * Освобождение всех блоков кода
 */
  for(index=0; index< MaxCodes; ++index) {
    if(Agent->codes[index].codesize != 0) ReleaseCode(index);
  }

/*
 * обработка созданных/стертых об'ектов
 */
 { table theFile;
   for(theFile=0 ; theFile < MaxFiles ; ++theFile) {
     file_elem* pfile = &Files[theFile];
     if( (pfile->status & BsyFile)  &&             /* этот занят    */
          Xlocked(pfile->iR) && (pfile->iR.Xlock.owner == Agent->ident)) {
       if( pfile->status & CreFile) {              /* вновь создан  */
         if(type) {                                /* закрываем его */
           pfile->status &= ~ CreFile;
           modified = TRUE;
         } else {
           /* освободим все занятые им MOD и NEW элементы */
           DropFile(theFile);                      /*откат->свободен*/
         }
       } else if (pfile->status & DrpFile) {       /* стирали файл  */
         if(!type) {
           pfile->status &= ~ DrpFile;
         } else {
           /* освободим все занятые им MOD и NEW элементы */
           DropFile(theFile);                      /* Commit->трем  */
           modified = TRUE;
         }
       }
     }
   }
 }

/*
 * обработка новых длин модифицированных блоков
 */
  if(type) {
    UpdFree(NIL);
    for(index=0; index<HashSize; ++index) {
      for(ptr=HashTable[index] ; ptr != NIL; ptr=ptr->link) {
        lock = &ptr->locks;
        if (Xlocked(*lock)                    &&     /* это X-lock    */
            lock->Xlock.owner == Agent->ident &&     /* и причем наш  */
            ptr->table < BgWrkFiles           &&     /* это не рабочий*/
            Files[ptr->table].kind == tableKind) {
          UpdFree(ptr);                              /* это данные    */
          modified = TRUE;
        }
      }
    }
  }

/*
 * обработка модифицированных нами блоков Cache
 */
  for(scan=First; scan != NIL ; scan=scan->next) {
    if( (ptr=scan->locking) !=NIL &&      /* заблокировано нами       */
         Xlocked(ptr->locks) && ptr->locks.Xlock.owner == Agent->ident){
      if(type && ptr->table < BgWrkFiles) {
        if(scan->modified) OutCache(scan);/* Commit -> пишем на диск  */
        modified = TRUE;
      } else {                            /* Rollback->трем содержимое*/
        scan->locking = NIL; scan->modified = FALSE;
        scan->table   = NoFile; scan->page = 0x7777;
      }
    }
  }

/*
 * обработка страничных LOCK-ов
 */
  modified |= UpdInfo((int)type);

/*
 * сброс блокировок таблиц
 */
  for(index=0;index<MaxFiles;++index) {        /*цикл по всем файлам  */
    if(Files[index].status & BsyFile) {        /* есть файлик         */
           /* снимаем все типы lock-ов   */
      for(lock= &Files[index].iR; lock<=&Files[index].iM; ++lock) {
        if(Xlocked(*lock) && lock->Xlock.owner == Agent->ident) {
          clearlock(*lock); Unlock(lock);      /*освободили X-lock    */
        } else if (tstbit(lock->Slock,Agent->ident)) {
          lock->Slock = clrbit(lock->Slock,Agent->ident);
          Unlock(lock);                        /* освободили S-lock   */
        }
      }
    }
  }

  if(modified) OutSyncro();

  Agent->starttime = -1;
}

/*
 * Обновление каталога: перенос информации из PD
 * При COMMIT (flag >= 0) - освобождение PD
 */
boolean UpdInfo(flag) int flag; {
  int        index;
  PD         ptr;
  lockblock* lock;
  boolean    modified = FALSE;
  for(index=0; index<HashSize; ++index) {
    PD next;
    for(ptr=HashTable[index] ; ptr != NIL; ptr=next) {
      next = ptr ->link;                       /*т.к. может измениться*/
      lock = &ptr->locks;
      if(Xlocked(*lock)) {                     /*обработаем наш X-lock*/
        if(lock->Xlock.owner == Agent->ident) {
          file_elem* modfile;
          if(ptr->table>=BgWrkFiles) myAbort(122);/* Lock на рабочий? */
          modfile = &Files[ptr->table];

          /* COMMIT или Update new ?*/
          if(flag>0 || (flag<0 && (modfile->status & CreFile))) { 
            short delta;                       /*обновим число записей*/
            modified = TRUE;
            if(modfile->recordnum != UniqIndexFlag &&
                          (delta=ptr->deltakey) !=0) {
              if(lock->Xlock.decrflag) delta = -delta;
              modfile->recordnum += delta;
              ptr->deltakey = 0; lock->Xlock.decrflag = 0;
            }
 
            if(modfile->kind == indexKind &&   /*обновим кластериз.   */
                                  (delta=lock->Xlock.newspace) != 0) {
              modfile->b.indexdescr.clustercount +=
                   (delta & 1 ? -(delta >> 1) : delta >> 1 );
              lock->Xlock.newspace = 0;
            }

            /*изменим актуальную длину файла */
            if(ptr->oldpage >= modfile->actualsize) {
              modfile->actualsize = ptr->oldpage + 1;
            }

            if(flag>0 && lock->Xlock.modpage!=0){ /* COMMIT           */
              if(ptr->newpage !=0 ) {          /* если был перенесен  */
                RlseBlock(ptr->newpage);       /* освобождаем старый  */
                ptr->newpage = 0; --Nnewpages;
              }
              ptr->newpage =                   /* заносим новый адрес */
                 lock->Xlock.modpage;          /* перенесенного блока */
              ++Nnewpages;
            }

          } else if(flag==0) {                 /* ROLLBACK            */

            if(lock->Xlock.modpage !=0) {      /* если был выделен    */
              RlseBlock(lock->Xlock.modpage);
              lock->Xlock.modpage = 0;         /* новый, то отдадим   */
            }
            ptr->deltakey = 0;
          }
          if(flag >= 0) {
            clearlock(*lock);
            Unlock(lock);                      /* разблокируем        */
            RlsePD(ptr);                       /* освободим блок      */
          }
        }

      } else if (flag >= 0 && tstbit(lock->Slock,Agent->ident)) {
        lock->Slock = clrbit(lock->Slock,Agent->ident);
        Unlock(lock); RlsePD(ptr);             /* освободим наш S-lock*/
      }
    }
  }
  return(modified);
}


/*
 * Освобождение всех lock-ов на файл с заданным номером (при DROP)
 * и всех занятых им new и mod блоков
 */
void DropFile(table tablenum) {
  short      index;
  PD         ptr;
  lockblock* lock;
  file_elem* pfile = &Files[tablenum];

  for(index=0; index<HashSize; ++index) {
    PD next;
    for(ptr=HashTable[index] ; ptr != NIL; ptr=next) {
      next = ptr ->link;                       /*т.к. может измениться*/
      if (ptr->table == tablenum) {            /*да, это - искомая    */
        lock = &ptr->locks;
        if(Xlocked(*lock) ) {
          if (lock->Xlock.owner != Agent->ident) myAbort(130);
          if(lock->Xlock.modpage !=0) {        /* если был выделен    */
            RlseBlock(lock->Xlock.modpage);
            lock->Xlock.modpage = 0;           /* новый, то отдадим   */
          }
        } else if  (clrbit(lock->Slock,Agent->ident) != 0) {
          myAbort(131);
        }

        if(ptr->newpage !=0) {                 /* если был выделен    */
          RlseBlock(ptr->newpage);
          ptr->newpage = 0; --Nnewpages;       /* новый, то отдадим   */
        }
        clearlock(*lock);                      /* Unlock не делаем    */
        RlsePD(ptr);
      }
    }
  }

  /* освободим ждущих весь файл */
  clearlock(pfile->iR); clearlock(pfile->iW); clearlock(pfile->iM);
  Unlock(&pfile->iR);
  if(pfile->status & OldFile) {                /* был старый файл     */
    pfile->status = ( BsyFile | DelFile | OldFile);
    pfile->b.tablename[0]= '-';
  } else {                                     /* старого не было ->  */
    pfile->status = 0;                         /* элемент свободен    */
  }

  /* выброс файла из списка индексов/ldata */
  if(pfile->mainfile != NoFile) {              /* это не таблица      */
    file_elem* scan = &Files[pfile->mainfile];
    if(scan->status & BsyFile) {               /* not dropped yet     */
      while (scan->nextfile != tablenum) {
        if(scan->nextfile == NoFile) myAbort(132);
        scan = &Files[scan->nextfile];
      }
    }
    scan->nextfile = pfile->nextfile;         /*выбрасываем из списка*/
  }

/*FreeCache(tablenum,MaxPage,0);*/
  FreeCache(tablenum);
}

/*
 * освобождение блока страничных блокировок
 * (если он никем не заблокирован)
 */
void RlsePD(ptr) PD ptr; {
  if(Nlocked(ptr->locks)) {                    /* никому не нужна     */
    PD* prevPD = HashPrev(ptr);                /* предыдущий в HASH   */
    if(ptr>=WhiteLocks) {                      /* более не нужна      */
      *prevPD = ptr->link;                     /* убираем из цепочки  */
      ChangePD(ptr,NIL); ptr->link = WhiteFree ; WhiteFree = ptr;
    } else if (ptr->newpage == 0) {            /* и эта не нужна      */
      *prevPD = ptr->link;                     /* убираем из цепочки  */
      ChangePD(ptr,NIL); ptr->link = BlackFree ; BlackFree = ptr;
    }
  }
}

/*
 * Блокировка элемента
 */
boolean LockElem(lockblock* lock, short locktype, boolean set) {
  short ident=Agent->ident;
  if(Agent->starttime == -1) Agent->starttime = Timer;

  if(Xlocked(*lock)) {                        /* таблица заблокирована*/
    if(lock->Xlock.owner != ident) {          /* и не нами            */
      Agent->waits = lock;
      return(TRUE);
    }
  } else if (locktype) {                      /* X-блокировка         */
    if(clrbit(lock->Slock,ident) !=0) {       /* а кто-то уже работает*/
      Agent->waits = lock;
      return(TRUE);
    }
    if(set) {
      lock->Xlock.Xflag   = 1;
      lock->Xlock.owner   = ident;
      lock->Xlock.modpage = 0;
      lock->Xlock.newspace= 0;
    }
  } else if (! tstbit(lock->Slock,ident))  {  /* мы еще не трогали    */
    agent scan = AgentTable+NbAgents;
    while(--scan>=AgentTable) {
      if(scan->waits== lock) {                /* кто-то уже ждет      */
        Agent->waits = lock; return(TRUE);    /* -> мы тоже будем     */
      }
    }
    if(set) {                                 /* надо ставить признак */
      lock->Slock= setbit(lock->Slock,ident); /* S-блокировки         */
    }
  }
  return(FALSE);
}

/*
 * Блокировка таблицы
 * locktype:
 * Slock  гарантирует от модификации еще не модифицированых страниц
 *        (запрет дальнейших модификаций - мы не храним WHITElock-и)
 * Xlock  то же самое, но нам самим можно модифицировать -
 *        (не храним WHITElock-и, но такие мы одни);
 * Olock - запрещаем другим вообще обращаться к таблице;
 *
 */
boolean LockTable(table tablenum, short locktype) {
  file_elem* fptr = &Files[tablenum];
  PD ptr,next;
  boolean already = tstbit (getmask(&fptr->iW), Agent->ident);

  if(LockElem(&Files[tablenum].iR,locktype==Oltype,TRUE  ) ||
      /* проверим, что еще никто не модифицировал */
     LockElem(&Files[tablenum].iM,Xltype,locktype!=Sltype) ||
     LockElem(&Files[tablenum].iW,locktype!=Sltype,TRUE  ) )
         return(TRUE);

  if( ! already) {                              /* хватаем по новой ? */
    /* освобождаем все White - Lock-и, захваченные нами */
    for(Hashvalue=0; Hashvalue<HashSize; ++Hashvalue) {
      for(ptr=HashTable[Hashvalue] ; ptr != NIL; ptr=next) {
        next = ptr->link;                       /* т.к. может изм-ться*/
        if( ptr >= WhiteLocks    &&             /*освободим наш S-lock*/
            ptr->table==tablenum &&
            tstbit(ptr->locks.Slock,Agent->ident) ){
          ptr->locks.Slock = clrbit(ptr->locks.Slock,Agent->ident);
          Unlock(&ptr->locks); RlsePD(ptr);
        }
      }
    }
  }
  return(FALSE);
}

/*
 * Блокировка страницы
 * locktype: 0 - Shared, 1-eXclusive; 2-eXclusive для FreeMap
 * выход: 1 - locked ; 2 - такой страницы нет
 */
PD LockPage(table tablenum, page pageadr, short locktype) {
  PD        descr;

  short     tlocking;

  if( tablenum >= BgWrkFiles) {
    if(TempHeads[tablenum-BgWrkFiles].owner != Agent->ident)myAbort(121);
    return(NIL);
  }

  /* таблицу мы должны были пощупать */
  if(!tstbit( getmask(&Files[tablenum].iR) ,Agent->ident) ) myAbort(20);

  /* и поставить "intent exclusive", если собираемся модифицировать */
  if(locktype &&
       !tstbit( getmask(&Files[tablenum].iM) ,Agent->ident) ) myAbort(21);

/*
 * Проверка на ненужность White-блоков
 */
  tlocking = tstbit( getmask(&Files[tablenum].iW),  Agent->ident );

/*
 * Ищем страницу в таблице PageLock
 */
  descr=HashLook(tablenum,pageadr);         /* ищем страницу в S-блоке*/
  if(descr &&  Xlocked(descr->locks)) {     /* уже заблокировано      */
    if(descr->locks.Xlock.owner != Agent->ident) {
      Agent->waits = &descr->locks;         /* но не нами             */
      return((PD) 1);
    }
    return(descr);
  }

  if(locktype) {                              /* X-lock               */
    if(descr==NIL) {                          /* надо создавать       */
      if((descr=CreatePD(&BlackFree,tablenum,pageadr))== NIL) {
         ExecError(ErrModSpace);
      }
      descr->newpage=0;
    }
    if(LockElem(&descr->locks,Xltype,TRUE)) return((PD) 1);
    if(descr>=WhiteLocks) {                   /*надо перенести в BLACK*/
      PD  new;
      if((new=CreatePD(&BlackFree,tablenum,pageadr))== NIL) {
        ExecError(ErrModSpace);
      }
      * HashPrev(descr) = descr->link;        /* предыдущий в HASH    */
      descr->link  = new->link;
      MVS((char*)descr,whitesize,(char*)new);
      new->newpage = 0;
      ChangePD(descr,new);                   /* меняем в агентах      */
      descr->link = WhiteFree; WhiteFree = descr; descr=new;
    }
  } else {                                   /* S-lock                */
    if(descr==NIL) {                         /* блочка нет            */
      if(pageadr>=Files[tablenum].size) {    /* за границей файла     */
        if(ScanFlag) return ( (PD) 2) ;      /* флаг конца таблицы    */
        myAbort(97);
      }
      if(tlocking) return(NIL);              /* и не надо             */
      if((descr=CreatePD(&WhiteFree,tablenum,pageadr))==NIL) {
        /* если блочков более нет -> захватываем всю таблицу S-lockом */
        if(LockTable(tablenum,Sltype)) return((PD) 1);
        return(NIL);
      }
    }
    if(LockElem(&descr->locks,Sltype,! tlocking)) return((PD) 1);
  }
  return(descr);
}

/*
 * Создание Page Descriptor-а
 */
PD CreatePD(PD* freelist, table tablenum, page pageadr) {
  PD new;
  if(*freelist==NIL) return(NIL);             /* список свободных пуст*/
  new = *freelist; *freelist = new->link;     /* нет-> берем элемент  */
  Hashvalue=HASH(tablenum,pageadr);
  new->link = HashTable[Hashvalue];           /* заносим в HASH-список*/
  HashTable[Hashvalue] = new;
  clearlock(new->locks);
  new->oldpage     = pageadr;
  new->table       = tablenum;
  new->deltakey    = 0;
  return(new);
}

/*
 * Коррекция ссылок на PD при переносе PD
 */
void ChangePD(old,new) PD old; PD new; {
  CacheElem scan1;
  agent     scan = AgentTable+NbAgents;
  while(--scan >= AgentTable) {
    if(scan->waits == &old->locks) scan->waits = &new->locks;
  }
  forlist(scan1,First) {
    if(scan1->locking == old) scan1->locking = new;
  }
}

/*
 * Выделение свободного блока (0-го блока не существует)
 */
page ReqBlock() {
  unchar* ptr;
  short i; unsigned mask;
  for(ptr=UpdMap; *ptr== 0xFF; ++ptr) if(ptr>=UpdMEnd) myAbort(95);
  for(i=0, mask= 0x80 ; *ptr & mask ; mask = mask>>1 , i++);
  *ptr |= (unchar) mask;
  return((page) ((ptr-UpdMap) * 8 + i) + UpdShift);
}

/*
 * освобождение блока
 */
void RlseBlock(page pageadr) {
  unchar* ptr = &UpdMap[(pageadr -= UpdShift)/8];
  unsigned mask = 0x80 >> (pageadr % 8);
  if((*ptr & mask) == 0) myAbort(96);        /* блок и так не занят    */
  *ptr &= (unchar) ~mask;
}

/*
 * Поиск блока с подходящим свободным местом среди захваченных нами
 */
page FindUsed(page bestpage, table tablenum, short length) {
  PD       ptr;
  page     found = 0;
  short    delta = 0x7FFF;
  for(Hashvalue=0; Hashvalue<HashSize; ++Hashvalue) {
    for(ptr=HashTable[Hashvalue] ; ptr != NIL; ptr=ptr->link) {
      if( ptr->table==tablenum                    &&
          Xlocked(ptr->locks)                     &&
          ptr->locks.Xlock.owner == Agent->ident  &&
          ptr->oldpage != (page) 1                &&
          ptr->oldpage % FreeMapStep != 0         &&
          BlockSize - (ptr->locks.Xlock.newspace+1)*AlignBuf>= length) {
        short delta1;
        if((delta1=ptr->oldpage - bestpage)<0) delta1 = -delta1;
        if(delta1<delta) {                      /* поближе к заданному*/
          delta = delta1; found = ptr->oldpage;
        }
      }
    }
  }
  return(found);
}

static lockmask getmask(locks) lockblock* locks; {
  return((Xlocked(*locks) ?
     setbit((lockmask) 0 ,locks->Xlock.owner) : locks->Slock)
  );
}

/*
 * обнаружение DEADLOCK-a
 */
boolean DeadLock() {
  lockmask new = clrbit(getmask(Agent->waits),Agent->ident), old;
  short i; agent scan;
  do {
    old = new;
    for(i=0,scan=AgentTable; i<NbAgents; i++,scan++) {
     if(scan != Agent && tstbit(new,i) && scan->status==locked)
       new |= getmask(scan->waits);
    }
  } while (new != old);
  return(tstbit(new,Agent->ident));
}

/*
 * Распределение temp-файлов: выделение файла
 */
table ReqTemp(unchar codeNum) {
  short i;
  for(i=0 ; i<NbTempFiles ; i++ ) {     /* цикл по усем temp-файлам   */
    if(TempHeads[i].owner == 0xFF) {    /* пока не найдем свободный   */
      TempHeads[i].owner  =Agent->ident;/* тогда его займем           */
      TempHeads[i].codeNum=codeNum;
      TempHeads[i].first  = -1;         /* блоков нет                 */
      return((table)(BgWrkFiles+i));    /* и вернем номер             */
    }
  }
  return(0);
}

/*
 * Распределение temp-файлов: освобождение файла
 */
void RlseTemp(table index) {
  short scan,free;
  if( index<BgWrkFiles || index >=BgWrkFiles+NbTempFiles ||
      TempHeads[index-BgWrkFiles].owner != Agent->ident) myAbort(139);

  /* выбрасываем файл из Cache */
  FreeCache(index);

  /* отдаем блоки Temp-файла в список свободных */
  scan = TempHeads[index-BgWrkFiles].first;
  while ((free = scan) >= 0) {
    scan = TempMap[free]; TempMap[free] = TempFree;
    TempFree = free;
  }
  TempHeads[index-BgWrkFiles].owner   = 0xFF;
  TempHeads[index-BgWrkFiles].codeNum = 0x77;
  TempHeads[index-BgWrkFiles].first   = -1;
}

/*
 * Освобождение всех temp-файлов данного агента, использованных
 * в коде с заданным номером
 */
void RlseAllTemps(unchar codeNum) {
  short index;
  for(index = 0; index < NbTempFiles; ++index) {
    if(TempHeads[index].owner   == Agent->ident &&
       TempHeads[index].codeNum == codeNum) RlseTemp((table)(index+BgWrkFiles));
  }
}
