                 /* ==================================== *\
                 ** ==            M.A.R.S.            == **
                 ** == Интерпретация исполнимого кода == **
                 \* ==================================== */

#define   CodeMain
#include "FilesDef"
#include "CodeDef"
#include "ExecDef"
#include "AgentDef"
#include "BuiltinDef"

         void     ExecCode(unsigned,short,char*);
  extern void     LockCode(table,int,boolean);
  extern void     ReleaseCode(unshort);

  /* обращения к диспетчеру             */
  extern void     LockAgent(void);           /* передиспетчеризация   */

  /* обращения к в/выводу нижнего уровня*/
  extern boolean  LockElem(lockblock*,short,boolean);
  extern boolean  UpdInfo(int);
  extern void     RlseTemp(table);           /*освобождение temp-файла*/

  /* обращения к swapping - системе     */
  extern CdBlockH LoadCode(codeaddress*,CdBlockH);

  /* run-time операции: выдача сообщений об ошибках */
  extern void     ExecError(int);

  /* run-time операции (GetRecords) */
  extern void     InitScan (UpdateScan*);    /* инициация сканера табл*/
  extern zptr     GetRecord(RID*);           /* доступ к записи по RID*/
  extern zptr     NxtRecord(RID*);           /* доступ по сканеру     */
  extern boolean  UpdRecord(UpdateScan*,     /* модификация записи    */
                            char*,short*);
  extern void     InsRecord(UpdateScan*,     /* вставка     записи    */
                            char*,boolean);
  extern void     DelRecord(UpdateScan*,     /* удаление    записи    */
                                  boolean);

  /* run-time операции (GetIndex)   */
  extern void     ConvKey  (IndexScan*);     /* преобразование ключа  */
  extern boolean  LookIndex(IndexScan*);     /* поиск индекса         */
  extern void     IniFIndex(IndexScan*);     /* инициация сканера инд.*/
  extern void     InsIndex(table,char*,RID*);/* вставка в индекс      */
  extern int      NextIndex(IndexScan*);     /* сканирование индекса  */

  /* run-time операции (Sort)       */
  extern void     IniSort(SortScan*);        /* иниц-ия сканера сортир*/
  extern void     PutSort(SortScan*,unchar); /* занесение сортируемых */
  extern void     FinSort(SortScan*,unchar); /* окончат.  сортировка  */
  extern zptr     GetSort(SortScan*,unchar); /* чтение отсортированных*/

  /* run-time операции (GetCatalog) */
  extern zptr     NxtCatal(char*,int);       /* спец-сканер           */

  /* run-time операции (ExecSupport)*/
  extern boolean  Match(char*,short,         /* сравнение с образцом  */
                        char*,short);
  extern void     Calendar (unshort,short*,short*,short*);


  extern boolean  InitLDParms (CdBlockH);
  extern void     StoreLDParms(CdBlockH,char*);
  extern void     CheckLDParms(CdBlockH);
  extern short*   LoadLDParms (CdBlockH);

#define GetArithm(TY) \
    (IsZaddr(Pcode->addr) ? ZDATA(Zresult+Pcode->addr,TY): *(TY*)addr);

#define ExecArithm(TY,Reg,m) \
    { TY op = GetArithm(TY); \
      switch(Pcode->parm) { \
      case 0: Reg += op; break; \
      case 1: Reg -= op; break; \
      case 2: Reg *= op; break; \
      case 3: if(op == 0) ExecError(ErrOverflow); \
              Reg /= op; break; \
      case 4: if(op == 0) ExecError(ErrOverflow); \
              Reg m  op; break;\
      case 5: CondCode = (Reg==op ? ccEQ : \
                          Reg< op ? ccLT : ccGT); break;\
      case 6: Reg  = op;        \
      } \
    }

#define StoreArithm(TY,Reg) \
   if(IsZaddr(Pcode->addr)) \
     ZDATA(Zresult+Pcode->addr,TY)= Reg; \
   else \
     *(TY*)addr = Reg;

#define StoreArithm1(TY,Reg) \
   if(IsZaddr(Pcode->addr)) \
     _GZmov((char*)&Reg,sizeof(TY),Zresult+Pcode->addr); \
   else \
     MVS((char*)&Reg,sizeof(TY),addr);

#define StoreEntry() ABlock->ptrs.contentry = VirtAddr(Pcode);

#define ccGT 0x04
#define ccEQ 0x02
#define ccLT 0x01
#define ccNE (ccGT + ccLT)

void ExecCode(unsigned number, short operation, char* parameters) {
  CdBlockH   ABlock;                        /* адрес 0-блока кода     */
  Mcode*     Pcode;                         /* адрес кода             */
  zptr       Zresult;
  short*     retcode = NIL;

  short      Regshort;
  long       Reglong ;
  real       Regreal;
  ldataDescr Regldata;

#ifdef ZCache
  zptr       ZStringAddr;
#endif
  char*      StringAddr;
  short      StringLeng;

  unchar     CondCode;                      /* коды условий сравнений */

  static unchar  cond_m[3] = {ccLT,ccEQ,ccGT};
  static unchar* CondMasks = cond_m + 1;


  /* проверим номер кода и его наличие */
  if (number>=MaxCodes || Agent->codes[number].codesize == 0) {
    ExecError(ErrNoCode);
  }

  /* загрузим код в память */
  ABlock  = LoadCode(&Agent->codes[number],NIL);

  /* проверим выполнимость кода */

  switch(operation) {
  default:
    ExecError(ErrSyst);
  case doStartFetch:
    if(ABlock->stage != stageFetch && ABlock->stage != stageData)
       ExecError(ErrClosed);
  case doContinue:
    if (ABlock->ptrs.contentry == Cnil) ExecError(ErrClosed);
    Pcode = (Mcode*) RealAddr(ABlock->ptrs.contentry);

  break; case doStartExec:
    ABlock->ptrs.contentry = Cnil;
    ABlock->stage = stageClosed;

    if(ABlock->ldataTempFile != 0) {
      RlseTemp(ABlock->ldataTempFile);  ABlock->ldataTempFile = 0;
    }
    if(ABlock->ptrs.inpbuf != Cnil) {
      int i;
      short* bufadr = (short*) RealAddr(ABlock->ptrs.inpbuf);
      MVS(parameters,ABlock->maxibuff,(char*) bufadr);
      if (bufadr[ABlock->nivars] != ABlock->minibuff)
                                            ExecError(ErrParameters);
      for(i=0; i<ABlock->nivars; i++) {
        /* проверим, что длины var не вылезают за конец и >= 0 */
        if(bufadr[i] > ABlock->maxibuff         ||
           bufadr[i] < bufadr[i+1]+sizeof(short) )
                               ExecError(ErrParameters);
      }

      if(InitLDParms(ABlock)) {
        if (TraceFlags & TraceOutput) Reply("Wait Long");
        ABlock->stage = stageWaitLong;
        MesBuffer[0] = sizeof(short);
        OutBuffer    = MesBuffer;
        OutAgent     = Agent->ident;
        return;
      }
    }
    Pcode = (Mcode*) ((char*) ABlock + ABlock->execentry);

  break; case doPutLong:
    if(ABlock->stage != stageWaitLong) ExecError(ErrClosed);
    if(*(short*)parameters >= 0) {               /* set data         */
      StoreLDParms(ABlock,parameters);
      if (TraceFlags & TraceOutput) {
        Reply(Fdump("Ldata[%2.16] stored: %4.16",*(short*)parameters,
                    (unsigned)*(long*)(parameters+sizeof(short))));
      }
      MesBuffer[0] = sizeof(short);
      OutBuffer    = MesBuffer;
      OutAgent     = Agent->ident;
      return;
    }
    CheckLDParms(ABlock);                        /* end of longdata  */
    Pcode = (Mcode*) ((char*) ABlock + ABlock->execentry);

  break; case doGetLong:
    if(ABlock->stage != stageData) ExecError(ErrClosed);
    OutBuffer    = LoadLDParms(ABlock);
    OutAgent     = Agent->ident;
    if(TraceFlags & TraceOutput) {
      Reply(Fdump("Ldata got: %4.16",*(short*)OutBuffer));
    }
    return;
  }

  do {                                   /* основной цикл выполнения  */
    char*   addr = RealAddr(Pcode->addr);
    char    oper = Pcode->oper;
    short   size = (oper>=Mwithleng ? sizeof(Mlcode) : sizeof(Mcode));
    switch(oper) {

    default:                            /* неверная машинная команда  */
      ExecError(ErrSyst);

    case Mbegfetch:                     /* конец подготовки Fetch     */
      *((char**) &Pcode) += size;
      StoreEntry();
      ABlock->stage = stageFetch;
      if (TraceFlags & TraceOutput) Reply("Scan inited");
      MesBuffer[0] = sizeof(short); retcode = MesBuffer;

    break; case Mretfetch:              /* возврат из Fetch с записью */
      *((char**) &Pcode) += size;
      StoreEntry();
      addr = RealAddr(ABlock->ptrs.outbuf);
      if (TraceFlags & TraceOutput) {   /* трассировка выдачи рез-тата*/
        Reply(Fdump("Result=%( %4.16)",
               (unsigned) *(short*) addr, (unshort*) addr));
      }
      ABlock->stage = stageData;
      retcode = (short*)addr;

    break; case Mendfetch:              /* возврат из Fetch с EndOfTab*/
      StoreEntry();
      if (TraceFlags & TraceOutput) Reply("End of table");
      ABlock->stage = stageFetch;
      MesBuffer[0] = sizeof(short); retcode = MesBuffer;

    break; case Mmodret:                /* возврат из Exec модификаций*/
      if (TraceFlags & TraceOutput) Reply("Modified");
      *(long*)(MesBuffer+1) = ABlock->modcounter;
      MesBuffer[0] = sizeof(short)+sizeof(long); retcode = MesBuffer;

    break; case Mindret:                /* возврат NIL (created)      */
      if (TraceFlags & TraceOutput) Reply("Created");
      UpdInfo(-1);                      /* обновим статистикку        */
      MesBuffer[0] = sizeof(short); retcode = MesBuffer;

    break; case Mcjump:                 /* условный переход           */
      if(CondCode & Pcode->parm) {
        Pcode = (Mcode*) addr; size = 0;
      }

    break; case Mejump:                 /* прыжок при 0-ом рез-те     */
      if(Zresult != (zptr) 0) break;
    case Mjump:                         /* прыжок в коде              */
      Pcode = (Mcode*) addr; size = 0;

    break; case Mclrbuf:                /* обнуление буфера           */
      _fill(addr,Pcode->parm,0);

    break; case MsetSconst:             /* занесение short- константы */
      *(short*) addr = Pcode->parm;

    break; case MsetLconst:             /* занесение long - константы */
      *(long*)  addr = Pcode->parm;

    break; case McmpSconst:             /* сравнение с short-константой*/
      CondCode = *(short*)addr == Pcode->parm ? ccEQ :
                 *(short*)addr <  Pcode->parm ? ccLT : ccGT;

    break; case McmpLconst:             /* сравнение с long -константой*/
      CondCode = *(long*)addr == Pcode->parm ? ccEQ :
                 *(long*)addr <  Pcode->parm ? ccLT : ccGT;

    break; case MadmLconst:             /* прибавление long - константы*/
      *(long*)  addr += Pcode->parm;

    break; case Mchuniq:                /* проверка существ. и единств*/
      if((*(short*)addr += Pcode->parm) != 1)
                    ExecError(Pcode->parm ? ErrEqmany : ErrEqempty);

    break; case Mshortng:               /* смена знака: short         */
      Regshort = -Regshort;

    break; case Mlongng:                /* смена знака: long          */
      Reglong  = -Reglong;

    break; case Mfloatng:               /* смена знака: float         */
      Regreal = -Regreal;

    break; case Mshortcv:               /* преобразование к short     */
      Regshort = (Pcode->parm==1 ? (short)Reglong : (short)Regreal);

    break; case Mlongcv:                /* преобразование к long      */
      Reglong  = (Pcode->parm==0 ? (long) Regshort: (long) Regreal);

    break; case Mfloatcv:               /* преобразование к float     */
      Regreal  = (Pcode->parm==0 ? (real) Regshort: (real) Reglong);

    break; case Mshortop:               /* арифметика short-чисел     */
      ExecArithm(short,Regshort,%=);

    break; case Mlongop:                /* арифметика long-чисел      */
      ExecArithm(long,Reglong,%=);

    break; case Mfloatop:               /* арифметика float-чисел     */
      ExecArithm(real,Regreal,*=);

    break; case Mshortst:               /* запись short-чисел         */
      StoreArithm(short,Regshort);

    break; case Mlongst:                /* запись long-чисел          */
      StoreArithm(long,Reglong);

    break; case Mfloatst:               /* запись float-чисел         */
      StoreArithm(real,Regreal);

    break; case Mshortst1:              /* запись short-чисел (odd)   */
      StoreArithm1(short,Regshort);

    break; case Mlongst1:               /* запись long-чисел  (odd)   */
      StoreArithm1(long,Reglong);

    break; case Mfloatst1:              /* запись float-чисел (odd)   */
      StoreArithm1(real,Regreal);

    break; case Mldatald:               /* загрузка longdata идентиф. */
      if(IsZaddr(Pcode->addr)) {
        Regldata = ZDATA(Zresult+Pcode->addr,ldataDescr);
      } else {
        Regldata = *(ldataDescr*)addr;
      }

    break; case Mldatast:               /* запись longdata идентиф.   */
      StoreArithm1(ldataDescr,Regldata);

    break; case Mloadleng:              /* загрузка длины стринга     */
      Regshort = StringLeng;

    break; case Mcnvkey:                /* преобразвание ключа        */
      ConvKey((IndexScan*)addr);

    break; case Mstndf:                 /* вызов стандартных функций  */
     {unshort daynum = (unshort) ((unlong)Reglong / (60*24));
      switch(Pcode->parm) {
      case fUser:                       /* USER()                     */
        Regshort = Agent->userid;
      break; case fTime:                /* TIME()                    */
        if(Agent->starttime == -1) Agent->starttime = Timer;
        Reglong  = Agent->starttime;
      break; case fGMTime:              /* GMT()                     */
        if(Agent->starttime == -1) Agent->starttime = Timer;
        Reglong  = Agent->starttime - theTimeZone;
      break; case fMinute:              /* MINUTE(long)               */
        Regshort = (unshort) ((unlong)Reglong % (60*24));
      break; case fWeekDay:             /* WEEKDAY(long)              */
        Regshort = daynum % 7 + 1;
      break; case fYear:                /* YEAR(long)                 */
        Calendar(daynum,&Regshort,(short*)NIL,(short*)NIL);
      break; case fMonth:               /* MONTH(long)                */
        Calendar(daynum,(short*)NIL,&Regshort,(short*)NIL);
      break; case fDay:                 /* DAY(long)                  */
        Calendar(daynum,(short*)NIL,(short*)NIL,&Regshort);
      break; case fSize:                /* Length(VarChar)            */
        Regshort = StringLeng;
      break; case fLongSize:            /* Length(LongData)           */
        Reglong = Regldata.size;
      break; case fToUpper: case fToLower:
#       ifdef ZCache
          if(ZStringAddr != 0) ExecError(ErrSyst);
#       endif
        ExecError(ErrSyst);
/*      ToCase(StringAddr,StringLeng); */
      break; default:
        ExecError(ErrSyst);
      }
      }

    break; case Mbegscan:               /* инициац. сканера по таблице*/
      StoreEntry();
      InitScan((UpdateScan*) addr);

    break; case Mnxtrec:                /* сканирование таблицы       */
      StoreEntry();
      Zresult = NxtRecord((RID*)addr);

    break; case Mgetrec:                /* доступ к таблице по RID    */
      StoreEntry();
      Zresult = GetRecord((RID*)addr);

    break; case MinitU:                 /* инициация сканера модиф.   */
      ((UpdateScan*)RealAddr(ABlock->ptrs.modscan))->stage = mStageInited;
      *(char**)&Pcode += size; size = 0;
      StoreEntry();

    break; case Minsert:               /* Insert запись               */
      InsRecord((UpdateScan*)RealAddr(ABlock->ptrs.modscan),
                   RealAddr(ABlock->ptrs.outbuf), (boolean)Pcode->parm);

    break; case Mdelete:               /* Delete запись               */
      DelRecord((UpdateScan*)RealAddr(ABlock->ptrs.modscan),
                                                  (boolean)Pcode->parm);

    break; case Mupdate:               /* Update запись               */
      CondCode = CondMasks[
        UpdRecord((UpdateScan*)RealAddr(ABlock->ptrs.modscan),
                               RealAddr(ABlock->ptrs.outbuf),
                      (short*) RealAddr(ABlock->ptrs.modfields)) ];

    break; case Mmovrec:               /* MoveRec  (при Create Index) */
     _ZGmov(Zresult,ZDATA(Zresult,short),RealAddr(ABlock->ptrs.outbuf));

    break; case Minsind:               /* InsIndex (при Create Index) */
      InsIndex((table)Pcode->parm,RealAddr(ABlock->ptrs.outbuf),
                            (RID*)RealAddr(ABlock->ptrs.modscan));

    break; case Mcountmod:             /* Подсчет модификаций         */
      ABlock->modcounter++;

    break; case Minitmod:              /* Инициация процесса модиф-ции*/
      ABlock->modcounter = 0;
      StoreEntry();
      {
        table subfile = (table) Pcode->parm;

        /* захватываем таблицу и все ее индексы/longdata */
        while (subfile != NoFile) {
          /* проверим, что на таблице нет чужого Shared-lock-a */
          if(LockElem(&Files[subfile].iR,Sltype,TRUE)) myAbort(22);

          /* поставим свой "intent exclusive": */
          /* проверим, что iW чист (т.е. нет Lock in Shared) */
          /* поставим свой флажок - "модифицируем"           */
          if ( LockElem(&Files[subfile].iW,Xltype,FALSE) ||
               LockElem(&Files[subfile].iM,Sltype,TRUE )  ) {
            if(Files[subfile].kind == tableKind ||
               Files[subfile].kind == ldataKind) LockAgent();
            /* если захватили таблицу, то индекс обязаны захватить */
            myAbort(23);
          }
          subfile = Files[subfile].nextfile;
        }

        /* далее закрываем свои курсоры, использующие данную таблицу,
           кроме текущего */
        LockCode((table)Pcode->parm,(int) number,FALSE);
      }

    break; case Mcolscan:case Mcatscan:/* Сканер описателей столбцов  */
      StoreEntry();
      Zresult = (zptr) NxtCatal(addr,Pcode->parm);

    break; case Mlookind:              /* Поиск в индексе по ключу    */
      StoreEntry();
      CondCode = CondMasks [ LookIndex((IndexScan*) addr) ];

    break; case Mbegsind:              /* Начало полного скан. индекса*/
      StoreEntry();
      IniFIndex((IndexScan*) addr);
      CondCode = CondMasks [TRUE];

    break; case Mnxtind:               /* Сканирование по индексу     */
      StoreEntry();
      CondCode = CondMasks [NextIndex((IndexScan*) addr)];

    break; case Mbegsort:              /* Инициация сортировки        */
      IniSort((SortScan*) addr);

    break; case Mputsort:              /* Входная инф-ция сортировки  */
      StoreEntry();
      PutSort((SortScan*) addr,ABlock->codenum);

    break; case Mfinsort:              /* окончательная сортировка    */
      StoreEntry();
      FinSort((SortScan*) addr,ABlock->codenum);

    break; case Mgetsort:              /* Выборка отсортированного    */
      StoreEntry();
      Zresult = GetSort((SortScan*) addr,ABlock->codenum);
      CondCode = CondMasks[Zresult == (zptr) 1];

    break; case Mloadstr: case Mcmpstr: case Mlike:
      { short aLeng  = ((Mlcode*)Pcode)->leng;
        short i,minlen;
#       ifndef ZCache
          if(IsZaddr(Pcode->addr)) addr = Pcode->addr + (char*)Zresult;
#       else
          zptr  aZaddr = Pcode->addr + Zresult;
#       endif
        if(Pcode->parm) {             /* VarChar                      */

#         ifdef ZCache
          if( IsZaddr(Pcode->addr)) {
            aZaddr += sizeof(short);
            aLeng = ZDATA(aZaddr-sizeof(short),short) -
                      ZDATA(aZaddr,short) - sizeof(short);
            aZaddr += ZDATA(aZaddr,short);
          } else
#         endif
          { addr += sizeof(short);
            aLeng = ((short*) addr)[-1] -
                    ((short*) addr)[ 0] - sizeof(short);
            addr += *(short*)addr;
          }
        }

        switch(Pcode->oper) {
        case Mloadstr:
          StringLeng = aLeng;
          StringAddr = addr;
#         ifdef ZCache
            ZStringAddr = (IsZaddr(Pcode->addr) ? aZaddr : 0);
#         endif

        break; case Mcmpstr:                    /* сравнение строк    */
          CondCode = ccEQ;
          minlen = Min(aLeng,StringLeng);
          i      = 0;
#         ifdef ZCache
          if(IsZaddr(Pcode->addr)) {
            if(ZStringAddr) {
              while(i<minlen &&
                  Byte0DATA(i+ZStringAddr) == Byte0DATA(aZaddr++)) ++i;
            } else {
              while(i<minlen &&
                  StringAddr[i] == Byte0DATA(aZaddr++) ) ++i;
            }
          } else if (ZStringAddr) {
            while(i<minlen &&
                   Byte0DATA(ZStringAddr+i) == *addr++) ++i;
          } else
#endif
            while(i<minlen &&
                   StringAddr[i] == *addr++) ++i
          ;
          if (i<minlen) {
            CondCode = CodeTable[
#             ifdef ZCache
                ZStringAddr != 0 ? Byte0DATA(ZStringAddr+i) :
#             endif
              StringAddr[i] ] >= CodeTable[
#             ifdef ZCache
                IsZaddr(Pcode->addr) ? Byte0DATA(--aZaddr) :
#             endif
              *--addr] ? ccGT : ccLT;
          } else if (aLeng < StringLeng) {
#           ifdef ZCache
            if(ZStringAddr != 0)
              while(Byte0DATA(ZStringAddr+i)==' ' && ++i<StringLeng);
            else
#           endif
            while(StringAddr[i]==' ' && ++i<StringLeng);
            if(i<StringLeng) CondCode = ccGT;
          } else if (aLeng > StringLeng) {
#           ifdef ZCache
            if(IsZaddr(Pcode->addr))
              while(Byte0DATA(aZaddr++)==' ' && ++i<aLeng);
            else
#           endif
              while(*addr++ ==' ' && ++i<aLeng)
            ;
            if(i<aLeng) CondCode = ccLT;
          }

        break; case Mlike:                      /*сравнение по образцу*/
#         ifdef ZCache
            if( IsZaddr(Pcode->addr) || ZStringAddr != 0)
                                                   ExecError(ErrSyst);
#         endif
          CondCode =
                 Match(StringAddr,StringLeng,addr,aLeng) ? ccEQ : ccNE;
        }
      }

    break; case Mstorestr:                     /* запись char: parm=0->char*/
      { short maxleng = ((Mlcode*)Pcode)->leng;/* parm>0 ->var, 2-> в index*/
        short cutleng = (Pcode->parm ? 0 : maxleng);
        zptr  zshift  = Pcode->addr + Zresult;
#       ifdef ZCache
          if(ZStringAddr)
            while(cutleng < StringLeng &&
               Byte0DATA(ZStringAddr+StringLeng-1) == ' ') --StringLeng;
          else
#       endif
            while(cutleng < StringLeng &&
                       *( StringAddr+StringLeng-1) == ' ') --StringLeng;

	if(maxleng < StringLeng) ExecError(ErrOverflow);

        if(Pcode->parm) {                     /* занесение VarChar    */
#         ifdef ZCache
          if(IsZaddr(Pcode->addr))            /* Var в Z нельзя       */
                            ExecError(ErrSyst);
#         endif
          if(Pcode->parm != 2) {              /* это не в ключ индекса*/
            ((short*)addr)[0] =               /* пишем отн.адрес конца*/
                ((short*)addr)[1] + StringLeng +sizeof(short);
            addr += ((short*)addr)[1]+sizeof(short);
          }
          maxleng = StringLeng;
        }

#       ifdef ZCache
        if(ZStringAddr) {                     /* перенос из Z под Z ? */
          if( IsZaddr(Pcode->addr)) ExecError(ErrSyst);
          /* перенос из Z в G     */
          _ZGmov(ZStringAddr,StringLeng,addr);
          while (maxleng != StringLeng) addr[StringLeng++] = ' ';
        } else if(IsZaddr(Pcode->addr)) {     /* перенос из G под Z   */
          _GZmov(StringAddr,StringLeng,zshift);
          while (maxleng != StringLeng)
                          Byte0DATA(zshift+ StringLeng++) = ' ';
        } else                                /* перенос из G под G   */
#       endif
        {
          MVS(StringAddr,StringLeng,addr);
          while (maxleng != StringLeng) addr[StringLeng++] = ' ';
        }
      }

    break; case Mlea:                         /* запись адреса        */
      *(Caddr*) RealAddr(((Mlcode*)Pcode)->saddr) = Pcode->addr;

    }
    *(char**) &Pcode += size;
  } while (retcode == NIL);
  OutBuffer    = retcode;
  OutAgent     = Agent->ident;
}
