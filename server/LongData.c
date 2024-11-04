#include "FilesDef"
#include "CodeDef"
#include "AgentDef"

         boolean  InitLDParms (CdBlockH);
         void     StoreLDParms(CdBlockH,char*);
         void     CheckLDParms(CdBlockH);
         short*   LoadLDParms (CdBlockH);

         void     InsLData(ldataDescr*,table);/* вставка в LData       */
         void     DelLData(table,page,long); /* стирание из LData     */

         void     InitLDMap(table,page);

  /* обращения к в/выводу нижнего уровня*/
  extern zptr     GetPage(page,table,short); /* доступ к блоку        */
  extern table    ReqTemp (unchar);          /* выделение temp-файла  */
  extern void     RlseTemp(table);           /*освобождение temp-файла*/
  extern void     SetModified(short,short);  /* отметка блока: модиф-н*/
  extern boolean  LockTable(table,short);    /* блокировка таблиц     */

  /* run-time операции: выдача сообщений об ошибках */
  extern void     ExecError(int);

  /* обращения к диспетчеру             */
  extern void     LockAgent(void);           /* передиспетчеризация   */

typedef page ldMapHeader;
typedef page ldDataHeader;

  enum {effQuant = BlockSize - sizeof(ldDataHeader)-sizeof(page)};

boolean InitLDParms(ABlock) CdBlockH ABlock; {
  boolean retcode = ABlock->ptrs.ldataParams != Cnil;
  if(retcode) {                              /*there are LongData parms*/
    short* bufadr    = (short*) RealAddr(ABlock->ptrs.inpbuf);
    short* pNldata   = (short*) RealAddr(ABlock->ptrs.ldataParams);
    page   startPage = 0;
    ldataParamDescr* pDescr = (ldataParamDescr*)(pNldata+1);
    int i;
    for(i=0;i<*pNldata;++i) {
      ldataDescr* pItem = (ldataDescr*)((char*)bufadr+pDescr[i].paramShift);
      long        Nblocks = (pItem->size + effQuant-1)/effQuant;
      if(pItem->size < 0 || startPage + Nblocks>= MaxPage ||
         Nblocks >= (BlockSize-sizeof(ldDataHeader))/sizeof(page)) ExecError(ErrParameters);
      pItem->addr = startPage; startPage += (page)Nblocks;
      pDescr[i].filled = 0;
    }
  }
  return(retcode);
}

void StoreLDParms(ABlock,parameters) CdBlockH ABlock; char* parameters; {
  short*     pXldata = (short*)parameters;
  long*      pSlice  = (long*)  (pXldata+1);
  unchar*    pData   = (unchar*)(pSlice+1);
  long       rest    = *pSlice;
  short*           Nldata = (short*)RealAddr(ABlock->ptrs.ldataParams);
  short            Xldata = 0;
  ldataParamDescr* pDescr = (ldataParamDescr*)(Nldata+1);
  ldataDescr*      pItem;

  /* find the descriptor */
  while(pDescr->paramNumber != *pXldata) {
    if(++Xldata >= *Nldata) ExecError(3001);
    ++pDescr;
  }

  pItem = (ldataDescr*) (RealAddr(ABlock->ptrs.inpbuf)+pDescr->paramShift);

  if(rest < 0 || rest>MaxLongSlice ||
       pDescr->filled + rest > pItem->size) ExecError(ErrParameters);

  while(rest != 0) {
    page  nBlocks = (page)((pItem->size+effQuant-1)/effQuant);
    page  modPage = pItem->addr;
    long  shift;
    zptr  Index;
    if(ABlock->ldataTempFile == 0) {
      if((ABlock->ldataTempFile = ReqTemp(ABlock->codenum)) == 0) ExecError(ErrNoTempFile);
    }
    if(pDescr->filled == 0) {
      int i,j;
      i = sizeof(ldDataHeader);
      Index = GetPage( (page) pItem->addr, ABlock->ldataTempFile, Nltype);
      intDATA(i) = nBlocks; i += sizeof(page);
      for(j=1;j<nBlocks;++j) {
        intDATA(i) = pItem->addr + j; i += sizeof(page);
      }
      SetModified(0,0);
      pItem->table = ABlock->ldataTempFile;
    }

    shift = sizeof(ldDataHeader) + nBlocks*sizeof(page) + pDescr->filled;
    if(shift >= BlockSize) {                  /* add to the cont. page */
      shift   -= BlockSize;
      modPage += (page)(shift/(BlockSize-sizeof(page))+1);
      shift    = shift%(BlockSize-sizeof(page))+sizeof(ldDataHeader);
    }
    Index = GetPage(modPage,ABlock->ldataTempFile,
                    shift == sizeof(ldDataHeader) ? Nltype : Xltype);
    { short thisPart = BlockSize - (short)shift;
      if(thisPart > rest) thisPart = (short)rest;
      _GZmov(pData,thisPart,Index+(short)shift);
      rest -= thisPart; pData += thisPart;
      pDescr->filled += thisPart;
    }
    SetModified(0,0);
  }
}

void CheckLDParms(ABlock) CdBlockH ABlock; {
  short*           Nldata = (short*)RealAddr(ABlock->ptrs.ldataParams);
  ldataParamDescr* pDescr = (ldataParamDescr*)(Nldata+1);
  short            Xldata;
  for(Xldata=0;Xldata<*Nldata;++Xldata,++pDescr) {
    ldataDescr* pItem = (ldataDescr*)
                     (RealAddr(ABlock->ptrs.inpbuf)+pDescr->paramShift);
    if(pItem->size != pDescr->filled) ExecError(ErrParameters);
  }

}

short* LoadLDParms(ABlock) CdBlockH ABlock; {
  short*     Nldata;
  short      Xldata  = 0;
  long       lShift  = Agent->parms.lGet.shift;
  long       lSize   = Agent->parms.lGet.size;
  ldataColumnDescr* pDescr;
  ldataDescr*       pItem;

  long  outLeng;
  short outShift = sizeof(short);

  if(ABlock->ptrs.ldataColumns == Cnil) ExecError(ErrLongNum);
  Nldata = (short*)RealAddr(ABlock->ptrs.ldataColumns);
  pDescr = (ldataColumnDescr*)(Nldata+1);

  /* find the descriptor */
  while(pDescr->columnNumber != Agent->parms.lGet.columnNum) {
    if(++Xldata >= *Nldata) ExecError(ErrLongNum);
    ++pDescr;
  }

  pItem = (ldataDescr*) (RealAddr(ABlock->ptrs.outbuf)+pDescr->columnShift);

  if((outLeng = pItem->size-lShift) < 0) ExecError(ErrLongNum);
  if(outLeng > lSize) outLeng = lSize;
  if(outLeng > sizeof(CommonBuffer)-outShift) 
     outLeng = sizeof(CommonBuffer)-outShift;

  while(outLeng != 0) {
    long  Nblocks = (pItem->size+effQuant-1)/effQuant;
    page  readPage = pItem->addr;
    long  shift;
    zptr  Index    = GetPage(readPage,pItem->table,Sltype);
    if(Busy(Index)) LockAgent();
    if(intDATA(sizeof(ldDataHeader)) != Nblocks) myAbort(756);

    shift = sizeof(ldDataHeader) + Nblocks*sizeof(page) + lShift;
    if(shift >= BlockSize) {                     /* read from a cont. page */
      shift   -= BlockSize;
      readPage = intDATA(sizeof(page)*(short)(shift/(BlockSize-sizeof(page))) +
                         sizeof(page) + sizeof(ldDataHeader));
      shift    = shift%(BlockSize-sizeof(page))+sizeof(ldDataHeader);
      if(Busy(Index = GetPage(readPage,pItem->table,Sltype))) LockAgent();
    }
      
    { short thisPart = BlockSize - (short)shift;
      if(thisPart > outLeng) thisPart = (short)outLeng;
      _ZGmov(Index+(short)shift,thisPart,CommonBuffer+outShift);
      outLeng -= thisPart; outShift += thisPart;
      lShift  += thisPart;
    }
  }
  *(short*)CommonBuffer = outShift;
  return((short*)CommonBuffer);
}

  enum {pagesByMap = (BlockSize-sizeof(ldMapHeader))*8};

void InitLDMap(table sFile, page mapPage) {
  zptr Index;
  int  scan = sizeof(ldMapHeader);
  if(mapPage >= MaxPage-pagesByMap) ExecError(ErrLongSpace);
  if(Busy(Index=GetPage(mapPage,sFile,Nltype)) ) myAbort(765);
  do intDATA(scan) = 0; while((scan += sizeof(short)) < BlockSize);
  SetModified(0,0);
}

static void ReqLDBlocks(table sFile,page* pagelist,page nPages) {
  zptr    Index;
  page    mapPage  = 0;
  for(;;) {
    int     scan = sizeof(ldMapHeader);
    if(Busy(Index=GetPage(mapPage,sFile,Xltype)) ) myAbort(735);
    while(scan < BlockSize) {
      while(intDATA(scan) != (short) 0xFFFF) {
        page    found  = (scan-sizeof(ldMapHeader))*8+1;
        unshort mask   = 0x8000;
        while((intDATA(scan) & mask) != 0) {++found; mask >>= 1;}
        intDATA(scan) |= mask;
        SetModified(0,0);
        if(found == pagesByMap) {       /* it's time to create a new Map */
/* ??? !! if(Busy(GetPage(mapPage+pagesByMap-1,sFile,Nltype))) myAbort(737); */
          InitLDMap(sFile,mapPage+pagesByMap);
        } else {
          *pagelist++ = found+mapPage;
          if(--nPages == 0) return;
        }
      }
      scan += sizeof(unshort);
    }
    mapPage += pagesByMap;
  }

}

/*
 * Освобождение блоков
 */
static void RlseLDBlocks(table sFile,page* pagelist,page nPages) {
  zptr    Index;
  page    mapPage  = MaxPage;
  short   xPage    = 0;

  for(xPage=0; xPage < nPages; ++xPage) {
    page    delPage = pagelist[xPage];
    page    newmap  = delPage/pagesByMap*pagesByMap;
    short   shift   = delPage%pagesByMap;
    unshort mask    = (unshort)0x8000 >> ((shift-1)%16);

    if(shift == 0) myAbort(736);
    shift = ((shift-1)/16)*sizeof(unshort) + sizeof(ldMapHeader);

    if(newmap != mapPage) {
      if(Busy(Index = GetPage(mapPage = newmap,sFile,Xltype))) myAbort(739);
      SetModified(0,0);
    }
    if(!(intDATA(shift) & mask)) myAbort(738);
    intDATA(shift) &= ~mask;
  }
}


/*
 * Copying LongData, modifing the descriptor
 */
void InsLData(ldataDescr* pDescr, table sFile) {
  if(pDescr->size != 0) {
    int   i;
    page  Nblocks = (page)((pDescr->size+ effQuant-1)/effQuant);
    page* oAddrs  = (page*) CommonBuffer;
    page* iAddrs  = oAddrs + Nblocks;
    *iAddrs = pDescr->addr;

    if(LockTable(sFile,Xltype)) LockAgent();

    ReqLDBlocks(sFile,oAddrs,Nblocks);

    for(i=0;i<Nblocks;++i) {
      zptr Index  = GetPage(iAddrs[i],pDescr->table,Sltype);
      zptr Index1 = GetPage(oAddrs[i],sFile,        Nltype);
      int  shift  = sizeof(ldDataHeader);
      if(Busy(Index) || Busy(Index1)) myAbort(732);
      SetModified(0,0);
      if(i == 0) {
        if(intDATA(shift) != Nblocks) myAbort(733);
        int0DATA(Index1+shift) = Nblocks; shift += sizeof(page);

        _GZmov(oAddrs+1,(Nblocks-1)*sizeof(page),Index1+shift);
        _ZGmov(Index+shift,(Nblocks-1)*sizeof(page),iAddrs+1);
        shift += (Nblocks-1)*sizeof(page);
      }
      _ZZmov(Index+shift,BlockSize-shift,Index1+shift);
    }
    pDescr->addr = *oAddrs;
  }
  pDescr->table = sFile;
}

void DelLData(table sFile, page addr, long size) {
  if(size != 0) {
    page  Nblocks = (page)((size+ effQuant-1)/effQuant);
    page* dAddrs  = (page*) CommonBuffer;

    if(LockTable(sFile,Xltype)) LockAgent();

    if(Nblocks > 1) {
      zptr  Index  = GetPage(addr,sFile,Xltype);
      if(Busy(Index)) myAbort(734);
      if(intDATA(sizeof(ldDataHeader)) != Nblocks) myAbort(733);
      _ZGmov(Index+sizeof(ldDataHeader)+sizeof(page),(Nblocks-1)*sizeof(page),dAddrs+1);
    }
    *dAddrs = addr;
    
    RlseLDBlocks(sFile,dAddrs,Nblocks);
  }
}
