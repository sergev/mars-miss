                    /* ============================ *\
                    ** ==         M.A.R.S.       == **
                    ** ==   Манипуляция данными  == **
                    ** ==     в файлах таблиц    == **
                    \* ============================ */

#include "FilesDef"
#include "AgentDef"
#include "DataDef"

         void     InitScan (UpdateScan*);    /* инициация сканера табл*/
         zptr     GetRecord(RID*);           /* доступ к записи по RID*/
         zptr     NxtRecord(RID*);           /* доступ по сканеру     */
         boolean  UpdRecord(UpdateScan*,     /* модификация записи    */
                            char*,short*);
         void     InsRecord(UpdateScan*,     /* вставка     записи    */
                            char*,boolean);
         void     DelRecord(UpdateScan*,     /* удаление    записи    */
                                  boolean);
         void     UpdFree  (PD);             /* обновл. room (COMMIT) */

         boolean  ModIndex(table,short*);    /* проверка мод. полей  */
         boolean  ModLData(table,short*);    /* проверка мод. ldata  */
         table    GetNxIndex(table);

  /* обращения к диспетчеру             */
  extern void     LockAgent(void);           /* передиспетчеризация   */

  /* выдача сообщений об ошибках */
  extern void     ExecError(int);


  /* обращения к в/выводу нижнего уровня*/
  extern zptr     GetPage(page,table,short); /* доступ к блоку        */
  extern zptr     GetPins(page,table,short); /* доступ к блоку для Ins*/
  extern zptr     GetMapPage(page,table);    /* загрузка Map(без блок)*/
  extern void     SetModified(short,short);  /* отметка блока: модиф-н*/
  extern boolean  LockTable(table,short);    /* блокировка таблиц     */
  extern page     FindUsed(page,table,short);/* поиск для Insert      */

  /* обращния на манипуляцию индексами  */
  extern void     InsIndex(table,char*,RID*);/* вставка в индекс      */
  extern void     DelIndex(table,zptr,RID*,  /* стирание из индекса   */
                                 IndexScan*);
  extern void     UpdIndex(IndexScan*,RID*); /* модификация индекса   */
  extern page     ClustIndex(table,char*);   /* место предпочт.вставки*/

  /* обращния на манипуляцию LongData  */
  extern void     InsLData(ldataDescr*,table);/* вставка в LData       */
  extern void     DelLData(table,page,long); /* стирание из LData     */


typedef short       pageptr;        /* указатель смещ. в блоке даннных*/
#define FMoveFlag   0x4000          /* флаг переноса вперед при Update*/

#define DATAHEAD     (&ZDATA(Index,data_header))
#define ldataDATA(x) ZDATA(Index+x,ldataDescr)



/*----------------------------*
 |    шапка блока данных      |
 +----------------------------+
 |                            |
 +----------+-----------------+  длина может быть нечетной,
 | длина    | запись с        |  но каждая запись располагается с
 +----------+                 |  четного адреса
 |        данными             |  к длине записи может быть прибавлена
 +----------------------------+  константа FMoveFlag, в этом случае
 | длина св.| свободный кусок |  запись необходимо пропустить,
 +----------+                 |  вычтя эту констнту (используется при
 | (длина с обратным знаком)  |  переносе расширенной во время UPDATE
 +----------------------------+  записи вперед по таблице)
 |                            |
 |                            |
 |                            |
 |                            |
 +--------+--------+----------+
 | адрес 2| адрес 1| адрес 0  |    если адресN = 0, то строки нет
 *--------+--------+----------*/


  static boolean  GetData   (RID*,short);     /* чтение блока         */
  static void     PutRecord (short,char*,     /* занесение записи в бл*/
                             short,short);
  static void     PackBlock (void);           /* упаковка блока       */
  static void     MergeFree (unsigned,int);   /* слияние своб.областей*/
  static void     AllocBlock(page*,table,     /* поиск подходящ. блока*/
                             unshort);
  static boolean  ScanMap   (page*,table,     /* поиск в MAP-блоках   */
                             unshort,unshort);

  static void     InsLongFields(UpdateScan*,char*,short*);
  static boolean  DelSubFiles  (UpdateScan*,short*,boolean);

  static zptr     Index;                      /* адрес блока в Z-сегм */
  static unshort  shift;                      /* адрес записи в блоке */
  static unshort  Dataend;                    /* адрес конца данных   */
  static unshort  NbRow;                      /* адрес конца данных   */
  static unshort  internal;                   /* адрес адреса записи  */

/*
 * Инициация прямого сканера по таблице
 */
void InitScan(ident) UpdateScan* ident; {
  if(LockTable(ident->ident.table,Sltype)) LockAgent();
  ident->ident.page = (page) 2;  ident->ident.index = 0xFF;
  ident->secondary = FALSE;
}

/*
 * Доступ к блоку таблицы
 */
static boolean GetData(RID* ident, short locktype) {
  if (Busy (Index = GetPage(ident->page, ident->table, locktype)) )
            return(FALSE);
  NbRow   = DATAHEAD->NbRow;
  Dataend = BlockSize -     NbRow         * sizeof(pageptr);
  internal= BlockSize + (~ ident->index)  * sizeof(pageptr);
  return(TRUE);
}

/*
 * Чтение записи по ее RID
 */
zptr GetRecord(ident) RID* ident; {
  zptr saddr;                                    /* адрес записи         */
  if (LimitResources || ! GetData(ident,Sltype)) LockAgent();
  if(internal<Dataend) myAbort(70);
  if( (shift = intDATA(internal))==0)myAbort(71);/* такой записи нет  */
  saddr = shift + Index;
  if(int0DATA(saddr) & FMoveFlag) {              /*перенесенная вперед*/
    int0DATA(saddr) &= ~ FMoveFlag;              /*сбросим флаг       */
    SetModified(0,0);                            /*в блоке            */
    saddr = 0;                                   /*вернем 0: повтор   */
  }
  return(saddr);
}

/*
 * Сканер по таблице
 */
zptr NxtRecord(ident) RID* ident; {
  zptr saddr;                                    /* адрес записи         */
  for(;;) {
    for(;;) {
      if(LimitResources) LockAgent();            /* пора и честь знать*/
      ScanFlag = TRUE;                           /* флаг "ищем конец" */
      Index = GetPage(ident->page, ident->table, Sltype);
      ScanFlag = FALSE;
      if (Index == (zptr) 2) return(0);          /* конец таблицы     */
      if (Busy(Index))       LockAgent();        /* заблокировались   */

      NbRow   = DATAHEAD->NbRow;
      Dataend = BlockSize - NbRow * sizeof(pageptr);

      ident->index = ident->index+1 & 0xFF;      /* следующий RID     */
      internal= BlockSize + (~ ident->index)  * sizeof(pageptr);
      while( internal >=Dataend && intDATA(internal)==0) {
        internal -= sizeof(pageptr);             /* пропускаем пустой */
        ++ident->index;
      }
    if(internal>=Dataend) break;                 /* блок не кончился  */
      ident->index = 0xFF;
      /* на след. страницу */
      if(++ident->page % FreeMapStep == 0) ++ident->page;
    }
    saddr = intDATA(internal)+Index;
  if(! (int0DATA(saddr) & FMoveFlag)) break;     /* уже "Updated"     */
    int0DATA(saddr) &= ~FMoveFlag;
    SetModified(0,0);
  }
  return(saddr);
}

/*
 * занесение записи в блок по адресу shift, старая длина - oldlength
 */
static void PutRecord(short oldlength, char* newrecord, short newlength, short delta) {

  _GZmov(newrecord,newlength,Index+shift);  /* переносим данные       */
  intDATA(internal) = shift;
  DATAHEAD->freesize -= newlength;          /* уменьшаем свободное    */
  shift += newlength;                       /* адрес за нами          */
  if (DATAHEAD->tailfree+newlength == shift) DATAHEAD->tailfree = shift;
  if ((newlength -= oldlength) != 0 ) {     /* вставили меньше старой */
    intDATA(shift) = newlength;             /* свобод. область за нами*/
  }
  SetModified(delta,0);
}

/*
 *
 */
static void InsLongFields(scan,newrecord,modfields)
                  UpdateScan* scan; char* newrecord; short* modfields; {
  while (scan->subfile != NoFile) {
    file_elem* pFile = Files + scan->subfile;
    if(pFile->kind == ldataKind) {
      if(modfields == (short*)-1 || ModLData(scan->subfile,modfields)) {
        short columnshift = pFile->b.ldatadescr.shift;
        InsLData((ldataDescr*) (newrecord+columnshift),scan->subfile);
      }
    }
    scan->subfile = pFile->nextfile;
  }
}

/*
 * Removing all indexes/longdata
 * the record should be loaded: <Index,shift> = its address
 */
static boolean DelSubFiles(scan,modfields,delscan)
                  UpdateScan* scan; short* modfields; boolean delscan; {
  boolean    done = FALSE;
  /* ищем модифицируемый индекс */
  while(!done && scan->subfile != NoFile) {
    file_elem* pFile = Files + scan->subfile;

    if(pFile->kind == ldataKind) {
      if( modfields == (short*)-1 || ModLData(scan->subfile,modfields)) {
        short columnshift = pFile->b.ldatadescr.shift;
        if(ldataDATA(shift+columnshift).table != scan->subfile) myAbort(736);
        DelLData(scan->subfile,ldataDATA(shift+columnshift).addr,
                               ldataDATA(shift+columnshift).size);
        done = TRUE;
      }

    } else if(pFile->kind == indexKind) {
      if( !(pFile->status & DrpFile)  &&
          (modfields == (short*)-1 || ModIndex(scan->subfile,modfields))) {
        if(delscan || scan->subfile != scan->scantable) {
          DelIndex(scan->subfile,shift+Index, &scan->ident,
                   scan->subfile == scan->scantable ? (IndexScan*)scan : NIL);
          done = TRUE;
        }
      }
    }
    scan->subfile = Files[scan->subfile].nextfile;
  }
  return(done);
}

/*
 * Модификация записи
 * Выход 0 - O.K.
 *       1 - запись сюда не влезает
 */
boolean UpdRecord(scan,newrecord,modfields)
                  UpdateScan* scan; char* newrecord; short* modfields; {
  short    newlength = Align (* (short*) newrecord);
  short    oldlength;
  short    freespace;

  if (scan->stage == mStageInited) {       /* запись еще не вставили */
    scan->stage   = mStageLdataIns;
    scan->subfile = Files[scan->ident.table].nextfile;
  }

  if(scan->stage  == mStageLdataIns) {
    InsLongFields(scan,newrecord,modfields);
    scan->stage   = mStageSubDel;
    scan->subfile = Files[scan->ident.table].nextfile;
  }

  /* запись еще не вставили */
  if (scan->stage == mStageSubDel || scan->stage == mStageRecord) {
    do {
      if(LimitResources || ! GetData(&scan->ident,Xltype)) LockAgent();
      if(internal<Dataend) myAbort(70);
      if((shift=intDATA(internal))==0)myAbort(71);/* такой записи нет */

      oldlength = Align (intDATA(shift));   /* длина старой записи    */

      /* можно ли засунуть запись в данный блок ? */
      if(newlength - oldlength > DATAHEAD->freesize) return(TRUE);

    } while(scan->stage == mStageSubDel && DelSubFiles(scan,modfields,FALSE));

    scan->stage = mStageRecord;

    freespace =  intDATA(shift+oldlength);  /* длина дырки за нами ?  */
    if (shift+oldlength == Dataend || freespace >=0) {
      freespace = 0;                        /* за нами нет пустых     */
    }

    if(shift+oldlength==DATAHEAD->tailfree){/* модифиц-мая - последняя*/
      DATAHEAD->tailfree = shift;           /* модифицируем последнюю */
    }

    DATAHEAD->freesize += oldlength;        /* добавляем в свободное  */
    oldlength -= freespace;                 /* об'единим с пустым     */

    if ( oldlength < newlength) {           /* на то же место нельзя  */
      intDATA(shift) = -oldlength;          /* освобождаем старое     */
      PackBlock();                          /* пакуем блок            */
      shift    = DATAHEAD->tailfree;        /* своб. область в конце  */
      oldlength= DATAHEAD->freesize;        /* ее длина               */
    }
    PutRecord(oldlength,newrecord,newlength,0);
    scan->stage   = mStageIndIns;
    scan->subfile = Files[scan->ident.table].nextfile;
  }

  /* вставляем индексы */
  while((scan->subfile = GetNxIndex(scan->subfile)) != NoFile) {
    if(ModIndex(scan->subfile,modfields)) {
      InsIndex(scan->subfile,newrecord,&scan->ident);
    }
    scan->subfile = Files[scan->subfile].nextfile;
  }

  return(FALSE);
}

/*
 * Вставка записи в блок
 * before: -> уже нашли, куда вставлять
 * Алгоритм работы:
 * В первую очередь определяется желательное место вставки (#страницы):
 *    если у таблицы есть не стертый еще индекс, то
 *      определяется по этому индексу,
 *    если индекса нет, то желательное место вставки -
 *       первый блок данных (страница #2),
 *    а при вставке во время Update - (зачем нам это нужно ???!!!)
 *       страница, из которой нас вынесли (хотя в нее мы уже не попадем)
 * Вставленные при Update записи могут быть помечены FMoveFlag-ом
 *       (см. ниже)
 * В конце вставляется информация о записи во все индексы и 
 *    LongData
 */
void InsRecord(scan,newrecord,updateflag)
                UpdateScan* scan; char* newrecord; boolean updateflag; {

  if (LimitResources) LockAgent();
  if(scan->stage == mStageInited) {
    scan->stage   = mStageLdataIns;
    scan->subfile = Files[scan->ident.table].nextfile;
  }

  if(scan->stage == mStageLdataIns) {
    if(!updateflag) InsLongFields(scan,newrecord,(short*)-1);
    scan->stage = mStageLook;
  }

  if (scan->stage == mStageRecord ||
      scan->stage == mStageLook) {          /* запись еще не вставили */
    short newlength = Align (* (short*) newrecord);
    short add = 0;                          /* кусок на указатель     */

    if(scan->stage == mStageLook) {         /* место еще не искали    */
      table clastindex =                    /* поищем в 1-ом индексе  */
          GetNxIndex(Files[scan->ident.table].nextfile);
      if( clastindex != NoFile) {           /* есть по чему кластер-ть*/
        scan->inspage = ClustIndex(clastindex,newrecord);
      } else {                              /* нет индекса -> в начало*/
        /* это вроде бы нам не нужно */
/*      scan->inspage =                     /* или откуда вынесли при */
/*          (updateflag ? oldRID.page : 2); /* Update                 */
        scan->inspage = 2;
      }
      scan->stage = mStageRecord;
    }

    AllocBlock(&scan->inspage,scan->ident.table,newlength+sizeof(pageptr));

    for( internal=BlockSize;                /* ищем дырку в ук-лях    */
      (internal -= sizeof(pageptr))>=Dataend && intDATA(internal) !=0;);
    if (internal < Dataend) add=sizeof(pageptr);
    if(DATAHEAD->freesize < newlength+add) {
      myAbort(72);                          /* на странице места нет  */
    }
    while (Dataend - DATAHEAD->tailfree < newlength + add) PackBlock();

    if (add !=0 ) {                         /* число записей увеличим */
      DATAHEAD->freesize -= add;  NbRow = ++DATAHEAD->NbRow ;
      Dataend -= sizeof(pageptr);
    }

    shift     = DATAHEAD->tailfree;         /* вписываем в хвост блока*/

    PutRecord(Dataend-DATAHEAD->tailfree,newrecord,newlength,1);

    /* записи, вставленные во время Update, помечаются специальным
       флажком FMoveFlag, если они будут еще раз считаны Update-ом:
       это произойдет при переносе записи вперед по файлу. */
    if(updateflag && scan->ident.page<scan->inspage) {
      intDATA(intDATA(internal)) |= FMoveFlag;
    }
    scan->insindex= (unchar)((BlockSize-internal)/sizeof(pageptr)-1);
    scan->stage   = mStageIndIns;
    scan->subfile = Files[scan->ident.table].nextfile;
  }

  /* вставляем индексы */
  while ((scan->subfile = GetNxIndex(scan->subfile)) != NoFile) {
    RID newRID;
    newRID.index = scan->insindex;
    newRID.table = scan->ident.table;
    newRID.page  = scan->inspage;
    if(updateflag && scan->subfile == scan->scantable) { /* pri perenose zapisi -  */
      UpdIndex((IndexScan*)scan,&newRID);                /* модиф-ия скана по Index*/
    } else {                             
      InsIndex(scan->subfile,newrecord,&newRID);
    }
    scan->subfile = Files[scan->subfile].nextfile;
  }
}

/*
 * Удаление записи из блока
 * Сначала удаляется информация о записи из всех индексов и LongData
 * (status - номер не обаботанного индекса)
 * Если сканирование идет по индексу, то:
 *   при стирании из-за Update индекс не удаляется вообще
 *   при реальном стирании передается IndexScan для корректировки
 * В конце удаляется сама запись из таблицы
 */
void DelRecord(scan,updateflag) UpdateScan* scan; boolean updateflag; {
  unsigned i;
  if(scan->stage == mStageInited) {
    scan->stage   = mStageSubDel;
    scan->subfile = Files[scan->ident.table].nextfile;
  }

  do {
    if(LimitResources || !GetData(&scan->ident,Xltype)) LockAgent();
    if(internal<Dataend) myAbort(70);
    if( (shift= intDATA(internal))==0)myAbort(71);/* такой записи нет */

    /* все индексы/ldata потерли ? */
  }while(scan->stage==mStageSubDel && DelSubFiles(scan,(short*)-1,!updateflag));

  scan->stage = mStageRecord;

  intDATA(internal) = 0;                    /* затираем адрес в блоке */

  for(i=Dataend; i<BlockSize && intDATA(i)==0; i+=sizeof(pageptr)) ;
  if(i != Dataend) {                        /* что-то освободили      */
    unsigned j = Dataend;
    DATAHEAD->NbRow = (BlockSize - i) / sizeof(pageptr);
    Dataend = i;
    MergeFree(j,i-j);                       /* включаем ее в блок     */
  }
  MergeFree(shift,Align(intDATA(shift)));   /* об'являем свободной    */
  SetModified(-1,0);
}

/*
 * Обработка создания новой свободной области
 */
static void MergeFree(addr,leng) unsigned addr; int leng; {
  int      i;
  unsigned prev = 0;
  unsigned this;

  DATAHEAD->freesize += leng;
  if(DATAHEAD->tailfree <= addr) {           /* сделали последнюю зону*/
    return;
  } if(DATAHEAD->tailfree == addr+leng) {    /* за нами - посл. дыра  */
    DATAHEAD->tailfree = addr;
  } else if(intDATA(addr+leng) <0 ) {        /* за нами тоже пустая   */
    leng -= intDATA(addr+leng);              /* сливаем с нею         */
  }

  intDATA(addr) = - leng;                    /* метим себя как пустую */

  if(addr == sizeof(data_header) ) {         /* мы - первые в блоке ? */
    return;
  }

  if(sizeof(data_header)-intDATA( sizeof(data_header))== addr) {
    prev= sizeof(data_header);               /* перед нами- 0-ая дыра */
  } else {
    short prevleng;
    /* ищем запись перед нами */
    for(i=Dataend; i<BlockSize; i+=sizeof(pageptr)) {
      if( (this = intDATA(i)) != 0 && this<addr) prev = Max(prev,this);
    }

    if(prev == 0) myAbort(78);           /* перед нами должна быть з. */
    prevleng = Align(intDATA(prev) & ~FMoveFlag);
    if (prev + prevleng != addr) {       /* эта запись не вплотную -> */
         /* перед нами есть дыра -> проверим, все ли в порядке */
      if((prev += prevleng) >= addr || intDATA(prev)>=0 ||
          prev - intDATA(prev) != addr) myAbort(79);
    } else {                             /* запись лежит вплотную     */
      prev = 0;
    }
  }

  if (prev != 0) {                       /* перед нами есть дыра ->   */
    intDATA(prev) += intDATA(addr);      /* сливаемся с ней           */
    if(DATAHEAD->tailfree == addr) DATAHEAD->tailfree=prev;
  }
}


/*
 * Упаковка данных в блоке
 */
static void PackBlock() {
  unshort  ptr0 = sizeof (data_header);
  unshort  ptr1 = sizeof (data_header);
  short    slice;

  while (ptr1 != DATAHEAD->tailfree) {     /* сканируем записи        */
    if (ptr1 >= Dataend) myAbort(74);
    slice = intDATA(ptr1);
    if (slice < 0) {                       /* пустой кусок            */
      ptr1 -= slice;
    } else {                               /* это запись с данными    */
      slice = Align(slice & ~FMoveFlag);   /* длина записи с данными  */
      if (ptr1 != ptr0 ) {                 /* и ее будем двигать      */
        unshort  ind;                      /* ищем ид-ор записи       */
        for(ind= Dataend;intDATA(ind) != ptr1 ; ) {
          if ( (ind += sizeof(pageptr)) >= BlockSize) myAbort(75);
        }
        intDATA(ind) = ptr0;
        _ZZmov(ptr1+Index,slice,ptr0+Index);
      }
      ptr1 += slice;
      ptr0 += slice;
    }
  }
  if(ptr0 < Dataend) intDATA(ptr0)=ptr0-Dataend;
  DATAHEAD->freesize = Dataend-ptr0;       /* длина свободного куска  */
  DATAHEAD->tailfree = ptr0;               /* адрес свободного куска  */
}

/*
 * Подбор блока для занесения записи
 * *newpage - содержит желательный номер страницы
 * Первым делом ищем ближайший подходящий блок из уже захваченных нами
 * Если он совпадает с искомым или непосредственно ближайший, то
 *          его мы и используем.
 * В противном случае ищем подходящий блок по карте свободных мест
 */
static void AllocBlock(page* newpage, table tablenum, unshort length) {
  unshort delta    = 0xFFFF;
  page    bestpage = *newpage;            /* желательная страница    */
  page    found    = FindUsed(bestpage,tablenum,length);
  boolean created;

  if (found != 0) {                        /* найдена подходящая      */
    delta = (bestpage<found ? found-bestpage : bestpage - found);
  }

  /* если найден ближайший блок, либо в нашей карте нет подходящего,
     то берем найденный */
  if (delta < 2 || (! (created = ScanMap(newpage,tablenum,delta,length)) &&
                      found != 0) ) {
    RID newIdent;
    newIdent.page = *newpage = found;
    newIdent.table= tablenum;
    if(!GetData(&newIdent,Xltype)) myAbort(80);

  /* Если в текущей карте не нашли, то ищем по всем картам */
  /* при этом delta нас уже не волнует: ставим 0xFFFF      */
  } else if (!created) {
    *newpage = 1;
    while (bestpage/FreeMapStep*FreeMapStep + 1 == *newpage ||
             ! ScanMap(newpage,tablenum,0xFFFF,length) ) {
      *newpage += FreeMapStep;
    }
  }
}


/*
 * Поиск подходящего блока по карте свободного места
 * поиск ведется по карте, соответствующей текущей странице, начиная
 * от нее, в обе стороны, однако не дальше, чем "maxdelta"
 * При обнаружении подходящей страницы она захватывается, а также
 *          производится коррекция карты, если это необходимо
 */
static boolean ScanMap(page* newpage, table tablenum, unshort maxdelta, unshort length) {
  enum {Nelems = BlockSize/sizeof(short)};     /* число элементов Map */

  page mappage = *newpage / FreeMapStep * FreeMapStep;
  int  from    = *newpage % FreeMapStep;
  int  ind     = from;
  int  delta   = 0;
  int  count   = 1;
  int  freeleng;
  zptr newindex;

  /* читаем карту, не захватывая ее */
  if (Busy(Index=GetMapPage(mappage,tablenum)) ) LockAgent();

  /* ищем свободную или подходящую страницу */
  while((freeleng = intDATA(ind*sizeof(short))) != BlockSize &&
        (freeleng < length ||
           Busy(newindex=GetPins(mappage+ind,tablenum,length)) )) {

    if(++count>=Nelems) return(FALSE);        /* карта исчерпана      */

    do {                            /* выбираем следующий элемент MAP */
      ind = (ind >= from ? - ++delta : delta) + from;
    } while(ind < 1 || ind >= Nelems);

    if(delta>=maxdelta) return(FALSE);        /* ушли далеко от места */
  }

  *newpage = mappage + ind;                   /* адрес подошедшей стр.*/

  if(freeleng == BlockSize) {                 /* создаем новый блок   */
    /* для создания надо захватить всю таблицу */
    if( LockTable(tablenum,Xltype)) LockAgent();

    /* если создаем последний блок, относящийся к данной карте,
       то расширяем файл, создавая новую карту */
    if(ind + 1 == Nelems) {
      page newmap = mappage + FreeMapStep;
      int  i      = sizeof(page);
      if(mappage>= MaxPage - 2*FreeMapStep) ExecError(ErrTableSpace);
      if( Busy(Index=GetPage(newmap,tablenum,Nltype)) ) {
        myAbort(81);                           /* не должно бы ..     */
      }
      /* засеваем блок длинами блоков - "все свободны" */
      do {
        intDATA(i) = BlockSize;
      } while ((i += sizeof(page) ) < BlockSize);
      SetModified(0,0);
    }

    /* меняем длину в карте на реальную */
    if( Busy(Index=GetPage(mappage,tablenum,Xltype)) ) {
      myAbort(81);                             /* не должно бы ..     */
    }
    intDATA(ind*sizeof(short)) -= sizeof(data_header);
    SetModified(0,0);
    Index = GetPage(*newpage,tablenum, Nltype);
    DATAHEAD->freesize = BlockSize - sizeof(data_header);
    DATAHEAD->tailfree = sizeof(data_header);
    DATAHEAD->NbRow    = 0;
    SetModified(0,0);
  } else {                                     /* зачитали старый блок*/
    Index = newindex;
  }
  NbRow   = DATAHEAD->NbRow;
  Dataend = BlockSize - NbRow * sizeof(pageptr);
  return(TRUE);
}

/*
 * обновление FreeMap-блоков
 * перенос длин модифицированных блоков из "Xlock.newspace" в FreeMap
 */
void UpdFree(ptr) PD ptr; {
  static table maptable;
  static page  mappage;
  if(ptr == NIL) {                             /* инициализация       */
    maptable = 0xFF;
  } else if (ptr->oldpage != (page) 1) {       /* не блок дескрипторов*/
    page newmappage = ptr->oldpage / FreeMapStep * FreeMapStep;
    if (newmappage != ptr->oldpage) {          /* саму карту не обновл*/
      /* это новая карта ? */
      if(newmappage != mappage || ptr->table != maptable) {
        if(Busy(Index=
             GetPage(mappage=newmappage,maptable=ptr->table,Xltype)
           )) myAbort(83);
      }
      intDATA((ptr->oldpage-mappage-1)*sizeof(short)+sizeof(page)) =
                     BlockSize - (ptr->locks.Xlock.newspace+1)*AlignBuf;
      SetModified(0,0);
    }
  }
}

/*
 * Проверка влияния модификации на индекс
 */
boolean ModIndex(table index, short* shiftarray) {
  short  columnshift;
  int j,i=0;
  if(shiftarray != NIL) {
    for(j=0; j<MaxIndCol &&
        (columnshift = Files[index].b.indexdescr.columns[j].shift)>=0;++j){
      for(i= *shiftarray; i!=0 ; --i) {
        if (shiftarray[i] == columnshift) return(TRUE);
      }
    }
  }
  return(FALSE);
}

boolean ModLData(table subfile, short* shiftarray) {
  short columnshift = Files[subfile].b.ldatadescr.shift;
  int   i = shiftarray == NIL         ? 0 : *shiftarray;
  while(i>0  && shiftarray[i] != columnshift) --i;
  return(i > 0);
}

table GetNxIndex(table ind) {
  while(ind != NoFile) {
    file_elem* ptr = Files + ind;
    if(ptr->kind == indexKind &&        /* this is an index       */
       !(ptr->status & DrpFile)) break; /* мы его еще не стерли   */
    ind = ptr->nextfile;
  }
  return(ind);
}
