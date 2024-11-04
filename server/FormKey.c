/*
 * Формирование ключа по его описателю и записи данных
 */
#include "FilesDef"

  void FormKey(table,char*,boolean);

void FormKey(table index, char* record, boolean zflag) {
  indcolumn* descr= Files[index].b.indexdescr.columns;
  int        i;
  unchar*    scan = KeyBuffer;
  for(i=0; i<MaxIndCol && descr->shift != -1; descr++, i++) {
    boolean  inv    = descr->invFlag;
    short    length = descr->length;
    char*    ptr    = record + descr->shift;

    /* вычислим длину и адрес для varchar */
    if(!descr->numFlag && descr->flvrFlag) {
      ptr += sizeof(short);
#ifdef ZCache
      if(zflag) {
        length = int0DATA((zptr)ptr-sizeof(short))-int0DATA((zptr)ptr) - sizeof(short);
        ptr   += int0DATA((zptr)ptr);
      } else
#endif
      { length = ((short*) ptr)[-1] - ((short*)ptr)[0] - sizeof(short);
        ptr   += ((short*) ptr) [0];
      }
    }

    /* перенесем данные в ключ */
#ifdef ZCache
    if(zflag) _ZGmov((zptr)ptr,length,scan);
    else
#endif
      MVS(ptr,length,scan);

    if(descr->numFlag) {
#     ifdef ReverseBytes
        SwapBytes(scan,length);
#     endif
#     ifdef BSFloat
      if(descr->flvrFlag && (*scan & 0x80)) {
        inv = ~inv;
      } else
#     endif
        *scan ^= 0x80;
    } else {
      TRS(scan,length,CodeTable);
    }
    if(inv) {
      register int j=length;
      while(--j>=0) *scan++ ^= 0xFF;
    } else {
      scan += length;
    }
/*  if(descr->length & (AlignBuf-1)) *scan++ = 0; @ 28 Jun 92 */
  }
  keylength = scan - KeyBuffer;
}
