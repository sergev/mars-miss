                   /* ================================ *\
                   ** ==          M.A.R.S.          == **
                   ** == Генерация исполнимого кода == **
                   \* ================================ */

#include "FilesDef"
#include "TreeDef"
#include "CodeDef"
#include "ExecDef"

         short        Translate(void);

  /* создание дескр-ра      */
  extern void         Describe (node,unchar,dsc_table*);

  /* присвоение коду номера      */
  extern codeaddress* GenCdNum (unchar*,unchar*);

  /* добывание памяти для кода   */
  extern void         GetSpace (unshort,CdBlockH);

  /* run-time операции: выдача сообщений об ошибках */
  extern void         ExecError(int);

typedef struct {                             /* рез-тат транс-ии узла */
  Caddr  getnext;                            /* адрес "след. запись"  */
  Caddr  endjump;                            /* адрес перехода на end */
} nodedescr;

  static CdBlock    Fblock;                    /* 0-блок кода         */
  static CdBlock    Pblock;                    /* тек. блок кода      */
  static Caddr      Pcode;                     /* указатель маш.кода  */
  static unchar     Code;                      /* номер запроса       */
  static int        Pvarout;                   /* длина фикс. части   */

  static nodedescr  TransNode (node);          /* доступ к узлу       */
  static nodedescr  TransTable(node);          /* доступ к таблице    */
  static nodedescr  TransAttr (node);          /* доступ к атрибутам  */
  static Caddr      TransReduc(node,Caddr);    /* вычисление редукций */
  static void       TransEndRed(node,Caddr scanner);
  static void       TransEqRef(eqref);
  static void       TransDoList(exprlist,Caddr,boolean);
  static Caddr      SortRecord(node);          /* подготовка ORDER BY */

  static void       LoadArithm(expr);          /* трансляция выражения*/
  static Caddr      AccArithm (expr);          /* доступ к операнду   */
  static Caddr      LoadPred  (expr,Caddr,     /* трансляция предиката*/
                                       unsigned);

  static void       ClearEqDone(projection);

  static void       PutJump  (Caddr,unchar);   /* генерация перехода  */
  static Caddr      PutPJump (Caddr,unchar);   /* генерация перехода  */
  static void       DefJump  (Caddr,Caddr);    /* определение postdef */

  static void       GenLoad  (Caddr,datatype,short);
  static void       GenStore (Caddr,datatype,short);

  static CdBlock    GetCdBlock(blocktype);     /* генерация блока кода*/

  static Caddr      GenBuffer (short);         /* indir - буфер       */
  static Caddr      GenWork   (short,datatype);/* генерация рабочей   */

  static void       WrOp(unchar);              /* генерация команды   */
  static void       WrPar(unchar,unchar);      /* команда с параметром*/
  static Caddr      WrOpAddr(unchar,unchar,Caddr);
  static Caddr      WrOpLeng(unchar,unchar,Caddr,unshort);
  static Caddr      WriteCode(char*,unsigned);

  static Caddr      TableScan;                 /* сканер таблицы      */

  static short      Nblocks;
  static CdBlock*   PageMap;


#define ABlock       ( (CdBlockH) Fblock)      /* 0-ой блок кода      */

/*
 * Трансляция запроса
 */
short Translate() {
  node         Node      = ParseHeader->Node;
  dsc_table*   Fields    = ParseHeader->inpparams;
  table*       filelist  = ParseHeader->filelist;


  projection proj;
  nodedescr  result;

  /* генерируем блок прямой адресации, 0-ой блок кода */
  Fblock  = NIL;
  GetCdBlock(CdDirect);

  /* заносим список используемых таблиц */
  MVS((char*)filelist,sizeof(table) * (*filelist+1),
      (char*)&ABlock->Ntableused);

  /* флаг: код модификации данных       */
  ABlock->modflag = (Node->mode >= MODIFICATION);

  /* сканер изначально закрыт */
  ABlock->ptrs.contentry= Cnil;
  ABlock->stage         = stageClosed;

  /* начало кода - после списка таблиц */
  ABlock->execentry =
            Align((char*)(ABlock->tableused + ABlock->Ntableused) -
                  (char*) ABlock);

  /* код начинается в 0-ом блоке */
  Pcode  = VirtAddr(Pblock = Fblock) + ABlock->execentry;
  Pblock->head.freespace = CdBlockSize - ABlock->execentry;

  if (Fields->Ncolumns != 0) {          /* есть входные параметры ?   */
    dsc_column* pColumn;                /* генерируем описатель пар-ов*/
    int         xParm;
    short       Nldata = 0;

    ABlock->ptrs.inpbuf =
                      GenBuffer(ABlock->maxibuff=Fields->Mtotlength);
    ABlock->minibuff    = Fields->Mfixlength;
    ABlock->nivars      = Fields->Nvarlength;
    for(pColumn =(dsc_column*)((char*)Fields+Fields->totalleng),xParm=0;
        xParm<Fields->Ncolumns;
        pColumn =(dsc_column*)((char*)pColumn+pColumn->totalleng),++xParm){
      if(pColumn->type == Tldata) ++Nldata;
    }

    if(Nldata != 0) {                    /* Store descr. of ldata parms */
      ldataParamDescr* pDescrs;
      ABlock->ptrs.ldataParams = 
          GenBuffer(sizeof(short)+sizeof(ldataParamDescr)*Nldata);
      *(short*) RealAddr(ABlock->ptrs.ldataParams) = Nldata;
      pDescrs = (ldataParamDescr*)
          RealAddr(ABlock->ptrs.ldataParams+sizeof(short));

      for(pColumn =(dsc_column*)((char*)Fields+Fields->totalleng),xParm=0;
          xParm<Fields->Ncolumns;
          pColumn =(dsc_column*)((char*)pColumn+pColumn->totalleng),++xParm){
        if(pColumn->type == Tldata) {
          pDescrs->paramNumber = xParm;
          pDescrs->paramShift  = pColumn->shift;
          ++pDescrs;
        }
      }
    }
  }

  /* трансляция оператора выборки данных ? */
  if (Node->mode < MODIFICATION) {
    int   lalign    = 0, palign;
    int   lchar     = 0, pchar;
    int   lvar      = 0;
    int   nvar      = 0;
    int   total     = sizeof(short);
    int   nldata    = 0;
    int   xColumn;

    WrOp(Mbegfetch);                    /*открываем переход на сканер */

    /* считаем память под выводной буфер, цифровые и текстовые поля */
    forlist(proj,Node->projlist) {
      switch (proj->expr->type) {
      case Tchar:
         lchar  += proj->expr->leng;
      break; case Tvar :
         lvar   += proj->expr->leng; ++nvar;
      break; case Tldata:
         nldata++;
      default :
         lalign += proj->expr->leng;
      }
    }
    Pvarout= (pchar= (palign= total+nvar*sizeof(short)) + lalign)+lchar;
    ABlock->ptrs.outbuf = GenBuffer(Pvarout+lvar);
    /* заносим адрес 0-го var (или длину всей записи, если без var*/
    ((short*) RealAddr(ABlock->ptrs.outbuf))[nvar] =
                                           Pvarout - nvar*sizeof(short);

    /* заведем буффер для описателей выходных longdata полей */
    /* для каждого поля потом занесем его # и смещение       */
    if(nldata != 0) {
      ABlock->ptrs.ldataColumns =
        GenBuffer(sizeof(short)+sizeof(ldataColumnDescr)*nldata);
      *(short*)RealAddr(ABlock->ptrs.ldataColumns) = nldata;
    }
    

    /* заполняем адреса для выходных полей */
    xColumn = 0;
    forlist(proj,Node->projlist) {
      proj->location = ABlock->ptrs.outbuf;
      switch (proj->expr->type) {
      case Tchar:
        proj->location += pchar;
        pchar  += proj->expr->leng;
      break; case Tvar :
        proj->location += --nvar * sizeof(short);
      break; case Tldata:
        { ldataColumnDescr* pCol = (ldataColumnDescr*)
            (RealAddr(ABlock->ptrs.ldataColumns)+sizeof(short)) + --nldata; 
          pCol->columnNumber = xColumn;
          pCol->columnShift  = palign;
        }
      default   :
        proj->location += palign;
        palign += proj->expr->leng;
      }
      ++xColumn;
    }

  } else if (Node->mode != CRINDEX) {            /* модификация данных*/
    WrPar(Minitmod,                              /* IniUpdate(tabnumb)*/
      (Node->mode == INSERT || Node->mode == INSVALUE ?
           Node->body.I.tablenum : Node->body.M.subq->body.T.tablenum));

    if ( Node->mode != DELETE ) {                /* нужен буфер записи*/
      dsc_table* descr =
          (Node->mode == UPDATE ? Node->body.M.tabdescr :
                                  Node->body.I.tabdescr);
      ABlock->ptrs.outbuf = GenBuffer(descr->Mtotlength);

      /* адрес 0-го Var или длина всей записи, если без Var */
      ((short*) RealAddr(ABlock->ptrs.outbuf))[descr->Nvarlength] =
                                                      descr->Mfixlength;

      /* занесем адреса полей вносимой записи */
      forlist(proj,Node->projlist) proj->location+= ABlock->ptrs.outbuf;

      if(Node->mode==INSERT) Node->projlist = NIL;/* т.к. все в subq  */
    }
  } else {                                       /* Create Index      */
    dsc_table* descr   = Node->body.C.tabdescr;
    ABlock->ptrs.outbuf= GenBuffer(descr->Mtotlength);
  }

  /* Подготовка подзапросов */
  { eqref escan;
    forlist(escan,ParseHeader->eqlist) {
      expr resExpr = escan->eqNode->projlist->expr;
      escan->flagLocation = GenWork(sizeof(short),Tshort);
      escan->resLocation  = escan->mode == Eexists ? GenWork(sizeof(short),Tshort) :
                                                     GenWork(resExpr->leng,resExpr->type);
      escan->retAddr      = Cnil;
      escan->entryAddr    = Cnil;
      /* если запрос не зависит ни от чего, сгенерим обнуление флага */
      if(escan->extprojs == NIL) WrOpAddr(MsetSconst,0,escan->flagLocation);
    }
  }

  /* трансляция самого запроса */
  result = TransNode(Node);

  if (Node->mode < MODIFICATION ) {              /* запрос на выборку */
    /* генерируем последовательность выхода */
    WrOp(Mretfetch);                             /*lda r; spa ret; rts*/
    PutJump(result.getnext,Juc);                 /*   bru getnext     */

    /* генерируем последовательность выхода при конце таблицы */
    DefJump(result.endjump,Pcode);

    WrOp(Mendfetch);                             /* spa ret; rts NIL; */

  } else {                                       /* операции модификац*/
    WrOp(Node->mode!=CRINDEX ? MinitU            /* IniUscan(&ModScan)*/
                             : Mmovrec);         /* Move(rec,l,outbuf)*/

    switch (Node->mode) {
    case INSVALUE: case INSERT:
      WrPar(Minsert,FALSE);                      /* Insert(Scan,rec,F)*/
    break; case DELETE:
      WrPar(Mdelete,FALSE);                      /* DelRecord(ModScan)*/
    break; case UPDATE:
      /* сгенерим список модиф-мых полей */
      {
        Caddr shortjump;
        short* fieldlist       = Node->body.M.fieldlist;
        ABlock->ptrs.modfields =GenBuffer((*fieldlist+1)*sizeof(short));

        MVS( (char*) fieldlist, (*fieldlist+1)*sizeof(short),
             (char*) RealAddr(ABlock->ptrs.modfields));

        WrOp(Mupdate);                           /* UpdRecord(Mod,...)*/

        shortjump = PutPJump(Cnil,Jeq);          /*   baz endupdate   */

        WrOp(MinitU);                            /* IniUscan(&ModScan)*/
        WrPar(Mdelete,TRUE);                     /* DelRecord(ModScan)*/

        WrOp(MinitU);                            /* IniUscan(&ModScan)*/
        WrPar(Minsert,TRUE);                     /* InsRecord(.....,T)*/
        DefJump(shortjump,Pcode);                /* endupdate equ $   */
      }

    break; case CRINDEX:
      WrPar(Minsind,Node->body.C.index);         /* InsIndex(ni,Zadr,)*/
    }

    if(Node->mode != CRINDEX) {                  /* считаем количество*/
      WrOp(Mcountmod);                           /* модифицированных  */
    }
    if(Node->mode != INSVALUE) {                 /* зацикливаем запрос*/
      PutJump(result.getnext,Juc);               /*   bru getnext     */
      DefJump(result.endjump,Pcode);             /*en equ $           */
    }
    if(Node->mode != CRINDEX) {                  /* вернем кол-во мод.*/
      WrOp(Mmodret);
    } else {
      WrOp(Mindret);                             /*   lda = 0; rts    */
    }
  }

  /* теперь оттранслируем все подзапросы (с конца, с внешних) */
  while(ParseHeader->eqlist != NIL) {
    eqref      theRef;
    Caddr      doneJump = Cnil;
    nodedescr  eqRes;
    { eqref* eqscan = &ParseHeader->eqlist;
      while((*eqscan)->next != NIL) eqscan = &(*eqscan)->next;
      theRef = *eqscan; *eqscan = NIL;
    }

    /* Обработаем список ссылок на точку входа */
    DefJump(theRef->entryAddr,Pcode);
    /* Проверим вычисленность */
    WrOpAddr(McmpSconst,0,theRef->flagLocation);
    doneJump = PutPJump(Cnil,Jne);

    if(theRef->mode == Eexists) {
      WrOpAddr(MsetSconst,0,theRef->resLocation);
    }

    eqRes = TransNode(theRef->eqNode);
    if(theRef->mode == Eexists) {
      WrOpAddr(MsetSconst,1,theRef->resLocation);
      DefJump(eqRes.endjump,Pcode);
      WrOpAddr(MsetSconst,1,theRef->flagLocation);
    } else {
      projection resProj = theRef->eqNode->projlist;
      /* проверим единственность */
      WrOpAddr(Mchuniq,1,theRef->flagLocation);
      GenLoad (resProj->location,  resProj->expr->type,resProj->expr->leng);
      GenStore(theRef->resLocation,resProj->expr->type,resProj->expr->leng);
      PutJump(eqRes.getnext,Juc);
      DefJump(eqRes.endjump,Pcode);
      /* проверим существование */
      WrOpAddr(Mchuniq,0,theRef->flagLocation);
    }
    DefJump(doneJump,Pcode);
    if(theRef->mode == Eexists) {
      WrOpAddr(McmpSconst,1,theRef->resLocation);
    }
    DefJump(theRef->retAddr,PutPJump(Cnil,Juc));
  }

  /* генерируем описатель входной/выходной строки в выходном буфере */
  if (Node->mode != CRINDEX) {
    /* заменим адреса колонок на смещения */
    if(Node->mode < MODIFICATION) {
      forlist(proj,Node->projlist) proj->location -= ABlock->ptrs.outbuf;
    }
    Describe(Node,Code,Fields);
  }
  return(Code);
}

/*
 * Трансляция узла
 */
recursive nodedescr TransNode(theNode) node theNode; {
  nodedescr retcode;

  /* подготовим трансляцию проекции */
  {
    projection proj;
    /* для всех проекций выделяем память, если это не сделано в
       об'емлющем запросе */
    forlist(proj,theNode->projlist) {
      expr value = proj->expr;
      if(proj->location != Cnil) {             /* память уже сделана  */
        if(value->type != Tvar) {              /* хотя для 1-го можно */
          /* если нам распредилили память, а мы = нижней проекции, то */
          /* попробуем заставить вычислить нижнюю проекцию прямо в нас*/
          if (value->mode == Eproj &&
                  value->body.projection->location == Cnil) {
            value->body.projection->location = proj->location;
          } else if(value->mode == Eredref &&  /* аналогично         */
                   value->body.reduction->location == Cnil) {
            value->body.reduction->location = proj->location;
          }
        }
      }


      /* занесение константных полей, для проекций, совпадающих с
                    проекциями низлежащего запроса - совмещаем память */
/*    if(value->type != Tvar) {
/*      if (value->mode == Econst) {
/*        /* вообще-то 0-ой var можно тоже здесь обработать */
/*        MVS(value->body.constvalue,value->leng,
/*                                            RealAddr(proj->location));
/*        *(size_t*) &proj->expr ^= 1;         /* "не вычислять"      */
/*      } else if (value->mode == Eproj &&     /* просто присвоение   */
/*                 value->body.projection->location == Cnil) {
/*        value->body.projection->location = proj->location;
/*        *(size_t*) &proj->expr ^= 1;         /* "не вычислять"      */
/*      }
/*    } */
    }
  }

  TransDoList(theNode->doBefore,Cnil,FALSE);

  /* Транслируем операции выборки */
  switch(theNode->mode) {
  case SELECT: {                              /* операция селекции    */
/*  exprlist pred; */
    retcode = TransNode(theNode->body.S.subq);
/*  forlist(pred,Node->body.S.predlist) {     /*вычислим все предикаты*/
/*    LoadPred( pred->expr,retcode.getnext,FALSE);
/*  }*/

  } break; case CRINDEX: {                    /* сканер для Create Ind*/
    retcode = TransTable(theNode);
    ABlock->ptrs.modscan = TableScan;

  } break; case ATTRIBUTES: {             /* сканер для доступа к Attr*/
    retcode = TransAttr(theNode);

  } break; case TABLE: {                      /* сканер таблицы       */
/*  exprlist pred; */
    retcode = TransTable(theNode);
/*  forlist(pred,Node->body.T.predlist) {     /*вычислим все предикаты*/
/*    LoadPred(pred->expr,retcode.getnext,FALSE);
/*  }*/

  } break; case CATALOG: {
    /* сгенерим буфер под "запись" и счетчик */
    Caddr      catscan;
    catscan = GenBuffer( (theNode->body.A.regime == 0 ?
                             TS_Fname+MaxFileName+MaxIndxName :
                             US_leng) + sizeof(short));

    WrOpAddr(MsetSconst,0,catscan);               /*  clr  scnner[-2] */
    catscan += sizeof(short);

    if(theNode->body.A.regime == 0) {
      /* указатель на начало var-поля (имя файла) */
      ((short*) RealAddr(catscan))[2]= TS_Fname - /* длина фикс. части*/
                                               sizeof(short)*2;
    }
    TableScan = catscan;

    retcode.getnext= Pcode;                       /* nex equ $        */

    WrOpAddr(Mcatscan,(char)theNode->body.A.regime,/*NextCatal(scan)  */
             catscan);
                                                  /*  baz endjump     */
    retcode.endjump = PutPJump(Cnil,Jend);

  } break; case JOIN:  {                       /* соединение          */
    retcode.endjump = Cnil;
    {
      nodelist  list;                          /* список подзапросов  */
      forlist(list,theNode->body.J.subqs) {
        nodedescr retsubq;
        retsubq = TransNode(list->node);       /* транслируем подзапр.*/
        if(retcode.endjump == Cnil) {          /* внешний цикл -> его */
          retcode.endjump = retsubq.endjump;   /* конец->конец всему  */
        } else {                               /* внутренний конец    */
          DefJump(retsubq.endjump,retcode.getnext);
        }
        retcode.getnext=retsubq.getnext;
      }
    }

/*  { /* вычисляем предикаты слияния */
/*    exprlist pred;
/*    forlist(pred,Node->body.J.predlist) {
/*      LoadPred(pred->expr,retcode.getnext,FALSE);
/*    }
/*  } */

  } break; case UNION: {                       /* об'единение         */
    nodelist   list;                           /* список подзапросов  */
    projection pscan;
    Caddr      endunion;
    Caddr      nextaddr;
    Caddr      jumpaddr;

    endunion = PutPJump(Cnil,Juc);            /*      bru jmp0        */
    retcode.getnext = Pcode;                  /* next equ $           */
    nextaddr = PutPJump(Cnil,Juc);            /*      bru next        */
    DefJump(endunion,Pcode);                  /* jmp0 equ $           */

    endunion = Cnil;
    forlist(list,theNode->body.U.subqs) {
      nodedescr retsubq;
      /* занесем все константные поля */
      forlist(pscan,list->node->projlist) {
        expr value;
        value = pscan->expr;
        /* занесение константных полей */
        if(value->type != Tvar && value->mode == Econst) {
          LoadArithm(value);                   /* загружаем ее        */
          if(pscan->location == Cnil)
            pscan->location = GenWork(value->leng,value->type);
          GenStore(pscan->location,value->type,value->leng);
          ClearEqDone(pscan);
        }
      }
      jumpaddr = WrOpLeng(Mlea,0,Cnil,0);

      retsubq  = TransNode(list->node);

      /* заносим в NextAddr адрес перехода */
      ((Mlcode*)RealAddr(jumpaddr))->addr = retsubq.getnext;
      ((Mlcode*)RealAddr(jumpaddr))->saddr= nextaddr;

      if(list->next != NIL) {                  /* не последний эл-нт  */
        projection pscan1;                     /* совмещаем проекции  */
        for (pscan = list->node->projlist,     /* элементов в памяти  */
             pscan1=list->next->node->projlist;
             pscan != 0; pscan = pscan->next, pscan1= pscan1->next) {
          if(list == theNode->body.U.subqs && pscan->location == Cnil) {
            pscan->location = GenWork(pscan->expr->leng,pscan->expr->type);
          }
          if (pscan->location == Cnil ||
                     pscan1->location != Cnil) myAbort(898);
          pscan1->location = pscan->location;
        }
        endunion = PutPJump(endunion,Juc);     /* прыгаем на конец    */
        DefJump(retsubq.endjump,Pcode);        /* начало следующего   */
      } else {                                 /* последний элемент   */
        retcode.endjump=retsubq.endjump;
      }
    }
    DefJump(endunion,Pcode);

  } break; case ORDER: case GROUP: {           /* сортировка/группиров*/
    if(theNode->body.O.doSort) {
      Caddr      scanner  = SortRecord(theNode);
      SortScan*  scanbody = ((SortScan*) RealAddr(scanner))-1;
      projection totallist;

      WrOpAddr(Mbegsort,0,scanner);            /* IniSort(&SortScan)  */
      retcode  = TransNode(theNode->body.O.subq);
      WrOpAddr(Mputsort,0,scanner);            /* PutSort(&SortScan)  */
      PutJump(retcode.getnext,Juc);            /*     bru nextrec     */
      DefJump(retcode.endjump,Pcode);          /* end equ $           */
      WrOpAddr(Mfinsort,0,scanner);            /* FinSort(&SortScan)  */

      scanbody->copyOut = theNode->body.O.doCopy;
      if(!scanbody->copyOut) {
        /* адреса выходных полей сортировки -> в смещения под Z  */
        forlist(totallist,theNode->body.O.subq->projlist) {
          totallist->location -= scanner;
        }

        /* для GROUP смещения полей группирования -> обратно в адреса */
        if(theNode->mode == GROUP) {
          collist    grouplist;
          forlist(grouplist,theNode->body.O.orderlist) {
            grouplist->sortproj.proj->location += scanner;
          }
        }
      }

      retcode.getnext= Pcode;

      WrOpAddr(Mgetsort,0,scanner);            /* GetSort(&SortScan)  */
      retcode.endjump = PutPJump(Cnil,Jend);   /*     baz endjump     */

      /*обнулим все зависящие от нас equery (проекции "перевычислены")*/
      forlist(totallist,theNode->body.O.subq->projlist) {
        ClearEqDone(totallist);
      }
      /* вычисляем редукции */
      if(theNode->mode == GROUP) {
        Caddr count    = scanner-sizeof(SortScan) +
                                         offsetof(SortScan,groupcount);
        Caddr evaljump = TransReduc(theNode,count);
        WrOpAddr(Mgetsort,0,scanner);          /* GetSort(&scanner)   */
        PutJump(evaljump,Jle);                 /* cmp =1; bne eval    */
        TransEndRed(theNode,count);
      }


    } else if(theNode->mode != GROUP) {        /* ORDER без сортировки*/
      retcode  = TransNode(theNode->body.O.subq);

    } else {
      /* Генерируемый код:
       *        endFlag := FALSE                    Если нет группирования:
       *        count   := 0
       *        SUBQUERY start
       * sNext: SUBQUERY cont
       *        if count == 0       goto first  |   count   := count + 1
       *        if newKeys!=oldKeys goto endGr  |   if count != 1 goto eval
       *        count   := count + 1;           |
       *        goto eval                       |
       * next:  if endFlag          goto end    |
       * first: oldKeys := newKeys              |
       *        count   := 1;
       *
       *        CALCRed start
       * eval:  CALCRed cont
       *        goto    snext
       *                                        | next: goto end
       * sEnd:  if count == 0      goto end
       *        endFlag  = TRUE;
       * endGr: FINRed
       *
       */
      Caddr     endFlag   = GenWork(sizeof(short),Tshort);
      Caddr     count     = GenWork(sizeof(long ),Tlong );
      Caddr     evalJump;
      Caddr     groupJump = Cnil;
      nodedescr subNode;
      collist   grouplist;
      int       xGroup;

      WrOpAddr(MsetSconst,0,endFlag);
      WrOpAddr(MsetLconst,0,count);
      /* запомним адреса выходных проекций группировки */
      /* подставим Cnil для вычисления subq           */
      xGroup = 0;
      forlist(grouplist,theNode->body.O.orderlist) {
        theNode->body.O.groupvalues[xGroup++] = 
            grouplist->sortproj.proj->location;
        grouplist->sortproj.proj->location = Cnil;
      }

      subNode  = TransNode(theNode->body.O.subq);
      WrOpAddr(McmpLconst,0,count);            /* CMP.L 0,count       */
      if(theNode->body.O.orderlist != NIL) {
        Caddr     firstJump;

        firstJump = PutPJump(Cnil,Jeq);        /* BE    first         */

        /* сравним значения ключей  */
        xGroup = 0;
        forlist(grouplist,theNode->body.O.orderlist) {
          projection proj = grouplist->sortproj.proj;
          datatype type = proj->expr->type;
          short    leng = proj->expr->leng;
          if(theNode->body.O.groupvalues[xGroup] == Cnil) {
            theNode->body.O.groupvalues[xGroup] = GenWork(leng,type);
          }
          GenLoad (theNode->body.O.groupvalues[xGroup],type,leng);
          if(type < Tchar) {                   /* арифметич. сравнение*/
            WrOpAddr((unchar)(Mshortop+(type-Tshort)),
                     (unchar)(Ecmp-Earithm),proj->location);
          } else {                             /*  сравнение  строк   */
            WrOpLeng(Mcmpstr,type==Tvar,proj->location,leng);
          }
          groupJump = PutPJump(groupJump,Jne); /*        BNE endgroup */
        }
        WrOpAddr(MadmLconst,1,count);          /*        ADD  1,count */
        evalJump = PutPJump(Cnil,Juc);         /*        BRU eval     */

        /* точка входа для следующей группы*/
        retcode.getnext = Pcode;
        /* проверим, что не конец         */
        WrOpAddr(McmpSconst,0,endFlag);        /*        TST endFlag  */
        retcode.endjump = PutPJump(Cnil,Jne);  /*        BNE END      */

        /* обработка первой записи группы */
        DefJump(firstJump,Pcode);              /* first  EQU  $       */
        WrOpAddr(MsetLconst,1,count);          /*        MOV 1,count  */

        /* запишем значение ключей                   */
        /* и подменим адреса (для вычислений редукций)*/
        xGroup = 0;
        forlist(grouplist,theNode->body.O.orderlist) {
          projection proj = grouplist->sortproj.proj;
          datatype type = proj->expr->type;
          short    leng = proj->expr->leng;
          GenLoad (proj->location,type,leng);
          GenStore(proj->location=theNode->body.O.groupvalues[xGroup],
                                  type,leng);
        }

      } else {
        WrOpAddr(MadmLconst,1,count);         /*        ADD  1,count */
        evalJump = PutPJump(Cnil,Jne);        /* count++ != 0 -> eval*/
      }

      /* вычисляем редукции */
      DefJump(evalJump,TransReduc(theNode,count)); /*    calc MIN,... */
      PutJump(subNode.getnext,Juc);            /*        JUMP NEXT    */

      if(theNode->body.O.orderlist == NIL) {
        retcode.getnext = Pcode;               /* getnext EQU $       */
        retcode.endjump = PutPJump(Cnil,Juc);  /*         BRU END     */
      }
      
      /* Обработаем конец подзапроса */
      DefJump(subNode.endjump,Pcode);          /* subend EQU $        */
      WrOpAddr(McmpLconst,0,count);            /*        TST count    */
      retcode.endjump = PutPJump(retcode.endjump,Jeq); /*BZ  END      */
      WrOpAddr(MsetSconst,1,endFlag);          /*        MOV 1,endFlag*/

      DefJump(groupJump,Pcode);                /* endgr  EQU $        */
      TransEndRed(theNode,count);
    }

  } break; case INSVALUE: case INSERT: {       /* вставка записей     */
    Caddr       scanner;
    UpdateScan* scanbody;
    scanner  = GenBuffer(sizeof(UpdateScan));
    scanbody = (UpdateScan*) RealAddr(scanner);
    ABlock->ptrs.modscan = scanner;

    scanbody->ident.table = theNode->body.I.tablenum;
    if(theNode->mode == INSERT) {
      retcode = TransNode(theNode->body.I.subq);
    }

  } break; case DELETE  : case UPDATE: {       /* удаление записей    */
/*  exprlist pred; */
    retcode = TransNode(theNode->body.M.subq); /* подзапрос           */
    ABlock->ptrs.modscan = TableScan;
/*  forlist(pred,Node->body.M.predlist) {      /* предикаты           */
/*    LoadPred(pred->expr,retcode.getnext,FALSE);
/*  }
*/

  } break; default:
    ExecError(ErrSyst);
  }


  /* теперь транслируем все doAfter */
  TransDoList(theNode->doAfter,retcode.getnext,TRUE);
  return(retcode);
}

/*
 * Доступ к таблице
 */
static recursive nodedescr TransTable(Node) node Node; {
  nodedescr   retcode;
  Caddr       scanner;
  UpdateScan* scanbody;
  int         indexLeng = 0;          /* если по index-у ->длина ключа*/
  retcode.endjump = Cnil;


  /* если доступ по индексу, то вычисляем длину и смещение поля-ключа */
  if (Node->mode==TABLE && Node->body.T.path != Node->body.T.tablenum) {
    int i;
    indcolumn* pcol = Files[Node->body.T.path].b.indexdescr.columns;
    for(i=0; i<MaxIndCol & pcol->shift>=0; ++i) {
      indexLeng += pcol->length;
      ++pcol;
    }
  }

  /* генерим сканер  */
  TableScan = scanner  =
              GenBuffer(indexLeng == 0 ? sizeof(UpdateScan) :
                              IndexScanSize +indexLeng);

  scanbody = (UpdateScan*) RealAddr(scanner);

  /* и доступ к нему */

  scanbody->ident.table = (Node->mode==TABLE ? Node->body.T.tablenum
                                             : Node->body.C.tablenum);

  if ( indexLeng != 0) {                     /* доступ по индексу     */
    exprlist pred;
    scanbody->scantable = Node->body.T.path;
    ((IndexScan*) scanbody)->keyLeng  = indexLeng;

    if((pred=Node->body.T.pathpred) != NIL){ /* индекс исп. для выбора*/
      short iPosition = 0;
      int   colNum    = 0;
      do {
        expr     col;
        expr     arg;
        datatype type;

        if(pred->expr->mode != Eeq) ExecError(ErrSyst);
        col   = pred->expr->body.binary.arg1;
        arg   = pred->expr->body.binary.arg2;
        if (col->mode != Ecolumn) ExecError(ErrSyst);
        if (col->body.column.shift != 
            Files[Node->body.T.path].b.indexdescr.columns[colNum].shift)
                    ExecError(ErrSyst);

        LoadArithm(arg);                     /* транслируем выраж"Key"*/

        if((type = col->type) == Tvar) {     /* занесение varchar-поля*/
          WrOpLeng(Mstorestr,2,iPosition+scanner+IndexScanSize,col->leng);
          WrOp(Mloadleng);                   /* возьмем его длину,если*/
          if(iPosition != 0) {               /* были другие, прибавим */
            Caddr oldLeng = GenBuffer(sizeof(short));
            *(short*)RealAddr(oldLeng) = iPosition;
            WrOpAddr(Mshortop,Eadd-Earithm,oldLeng);
          }                                  /* запишем длину ключа   */
          WrOpAddr(Mshortst,0,VirtAddr((char*)&((IndexScan*)scanbody)->keyLeng));
        } else {
          /*кладем поле в keyBody*/
          GenStore(iPosition+scanner+IndexScanSize,type,col->leng);
        }
        iPosition += col->leng;
        ++colNum;
      } while ((pred = pred->next) != NIL);
      pred = Node->body.T.pathpred;

      WrOpAddr(Mcnvkey,0,scanner);           /* ConvKey()             */
      WrOpAddr(Mlookind,0,scanner);          /* LookIndex(&IndexScan) */

    } else {                                 /* нет предикатов        */
          /* начинаем полный просмотр по индексу */
      WrOpAddr(Mbegsind,0,TableScan);        /* IniFIndex(&IndexScan) */

    }
    retcode.endjump =                        /*     baz endjump       */
               PutPJump(retcode.endjump,Jeq);
    retcode.getnext = Pcode;                 /* nex equ $             */

    WrOpAddr(Mnxtind,0,TableScan);           /* NextIndex(&scanner)   */
    if (pred != NIL) {                       /* идет селекция индекса */
      retcode.endjump =
           PutPJump(retcode.endjump,Jle);    /*     ble endjump       */
    } else {                                 /* нет селекции->след.кл.*/
      PutJump(retcode.getnext,Jeq);          /*     baz @continue     */
      retcode.endjump =
           PutPJump(retcode.endjump,Jlt);    /*     ban endjump       */
    }

    WrOpAddr(Mgetrec,0,TableScan);           /* GetRecord(&scanner)   */

    /* повтор при чтении перенесенной вперед */
    PutJump(retcode.getnext,Jend);           /*     baz @getnext      */

  } else {

    scanbody->scantable = NoFile;            /* флаг "не по индексу"  */
    WrOpAddr(Mbegscan,0,scanner);            /* InitScan(&scanner)    */
    retcode.getnext = Pcode;                 /* nex equ $             */

    WrOpAddr(Mnxtrec,0,scanner);             /* NxtRecord(&scanner)   */

    retcode.endjump =                        /*     baz endjump       */
             PutPJump(retcode.endjump,Jend);
  }

  return(retcode);
}


/*
 * Доступ к атрибутам &COLUMNS(xxx), &ACCESS(xxx)
 */
static recursive nodedescr TransAttr(Node) node Node; {
  nodedescr   retcode;
  Caddr       scanner;
  retcode.endjump = Cnil;

  /* резервируем место под "запись" и указатели перед ней */
  scanner = GenBuffer(
     (Node->body.A.regime ==1 ? CS_Fname+MaxColName :
                                AS_leng             )+ sizeof(short)*2);

  /* номер таблицы -> в сканер */
  *(short*) RealAddr(scanner) = Node->body.A.tablenum;
  scanner += sizeof(short);

  WrOpAddr(MsetSconst,0,scanner);            /* обнулим ук-тель       */
  scanner += sizeof(short); TableScan = scanner;

  /* указатель на начало var-поля (имя столбца) */
  if(Node->body.A.regime == 1) {
    ((short*) RealAddr(scanner)) [1] = CS_Fname - sizeof(short);
  }

  retcode.getnext=Pcode;                     /*       nex equ $       */

  WrOpAddr(Mcolscan,(unchar)Node->body.A.regime,/* NextCatal(scan,1)  */
             scanner);

  retcode.endjump =                          /*       baz endjump     */
                 PutPJump(retcode.endjump,Jend);
  return(retcode);
}

/*
 * Обработка редукций
 * вызывается после того, как очередная выходная строка SORT считана
 */
static Caddr TransReduc(node theNode, Caddr countAddr) {
  reduction  reduce;
  Caddr      evaljump;
  Caddr      summBuf;
  short      nullleng = 0;

  /* считаем длину обнуляемого буфера (для summ, avg) */
  forlist(reduce,theNode->body.O.reductions) {
    if(reduce->func == Rsumm || reduce->func == Ravg) {
      reduce->location = nullleng;
      nullleng += reduce->arg1->leng;
    }
  }

  /* генерируем буффер для подсчета summ и avg */
  summBuf = GenBuffer(nullleng);

  /* Обнуляем буфер суммирований */
  while (nullleng != 0) {
    unchar slice = (unchar) Min(nullleng,0xFC);
    nullleng -= slice;
    WrOpAddr(Mclrbuf,slice,summBuf+nullleng);
  }

  /* начало кода вычислений в цикле для группы */
  evaljump = Pcode;                         /* eval    equ $          */

  /* вычисляем все редукции */
  forlist(reduce,theNode->body.O.reductions) {
    if(reduce->func != Rcount) {
      expr  arg1 = reduce->arg1;            /* выражение редукции     */
      datatype type = arg1->type;

      switch(reduce->func) {
      case Ravg : case Rsumm:
        reduce->location += summBuf;        /* адрес результа         */
        LoadArithm(arg1);                   /* вычислим его и возьмем */
        WrOpAddr((unchar)(Mshortop+(type-Tshort)),
                          (unchar)(Eadd-Earithm), reduce->location);
        GenStore(reduce->location,type,arg1->leng);
      break; case Rmin: case Rmax:          /* минимум, максимум:     */
             case Rmin2: case Rmax2: {
        Caddr  noStoreJump;
        Caddr  storeJump;
        Caddr  cmpLoc      = Cnil;
        Caddr* cmpLocation;
        if(reduce->arg2 != NIL) {
          if(reduce->location == Cnil) 
             reduce->location = GenWork(reduce->arg2->leng,reduce->arg2->type);
          cmpLocation = &cmpLoc;
        } else {
          cmpLocation = &reduce->location;
        }
        if(*cmpLocation == Cnil) *cmpLocation = GenWork(arg1->leng,type);

        LoadArithm(arg1);                   /* вычислим его и возьмем */

        /* генерим обход, если первая запись */
        WrOpAddr(McmpLconst,1,countAddr);   /* cmp groupcount,1       */
        storeJump = PutPJump(Cnil,Jeq);     /* beq setvalues          */
        if(type < Tchar) {                  /* сравниваем со старым   */
          WrOpAddr((unchar)(Mshortop+(type-Tshort)),
                          (unchar)(Ecmp-Earithm), *cmpLocation);
        } else {
          WrOpLeng(Mcmpstr,type==Tvar,*cmpLocation,arg1->leng);
        }

        /* генерим обход, если >= Min или <= Max */
        noStoreJump = PutPJump(Cnil,reduce->func == Rmin ||
                                    reduce->func == Rmin2 ? Jge : Jle);
        DefJump(storeJump,Pcode);
        GenStore(*cmpLocation,type,arg1->leng);
        if(reduce->arg2 != NIL) {
          LoadArithm(reduce->arg2);
          GenStore(reduce->location,reduce->arg2->type,reduce->arg2->leng);
        }
        DefJump(noStoreJump,Pcode);
      } break; default:
        ExecError(ErrSyst);
      }
    } else {
      reduce->location = countAddr;
    }
  }
  return(evaljump);
}

/*
 * Обработка редукций
 * вызывается после того, как встретили конец группы
 * окончательные вычисления над группой (средние)
 */
static void TransEndRed(node theNode, Caddr countAddr) {
  reduction  reduce;
  Caddr      FcountAddr = Cnil;          /* доступ к "float(count(*))"*/
  forlist(reduce,theNode->body.O.reductions) {
    if (reduce->func == Ravg) {
      expr     arg  = reduce->arg1;
      short    leng = arg->leng;
      datatype type = arg->type;

      /* для float преобразуем счетчик элементов из long в float */
      if(type == Tfloat && FcountAddr == Cnil) {
        FcountAddr = GenWork(sizeof(real),Tfloat);
        GenLoad(countAddr,Tlong ,sizeof(long));
        WrPar(Mfloatcv,(Tlong-Tshort));
        GenStore(FcountAddr,Tfloat,sizeof(real));
      }
      GenLoad(reduce->location, type,leng);
      WrOpAddr((unchar)(Mshortop+(type-Tshort)),(unchar)(Ediv-Earithm),
                              (type==Tfloat ? FcountAddr : countAddr));
      GenStore(reduce->location, type,leng);
    }
  }
}

/*
 * Генерация структуры записи для сортировки
 * непосредственно перед записью располагается сканнер сортировки, а
 * перед ним - список полей и инверсий:
 *  +-------+-------+-  -+------+-------+-------------------+----
 *  | длина | длина |    |длина | длина |   число кусков    |
 *  |  inv  | норм  |    | inv  |  норм | 1 кусок - 2 слова |
 *  +-------+-------+-  -+------+-------+-------------------+----
 *
 * -+---------------+---------------+   -+-------------------+----
 *  | смещение поля | длина для char|    |   число кусков    |
 *  | в sort-записи | ~тип для числ.|    | 1 кусок - 2 слова |
 * -+---------------+---------------+-  -+-------------------+----
 *
 * -+-------------------+--------------------+
 *  | Сканер сортировки | Сортируемая запись |
 * -+-------------------+--------------------+
 *
 * структура сортируемой записи:
 *
 *  +----------+--------------+---------------+-----------+-----------+
 *  |var-адреса| сорт-ые поля | несорт. числа |несорт.char| несорт.var|
 *  +----------+--------------+---------------+-----------+-----------+
 * Замечание: сортируемые Tvar преобразованы в Tchar
 */

static Caddr SortRecord(Node) node Node; {
  Caddr      scanner;
  SortScan*  scanbody;
  projection totallist;
  collist    orderlist;
  unshort    Mvarlength = 0;                       /* сумм. длина var */
  unshort    begcompare= sizeof(short);            /*начало сорт.части*/
  unshort    endcompare;                           /* конец сорт.части*/
  unshort    vshift;                               /* смещение var    */
  unshort    shift;                                /* смещение fix    */

  int        charflag;
  int        Nnumbers   = 0;                       /* кол-во sort чис.*/
  char*      bufaddr;                              /*адрес sort-записи*/
  short      invbuf[MaxSortCol];                   /*для inv          */
  int        Xinv       = 0;
  int        invsign    = 0;                       /* текущая inv     */
  int        invleng;                              /* текущая длин.inv*/


  /* убираем распределение памяти для проекций subq: распределим сами */
  /* резервируем память под адреса VAR-переменных, считаем их длины   */
  forlist(totallist,Node->body.O.subq->projlist) {
    totallist->location = Cnil;
    if(totallist->expr->type == Tvar) {         /* результат типа var */
      Mvarlength += totallist->expr->leng;
      begcompare += sizeof(short);
    }
  }

  /* распределяем память под сортируемые колонки */
  shift   = begcompare;
  invleng = begcompare;
  forlist(orderlist,Node->body.O.orderlist) {
    short fieldleng;
    orderlist->sortproj.proj->location = shift + 1; /* обеспечим доступ   */
                                                    /* в sort-записи      */
    if((orderlist->sortproj.order == invOrder) != invsign) { /* смена inv~знака    */
      invbuf[Xinv] = invleng; invleng = 0;
      invsign = !invsign;
      if(++Xinv>= MaxSortCol-2) ExecError(ErrComplicated);
    }

    invleng +=                                  /* длина поля         */
               (fieldleng = orderlist->sortproj.proj->expr->leng);
    switch (orderlist->sortproj.proj->expr->type) {
    case Tvar:                                  /* не должно быть     */
      ExecError(ErrType);
    case Tchar:
      if (fieldleng & (AlignBuf-1)) {           /* не выравнено       */
        if(invsign) {                           /* не будем inv       */
          invbuf[Xinv] = invleng; invleng = invsign = 0;
          if(++Xinv>= MaxSortCol-2) ExecError(ErrComplicated);
        }
        fieldleng += AlignBuf-1; invleng += AlignBuf-1;
      }
    }
    ++Nnumbers;                                 /* для перекодировок  */
    shift += fieldleng;
  }

  if(invsign) invbuf[Xinv++] = invleng;         /* посл. порция inv   */

  endcompare = shift;                           /* конец сорт-мых     */
  vshift = begcompare - sizeof(short);          /* адреса var         */

  /* распределяем память под остальные колонки */
  for(charflag=0; charflag<2; charflag++) {     /* чтоб Char  в конце */
    forlist(totallist,Node->body.O.subq->projlist) {

      if (totallist->location==Cnil) {
        switch (totallist->expr->type) {
        case Tvar:                              /* varchar - смещ.адр */
          totallist->location = (vshift -= sizeof(short))  + 1;
        break; case Tchar:                      /* char - после чисел */
          if(charflag==0) break;
        default:                                /* проставим смещения */
          totallist->location  = shift + 1;
          shift += totallist->expr->leng;
        }
      }
    }
  }

         /* заводим буфер под запись        */
  scanner = GenBuffer(shift + (Nnumbers*2+1)*sizeof(short) +
                      Mvarlength +  sizeof(SortScan) +
                      (Xinv+1) * sizeof(short));

  scanner +=
                (Xinv+1)*sizeof(short) + (Nnumbers*2+1)*sizeof(short);

  {      /* заносим адреса для перекодировок */
    short* invaddr = (short*) RealAddr(scanner);
    *--invaddr = Nnumbers;

    forlist(orderlist,Node->body.O.orderlist) {
      expr Expr = orderlist->sortproj.proj->expr;

      /* занесем длину char или -1 для чисел */
      *--invaddr = (Expr->type < Tchar ? ~Expr->type : Expr->leng);

      /* занесем смещение в sort-записи */
      *--invaddr = orderlist->sortproj.proj->location - 1;
    }

    /* число и адреса полей, инвертируемых целиком (для sort "desc") */
    *--invaddr = Xinv/2; invaddr -= Xinv;
    while(--Xinv>=0) *invaddr++ = invbuf[Xinv];
  }

  /* занесем адрес sort-записи, сканера */
  scanbody = (SortScan*) RealAddr(scanner);
  _fill((char*)scanbody,sizeof(SortScan),0);
  scanner += sizeof(SortScan);
  bufaddr  = RealAddr(scanner);

  /* распределяем адреса полей       */
  forlist(totallist,Node->body.O.subq->projlist) {
    totallist->location += scanner-1;
  }


  /* обнуляем сортируемую часть из-за выравнивания строк */
  for(charflag=begcompare; charflag<endcompare; charflag++) {
    bufaddr[charflag] = 0;
  }

  ((short*)(bufaddr+begcompare))[-1]=       /* отн. адрес 0-го string */
                            shift - (begcompare - sizeof(short));

  scanbody->maxlength   = shift+Mvarlength; /* max длина сорт-ой зап. */
  scanbody->begcompare  = begcompare;       /* начало сортируемого    */
  scanbody->endcompare  = endcompare;       /* конец  сортируемого    */
  scanbody->doGroup     = (Node->mode == GROUP);

  return(scanner);
}


/*
 * Вызов после вычисления projection:
 * флаги всех подзаппросов, зависящих от нее, обнуляются
 */
static void ClearEqDone(proj) projection proj; {
  eqref eqscan;
  /* для UNION - возьмем соответствующий из 1-го запроса */
  if(proj->owner->upper != NIL && proj->owner->upper->mode == UNION &&
     proj->owner->upper->body.U.subqs->node != proj->owner) {
    projection px= proj->owner->projlist;
    projection p0= proj->owner->upper->body.U.subqs->node->projlist;
    while(px != proj) {
      if((p0=p0->next) == NIL || (px=px->next) == NIL) ExecError(ErrSyst);
    }
    proj = p0;
  }

  /* проверим все ссылки во всех подзапросах */
  forlist(eqscan,ParseHeader->eqlist) {
    prjlist pscan;
    short   maxLevel = -1;
    boolean found    = FALSE;
    forlist(pscan,eqscan->extprojs) {
      if(pscan->proj == proj) found = TRUE;
      maxLevel = Max(pscan->proj->owner->eqLevel,maxLevel);
    }
    if(found && maxLevel <= proj->owner->eqLevel) {
      WrOpAddr(MsetSconst,0,eqscan->flagLocation);
    }
  }
}

static void TransDoList(exprlist elist, Caddr jumpAddr, boolean after) {
  while(elist != NIL) {
    expr theExpr = elist->expr;
    if(theExpr->mode == Eproj) {
      projection proj = theExpr->body.projection;
      expr     value = proj->expr;
      Caddr subLoc= (value->mode == Eproj   ? value->body.projection->location :
                 value->mode == Eredref ? value->body.reduction->location : Cnil);
      if(proj->location == Cnil && value->type != Tvar) {
        proj->location = subLoc;
      }
      if(proj->location == Cnil) {
        proj->location = GenWork(value->leng,value->type);
      }
      if(proj->location == subLoc) {           /* proj: proj, redref  */
        /* константные проекции занесем прямо в код */
      } else if(value->mode == Econst && value->type != Tvar) {
        MVS(value->body.constvalue,value->leng,
                                           RealAddr(proj->location));
      } else {
        LoadArithm(value);                 /* загружаем ее        */
        GenStore(proj->location,proj->expr->type,proj->expr->leng);
      }
      ClearEqDone(proj);

    } else if(theExpr->type == Tbool) {
      if(!after) ExecError(ErrSyst);
      LoadPred(theExpr,jumpAddr,after ? 0 : 2);
    } else {
      ExecError(ErrSyst);
    }
    elist = elist->next;
  }
}


/*
 * Трансляция арифметических выражений
 */
recursive void LoadArithm(Expr) expr Expr; {
  if(Expr->mode >= Earithm) {                /* бинарная операция     */
    expr  arg2 = Expr->body.binary.arg2;
    Caddr oper = AccArithm(arg2);            /* получаем доступ к arg2*/
    LoadArithm(Expr->body.binary.arg1);

    if (arg2->type < Tchar) {                /* арифметические операц.*/
      WrOpAddr((unchar)(Mshortop+(arg2->type-Tshort)),
       (unchar)((Expr->mode<Ecompare ? Expr->mode:Ecmp)-Earithm),oper);
    } else {                                 /* строки: сравнение     */
      WrOpLeng((Expr->mode >= Ecompare ? Mcmpstr : Mlike),
                                      arg2->type==Tvar,oper,arg2->leng);
    }


  } else if (Expr->mode == Econv) {          /* преобразование типа   */
    LoadArithm(Expr->body.unary);
    if(Expr->type < Tchar &&
       Expr->type !=Expr->body.unary->type){ /* оно и вправду нужно ? */
       WrPar((unchar)(Mshortcv+(Expr->type-Tshort)),
                    (unchar)(Expr->body.unary->type-Tshort));
    }

  } else if (Expr->mode == Esign) {          /* смена знака           */
    LoadArithm(Expr->body.unary);
    WrOp((unchar)(Mshortng+(Expr->type-Tshort)));

  } else if (Expr->mode == Eif  ) {          /* условное выражение    */
    Caddr  fjump,ejump;                      /* переход на ELSE, END  */
    fjump = LoadPred(Expr->body.cond.cond,   /*не выполнилось->ELSE   */
                                        Cnil,FALSE | 2);
    LoadArithm(Expr->body.cond.tvar);        /* выполнилось -> THEN   */
    ejump=PutPJump(Cnil,Juc);                /*      bru  end         */
    DefJump(fjump,Pcode);                    /* else equ  $           */
    LoadArithm(Expr->body.cond.fvar);        /* не выполнилось -> ELSE*/
    DefJump(ejump,Pcode);                    /* end  equ  $           */

  } else if (Expr->mode == Efunc ) {         /* стандартная функция   */
    if(Expr->body.scalar.arg != NIL) LoadArithm(Expr->body.scalar.arg);
    WrPar(Mstndf,(unchar) Expr->body.scalar.func);

  } else {                                   /* доступ к ячейке?      */
    GenLoad(AccArithm(Expr),Expr->type,Expr->leng);
  }
}


/*
 * Трансляция предикатов
 * flag: бит 1 - переход на TRUE/FALSE
 *       бит 2 - метка PostDef
 */
static recursive Caddr LoadPred(expr Expr, Caddr jump, unsigned flag) {
  if (Expr->mode == Eand || Expr->mode == Eor) {
    if((flag&1)^ (Expr->mode == Eand)) {  /* if not and  , if or      */
      jump = LoadPred(Expr->body.binary.arg1,jump,flag);
      jump = LoadPred(Expr->body.binary.arg2,jump,flag);
    } else {                              /* if and ,  if not or      */
      Caddr shortjump;
      shortjump = LoadPred(Expr->body.binary.arg1,Cnil     ,(flag^1)|2);
      jump      = LoadPred(Expr->body.binary.arg2,jump     , flag     );
      DefJump(shortjump,Pcode);
    }

  } else if (Expr->mode == Enot) {
    jump = LoadPred(Expr->body.unary,jump, flag^1);

  } else if (Expr->mode == Elike) {
    unchar cond = (unchar)(Jne + (flag & 1));
    LoadArithm(Expr);
    if(flag & 2) jump = PutPJump(jump,cond); else PutJump(jump,cond);

  } else if (Expr->mode == Eexists) {        /* EXISTS (... )         */
    unchar cond = (unchar)(Jne + (flag & 1));
    TransEqRef(Expr->body.equery);
    if(flag & 2) jump = PutPJump(jump,cond); else PutJump(jump,cond);

  } else if (Expr->mode >= Ecompare) {       /* сравнение данных      */
    unchar cond = (unchar)((Expr->mode ^ 1 ^ (flag & 1))-Ecompare+Jne);
    LoadArithm(Expr);
    if(flag & 2) jump = PutPJump(jump,cond); else PutJump(jump,cond);

  } else {
    ExecError(ErrType);
  }
  return(jump);
}

/*
 * Получение доступа к выражению
 */
recursive Caddr AccArithm(Expr) expr Expr; {
  Caddr  result;
  switch(Expr->mode) {
  case Econst:
    result = GenBuffer(Expr->leng);
    MVS(Expr->body.constvalue,Expr->leng,RealAddr(result));
  break; case Eproj: case Eextproj:         /* ссылка на проекцию       */
    if(Expr->body.projection->location == Cnil) {
      node upNode = Expr->body.projection->owner->upper;
      if(upNode == NIL || (upNode->mode != ORDER && upNode->mode != GROUP)) {
         ExecError(ErrSyst);
      }
    }
  case Etemp:                              /* поле отсортированной    */
    result = Expr->body.projection->location;
  break; case Eredref:                     /* ссылка на редукцию      */
    result = Expr->body.reduction->location;
  break; case Ecolumn:                     /* поле таблицы            */
    result = Expr->body.column.shift;
    if (Expr->body.column.specq) {         /* поле спец-запроса       */
      result += TableScan;
    }
  break; case Eparam:                      /* параметр запроса        */
    result = ABlock->ptrs.inpbuf + Expr->body.parameter->shift;
  break; case Equery:
    TransEqRef(Expr->body.equery);
    result = Expr->body.equery->resLocation;
  break; default:
    LoadArithm(Expr);
    result = GenWork(Expr->leng,Expr->type);
    GenStore(result,Expr->type,Expr->leng);
  }
  return(result);
}


/*
 * Вызов вычисдения подзапроса
 */
static void TransEqRef(theRef) eqref theRef; {
  /* генерим "запись адреса" */
  Caddr leaAddr = WrOpLeng(Mlea,0,Cnil,0);
  ((Mlcode*)RealAddr(leaAddr))->saddr= theRef->retAddr;
  theRef->retAddr = Pcode - sizeof(Caddr);

  /* генерим переход на вычисления */
  theRef->entryAddr = PutPJump(theRef->entryAddr,Juc);

  ((Mlcode*)RealAddr(leaAddr))->addr = Pcode;
}


/*
 * Генерация команды "запись данных"
 */
static void GenStore(Caddr dist, datatype type, short leng) {
  if(type == Tldata) {
    WrOpAddr(Mldatast,0,dist);
  } else if(type == Tchar || type == Tvar) {
    WrOpLeng(Mstorestr,type==Tvar,dist,leng);
  } else if(dist & (AlignBuf-1)) {         /* @ 28Jun92 (for LookIndex)*/
    WrOpAddr((unchar)(Mshortst1+(type-Tshort)),0,dist);
  } else {
    WrOpAddr((unchar)(Mshortst+(type-Tshort)),0,dist);
  }
}

/*
 * Генерация команды "загрузка данных"
 */
static void GenLoad(Caddr dist, datatype type, short leng) {
  if(type < Tchar) {
    WrOpAddr((unchar)(Mshortop+(type-Tshort)), Eload-Earithm,dist);
  } else if(type == Tldata) {
    WrOpAddr(Mldatald,0,dist);
  } else {
    WrOpLeng(Mloadstr,type==Tvar,dist,leng);
  }
}

/*
 * Создание ячейки доступа и тела функции
 * для переменных типа Tvar резервируем слово перед телом для длины и
 *                          заносим отрицательное смещение
 */
static Caddr GenWork(short leng, datatype type) {
  Caddr result;
  short shift = (type == Tvar ? sizeof(short)*2 : 0);
  result = GenBuffer (leng + shift);
  if (shift != 0) ((short*) RealAddr(result))[1] = sizeof(short);
  return(result);
}

/*
 * создание буфера длины "size"
 */
Caddr GenBuffer(short size) {
  Caddr   result;
  CdBlock scan;
  int     i = 0;
  size = Align(size);
  if (size > CdBlockSize - sizeof(struct head0)) ExecError(ErrLeng);
  /* ищем подходящий блок  */
  while((scan = PageMap[i])->head.blocktype != CdData ||
        scan->head.freespace<size) {
    if(++i >= Nblocks) {                   /* создаем новый блок Data */
      scan = GetCdBlock(CdData);
    }
  }

  result = VirtAddr(scan) + CdBlockSize - scan->head.freespace;

  scan->head.freespace -= size;
  return(result);
}

/*
 * Генерация PostDef - переходов
 */
void DefJump(Caddr addr, Caddr value) {
  while(addr != Cnil) {
    Caddr* jumpaddr = (Caddr*) RealAddr(addr);
    addr = *jumpaddr; *jumpaddr = value;
  }
}

/*
 * Запись predef - перехода
 */
static void PutJump(Caddr addr, unchar code) {
  /* маски кодов результата: 1- lt, 2-eq, 4-gt */
  static unchar masks[Jgt+1] = {0x5,0x2,0x6,0x1,0x3,0x4};
  switch(code) {
  case Juc:
    WrOpAddr(Mjump,code,addr);
  break; case Jend:
    WrOpAddr(Mejump,0,addr);
  break; default:
    WrOpAddr(Mcjump,masks[code],addr);
  }
}

/*
 * Запись postdef - перехода
 */
static Caddr PutPJump(Caddr addr, unchar code) {
  PutJump(addr,code);
  return(Pcode - sizeof(Caddr));
}

/*
 * Запись команд без параметров
 */
static void WrOp(unchar code) {WrPar(code,0);}

/*
 * Запись команд с параметром, без адреса
 */
static void WrPar(unchar code, unchar parm) {
  WrOpAddr(code,parm,Cnil);
}

/*
 * Запись команд с параметром, с адресом
 */
static Caddr WrOpAddr(unchar code, unchar parm, Caddr addr) {
  Mcode theCode;
  theCode.oper = code; theCode.parm = parm; theCode.addr=addr;
  return(WriteCode((char*) &theCode,sizeof(theCode)));
}


/*
 * Запись команд с параметром, с адресом и длиной
 */
static Caddr WrOpLeng(unchar code, unchar parm, Caddr addr, unshort leng) {
  Mlcode theCode;
  theCode.oper = code; theCode.parm = parm;
  theCode.addr = addr; theCode.leng = leng; theCode.saddr = Cnil;
  return(WriteCode((char*) &theCode,sizeof(theCode)));
}

/*
 * Запись машинных команд
 */
static Caddr WriteCode(code,leng) char* code; unsigned leng; {
  static Mcode jumppage = {Mjump,Juc,Cnil};

  /* проверим, есть ли место на этой странице кода */
  if (Pblock->head.freespace - sizeof(jumppage) < leng) {
    Pblock->head.freespace -= sizeof(jumppage);
    jumppage.addr = VirtAddr(Pblock = GetCdBlock(CdCode))
                                                 + sizeof(struct head0);

    /* генерируем переход на следующую страницу */
    MVS((char*)&jumppage,sizeof(jumppage),RealAddr(Pcode));

    /* продолжим отсюда */
    Pblock->head.freespace = CdBlockSize - sizeof(struct head0);
    Pcode  = jumppage.addr;
  }

  /* выводим трассировочную информацию */
  if(TraceFlags & TraceCode) {
#if defined(MPW)
    typedef char mnemo[4];
#else
    typedef char mnemo[3];
#endif
    static mnemo mnemocodes[Mend] =
          {"BgF","RtF","EnF","EnI","RtM","BgM","Mco","Fun",
           "BgU","MvR","PtI",
           "INS","DEL","UPD",
           "sCv","lCv","fCv",
           "sNg","lNg","fNg",
           "lLn",
           "Jmp","Jc ","Jen","Clr","Chu","sCn","lCn","sCm","lCm","lAd",
           "ctS","atS",
           "LkI","BgI","NxI","CvK",
           "BgT","GtT","NxT",
           "BgS","PtS","FnS","GtS",
           "sOp","lOp","fOp",
           "sSt","lSt","fSt",
           "sS1","lS1","fS1",                   /* @28 Jun 92 */
           "dLd","dSt",
           "Lik","cCm","cLd","cSt","Lea"};

    static char codedump[] = "Code %4.16 = xxx %2.16%( %4.16)";
    MVS(mnemocodes[((Mcode*)code)->oper],3,codedump+13);
    Reply(Fdump(codedump,Pcode,(unsigned)((Mcode*)code)->parm,
                         (unsigned)(leng-offsetof(Mcode,addr)),
                         (unshort*)&((Mcode*)code)->addr));
  }

  Pblock->head.freespace -= leng;
  MVS(code,leng,RealAddr(Pcode)); Pcode += leng;
  return(Pcode-leng);
}

/*
 * Выделение блока из числа свободных буферов (для генерации кода)
 */
static CdBlock GetCdBlock(type) blocktype type; {
  CdBlock new;
  unchar  owner;
  static  codeaddress* descr;

  if(Fblock == NIL) {                  /* начало генерации кода       */
    descr   = GenCdNum(&Code,&owner);  /* сгенерим номер кода         */
    Nblocks = 0;
  }

  if(Nblocks>=MaxCodeSize) ExecError(ErrComplicated);
  if(Eblock == NIL) GetSpace(1,ABlock);
  new = (CdBlock) Eblock; Eblock = *(CdBlock*) Eblock;


  if(Fblock == NIL) {               /* начало генерации кода          */
    int    i;
    Fblock = new;

    /* формируем карту памяти */
    PageMap = ABlock->PageMap;
    for(i=0; i<MaxCodeSize; ++i) PageMap[i] = NIL;

    descr->outside = FALSE;
    descr->address = new - CodeBuf;
    descr->codesize= 0;

    ABlock->nextloaded = Hblock; Hblock = ABlock;
    ABlock->owner      = owner;
    ABlock->codenum    = Code;
    ABlock->ldataTempFile = 0;

    /* чистим указатели           */
    _fill((char*)&ABlock->ptrs,sizeof(ABlock->ptrs),0);
  }

/*PageMap[descr->codesize++] = new; -- ошибка MarkC */
  PageMap[descr->codesize] = new;
  ++descr->codesize;
  ABlock->Nblocks = (unchar) ++Nblocks;

  new->head.blocktype = type;
  new->head.freespace = CdBlockSize - sizeof(struct head0);

  /* выводим трассировочную информацию */
  if(TraceFlags & TraceCode) {
    Reply(Fdump("CdBlock #%1.16(%c), address =%4.16",
          (unsigned)(Nblocks-1),(unsigned)"IDCF"[type], (unsigned)VirtAddr(new)));
  }
  return(new);
}
