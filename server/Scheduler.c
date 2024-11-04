                    /* ========================== *\
                    ** ==       M.A.R.S.       == **
                    ** ==  Диспетчер агентов   == **
                    \* ========================== */

/*
 * Диспетчер агентов
 */
#define  AgentMain
#include "FilesDef"
#include "AgentDef"


         void     Scheduler (void);      /* диспетчер агентов         */
         void     SetWait(agentstate);   /* установ ожидания          */
         void     ClearAgent(void);
         void     Reply     (char*);     /* трассировки               */
         void     Unlock    (lockblock*);/* разблокировка ждущих      */


  extern void     Talk(void);            /* диалог с пользователями   */
  extern void     Reply0(agent,char*);   /* трассировочные выдачи     */

  extern boolean  DeadLock(void);        /* проверка на взаимоблокир. */

  extern void     ExecError(int);        /* выдача сообщ. об ошибках  */


  static agent    LookHead(lockblock*);

void Reply(text) char* text; { Reply0(Agent,text); }

void ClearAgent() {
  Agent->userid      = nobody;         /* развязываемся с user      */
  Agent->privil      = 0;
  Agent->resources   = 0;
  Agent->starttime   = -1;
  MVS((char*) initial, sizeof(jmp_buf),(char*) Agent->pentry);
}

/*
 * Диспетчер агентов
 */
void Scheduler() {
  static short lastcheck = 0;
  do {
    Agent = NIL;
    /*
     * проверим, не пора ли разблокировать агентов,
     * чтоб крикнуть абоненту
     */
    if(OutAgent < 0 && SliceDelta(lastcheck)>= LockTime/2+1) {
      agent ascan = AgentTable+NbAgents;
      while(--ascan>=AgentTable) {
        if(ascan->status==locked &&
                               SliceDelta(ascan->waittime)>=LockTime) {
          if(ascan->queueheader) {           /* агент- 1-ый из ждущих */
            if(ascan->queue != NIL) {        /* есть следующий ждущий */
              ascan->queue->queueheader=TRUE;/* метим его "первым"    */
            }
            ascan->queueheader = FALSE;
          } else {                            /* но не первый ждущий  */
            agent head=LookHead(ascan->waits);/* ищем первого  ждущего*/
            while(head->queue != ascan) head=head->queue;
            head->queue = ascan->queue;       /* вынем его из списка  */
          }
          ascan->status = ready;              /* запустим его         */
          ascan->waits  = NIL;
          if (TraceFlags & TraceLocks) {
            Reply(Fdump("Agent %2.16 unlocked: time-out",
                                          (unsigned)ascan->ident));
          }
          Agent=ascan;
          longjmp(Agent->pentry,jmpProclock);
        }
      }
      lastcheck = SliceTimer;                 /* будить никого не надо*/
    }
    Talk();                                   /* проверим обмен       */
  } while ((Agent=Ready) == NIL);             /* есть готовый агент?  */
  Ready = Ready->queue;                       /* прдвинем очередь     */
  longjmp(Agent->pentry,jmpWaked);
}

/*
 * Установ ожидания: явная диспетчеризация
 */
void SetWait(waittyp) agentstate waittyp; {
  if((Agent->status=waittyp) == ready) {   /* просто time-sharing     */
    Agent->resources = 0;
    Inqueue(&Ready,Agent);                 /* ставим в очередь готовых*/
  } else if (waittyp==locked) {            /* ставим блокировку       */
    if (TraceFlags & TraceLocks) {
      static char logrec[] = "Locked at %4.16 S(%15.2)";
#     define      ltype      16
      lockblock   lock;
      lock = *Agent->waits; logrec[ltype]='S';
      if (Xlocked(lock)) {
        logrec[ltype]='X';
        lock.Slock=setbit((lockmask)0,lock.Xlock.owner);
      }
      Reply( Fdump( logrec,
                   (short)Agent->waits,(short)(lock.Slock>>16)));
    }

    if(DeadLock()) {                       /* допрыгались !!          */
      Agent->waits = NIL; Agent->status=dead;
      ExecError(ErrDeadlock);
    } else {                               /* вставляем в очередь     */
      agent head = LookHead(Agent->waits);
      if(head==0) {                        /* мы первые ждущие        */
        Agent->queue = NIL; Agent->queueheader = TRUE;
      } else {
        Inqueue(&head->queue,Agent);       /* ставим в очередь        */
      }
    }
  }
  Scheduler();
}

/*
 * поиск первого в очереди ждущих данный LOCK-блок
 */
static agent LookHead(ptr) lockblock* ptr; {
  agent scan = AgentTable + NbAgents;
  while (--scan>=AgentTable) {
    if(scan->queueheader && scan->waits==ptr) return(scan);
  }
  return(NIL);
}

/*
 * Разблокирование Lock-элемента: пробуждение всех ждущих
 */
void Unlock(ptr) lockblock* ptr; {
  agent scan = LookHead(ptr);                    /* ищем первый ждущий*/
/*if (scan != 0 && clrbit(ptr->Slock,scan->ident)==0) {
     так не выходит из-за скрытых deadlock-ов           */
  if (scan != NIL) {                             /* ждущие впрямь есть*/
    agent next;
    do {                                         /* будим по порядку  */
      if (TraceFlags & TraceLocks) {
        Reply(Fdump("Agent %2.16 unlocked from &%4.16",
                       (unsigned)scan->ident,(unsigned)(scan->waits)
        ));
      }
      next = scan->queue;
      scan->waits  = NIL;
      scan->queueheader = FALSE;                 /* снимая ожидания   */
      scan->status = ready; Inqueue(&Ready,scan);/* и ставя в очередь */
    } while ((scan=next) != NIL);
  }
}


/*
 * вставка элемента в очередь
 */
void Inqueue(queue,elem) agent* queue; agent elem; {
  while(*queue) queue= &(*queue)->queue;
  *queue = elem; elem->queue = NIL;
}
