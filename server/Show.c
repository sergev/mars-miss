                   /* ============================ *\
                   ** ==         M.A.R.S.       == **
                   ** == Отображение таблиц на  == **
                   ** == на "консоли оператора" == **
                   \* ============================ */

#include "FilesDef"

         void       Show(char);

         int        shiftshow = 0;        /* номер показываемого эл.  */
         boolean    EndShow   = TRUE;
  extern short      Nelems;               /* полное число элементов   */
  extern char       regshow;              /* номер показываемого эл.  */
  extern char*      title;                /* название таблицы         */
  extern char*      line;                 /* заголовок таблицы        */


#if defined(MISS)
#include <Display>
#include <SysStrings>
#include <StartOpers>
#include <SysCalls>
#define  MaxSize   15

  extern void       ShowTable(int);


void Show(symbol) char symbol; {
  static char trailer[] = {SC,0,0,0};
  static char  clear[MaxSize*2+5] =
       {LF,EL, LF,EL, LF,EL, LF,EL, LF,EL, LF,EL, LF,EL, LF,EL,
        LF,EL, LF,EL, LF,EL, LF,EL, LF,EL, LF,EL, LF,EL,
        HO,SC, FI_INVRS, 0x40, 0};

  int     sizeshow = (GetScreenSize().v/2 - 1) /\ MaxSize;
  boolean newflag  = FALSE;
  int     i;

  switch(symbol) {
  case ctrl('G'):
    StartProgram(Kernel,0,StackCall);
    return;
  case CD: case '>':
    if(EndShow) return;
    symbol     = regshow;
    shiftshow += sizeshow;
  break; case CU: case '<':
    if(shiftshow==0) return;
    symbol     = regshow;
    shiftshow -= sizeshow;
  break; case ' ':
    symbol     = regshow;
  break; default:
    shiftshow = 0; newflag = TRUE;
  }

  ShowTable(UpperCase(symbol));

  if (newflag) {
    static char begtitle[] = {HO,EL,         SC, FI_HIGHL, 0x20,0 };
    static char outtitle[] = {HO,CD,SM,0,CL, SC, FI_HIGHL+FI_INVRS,1,0};
    outtitle[3] = (char) (_cps(title,100,0)-title);
    dpc(begtitle); dpc(clear+(MaxSize-sizeshow)*2); dpc(line);
    dpc(outtitle); dpc(title);
  }

  dpc(trailer);
  for( i = 0 ; i < sizeshow; ++i) {
    ShowTable(-1);
    SetCursorPos((char)(i+1),0);
  if(line == NIL) break;
    dpc(line);
  }

  if (EndShow = (line == NIL)) {
    dpo(EL); dpom('-',60);
    while(++i < sizeshow) {dpo(LF); dpo(EL);}
  }
  dpo(HO);
}

#elif defined(macintosh)
void Show(symbol) char symbol; {
}

#else
void Show(char symbol) {
}

#endif
