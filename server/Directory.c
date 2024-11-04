                    /* ============================ *\
                    ** ==         M.A.R.S.       == **
                    ** == Обслуживание  каталога == **
                    \* ============================ */

#include "FilesDef"
#include "AgentDef"

         int     LookDir(char*,table);

  /* обращения к диспетчеру             */
  extern void     LockAgent(void);           /* передиспетчеризация   */

  /* обращения к в/выводу нижнего уровня*/
  extern zptr     GetPage(page,table,short); /* доступ к блоку        */
  extern boolean  LockElem(lockblock*,short,boolean);

int LookDir(char* name, table mainfile) {
  int      index, empty;
  zptr     Index;
  int      owner    = Agent->userid;
  int      nameleng = MaxIndxName;
  unchar   kind     = mainfile == NoFile ? tableKind : indexKind;

  if(mainfile == NoFile) {                   /* поиск таблицы         */
    if(name[nameleng = MaxFileName] != ' ') {/* username prefix exists*/
      if(LockElem(&Files[UserFile].iR,Sltype,TRUE) ||
         Busy(Index = GetPage((page)0,UserFile,Sltype)) ) LockAgent();
      owner = Nusers;
      do {
        if(--owner<0) {ErrCode=ErrUser; return(-10000);}
      } while (cmpsZG(Index+sizeof(page)+owner*sizeof(logon_elem),
                      name+nameleng,UserName) != 0);
    }
  } else {
    owner = Files[mainfile].owner;          /* index owner=tab. owner */
  }

  for (index = empty = MaxFiles ; --index >= 0 ;) {
    file_elem* ptr = &Files[index];
    if (ptr->status & BsyFile) {            /* не пустой элемент     */
      /* сравниваем имена                */
      if(ptr->owner == owner && ptr->kind == kind &&
         ptr->mainfile == mainfile &&
         _cmps((char*) &ptr->b,name,nameleng) == 0) {
        /* стертые нами об'екты невидимы */
        if(! (ptr->status & DrpFile) ||
              ptr->iR.Xlock.owner != Agent->ident ) {
          return(index);
        }
      }

    } else {
      empty = index;
    }
  }
  ErrCode = ErrNoExists;
  return(~empty);
}
