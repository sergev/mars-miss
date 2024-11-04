                    /* ======================== *\
                    ** ==       M.A.R.S.     == **
                    ** ==    Главный модуль  == **
                    ** == Цикл работы агента == **
                    \* ======================== */

#define  GlobalMain
#include "FilesDef"
#include "AgentDef"

         void      LockAgent(void);

  extern void      Reply    (char*);    /* выдача отладочного проток. */
  extern void      ErrTrace (void);     /* выдача сообщения об ошибке */
  extern void      ExecError(errcode);  /* обработка Rollback-ошибка  */

  extern int       TabCreate(void);     /* создание    об'ектов       */
  extern int       TabDrop  (void);     /* уничтожение об'ектов       */
  extern int       TabLock  (void);     /* блокировка  таблиц         */

  extern int       ServPriv (boolean);  /* изменение привилегий       */
  extern int       Logon    (void);     /* идентификация в системе    */

  extern int       LinkTree (void);     /* привязка дерева к данным   */
  extern int       Optimize (void);     /* оптимизация дерева разбора */
  extern int       Translate(void);     /* генерация машинного кода   */

  extern void      ExecCode   (unsigned,short,char*);
  extern void      ReleaseCode(unsigned);

  extern int mfar  Parser   (char*);    /* разбор текстового запроса  */
  extern void      SaveParse(void);     /* спасение Parse-блока в Temp*/
  extern void      RestParse(void);     /* восстановление - "- из -"- */

  extern void      Scheduler(void);     /* диспетчер агентов          */
  extern void      SetWait(agentstate); /* установ ожидания          */
  extern void      ClearAgent(void);

  extern void      Commit   (boolean);  /* Commit/Rollback транзакции */
  extern void      Show     (char);     /* управление консолью базы   */
  extern void      UsersCreate(void);   /* создание USERS-каталога    */

  extern void      Archive(char,fileid);/* архивация базы             */
  extern void      Init0    (int);      /* инициация системы          */
  extern void      Init(int,char**);    /* инициация системы          */

  extern void      FreeChannel(int);    /* пометка канала "окончание" */

  static void      ErrReply (void);     /* выдача сообщения об ошибке */
  static void      OkReply  (void);     /* выдача ответа "O.K."       */


int main(argc, argv) int argc; char **argv; {

  int retcode;

  ErrPosition  = -1;
  ScanFlag     =  FALSE;
  OutBuffer    =  NIL;
  OutAgent     = -1;
  TraceFlags   = ~0;
  Timer        =  0;

  Init(argc,argv);

  switch(setjmp(initial)) {
  case 0:                           /* первый раз ->             */
    for(Agent=AgentTable+NbAgents; --Agent>=AgentTable;) ClearAgent();
    Scheduler();                    /* переход в MultiUser       */

  case jmpLastkiss:                 /* ответ на Disconnect       */
    OkReply();

  case jmpDisconn:                  /* отключение канала         */
    Commit(FALSE);
    FreeChannel(Agent->ident);      /* после выдачи ответа (возм)*/
    ClearAgent();                   /* развязываемся с user      */
    SetWait(reading);               /* ждем инициации            */

  case jmpRollback:                 /* фатальная ошибка          */
    Commit(FALSE);                  /* (попадаем при ExecError)  */
    ErrReply();

  break; case jmpWaked:             /* запуск по инициализации   */
    if(! (Files[UserFile].status & BsyFile)) {
      UsersCreate(); Commit(TRUE);  /* нет &USERS - создадим     */
    }
    Agent->userid    = nobody;
    Agent->privil    = 0;
    Agent->starttime = -1;

    switch (*Agent->inpbuf) {
    default:                        /* пока не иниц-уют, ругаемся*/
      ErrCode = ErrInit;   ErrReply();
      SetWait(reading);
    case '#':                       /* на discon остаемся там же */
      longjmp(initial,jmpLastkiss);
    case '$':                       /* для входа после сбоя у-ва */
      longjmp(initial,jmpDisconn);
    case 'G':                       /* пошло подключение         */
      OkReply();
    }
  }

  for(;;) {
    /* чтение команды от абонента, ее разбор */
    for(;;) {
      if(setjmp(Agent->pentry) == 0) SetWait(reading);
      retcode         = 0;
      Agent->contflag = FALSE;
      switch(*Agent->inpbuf) {
      case '#':                           /* Disconnect    */
        longjmp(initial,jmpLastkiss);
      case '$':                           /* Interf. error */
        longjmp(initial,jmpDisconn);
      case 'E':                           /* Exec request  */
        Agent->opcode = OpExec;
        Agent->parms.codeNum   = Agent->inpbuf[1]-'0';
      break; case 'F':                    /* Fetch record  */
        Agent->opcode = OpFetch;
        Agent->parms.codeNum   = Agent->inpbuf[1]-'0';
      break; case '>':                    /* Put LongData  */
        Agent->opcode = OpPutLong;
        Agent->parms.codeNum   = Agent->inpbuf[1]-'0';
      break; case '<':                    /* Get LongData  */
        Agent->opcode = OpGetLong;
        Agent->parms.lGet.codeNum   = Agent->inpbuf[1]-'0';
        Agent->parms.lGet.columnNum = (unchar)*(short*)(Agent->inpbuf+2);
        Agent->parms.lGet.shift     = *(long*)(Agent->inpbuf+2+sizeof(short));
        Agent->parms.lGet.size      = *(long*)(Agent->inpbuf+2+sizeof(short)+sizeof(long));
      break; case 'C':                    /* Commit        */
        Agent->opcode = OpCommit;
      break; case 'R':                    /* RollBack      */
        Agent->opcode = OpRollback;
      break; case 'K':                    /* Kill Code     */
        Agent->opcode = OpRelease;
        Agent->parms.codeNum   = Agent->inpbuf[1]-'0';
      break; default :                    /* Translate     */
        Agent->opcode = (unchar)(retcode = Parser(Agent->inpbuf));
      }
    if(retcode >=0 ) break;
      ErrReply();
    }


    /* если заблокируют по ходу дела, то продолжим отсюда */

    switch (setjmp(Agent->pentry)) {
    case 0: case jmpRepeat:                /* сразу; при Crea Index   */
    break;  case jmpProclock:              /* мы слишком долго ждем   */
      MesBuffer[1] = 1;
    case jmpProceed:                       /* мы слишком долго раб-ем */
      MesBuffer[0] = 0;
      OutAgent     = Agent->ident;
      OutBuffer    = MesBuffer;
      if (TraceFlags & TraceOutput) Reply("Proceed ?");
                        SetWait(reading);

    case jmpLocked:                        /* нас блокирнули          */
      if(Agent->opcode < OpParsed) SaveParse();
      Agent->inpbuf = NIL;
      if(Agent->waits != NIL) {
        SetWait(locked);
      } else {
        if(SliceDelta(Agent->waittime)>=ContTime) {
          MesBuffer[1] = 0;                /* мы слишком долго раб-ем */
          longjmp(Agent->pentry,jmpProceed);
        }
        SetWait(ready);
      }
    case jmpWaked:                            /* нас разблокировали   */
      if(Agent->opcode < OpParsed) RestParse();
      if(Agent->inpbuf != NIL) switch(*Agent->inpbuf) {
      default:                                /* прервать операцию    */
        ExecError(ErrInterrupted);
      case '#':                               /* Disconnect           */
        longjmp(initial,jmpLastkiss);
      case '$':                               /* Interf. error        */
        longjmp(initial,jmpDisconn);
      case 'P':                               /* продолжить операцию  */
         ;
      }
    }


    /* выполняем заказанные операции */

    retcode = 0;
    switch (Agent->opcode) {
    case OpCreate:
      if ((retcode = TabCreate()) == 100) {   /* создадим индекс ?    */
        if ((retcode = Translate()) >= 0) {   /* оттранслируем запрос */
          Agent->parms.codeNum = (unchar)retcode;/* теперь пошли его  */
          Agent->contflag = FALSE;            /* выполнять            */
          Agent->opcode   = OpExecIndex;
          longjmp(Agent->pentry,jmpRepeat);
        }
      }
    break; case OpDrop:
      retcode = TabDrop();

    break; case OpLogon:                     /* Logon <username>    */
      retcode = Logon();

    break; case OpGrant: case OpRevoke:      /* Grant, Revoke       */
      retcode = ServPriv(Agent->opcode==OpGrant);

    break; case OpLock :                     /* Lock table          */
      retcode = TabLock();

    break; case OpShow :                     /* Show                */
      if (Agent->privil & (Priv_Oper | Priv_Auth)) {
        Show(Agent->parms.showSym);
      } else {
        retcode = -1; ErrCode = ErrRights;
      }
    break; case OpTrace:                     /* Trace               */
      if (! (Agent->privil & (Priv_Oper | Priv_Auth))) {
        retcode = -1; ErrCode = ErrRights;
      } else if(Agent->parms.traceMask >= 0) {
        TraceFlags |= Agent->parms.traceMask;
      } else {
        TraceFlags &= Agent->parms.traceMask;
      }
    break; case OpCommit:                    /* Commit              */
      Commit(TRUE);
    break; case OpRollback:                  /* Rollback            */
      Commit(FALSE);

    break; case OpCompile:                   /* Translate           */
      if ( (retcode=LinkTree()) >=0 &&
           (retcode=Optimize()) >=0 &&
           (retcode=Translate())>=0) {
        retcode = -100;
      }

    break; case OpExec:                      /* Exec                */
      ExecCode(Agent->parms.codeNum,
                           Agent->contflag ? doContinue : doStartExec,
                           Agent->contflag ? NIL : Agent->inpbuf+2);
      retcode = -100;

    break; case OpExecIndex:                 /* Create Index, шаг 2 */
      ExecCode(Agent->parms.codeNum,
                           Agent->contflag ? doContinue : doStartExec,NIL);
      ReleaseCode(Agent->parms.codeNum);

    break; case OpRelease:                   /* Kill Code           */
      ReleaseCode(Agent->parms.codeNum);

    break; case OpFetch:                     /* Fetch               */
      ExecCode(Agent->parms.codeNum,
               Agent->contflag ? doContinue : doStartFetch,NIL);
      retcode = -100;

    break; case OpPutLong:                   /* Put LongData        */
      ExecCode(Agent->parms.codeNum,
                           Agent->contflag ? doContinue : doPutLong,
                           Agent->contflag ? NIL : Agent->inpbuf+2);
      retcode = -100;

    break; case OpGetLong:                   /* Get LongData        */
      ExecCode(Agent->parms.lGet.codeNum,doGetLong,NIL);
      retcode = -100;

    break; case OpArchive:                   /* архивация          */
      if (! (Agent->privil & (Priv_Oper | Priv_Auth))) {
        retcode = -1; ErrCode = ErrRights;
      } else {
        Archive(Agent->parms.archMode,0);
      }
    break; default :
      ErrCode = ErrCommand; retcode = -1;
    }

    /* выдаем ответ о проделанной работе */
    if(retcode >= 0) {
      OkReply();
    } else if (retcode != -100) {
      ErrReply();
    }
  }
}

/*
 * Процедура вызывается при необходимости заблокировать агента
 * производит возврат (longjmp) в main, где
 * спасается (если надо) буфер Parser-а, ставится (если надо) блокировка
 * и вызывается диспетчер, после чего повторяется операция
 */
void LockAgent() {
  Agent->contflag = TRUE;
  longjmp(Agent->pentry,jmpLocked);
}

/*
 * Обнаружение ошибки при выполнении кода: откат
 */
void ExecError(code) errcode code; {
  ErrCode = code;
  longjmp(initial,jmpRollback);
}

/*
 * Выдача пользователю ответа "O.K."
 */
static void OkReply() {
  if (TraceFlags & TraceOutput) Reply("O.K.");
  MesBuffer[0] = sizeof(short);
  OutBuffer    = MesBuffer;
  OutAgent     = Agent->ident;
}

/*
 * Выдача пользователю сообщения об ошибке
 */
static void ErrReply() {
  if (TraceFlags & TraceOutput) ErrTrace();
  MesBuffer[0] = -ErrCode;
  MesBuffer[1] =  ErrPosition;
  OutBuffer    = MesBuffer;
  OutAgent     = Agent->ident;
  ErrPosition  = -1;
}
