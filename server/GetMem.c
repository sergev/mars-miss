                    /* =========================== *\
                    ** ==       M.A.R.S.        == **
                    ** == Обслуживание  буфера  == **
                    ** ==  для дерева разбора   == **
                    \* =========================== */

#include "FilesDef"
#include "AgentDef"
#include "TreeDef"
          char* mfar  GetMem   (int);

CodeSegment(Parser)

/*
 * Выделение памяти в буферочке
 */
char* mfar GetMem(size) int size; {
  size += size & (AlignBuf-1);
  if (((parseheader*) ParseBuf)->length + size >= ParseSize) {
    ErrPosition = -1;
    longjmp(err_jump,ErrComplicated);
  }
  return((((parseheader*) ParseBuf)->length += size) - size + ParseBuf);
}
