                    /* ============================ *\
                    ** ==         M.A.R.S.       == **
                    ** ==   Привязка запроса к   == **
                    ** == таблицам и оптимизация == **
                    \* ============================ */

/*--------------------------------------------------------------------*\
                     Алгоритм работы:
    Чтобы не зависеть от чужих Create/Drop:
      Захват модифицируемой таблицы на чтение
      Захват всех таблиц, используемых для чтения
        проверка, что модифицируемая таблица не используется

    Изменение структуры узла таблицы с "N" на "T"

    Чтение описателей таблиц, поиск и проставление атрибутов
        используемых колонок, проверка прав доступа

    Проставление, проверка и преобразование типов скалярных выражений
        для столбцов сортировки Tvar преобразуем в Tchar
        для сравнения строк делаем Ecolumn (если есть) 1-ым операндом
    Разбиение всех предикатов Select,Join на AND - список,
        опускание предикатов на таблицу

    Для Update формирование списка модифицируемых полей
    Для Update, Insert, InsValue занесение в location смещений полей
        в записи таблицы.
        Соответствующее  переупорядочивание вычислений Varchar

    Перебор возможных путей доступа с выбором оптимального
\*--------------------------------------------------------------------*/


#include "FilesDef"
#include "AgentDef"
#include "TreeDef"
#include "BuiltinDef"

          int          LinkTree(void);
          short        TabLock (void);

   extern char* mfar   GetMem (int);

   extern zptr         GetPage(page,table,short);
   extern int          LookDir(char*,table);
   extern void         LockAgent(void);
   extern boolean      LockElem (lockblock*,short,boolean);
   extern boolean      LockTable(table,short);
   extern void         ExecError(int);
   extern boolean      ModIndex(table,short*);
   extern table        GetNxIndex(table);

typedef struct {                    /* описатель характеристик узла:  */
  long   resource;                  /* использование ресурсов (I/O)   */
  long   recordnum;                 /* примерное число записей        */
} cost;

   static void         NodeTypeCheck(node);
   static void         ExprTypeCheck(expr);
   static void         LinkColumns  (node);
   static void         TypeErr      (expr);
   static expr         TypeConv     (expr,datatype,int);
   static void         ANDsplit     (exprlist*);
   static void         ChckPriv     (table,zptr,unsigned);


int LinkTree() {
  node    Tablelist = ParseHeader->tablelist;
  node    Node      = ParseHeader->Node;/* главный запрос             */
  short   ntables   = 0;                /* число используемых табл.   */
  node    tscan,next;
  node*   prev;
  int     tablenum;
  short   ModTable  = -1;           /* модифицируемая таблица         */
  short*  ModArray  = NIL;          /* список смещений модиф. полей   */

  if( (ErrCode = setjmp(err_jump)) != 0) return(-1);


  /* проходим по списку таблиц, захватываем все на чтение          */
  for(tscan = Tablelist ; tscan != NIL ; tscan = tscan->body.N.next) {
    table subfile;
    if( (tablenum =
           LookDir(tscan->body.N.tablename,NoFile)) < 0) return(-1);
    subfile = (table)tablenum;
    /* lock table and all ldata-files */
    do {
      if(LockElem(&Files[subfile].iR,Sltype,TRUE)) LockAgent();
      do {
        subfile = Files[subfile].nextfile;
      } while (subfile != NoFile && Files[subfile].kind != ldataKind);
    } while (subfile != NoFile);
    tscan->body.N.tablenum      = (table) tablenum;
    ++ntables;
  }

  /* захватываем таблицу для INSERT (ее нет в TABLELIST)  */
  if(Node->mode == INSERT || Node->mode == INSVALUE) {
    if( (tablenum=
         LookDir(Node->body.I.tablename,NoFile)) < 0) return(-1);
    if(LockElem(&Files[tablenum].iR,Sltype,TRUE)) LockAgent();
    Node->body.I.tablenum = (table) tablenum;
    ++ntables;
  }


  /* формируем буфер под идентификаторы используемых таблиц */
  ParseHeader->filelist    = (table*) GetMem((ntables+1)*sizeof(table));
  ParseHeader->filelist[0] = (table)  ntables;
  { int i;
    for(i=1;i<=ntables;++i) ParseHeader->filelist[i] = NoFile;
  }

  /* изменяем структуру узла-таблицы с "N" на "T" или "A"*/
  prev = &Tablelist;
  for(tscan = Tablelist; tscan != NIL ; tscan = next) {
    short spflag = tscan->body.N.spflag;
    tablenum = tscan->body.N.tablenum;
    next = tscan->body.N.next;
    if (spflag == 0) {                    /* доступ к данным таблицы  */
      tscan->body.T.tablenum      = (table) tablenum;
      tscan->body.T.predlist      = NIL;
      tscan->body.T.pathpred      = NIL;
      tscan->body.T.path          = (table) tablenum;
      *prev = tscan; prev = &tscan->body.T.next;
    } else {                              /* доступ к атрибутам       */
      zptr Index;                         /* проверим разрешение      */
      if(Busy(Index=GetPage((page)1,(table)tablenum,Sltype)))
             myAbort(88);
      ChckPriv((table)tablenum,Index,1<<Priv_read);
      tscan->body.A.tablenum      = (table) tablenum;
      tscan->body.A.regime        = spflag;
      tscan->mode                 = ATTRIBUTES;
    }
    ParseHeader->filelist[ntables--] = (table) tablenum;
  }
  *prev = NIL;

  /* зачитываем описание всех таблиц, проставляем атрибуты столбцов */
  for(tscan = Tablelist; tscan != NIL ; tscan = tscan->body.T.next) {
    LinkColumns(tscan);
  }

  /* проставим и проверим скалярные типы */
  NodeTypeCheck(Node);

  /* записываем операции модиф-ия */
  if (Node->mode >= MODIFICATION) LinkColumns(Node);

  /* проверим, что модифицируемая таблица не используется еще где-то */
  switch(Node->mode) {
  case INSERT:                         /* для INSVALUE - можно       */
    ModTable = Node->body.I.tablenum;
  case INSVALUE:
    ParseHeader->filelist[ntables--] = Node->body.I.tablenum;
  break; case UPDATE:                   /* сделаем   список индексов  */
    {
      projection mscan;
      short  Nmodified = 0;
      short* modarray;

      /* считаем модифицируемые столбцы */
      forlist(mscan,Node->projlist) if(mscan->name != NIL) ++Nmodified;

      /* формируем список модифицируемых полей */
      Node->body.M.fieldlist = ModArray = modarray =
                        (short*) GetMem( (Nmodified+1)*sizeof(short) );
      *modarray++ = Nmodified;         /* число модифицируемых полей  */

      /* заносим смещения модифицируемых полей */
      forlist(mscan,Node->projlist) {
        if(mscan->name != NIL) *modarray++ = mscan->location;
      }
    }
  case DELETE:
    ModTable = Node->body.M.subq->body.T.tablenum;
  }
  if (ModTable >= 0) {                  /* есть модифицируемая таблица*/
    int iscan = *ParseHeader->filelist;
    int count = 0;
    while(iscan!=0) if(ParseHeader->filelist[iscan--] == ModTable) ++count;
    if(count != 1) longjmp(err_jump,ErrModified);
  }

  if(ntables != 0) myAbort(8888);

  return(0);
}


/*
 * Связывание имен столбцов, для модификаций - генерация операции
 *                                             записи в таблицу
 */
static void LinkColumns(tscan) node tscan; {
  boolean     modflag = TRUE;
  projection  pscan;
  int         Ncolumns;
  int         Xcolumn = 0;
  table       tablenum;
  zptr        Index;
  unsigned    index = sizeof(page);
  int         maxposit  = -1;                 /* макс. # позиционного */
  privmode    accrights = Priv_del;           /* права доступа        */
  dsc_table** ptabdescr = NIL;

  switch(tscan->mode) {
  default:
    myAbort(8887);
  case TABLE:
    tablenum = tscan->body.T.tablenum;
    accrights = Priv_read;
    modflag   = FALSE;
  break; case UPDATE:
    accrights = Priv_upd;
    ptabdescr = &tscan->body.M.tabdescr;
  case DELETE:
    tablenum = tscan->body.M.subq->body.T.tablenum;
  break; case INSERT: case INSVALUE:
    accrights = Priv_ins;
    tablenum  = tscan->body.I.tablenum;
    ptabdescr = &tscan->body.I.tabdescr;
  }

  /* зачитываем описатель таблицы - мы его уже S-локнули */
  if(Busy(Index = GetPage((page)1, tablenum, Sltype))) myAbort(88);
  Ncolumns = DescTable(index).Ncolumns;

  if(ptabdescr != NIL) {                      /* переносим описание   */
    _ZGmov(Index+index, sizeof(dsc_table),
          (char*)(*ptabdescr = (dsc_table*) GetMem(sizeof(dsc_table))));
  }
  ChckPriv((table)tablenum,Index,1<<accrights);
  if (tscan->mode == DELETE) return;

  /* проверка типов столбцов, на которые ссылаются в запросе */
  forlist(pscan,tscan->projlist) {
    expr     value = pscan->expr;             /* выражение проекции   */
    datatype type;
    int      length;
    int i = -1;
    index = sizeof(page);
    do {                                      /* поиск в описателе    */
      index += intDATA(index);
      if (++i == Ncolumns) {                  /* колонки кончились    */
        ErrPosition = value->source ;
        longjmp(err_jump,ErrUndefined);
      }
      /* позиционное задание ? */
    if(pscan->name[1]=='*' && pscan->name[2]==i) break;

    } while (cmpsZG(Index+index+offsetof(dsc_column,name),
                    pscan->name,*pscan->name+1) != 0);

    if(pscan->name[1] == '*') {               /* макс. # позиционного */
      maxposit = i;
    } else if (maxposit>= i) {                /* уже был в позиционных*/
      ErrPosition = value->source;
      longjmp(err_jump,ErrDoubleDef);
    }
/*  pscan->order = i;*/
    type   = DescColumn(index).type;
    length = DescColumn(index).length;

    if (modflag) {                     /* проверим соответствие типов */
      if(value->type == Tldata) {
      } else if(value->type >= Tchar) {/* текстовые присваиваем по дл */
        if(type < Tchar) TypeErr(value);
        if(length != value->leng ||    /* надо преобразовать          */
           value->type != Tchar || type != Tchar) {
          pscan->expr = value = TypeConv(value,type,length);
        }
      } else {                         /* числовые преобразуем        */
        if(type >= Tchar) TypeErr(value);
        if(type != value->type) {      /* если надо                   */
          pscan->expr = value = TypeConv(value,type,length);
        }
      }
      if(value->type != type || value->leng != length) {
        TypeErr(value);
      }
      pscan->location = DescColumn(index).shift;

    } else {                           /* при выборке заносим позиции */

      value->type = type;              /* и атрибуты столбцов         */
      value->leng = length;
/*    value->body.column.table = (table) tablenum; */
      value->body.column.shift = DescColumn(index).shift;
    }
    Xcolumn++;
  }

  /*
   * Добавление в список SET операция присваивания для немодифицируемых
   * полей: SET ...., fieldn = fieldn, fieldm = fieldm, ...
   */
  switch (tscan->mode) {
  case UPDATE: {
    node subNode = tscan->body.M.subq;
    /* ищем незатронутые модификацией колонки */
    for(index = sizeof(page), Xcolumn = 0;
        index += intDATA(index), Xcolumn < Ncolumns; ++Xcolumn) {
      for(pscan = tscan->projlist ;
          pscan != NIL && pscan->location != DescColumn(index).shift;
          pscan = pscan->next) {
      }

      if( pscan == NIL) {                     /* колонка не описана   */
        expr result;                          /* формируем ее сами    */
        pscan = (projection) GetMem ( sizeof(struct projection));
        pscan->next     = tscan->projlist; tscan->projlist=pscan;
        pscan->owner    = tscan;
        pscan->name     = NIL;
        pscan->location = DescColumn(index).shift;
        pscan->expr     = result = (expr) GetMem ( sizeof(struct expr));

        result->type = DescColumn(index).type;
        result->leng = DescColumn(index).length;
        result->mode = Eproj;

        /* ищем такую проекцию среди имеющихся */
        for(pscan = subNode->projlist;
            pscan != NIL &&
            pscan->expr->body.column.shift != DescColumn(index).shift;
            pscan = pscan->next);

        if(pscan != NIL) {                    /* нашли                */
          result->body.projection = pscan;

        } else {                              /* не нашли -> делаем   */
          result->body.projection = pscan =
                  (projection) GetMem ( sizeof(struct projection));
          pscan->next = subNode->projlist;
          pscan->owner= subNode;
          subNode->projlist = pscan;
          pscan->expr = result = (expr) GetMem ( sizeof(struct expr));
          pscan->location = Cnil;
          pscan->name     = NIL;

          result->mode = Ecolumn;
          result->type = DescColumn(index).type;
          result->leng = DescColumn(index).length;
          result->body.column.shift = DescColumn(index).shift;
/*        result->body.column.table = (table) tablenum;*/
          result->body.column.specq = 0;
        }
      }
    }

  } break; case INSERT: case INSVALUE:  /* все ли поля определены     */
    if(Xcolumn != Ncolumns) longjmp(err_jump,ErrColNum);
  }

  /* упорядочим вычисления varchar для UPDATE, INSERT */
  if(modflag && tscan->projlist != NIL) {
    projection* chain;
    projection  this;
    pscan = NIL;                        /* новый список varchar       */
    for(;;) {                           /* пока есть varchar          */
      for(chain = &tscan->projlist;
          (this = *chain) != NIL && this->expr->type != Tvar;
          chain = &this->next) {
      }

    if (this == NIL) break;             /* varchar больше нет ?       */

      *chain = this->next;              /* убираем из первоначального */
      for( chain = &pscan ;             /* вставляем в свой список    */
          *chain!=NIL && (*chain)->location >= this->location;
           chain = &(*chain)->next) {   /* упорядоченно по shift(убыв)*/
      }
      this->next = *chain; *chain = this;
    }
    *chain = pscan;                     /*вписываем наш список в хвост*/
    /* т.к. у INSERT тот же Projlist, что и у подзапроса, то запишем
      новый projlist и в него */
    if(tscan->mode == INSERT) {
      tscan->body.I.subq->projlist = tscan->projlist;
    }
  }
}

/*
 * Проверка типов выражений в запросе и всех его подзапросах
 */
static recursive void NodeTypeCheck(Node) node Node; {
  projection proj;
  nodelist list;
  static datatype typebuf[MaxProj];
  static short    lengbuf[MaxProj];
  static unsigned Nproj;
  static unsigned Minimum;

  switch (Node->mode) {
  case SELECT:
    NodeTypeCheck(Node->body.S.subq);
    if(Node->body.S.predlist) {
      ExprTypeCheck(Node->body.S.predlist->expr);
      ANDsplit(&Node->body.S.predlist);
    }
  break; case UNION :

    /* проверим подузлы */
    forlist(list,Node->body.U.subqs) NodeTypeCheck(list->node);

    /* проверим совместимость типов */
    Minimum = 0;
    forlist(list,Node->body.U.subqs) {
      for(Nproj=0, proj = list->node->projlist; proj != NIL;
          ++Nproj, proj = proj->next) {
       if(Nproj>=MaxProj) {
         ErrPosition = proj->expr->source;
         longjmp(err_jump,ErrComplicated);
       }
       typebuf[Nproj]=(datatype)
                      Max(Min(typebuf[Nproj],Minimum),proj->expr->type);
       lengbuf[Nproj]=Max(Min(lengbuf[Nproj],Minimum),proj->expr->leng);
      }
      Minimum = 10000;
    }

    /* преобразуем, если получится */
    forlist(list,Node->body.U.subqs) {
      for(Nproj=0, proj = list->node->projlist; proj != NIL;
          ++Nproj, proj = proj->next) {
        if(typebuf[Nproj] != proj->expr->type || /*у общего тип другой*/
           lengbuf[Nproj] != proj->expr->leng) {
          proj->expr=TypeConv(proj->expr,typebuf[Nproj],lengbuf[Nproj]);
        }
      }
    }
  break; case JOIN  :
    forlist(list,Node->body.J.subqs) NodeTypeCheck(list->node);

    if(Node->body.J.predlist) {
      ExprTypeCheck(Node->body.J.predlist->expr);
      ANDsplit(&Node->body.J.predlist);
    }
  break; case INSERT:
    NodeTypeCheck(Node->body.I.subq);
  break; case UPDATE: case DELETE:
    NodeTypeCheck(Node->body.M.subq);
    if(Node->body.M.predlist) {
      ExprTypeCheck(Node->body.M.predlist->expr);
      ANDsplit(&Node->body.M.predlist);
    }
  break; case ORDER: case GROUP:
    NodeTypeCheck(Node->body.O.subq);
  break; case TABLE: case INSVALUE: case CATALOG: case ATTRIBUTES:
  break; default:
    longjmp(err_jump,ErrSyst);
  }


  if(Node->mode == ORDER || Node->mode == GROUP) {
    collist    orderlist;                   /* все сортируемые столбцы*/
    reduction  reduct;
    forlist(orderlist,Node->body.O.orderlist) {
      switch(orderlist->sortproj.proj->expr->type) {
      case Tvar:                            /* типа varchar           */
        orderlist->sortproj.proj->expr =    /* преобразуем в char     */
                TypeConv(orderlist->sortproj.proj->expr,Tchar,
                         orderlist->sortproj.proj->expr->leng);
      break; case Tldata:
        TypeErr(orderlist->sortproj.proj->expr);
      }
    }

    /* теперь обработаем все редукции */
    forlist(reduct,Node->body.O.reductions) {
      if(reduct->func != Rcount) {
        expr arg = reduct->arg1;
        ExprTypeCheck(arg);
        if(arg->type == Tbool || arg->type == Tldata) TypeErr(arg);
        switch(reduct->func) {
        case Rsumm: case Ravg:               /* суммировать, усреднять*/
          if(arg->type>=Tchar) TypeErr(arg); /* тексты нельзя         */
          if(arg->type == Tshort) {          /* SUM, AVG(short)->long */
            reduct->arg1 = arg = TypeConv(arg,Tlong,sizeof(long));
          }
        break; case Rmax2: case Rmin2:
          arg = reduct->arg2;
          ExprTypeCheck(arg);
          if(arg->type == Tbool) TypeErr(arg);
        }
      }
    }
  }

  /* теперь проверим типы в выражениях проекций */
  forlist(proj,Node->projlist) {
    ExprTypeCheck(proj->expr);
    if(proj->expr->type == Tbool) TypeErr(proj->expr);
  }
}

/*
 * Проверка типов термов в выражении
 */
static recursive void ExprTypeCheck(Expr) expr Expr; {
  exproper mode = Expr->mode;

  if(mode==Econst || mode==Ecolumn || mode==Eparam) {

  } else if (mode == Econv) {             /* явное преобразование типа*/
    expr  arg;
    ExprTypeCheck(arg=Expr->body.unary);
    if(arg->type==Tbool || arg->type==Tldata ||
       (arg->type>=Tchar) != (Expr->type>=Tchar)) {
      TypeErr(arg);
    }
  } else if (mode == Eproj || mode == Eextproj) {
    Expr->type = Expr->body.projection->expr->type;
    Expr->leng = Expr->body.projection->expr->leng;

  } else if (mode == Eredref) {
    switch(Expr->body.reduction->func) {
    default:
      ExecError(ErrSyst);
    case Rcount:
      Expr->type = Tlong; Expr->leng = sizeof(long);
    break; case Rsumm: case Ravg: case Rmin: case Rmax:
      Expr->type = Expr->body.reduction->arg1->type;
      Expr->leng = Expr->body.reduction->arg1->leng;
    break; case Rmin2: case Rmax2:
      Expr->type = Expr->body.reduction->arg2->type;
      Expr->leng = Expr->body.reduction->arg2->leng;
    }

  } else if (mode == Esign  || mode == Enot) {
    expr expr1;
    ExprTypeCheck(expr1=Expr->body.unary);
    switch(mode) {
    case Enot:
      if(expr1->type != Tbool) TypeErr(expr1);
    break; case Esign:
      if(expr1->type == Tbool || expr1->type>=Tchar) TypeErr(expr1);
    }
    Expr->type = expr1->type;
    Expr->leng = expr1->leng;

  } else if (mode == Efunc)   {
    expr arg;
    if ((arg=Expr->body.scalar.arg) != NIL) {
      ExprTypeCheck(arg);
      if(Expr->body.scalar.type == Tvar) {
        if(Expr->body.scalar.func == fSize) {
          if(arg->type == Tldata) {
            Expr->type = Tlong;
            Expr->leng = sizeof(long);
            Expr->body.scalar.func = fLongSize;
          } else if(arg->type != Tvar) {
            TypeErr(arg);
          }
        } else {
          if(arg->type != Tchar && arg->type != Tvar) TypeErr(arg);
          Expr->type = arg->type;
          Expr->leng = arg->leng;
        }
      }
    }

  } else if (mode == Eif) {
    expr expr1,expr2;
    ExprTypeCheck(expr1=Expr->body.cond.cond);
    if(expr1->type != Tbool) TypeErr(expr1);
    ExprTypeCheck(expr1=Expr->body.cond.tvar);
    ExprTypeCheck(expr2=Expr->body.cond.fvar);
    if(expr1->type == Tbool) TypeErr(expr1);
    if(expr1->type == Tldata) {
      if(expr2->type != Tldata) TypeErr(expr2);
    } else if(expr1->type >= Tchar) {
      if (expr2->type == Tbool || expr2->type <Tchar) TypeErr(expr2);
    } else if(expr1->type < expr2->type) {
      Expr->body.cond.tvar = TypeConv(expr1,expr2->type,expr2->leng);
    } else if(expr2->type < expr1->type) {
      Expr->body.cond.fvar = TypeConv(expr2,expr1->type,expr1->leng);
    }
    Expr->type = Max(expr1->type,expr2->type);
    Expr->leng = Max(expr1->leng,expr2->leng);

  } else if (mode == Equery)  {       /* выражение - E-запрос         */
    node Node = Expr->body.equery->eqNode;
    NodeTypeCheck(Node);              /* проверим подзапрос           */
    Expr->type = Node->projlist->expr->type;
    Expr->leng = Node->projlist->expr->leng;
  } else if (mode == Eexists) {       /* EXISTS - выражение           */
    NodeTypeCheck(Expr->body.equery->eqNode);
    Expr->type = Tbool;
  } else if (mode >= Earithm) {
    expr expr1,expr2;
    ExprTypeCheck(expr1=Expr->body.binary.arg1);
    ExprTypeCheck(expr2=Expr->body.binary.arg2);
    if(expr1->type == Tldata) TypeErr(expr1);
    if(expr2->type == Tldata) TypeErr(expr2);
    if (mode >= Elogical) {
      if(expr1->type != Tbool) TypeErr(expr1);
      if(expr2->type != Tbool) TypeErr(expr2);
      Expr->type = Tbool;
    } else {
      Expr->type = Max(expr2->type,expr1->type);
      Expr->leng = Max(expr2->leng,expr1->leng);
      if(expr1->type == Tbool) TypeErr(expr1);
      if(expr1->type < Tchar) {
        if(expr2->type >= Tchar) TypeErr(Expr);
        if(mode  == Elike) TypeErr(Expr);
        /* приведем к общему типу */
        if(expr1->type < Expr->type) {
          Expr->body.binary.arg1 =TypeConv(expr1,Expr->type,Expr->leng);
        } else if(expr2->type < Expr->type) {
          Expr->body.binary.arg2 =TypeConv(expr2,Expr->type,Expr->leng);
        }
        /* функция mod не определена для float */
        if(mode == Emod && Expr->type==Tfloat) TypeErr(Expr);
      } else {
        if(expr2->type < Tchar || (mode < Ecompare && mode != Elike))
                                          TypeErr(Expr);
      }
      if(mode >= Ecompare || mode == Elike) Expr->type = Tbool;
    }
  } else {
    TypeErr(Expr);
  }
}


/*
 * выдача сообщения об ошибочном типе выражения (с установкой позиции)
 */
static void TypeErr(Expr) expr Expr; {
  ErrPosition = Expr->source;
  longjmp(err_jump,ErrType);
}

/*
 * Генерация выражения для неявного преобразования типов
 */
static expr TypeConv(expr Expr, datatype type, int length) {
  expr result = (expr) GetMem(sizeof(struct expr));

  if(Expr->type == Tbool || Expr->type == Tldata) TypeErr(Expr);
  if((Expr->type >= Tchar) != (type >= Tchar) ) TypeErr(Expr);

  result->mode       = Econv;
  result->body.unary = Expr;
  result->type       = type;
  result->leng       = length;
  return(result);
}


/*
 * Обход узлов запроса
 */
/*static recursive void NodeScan(Node,func)
     node Node; void (*func)(node,boolean); {
  nodelist plist;
  (*func)(Node,0);
  switch(Node->mode) {
  case SELECT:
    NodeScan(Node->body.S.subq,func);
  break; case JOIN:
    forlist(plist,Node->body.J.subqs) NodeScan(plist->node,func);
  break; case UNION:
    forlist(plist,Node->body.U.subqs) NodeScan(plist->node,func);
  break; case UPDATE: case DELETE:
    NodeScan(Node->body.M.subq,func);
  break; case INSERT:
    NodeScan(Node->body.I.subq,func);
  break; case ORDER: case GROUP:
    NodeScan(Node->body.O.subq,func);
  break; case INSVALUE: case TABLE: case CATALOG: case ATTRIBUTES:
  break; default:
    myAbort(201);
  }
  (*func)(Node,1);
} */


/*
 * Разбиение предикатов на AND - список, опускание предикатов
 * на таблицы
 */
static void ANDsplit(plist) exprlist* plist; {
  exprlist list = *plist;

  /* проверяем, что выражение - AND-предикат, и разбиваем его на 2 */
  while (list != NIL) {
    if (list->expr->mode == Eand) {
      expr    expr1 = list->expr->body.binary.arg1;
      expr    expr2 = list->expr->body.binary.arg2;
      exprlist  new = (exprlist) (list->expr);
      new ->next = list->next;
      new ->expr = expr2;
      list->expr = expr1;
      list->next = new;
    } else {
      list = list->next;
    }
  }
}


/*
 * Проверка разрешенности доступа к таблице "tablenum",
 * причем 1-ый блок (описатели) зачитан в память, его адрес - Index,
 * а желаемые биты доступа определяются маской "accneeded"
 */
static void ChckPriv(table tablenum, zptr Index, unsigned accneeded) {
  unshort  index    = sizeof(page);
  short    Ncolumns = DescTable(index).Ncolumns;

  /* если таблица не наша и мы - не администратор, то проверим права */
  if ( ! (Agent->privil & Priv_Auth) &&
       Files[tablenum].owner != Agent->userid) {
    int      i;
    zptr     scan;

    for(i = -1; i<Ncolumns; ++i) index += intDATA(index);
    i= -1; scan = Index+index+sizeof(short)-sizeof(privil);
    do {
      scan += sizeof(privil);
      if ( ++i == intDATA(index) ) ExecError(ErrRights);
    } while ( (pdescr(scan).who != Agent->userid &&
               pdescr(scan).who != nobody) ||
            ! (pdescr(scan).what & accneeded)  );
  }
}

/*
 * Блокировка таблицы
 */
          /* здесь бы надо заблокировать все индексы */
short TabLock() {
  char*    pscan = sizeof(struct parseheader) + ParseBuf;
  short    tablenum;
  short    locktype;
  zptr     Index;

  locktype = (*(short*) pscan ? Oltype : Sltype);
  pscan += sizeof(short);
  if ( (tablenum = LookDir(pscan,NoFile)) < 0) return(-1);

  if(LockElem(&Files[tablenum].iR,Sltype,TRUE)) LockAgent();
  if(Busy(Index=GetPage((page)1, (table)tablenum, Sltype))) myAbort(88);
  ChckPriv((table) tablenum, Index, ( locktype ? 0x0F ^ 0x01 : 0x01));

  if(LockTable((table)tablenum, locktype)) LockAgent();
  return(0);
}
