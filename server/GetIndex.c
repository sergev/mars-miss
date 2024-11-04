                    /* ============================ *\
                    ** ==         M.A.R.S.       == **
                    ** ==   Манипуляция данными  == **
                    ** ==    в файлах индексов   == **
                    \* ============================ */

#include "FilesDef"

         void     ConvKey  (IndexScan*);     /* преобразование ключа  */
         boolean  LookIndex(IndexScan*);     /* поиск индекса         */
         int      NextIndex(IndexScan*);     /* сканирование индекса  */
         void     IniFIndex(IndexScan*);     /* инициация сканера инд.*/
         page     ClustIndex(table,char*);   /* место предпочт.вставки*/
         void     InsIndex(table,char*,RID*);/* вставка в индекс      */
         void     DelIndex(table,zptr ,RID*, /* стирание из индекса   */
                                 IndexScan*);
         void     UpdIndex(IndexScan*,RID*); /* модификация RID       */
         void     InitIndex(table);          /* инициац. файла индекса*/

  /* обращения к диспетчеру             */
  extern void     LockAgent(void);           /* передиспетчеризация   */

  /* обращения к в/выводу нижнего уровня*/
  extern zptr     GetPage(page,table,short); /* доступ к блоку        */
  extern void     SetModified(short,short);  /* отметка блока: модиф-н*/

  /* формирование ключа из записи       */
  extern void     FormKey(table,char*,boolean);

  extern void     ExecError(int);            /*выдача ошибки и откат  */

/*
  +===================================+
  |  шапка блока |                    |
  |--------------+                    |
  |                                   |
  |                                   |  +=============================+
  +============================+======+  | длина начальной части ключа,|
  |    шапка (descr) ключа     |массив|  |     общей с предыдущим      |
  |----------------------------+      |  +-----------------------------+
  |  идентификаторов записей (RID-ы)  |  | длина концевой части ключа, |
  +===================================+  | отличающейся от предыдущего |
  |    шапка (descr) ключа     |массив|  +-----------------------------+
  |----------------------------+      |  |   количество RID при ключе  |
  |  идентификаторов записей (RID-ы)  |  +=============================+
  +===================================+  | текст концевой части ключа  |
  |                                   |  |                             |
  |    свободный остаток блока        |  +=============================+
  |                                   |
  +-----------------------------------+
  Ключи могут быть переменной длины, но недопустимо создание
     таких ключей, что некий ключ совпадает с началом другого
  Индекс кончается ключом с "common = MaxKeyLen"
  Если к одному ключу относится более "MaxIndRIDs" записей, то
     создается новый ключ с "distinct=0", содержащий остальные RID-ы
  Если такие ключи не помещаются в одном блоке, то блок разбивается на
     несколько, причем в начале следующего блока ключ пишется целиком
  Вставка производится всегда в RID-ы первого ключа из списка одинаковых

          Root - блок
  +-----------------+--------+
  | ключ - E        | блок  -|----+
  +-----------------+--------+   /
  | ключ - K        | блок  -|----+       +----> +---------+----------+
  +-----------------+--------+ / /        |   +--|-        |          |
  | EndOfIndex      | блок  -|----+       |   |  +---------+---+------+
  +-----------------+--------+ / /        |   |  | ключ-A      | RIDs |
  |                          |  /         |   |  +-------------+------+
  +--------------------------+ /          |   |  | ключ-AA     | RIDs |
                          / / /           |   |  +-------------+------+
      +------------------+ / /            |   +  | ключ-B      | RIDs |
      |                   / /            /     \ +-------------+------+
      |                  / /            /       \
      +-> +-----------------+--------+ /         |
          | ключ - B        | блок  -|-          |
          +-----------------+----+---+           V
          | ключ - D        | бл1|бл2|====+----->+----------+---------+
          +-----------------+----+---+    |   +--|-         |         |
          | ключ - E        | блок  -|-+  |   |  +----------+--+------+
          +-----------------+--------+ |  |   |  | ключ-C      | RIDs |
          |                          | |  |   |  +-------------+------+
          +--------------------------+ |  |   |  | ключ-D      | RIDs |
               / /                     |  |   |  +-------------+------+
              / /                      |  |   |  | ключ-D      | RIDs |
             / /                       |  |   |  +-------------+------+
            / /                        |  |    \
           / /                         |  |     \
          / /                          |  |      V
         / /                           |  +----->+----------+---------+
        / /                            |      +--|-         |         |
       / /                             |      |  +----------+--+------+
      + +                              |      |  | ключ-D      | RIDs |
      | |                              |      |  +-------------+------+
      | |                              |      |  | ключ-D      | RIDs |
      | |                              |      |  +-------------+------+
      | |                              |      |  | ключ-D      | RIDs |
      | |                              |      |  +-------------+------+
      | |                              |       \
      | |                              |        \
      | |                              |         V
      | |                              +-------->+----------+---------+
      | |                                     +--|-         |         |
      | |                                     |  +----------+--+------+
      | |                                     |  | ключ-DD     | RIDs |
      | |                                     |  +-------------+------+
      | |                                     |  | ключ-DD     | RIDs |
      | |                                     |  +-------------+------+
      | |                                     |  | ключ-E      | RIDs |
      | |                                     |  +-------------+------+
      | |                                     |
      | |

*/

typedef struct {                          /* шапка блока индекса      */
  page   page;                            /* # страницы (для контроля)*/
  short  freeaddr;                        /* адрес концевой записи    */
  short  level;                           /* уровень индекса (0->leaf)*/
  page   next;                            /* след. страница на уровне */
  page   prevxxxx;                        /* предыдущая -"-           */
} ipageheader;

typedef struct {                          /* шапка ключа в индексе    */
  unchar common;                          /* общая длина с предыдущим */
  unchar distinct;                        /* длина хвоста ключа       */
  unchar RIDnumber;                       /* число RID в ключе        */
} descr;
#define ldescr 3                          /* длина шапки без выравнив.*/

#define byte0DATA(x)   ZDATA(x,unchar)
#define byteDATA(x)    byte0DATA((x)+Index)
#define descr0DATA(x)  ZDATA(x,descr)
#define descrDATA(x)   descr0DATA((x)+Index)
#define header0DATA(x) ZDATA(x,ipageheader)
#define headerDATA     header0DATA(Index)

  static zptr     Index,NewIndex;       /* Z-адреса блоков            */
  static page     pagestack[MaxDepth];  /* путь к leaf-блоку          */
  static unshort  splitbound;           /* точка деления блока        */

  static boolean  SearchKey   (void);   /* поиск в странице индесов   */

  static int      cmpRID (RID*,RID*);   /* сравнение RID-ов           */
  static void     InsKey (RID*,int,     /* вставка ключа в блок       */
                               boolean);
  static boolean  FindSplit   (void);   /* поиск точки разбиения блока*/
  static void     SplitBlock            /* разбиение блока на 2 части */
                              (page,page,table);
  static page     GetIndBlock (table);  /* создание нового блока      */
  static void     RlseIndBlock          /* освобождение блока индекса */
                              (table,page);
  static int      CountCluster          /* подсчет изменения класт-ии */
                              (unshort,unshort,unshort);

  static int     prevkey,   nextkey;
  static int     prevcommon;
  static int     nextcommon;

/*
 * Поиск в странице индексов,
 * вход :   Index - адрес i-блока;
 *          искомый ключ - в KeyBuffer, keylength
 * выход:   TRUE  -> найден
 *          FALSE -> индекс отутствует (найден индекс со значением >данного)
 *          prevkey -адрес меньшего ключа (или 0, если первый же не меньше);
 *          nextkey -адрес найденного или большего (или конца);
 * prevcommon = общая с предыдущим часть ключа
 * nextcommon = общая со следующим часть ключа
 */
static boolean SearchKey() {
  int      distinct   = 1;
  zptr     prev;                              /* предыдущий ключ      */
  zptr     scan     = Index;                  /* начало индексов      */
  zptr     endindex =
                  headerDATA.freeaddr+Index;  /* конец индексов       */
  int delta;
  int jump = sizeof(ipageheader);             /* длина тек.индекса    */
  nextcommon = 0;
  do {
    prevcommon = nextcommon;                  /* совпад.длина искомого*/
    do {                                      /* и текущего           */
      prev = scan;
      scan += jump;
      if(scan>=endindex) myAbort(103);        /* дошли до конца       */
      jump = descr0DATA(scan).RIDnumber * sizeof(RID) +
               ldescr + descr0DATA(scan).distinct;
      nextcommon = descr0DATA(scan).common;   /* общая длина          */
    } while (nextcommon != MaxKeyLeng &&      /* пока не конец индекса*/
             nextcommon > prevcommon);        /* пока она больше пред.*/
                                              /* можно не сравнивать  */

  if(nextcommon<prevcommon) break;            /* эта запись > нашей   */

     /* проверим на конец индекса */
  if(nextcommon == MaxKeyLeng) { nextcommon = 0; break; }

    distinct = descr0DATA(scan).distinct;
    { zptr scanbody = scan+ldescr;            /* тело индекса (dist)  */
      while(distinct != 0 &&                  /* сравниваем с нашей   */
            nextcommon < keylength &&
            (delta= (int)(unchar)byte0DATA(scanbody)-
                    (int)(unchar)KeyBuffer[nextcommon])==0) {
        ++nextcommon;                         /* индекс совпал лучше  */
        ++scanbody; --distinct;               /* сравниваем дальше    */
      }
    }
    /* пока текущий ключ меньше   */
  } while (nextcommon < keylength &&          /* наш ключ найден не весь*/
           (distinct == 0 || delta<0));       /* и считаный короче или <*/

/*if(distinct==0 && nextcommon != keylength) myAbort(100); -- теперь не так */

  prevkey   = prev - Index; nextkey= scan - Index;
  return(distinct==0 && nextcommon == keylength);
}

/*
 * Преобразование ключа
 */
void ConvKey(ident) IndexScan* ident; {
  int        j;
  indcolumn* pcol = Files[ident->RIDscan.scantable].b.indexdescr.columns;
  unchar*    keyBody = ident->keyBody;

  for(j=0; j<MaxIndCol; ++j) {
    boolean    inv = pcol->invFlag;
    int        i;
  if(pcol->shift < 0) break;
    if (pcol->numFlag) {
#     ifdef ReverseBytes
        SwapBytes(keyBody,pcol->length);
#     endif

#     ifdef BSFloat
      if(pcol->flvrFlag && (*keyBody & 0x80))
        inv = ~inv;
      else
#     endif
        *keyBody ^= 0x80;
    } else {
      TRS(keyBody,pcol->length,CodeTable);
    }
    if(inv) for(i=pcol->length; --i>=0; ) keyBody[i] ^= 0xFF;
    keyBody += pcol->length; ++pcol;
  }
}

/*
 * Найти индекс:
 * возврат: 0 - не найден;
 *          1 - найден
 *  (nextkey-адрес большего или адрес конца данных)
 *  identlist   - адрес RID-списка;
 *  identlength - длина - "" - (в байтах)
 */
boolean LookIndex(ident) IndexScan* ident; {

  page       current =0;                /* берем ROOT страницу        */
  boolean    wasFound;
  MVS(ident->keyBody,keylength = ident->keyLeng,KeyBuffer);

  for(;;) {                                   /* ищем до leaf-страницы*/
    if (Busy(Index = GetPage(current,ident->RIDscan.scantable,
                         Sltype))) LockAgent();
    wasFound = SearchKey();                   /* ищем внутри страницы */
  if (headerDATA.level ==0 ) break;           /* leaf-страница        */
    _ZGmov(nextkey+Index+descrDATA(nextkey).distinct + ldescr,
             sizeof(page),(char*)&current);   /* берем нижний блок    */
  }

  if(!wasFound || descrDATA(nextkey).RIDnumber == 0) return(FALSE);
  ident->page=current; ident->shift=nextkey; ident->RIDscan.iKey=0;
  ident->RIDscan.secondary = FALSE;
  return(TRUE);
}

/*
 * Инициация сканера для полного чтения индекса
 */
void IniFIndex(ident) IndexScan* ident; {

  page  current =0;                           /* берем ROOT страницу  */
  for(;;) {                                   /* ищем до leaf-страницы*/
    if(Busy(Index = GetPage(current,ident->RIDscan.scantable,Sltype)) )
                                                           LockAgent();
  if (headerDATA.level == 0) break;
    _ZGmov(sizeof(ipageheader) + Index +
                      descrDATA(sizeof(ipageheader)).distinct + ldescr,
             sizeof(page),(char*)&current);   /* берем нижний блок    */
  }

  ident->page=current; ident->shift=0; ident->RIDscan.iKey=0;
  ident->RIDscan.secondary = FALSE;
}


/*
 * Сканирование индекса для чтения RID-ов
 * В RID заносится адрес очередной записи с данным ключом,
 *   если же записи с данным ключом кончились, то
 *   возвращается -1, если кончилась таблица
 *                 0, если кончился ключ
 *       при следующем обращении заполняется новое значение ключа в
 *                  keyBody,keyLeng;
 */
int NextIndex(ident) IndexScan* ident; {
  boolean  newKey = FALSE;
  zptr     keyadr;

  for(;;) {
    /* читаем наш блок индекса */
    if(Busy(Index=GetPage(ident->page,ident->RIDscan.scantable,Sltype)))
                  LockAgent();
    if (ident->shift == 0) {                  /*эту стр.еще не трогали*/
      ident->shift = sizeof(ipageheader);     /*начнем скан. с начала */
      newKey = TRUE;                          /*возможна смена ключа  */
    }

    if (ident->RIDscan.iKey == 0xFF) {        /*стерт весь ключ       */
      newKey = TRUE; ident->RIDscan.iKey = 0; /*возможно начало нового*/
    }

    /* список RID у данного ключа не кончился ? -> берем следующий */
  if (descrDATA(ident->shift).RIDnumber > ident->RIDscan.iKey) break;
    ident->RIDscan.secondary = TRUE;

    /* кончился весь индекс ? */
    if (descrDATA(ident->shift).common == MaxKeyLeng) return(-1);

    newKey = TRUE;                            /* ключ,видимо, кончился*/
    ident->shift += ident->RIDscan.iKey*sizeof(RID) + /* адрес след-го*/
                         ldescr+descrDATA(ident->shift).distinct;
    ident->RIDscan.iKey   = 0;

    /* блок не кончился -> берем следующий RID */
  if(headerDATA.freeaddr != ident->shift) break;

    /* блок кончился -> читаем следующий */
    ident->page = headerDATA.next; ident->shift = 0;

  }

  keyadr = Index + ident->shift;

  /* ключ мог смениться -> проверим, сменился ли */
  if(newKey) {
    /* кончился весь индекс */
    if (descr0DATA(keyadr).common == MaxKeyLeng) return(-1);

    if (ident->shift != sizeof(ipageheader)) {   /* не начало блока   */
      newKey = descr0DATA(keyadr).distinct;
      /* начало блока - сравним ключи */
    } else if (! (newKey=descr0DATA(keyadr).distinct-ident->keyLeng)) {
      newKey= cmpsZG(keyadr+ldescr,ident->keyBody,ident->keyLeng);
    }
  }

  /* ключ-таки сменился -> выходим из п/п */
  if (newKey) {
    ident->RIDscan.secondary = FALSE;
    ident->RIDscan.iKey = 0; return(0);
  }

  /* первый индекс в ключе -> занесем сам ключ */
  if(ident->RIDscan.iKey == 0) {
    _ZGmov(keyadr+ldescr,descr0DATA(keyadr).distinct,
           &ident->keyBody[descr0DATA(keyadr).common]);
    ident->keyLeng = descr0DATA(keyadr).distinct +
                     descr0DATA(keyadr).common;
  }

  /* занесем считанный RID */
  _ZGmov(Index+ident->shift+ldescr+descrDATA(ident->shift).distinct+
          ident->RIDscan.iKey*sizeof(RID),sizeof(RID),
          (char*) &ident->RIDscan.ident);
  ++ident->RIDscan.iKey;
  return(1);
}

/*
 * Вычисление места предпочтительной вставки записи
 */
page ClustIndex(table tablenum, char* record) {
  page    current = 0;                        /* берем ROOT страницу  */
  boolean wasFound;
  RID     old;
  old.page = 2;

  FormKey(tablenum,record,FALSE);             /* формируем ключ       */
  for(;;) {
    if ( Busy(Index = GetPage(current,tablenum,Sltype)) ) LockAgent();
    wasFound = SearchKey();
  if (headerDATA.level ==0 ) break;           /* leaf-страница        */
    _ZGmov(nextkey+Index+descrDATA(nextkey).distinct + ldescr,
           sizeof(page),(char*)&current);     /* берем нижний блок    */
  }

  if( descrDATA(nextkey).RIDnumber != 0) {    /* нашли ключ >= нашего */
    _ZGmov(Index+nextkey+ldescr+descrDATA(nextkey).distinct,
                                     sizeof(RID),(char*)&old);
  } else if (prevkey != 0) {                  /* это конец, но не 1-ый*/
    _ZGmov(Index+nextkey-sizeof(RID),sizeof(RID),(char*)&old);
  }
  return(old.page);
}

/*
 * Вставка в индекс
 * Ищем соответствующий leaf-блок
 * Если ключ уже есть, вставляем RID (в первый из ключей, если их
 *                                    несколько)
 * Если ключа нет, делаем новый ключ
 * Если в блоке места нет, то будем бить блок на 2:
 *   ищем ключ разбиения (последний ключ в "верхней" половине);
 *   вставляем в верхний по дереву блок соттветствующий ключ
 *      (или RID, если ключ разбиения совпадает с поледним ключом блока)
 *   если при этом переполняется верхний по дереву блок, то бьем и его
 *
 */
   static short uniqflag;
void InsIndex(table tablenum, char* record, RID* ident) {
  page    current;
  int     stackindex;                         /* индекс pagestack     */
  boolean wasFound;
  int     inslength;                          /* величина расширения  */
  /* флаг уникальности ключа */
  uniqflag = (Files[tablenum].recordnum == UniqIndexFlag);

  /* главный цикл - попытка вставить: после разбиения любого блока
     цикл повторяется с начала */
  for(;;) {
    page newpage;
    current    = 0;                           /* берем ROOT страницу  */
    stackindex = 0;
    FormKey(tablenum,record,FALSE);           /* формируем ключ       */

    /* поиск до leaf-блока */
    for(;;) {
      if( Busy(Index = GetPage(current,tablenum,Sltype)) ) LockAgent();
      wasFound = SearchKey();                 /* ищем внутри страницы */
      pagestack[stackindex]=current;          /* запоминаем путь      */

    if (headerDATA.level ==0 ) break;         /* leaf-страница        */

      if(++stackindex>=MaxDepth) myAbort(102);/* глубина дерева велика*/
/*    if (nextkey == 0) myAbort(101);         /* промежуточный пуст ? */

      _ZGmov(nextkey+Index+descrDATA(nextkey).distinct + ldescr,
               sizeof(page),(char*)&current); /* берем нижний блок    */
    }

    /* цикл, пока нет места для вставки (под'ем по дереву) */
    for(;;) {

      /* вычисляем длину вставки */
      inslength =
        !wasFound ?                          /* новый ключ ?         */
          keylength - Max(nextcommon,prevcommon) + ldescr + sizeof(RID) :
        headerDATA.level !=0 ?               /* создаем chain ?      */
           0 :
        descrDATA(nextkey).RIDnumber == MaxIndRIDs-1 ?
          sizeof(RID) + ldescr :             /* создаем дубль ключа  */
          sizeof(RID);                       /* просто добавляем RID */

      /* если можно вставить, то на этом кончим */
    if (BlockSize - headerDATA.freeaddr >= inslength) break;

      /* надо бить блок на 2 -> сразу блокируем карту */
      if(Busy(GetPage((page)1,tablenum,Xltype)) ) LockAgent();

      if(stackindex) {                        /* это не ROOT-блок     */
        current=pagestack[--stackindex];      /* верхний блок         */

      } else {                                /* это ROOT-блок ->     */
        unsigned addr = sizeof(ipageheader);  /* увеличим уровень инд.*/
        if(headerDATA.level >= MaxDepth) {
          ExecError(ErrIndexSpace);           /* переполнение индекса */
        }
        _move((char*)pagestack,(MaxDepth-1)*sizeof(page),
              (char*)(pagestack+1));         /* сдвигаем стек пути   */

        if( Busy(Index=GetPage(current,tablenum,Xltype)) ) myAbort(104);
        SetModified(0,0);
        newpage = GetIndBlock(tablenum);      /* берем чистый блок    */

        if( Busy(NewIndex=GetPage(newpage,tablenum,Nltype))) myAbort(104);
        SetModified(0,0);

        pagestack[1]=newpage;                  /* копируем ROOT в него*/

        _ZZmov(Index,headerDATA.freeaddr,      /* переносим старый    */
                                      NewIndex);
        header0DATA(NewIndex).page = newpage;  /* восст. # страницы   */

        descrDATA(addr).common   =MaxKeyLeng;  /* формируем шапку     */
        descrDATA(addr).distinct =0;           /* записи,описывающей  */
        descrDATA(addr).RIDnumber=1;           /* конец индекса       */

        addr += ldescr;                        /* занесем адрес блока */
        _GZmov((char*) &newpage, sizeof(page), Index+addr);

        headerDATA.level++;                    /* шапка нового ROOT-бл*/
        headerDATA.freeaddr = addr+sizeof(RID);
        headerDATA.next     = 0;

        Index=NewIndex;                        /* работаем со старым  */
      }

      /* ищем, где разбить   */
      FindSplit();

      /* блокируем здесь, чтоб не наткнуться в дальнейшем */
      if( Busy(Index=GetPage(current,tablenum,Xltype)) ) LockAgent();

      /* ищем в верхнем по дереву блоке */
      wasFound = SearchKey();                 /* т.к.это возврат цикла*/
    }

    /* место есть в leaf-> все, вставляем */
  if(headerDATA.level ==0) break;

    /* нашли место не в leaf -> бьем нижний блок, вставляем запись */
    newpage = GetIndBlock(tablenum);           /* новый блок          */

    /* ATTENTION !!! если получился chain-блок, то его не вставляем !!*/
    if(!wasFound) {                            /* вставляем с ключом  */
      if( Busy(Index=GetPage(current,tablenum,Xltype)) ) myAbort(104);
      _GZmov((char*)&newpage,sizeof(page),     /* старый -> новый     */
              Index+nextkey+ldescr+descrDATA(nextkey).distinct);
      InsKey((RID*)&pagestack[stackindex+1],   /* вставили ключ       */
             inslength,TRUE);
    }

    /* разбиваем нижний блок, повторяем поиск */
    current = pagestack[++stackindex];
    SplitBlock(newpage,current,tablenum);
  }

  /* вставка RID записи в leaf-блок */
  if( Busy(Index=GetPage(current,tablenum,Xltype)) ) LockAgent();
  InsKey(ident,inslength,!wasFound);            /* вставили в leaf    */
}

/*
 * вставка ключа (или только RID)
 * ключ - в KeyBuffer,
 * nextcommon, prevcommon, nextkey заполнены
 */
static void InsKey(link,inslength,withKey) 
        RID* link; int inslength; boolean withKey; {
  unshort  thiskey = nextkey;
  int      keyflag = 0;

  unshort  inspoint;                           /* куда впишем RID     */

  if (withKey) {                               /* вставляем с ключом  */
    int delta = nextcommon - prevcommon;
    keyflag   = 1;
    nextkey   = thiskey + inslength;

    if( delta>0 ) {                            /* есть доп. совпадения*/
      if(delta >= descrDATA(thiskey).distinct) myAbort(100);
      descrDATA(thiskey).common  +=(unchar)delta;/* ->след ключ смещаем */
      descrDATA(thiskey).distinct-=(unchar)delta;/* затираем общую часть*/
      _ZZmov(Index+thiskey,ldescr,Index+thiskey+delta);
    } else {
      delta = 0;
    }
    _ZZmov(Index+thiskey,headerDATA.freeaddr - thiskey,
           Index+nextkey);                     /* раздвигаем индекс   */
    headerDATA.freeaddr += inslength;          /* увелич.занятую часть*/
    nextkey += delta;

    /* формируем новый ключ*/
    descrDATA(thiskey).common   = (unchar) prevcommon;
    descrDATA(thiskey).distinct = (unchar)(delta=keylength-prevcommon);
    descrDATA(thiskey).RIDnumber= 1;
    inspoint = thiskey + ldescr;
    _GZmov(KeyBuffer+prevcommon, delta , Index+inspoint);
    _GZmov((char*)link,sizeof(RID),Index+(inspoint+=delta));

  } else {                                     /* вставляем только RID*/
    RID old;
    int pos  = descrDATA(thiskey).RIDnumber * sizeof(RID);
    inspoint = thiskey + descrDATA(thiskey).distinct+ldescr;
    nextkey  = inspoint + pos;                 /* следующий ключ      */

    if(pos!=0&&uniqflag) ExecError(ErrNotuniq);/* наруш. уникальность */
      
    _ZZmov(Index+nextkey,headerDATA.freeaddr - nextkey,
           Index+nextkey+inslength);           /* раздвигаем индекс   */
    headerDATA.freeaddr += inslength;          /* увелич.занятую часть*/

    if (headerDATA.level == 0) {               /* для leaf-блоков     */
      while (inspoint != nextkey) {            /* ищем, куда вставлять*/
        _ZGmov(inspoint+Index,sizeof(RID),(char*) &old);
      if(cmpRID(link,&old)<0) break;
        inspoint += sizeof(RID);
      }
    }

    _ZZmov(inspoint+Index,nextkey-inspoint,inspoint+sizeof(RID)+Index);
    _GZmov((char*)link,sizeof(RID),inspoint+Index);
    nextkey += sizeof(RID);                    /* вставили в список   */

    /* надо разбить ключ на 2 группы RID */
    if( ++descrDATA(thiskey).RIDnumber >= MaxIndRIDs) {
      zptr RIDarray = thiskey +
                  descrDATA(thiskey).distinct+ldescr + sizeof(RID) +
                  Index;
      _ZZmov(RIDarray,nextkey+Index-RIDarray,RIDarray+ldescr);
      descr0DATA(RIDarray).RIDnumber = descrDATA(thiskey).RIDnumber-1;
      descr0DATA(RIDarray).common    = (unchar) keylength;
      descr0DATA(RIDarray).distinct  = 0;
      descrDATA(thiskey).RIDnumber   = 1;
    }
  }

  /* пометим модифицированными, если при leaf-блок -> кластеризацию */
  if (headerDATA.level == 0) {
    SetModified(keyflag,CountCluster(thiskey,nextkey,inspoint));
  } else {
    SetModified(0,0);
  }
}

/*
 * Вычисление изменения кластеризации после вставки RID, на который
 * указывает "inspoint"
 * this,next - позиции текущего и следующего ключа в блоке,
 * inspoint  - позиция нового RID  в блоке
 */
static int CountCluster(unshort this, unshort next, unshort inspoint) {
  page     prevpage=0, nextpage=0,thispage;
  _ZGmov(Index+inspoint+
      sizeof(RID) - sizeof(page),sizeof(page),(char*) &thispage);

  /* вычислим предыдущий page */
  if(this+descrDATA(this).distinct+ldescr != inspoint) {
    _ZGmov(Index+inspoint-sizeof(page),sizeof(page),(char*)&prevpage);
  } else if (this != sizeof(ipageheader)) {
    _ZGmov(Index+this    -sizeof(page),sizeof(page),(char*)&prevpage);
  }

  /* вычислим следующий page */
  if(inspoint+sizeof(RID) != next) {
    _ZGmov(Index + inspoint + sizeof(RID) +
             sizeof(RID) - sizeof(page), sizeof(page),(char*)&nextpage);
  } else if (headerDATA.freeaddr != next &&
                                   descrDATA(next).RIDnumber != 0) {
    _ZGmov(Index + next + ldescr + descrDATA(next).distinct +
             sizeof(RID) - sizeof(page), sizeof(page),(char*)&nextpage);
  }
  return (thispage==nextpage || thispage==prevpage ? 0 :
                              nextpage != prevpage ? 1 : 2);
}

/*
 * Инициация файла индекса
 */
void InitIndex(table tablenum) {
  unsigned addr = sizeof(ipageheader);
  Index = GetPage((page) 0 , tablenum, Nltype);

  headerDATA.level = 0;
  headerDATA.freeaddr= addr+ldescr;
  headerDATA.next   = 0;
/*headerDATA.prev   = 0;*/

  descrDATA(addr).common   =MaxKeyLeng;         /* формируем шапку   */
  descrDATA(addr).distinct =0;                  /* записи,описывающей*/
  descrDATA(addr).RIDnumber=0;                  /* конец индекса     */

  SetModified(0,0);

  Index = GetPage((page) 1 , tablenum, Nltype);  /* формируем карту   */
  for(addr = sizeof(page) ; addr < BlockSize ; ++addr) byteDATA(addr)=0;
  byteDATA(sizeof(page)) = 0xC0;                 /* 0,1 - заняты      */
  SetModified(0,0);
}


/*
 * Поиск ключа разбиения блока (он становится последним в 1-ой половине)
 * разбиение производится по ключу, непосредственно предшествующему
 * половине заполненной части, но не первому в блоке
 * если в блоке находится единственный ключ, то он блок бьется на 2,
 * причем в старшую половину заносится 3/4 ключей
 * возвращаем флаг разбиения по копии ключа
 */
static boolean FindSplit() {
  zptr next = sizeof(ipageheader) + Index;
  zptr prev;
  zptr end  = (headerDATA.freeaddr+sizeof(ipageheader))/2 + Index;
  int  common    = 0;
  do {
    prev = next;
    keylength   = descr0DATA(prev).distinct;
    /* разбиение по маркеру "конца индекса" невозможно, т.к. он последн.
       и у него только 1 RID */
    common      = descr0DATA(prev).common;
    _ZGmov(prev+ldescr,keylength,KeyBuffer+common);
    next += descr0DATA(prev).RIDnumber*sizeof(RID) + ldescr +
            keylength;
  } while (next<end);
  keylength += common;
  splitbound = next - Index;
  if (splitbound == headerDATA.freeaddr) myAbort(111);
  return(descr0DATA(next).common != MaxKeyLeng &&
         descr0DATA(next).distinct==0);
}

/*
 * разбиение блока Index на 2: NewIndex и Index
 * начальная часть остается в исходном, остаток переносится в новый блок
 * подсчитываем число записей и нарушений кластеризации в новом блоке,
 * и обновляем соответствующую статистику
 */
static void SplitBlock(page newpage, page current, table tablenum) {
  int      common   = 0;
  unshort  addr;
  zptr     newaddr;
  short    nkey     = 0;
  short    ncluster = 0;
  page     prevpage = 0;
  RID      thisRID;
  int      i;

  if( Busy(Index=GetPage(current,tablenum,Xltype)) ) myAbort(105);

  /* для leaf-блоков подсчитаем число ключей икластер. в нижней части */
  if (headerDATA.level == 0) {
    for(addr=splitbound ; addr<headerDATA.freeaddr;) {
      ++nkey;
      for(i = descrDATA(addr).RIDnumber,
          addr += descrDATA(addr).distinct + ldescr;
          i!=0; addr += sizeof(RID), --i) {
        _ZGmov(Index+addr,sizeof(RID),(char*) &thisRID);
        if (thisRID.page != prevpage) {
          prevpage = thisRID.page; ++ncluster;
        }
      }
    }
  }

  /* вычтем перенесенное из "верхнего блока" */
  SetModified(-nkey,-ncluster+2);

  if( Busy(NewIndex=GetPage(newpage,tablenum,Nltype))) myAbort(104);
  /* занесем перенесенное в "нижний блок" */
  SetModified(nkey,ncluster);

  if(headerDATA.freeaddr == splitbound) myAbort(110); /* новый пустой */
  newaddr=sizeof(ipageheader)+NewIndex;
  common=descrDATA(splitbound).common;        /*общую часть пишем явно*/
  _ZZmov(Index+splitbound,ldescr,newaddr);
  descr0DATA(newaddr).common=0;               /*общей части не будет  */
  descr0DATA(newaddr).distinct += (unchar)common; /*а своя удлинится */

  newaddr += ldescr;
  _GZmov(KeyBuffer,common,newaddr);           /* старая "общая часть" */
  newaddr += common;

  _ZZmov(Index+splitbound+ldescr,             /* переносим половинку  */
         headerDATA.freeaddr-splitbound-ldescr,
         newaddr);                            /* в новый блок         */

  header0DATA(NewIndex).freeaddr =
     header0DATA(Index).freeaddr - splitbound+sizeof(ipageheader) +
                                   common;
  header0DATA(Index).freeaddr = splitbound;

  header0DATA(NewIndex).level = header0DATA(Index).level;
  header0DATA(NewIndex).next  = header0DATA(Index).next ;
  header0DATA(   Index).next  = newpage;      /* сцепляем в цепочку   */
}

/*
 * Стирание из индекса
 */
void DelIndex(table tablenum, zptr record, RID* ident, IndexScan* iscan) {
  page     current   = 0;                     /* текущий блок индекса */
  page     prevchain = 0;                     /* предыдущий в цепочке */
  boolean  wasFound;
  zptr     RIDarray;                          /* сканер списка RID    */
  unshort  thiskey;
  int      keyflag = 0;                       /* число стертых ключей */
  int      dcluster;                          /* изменение кластер-ции*/

  FormKey(tablenum,(char*)record,TRUE);       /* формируем ключ       */

  for(;;) {                                   /* ищем до leaf-страницы*/
    if ( Busy(Index = GetPage(current,tablenum,Sltype)) ) LockAgent();
    wasFound = SearchKey();                   /* ищем внутри страницы */

  if (headerDATA.level ==0 ) break;           /* leaf-страница        */

    _ZGmov(nextkey+Index+descrDATA(nextkey).distinct + ldescr,
             sizeof(page),(char*)&current);   /* берем нижний блок    */
  }
  if(!wasFound) myAbort(109);                 /* ключ не нашли ?!     */

  /* ищем ключ в списке RID */
  for(;;) {
    int count;
    for(count=descrDATA(nextkey).RIDnumber,
        RIDarray = descrDATA(nextkey).distinct + ldescr +nextkey+Index;
        --count >= 0; RIDarray += sizeof(RID)) {
      RID old;
      _ZGmov(RIDarray,sizeof(RID),(char*) &old);
    if(cmpRID(&old,ident) == 0) break;
    }

  if(count>=0) break;                         /* нашли нужный RID ?   */

    nextkey = RIDarray-Index;
    if(nextkey == headerDATA.freeaddr) {      /* блок кончился        */
      prevchain = current;                    /* запомним цепочку     */
      if((current=headerDATA.next) == 0) myAbort(118);
      if( Busy(Index = GetPage(current,tablenum,Sltype)) ) LockAgent();
      nextkey=sizeof(ipageheader);            /* начнем с начала блока*/

      /* сравним ключи        */
      if(descrDATA(nextkey).distinct != keylength ||
         cmpsZG(Index+nextkey+ldescr,KeyBuffer,keylength) != 0) myAbort(118);
    } else {                                  /* блок не кончился     */
                                              /* -> продолжение ключа */
      if(descrDATA(nextkey).common   != keylength) myAbort(119);
      if(descrDATA(nextkey).distinct != 0        ) myAbort(119);
    }
  }

  /* уничтожаем RID */
  if ( Busy(Index = GetPage(current,tablenum,Xltype)) ) LockAgent();
  thiskey = nextkey;
  nextkey+= ldescr+descrDATA(thiskey).distinct+
                   descrDATA(thiskey).RIDnumber*sizeof(RID);

  dcluster = CountCluster(thiskey,nextkey,RIDarray-Index);

  if ( --descrDATA(thiskey).RIDnumber != 0) {  /* не последний в ключе*/
    headerDATA.freeaddr -= sizeof(RID);
    _ZZmov(RIDarray + sizeof(RID),             /* затираем его        */
           headerDATA.freeaddr - (RIDarray-Index), RIDarray);
    if(iscan != NIL) --iscan->RIDscan.iKey;    /* отступим в сканере  */

    /* последний в ключе, но не последний в блоке */
  } else if(RIDarray - Index + sizeof(RID) != headerDATA.freeaddr) {
    int      oldcommon = descrDATA(thiskey).common;
    int      oldnexdist= descrDATA(nextkey).distinct;
    int      delta;
    if ( (delta = descrDATA(nextkey).common) == MaxKeyLeng) {
      delta = 0; oldnexdist = MaxKeyLeng;     /* за нами конец индекса*/
    }

    /* если за нами - другой и перед нами - другой мы не в chain */
    if (oldnexdist && descrDATA(thiskey).distinct && prevchain ==0) {
      keyflag = -1;                           /*стираем ключ полностью*/
    }

    if ( (delta -= oldcommon) >= 0) {         /* надо слить ключи     */
      _ZZmov(nextkey + Index,ldescr,nextkey-delta + Index);
      nextkey -= delta;                       /* расширяем следующий  */
      _GZmov(KeyBuffer+oldcommon,delta,nextkey + ldescr + Index);
      descrDATA(nextkey).distinct += (unchar) delta;
      descrDATA(nextkey).common   -= (unchar) delta;
    }
    _ZZmov(nextkey+Index,headerDATA.freeaddr - nextkey, thiskey+Index);
    headerDATA.freeaddr -= nextkey - thiskey;

    /* если бежим, сканируя по этому индексу*/
    if(iscan != NIL) {
      /* встанем на начало нового ключа */
      if(--iscan->RIDscan.iKey != 0) myAbort(117);
      /* если этот ключ - другой -> занесем флажок */
      iscan->RIDscan.iKey = oldnexdist ? 0xFF : 0x00;
    }

    /* стерли последний RID в единственном ключе Chain-блока */
  } else if(thiskey == sizeof(ipageheader) && prevchain != 0) {
                                              /* будем отдавать блок  */
    page next = headerDATA.next;              /* # следующего         */
    if(Busy(GetPage((page)1,tablenum,Xltype)) ) LockAgent();
    /* поменяем сцепку Chain в предыдущем */
    if ( Busy(Index = GetPage(prevchain,tablenum,Xltype)) ) LockAgent();
    RlseIndBlock(tablenum,current);
    Index = GetPage(prevchain,tablenum,Xltype);
    headerDATA.next = next;                   /* зачитали предыдущий  */
                                              /* в него и dcluster    */
    if (iscan != NIL) {                       /* модифицируем сканер  */
      iscan->RIDscan.iKey = 0;
      iscan->shift        = 0;
      iscan->page         = next;
    }

    /* стерли последний RID в последнем ключе блока */
  } else {
    headerDATA.freeaddr -= sizeof(RID);
    if (descrDATA(thiskey).distinct == 0) {   /*такой ключ есть сверху*/
      headerDATA.freeaddr -= ldescr;
    }
    if (iscan != NIL) {                       /* почитаем след. блок  */
      iscan->RIDscan.iKey = 0;
      iscan->shift        = 0;
      iscan->page         = headerDATA.next;
    }
  }

  SetModified(keyflag,-dcluster);
}

/*
 * Изменение RID-а при Update, сканирующем данный индекс, в случае
 *                             переноса записи в другую страницу
 */
void UpdIndex(iscan,new) IndexScan* iscan; RID* new; {
  RID     temp;
  zptr    ptr,pold,pend;
  int     retcode;
  int     dcluster;

  /* читаем наш блок индекса */
  if(Busy(Index=GetPage(iscan->page,iscan->RIDscan.scantable,Xltype)))
                                                           LockAgent();
  ptr = Index + iscan->shift; ptr += descr0DATA(ptr).distinct+ldescr;

  pold= ptr + (iscan->RIDscan.iKey-1)*sizeof(RID);
  pend= ptr + descrDATA(iscan->shift).RIDnumber*sizeof(RID);

  dcluster = -CountCluster(iscan->shift,pend-Index,pold-Index);

  _ZGmov(pold, sizeof(RID), (char*) &temp); /* переносим текущий      */
  if(cmpRID(&temp,&iscan->RIDscan.ident) != 0) myAbort(113);
  if(iscan->RIDscan.ident.page < new->page){/* перенос вниз           */
    ptr = pold;
    while((ptr += sizeof(RID)) < pend) {    /* идем вперед, ищем место*/
      _ZGmov(ptr,sizeof(RID),(char*)&temp);
    if((retcode=cmpRID(&temp,new))>0) break;
      if(retcode == 0) myAbort(112);
    }
    _ZZmov(pold+sizeof(RID),(ptr-=sizeof(RID))-pold,pold);
    --iscan->RIDscan.iKey;                  /* откатим сканер         */
  } else {                                  /* перенос назад,ищем куда*/
    while(ptr<pold) {
      _ZGmov(ptr,sizeof(RID),(char*)&temp);
    if((retcode=cmpRID(&temp,new))<0) break;
      if(retcode == 0) myAbort(112);
      ptr += sizeof(RID);
    }
    _ZZmov(ptr,pold-ptr,ptr+sizeof(RID));   /* сдвигаем всех вперед   */
  }
  _GZmov((char*)new,sizeof(RID),ptr);       /* заносим новый RID      */
  dcluster += CountCluster(iscan->shift,pend-Index,ptr-Index);
  SetModified(0,dcluster);
}

/*
 * Сравнение RID-ов
 */
static int cmpRID(x,y) RID* x; RID* y; {
  int retcode;
  if( (retcode=x->table - y->table) == 0) {
    if (x->page == y->page) {
      retcode = x->index - y->index;
    } else {
      retcode = (x->page >= y->page ? 1 : -1);
    }
  }
  return(retcode);
}

/*
 * Выделение нового блока индекса
 */
static page GetIndBlock(table tablenum) {
  unsigned scanner,i;
  unchar   mask;
  zptr     indmap;
  if(Busy(indmap=GetPage((page)1,tablenum,Xltype))) myAbort(106);
  for(scanner = sizeof(page) ; byte0DATA(indmap+scanner)==0xFF;) {
    if(++scanner>=BlockSize) ExecError(ErrIndexSpace);
  }
  for(i=0, mask= 0x80 ; byte0DATA(scanner+indmap) & mask ;
         mask = mask>>1 , i++);
  byte0DATA(indmap+scanner) |= mask;
  SetModified(0,0);
  return((page) ((scanner-sizeof(page))* 8 + i));
}

/*
 * Освобождение блока индекса
 */
static void RlseIndBlock(table tablenum, page block) {
  unchar  mask    = (unchar) (0x80 >> (block % 8));
  unshort scanner = block / 8 + sizeof(page);
  zptr    indmap;
  if(scanner>=BlockSize) myAbort(107);
  if( Busy(indmap=GetPage((page)1,tablenum,Xltype)) ) myAbort(106);
  indmap += scanner;
  if(! (byte0DATA(indmap) & mask)) myAbort(107);
  byte0DATA(indmap) &= ~ mask;
  SetModified(0,0);
}
