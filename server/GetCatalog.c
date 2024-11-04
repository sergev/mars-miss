                 /* ============================= *\
                 ** ==         M.A.R.S.        == **
                 ** == Выполнение спец-выборок == **
                 \* ============================= */

#include "FilesDef"
#include "AgentDef"
#include "TreeDef"

         zptr     NxtCatal(char*,int);

  /* обращения к в/выводу нижнего уровня*/
  extern zptr     GetPage(page,table,short); /* доступ к блоку        */
  extern boolean  LockElem(lockblock*,short,boolean);

  /* обращения к диспетчеру             */
  extern void     LockAgent(void);           /* передиспетчеризация   */

zptr NxtCatal(buffer,oper) char* buffer; int oper; {
  short* scan = ((short*) buffer) -1;
  switch (oper) {
  case 0: {                                /* каталог таблиц          */
    file_elem* ptr;
    do {
      ptr = Files + (*scan)++;
      if (ptr >= Files + MaxFiles) return((zptr)0);
    } while(( (ptr->status & (BsyFile | DelFile)) != BsyFile) ||
          ((ptr->status & CreFile) && ptr->iR.Xlock.owner!=Agent->ident)||
          ((ptr->status & DrpFile) && ptr->iR.Xlock.owner==Agent->ident)||
          (ptr->kind != tableKind && ptr->kind != indexKind));
    /* теперь формируем нашу "запись" */
    *((long*) (buffer + TS_Fsize)) = (long) ptr->actualsize;
    *((long*) (buffer + TS_Fnrec)) = (long) ptr->recordnum;
    *((short*)(buffer + TS_Fownr)) = ptr->owner;
    if(ptr->kind == tableKind) {          /* это таблица               */
      buffer[TS_Ftype] = 'T';
      MVS(ptr->b.tablename,MaxFileName,buffer+TS_Fname);
      ((short*) buffer) [1] =
               (*(short*) buffer = TS_Fname+MaxFileName)- sizeof(short);
    } else {                             /* это индекс                */
      buffer[TS_Ftype] = 'I';
      MVS(ptr->b.indexdescr.indexname,MaxIndxName,buffer+TS_Fname);
      ((short*) buffer) [1] = TS_Fname+MaxIndxName - sizeof(short);
      MVS(Files[ptr->mainfile].b.tablename,MaxFileName,
           buffer+TS_Fname+MaxIndxName);
      ((short*) buffer) [0] = TS_Fname+MaxIndxName+MaxFileName;
    }

  } break; case 1: case 2: {             /* взять описатель таблицы   */
    zptr     Index;
    unshort  shift = sizeof(page);
    int      Nrecords,i;
    if(Busy( Index = GetPage((page)1,(table)scan[-1],Sltype) ))
                                                          LockAgent();
    Nrecords = DescTable(shift).Ncolumns;
    if(oper == 1) {
      if (*scan>= Nrecords) return((zptr) 0);
      for(i = -1; i< *scan; ++i) shift+= intDATA(shift);
      Index += shift;                    /* Index - Z-adr описателя   */

      /* заносим длину данных в колонке (short-поле) */
      *((short*)(buffer+CS_Fleng)) = DescColumn(0).length;

      /* заносим тип   данных в колонке (char(1)-поле ) */
      buffer[CS_Ftype] = "?SLFCVD" [DescColumn(0).type];

      /* заносим имя колонки  (varchar-поле ) */
      i = DescColumn(0).name[0];          /* длина имени              */
      _ZGmov(Index+(unsigned)(&((dsc_column*)0)->name[1] - (char*)0),
                i,buffer+CS_Fname);
      *(short*) buffer = CS_Fname + i;
    } else {                             /* сканирование доступов     */
      privil elem;
      for(i = -1; i< Nrecords; ++i) shift+= intDATA(shift);
      Nrecords = intDATA(shift);
      if(*scan >= Nrecords) return((zptr) 0);
      elem= pdescr(Index+shift+sizeof(short) + *scan * sizeof(privil));
      *(short*)(buffer + AS_Fuser) = elem.who == nobody ? -1 : elem.who;
      for(i=0; i<4; ++i) {               /* занесем права доступа     */
        buffer[AS_Fread+i] = elem.what & (1<<i) ? '*' : ' ';
      }
    }
    ++*scan;                             /* увеличиваем сканер        */
  } break; case 3: {                     /* доступ к &Users           */
    zptr     Index;
    zptr     shift;
    int      i;
    if (LockElem(&Files[UserFile].iR,Sltype,TRUE ) ||
        Busy(Index=GetPage((page)0,(table)UserFile,Sltype) )
       ) LockAgent();
    do {
      if(*scan >= Nusers) return((zptr) 0);
      shift = Index + sizeof(page) + (*scan)++ *sizeof(logon_elem);
    } while (Byte0DATA(shift) == 0);
    *(short*) (buffer + US_Fuser) = *scan-1;
    _ZGmov(shift,UserName+PasswLng,buffer + US_Fname);
    for(i=0;i<3;++i) {
      buffer[US_Fadmin+i] = LogonDATA(shift).privil & (1<<i)? '*' : ' ';
    }
  }

  }
  return((zptr) buffer);
}
