#include "FilesDef"
#include "TreeDef"


/* ссылки на UNION-subq - проекции - ОСТОРОЖНО ! */

#define MaxSortProjs 4
#define MaxJoinNodes 6

          int          Optimize(void);
   extern boolean      LockElem (lockblock*,short,boolean);
   extern char* mfar   GetMem (int);
   extern boolean      ModIndex(table,short*);
   extern table        GetNxIndex(table);

typedef struct predData {short colShift; short sign; long selFactor;} predData;

typedef struct cost*     cost;
typedef struct joinCost* joinCost;
struct cost {
  cost       next;
  long       firstCost,repCost;
  short      nSorts;
  sortproj   sortProjs[MaxSortProjs];
  joinCost   child;
  char*      path;
};

typedef struct {
  short    nSorts;
  sortproj sortProjs[MaxSortProjs];
} jSort;


struct joinCost {
  short nNodes; boolean useIt;
  jSort subSorts[DummySize];
};

/*static sortproj FindSort    (expr,node,boolean);
/*static struct cost ConvSort(node,cost);
*/

  static short    ModTable;
  static short*   ModArray;

  static long     TryNode     (node,cost,expr*,char*);

  static long     OptTable    (node,cost,expr*,char*);
  static void     ErrOptimize(void);


  static void     CalcPredicate(expr,predData*);
  static expr     ConvColPred (expr);

  static boolean  CheckNotZ(expr);

  static joinCost CalcJoinCost(cost);
  static void     AddCost     (cost*,short,sortproj*,node);
  static cost     FindCost    (cost, short,sortproj*);

  static void     ApplyPreds  (long*,expr*,short*,short);


  static sortproj CheckUsedSet (expr);
  static sortproj CheckUsed3   (exproper,expr,expr,expr);
  static sortproj CheckUsedProj(projection);

  static void     CalcProjections(expr,exprlist**);
  static void     CalcFoundProj  (projection,exprlist**,expr);
  static void     CalcPathSize(node,node);

  static long     Cadd(long,long);
  static long     Cmul(long,long);
  static long     Cdiv(long,long);

  static node*    checkNodes;
  static short    checkNnodes;
  static short    checkXnode;

  static projection calcList;
  static short      CurrentLevel;

  static unchar   traceIndent[] = {"\0          "};

int Optimize() {
  expr  dummyPred = NIL;
  cost  result    = NIL;
  short nEqueries = 0;
  cost* eqCosts;
  eqref eqscan;
  *traceIndent = 0;

  if( (ErrCode = setjmp(err_jump)) != 0) return(-1);

  forlist(eqscan,ParseHeader->eqlist) ++nEqueries;
  eqCosts = (cost*)GetMem(sizeof(cost)*nEqueries);

  /* выбираем пути для подзапросов (начиная от самых вложенных) */
  nEqueries = 0;
  forlist(eqscan,ParseHeader->eqlist) {
    node eqNode  = eqscan->eqNode;
    CurrentLevel = eqNode->eqLevel;
    if(TraceFlags & TraceCode) Reply(Fdump("Equery(%1.16)",(unsigned)CurrentLevel));
    CalcPathSize(eqNode,NIL);
    eqCosts[nEqueries] = NIL;
    AddCost(&eqCosts[nEqueries],0,NIL,eqNode);
    TryNode(eqNode,eqCosts[nEqueries],&dummyPred,NIL);
    eqscan->cost = eqCosts[nEqueries]->firstCost;
    ++nEqueries;
  }

  CurrentLevel = ParseHeader->Node->eqLevel;
  CalcPathSize(ParseHeader->Node,NIL);
  AddCost(&result,0,NIL,ParseHeader->Node);

  ModArray = NIL;
  ModTable = -1;

  TryNode(ParseHeader->Node,result,&dummyPred,NIL);

  if(TraceFlags & TraceCode) Reply("*** Fixing the best path");
  calcList = (projection)-1;

  /*фиксируем пути для подзапросов (начиная от самых вложенных) */
  nEqueries = 0;
  forlist(eqscan,ParseHeader->eqlist) {
    node eqNode = eqscan->eqNode;
    CurrentLevel = eqNode->eqLevel;
    if(TraceFlags & TraceCode) Reply(Fdump("Equery(%1.16)",(unsigned)CurrentLevel));
    TryNode(eqNode,eqCosts[nEqueries],&dummyPred,eqCosts[nEqueries]->path);
    ++nEqueries;
  }

  CurrentLevel = ParseHeader->Node->eqLevel;
  TryNode(ParseHeader->Node,result,&dummyPred,result->path);
  return(0);
}

static recursive long TryNode(theNode,costs,outPreds,path) 
       node theNode; cost costs; expr* outPreds; char* path; {
  long  nRecords;
  cost  cscan;
  short savedMemory = ((parseheader*) ParseBuf)->length; /* запомним тек. память */
  if(TraceFlags & TraceCode) {
    static char unnamed[] = {1,'*'};
    expr* pscan;
    for(pscan=outPreds; *pscan != NIL; ++pscan);
    ++*traceIndent;
    Reply(Fdump("%pOpt Node %4.16(%c %p) Npreds=%2.16",traceIndent,
                (unsigned)theNode,(unsigned)"SJUGODTCA       IVDUR"[theNode->mode],
                (theNode->name == NIL ? unnamed : theNode->name),
                (unsigned)(pscan-outPreds)));
    forlist(cscan,costs) {
      Reply(Fdump("%pVar%4.16, nSorts=%1.16(%p %p)",traceIndent,(unsigned)cscan,
                  cscan->nSorts,cscan->nSorts == 0 ? "" : cscan->sortProjs->proj->name,
                  cscan->nSorts==0 || cscan->sortProjs->order == dirOrder? "" : "\04DESC"));
    }
  }


  forlist(cscan,costs) {
    cscan->firstCost = -1; cscan->repCost = -1;
  }

  switch(theNode->mode) {
  case SELECT: {
    short    nOutPreds = 0;
    short    nMyPreds = 0;
    exprlist pPreds;
    expr*    newPreds;
    cost     newcosts = NIL;
    node     subNode  = theNode->body.S.subq;

    /* преобразуем все те же сортировки */
    checkNodes  = &subNode;
    checkNnodes = 1;
    forlist(cscan,costs) {
      if((cscan->child = CalcJoinCost(cscan)) != NIL) {
        boolean empty = cscan->child->nNodes == 0;
        AddCost(&newcosts,empty ? 0   : cscan->child->subSorts[0].nSorts,
                          empty ? NIL : cscan->child->subSorts[0].sortProjs,subNode);
      }
    }

    /* подсчитаем полное число предикатов */
    while(outPreds[nOutPreds] != NIL) ++nOutPreds;
    forlist(pPreds,theNode->body.S.predlist) ++nMyPreds;
    newPreds = (expr*)GetMem(sizeof(expr)*(nOutPreds+nMyPreds+1));
    nMyPreds = nOutPreds = 0;

    /* передадим свои предикаты */
    forlist(pPreds,theNode->body.S.predlist) newPreds[nMyPreds++] = pPreds->expr;
    /* передадим внешние предикаты */
    while((newPreds[nMyPreds++] = outPreds[nOutPreds++]) != NIL);
    
    nRecords = TryNode(subNode,newcosts,newPreds,path);

    /* перенесем цену наших сортировок */
    forlist(cscan,costs) {
      if(cscan->child != NIL) {
        boolean empty = cscan->child->nNodes == 0;
        cost    found = FindCost(newcosts,empty ? 0   : cscan->child->subSorts[0].nSorts,
                                          empty ? NIL : cscan->child->subSorts[0].sortProjs);
        if( found == NIL) ErrOptimize();
        if( found->firstCost != -1 &&
            cscan->firstCost < 0 || found->firstCost<cscan->firstCost) {
          cscan->firstCost = found->firstCost;
          _move(found->path,subNode->pathSize,cscan->path);
        }
      }
    }
    if(path != NIL) {
      outPreds[0] = NIL;
    }

  } break; case ORDER: case GROUP: {
    collist   oscan;
    cost      bestCost;
    cost      plaincost = NIL;
    node      subNode   = theNode->body.O.subq;
    sortproj  sortProjs[MaxSortProjs];
    short     nOrders   = 0;
    expr      dummyPred = NIL;

    AddCost(&plaincost,0,NIL,subNode);

    forlist(oscan,theNode->body.O.orderlist) {
      if(nOrders < MaxSortProjs) sortProjs[nOrders++] = oscan->sortproj;
    }
    if(nOrders > 0 && nOrders<MaxSortProjs && (path == NIL || *(short*)path == 0)) {
      AddCost(&plaincost,nOrders,sortProjs,subNode);
    }

    /* вычислим сортируемую таблицу */
    nRecords = TryNode(subNode,plaincost,&dummyPred,path != NIL ? path+sizeof(short) : NIL);

    /* для plainCost (неотсортированного) добавим цену сортировки */
    {
      short      sortsize = sizeof(short);
      projection pscan;
      long       coeff    = 4;
      forlist(pscan,theNode->projlist) {
        sortsize += pscan->expr->leng;
      }
      plaincost->repCost   = nRecords/(BlockSize/sortsize);
      if(plaincost->repCost >= 40) coeff += 2;
      plaincost->firstCost = Cadd(plaincost->firstCost,Cmul(plaincost->repCost,coeff));
    }

    bestCost = (plaincost->next == NIL || plaincost->next->firstCost == -1 ||
                plaincost->firstCost < plaincost->next->firstCost) ?
                                                   plaincost : plaincost->next;
    if(theNode->mode == GROUP) nRecords = Cdiv(nRecords,10);
    ApplyPreds(&nRecords,outPreds,NIL,0);

    /* занесем цену сортировок, которые выполнятся после ORDER */
    checkNodes  = &subNode;
    checkNnodes = 1;
    forlist(cscan,costs) {
      joinCost subSort = CalcJoinCost(cscan);
      if(subSort != NIL) {
        short nSorts = (subSort->nNodes == 0 ? 0 : subSort->subSorts[0].nSorts);
        if(nSorts <= nOrders &&
	   _cmps((char*)subSort->subSorts[0].nSorts,(char*)sortProjs,
                 nSorts*sizeof(sortproj)) == 0) {
          cscan->firstCost = bestCost->firstCost;
          cscan->repCost   = bestCost->repCost;

          *(short*)cscan->path   = bestCost == plaincost && nOrders > 0;
          _move(bestCost->path,subNode->pathSize,cscan->path+sizeof(short));
        }
      }
    }

    if(path != NIL) {
      if(theNode->body.O.doSort = *(short*)path) {
        theNode->body.O.doCopy = FALSE;
        /* проверим, не используется ли в редукциях EQuery */
        /* или (для Z-систем) - Elike(sortproj)            */
        if(theNode->mode == GROUP) {
          reduction rscan;
          forlist(rscan,theNode->body.O.reductions) {
            expr arg = rscan->arg1;
          if(arg != NIL && CheckNotZ(arg)) {theNode->body.O.doCopy = TRUE; break;}
            arg = rscan->arg2;
          if(arg != NIL && CheckNotZ(arg)) {theNode->body.O.doCopy = TRUE; break;}
          }
        } else {
          theNode->body.O.doCopy = TRUE;
/*        projection pscan;
/*        forlist(pscan,theNode->projlist) {
/*          if(CheckNotZ(pscan->expr)) {theNode->body.O.doCopy = TRUE; break;}
/*        }
*/
        }
      } else {                           /* Group без сортировки -> groupvalues */
        theNode->body.O.groupvalues = (Caddr*)GetMem(sizeof(Caddr)*nOrders);
      }
    }

  } break; case UNION: {
    /* UNION-слияние (тупое) нарушает все сортировки,
     * поэтому ничего не заказываем от нижних таблиц
     * и на верх выдаем неотсортированную
     * 
     * все предикаты надо бы опустить на каждую нижнюю таблицу,
     *  но пока не можем - они вычисллят верхние проекции, а на следующем - не
     *  вычислят
     */ 
    nodelist nscan;
    short    pathShift = 0;
    expr     dummyPred = NIL;
    long     firstCost = 0;

    if(costs == NIL) ErrOptimize();
    nRecords = 0;
    forlist(nscan,theNode->body.U.subqs) {
      cost plaincost   = NIL;
      AddCost(&plaincost,0,NIL,nscan->node);
      nRecords = Cadd(TryNode(nscan->node,plaincost,&dummyPred,
                              path==NIL ? NIL : path+pathShift),nRecords);
      if(plaincost->firstCost == -1) ErrOptimize();
      firstCost = Cadd(plaincost->firstCost,firstCost);
      _move(plaincost->path,nscan->node->pathSize,costs->path+pathShift);
      pathShift += nscan->node->pathSize;
    }

    ApplyPreds(&nRecords,outPreds,NIL,0);

    /* теперь занесем подсчитаное в верхние цены     */
    /* перенесем цену в эффективно пустые сортировки */
    checkNodes  = &theNode->body.U.subqs->node;
    checkNnodes = 1;
    forlist(cscan,costs) {
      joinCost subSort = CalcJoinCost(cscan);
      if(subSort->nNodes == 0) {
        cscan->firstCost = firstCost;
        _move(costs->path,theNode->pathSize,cscan->path);
      }
    }  


  } break; case JOIN:  {
    /*
     * 1. Определяем порядок вычисления
     * 2. Находим предикаты, зависящие только от "зафиксированных" joined-таблиц
     * ***** Тут пропадают константные предикаты !!! ***
     */
    node*    nodes;
    unchar*  permutation;
    short    nNodes = 0;
    short    nPreds = 0;
    nodelist nscan;
    short    xPermutation;
    short    nPermutations = 1;
    cost*    nodeCosts;
    cost*    foundCosts;
    long*    nodeRecords;
    expr*    downPreds;

    /* подсчитаем число узлов */
    forlist(nscan,theNode->body.J.subqs) nPermutations *= ++nNodes;
    if(nNodes > MaxJoinNodes) longjmp(err_jump,ErrComplicated);
    permutation = (unchar*) GetMem(nNodes);
    nodes       = (node*)   GetMem(sizeof(node)*nNodes);
    nodeRecords = (long*)   GetMem(sizeof(long)*nNodes);
    nodeCosts   = (cost*)   GetMem(sizeof(cost)*nNodes);
    foundCosts  = (cost*)   GetMem(sizeof(cost)*nNodes);

    /* подсчитаем число предикатов */
    { exprlist escan;
      expr*    pscan;
      forlist(escan,theNode->body.J.predlist) ++nPreds;
      for(pscan = outPreds;*pscan != NIL; ++pscan) ++nPreds;
      downPreds = (expr*) GetMem(sizeof(expr)*(nPreds+1));
    }

    /* преобразуем интересующие сортировки */
    nNodes = 0;
    forlist(nscan,theNode->body.J.subqs) nodes[nNodes++] = nscan->node;
    checkNodes  = nodes;
    checkNnodes = nNodes;
    checkXnode  = -1;
    forlist(cscan,costs) {
      cscan->child = CalcJoinCost(cscan);
    }

    nRecords = -1;
    /*цикл по всем вариантам перестановок таблиц (если не 2 проход) */
    xPermutation = (path == NIL ? 0 : *(short*)path);
    while(xPermutation < nPermutations) {
      short pathShift   = sizeof(short);
      long  thisRecords = 1;
      short xNode;
      /* генерим перестановки */
      {
        short used = 0;
        short permTemp = xPermutation;
        for(xNode=0;xNode<nNodes;++xNode) {
          short x = permTemp%(nNodes-xNode);
          short y = -1;
          permTemp /= (nNodes-xNode);
          do if(!(used & (1<<++y))) --x; while(x>=0);
          permutation[xNode] = (unchar)y; used |= (1<<y);
        }
      }

      if(TraceFlags & TraceCode) {
        Reply(Fdump("%pPerm %2.16 (%2.16,%2.16,%2.16)",traceIndent,
                    (unsigned)xPermutation,  (unsigned)permutation[0],
                    (unsigned)permutation[1],(unsigned)permutation[2]));
      }

      /* формируем упорядоченный массив узлов */
      xNode = 0;
      forlist(nscan,theNode->body.J.subqs) nodes[permutation[xNode++]] = nscan->node;

      /* помечаем сортировки, интересующие при этой перестановке */
      forlist(cscan,costs) {
        if(cscan->child != NIL) {              /* есть последовательность subsorts*/
          int index = cscan->child->nNodes;    /* проверим, что узлы вычисл в нужном порядке */
          while(--index >= 0 &&
                cscan->child->subSorts[index].sortProjs[0].proj->owner == nodes[index]);
          cscan->child->useIt = index < 0;
        }
      }

      /* проходим по узлам (упорядоченно) */
      for(xNode=0;xNode<nNodes;++xNode) {
        node  subNode = nodes[xNode];
         /* формируем сортировки, интересующие нас при данном порядке */
        nodeCosts[xNode] = NIL;
        AddCost(&nodeCosts[xNode],0,NIL,subNode);                  /* plain cost */
        forlist(cscan,costs) {
          /* если преобразованная внешняя сортировка требует сортировки этого узла */
          if(cscan->child != NIL && cscan->child->useIt &&
             cscan->child->nNodes > xNode) {
            AddCost(&nodeCosts[xNode],cscan->child->subSorts[xNode].nSorts,
                                      cscan->child->subSorts[xNode].sortProjs,subNode);
          }
        }
        /* теперь сформируем предикаты, которые можно опустить */
        { int       nNewPreds =0;
           exprlist escan;
           expr*    pscan;
           short    noConst = xNode == 0 ? badOrder : constOrder;   /* ВРЕМЕННО! */
           checkNodes  = nodes;
           checkNnodes = nNodes;
           checkXnode  = xNode;
           /* пройдем по предикатам слияния */
           forlist(escan,theNode->body.J.predlist) {
             sortproj used = CheckUsedSet(escan->expr);
             if(used.order != badOrder && used.order != noConst) downPreds[nNewPreds++] = escan->expr;
           }

           /* пройдем по внешним предикатам */
           for(pscan=outPreds;*pscan != NIL;++pscan) {
             sortproj used = CheckUsedSet(*pscan);
             if(used.order != badOrder && used.order != noConst) downPreds[nNewPreds++] = *pscan;
           }
           downPreds[nNewPreds] = NIL;
        }

        /* теперь вычислим узел */
        nodeRecords[xNode] = TryNode(nodes[xNode],nodeCosts[xNode],downPreds,
                                     path == NIL ? NIL : path+pathShift);
        thisRecords = Cmul(thisRecords,nodeRecords[xNode]);
        pathShift += nodes[xNode]->pathSize;
      }

      /* теперь подсчитаем цену для каждого cost */
      forlist(cscan,costs) {
        /* эта сортировка могла получиться при данном порядке ? */
        if(cscan->child != NIL && cscan->child->useIt) {
          long firstCost = 0;
          /* для всех узлов (от внутреннего) возьмем цену соотв. сортировки*/
          /* Если узел участвует в сортировке внешнего -> ищем эту сорт-ку */
          /* если не участвует (он после сортируемых узлов) -> берем plain */
          for(xNode=nNodes;--xNode>=0;) {
            boolean usedInSort = xNode < cscan->child->nNodes;
            cost theCost =
               FindCost(nodeCosts[xNode],
                        usedInSort ? cscan->child->subSorts[xNode].nSorts    : 0,
                        usedInSort ? cscan->child->subSorts[xNode].sortProjs : NIL);
            if(theCost == NIL) ErrOptimize();
          if(theCost->firstCost == -1) break;
            foundCosts[xNode] = theCost;
            firstCost = Cadd(Cmul(nodeRecords[xNode],firstCost),theCost->firstCost);
          }

          /* занесем cost, если он есть и лучше */
          if(xNode < 0 && (cscan->firstCost < 0 || cscan->firstCost > firstCost)) {
            pathShift = sizeof(short);
            cscan->firstCost     = firstCost;
            *(short*)cscan->path = xPermutation;
            for(xNode=0; xNode<nNodes;++xNode) {
              _move(foundCosts[xNode]->path,nodes[xNode]->pathSize,cscan->path+pathShift);
              pathShift += nodes[xNode]->pathSize;
            }
          }
        }
      }

    if(path != NIL) break;
      /* количество записей занесем минимальное */
      if(nRecords < 0 || nRecords > thisRecords) nRecords = thisRecords;
      ++xPermutation;
    }
    
    if(path != NIL) {           /* переставим все node в nodelist   */
      int      xNode = 0;
      forlist(nscan,theNode->body.J.subqs) nscan->node = nodes[xNode++];
      outPreds[0] = NIL;
    }

  } break; case TABLE: {
    nRecords = OptTable(theNode,costs,outPreds,path);

  } break; case CATALOG: case ATTRIBUTES: {
    nRecords = 20;
    if(costs == NIL || costs->nSorts != 0) ErrOptimize();
    costs->firstCost = 1;

  } break; case UPDATE: case DELETE: {
    short    nMyPreds = 0;
    exprlist pPreds;
    expr*    newPreds;
    node     subNode = theNode->body.M.subq;
    if(theNode->mode == UPDATE) ModArray = theNode->body.M.fieldlist;
    if(subNode->mode != TABLE) ErrOptimize();
    ModTable = subNode->body.T.tablenum;

    /* вычисляем нижний запрос, зная что мы - самые верхнии */
    /* и нам переданы пустые предикаты и plaincost          */
    forlist(pPreds,theNode->body.M.predlist) ++nMyPreds;
    newPreds = (expr*)GetMem(sizeof(expr)*(nMyPreds+1));
    nMyPreds = 0;
    forlist(pPreds,theNode->body.M.predlist) newPreds[nMyPreds++] = pPreds->expr;
    newPreds[nMyPreds] = NIL;

    nRecords = TryNode(theNode->body.M.subq,costs,newPreds,path);


  } break; case INSERT: {
    node subNode = theNode->body.I.subq;
    /* вычисляем нижний запрос, зная что мы - самые верхнии */
    /* и нам переданы пустые предикаты и plaincost          */
    ModTable = subNode->body.T.tablenum;
    nRecords = TryNode(subNode,costs,outPreds,path);

  } break; case INSVALUE: {
    nRecords =1;
    costs->firstCost = 0;

  } break; default:
    ErrOptimize();
  }

  if(TraceFlags & TraceCode) {
    Reply(Fdump("%pEnd Node %4.16, nRecords = %4.16",traceIndent,
                (unsigned)theNode,(unsigned)nRecords));
    forlist(cscan,costs) {
      Reply(Fdump("%pVar%4.16 costs=(%4.16,%4.16), path=<%( %2.16) >",traceIndent,
                  (unsigned)cscan,(unsigned)cscan->firstCost,(unsigned)cscan->repCost,
                  (unsigned)theNode->pathSize,(unshort*)cscan->path));
    }
  }

  if(path != NIL) {
    /*
     * Обработка спущенных сверху предикатов
     * Для каждого предиката проверяется, какие проекции
     * он использует, они вычисляются
     */
    projection  pscan;
    exprlist*   last  = &theNode->doAfter;
    boolean     noZ   = theNode->mode == TABLE;

    *last = NIL;

    /* если узел - таблица, то не вычисляем EQ-предикаты */
    /* пока не вычислим все проекции                     */
    for(;;) {
      expr* parray = outPreds;
      while(*parray != NIL) {
        if(*parray != (expr)-1 && (!noZ || !CheckNotZ(*parray))) {
          /* найдем и вычислим все проекции */
          CalcProjections(*parray,&last);
          /* вычислим сам предикат */
          { exprlist new = (exprlist)GetMem(sizeof(struct exprlist));
            new->expr = *parray; *parray = (expr)-1;
            *last = new; last = &new->next; *last = NIL;
          }
        }
        ++parray;
      }
    if(!noZ) break;
      forlist(pscan,theNode->projlist) CalcFoundProj(pscan,&last,NIL);
      noZ = FALSE;
    }

    /* далее вычисляются также все недовычисленные проекции */
    forlist(pscan,theNode->projlist) CalcFoundProj(pscan,&last,NIL);
  } else {
    ((parseheader*) ParseBuf)->length = savedMemory;
  }
  --*traceIndent;
  return(nRecords);
}

/*
 * Оптимизация выборки таблицы
 */
static long OptTable(theNode,costs,outPreds,path)
    node theNode; cost costs; expr* outPreds; char* path; {
  short     MaxIndexUsedForNRecs = 0;
  table     theTable = theNode->body.T.tablenum;
  table     subfile;
  expr*     pscan;
  int       nPreds;
  predData* predArray;
  short     foundPreds[MaxIndCol];

  long      tabRecords = Files[theTable].recordnum;
  long      nRecords   = 0;

  checkNodes = &theNode;
  checkNnodes= 1;
  checkXnode = 0;

  for(nPreds=0,pscan=outPreds;*pscan != NIL; ++pscan,++nPreds);
  predArray = (predData*)GetMem(sizeof(predData)*nPreds);

  /* найдем все "индексо-хорошие" предикаты */
  for(pscan=outPreds;*pscan != NIL; ++pscan) {
    CalcPredicate(*pscan,&predArray[pscan-outPreds]);
  }

  subfile = (path != NIL ? (table)*(short*)path : theTable);

  while(subfile != NoFile) {
    int  columnNum;
    long firstCost;
    if(Files[subfile].kind == tableKind) {
      columnNum = 0;
      firstCost = Files[theTable].actualsize-2;
      if(firstCost < 1) firstCost = 1;
      if(MaxIndexUsedForNRecs == 0) {
        nRecords = tabRecords;
        ApplyPreds(&nRecords,outPreds,NIL,0);
      }

    } else {
      boolean    all = TRUE;

      /* подсчитаем число колонок в индексе и проверим, что */
      /* есть предикаты, определяющие индекс однозначно     */
      for(columnNum=0; columnNum<MaxIndCol; ++columnNum) {
        short colShift = Files[subfile].b.indexdescr.columns[columnNum].shift;
      if(colShift < 0) break;
        /* поищем предикат для этой колонки */
        for(pscan=outPreds; *pscan != NIL &&
            (predArray[pscan-outPreds].colShift != colShift ||
             predArray[pscan-outPreds].sign     != Eeq       );
            ++pscan);
      if(*pscan == NIL) all = FALSE;
        foundPreds[columnNum] = pscan-outPreds;
      }

      firstCost = Cadd(Files[subfile].b.indexdescr.clustercount,
                       Files[subfile].actualsize-1);
      if(!all) {                                     /*будем бегать по всему */
        if(MaxIndexUsedForNRecs == 0) {
          nRecords = tabRecords;
          ApplyPreds(&nRecords,outPreds,NIL,0);
        }

      } else {
        long nKeys      = Files[subfile].recordnum;
        long uniqueCost = Files[subfile].actualsize <= 2   ? 1 :
                          Files[subfile].actualsize <= 20  ? 2 : 3;
        if(TraceFlags & TraceCode) {
          Reply(Fdump("%p  Index [%2.16] Pred found",traceIndent,(unsigned)subfile));
        }

        if(nKeys == UniqIndexFlag) {                /* unique Index */
          MaxIndexUsedForNRecs = 100;
          firstCost = uniqueCost;
          nRecords = 1;
        } else {
          if(nKeys == 0) nKeys = 1;
          if(MaxIndexUsedForNRecs < columnNum) {   /* более точное число записей */
            MaxIndexUsedForNRecs = columnNum;
            nRecords = Cdiv(tabRecords,nKeys);
          }
          firstCost= Cdiv(firstCost,nKeys);
          ApplyPreds(&nRecords,outPreds,foundPreds,columnNum);
          if(firstCost < uniqueCost) firstCost = uniqueCost;
        }
      }

      if(path != NIL) {
        exprlist*  last  = &theNode->doBefore;
        if(all) {
          int      index   = columnNum;
          while(--index >= 0) {
            exprlist pathpred = (exprlist) GetMem(sizeof(struct exprlist));
            pathpred->next = theNode->body.T.pathpred;
            pathpred->expr = ConvColPred(outPreds[foundPreds[index]]);
            theNode->body.T.pathpred = pathpred;
            CalcProjections(pathpred->expr,&last);
            outPreds[foundPreds[index]] = (expr)-1;
          }
        }
      }
    }

    /* теперь подставим эту цену всем, требующим такой (или меньшей) сорт-ки */
    { cost cscan;
      if(TraceFlags & TraceCode) {
        Reply(Fdump("%p  Path %2.16[%2.16]. Cost=%4.16",traceIndent,
                    (unsigned)theTable,(unsigned)subfile,(unsigned)firstCost));
      }

      forlist(cscan,costs) {
        if(cscan->nSorts <= columnNum) {
          int i = cscan->nSorts;
          while(--i >= 0) {
            indcolumn* pColumn = &Files[subfile].b.indexdescr.columns[i];
          if(cscan->sortProjs[i].proj->expr->body.column.shift != pColumn->shift ||
             cscan->sortProjs[i].order != pColumn->invFlag) break;
          }
          if(i < 0) {                 /* да, этот путь дает нужную сортировку */
            if(cscan->firstCost == -1 || cscan->firstCost > firstCost) {
              cscan->firstCost = firstCost;
              *(short*)cscan->path = subfile;/* использованный путь           */
            }
          }
        }
      }
    }

  if(path != NIL) break;

    /* теперь попробуем следующий индекс */
    while((subfile = GetNxIndex(Files[subfile].nextfile)) != NoFile &&
           (theTable == ModTable && ModIndex(subfile,ModArray)));

  }

  if(path != NIL) {
    theNode->body.T.path = subfile;
    /* захватываем используемый индекс */
    if(subfile != theNode->body.T.tablenum) {
      if(LockElem(&Files[subfile].iR,Sltype,TRUE)) myAbort(24);
    }

  }
  return(nRecords);
}

 static exproper convCodes[Egt+1-Ecompare] = {Ene,Eeq,Ele,Egt,Ege,Elt};

/*
 * Проверка, что данный предикат (опущенный на таблицу),
 * приводим к виду "Column ? <Expr>"
 * проверка делается поиском аддитивных выражений для столбцов
 */
static void CalcPredicate(theExpr,retData)
                          expr theExpr; predData* retData; {
  retData->sign     = -1;
  retData->colShift = -1;
  if(theExpr->mode >= Ecompare && theExpr->mode < Elogical) {
    sortproj first = CheckUsedSet(theExpr->body.binary.arg1);
    if(first.order != noOrder) {
      sortproj second = CheckUsedSet(theExpr->body.binary.arg2);
      boolean inv    = FALSE;
      expr    colRef = NIL;
      if(second.order == noOrder) {
      } else if(first.order != constOrder && second.order == constOrder) {
        inv = first.order == invOrder;
        colRef = first.proj->expr;
      } else if(first.order == constOrder && second.order != constOrder) {
        inv = second.order == dirOrder;
        colRef = second.proj->expr;
      }
      if(colRef != NIL) {
        if(colRef->mode != Ecolumn) ErrOptimize();
        retData->colShift = colRef->body.column.shift;
        if(!inv) {
          retData->sign = theExpr->mode;
        } else {
          retData->sign = convCodes[theExpr->mode-Ecompare];
        }
      }
    }  
  }
}

/*
 * Преобразование предиката к виду:
 * <Ecolumn> <oper> <выражение>
 */
static expr ConvColPred(argCol) expr argCol; {
  expr     argConst = NIL;
  exproper oper     = argCol->mode;
  expr     result;
  while(argCol->mode != Ecolumn) {
    boolean  sign   = FALSE;
    if(argCol->mode >= Earithm) {
      expr     arg1   = argCol->body.binary.arg1;
      expr     arg2   = argCol->body.binary.arg2;
      if(argConst != NIL && argCol->mode != Eadd && argCol->mode != Esub) ErrOptimize();

      { sortproj first  = CheckUsedSet(arg1);
        sortproj second = CheckUsedSet(arg2);
        if(second.order == dirOrder || second.order == invOrder) {
          expr swap = arg1; arg1=arg2; arg2=swap;
          if(first.order != constOrder) ErrOptimize();
          sign = (argConst == NIL || argCol->mode == Esub);
        } else {
          if(second.order != constOrder ||
             (first.order != dirOrder && first.order != invOrder)) ErrOptimize();
        }
      }
      if(argConst == NIL) {
        argConst = arg2;
      } else {
        expr newConst    = GenEXPR;
        newConst->source = argCol->source;
        newConst->mode   = (Eadd+Esub)-argCol->mode;
        newConst->body.binary.arg1 = argConst;
        newConst->body.binary.arg2 = arg2;
        argConst = newConst;
      }
      argCol   = arg1;
    } else switch(argCol->mode) {
    default:
      ErrOptimize();
    case Eproj:
      argCol = argCol->body.projection->expr;
    break; case Esign:
      sign = TRUE;
      argCol = argCol->body.unary;
    }
    if(sign) {
      expr newConst = GenEXPR;
      newConst->source = argCol->source;
      newConst->mode = Esign;
      newConst->body.unary = argConst;
      oper = convCodes[oper-Ecompare];
    }
  }
  result = GenEXPR;
  result->source = argCol->source;
  result->mode   = oper;
  result->body.binary.arg1 = argCol;
  result->body.binary.arg2 = argConst;
  return(result);
}

/*
 * Учет влияния предикатов на число вынутых записей
 * found задает массив индексов уже использованных предикатов, foundNum - его длину
 */
static void ApplyPreds(long* pRecords, expr* pscan, short* found, short foundNum) {
  short index = 0;
  while(*pscan != NIL) {
    int i = foundNum;
    while(--i>=0 && found[i] != index);
    if(i<0) *pRecords = Cdiv(*pRecords,(*pscan)->mode == Eeq ? 10 : 3);
    ++pscan; ++index;
  }
}

/*
 * Для данного cost (набора сортировок) вычисляем набор сортировок для
 * каждого узла (и порядок узлов), при котором исходная сортировка выполняется
 * checkNodes, checkNnodes - массив узлов JOIN,
 * checkXnode - ставим -1 для того, чтобы словить все узлы, независимо от порядка
 */
static joinCost CalcJoinCost(thisCost) cost thisCost; {
  joinCost result;
  sortproj usedSorts[MaxSortProjs];       /* от какой проекции зависит */
  short    nUsed = 0;
  int      sscan;
  int      nNodesUsed;

  checkXnode = -1;

  /* просмотрим все сортировки */
  for(sscan=0; sscan < thisCost->nSorts; ++sscan) {
    sortproj thisSort = CheckUsedSet(thisCost->sortProjs[sscan].proj->expr);
    if(thisSort.order == noOrder) return(NIL);     /* зависит, но нелинейно */
    if(thisSort.order != constOrder) {             /* не зависит ни от чего */
      /* поищем ту же колонку - не зависят ли от нее пред. JOIN-проекции    */
      /* т.е. если нужна сортировка по <X,Y,Z>, и X ллинейно зависит от M.n,*/
      /* то, если Z тоже зависит линейно от M.n,                            */
      /* считаем, что сортировка по Z не нужна                              */
      int oscan = nUsed;
      while(--oscan>=0 && usedSorts[oscan].proj != thisSort.proj);
      if(oscan < 0) {
        if(thisCost->sortProjs[sscan].order == invOrder) {
          thisSort.order = (invOrder+dirOrder)-thisSort.order;
        }
        usedSorts[nUsed++] = thisSort;
      }
    }
  }

  /* теперь проверим на отсутствие дырок, т.е. все сортировки, */
  /* относящиеся к одному узлу, должны идти подряд */
  sscan          = 0;
  nNodesUsed     = 0;
  for(;;) {
    node  theNode;
    int   oscan;
  if(sscan >= nUsed) break;
    theNode = usedSorts[sscan].proj->owner;
    while(++sscan<nUsed && usedSorts[sscan].proj->owner==theNode);
    ++nNodesUsed;
    /* проверим, что дальше на этот узел не ссылаются */
    for(oscan = sscan; oscan < nUsed; ++oscan) {
      if(usedSorts[sscan].proj->owner == theNode) return(NIL);
    }
  }

  /* теперь сформируем описатель */
  result = (joinCost)GetMem(offsetof(struct joinCost,subSorts) +
                            nNodesUsed * sizeof(jSort));
  result->nNodes = nNodesUsed;

  sscan          = 0;
  nNodesUsed     = 0;
  for(;;) {
    node      theNode;
    jSort*    pSub = result->subSorts + nNodesUsed;
  if(sscan >= nUsed) break;
    pSub->nSorts = 0;
    theNode = usedSorts[sscan].proj->owner;
    while(sscan<nUsed && usedSorts[sscan].proj->owner==theNode) {
      pSub->sortProjs[pSub->nSorts++] = usedSorts[sscan++];
    }
    ++nNodesUsed;
  }
  if(result->nNodes != nNodesUsed) ErrOptimize();

  return(result);
}




static recursive sortproj CheckUsed3(mode,arg1,arg2,arg3) 
                                   exproper mode; expr arg1,arg2,arg3; {
  sortproj retcode = CheckUsedSet(arg1);
  boolean  additive = mode == Eadd || mode == Esub;
  if(retcode.order != badOrder) {
    sortproj res1     = CheckUsedSet(arg2);

    if(!additive && retcode.order != constOrder) retcode.order = noOrder;

    if(res1.order == badOrder) {
      retcode.order = badOrder;
    } else if(res1.order == constOrder) {
    } else if(!additive) {
      retcode.order = noOrder;
    } else if(retcode.order == constOrder &&
                (res1.order == dirOrder || res1.order == invOrder)) {
      retcode=res1;
      if(mode == Esub) retcode.order = (invOrder+dirOrder)-retcode.order;
    } else {
      retcode.order = noOrder;
    }
    if(retcode.order != badOrder && arg3 != NIL) {
      res1 = CheckUsedSet(arg3);
      if(res1.order == badOrder) {
        retcode.order = badOrder;
      } else if(res1.order != constOrder) {
        retcode.order = noOrder;
      }
    }
  }
  return(retcode);
}

static recursive sortproj CheckUsedProj(proj) projection proj; {
  sortproj retcode;
  int   index = checkNnodes;
  /* это проекция из наших подузлов ? */
  while(--index >= 0 && proj->owner != checkNodes[index]);

  
  if(index >= 0) {                      /* да, один из наших подузлов */
    retcode.order = checkXnode == -1 ||  index == checkXnode ? dirOrder :
                    index <  checkXnode ? constOrder : badOrder;
    retcode.proj  = proj;
  } else {
    node uscan = (*checkNodes)->upper; /* ищем в верхних узлах     */
    while(uscan != NIL && uscan != proj->owner) uscan=uscan->upper;
    if(uscan == NIL) {
      retcode.order = constOrder;       /* это "боковая" проекция  */
    } else {
      retcode = CheckUsedSet(proj->expr);
    }
  }
  return(retcode);
}

/*
 * Проверка зависимости выражения от узлов JOIN:
 * проекции узлов, предшествующих в перестановке, обрабатываются как константы
 * проекции следующих узлов недопустимы
 * Возврат: constOrder         -> не зависит от проекций даного узла
 *          dirOrder,invOrder  -> линейно зависит от проекции proj данного узла
 *          noOrder            -> просто зависит от данного узла 
 *          badOrder           -> зависит от узлов, следующих далее в перестановке
 */
static recursive sortproj CheckUsedSet(theExpr) expr theExpr; {
  sortproj retcode;
  if(theExpr->mode >= Earithm) {
    retcode = CheckUsed3(theExpr->mode,theExpr->body.binary.arg1,theExpr->body.binary.arg2,NIL);
  } else switch(theExpr->mode) {
  case Eproj:
   retcode = CheckUsedProj(theExpr->body.projection);
  break; case Econv: case Esign: case Enot:
    retcode = CheckUsedSet(theExpr->body.unary);
    if(retcode.order == dirOrder || retcode.order == invOrder) {
      if(theExpr->mode == Esign) {
        retcode.order = (dirOrder+invOrder)-retcode.order;
      } else {
        retcode.order = noOrder;      /* Econv - тоже, из-за индекс-преобр */
      }
    }
  break; case Econst: case Eparam: case Eextproj:
    retcode.order = constOrder;
/*break; case Ecolumn:
/*  retcode = 0;                     /* дошли до уровня колонки -> не из JOIN */
  break; case Eif:
    retcode = CheckUsed3(Eif,theExpr->body.cond.cond,theExpr->body.cond.tvar,
                             theExpr->body.cond.fvar);
  break; case Efunc:
    if(theExpr->body.scalar.arg == NIL) {
      retcode.order = constOrder;
    } else {
      retcode = CheckUsedSet(theExpr->body.scalar.arg);
      if(retcode.order == dirOrder || retcode.order == invOrder) retcode.order = noOrder;
    }

  break; case Eredref:
    retcode.order = noOrder;        /* ??? !!! */

  break; case Equery: case Eexists:
    retcode.order = constOrder;
    { prjlist pscan;
      forlist(pscan,theExpr->body.equery->extprojs) {
        /* это не внешняя ссылка ? */
        if(CurrentLevel == pscan->proj->owner->eqLevel) {
          sortproj ret1 = CheckUsedProj(pscan->proj);
          if(ret1.order == badOrder) {retcode.order = badOrder; break;}
          if(ret1.order != constOrder) retcode.order = noOrder;
        } else if(CurrentLevel < pscan->proj->owner->eqLevel) {
          ErrOptimize();
        }
      }
    }
  break; default:
    ErrOptimize();
  }
  return(retcode);
}

static cost FindCost(cost costs, short nSorts, sortproj* sortProjs) {
  while(costs != NIL && 
          (costs->nSorts != nSorts ||
           _cmps((char*)costs->sortProjs,(char*)sortProjs,nSorts*sizeof(sortproj)) != 0)) {
    costs = costs->next;
  }
  return(costs);
}

static void AddCost(cost* pcosts, short nSorts, sortproj* sortProjs, node theNode) {
  cost new;
  if(FindCost(*pcosts,nSorts,sortProjs) != NIL) return;
  new = (cost)GetMem(sizeof(struct cost));
  new->nSorts = nSorts;
  new->path   = GetMem(theNode->pathSize);
  _move((char*)sortProjs,nSorts*sizeof(sortproj),(char*)new->sortProjs);
  while(*pcosts != NIL) pcosts = &(*pcosts)->next;
  new->next = NIL;
  *pcosts   = new;
}


static recursive void CalcFoundProj(theProj,elast,theExpr)
                 projection theProj; exprlist** elast; expr theExpr; {
  if(theProj->calcLink == NIL) {
    if(TraceFlags & TraceCode) {
      static char dummyName[] = {1,'*'};
      Reply(Fdump("%p+Calc'%p'",traceIndent,theProj->name == NIL ? dummyName : theProj->name));
    }
    CalcProjections(theProj->expr,elast);
    if(theProj->calcLink != NIL) ErrOptimize();
    { exprlist new = (exprlist)GetMem(sizeof(struct exprlist));
      if(theExpr == NIL) {
        theExpr = GenEXPR; theExpr->mode = Eproj;
        theExpr->type = Tvoid;
        theExpr->body.projection = theProj;
        theExpr->source = 0;
      }
      new->expr = theExpr;
      **elast = new; *elast = &new->next; **elast = NIL;
    }
    theExpr->body.projection->calcLink = calcList;
    calcList = theExpr->body.projection;
  }
}

/*
 * Поиск всех не вычисленных еще проекций, от которых зависит данное выражение
 * Для всех таких проекций вычисляющие их выражения включаются в список elast
 */
static recursive void CalcProjections(theExpr,elast) expr theExpr; exprlist** elast; {
  if(theExpr->mode >= Earithm) {
    CalcProjections(theExpr->body.binary.arg1,elast);
    CalcProjections(theExpr->body.binary.arg2,elast);
  } else switch(theExpr->mode) {
  case Eextproj: case Ecolumn: case Econst: case Eparam: case Eredref:
  break; case Econv: case Esign: case Enot:
    CalcProjections(theExpr->body.unary,elast);
  break; case Efunc:
    if(theExpr->body.scalar.arg != NIL) {
      CalcProjections(theExpr->body.scalar.arg,elast);
    }
  break; case Eif:
    CalcProjections(theExpr->body.cond.cond,elast);
    CalcProjections(theExpr->body.cond.tvar,elast);
    CalcProjections(theExpr->body.cond.fvar,elast);
  break; case Eproj:
    CalcFoundProj(theExpr->body.projection,elast,theExpr);
  break; case Equery: case Eexists:
    { prjlist ourProjs;
      forlist(ourProjs,theExpr->body.equery->extprojs) {
        if(CurrentLevel == ourProjs->proj->owner->eqLevel) {
          CalcFoundProj(ourProjs->proj,elast,NIL);
        } else if(CurrentLevel < ourProjs->proj->owner->eqLevel) {
          ErrOptimize();
        }
      }
    }
  break; default:
    ErrOptimize();
  }
}

/*
 * Проверка того, что выражение не может быть вычислено при данных под Z
 */
static recursive boolean CheckNotZ(theExpr) expr theExpr; {
  if(theExpr->mode >= Earithm) {
    if(theExpr->mode == Elike) return(TRUE);  /* хотя можно и разобраться */
    return(CheckNotZ(theExpr->body.binary.arg1) ||
           CheckNotZ(theExpr->body.binary.arg2));
  }
  switch(theExpr->mode) {
  case Eif:
    return(CheckNotZ(theExpr->body.cond.cond) ||
           CheckNotZ(theExpr->body.cond.tvar) ||
           CheckNotZ(theExpr->body.cond.fvar));
  case Ecolumn: case Eproj: case Eextproj: case Econst: case Eparam: case Eredref:
    return(FALSE);
  case Econv: case Enot: case Esign:
    return(CheckNotZ(theExpr->body.unary));
  case Efunc:
    return(theExpr->body.scalar.arg == NIL ? FALSE :
           CheckNotZ(theExpr->body.scalar.arg));
  case Equery: case Eexists:
    return(TRUE);
  }
  ErrOptimize();
  return(TRUE);
}


static recursive void CalcPathSize(theNode,upper) node theNode; node upper; {
  short pathSize = 0;
  theNode->upper = upper;
  switch(theNode->mode) {
  case TABLE: {
    pathSize = sizeof(short);          /* для используемого индекса */

  } break; case SELECT: {
    CalcPathSize(theNode->body.S.subq,theNode);
    pathSize = theNode->body.S.subq->pathSize;

  } break; case JOIN: {
    nodelist nscan;
    pathSize = sizeof(short);          /*используемый порядок       */
    forlist(nscan,theNode->body.J.subqs) {
      CalcPathSize(nscan->node,theNode); pathSize += nscan->node->pathSize;
    }

  } break; case UNION: {
    nodelist nscan;
    forlist(nscan,theNode->body.U.subqs) {
      CalcPathSize(nscan->node,theNode); pathSize += nscan->node->pathSize;
    }

  } break; case ORDER: case GROUP: {
    CalcPathSize(theNode->body.S.subq,theNode); /* используемая сортировка*/
    pathSize = theNode->body.S.subq->pathSize + sizeof(short);

  } break; case CATALOG: case ATTRIBUTES: case INSVALUE: {
    pathSize = 0;
  } break; case INSERT: {
    CalcPathSize(theNode->body.I.subq,theNode);
    pathSize = theNode->body.I.subq->pathSize;
  } break; case UPDATE: case DELETE: {
    CalcPathSize(theNode->body.M.subq,theNode);
    pathSize = theNode->body.M.subq->pathSize;
  } break; default:
    ErrOptimize();
  }
  theNode->pathSize = pathSize;
  theNode->doAfter  = NIL;
  theNode->doBefore = NIL;
  { projection pscan;
    forlist(pscan,theNode->projlist) pscan->calcLink = NIL;
  }
}

static void ErrOptimize() {
  longjmp(err_jump,ErrSyst);
}


#define MaxLong 0x7FFFFFFF
static long Cadd(x,y) long x,y; {
  if((x += y) < 0) x = MaxLong;
  return(x);
}

static long Cmul(x,y) long x,y; {
  if(x == 0) return(0);
  return((x * y) / x != y ? MaxLong : x*y);
}

static long Cdiv(x,y) long x,y; {
  if(y <= 0)  ErrOptimize();
  if((x /= y) == 0) x = 1;
  return(x);
}
