                    /* =========================== *\
                    ** ==       M.A.R.S.        == **
                    ** == Обслуживание  буфера  == **
                    ** ==  для дерева разбора   == **
                    \* =========================== */

/*
 * Модуль служит для сохранения и восстановления буфера разбора
 * в случае возникновения блокировок при трансляции
 */
#include "FilesDef"
#include "AgentDef"
#include "TreeDef"
          void        SaveParse(void);
          void        RestParse(void);

   extern zptr  GetPage(page,table,short);
   extern void  SetModified(short,short);

/*
 * Спасение буфера с деревом разбора в рабочий файл
 */
void SaveParse() {
  page      pagenum = 0;
  unsigned  shift   = 0;
  zptr      Index;
  unsigned  MemPointer =  ((parseheader*) ParseBuf)->length;
  while (MemPointer != 0) {
    int slice = Min(MemPointer,BlockSize - sizeof(page));
    Index = GetPage(pagenum, Agent->ident+BgWrkFiles, Nltype);
    SetModified(0,0);
    _GZmov(ParseBuf+shift, slice, Index + sizeof(page));
    MemPointer -= slice; shift += slice; pagenum++;
  }
}

/*
 * Восстановление буфера с деревом разбора из рабочего файла
 */
void RestParse() {
  page      pagenum = 0;
  unsigned  shift   = 0;
  zptr      Index;
  unsigned MemPointer = BlockSize;
  do {
    int slice = Min(MemPointer,BlockSize - sizeof(page));
    Index = GetPage(pagenum, Agent->ident+BgWrkFiles, Sltype);
    _ZGmov( Index + sizeof(page), slice, ParseBuf+shift);
    if (pagenum==0) MemPointer = ((parseheader*) ParseBuf)->length;
    pagenum++; shift += slice;
  } while (shift < MemPointer) ;
}