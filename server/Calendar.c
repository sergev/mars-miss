#include "FilesDef"
         void      Calendar (unshort,short*,short*,short*);
/*
 * Преобразование дата -> год,месяц,число
 */
void Calendar(unshort daynum, short* year, short* month, short* day) {
  long    mylong  = ((long)(daynum-58) << 2) - 1;
  unshort myyear  = (unshort) (mylong/(365*4+1));
  daynum  = ((unshort)(mylong % (365*4+1)) + 4) / 4 * 5 - 3;
  if (day   != NIL) *day = (daynum%153) / 5 + 1;
  daynum += ((daynum /= 153) < 10 ? 3 : (++myyear, -9));
  if (month != NIL) *month = daynum;
  if (year  != NIL) *year  = myyear;
}
