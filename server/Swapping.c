                   /* ============================= *\
                   ** ==         M.A.R.S.        == **
                   ** == Управление памятью  для == **
                   ** ==  сгенерированных кодов  == **
                   \* ============================= */

#include "FilesDef"
#include "AgentDef"
#include "CodeDef"
#include "ExecDef"

         CdBlockH LoadCode(codeaddress*,CdBlockH);
         void     GetSpace(short,CdBlockH);  /* выделение памяти      */
         codeaddress*                        /* генерация номера кода */
                  GenCdNum(unchar*,unchar*);
         void     LockCode(table,int,boolean);
         void     ReleaseCode(unshort);

  extern void     ExecError(int);            /* сообщение об ошибке   */
  extern void     Show     (char);           /* вывод табл. на консоль*/

  /* обращения к в/выводу нижнего уровня*/
  extern void     RlseTemp(table);           /*освобождение temp-файла*/
  extern void     RlseAllTemps(unchar);      /* осв. всех temp-f. кода*/
  extern void     DiskIO   (fileid,void*,unsigned,unsigned,boolean);

  static void     SwapOut  (CdBlockH);
  static void     ToLinear (CdBlock);
  static void     Linear   (Caddr* );

/*
 * Выбрасывание кодов (кроме текущего) для получения
 * "nblocks" свободных блоков
 */
void GetSpace(short nblocks, CdBlockH memlock) {
  CdBlockH   cptr,xptr;
  short      count;
  CdBlock    empty;
  for (;;) {
    count =0;                          /* подсчитаем число своб.блоков*/
    for(empty=Eblock; empty != 0; empty = *(CdBlock*)empty) ++count;
  if (count>=nblocks) break;           /* число свободных достаточно  */
    xptr = NIL;                        /* ищем, кого бы выкинуть      */
    for (cptr = Hblock; cptr != NIL; cptr = cptr->nextloaded) {
/*    if(cptr != locked && AgentTable[cptr->owner].status == locked){ */
      if(cptr != memlock) {
        xptr = cptr;
      }
    }
    if (xptr == NIL) myAbort(152);     /* выкидывать некого !         */
    SwapOut(xptr);                     /* выкидываем найденный код    */
  }
}

/*
 * Выброс кода в SWAP.
 * адреса блоков (PageMap) заменяются на дисковые адреса блоков
 * адреса в коде заменяются на линейные адреса
 */
static void SwapOut(ABlock) CdBlockH ABlock; {
  int          xpage;
  short        diskaddr;
  CdBlockH*    hscan;
  codeaddress* codeaddr =
                  AgentTable[ABlock->owner].codes + ABlock->codenum;
  unsigned     ptr = 0;

  /* формируем карту для преобразования в линейные адреса */
  for(xpage = 0; xpage < NCodeBuf; ++xpage) TrnsAddr[xpage] = 1;

  for(xpage = codeaddr->codesize; --xpage>=0;) {  /* разности для лин.*/
    TrnsAddr[ABlock->PageMap[xpage]-CodeBuf] =
        (char*)(xpage + CodeBuf) - (char*) ABlock->PageMap[xpage];
  }

  /* сбрасываем блоки в файл */
  for(xpage = codeaddr->codesize; --xpage>=0;) {
    unsigned bitnum  = 0;
    CdBlock  bufaddr = ABlock->PageMap[xpage];

    /* формируем дисковый адрес блока */
    while (SwapMap[ptr] == 0xFF) if (++ptr>= SwapSize) myAbort(155);
    while (SwapMap[ptr] & (0x80>>bitnum)) ++bitnum;

    SwapMap[ptr] |= (unchar) 0x80>>bitnum;
    diskaddr  = ptr*8+bitnum;

    /* выводим трассировочную информацию */
    if (TraceFlags & TraceSwap) {
      Reply(Fdump("SwapOut code=%2.16(%1.16), mem=%2.16, disk=%3.16",
                  (unsigned)ABlock->owner, (unsigned)ABlock->codenum,
                  (unsigned)(bufaddr - CodeBuf), (unsigned) diskaddr));
    }

    ToLinear(bufaddr);

    /* пишем блок на диск */
    DiskIO(SwapFile,(char*)bufaddr,CdBlockSize,diskaddr,TRUE);

    /* проставляем дисковый адрес в PageMap */
    ABlock->PageMap[xpage] = (CdBlock) diskaddr;

    /* освобождаем занятую память */
    *(CdBlock*)bufaddr = Eblock; Eblock = (CdBlock) bufaddr;
  }

  /* пометим код как вынесенный в таблице агента */
  codeaddr->outside = TRUE; codeaddr->address = diskaddr;

  /* выбросим код из списка загруженных */
  for(hscan= &Hblock; *hscan != ABlock; hscan = &(*hscan)->nextloaded) {
    if (*hscan == NIL) myAbort(151);
  }
  *hscan = (*hscan)->nextloaded;
}

/*
 * Преобразование адресов
 */

static void ToLinear(bufaddr) CdBlock bufaddr; {
  Mcode* till = (Mcode*)((char*)bufaddr + sizeof(struct head0));
  Mcode* scan;

  /* переводим адреса в линейные */
  switch(bufaddr->head.blocktype) {
  case CdDirect:
    till = (Mcode*) ((char*)bufaddr + ((CdBlockH)bufaddr)->execentry);
    {
      Caddr* hscan = (Caddr*)(&((CdBlockH)bufaddr)->ptrs + 1);
      while(--hscan>=(Caddr*) &((CdBlockH)bufaddr)->ptrs) Linear(hscan);
    }
  case CdCode  :
    scan = (Mcode*)
        ((char*)(bufaddr+1) - bufaddr->head.freespace);
    while(--scan>=till) Linear(&scan->addr);
  }
}

static void Linear(ptr) Caddr* ptr; {
  if(! IsZaddr(*ptr) ) {
    short thePage =
#ifdef CdShort
      (*ptr - CdShift) / CdBlockSize
#else
      (CdBlock) *ptr - CodeBuf
#endif
      ;
    if(thePage >= NCodeBuf || TrnsAddr[thePage] == 1) myAbort(156);
    *ptr += TrnsAddr[thePage];
  }
}


/*
 * Загрузка кода: code - дисковый адрес
 */
CdBlockH LoadCode(addr,memlock) codeaddress* addr; CdBlockH memlock; {
  CdBlockH ABlock;
  if(addr->outside) {                     /* код действительно в swap */
    int     xpage;
    CdBlock bufaddr;
    short   diskaddr = addr->address;

    short   agentnum = ((char*)addr - (char*) AgentTable) /
                                                  sizeof(struct agent);
    short   codenum  = addr - AgentTable[agentnum].codes;

    GetSpace(addr->codesize,memlock);     /* выделили память под код  */

    /* формируем карту для преобразования в линейные адреса */
    for(xpage = 0; xpage < NCodeBuf; ++xpage) TrnsAddr[xpage] = 1;

    for(xpage=0;;) {                      /* цикл загрузки блоков     */
      if((bufaddr = Eblock) == NIL) myAbort(153);
      Eblock = *(CdBlock*)Eblock;
      if (TraceFlags & TraceSwap) {       /* трассировочные выдачи    */
        Reply(Fdump("SwapInp code=%2.16(%1.16), mem=%2.16, disk=%3.16",
                    (unsigned)agentnum,(unsigned)codenum,
                    (unsigned)(bufaddr-CodeBuf), (unsigned) diskaddr));
      }

      /*закачиваем блок в память   */
      DiskIO(SwapFile, (char*)bufaddr, CdBlockSize, diskaddr, FALSE);

      /* освободим блок SWAP-файла */
      SwapMap[diskaddr/8] &= (unchar) ~(0x80>>(diskaddr%8));

      if (xpage==0) {                    /* загрузили нулевой блок    */
        ABlock = (CdBlockH) bufaddr;
        if(ABlock->owner != agentnum || ABlock->codenum != codenum)
                                                           myAbort(154);
      }

      /* адрес блока -> в карту кода */
      ABlock->PageMap[xpage] = bufaddr;
      TrnsAddr[xpage] =
           (char*) bufaddr - (char*) (xpage + CodeBuf);

    if(++xpage >= addr->codesize) break;

      /* дисковый адрес следующего блока */
      diskaddr = (short) ABlock->PageMap[xpage];
    }

    /* включим в список кодов         */
    ABlock->nextloaded = Hblock; Hblock = ABlock;

    /* пометим "загруженным в паямть" */
    addr->outside = FALSE; addr->address = (CdBlock) ABlock - CodeBuf;

    if (TraceFlags & TraceSwap) Show('A');

    /* перенастроим адреса */
    for(xpage=0; xpage<addr->codesize; ++xpage) {
      ToLinear(ABlock->PageMap[xpage]);
    }

  } else {
    ABlock = (CdBlockH) &CodeBuf[addr->address];
  }
  return(ABlock);
}


/*
 * Генерация номера кода
 */
codeaddress* GenCdNum(Code,owner) unchar* Code; unchar* owner; {
  for(*Code = 0; Agent->codes[*Code].codesize !=0;) {
    if (++*Code >= MaxCodes) ExecError(ErrManyCodes);
  }
  *owner = Agent->ident;
  return(&Agent->codes[*Code]);
}
/*
 * Освобождение блоков кода запроса
 */
void ReleaseCode(unshort number) {
  CdBlockH    ABlock;
  int         i;
  CdBlockH*   uscan;

  if (number>=MaxCodes || Agent->codes[number].codesize == 0) {
    ExecError(ErrNoCode);
  }

  /* загрузим код, если его нет*/
  ABlock = LoadCode(&Agent->codes[number],NIL);

  /* освобождаем все временные файлы */
  RlseAllTemps((unchar)number);
/* 
/*for(i = 0; i < ABlock->nSorts; ++i) {
/*  SortScan* scan = (SortScan*)RealAddr(ABlock->ptrs.sortscans[i]) -1;
/*  int       nlevel;
/*  /* освобождаем файл сортировки */
/*  if (scan->sortfile != 0) RlseTemp(scan->sortfile);
/*
/*  /* освобождаем файлы уровней слияния */
/*  for(nlevel=0; nlevel<MaxMergeLev;++nlevel) {
/*    if(scan->levels[nlevel].filled != 0)
/*                        RlseTemp(scan->levels[nlevel].file);
/*  }
/*}
/*if(ABlock->ldataTempFile != 0) {
/*  RlseTemp(ABlock->ldataTempFile); ABlock->ldataTempFile = 0;
/*}
*/

  /* выпихнем из списка загруженных кодов */
  for(uscan= &Hblock; *uscan != ABlock ; uscan= &(*uscan)->nextloaded) {
    if (*uscan == NIL) myAbort(150);
  }
  *uscan = (*uscan)->nextloaded;
  Agent->codes[number].codesize = 0;

  /* освобождаем все блоки кода */
  for(i = ABlock->Nblocks; --i>=0; ) {
    CdBlock block = ABlock->PageMap[i];
    *(CdBlock*) block = Eblock; Eblock = block;
  }
}

/*
 * Проход по своим кодам и убирание/блокирование Fetch кодов,
 * использующих таблицу tablenum (кроме данного кода - thisCode)
 */
void LockCode(table tablenum, int current, boolean dropflag) {
  int         j;
  CdBlockH    thisCode = (current >= 0 ?
                           LoadCode(&Agent->codes[current],NIL) : NIL);

  /* ищем код, использующий данную таблицу */
  for (j=0; j<MaxCodes; ++j) {
    if (Agent->codes[j].codesize != 0  && j != current) {
      CdBlockH code  = LoadCode(&Agent->codes[j],thisCode);
      table*   list  = code->tableused;
      short    i     = code->Ntableused;
      while (--i>=0 && *list++ != tablenum) ;
      if (i>=0) {                    /* да, эта таблица использована  */
        if (dropflag) {              /* мы эту таблицу стерли ?       */
          ReleaseCode(j);            /* -> похерим код вообще         */
        } else {                     /* нет, мы ее только модифицируем*/
          code->ptrs.contentry= Cnil;/* закроем сканер на нее         */
          code->stage         = stageClosed;
        }
      }
    }
  }
}
