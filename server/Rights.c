                    /* ======================== *\
                    ** ==       M.A.R.S.     == **
                    ** ==   Обработка прав   == **
                    \* ======================== */

#include "FilesDef"
#include "AgentDef"
#include "TreeDef"

         short    Logon(void);               /* идентификация польз-ля*/
         short    ServPriv(boolean);         /* установ привилегий    */
         void     UsersCreate(void);         /* создание USERS-катал. */

  /* обращения к диспетчеру             */
  extern void     LockAgent(void);           /* передиспетчеризация   */

  /* сообщения об ошибках               */
  extern void     ExecError(int);            /* сообщение и откат     */

  /* обращения к в/выводу нижнего уровня*/
  extern zptr     GetPage(page,table,short); /* доступ к блоку        */
  extern void     SetModified(short,short);  /* отметка блока: модиф-н*/
  extern boolean  LockElem(lockblock*,short,boolean);
  extern boolean  LockTable(table,short);    /* блокировка таблицы    */

  /* работа с каталогом базы */
  extern int      LookDir(char*,table);       /*поиск по каталогу базы*/
  static short    LookUser(char*,boolean);  /* поиск пользователя     */

  static short    empty;
  static zptr     Index;
  static zptr     shift;


/*
 * Поиск в таблице пользователей
 * Если задан modflag, то захват каталога пользователей
 */
static short LookUser(name,modflag) char* name; boolean modflag; {
  short    user  = Nusers-1;

  if(LockTable(UserFile,modflag ? Xltype : Sltype)) LockAgent();

/*if (LockElem(&Files[UserFile].iR,Sltype,TRUE ) || (modflag &&
     (LockElem(&Files[UserFile].iW,Xltype,FALSE) ||
      LockElem(&Files[UserFile].iM,Sltype,TRUE )    ))) LockAgent();
*/

/*if ( Busy(Index = GetPage((page)0,UserFile,modflag)) ) LockAgent(); */
  if ( Busy(Index = GetPage((page)0,UserFile,modflag)) ) myAbort(512);

  empty = -1;
  do {
    shift = Index + sizeof(page) + user*sizeof(logon_elem);
    if(Byte0DATA(shift)==0) empty = user;
  } while (cmpsZG(shift,name,UserName) != 0 && --user>=0);

  return(user);
}

/*
 * Вход в Систему, инициализация
 */
short Logon() {
  char*    pscan = sizeof(struct parseheader) + ParseBuf;
  short    user  = LookUser(pscan,FALSE);
  int      i;

  if (user<0) {ErrCode = ErrUser; return(-1);}
  for(i=PasswLng+UserName;--i>=0 && Byte0DATA(shift+i)==pscan[i];);
  if (i>=0) {ErrCode = ErrPassw; return(-1);}
  Agent->userid = (userid) user;
  Agent->privil = LogonDATA(shift).privil;
  return(0);
}

/*
 * Обработка привилегий: вставка/уничтожение прав доступа
 */
short ServPriv(flag) boolean flag; {

  char*     pscan = sizeof(struct parseheader) + ParseBuf;
  short     tablenum;
  unsigned  scan,ptr0;
  zptr      ptr;
  int       i,Ncolumns,Nelems;
  short     what = * (short*) pscan;
  userid    who;

  pscan +=  sizeof(unshort);
  if (what < 0) {                    /* права польз-теля как такового */
    what = ~ what;
    if (*pscan == '*') ExecError(ErrName);
    if (! (Agent->privil & Priv_Auth) ) ExecError(ErrRights);

    if ((i=LookUser(pscan,TRUE))<0) {/* пользователя нет ->           */
      if(what != 0)  ExecError(ErrUser);
      if(! flag) return(0);
      if((i=empty)<0) ExecError(ErrFull);
      shift = Index + sizeof(page) + i*sizeof(logon_elem);
      _GZmov(pscan,UserName,shift);  /* заносим нового пользователя   */
      if (! (Agent->privil & Priv_Auth) ) ExecError(ErrRights);
      _GZmov(pscan+UserName,PasswLng,shift+UserName);
      LogonDATA(shift).privil = 0;
    }

    if (what == 0) {                /* Grant / Revoke Connect         */
      if(! flag) {
        Byte0DATA(shift) = 0;       /* Revoke Connect                 */
        /* здесь бы не плохо проверить, не работает ли он сейчас */
        /* а также стереть  все его права доступа из всех таблиц */
      } else {
        _GZmov(pscan+UserName,PasswLng,shift+UserName);
      }

    } else {                        /* Grant / Revoke ADMIN, CREATE...*/
      unchar mask = 1<<(what-1);
      if (flag) {
        LogonDATA(shift).privil |=  mask;
      } else {
        LogonDATA(shift).privil &= ~mask;
      }
    }
    SetModified(0,0);
    return(0);
  }

  if (flag) {                        /* если это GRANT ->             */
    what = what | 1;                 /* то разрешим как минимум чтение*/
  } else if (what & 1) {             /* REVOKE READ    ->             */
    what = 0x0F;                     /* запретим все                  */
  }

  /* поищем таблицу, которую мы открываем/защищаем */
  if( (tablenum = LookDir(pscan,NoFile)) < 0) return(-1);
  pscan += MaxFileName+UserName;

  /* проверим, что мы можем менять права на эту таблицу */
  if (Files[tablenum].owner != Agent->userid &&
           ! (Agent->privil &  Priv_Auth) ) {
    ErrCode = ErrRights; return(-1);
  }

  /* прочтем, кому мы все это дарим (отбираем) */
  if (*pscan == '*') {
    i = nobody;                                     /* .. to PUBLIC   */
  } else if ((i=LookUser(pscan,FALSE))<0) {
    ErrCode = ErrUser; return(-1);                  /*нет у нас негров*/
  }
  who = (userid) i;

  /* захватим таблицу */
  if (LockTable((table) tablenum,Oltype)) LockAgent();

  /* прочитаем блок описателей */
  Index    = GetPage((page)1,(table)tablenum,Xltype);

  scan     = sizeof(page);
  Ncolumns = DescTable(scan).Ncolumns;
  /* просканируем описатели таблицы и полей */
  for(i = -1; i<Ncolumns; ++i) scan += intDATA(scan);

  Nelems = intDATA(scan);
  ptr = ( ptr0 = scan + sizeof(short) + Nelems*sizeof(privil)) + Index;

  for(i=Nelems; --i>=0;) {                   /* поищем права клиента  */
    ptr -= sizeof(privil);
  if(pdescr(ptr).who == who) break;
  }

  if(i>=0) {                                 /* элемент найден        */
    if (flag) {                              /* Grant                 */
      pdescr(ptr).what |= (unchar) what;
    } else if ((pdescr(ptr).what &= (unchar)(~what))
                                     == 0) { /* элемент опустел       */
      _ZZmov(ptr+sizeof(privil),(Nelems-i-1)*sizeof(privil),ptr);
      --intDATA(scan);
    }
  } else if (flag) {                         /* создаем новый элемент */
    if(ptr0+sizeof(privil)>BlockSize) { ErrCode = ErrFull; return(-1);}
    ptr = ptr0 + Index;
    pdescr(ptr).who  = who ;
    pdescr(ptr).what = (unchar) what;
    ++intDATA(scan);
  }
  SetModified(0,0);
  return(0);
}

    /* создание таблицы абонентов */
void UsersCreate() {
  static logon_elem MainUser = {
     {'S','Y','S',' ',' ',' ',' ',' ',' ',' '},
     {'S','Y','S','T','E','M',' ',' '},
     Priv_Auth, 0
   };

  int i;
  Files[UserFile].status    = CreFile | BsyFile;
  Files[UserFile].mainfile  = NoFile;
  Files[UserFile].nextfile  = NoFile;
  Files[UserFile].kind      = tableKind;
  Files[UserFile].owner     = 0;
  _fill(Files[UserFile].b.tablename,MaxFileName,' ');
  _move("&USERS",6,Files[UserFile].b.tablename);
  if (LockTable((table)UserFile, Oltype) ) myAbort(201);
/*if (LockElem (&Files[UserFile].iM, Sltype, TRUE) ) myAbort(202);*/

  /* чистим, создаем администратора */
  Index = GetPage( (page) 0, (table)UserFile, Nltype);
  for(i=sizeof(page); i<BlockSize; i+=sizeof(short)) intDATA(i)=0;
  _GZmov((char*)&MainUser, sizeof(logon_elem), Index+sizeof(page));
  SetModified(0,0);
}
