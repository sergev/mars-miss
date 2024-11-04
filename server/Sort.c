                    /* ======================== *\
                    ** ==       M.A.R.S.     == **
                    ** == Сортировка записей == **
                    \* ======================== */

#include "FilesDef"
#include "AgentDef"
#include "TreeDef"

#if defined(MISS)
#  include <stdlib.h>
#endif

        void      IniSort(SortScan*);        /* иниц-ия сканера сортир*/
        void      PutSort(SortScan*,unchar); /* занесение сортируемых */
        void      FinSort(SortScan*,unchar); /* окончат.  сортировка  */
        zptr      GetSort(SortScan*,unchar); /* чтение отсортированных*/

  /*
   * При выборке записей из отсортированного потока существеннен
   * "distflag" сканера, задающий группирование:
   *    если группирование задано, то в начале каждой группы
   *    в буфер, расположенный за сканером, записываются значения полей
   *    группирования, а для следующих записей производится сравнение
   *    с данным полем для обнаружения конца группы
   *    Помимо этого, при доступе из программы к этим полям происходит
   *    обращение к их образу в этом буфере.
   *
   */

  /* обращения к диспетчеру             */
  extern void     LockAgent(void);           /* передиспетчеризация   */

  /* сообщения об ошибках               */
  extern void     ExecError(int);            /* сообщение и откат     */

  /* обращения к в/выводу нижнего уровня*/
  extern zptr     GetPage(page,table,short); /* доступ к блоку        */
  extern void     SetModified(short,short);  /* отметка блока: модиф-н*/
  extern table    ReqTemp (unchar);          /* выделение temp-файла  */
  extern void     RlseTemp(table);           /* освобожд. temp-файла  */
  extern void     SetTail(zptr,boolean);     /* установ в хвост Cache */



#define MaxSortPage (NbCacheBuf-3)        /* число сорт-ых блоков     */


    /* макс. число сорт-ых зап. */
#define MaxSortRecs (ParseSize/sizeof(zptr))
#define MergeBuf    ((stream) ParseBuf)


#define EOSmarker ((short) sizeof(short)) /* маркер конца списка      */

    /* MaxSortPage * (1 + n + n**2 + n**3 .. n**MaxMergeLev) < 0x10000
       <=> MaxSortPage * (n**(MaxMergeLev+1)) / (n-1) < 0x10000 */

typedef struct {                          /* заголовок страницы списка*/
  page  ident;                            /* номер блока в файле      */
  short freeaddr;                         /* адрес свободной области  */
} spageheader;

#define header(x)    ZDATA(x,spageheader)


  static void     SortInverse            /* инвертирование битов      */
                              (short*,unchar*,boolean);
#ifdef ZCache
  static void     ZSortInverse           /* -- " -- для Z-блока       */
                              (short*,unshort,boolean);
#else
#  define ZSortInverse(x,y,z) SortInverse(x,y,z)
#endif

  static void     SortList               /* сортировка накопленного   */
                              (SortScan*,boolean,unchar);

#ifdef ReverseBytes                      /* байты инвертированы    */
  static unchar Sizes[Tfloat+1] =
                         {0,sizeof(short),sizeof(long),sizeof(real)};
#endif

/*
 * Инвертирование знаковых битов и полей для desc
 */
static void SortInverse(invbuf,record,put)
                         short* invbuf; unchar* record; boolean put; {
  int    i,j;
#ifdef BSFloat
  unchar mask = (put ? 0x00 : 0x80);
#endif

     /* инвертируем байты desc-сортировки */
  if(! put) {
    short*  reinvbuf = invbuf - (invbuf[-1]*2+1);
    unchar* rerecord = record;
    for(i = *--reinvbuf; i != 0; i--) {
      rerecord += *--reinvbuf;
      for(j = *--reinvbuf; j!=0 ; j--) *rerecord++ ^= 0xFF;
    }
  }
     /* инвертируем знаковый бит числовых данных, кодируем тексты */
  for(i = *--invbuf; i != 0; i--) {
    unchar* pdata;
    j     = *--invbuf;
    pdata = record + *--invbuf;
    if(j >= 0) {                            /* это текст              */
      TRS(pdata,j,(put ? CodeTable : DecodeTable));
    } else {

#     ifdef ReverseBytes                    /* байты инвертированы    */
        if(put) SwapBytes(pdata,Sizes[~j]);
#     endif

#     ifdef BSFloat
        if(j == ~Tfloat && (*pdata & 0x80) ^ mask) InvFloat((real*)pdata);
        else
#     endif
        *pdata ^= 0x80;

#     ifdef ReverseBytes                    /* байты инвертированы    */
        if(!put) SwapBytes(pdata,Sizes[~j]);
#     endif

    }
  }

     /* инвертируем байты desc-сортировки */
  if( put) for(i = *--invbuf; i != 0; i--) {
    record += *--invbuf;
    for(j = *--invbuf; j!=0 ; j--) *record++ ^= 0xFF;
  }
}

#ifdef ZCache
static void ZSortInverse(invbuf,result,put)
                   short* invbuf; unshort result; boolean put; {
  int    i,j;
#ifdef BSFloat
  unchar mask = (put ? 0x00 : 0x80);
#endif

  if (! put) {
    short*   reinvbuf = invbuf - (invbuf[-1]*2+1);
    unshort  reresult = result;
    for(i = *--reinvbuf; i != 0; i--) {
      reresult += *--reinvbuf;
      for(j = *--reinvbuf; j!=0 ; j--) {
        Byte0DATA(reresult) ^= 0xFF; reresult++;
      }
    }
  }
     /* инвертируем знаковый бит числовых данных, кодируем тексты */
  for(i = *--invbuf; i != 0; i--) {
    zptr  pdata;
    j     = *--invbuf;
    pdata = result + *--invbuf;
    if(j >= 0) {                            /* это текст              */
      ZTRS(pdata,j,(put ? CodeTable : DecodeTable));
    } else {

#     ifdef ReverseBytes                    /* байты инвертированы    */
        if(put) ZSwapBytes(pdata,Sizes[~j]);
#     endif

#     ifdef BSFloat
        if(j == ~Tfloat && (Byte0DATA(pdata) & 0x80) ^ mask)
                               ZInvFloat(pdata);
        else
#     endif
        Byte0DATA(pdata) ^= 0x80;

#     ifdef ReverseBytes                    /* байты инвертированы    */
        if(!put) ZSwapBytes(pdata,Sizes[~j]);
#     endif
    }
  }

     /* инвертируем desc - поля */
  if (put) for(j= *--invbuf; j != 0; j--) {
    result  += *--invbuf;
    for(i = *--invbuf; i!=0; i--) {Byte0DATA(result) ^= 0xFF; result++;}
  }
}
#endif

/*
 * Инициализация сканера сортировки
 * закрываем незакрытые уровни сортировки, создаем sortfile,
 */
void IniSort(scan) SortScan* scan; {
  int  i;
  --scan;                                  /* т.к. задан конец сканера*/
  for(i=0; i<MaxMergeLev; ++i) {           /* пройдем по всем уровням */
    if(scan->levels[i].filled !=0) {       /* уровень есть->закроем   */
      scan->levels[i].filled=0;
      RlseTemp(scan->levels[i].file);
    }
  }
  if(scan->sortfile != 0) {
    RlseTemp(scan->sortfile);
    scan->sortfile = 0;
  }

/*scan->groupcount = -1;                   /*флаг для GetSort         */
  scan->wasGroup   = FALSE;                /*снимем флаг "идет группа"*/
  scan->nrecords   = scan->sortpage = 0;

  /* создадим 0-ую страницу 0-го потока 0-го уровня */
/*if(Busy(Index=GetPage((page)0,scan->table,Nltype))) myAbort(140);
  header(Index).freeaddr = sizeof(spageheader);
  SetModified(0,0);
*/
}

/*
 * Занесение записи в сортируемый поток
 */
void PutSort(SortScan* scan, unchar codeNum) {
  int      length;
  zptr     Index;
  unchar*  record = (unchar*) scan;
  --scan;

  if (LimitResources) LockAgent();
  if (scan->nrecords >= MaxSortRecs) SortList(scan,FALSE,codeNum);
  length = Align(*(short*) record);       /* длина записи             */

  if (scan->sortfile == 0) {               /* первая запись           */
    if((scan->sortfile=ReqTemp(codeNum))==0) ExecError(ErrNoTempFile);
  }

  if (scan->nrecords == 0) {               /* начало порции сортировки*/
    scan->sortpage = 0;
    Index=GetPage(0,scan->sortfile,Nltype);
    header(Index).freeaddr = sizeof(spageheader);
  } else {                                 /* не первая запись        */
    Index=GetPage(scan->sortpage,scan->sortfile,Xltype);
  }

  /* приведем запись к сортируемому виду */
  SortInverse((short*) scan,record,TRUE);

  if ( BlockSize - header(Index).freeaddr < length) {
    if (scan->sortpage>=MaxSortPage-1) {  /* пора сортир..овать       */
      SortList(scan, FALSE,codeNum);
    } else {
      ++scan->sortpage;
    }
    Index=GetPage(scan->sortpage,scan->sortfile,Nltype);
    header(Index).freeaddr = sizeof(spageheader);
  }
  _GZmov(record, length, header(Index).freeaddr + Index );
  header(Index).freeaddr += length;
  SetModified(0,0);

  ++scan->nrecords;

  /* восстанавливаем знаковые биты */
  SortInverse((short*) scan,record,FALSE);
}

/*
 * Конец сортироваки - слияние всех потоков
 */
void FinSort(SortScan* scan, unchar codeNum) {
  if (LimitResources) LockAgent();
  --scan;

  if (scan->sortfile == 0) return;     /* пустая сортировка       */
  SortList(scan,TRUE,codeNum);
  scan->sortpage  = 0;
  scan->nrecords  = sizeof(spageheader);
  scan->groupcount= 0;
}

/*
 * Чтение отсортированного потока
 * выход: Z-адрес считанной записи или 0 -> конец списка
 *                                     1 -> кончилась группа
 * distinctflag: бит 0x01 -> нужны группы
 *               бит 0x02 -> группа инициирована
 */
zptr GetSort(SortScan* scan, unchar codeNum) {
  zptr     Index,result,invscan;
  codeNum=codeNum;
  if (LimitResources) LockAgent();
  --scan;

  if (scan->sortfile == 0) return((zptr)0);/* пустая сортировка       */

  /* читаем записи из отсортированного потока */
  Index=GetPage(scan->sortpage,scan->sortfile,Sltype);
  invscan = result = Index + scan->nrecords;
  if( int0DATA(result) == EOSmarker) {     /* считан конец потока     */
    if(scan->wasGroup) {                   /* надо кончить группу     */
      scan->wasGroup = FALSE; return((zptr)1 );
    }
    RlseTemp(scan->sortfile); scan->sortfile=0;
    return((zptr)0 );
  }

  ZSortInverse((short*)scan, result,FALSE);/* инверсии выходной строки*/

  if(scan->doGroup) {                      /* надо выделять группы   */
    if(scan->wasGroup) {                   /* группа инициирована    */
      if(cmpsZG(result            + scan->begcompare,
               (char*) (scan + 1) + scan->begcompare,
                scan->endcompare  - scan->begcompare) != 0) {
        scan->wasGroup = FALSE;             /* группа кончилась       */
        ZSortInverse((short*)scan, result,TRUE);
        return((zptr)1);                    /* вернем все обратно     */
      }
    } else {                                /* начало новой группы    */
      _ZGmov(result,scan->endcompare,(char*)(scan+1));
      scan->groupcount = 0; scan->wasGroup = TRUE;
    }
    ++scan->groupcount;
  }

  if(scan->copyOut) {
    _ZGmov(result,Align(int0DATA(result)),(char*)(scan+1));
  }

  if( (scan->nrecords += Align(int0DATA(result)) )
                          >= header(Index).freeaddr) {
           /* освобождаем блок файла сортировки */
    SetTail(Index,TRUE);

    scan->sortpage++;                      /* читаем следующую стр-цу */
    scan->nrecords = sizeof(spageheader);  /* при следующем обращении */
  }
  return(result);
}

/*
 * Сортировка - слияние записей
 * Накопленные в sortfile записи зачитываются в Cache (все сразу)
 * формируется буфер сортировки с адресами записей, он qsort-ится,
 * и записи выводятся отсортировано в очередной поток 0-го уровня
 * Если все потоки 0-го уровня сформированы, то
 *          происходит слияние в первый свободный поток верхних уровней
 * Если это - последняя сортировка, то сливаются все имеющиеся потоки
 *          в файл sortfile;
 * В конце потоков ставится признак конца - EOStream;
 */

typedef struct stream {                  /* описатель потока слияния  */
  zptr           ptr;                    /* указатель в Z-сегменте    */
  page           page;                   /* адрес блока               */
  table          file;                   /* файл потока               */
  struct stream* link;                   /* сцепка (упорядоченно)     */
}* stream;

  static void     InsMerge(stream*,      /* вставка в список слияния  */
                               stream);
#if defined(i_86)
  static int mfar cmpsort (const void *, const void *);
                                         /* сравнение при сортировке  */
#else
  static int      cmpsort (zptr*,zptr*); /* сравнение при сортировке  */
#endif

  static unsigned begcompare;            /* начало сравниваемого      */
  static unsigned endcompare;            /* конец  сравниваемого      */

static void SortList(SortScan* scan, boolean finish, unchar codeNum) {
  zptr       Index,OutIndex;
  table      outfile;                    /* выходной файл             */
  page       outpage;                    /* выходная страница         */
  SortLevel* outlevel;                   /* описатель выводного уровня*/
  unshort    freeaddr;                   /* адрес свободного в Out-бло*/


  if( TraceFlags & TraceSort) {
    Reply(Fdump("Sorting: nrecords = %4.16",(unsigned)scan->nrecords));
  }
  begcompare = scan->begcompare;             /* поле сортировки       */
  endcompare = scan->endcompare;             /* и его конец           */

  /* если есть что сортировать, то сортируем */
  if(scan->nrecords != 0) {
    zptr*    sortbuf = (zptr*)ParseBuf;      /* буфер сортировки      */
    unshort  ptr;
    int      bufptr;
    int      buflength  = 0;           /* кол-во элементов сортировки */

    /* формируем буфер адресов */
    for(;;) {
      Index=GetPage(scan->sortpage,scan->sortfile,Sltype);
      ptr = sizeof (spageheader);
      /* заносим адреса всех записей из данного блока */
      while (ptr < header(Index).freeaddr) {
        ptr += Align(int0DATA(sortbuf[buflength++] = ptr+Index));
      }
    if(scan->sortpage == 0) break;
      --scan->sortpage;
    }

    /* проверим число считанных записей */
    if(buflength != scan->nrecords) myAbort(142);

    /* теперь сортируем буфер адресов */
    qsort( (char*) sortbuf, buflength, sizeof(zptr), (int(*)(const void*, const void*)) cmpsort);

    /* теперь выводим отсортированные в поток 0-го уровня    */
    /* определим, идентификатор файла вывода и позицию в нем */
    outlevel = &scan->levels[0];
    if (outlevel->filled == 0) {                /* это 0-ой поток     */
      if((outlevel->file=ReqTemp(codeNum)) ==0) /* создаем файл уровня*/
                                     ExecError(ErrNoTempFile);
      outpage = 0;
    } else {                                    /* пишем с конца пред.*/
      outpage = outlevel->ends [ outlevel->filled-1 ] ;
    }

    /* вывод отсортированных записей */

    freeaddr = BlockSize+1;
    bufptr=0;
    for (;;) {
      unshort length = bufptr<buflength ?        /* длина записи      */
                       Align(int0DATA(sortbuf[bufptr])) : sizeof(short);

      /* проверим свободное место в блоке */
      if(freeaddr + length >= BlockSize) {
        if(freeaddr<=BlockSize) header(OutIndex).freeaddr = freeaddr;
        /* создаем блок вывода   */
        OutIndex = GetPage(outpage++,outlevel->file,Nltype);
        SetModified(0,0);
        SetTail(OutIndex,FALSE);
        freeaddr = sizeof(spageheader);
      }

      /* все записи скинули */
    if(bufptr>=buflength) break;

      _ZZmov(sortbuf[bufptr],length,OutIndex+freeaddr);
      freeaddr += length;
      ++bufptr;
    }

    /* пишем признак конца потока */
    int0DATA(OutIndex+freeaddr) = EOSmarker;
    header(OutIndex).freeaddr = freeaddr + sizeof(short);

/*  FreeTemp(scan->table,MaxSortPage,0);       /* освобождаем память  */

    outlevel->ends [outlevel->filled++] = outpage;
  }
  scan->sortpage = 0;
  scan->nrecords = 0;

  /* теперь сливаем отсортированые потоки, если надо */
  if(finish || scan->levels[0].filled == MaxMergeStr) {

    int    elevel,xlevel;              /* старший уровень слияния     */
    int    nstream = 0;                /* число сливаемых потоков     */
    stream list    = NIL;              /* список голов потоков        */

    if(finish) {
      elevel  = MaxMergeLev;
      outfile = scan->sortfile; outpage = 0;
    } else {
      for(elevel = 0; elevel<MaxMergeLev &&
                scan->levels[elevel].filled == MaxMergeStr; ++elevel);
      /* проверим, что можно сделать следующий поток */
      if(elevel>=MaxMergeLev) ExecError(ErrBigSort);
      outlevel = scan->levels + elevel;

      if(outlevel->filled == 0) {           /* создаем новый уровень ?*/
        if( (outlevel->file = ReqTemp(codeNum)) == 0) ExecError(ErrNoTempFile);
        outpage = 0;
      } else {                              /* новый поток старого ур.*/
        outpage = outlevel->ends[outlevel->filled-1];
      }

      outfile = outlevel->file;
    }

    /* инициируем потоки: формируем массив описателей потоков */
    /* потоки связываем в упорядоченный список              */
    for(xlevel=0; xlevel<elevel; ++xlevel) {   /* цикл по уровням     */
      SortLevel* level = scan->levels+xlevel;
      int j = level->filled;
      while(--j>=0) {                          /* цикл по потокам     */
        stream new = MergeBuf + nstream++;
        new->page= (j ? level->ends[j-1] : 0);
        new->ptr = GetPage(new->page,
                           new->file= level->file, Sltype) +
                    sizeof(spageheader);
        InsMerge(&list, new);
      }
    }

    if( TraceFlags & TraceSort) {
      Reply(Fdump("Merging: %4.10 streams -> %1.10 Level",
                       (unsigned)nstream, (unsigned)elevel));
    }

    if(nstream == 0) {                          /* пустая сортировка  */

    } else if (nstream == 1) {                  /* слияие 1 потока    */

      /* этот единственный поток должен быть 0-ым  в 0-ом уровне */
      if(! finish || scan->levels[0].filled != 1) myAbort(140);

      /* просто подставляем его файл в качестве файла-результата */
      RlseTemp(scan->sortfile);
      scan->sortfile = scan->levels[0].file;
      scan->levels[0].filled = 0;

    } else {

      /* проверим, что головы потоков поместятся в памяти */
      if(nstream >= NbCacheBuf - 2) ExecError(ErrBigSort);
      freeaddr = BlockSize+1;

      /* цикл слияния      */
      for(;;) {
        int      length = list ? Align(int0DATA(list->ptr)) :
                                 sizeof(short);
        stream   old;

        /* очередная запись поместится в этот блок ? */
        if(freeaddr+length > BlockSize) {   /* выводной блок заполнен */
          if(freeaddr<=BlockSize) {
            header(OutIndex).freeaddr = freeaddr;
            SetTail(OutIndex,FALSE);        /* скидываем блок на диск */
          }
          OutIndex = GetPage(outpage++,outfile,Nltype);
          SetModified(0,0);
          freeaddr = sizeof(spageheader);
        }

        /* список сливаемых потоков пуст -> все сделали */
      if( list == NIL) break;

        /* переносим запись в выводной блок */
        _ZZmov(list->ptr,length,OutIndex+freeaddr);
        freeaddr  += length;
        Index  = (list->ptr - CacheBase)/BlockSize*BlockSize +CacheBase;
        list->ptr += length;

        /* блок потока кончился ? */
        if (list->ptr - Index >= header(Index).freeaddr) {
          if( TraceFlags & TraceSort) {
            Reply(Fdump("Merging: End Of Block %4.16(%2.10)",
                  (unsigned) list->page,
                  (unsigned)(list-MergeBuf)));
          }

          /* выбрасываем блок из Cache, без сброса содержимого */
          SetTail(Index,TRUE);
          /* зачитываем следующий    */
          Index= GetPage(++list->page,list->file,Sltype);
          list->ptr = Index + sizeof( spageheader);
        }

        /* поднимаем следующую запись из потока */
        old  = list; list = list->link;
        if( int0DATA( old->ptr) != EOSmarker){ /* поток не кончился   */
          InsMerge(&list, old);                /* вставляем следующ.  */
        } else {                               /* поток кончился      */
          if( TraceFlags & TraceSort) {
            Reply(Fdump("Merging: End Of Stream %2.10",
                       (unsigned)(old-MergeBuf)));
          }
          SetTail(Index,TRUE);                /* блок Cache освободим */
        }
      }

      /* заносим признак конца потока */
      int0DATA(OutIndex+freeaddr) = EOSmarker;
      header(OutIndex).freeaddr = freeaddr + sizeof(short);

      if(! finish) {                       /* сформируем поток        */
        outlevel->ends[outlevel->filled++] = outpage;
      }

      /* освобождаем все файлы потоков нижнего уровня  */
      for(xlevel=0; xlevel<elevel; ++xlevel) {
        if(scan->levels[xlevel].filled != 0) {
          scan->levels[xlevel].filled = 0;
          RlseTemp(scan->levels[xlevel].file);
        }
      }

    }

  }

}

/*
 * Вставка в список голов сливаемых потоков упорядоченно по возрастанию
 */
static void InsMerge(hscan,new) stream* hscan; stream new; {
  while (*hscan != 0 && cmpsort( &(*hscan)->ptr, &new->ptr) <0) {
    hscan = & (*hscan)->link;
  }
  new->link = *hscan;  *hscan = new;
}

/*
 * Сравнение
 */
#if   defined(MISS) && defined(i_86)
static int mfar cmpsort(first,second)
const void *first; const void *second;
{
  return ( cmpsZZ(*(zptr *)first+begcompare, *(zptr *)second+begcompare,
                   endcompare-begcompare) );
}
#else
static int mfar cmpsort(zptr* first, zptr* second) {
  register zptr end   = *first + endcompare;
  register zptr scan1 = *second+ begcompare;
  register zptr scan  = *first + begcompare;
  while(scan < end) {
    if(int0DATA(scan) != int0DATA(scan1)) {
      return (
          ZDATA(scan,unchar) != ZDATA(scan1,unchar)
             ?  (ZDATA(scan,unchar)   <  ZDATA(scan1  ,unchar) ? -1 : 1)
             :  (ZDATA(scan+1,unchar) <  ZDATA(scan1+1,unchar) ? -1 : 1)
          );
    }
    scan += sizeof(short); scan1 += sizeof(short);
  }
  return(0);
}
#endif
