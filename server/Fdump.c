                   /* ============================ *\
                   ** ==         M.A.R.S.       == **
                   ** == Формирование отладочных== **
                   ** ==  выдач (mini-sprintf)  == **
                   \* ============================ */

#include "FilesDef"
#include <stdarg.h>
#define MaxLine 90

  char* Fdump(text
#if ! defined(R_11) && ! defined(macintosh) && ! defined(__GNUC__)
    ,...
#endif
                  ) char* text; {

#if defined(MSDOS)
#  define NULL 0
#endif

  static     char    Buffer[MaxLine+1];
  char*      tscan = Buffer;
  va_list    dscan;
  va_start(dscan,text);
  while(tscan < Buffer+MaxLine-20 && *text != 0) {
    if (*text != '%') {
      *tscan++ = *text++;
    } else if(text[1] == 'p') {
      unchar* pData = va_arg(dscan,unchar*);
      short   length= *pData++;
      text += 2;
      while(--length >= 0) *tscan++ = *pData++;
    } else {
      int      nrepeat = -1;
      char*    tsave;
      unshort* pshorts;
      if (text[1] == '(') {
        ++text;
        nrepeat = Align(va_arg(dscan,unsigned))/sizeof(short);
        pshorts = va_arg(dscan,unshort*);
      }
      tsave = text;
      if(nrepeat != 0) {
        do {
          unsigned data;
          int width = 0, radix = 0;
          data = (nrepeat < 0 ? va_arg(dscan,unsigned) : *pshorts++);
          text = tsave;
          if(nrepeat>=0) while (*++text != '%') *tscan++ = *text;
          if(text[1] == 'c') {
            *tscan++ = (char) data; text += 2;
          } else {
            while (*++text != '.') width = width*10 + *text - '0';
            while (*++text>='0' && *text<='9') radix=radix*10 + *text-'0';
            MVS(_conv(data,width+0x80,radix),width,tscan);
            tscan += width;
          }
          if(nrepeat>=0) while (*text++ != ')') *tscan++ = text[-1];
        } while (tscan < (Buffer+MaxLine-20) && --nrepeat >0 );
      } else {
        while (*text++ != ')');
      }
    }
  }
  *tscan = 0;
  va_end(dscan);
  return(Buffer);
}
