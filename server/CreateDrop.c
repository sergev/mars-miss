                    /* ============================ *\
                    ** ==         M.A.R.S.       == **
                    ** == Создание / уничтожение == **
                    ** ==    таблиц и индексов   == **
                    \* ============================ */
/* !!! Для администратора позволить CREATE TABLE username.tabname (..)*/

#include "FilesDef"
#include "AgentDef"
#include "TreeDef"
#include "CacheDef"                          /* ради обнуления FileLab*/

         short    TabCreate(void);
         short    TabDrop  (void);

  /* обращения к swapping-системе       */
  extern void     LockCode(table,int,boolean);

  /* обращения к диспетчеру             */
  extern void     LockAgent(void);           /* передиспетчеризация   */
  extern void     ExecError(int);            /* ошибка с rollback-ом  */

  /* обращения к в/выводу нижнего уровня*/
  extern zptr     GetPage(page,table,short); /* доступ к блоку        */
  extern boolean  LockTable(table,short);    /* блокировка таблицы    */
  extern boolean  LockElem(lockblock*,short,boolean);
  extern void     SetModified(short,short);  /* отметка блока: модиф-н*/

  extern void     DropFile (table);          /* стирание блоков файла */

  /* обращения к манипулятору индексов  */
  extern void     InitIndex(table);          /* инициац. файла индекса*/
  extern void     InitLDMap(table,page);

  /* работа с каталогом базы */
  extern int      LookDir(char*,table);      /*поиск по каталогу базы */

  static zptr     Index;

  static table    CreateFile(table);

/*
 * Создание таблицы, индекса
 *
 *  ищем, захватываем (если индекс, то и таблицу тоже)
 *  (если есть ->
 *    не захвачен -> ошибка, захвачен -> встаем в очередь, повтор
 *  нет ->
 *    заносим тип, имя, флаг "создано", флаг "новый"
 *    создаем 0-блок, 1-блок (описатели структуры)
 *  если)
 * Rollback - освобождаем все black-lock (ну это и так должно сработать)
 *            освобождаем элемент
 * Commit   - снимаем бит "создано"
 */

short TabCreate() {
  file_elem* pFile;
  table      newFile;
  char*      pscan    = sizeof(struct parseheader) + ParseBuf;
  short      objmode  = *(short*) pscan;
  char*      objname  = (pscan += sizeof(short));
  table      mainfile = NoFile;


  /* проверим права */
  if (! (Agent->privil & (Priv_Create | Priv_Auth))) {
    ErrCode = ErrRights; return(-1);         /* не имеешь права !     */
  }

  objname = pscan;
  if (objmode) {                             /* создание индекса      */
    int tablenum;

    /* ищем егойную таблицу */
    if((tablenum=LookDir(pscan+=MaxIndxName,NoFile)) < 0) return(-1);
    mainfile = (table) tablenum;

    if (! (Agent->privil & Priv_Auth) &&     /* создавать может ADMIN */
           Agent->userid != Files[mainfile].owner) {
      ErrCode = ErrRights; return(-1);       /* или владелец таблицы  */
    }
    if (LockTable(mainfile,Oltype)) LockAgent(); /* таблицу блокируем */
    if ( Busy(Index=GetPage((page)1, mainfile, Sltype)) ) myAbort(203);
  }

  /* ищем такой же об'ект (проверка на дублирование) */
  { int found;
    if ( (found = LookDir(objname,mainfile)) >= 0) {
      if (LockElem(&Files[found].iR,Sltype,FALSE)) LockAgent();
      ErrCode = ErrExists;
      return(-1);                              /*такая уже есть         */
    }
    if (ErrCode != ErrNoExists) return(-1);
  }
  pscan += MaxFileName+UserName;

  newFile = CreateFile(mainfile);
  pFile   = &Files[newFile];

/*if (LockElem (&ptr->iM, Sltype, TRUE) ) myAbort(202);*/

  if (objmode == 0) {                        /* создаем таблицу       */
    short inpage = sizeof(short);            /* адрес в блоке         */
    short space  = *(short*) pscan;          /* и длина описателя     */

    pscan       += sizeof(short);

    pFile->kind = tableKind;
    MVS(objname,MaxFileName,pFile->b.tablename);


        /* создаем карту в 0-ом блоке */
    {
      short i      = sizeof(page);
      Index = GetPage( (page) 0, newFile, Nltype);
      intDATA(i) = 0;                        /* page #1 is busy       */
      while( (i += sizeof(page) ) < BlockSize) intDATA(i) = BlockSize;
      SetModified(0,0);
    }

      /* создаем описатель таблицы в 1-ом блоке */
    Index = GetPage( (page)1, newFile, Nltype);
    _GZmov( pscan, space, Index + inpage);  /* перенесем описатели    */
    intDATA(space+inpage) = 0;              /* список доступа         */
    SetModified(0,0);

    /* create long-field files */
    { dsc_table*  tabDescr = (dsc_table*)pscan;
      dsc_column* colDescr = (dsc_column*)((char*)tabDescr + tabDescr->totalleng);
      short i;
      for(i=0;i<tabDescr->Ncolumns;++i) {
        if(colDescr->type == Tldata) {
          table dataFile = CreateFile(newFile);
          file_elem* pData = Files + dataFile;
          pData->kind = ldataKind;
          pData->b.ldatadescr.shift = colDescr->shift;
          InitLDMap(dataFile,0);
        }
        colDescr = (dsc_column*)((char*)colDescr + colDescr->totalleng);
      }
    }

  } else {                                  /* создание индекса       */
    boolean uniqueflag = *(short*)pscan;
    short** colarray =(short**) (pscan += sizeof(short));
    int     Ncolumns = DescTable(sizeof(page)).Ncolumns;
    int     colnum;
    int     keyleng = 0;
    indcolumn* inddescr = pFile->b.indexdescr.columns;
    datatype   type;

    pFile->kind = indexKind;
    MVS(objname,MaxIndxName,pFile->b.indexdescr.indexname);

    if (uniqueflag) pFile->recordnum = UniqIndexFlag;
    pFile->b.indexdescr.clustercount = 0;

    /* цикл по колонкам, входящим в индекс */
    for (colnum=0; colnum< MaxIndCol && colarray[colnum] != NIL; ++colnum) {
      short*     descr0 = colarray[colnum];
      char*      descr  = (char*) (descr0 + 2);
      unshort    index  = sizeof(page);
      int i=0,j;

      do {                                    /* поиск в описателе    */
        index += intDATA(index);
        if (i++ == Ncolumns) {                /* колонки кончились    */
          ErrPosition = *descr0; ExecError(ErrUndefined);
        }
        for(j = *descr+1; --j>=0 ;) {         /* сравниваем имена     */
          if ( DescColumn(index).name[j] != descr[j] ) break;
        }
      } while (j>=0);

      /* VarChar данные не могут входить в индекс           */
      /* а, может, разрешить последней колонке быть VARCHAR?*/
      if( (type = DescColumn(index).type) == Tvar) {
         if(colnum+1 < MaxIndCol && colarray[colnum+1] != NIL) {
           ErrPosition = *descr0; ExecError(ErrType);
          }
      } else if( type == Tldata){ /*LongData данные не могут входить в индекс*/
         ErrPosition = *descr0; ExecError(ErrType);
      }
      inddescr->shift   = DescColumn(index).shift;
      keyleng += (inddescr->length = DescColumn(index).length);
      inddescr->numFlag = (type < Tchar);
      inddescr->flvrFlag = (type == Tfloat || type == Tvar);
      inddescr->invFlag = descr0[1];
      ++inddescr;
    }
    /* длина ключа не может быть больше Max */
    if(keyleng >= MaxKeyLeng) ExecError(ErrLeng);

    /* расписываем неиспольз-ные поля в описателе индекса в каталоге  */
    while(colnum++ < MaxIndCol) {inddescr->shift = -1;  ++inddescr;}
    InitIndex(newFile);

    /* формируем программу для записи индекса */
    {
      dsc_table*   Fields = (dsc_table*) (ParseHeader+1);
      dsc_table*   descr  = Fields + 1;
      node Node           = (node)       (descr+1);

      ParseHeader->inpparams  = Fields;
      ParseHeader->Node       = Node;
      ParseHeader->filelist   = (table*) (Node + 1);
      ParseHeader->eqlist     = NIL;

      ParseHeader->filelist[0]= 1;
      ParseHeader->filelist[1]= mainfile;

      Fields->totalleng = sizeof(dsc_table);
      Fields->Ncolumns  = 0;

      /* MarkC error */
/*    *descr = *(dsc_table*)&zget(Index+sizeof(page)); */

      /* переносим описатель таблицы в дерево кода */
      _ZGmov(Index+sizeof(page),sizeof(dsc_table),(char*)descr);

      Node->mode            = CRINDEX;
      Node->upper           = NIL;
      Node->name            = NIL;
      Node->doBefore = Node->doAfter = NIL;
      Node->projlist        = NIL;
      Node->body.C.tablenum = mainfile;
      Node->body.C.index    = newFile;
      Node->body.C.tabdescr = descr;
    }
    return(100);                    /* чтобы запустил трансляцию      */
  }
  return(0);

}

static table CreateFile(table mainfile) {
  table      newFile = 0;
  file_elem* pFile;
  while(newFile < MaxFiles && (Files[newFile].status & BsyFile)) ++newFile;
  if(newFile >= MaxFiles) ExecError(ErrFull);

  pFile = &Files[newFile];

  pFile->status = BsyFile | CreFile;

  FileLabels[newFile] = 0;
  pFile->recordnum  = 0;
  pFile->size       = 0;
  pFile->actualsize = 0;

  pFile->mainfile  = mainfile;
  pFile->owner     = mainfile == NoFile ? Agent->userid :
                                          Files[mainfile].owner;
  /* вставляем в цепочку индексов/ldata */
  if(mainfile != NoFile) {
    file_elem* scan = &Files[mainfile];
    while (scan->nextfile != NoFile) scan= &Files[scan->nextfile];
    scan->nextfile = (table)newFile;
  }
  pFile->nextfile  = NoFile;

  clearlock(pFile->iR); clearlock(pFile->iM); clearlock(pFile->iW);
  if (LockTable(newFile, Oltype) ) myAbort(201);

  return(newFile);
}

/*
 * Уничтожение таблицы, индекса
 * ищем, захватываем (и таблицу для инд), ставим бит "стерто".
 * Rollback - снимаем бит "стерто"
 * Commit   - вытираем все black-lock на этот файл (с отдачей страниц)
 *            в таблицу ставим "стерто" и чистим имя
 *            если бит "новый"  -> освобождаем и статью
 * если DROP на таблицу, то сначала захватываем ее, а потом
 * DROP-ем все ее индексы, а только потом - ее саму.
 */

short TabDrop() {
  file_elem* ptr;
  table      delFile;
  table      mainfile = NoFile;

  char* pscan   = sizeof(struct parseheader) + ParseBuf;
  short IsIndex = *(short*) pscan;
  pscan += sizeof(short);

  if (IsIndex) {                             /* стирание индекса      */
    int tablenum;
    /* ищем таблицу */
    if ((tablenum=LookDir(pscan+MaxIndxName,NoFile))<0) return(-1);
    mainfile = (table)tablenum;

    if (! (Agent->privil & Priv_Auth) &&     /* вытирать  может ADMIN */
           Agent->userid != Files[mainfile].owner) {
      ErrCode = ErrRights; return(-1);       /* или владелец таблицы  */
    }
    if(LockTable(mainfile,Oltype)) LockAgent();
  }

  /* ищем об'ект  */
  { int index;
    if ( (index = LookDir(pscan,mainfile)) < 0) return(-1);
    delFile = (table) index;
  }

  ptr = &Files[delFile];
  if(!IsIndex) {                             /* это таблица           */
    if (! (Agent->privil & Priv_Auth) &&     /* вытирать  может ADMIN */
           Agent->userid != ptr->owner) {
      ErrCode = ErrRights; return(-1);       /* или владелец таблицы  */
    }
  }

  /*подождем конца работы ?  */
  if(LockTable(delFile,Oltype)) LockAgent();

  if(!IsIndex) {                            /* это таблица           */
    table index1;
    file_elem* ptr1;

    for(index1 = ptr->nextfile; index1 != NoFile;
        index1 = ptr1->nextfile) {          /* стираем все индексы   */
      ptr1 = &Files[index1];
      if(LockTable(index1,Oltype)) myAbort(200);
      if (ptr1->status & CreFile) {          /*надо удалять из очереди*/
        DropFile(index1);
      } else {
        ptr1->status |= DrpFile;
      }
    }
  }

  /* уничтожаем все свои коды, ссылающиеся на эту таблицу */
  LockCode(IsIndex ? mainfile : delFile, -1 , TRUE);

  if (ptr->status & CreFile) {               /*надо удалять из очереди*/
    DropFile(delFile);                       /*индексов к таблице     */
  } else {                                   /*нет, подождем "Commit" */
    ptr->status |= DrpFile;                  /*и все сотрем тогда     */
  }
  return(1);
}
