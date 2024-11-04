                    /* =========================== *\
                    ** ==       M.A.R.S.        == **
                    ** == Синтаксический разбор == **
                    ** ==    текста запроса     == **
                    \* =========================== */

/*
 * Выражение разбирается, на выход выдается либо сообщение об ошибке:
 * -1, replybuf заполнен сообщением
 * либо синтаксически разобранное дерево помещается в Z-блок
 * и выдается код операции
 */
#include "FilesDef"
#include "AgentDef"
#include "TreeDef"
#include "BuiltinDef"

#define MaxNodes 10

CodeSegment(Parser)
         parseheader* ParseHeader;

         int   mfar   Parser(char*);

  extern char* mfar   GetMem  (int);      /* заказ памяти в "Tree"    */


#define  MaxNumeric sizeof(real)          /* максимальное числовое    */

  static void     ERROR      (int );

  static void     ReadFields (void);      /* чтение полей с их опис.  */
  static dsc_column*
                  LookField  (char*,int); /* поиск поля в подузлах    */

  static void     PutFName   (int);       /* чтение имени с фикс. дл. */
  static char*    PutTabName (int);       /* чтение имени таблицы     */

  static char     GetSym     (void);      /* чтение со скан. пробелов */
  static void     CheckSym   (char);      /* проверка на обязат.символ*/
  static int      CheckKey(char*,boolean);/* проверка на ключ         */
  static int      LookKey(char**,boolean);/* проверка на список ключей*/
  static int      CheckList  (void);      /* проверка на ","          */

  static boolean  IsDigit    (char);      /* проверка: число/не число */
  static boolean  IsLetter   (char);      /* проверка: буква/не буква */
  static boolean  IsConst    (char);      /* проверка: начало конст.  */

  static short    GetNum     (void);      /* чтение натурального числа*/
  static int      GetName    (int );      /* чтение имени             */
  static node     GenNode    (void);
  static nodelist GetSubNodes(node);

  static void     ParseCreate(void);
  static void     ParseTransl(void);

  static char*    scan;                   /* сканер исходного текста  */
  static char*    begscan;                /* начало исходного текста  */

  static dsc_table* Fields;               /* список описаний полей    */

  static char     constbuffer[MaxNumeric];/* буфер под считанное число*/
  static int      constlength;
  static datatype consttype;

  static char     ttable[]   = "TABLE";
  static char     Tindex[]   = "INDEX";
  static char     Twhere[]   = "WHERE";
  static char     Tread  []  = "READ";
  static char     Tselect[]  = "SELECT";
  static char     Tinsert[]  = "INSERT";
  static char     Tdelete[]  = "DELETE";
  static char     Tupdate[]  = "UPDATE";
  static char     Tadmin []  = "ADMIN";
  static char     Tcreate[]  = "CREATE";
  static char     Toper  []  = "OPER";
  static char     Ton    []  = "ON";
  static char     Tfrom  []  = "FROM";
  static char     Tall   []  = "ALL";

  static char*    operwords[]= {Tread,Tinsert,Tdelete,Tupdate,NIL};
  static char*    accesswords[]= {Tselect,Tinsert,Tdelete,Tupdate,NIL};

int mfar Parser(scanner) char* scanner; {
  static char* keywords[] =
    {"LOGON",
     Tcreate,  "DROP",     "COMPILE", "LOCK",
     "GRANT",  "REVOKE",
     "COMMIT", "ROLLBACK", "SHOW",    "TRACE",
     "EXEC"  , "PEEK",    "ARCHIVE",
     NIL};
  int       code;
  begscan           =  scanner;
  scan              =  scanner;

  ParseHeader = (parseheader*) ParseBuf;
  ParseHeader->length   = sizeof(struct parseheader);
  ParseHeader->eqlist   = NIL;

  if( (ErrCode = setjmp(err_jump)) != 0) return(-1);
  switch (code = LookKey(keywords,FALSE)) {
  case OpLogon:                             /* Logon username password*/
    PutFName(UserName); PutFName(PasswLng);
  break; case OpCreate:
    ParseCreate();
  break; case OpDrop: {
    short* objmode = GEN(short*);
    *objmode = 0;
    if(CheckKey(Tindex,FALSE)>=0) {         /* drop index             */
      *objmode = 1;
      PutFName(MaxIndxName);
      CheckKey(Ton,TRUE);                   /* ... on ...             */
    }
    PutTabName(FALSE);                      /* [table] tablename      */

  } break; case OpGrant: case OpRevoke: {   /* Grant read xxxx to yyy */
    static char* qualwords[] = {"CONNECT",Tadmin,Tcreate,Toper,NIL};
    short*       grantmode = GEN(short*);

    /* Grant что-нибудь to кому-нибудь */
    if ( (*grantmode= ~ LookKey(qualwords,FALSE))>=0) {
      if( CheckKey(Tall,FALSE)>=0) {           /* Grant all on ttt to*/
        CheckKey(Ton,TRUE); *grantmode = 0xF;
      } else {                                  /* Grant update,insert*/
        for (;;) {                              /* цикл по доступам   */
	  *grantmode |= 1<<LookKey(accesswords,TRUE);
        if (GetSym() != ',') break;             /* пока идут доступы  */
          ++scan;
        };
      }
      PutTabName(FALSE);
    }
    CheckKey( (code==OpGrant ? "TO" : Tfrom), TRUE);
    if (CheckKey("PUBLIC",FALSE)>=0) {
      *GEN(short*) = '*';                       /* to PUBLIC -> *     */
    } else {
      PutFName( UserName);                      /* to username        */
    }

    if (code==OpGrant && ~ *grantmode ==0) {    /* grant connect to ..*/
      CheckSym('('); PutFName(PasswLng); CheckSym(')');
    }

  } break; case OpLock: {                   /* Lock table in xxx mode */
    short* objmode = GEN(short*);
    static char* lockwords[] = { "SHARE", "EXCLUSIVE", NIL};

    PutTabName(FALSE);                      /* LOCK [table] ....      */
    CheckKey("IN",TRUE);
    *objmode = LookKey(lockwords,TRUE);
    CheckKey("MODE",TRUE);

  } break; case OpShow: {
    Agent->parms.showSym = GetSym(); ++scan;

  } break; case OpArchive: {
    static char* archwords[] = { "NOMOD", "NOFILE", NIL};
    Agent->parms.archMode = "AIL" [LookKey(archwords,FALSE)+1];

  } break; case OpCommit: case OpRollback: {
    CheckKey("WORK",FALSE);     /* необязательный аргумент WORK */

  } break; case OpTrace: {
    static char* traceregs[]=
      {Tall,  "IO","CODE","SORT","OUTPUT","INPUT",
              "SWAP","LOCKS","CACHE","COMMANDS",NIL};
    static char* traceswitch[]= {Ton,"OFF",NIL};
    short key = LookKey(traceregs,TRUE);
    Agent->parms.traceMask = key == 0 ? 0xFF : (1 << (key-1));
    if(LookKey(traceswitch,FALSE)==1) Agent->parms.traceMask ^= -1;

  } break; case OpExec: {
    char* inpbuf = Agent->inpbuf+2;
    int i;
    Agent->parms.codeNum = (unchar) GetNum();
    *inpbuf = 0;
    if(GetSym()=='(') {
      ++scan;
      for(i=0; i<MaxInput-2; i++) {
        inpbuf[i] = (char) _convi(scan,2,16);
      if(*(scan += 2) !=' ') break;
        ++scan;
      }
      CheckSym(')');
    }
  } break; case OpFetch: {
    Agent->parms.codeNum = (unchar) GetNum();

  } break; default:
    code = OpCompile; ParseTransl();
  }
  CheckSym(';');

  ErrPosition = -1;
  return(code);
}

/*
 * Чтение имени таблицы
 */
static char* PutTabName(key) int key; {
  char* result   = GetMem(MaxFileName+UserName);
  char* name;
  int   nameleng;
  _fill(result,MaxFileName+UserName,' ');
  if(key>=0) CheckKey(ttable,key);
  nameleng = GetName(MaxFileName); name = scan-nameleng;
  if (GetSym() == '.') {                /* имя пользователя . имя_табл*/
    if(nameleng > UserName) ERROR(ErrName);
    MVS(name,nameleng,result+MaxFileName);
    ++scan;
    nameleng = GetName(MaxFileName); name = scan-nameleng;
  }
  MVS(name,nameleng,result);
  return(result);
}

/*
 * Чтение имени и перенос его в буфер (с пробелами)
 */
static void PutFName(nameleng) int nameleng; {
  int   realleng = GetName(nameleng);
  char* namebuf  = GetMem (nameleng);
  MVS(scan - realleng, realleng , namebuf);
  _fill(namebuf+realleng,nameleng-realleng,' ');
}

/*
 * Создание/уничтожение таблиц
 */
  static char* typewords[] = {
	"SHORT", "SMALLINT", "LONG", "INTEGER", "FLOAT",
	"CHAR", "CHARACTER", "VARCHAR", "LONGDATA",
	NIL,
  };
  static datatype types [sizeof(typewords)/sizeof(char*)-1] = {
	Tshort, Tshort, Tlong, Tlong, Tfloat,
	Tchar, Tchar, Tvar, Tldata,
  };
  static short lengths [sizeof(typewords)/sizeof(char*)-1] = {
	sizeof(short), sizeof(short), sizeof(long), sizeof(long), sizeof(real),
	0, 0, 0, sizeof(ldataDescr),
  };

  static char*    orderkeys[]=
       {"ASC","DESC",NIL};

static void ParseCreate() {
  short      uniqueflag = (CheckKey("UNIQUE",FALSE) == 0);

  if(! (*GEN(short*) = (CheckKey(Tindex,uniqueflag) >= 0))){
    short* lengword;
    PutTabName(FALSE);                        /* описание таблицы     */
    lengword = GEN(short*);                   /* слово с длиной       */
    CheckSym( '(' );
    ReadFields();
    *lengword = GetMem(0) - (char*)(lengword + 1);
    if( *lengword + sizeof(short) + sizeof(short)*(Fields->Ncolumns+1)>=
        BlockSize) ERROR(ErrLeng);
  } else {                                   /* описание индекса      */
    char** colarry;
    int   count = 0;
    PutFName(MaxIndxName);
    CheckKey(Ton,TRUE); PutTabName(FALSE);   /*create index on [table]*/
    *GEN(short*) = uniqueflag;
    CheckSym( '(' );
    colarry = (char**) GetMem(MaxIndCol * sizeof(char*) );
    do {
      int   nameleng = GetName(MaxColName);
      int   i;
      char* nameadr  = scan - nameleng;
      char* descr;
      for(i=0; i<count ; i++) {              /* имена д.б. уникальны  */
        descr = colarry[i] + sizeof(short)*2;
        if(*descr == nameleng && _cmps(descr+1,nameadr,nameleng)==0) {
          scan = nameadr;
          ERROR(ErrDoubleDef);
        }
      }
      if (count >= MaxIndCol) ERROR(ErrComplicated);
      colarry[count++] = descr = GetMem (sizeof(short)*2+nameleng+1);
      * (short*) descr = nameadr - begscan; /* позиция в тексте       */
      * ((short*) descr + 1) = (LookKey(orderkeys,FALSE) == 1);
      descr += sizeof(short)*2;
      *descr = (char) nameleng;
      MVS(nameadr,nameleng,descr+1);
    } while ( CheckList());
    while(count<MaxIndCol) colarry[count++]=0; /* неиспольз. колонки  */
  }
  CheckSym(')');
}

/*
 * Чтение списка имен с описаниями типов (для Create, Compile)
 * список кладется в память в виде послед-ти выровненных записей
 * первая запись - описатель списка полей (dsc_table) -> C....
 * далее по 1 записи на поле
 */
static void ReadFields() {
  int         Nvars     = 0;                 /* число полей VARCHAR */
  unsigned    Lvars     = 0;                 /* сум. длина  VARCHAR */
  unsigned    Ashift    = sizeof(short);     /* адрес поля  Align   */
  unsigned    Cshift    = sizeof(short);     /* адрес поля  CHAR    */
  unsigned    Cvars     = 0;
  dsc_column* rown;                          /* описатель столбца   */
  int i;

  Fields = GEN(dsc_table*);

  Fields->totalleng = sizeof(dsc_table);    /* длина 0-ой записи     */
  Fields->Ncolumns  = 0;

  /* читаем описатели столбцов */
  do {
    int   nameleng = GetName(MaxColName);
    char* nameadr  = scan - nameleng;
    int   nkey;

    int rowleng = Align(sizeof(dsc_column)-DummySize + nameleng + 1);

    rown = (dsc_column*) GetMem(rowleng);

    if( LookField(nameadr,nameleng) != 0) {  /* такое имя уже есть ?  */
      scan = nameadr ; ERROR(ErrDoubleDef);
    }

    rown->totalleng  = rowleng;               /* заносим имя столбца  */
    rown->name[0]    = (char) nameleng;
    MVS(nameadr,nameleng,rown->name+1);
    Fields->Ncolumns++;

#if 0
    CheckSym( ':' );                          /*обработка типа столбца*/
#else
    if (GetSym() == ':') ++scan;              /* пробел или двоеточие */
#endif
    switch(rown->type = types[ nkey = LookKey(typewords,TRUE) ] ) {
           case Tvar   : rown->shift= ++Nvars * sizeof(short);
           case Tchar  :
             CheckSym( '(');
             if((rown->length= GetNum()) >= MaxString) ERROR(ErrLeng);
             CheckSym(')');
    break; default     : rown->length = lengths[nkey];
    }
    if(rown->type!=Tvar && rown->type!=Tchar) Cshift += rown->length;
  } while (CheckList());

  /* Распределяем адреса колонок в кортежах */

  Ashift += Nvars * sizeof(short);         /* начало Align-полей      */
  Cshift += Nvars * sizeof(short);         /* начало Char -полей      */
  for(rown = (dsc_column*) (Fields + 1), i = 0;  i < Fields->Ncolumns;
      rown = (dsc_column*) ( (char*) rown + rown->totalleng), ++i) {
    switch(rown->type) {
           case Tchar: rown->shift = Cshift ; Cshift += rown->length;
    break; case Tvar : rown->shift = (Nvars- ++Cvars)* sizeof(short);
                                              Lvars  += rown->length;
    break; default   : rown->shift = Ashift ; Ashift += rown->length;
    }
  }
  Fields->Nvarlength = Nvars;              /* число VAR-полей         */
                                           /* сумм. длина fix-полей   */
  Fields->Mfixlength = Cshift-Nvars*sizeof(short);
  if( (Fields->Mtotlength = Cshift+Lvars) >= MaxRowLeng) ERROR(ErrLeng);
}

/*
 * Поиск поля с заданным именем в Fields.
 */
static dsc_column* LookField(nameadr,nameleng)
                                  char* nameadr; int nameleng; {
  dsc_column* oldcolumn;
  int Ncolumns = Fields->Ncolumns;
  for(oldcolumn = (dsc_column*) (Fields+1); --Ncolumns >= 0;
      oldcolumn=(dsc_column*)(((char*)oldcolumn)+oldcolumn->totalleng)){
    if(oldcolumn->name[0] == nameleng &&
       _cmps(oldcolumn->name+1,nameadr,nameleng) == 0) {
      return(oldcolumn);
    }
  }
  return(NIL);
}


/*
 * Служебные п/п
 */
static char GetSym() {
  while (*scan == ' ') ++scan;
  return(*scan);
}

static void CheckSym(char sym) {
  if(GetSym() != sym) ERROR(ErrSymbol);
  ++scan;
}

static boolean CheckList() {
  if(GetSym() == ',') { ++scan; return(TRUE); }
  return(FALSE);
}

/*
 * Проверка, что имеется заданное ключевое слово.
 * (нет -> сканнер на 1-ом символе слова)
 */
static int CheckKey(keyword,flag) char* keyword; boolean flag; {
  char* save;
  GetSym();  save = scan;
  for(;;) {
    if (*keyword==0) {
      if(! IsLetter(keyword[-1]) || ! IsLetter(*scan)) {
        return(0);                         /* после буквы нельзя букву*/
      }                                    /* т.е. NOTA=B неверно     */
    }
    if ( UpperCase(*scan) != *keyword++) break;
      ++scan;
  }
  scan = save;
  if(flag) ERROR(ErrKey);
  return(-1);
}

/*
 * Поиск в списке ключей. Выход - номер ключа в списке
 */
static int LookKey(keylist,flag) char* keylist[]; boolean flag; {
  char* keyword;
  int   retcode;

  for(retcode=0 ; (keyword = keylist[retcode]) !=0 ; ++retcode) {
    if(CheckKey(keyword,FALSE)==0) return(retcode);
  }
  if(flag) ERROR(ErrKey);
  return(-1);
}

/*
 * Чтение имени: выход - длина имени, курсор - сразу за ним
 */
static int GetName(maxleng) int maxleng; {
  int nameleng = 0;
  if( ! IsLetter( GetSym() ) ) ERROR(ErrName);
  do {
    if(++nameleng>maxleng) ERROR(ErrName);
    ++scan;
  } while (IsDigit(*scan) || IsLetter(*scan) || *scan=='_');
  return(nameleng);
}

/*
 * Чтение натурального числа
 */
static short GetNum() {
  long  retvalue =0;
  if( ! IsDigit(GetSym()) )  ERROR(ErrNum);

  while ( IsDigit(*scan) ) {
    if ((retvalue = retvalue * 10 + *scan -'0')>=(unsigned)0x8000) {
      ERROR(ErrNum);
    }
    ++scan;
  }
  return((short) retvalue);
}

static boolean IsLetter(char byte) {
  byte = UpperCase(byte);
  if (byte >= 'A' && byte <= 'Z')
    return (1);
#ifdef MyIsLetter
  if (MyIsLetter (byte))
    return (1);
#endif
  return (0);
}

static boolean IsDigit(char byte) {
  return (byte >= '0' &&  byte <= '9');
}

static boolean IsConst(char byte) {
  return (byte == '\'' || byte == '"' || IsDigit(byte) || byte=='.');
}


/*
 * Разбор предложений манипулирования данными
 */
  static node    CurrentNode;
  static node    TableList;

  static node    GenTabNode (void);        /* генерация Т-вершины     */
  static node    Query      (void);        /* трансляция запроса      */
  static node    SubQuery   (void);
  static expr    ReadExpr   (node);        /* трансляция скалярн. выр */
  static expr    ParseExpr  (int);         /* разбор бинарного        */
  static expr    ParseUnary (int);         /* разбор унарного         */
  static void    ReadConst  (void);        /* чтение констант         */
  static expr    ReadEquery (exproper);    /* чтение подзапроса       */
  static void    LetList    (node);        /* список присваиваний     */

  static projection
                 ReadProj   (node,char*,int);
  static projection
                 LookProj   (char*, int,node);
  static expr    GenProj    (projection,unsigned);
  static expr    GenConstant(char*,int,datatype,char*);

  static projection
                 InsProjList(char*,int,node);
  static void    ChkNameList(node,nodelist);

typedef struct eqchain* eqchain;

  /* структура для описания вложения E-запросов */
struct eqchain {
  eqchain   upper;                        /* верхний элемент          */
  short     level;
  node      extnode;                      /* узел, в котором был E-q  */
  short     reductLevel;                  /* уровень редукции         */
  prjlist   extprojs;                     /* внешние,исп-нные в E-query*/
};

  static eqchain   EQchain;                 /* список вложенных запросов*/


/*
 * Разбор компилируемого выражения
 */
typedef enum {                            /* операции манипулирования */
   CoGet, CoInsert, CoDelete, CoUpdate 
} cocodes;


static void ParseTransl() {
  struct eqchain upList;

  upList.upper       = NIL;
  upList.extnode     = NIL;
  upList.extprojs    = NIL;
  upList.level       = 0;
  upList.reductLevel = 0;
  EQchain            = &upList;

  TableList = NIL;
  if( GetSym() == '(') {           /* есть параметры                  */
    ++scan; ReadFields();          /* читаем их                       */
    CheckSym( ')' );
  } else {                         /* запрос без параметров           */
    Fields = GEN(dsc_table*);
    Fields->totalleng  = sizeof(dsc_table);
    Fields->Ncolumns   = 0;
    Fields->Nvarlength = 0;
    Fields->Mfixlength = 0;
    Fields->Mtotlength = 0;
  }

  ParseHeader->inpparams = Fields;

  switch (LookKey(operwords,FALSE)) {
  case CoInsert: {                     /* разбор предложения "Insert" */
    node  Node     = GenNode();

    if (GetSym() == '(') {                 /* Insert с подзапросом    */
      ++scan; Node->mode = INSERT;         /* Insert (query) into tab */
      Node->projlist = (Node->body.I.subq = Query())->projlist;
      CheckSym( ')');
    } else {                               /* Insert values           */
      CheckKey("VALUES",TRUE);
      Node->mode = INSVALUE; LetList(Node);
    }
    CheckKey("INTO",TRUE);
    Node->body.I.tablename = PutTabName(-1);
    ParseHeader->Node = Node;
  } break; case CoGet: case -1: {
    ParseHeader->Node = Query();
  } break; case CoDelete: {            /* Разбор предложения "Delete" */
    node  Node = GenNode();
    Node->mode = DELETE;

    CheckKey(Tfrom,FALSE);                 /* Delete [from] table ... */
    Node->body.M.subq     = GenTabNode();
    Node->body.M.predlist = NIL;
    if(CheckKey(Twhere,FALSE)>=0) {        /* Delete table where ...  */
      Node->body.M.predlist = GEN(exprlist);
      Node->body.M.predlist->next = NIL;
      Node->body.M.predlist->expr = ReadExpr(Node);
    }
    ParseHeader->Node = Node;
  } break; case CoUpdate: {             /* Разбор предложения "Update"*/
    node  Node = GenNode();
    Node->mode = UPDATE;

    /* заполним имя мод-емой таблицы */
    Node->body.M.subq     = GenTabNode();
    Node->body.M.predlist = NIL;

    if (CheckKey(Twhere,FALSE) >= 0) {
      Node->body.M.predlist = GEN(exprlist);
      Node->body.M.predlist->next = NIL;
      Node->body.M.predlist->expr = ReadExpr(Node);
    }

    CheckKey("SET",TRUE); LetList(Node);
    ParseHeader->Node = Node;

  } break; default:
    ERROR(ErrSyst);
  }
  ParseHeader->tablelist = TableList;
}

/*
 * Разбор списка присваиваний
 */
static void LetList(Node) node Node; {
  int keyflag = 0;
  do {
    int        nameleng;
    projection pscan;
    if(keyflag >= 0 && IsLetter(GetSym())) {
      char* savescan = scan; GetName(MaxColName);
      if(GetSym() == '=') keyflag = -1;    /* начались позиционные    */
      scan = savescan;
    }
    if(keyflag<0) {                        /* должны быть ключи       */
      nameleng = GetName(MaxColName);
      pscan    = InsProjList(scan-nameleng,nameleng,Node);
      CheckSym('=');
    } else {                               /* позиционные параметры   */
      static char fictname[2] = {'*',0};   /* фиктивные имена         */
      fictname[1] = (char)keyflag;
      pscan = InsProjList(fictname,sizeof(fictname),Node);
      ++keyflag;
    }
    pscan->expr = ReadExpr(Node);
  } while ( CheckList() );               /* список значений         */
}

/*
 * Разбор подзапроса
 */
recursive static node SubQuery() {
  static node  Node;

  char*        nodelabel = NIL;
  static int   nameleng;
  static char* nodename;

  while (GetSym() != '(' && *scan != '&') {   /* (subq) ...           */
    nameleng  = GetName(MaxFileName);         /* имя таблицы (метки)  */
    nodename  = scan - nameleng;

    if(GetSym() != ':') {                     /* это имя таблицы      */
      scan = nodename;
      Node = GenTabNode();
      if(nodelabel) Node->name = nodelabel;
      return(Node);
    }
    if(nodelabel != NIL) ERROR(ErrSyntax);    /* 2 метки подряд нельзя*/
    nodelabel  = GetMem( nameleng + 1);
    *nodelabel = (char)nameleng;
    MVS(nodename, nameleng, nodelabel + 1);
    ++scan;                                   /* запомнили как метку  */
  }

  switch (*scan++) {
    int spcflag;
    static char* specwords[] =
        {"CATALOG","COLUMNS","ACCESS","USERS",NIL};
  case '(':
    Node = Query();
    CheckSym(')');
  break; case '&':                            /* спец-таблица         */
    switch (spcflag=LookKey(specwords,TRUE)) {
    case 0: case 3:                           /* &CATALOG             */
      Node = GenNode();
      Node->mode          = CATALOG;
      Node->body.A.regime = spcflag;
    break; case 1: case 2:                    /* &COLUMNS, ACCESS     */
      CheckSym('(');
      Node = GenTabNode(); Node->body.N.spflag = spcflag;
      CheckSym(')');
    }
  }
  if (nodelabel != NIL) Node->name = nodelabel;
  return(Node);
}

/*
 * Создание узла "ссылка на таблицу"
 */
static node GenTabNode() {
  int   nameleng = MaxFileName;
  node  Node = GenNode();
  Node->mode             = TABLE;
  Node->body.N.spflag    = 0;
  Node->body.N.next      = TableList; TableList = Node;

  Node->body.N.tablename = PutTabName(-1);
  while(Node->body.N.tablename[--nameleng]== ' ');

  Node->name  = GetMem(++nameleng + 1);
  *Node->name = (char) nameleng;
  MVS(Node->body.N.tablename, nameleng, Node->name + 1);

  return(Node);
}

/*
 * Проверка на дублирования имен подзапросов
 */
static void ChkNameList(new,list) node new; nodelist list; {
  if (new->name != NIL) {               /* задано имя запроса         */
    while (list != NIL) {
      if(list->node->name != NIL &&     /* у старого задано имя       */
         _cmps(new->name,list->node->name,*new->name+1) == 0)
                  ERROR(ErrDoubleDef);  /* они совпали -> ошибка      */
      list = list->next;
    }
  }
}

/*
 * Ключевые слова запросов на выборку данных
 */
  static char* getkeys[]=
     {"SELECT", "JOIN","UNION","GROUP","ORDER","DISTINCT",NIL};

/*
 * Разбор запроса на доступ к данным
 */
recursive static node Query() {
  int   mode;
  node  subq;
  node  Node = GenNode();
  mode      = LookKey(getkeys,TRUE);

  switch (Node->mode = mode) {
#if 1
  case SELECT: case JOIN:
    subq = SubQuery();
    if (GetSym() != ',') {                        /* select */
      exprlist list = NIL;
      Node->mode = SELECT;
      Node->body.S.subq = subq;
      if (CheckKey(Twhere,FALSE) >= 0) {
	list = GEN(exprlist);
	list->next = NIL;
	list->expr = ReadExpr(Node);
      }
      Node->body.S.predlist  = list;

    } else {                                            /* join */
      nodelist last = Node->body.J.subqs = GEN(nodelist);
      Node->mode = JOIN;
      last->node = subq;
      last->next = NIL;
      while (CheckList()) {
	subq = SubQuery();
	ChkNameList(subq,Node->body.J.subqs);
	last = last->next = GEN(nodelist);
	last->node = subq;
	last->next = NIL;
      }

      {
	exprlist list = NIL;
	if (CheckKey(Twhere,FALSE) >= 0) {
	  list = GEN(exprlist);
	  list->next = NIL;
	  list->expr = ReadExpr(Node);
	}
	Node->body.J.predlist  = list;
      }
    }
    break;
#else
  case SELECT:
    exprlist list = NIL;
    Node->body.S.subq      = subq = SubQuery();
    if (CheckKey(Twhere,FALSE) >= 0) {
      list = GEN(exprlist);
      list->next = NIL;
      list->expr = ReadExpr(Node);
    }
    Node->body.S.predlist  = list;
    break;
  case JOIN:
    {
      nodelist last = NIL;
      Node->body.J.subqs = NIL;
      do {
        subq = SubQuery();
        ChkNameList(subq,Node->body.J.subqs);
        last = *(last == NIL ? &Node->body.J.subqs : &last->next) = GEN(nodelist);
        last->node = subq;
        last->next = NIL;
      } while ( CheckList() );
      if(Node->body.J.subqs == last) ERROR(ErrSyntax);
    }
    {
      exprlist list = NIL;
      if (CheckKey(Twhere,FALSE) >= 0) {
        list = GEN(exprlist);
        list->next = NIL;
        list->expr = ReadExpr(Node);
      }
      Node->body.J.predlist  = list;
    }
    break;
#endif
  case ORDER: case GROUP: {
    Node->body.O.subq      = subq = SubQuery();
    Node->body.O.reductions= NIL;
    Node->body.O.orderlist = NIL;
    if(CheckKey("BY",mode == ORDER) >= 0) {
      do {
        collist  this  = GEN(collist);
        collist* cscan;
        GetSym();
        this->source        = scan - begscan;
        this->sortproj.proj = ReadProj(Node,NIL,0);/* имя sort столбца*/
        if(this->sortproj.proj->owner != subq) {
          ErrPosition = this->source;
          longjmp(err_jump,ErrColName);
        }

        /* смотрим, что такого столбца еще не было */
        for(cscan= &Node->body.O.orderlist;*cscan;cscan= &(*cscan)->next){
          if ((*cscan)->sortproj.proj == this->sortproj.proj) ERROR(ErrDoubleDef);
        }
        /* наращиваем список */
        *cscan = this;  this->next = NIL;
        this->sortproj.order = (LookKey(orderkeys,FALSE) == 1 ? invOrder : dirOrder);
      } while ( CheckList() );
    }

  } break; case UNION: {
    nodelist last = NIL;
    do {
      static projection pscan,pscan1;
      subq = SubQuery();
      if (last == NIL) {                        /* первый в Union     */
        last = Node->body.U.subqs = GEN(nodelist);
      } else {
        ChkNameList(subq,Node->body.U.subqs);   /* проверим имена Subq*/
        /* проверим на соответствие числа столбцов и их имена */
        for (pscan = last->node->projlist, pscan1=subq->projlist;
             pscan != NIL; pscan = pscan->next, pscan1=pscan1->next) {
          if(pscan1 == NIL) ERROR(ErrColNum);
          if(_cmps(pscan->name,pscan1->name,*pscan->name+1) != 0) {
            ErrPosition = pscan1->expr->source;
            longjmp(err_jump,ErrColName);
          }
        }
        if(pscan1 != NIL) ERROR(ErrColNum);
        last = last->next = GEN(nodelist);
      }
      if(subq->projlist == NIL) ERROR(ErrColNum);
      last->node = subq;
      last->next = NIL;
    } while ( CheckList() );
    if(Node->body.U.subqs == last) ERROR(ErrSyntax);
  } break; default:
    ERROR(ErrSyst);
  }

  Node->projlist      = NIL;
  EQchain->reductLevel = (Node->mode == GROUP);

  if(CheckKey("FOR",FALSE) >= 0) {                /* задана проекция  */
    do {
      int   nameleng = GetName(MaxColName);
      char* nameadr  = scan - nameleng;
      projection pscan = InsProjList(nameadr,nameleng,Node);
      if (GetSym() == ':') {                      /* имя : выражение  */
        ++scan; pscan->expr = ReadExpr(Node);
      } else {                                    /* имя <-> имя:имя  */
        pscan->expr =                             /* ищем как proj    */
          GenProj(ReadProj(Node,nameadr,nameleng),nameadr-begscan);
      }
    } while ( CheckList() );

  } else {                                        /* проекция не ук-на*/
    projection  psubq;                            /* старый  proj     */

    switch(Node->mode) {
    case SELECT:
      subq = Node->body.S.subq;
    break; case ORDER: case GROUP:
      subq = Node->body.O.subq;
    break; case UNION:
      subq = Node->body.U.subqs->node;
    break; default:
      ERROR(ErrSyntax);
    }

    /* копируем проекции из subq */
    forlist(psubq,subq->projlist) {
      char* nameadr  = psubq->name+1;
      int   nameleng = psubq->name[0];
      projection pscan = InsProjList(nameadr,nameleng,Node);
      pscan->expr =                               /* ищем как proj    */
      GenProj(ReadProj(Node,nameadr,nameleng),psubq->expr->source);
    }
  }

  if(Node->projlist == NIL) ERROR(ErrColNum);
  EQchain->reductLevel = 0;

  return(Node);
}

/*
 * Разбор выражения - запроса
 */
static recursive expr ReadEquery(mode) exproper mode; {
  struct eqchain qlist;
  eqref  theRef    = GEN(eqref);

  char*  savescan  = scan;
  expr   result    = GenEXPR;
  node   Node;

  qlist.extnode     = CurrentNode;          /* сцепляем список Equery */
  qlist.extprojs    = NIL;
  qlist.reductLevel = 0;
  qlist.level       = EQchain->level + 1;
  qlist.upper       = EQchain;  EQchain = &qlist;

  Node              = SubQuery();           /* читаем E-запрос        */

  theRef->extprojs  = qlist.extprojs;
  theRef->eqNode    = Node;
  theRef->mode      = mode;

  { eqref* escan = &ParseHeader->eqlist;
    while(*escan != NIL) escan = &(*escan)->next;
    *escan = theRef; theRef->next = NIL;
  }

  result->source     = scan - begscan;
  result->mode       = mode;
  result->body.equery= theRef;


  if(mode == Equery) {
    if(Node->projlist == NIL || Node->projlist->next != NIL) {
      scan = savescan; ERROR(ErrEquery);
    }
  } else if (mode == Eexists) {
    result->type = Tbool;
  }

  /* теперь проверим, нельзя ли его вынести наверх */
/*{ eqchain   eqscan = EQchain;
/*  while(eqscan->extnode != NIL) {
/*    nodelist subqs = GetSubNodes(eqscan->extnode);
/*    prjlist  pscan;
/*    while(subqs != NIL) {
/*      forlist(pscan,qlist.extprojs) if(pscan->proj->owner==subqs->node) break;
/*    if(pscan != NIL) break;
/*      subqs= subqs->next;
/*    }
/*  if(pscan != NIL) break;
/*    eqscan=eqscan->upper;
/*  }

/*  if(eqscan != EQchain) {                   /* будем выносить наверх */
/*    expr     oldRes = result;
/*    exprlist newsubq;
/*    result = GenEXPR;
/*    *result = *oldRes;
/*    result->mode = Eextq;
/*    result->body.extquery.arg      = oldRes;
/*    result->body.extquery.location = Cnil;

/*    /* вставим в список subqlist узла, от которого зависим */
/*    newsubq = GEN(exprlist);
/*    newsubq->expr = result;
/*    newsubq->next = *eqscan->subqlist;
/*    *eqscan->subqlist = newsubq;
/*  }
/*} */

  CurrentNode = qlist.extnode;
  EQchain     = qlist.upper;

  return(result);
}

/*
 * вставка элемента в список проекций
 */
static projection InsProjList(nameadr,nameleng,Node)
                               char* nameadr; int nameleng; node Node; {
  projection* pscan;
  projection  new;
  for(pscan = &Node->projlist; *pscan != NIL; pscan = &(*pscan)->next) {
    if((*pscan)->name[0]==nameleng &&
       _cmps((*pscan)->name+1,nameadr,nameleng)==0) ERROR(ErrDoubleDef);
  }
  *pscan = new  = GEN(projection);
  new->next     = NIL;
  new->location = Cnil;
  new->owner    = Node;
  new->name     = GetMem (nameleng + 1);
  new->name[0]  = (char) nameleng;
  MVS(nameadr,nameleng,new->name + 1);
  return(new);
}

/*
 * Разбор выражения (пускач)
 */
recursive expr ReadExpr(Node) node Node; {
  expr retcode;
  CurrentNode = Node;
  retcode     = ParseExpr(1);
  return(retcode);
}

/*
 *  Разбор выражений
 */
recursive static expr ParseExpr(leftprior) int leftprior; {
  static char* binopers[] =
    {"+", "-", "*", "/", "MOD",
       "=/=", "=", ">=", ">", "<=", "<", "LIKE",
     "AND", "OR", NIL};
  static exproper bincodes[sizeof(binopers)/sizeof(char*)-1]=
    {Eadd, Esub, Emult, Ediv, Emod,
     Ene , Eeq , Ege  , Egt , Ele , Elt , Elike,
     Eand, Eor};

  static char     binprior[sizeof(binopers)/sizeof(char*)  ]=
    { 0 ,
      40 , 40  , 50   , 50  , 50  ,
      30 , 30  , 30   , 30  , 30  , 30  , 30 ,
      20 , 10 };

  static int keynum;
  char*  savescan;

  expr     this, bin;
  this = ParseUnary(leftprior);
  for(;;) {
    GetSym();
    savescan = scan;
  if (binprior[(keynum=LookKey(binopers,FALSE))+ 1] <= leftprior) break;
    bin = GenEXPR;
    bin->mode   = bincodes[keynum];
    bin->source = savescan - begscan;
    bin->body.binary.arg1 = this;
    bin->body.binary.arg2 = ParseExpr((int) binprior[keynum+1]);
    this = bin;
  }
  scan = savescan;
  return(this);
}

/*
 * Поиск столбца проекции в указанном узле
 */
static projection LookProj(nameadr,nameleng,Node)
                             char* nameadr; int nameleng; node Node; {
  projection pscan = Node->projlist;
  typedef struct {char* name; datatype type ; short leng,shift;} fdef;
  fdef*      descr;
  int        ndescr;
  static char  Tname[] = "NAME";
  static char  Ttype[] = "TYPE";
  static char  Tuser[] = "USERID";

  static  fdef columns[] = {
       {Tname ,  Tvar,  MaxColName,   0       },
       {Ttype,   Tchar ,1,            CS_Ftype},
       {"LENGTH",Tshort,sizeof(short),CS_Fleng},
    };

  static  fdef accesses[] = {
       {Tuser,   Tshort,sizeof(short),AS_Fuser     },
       {Tselect, Tchar ,1            ,AS_Fread     },
       {Tinsert, Tchar ,1            ,AS_Fins      },
       {Tdelete, Tchar ,1            ,AS_Fdel      },
       {Tupdate, Tchar ,1            ,AS_Fupd      }
    };

  static  fdef tables [] = {
       {Tname ,     Tvar,  MaxFileName,  sizeof(short)},
       {"TABLENAME",Tvar,  MaxFileName,  0            },
       {Ttype,      Tchar ,1,            TS_Ftype     },
       {"NRECORDS", Tlong ,sizeof(long) ,TS_Fnrec     },
       {"SIZE",     Tlong ,sizeof(long) ,TS_Fsize     },
       {"OWNER",    Tshort,sizeof(short),TS_Fownr     },
    };

  static  fdef users  [] = {
       {Tuser,     Tshort,sizeof(short),US_Fuser     },
       {Tname,     Tchar ,UserName     ,US_Fname     },
       {Tadmin ,   Tchar ,1            ,US_Fadmin    },
       {Tcreate,   Tchar ,1            ,US_Fcreat    },
       {Toper  ,   Tchar ,1            ,US_Foper     },
       {"PASSWORD",Tchar ,PasswLng     ,US_Fpwd      },
    };


  /* ищем, есть ли в списке проекций */
  forlist(pscan,Node->projlist) {
    if(*pscan->name==nameleng &&
       _cmps(pscan->name+1,nameadr,nameleng) == 0) {
      return(pscan);
    }
  }

  /* нет такой projection -> создадим для Table и Special */
  descr = NIL;
  switch (Node->mode) {
  case TABLE:
    switch(Node->body.N.spflag) {              /*спец-запрос к таблице*/
    case 1:
      descr = columns;  ndescr = sizeof(columns)  / sizeof(fdef);
    break; case 2:
      descr = accesses; ndescr = sizeof(accesses) / sizeof(fdef);
    }
  break; case CATALOG:
    switch(Node->body.A.regime) {              /*спец-запрос к каталог*/
    case 0:
      descr = tables; ndescr = sizeof(tables) / sizeof(fdef);
    break; case 3:
      descr = users; ndescr = sizeof(users) / sizeof(fdef) -
                     ((Agent->privil & Priv_Auth) == 0);
    }
  break; default:
    scan=nameadr ; ERROR(ErrUndefined);
  }

  if(descr != NIL) {                          /* имеем спец-"таблицу" */
    for(;;) {                                 /* ищем среди ее описат.*/
      if(--ndescr<0) {scan=nameadr ; ERROR(ErrUndefined);}
    if (_cmps(nameadr,descr->name,nameleng) == 0 &&
                                     descr->name[nameleng] == 0) break;
      ++descr;
    }
  }

  /* генерируем "проекцию - столбец" для таблицы */
  { projection* last = &Node->projlist;
    while(*last != NIL) last = &(*last)->next;
    pscan = GEN(projection);
    *last = pscan; pscan->next = NIL;
    pscan->location= Cnil;
    pscan->owner   = Node;
    pscan->name    = GetMem (nameleng + 1);
    MVS(nameadr,nameleng,pscan->name + 1);
    *pscan->name   = (char) nameleng;
  }

  /* вычисляющее проекцию выражение - "ссылка на колонку" */
  pscan->expr       = GenEXPR;
  pscan->expr->mode = Ecolumn;
  pscan->expr->body.column.shift = 0x77;
  pscan->expr->body.column.specq = 0;
  pscan->expr->source = nameadr - begscan;

  if(descr != NIL) {                          /* колонка уже известна */
    pscan->expr->type  = descr->type;         /* (это спец-запрос)    */
    pscan->expr->leng  = descr->leng;
    pscan->expr->body.column.shift = descr->shift;
    pscan->expr->body.column.specq = 1;
  }
  return(pscan);
}

static nodelist GetSubNodes(theNode) node theNode; {
  static struct nodelist oneList = {NIL,NIL};
  switch(theNode->mode) {
  default      :              ERROR(ErrSyst);
  case JOIN    :              return(theNode->body.J.subqs);
  case INSVALUE: case INSERT: return(NIL);
  case SELECT  : oneList.node = theNode->body.S.subq;
  break; case UNION : oneList.node = theNode->body.U.subqs->node;
  break; case DELETE: case UPDATE: oneList.node = theNode->body.M.subq;
  break; case ORDER : case GROUP : oneList.node = theNode->body.O.subq;
  }
  return(&oneList);
}

/*
 * Чтение терма "проекция"
 */
static projection ReadProj(Node,nameadr,nameleng)
                node Node; int nameleng; char* nameadr; {
  char*    nodename = nameadr;
  int      nodeleng = 0;
  nodelist list     = NIL;
  eqchain   eqscan   = EQchain;
  projection found  = NIL;

  if(nameadr == NIL) {                        /* имя еще не считано   */
    nameleng = GetName(MaxColName);
    nodename = nameadr  = scan - nameleng;
    if( GetSym() == '.') {                    /* имя с уточнением     */
      ++scan;
      nodeleng = nameleng;
      nameleng = GetName(MaxColName);         /* читаем само имя      */
      nameadr  = scan - nameleng;
    }
  }

  for(;;) {
    if(Node == NIL) ERROR(ErrUndefined);
    list    = GetSubNodes(Node);
    if(nodeleng != 0) {                        /* имя таблицы задано?*/
      while(list != NIL &&
            (list->node->name == NIL || list->node->name[0] != nodeleng ||
             _cmps(list->node->name+1,nodename,nodeleng) != 0)) list=list->next;
    } else {
      if(list == NIL || list->next != NIL) ERROR(ErrUndefined);
    }

  if(list != NIL) break;
    Node   = eqscan->extnode;
    eqscan = eqscan->upper;
  }

  found = LookProj(nameadr,nameleng,list->node);

  /* нашли в 1-ом от UNION -> должны найти во всех */
  if(Node->mode == UNION) {
    nodelist uscan;
    forlist(uscan,Node->body.U.subqs->next) {
      LookProj(nameadr,nameleng,uscan->node);
    }
  }

  /* для "GROUP BY" ищем в списке группирования */
  if (eqscan->reductLevel == 1) {
    collist orderlist  = Node->body.O.orderlist;
    if(Node->mode != GROUP) ERROR(ErrSyst);
    for(;;) {
      if (orderlist == NIL) {scan=nameadr; ERROR(ErrGroup);}
    if (orderlist->sortproj.proj == found) break;
      orderlist = orderlist->next;
    }
  }

  /* занесем в списки ссылок на внешние            */
  /* eqscan указывает на элемент цепочки, из чьего */
  /*  CurrentNode мы взяли это проекцию            */
  { eqchain upscan = EQchain;
    while(upscan != eqscan) {
      prjlist pscan;

      /* заносим в его extprojs эту ссылку, если ее там еще нет */
      forlist(pscan,upscan->extprojs) if(pscan->proj == found) break;
      if(pscan == NIL) {
        pscan = GEN(prjlist);
        pscan->proj = found;
        pscan->next = upscan->extprojs; upscan->extprojs = pscan;
      }
      upscan = upscan->upper;
    }
    
  }
  return(found);
}

/*
 * Формирование выражения "ссылка на проекцию"
 */
static expr GenProj(proj,source) projection proj; unsigned source; {
  expr    result = GenEXPR;
  prjlist pscan;
  forlist(pscan,EQchain->extprojs) if(pscan->proj == proj) break;

           /* формируем выражение   */
  result->mode = (pscan == NIL ? Eproj : Eextproj);
/*  (Node->mode == ORDER || Node->mode == GROUP ? Esort : Eproj);
/*if(Node->mode == GROUP) {
/*  /* для GROUP поля группирования -> как Eproj */
/*  collist grouplist;
/*  forlist(grouplist,Node->body.O.orderlist) {
/*    if(grouplist->sortproj.proj == proj) {result->mode = Eproj; break;}
/*  }
/*} */
  result->type            = 0;
  result->leng            = 0;
  result->body.projection = proj;
  result->source          = source;
  return(result);
}

/*
 * Разбор унарного выражения
 */
recursive expr ParseUnary(leftprior) int leftprior; {
  expr   result;
  char*  savescan = (GetSym(), scan);
  int    funccode;
  static char* reducenames[] = {"COUNT", "SUM", "AVG", "MAX","MIN",NIL};
  static char* scalarnames[] =
      {"USER","TIME","GMT","MINUTE","WEEKDAY","YEAR","MONTH","DAY","LENGTH","TOUPPER","TOLOWER",NIL};
  static datatype scalartype[] =
     {Tshort, Tlong, Tlong,Tshort,  Tshort,   Tshort,Tshort, Tshort,Tshort,Tvar,    Tvar};
  static datatype scalarargs [] =
     {Tvoid , Tvoid, Tvoid,Tlong  , Tlong   , Tlong, Tlong , Tlong, Tvar,  Tvar,    Tvar};
  static char*   opernames  [] = { "NOT", "-"  , "+",NIL};
  static opcodes opercodes  [] = { Enot , Esign, 0  };
  static char    operprior  [] = {  25  , 40   , 40 };

  if( *scan == '(') {                         /* "(" выражение ")"    */
    ++scan;
    if (LookKey(getkeys,FALSE)>=0) {          /* это E-запрос         */
      scan   = savescan;
      result = ReadEquery(Equery);
    } else {                                  /* это просто выражение */
      result = ParseExpr(1);
      CheckSym(')');
    }

  } else if (CheckKey("IF",FALSE)>=0) {       /* (IF ....             */
    result = GenEXPR;
    result->mode   = Eif;
    result->source = savescan - begscan;
    result->body.cond.cond = ParseExpr(1);
    CheckKey("THEN",TRUE);                     /* (IF <условие> THEN .*/
    result->body.cond.tvar = ParseExpr(1);
    CheckKey("ELSE",TRUE);                     /* (IF <условие> THEN .*/
    result->body.cond.fvar = ParseExpr(1);

  } else if (CheckKey("EXISTS",FALSE)>=0) {    /* EXISTS (    )       */
    CheckSym('('); --scan;
    result = ReadEquery(Eexists);

  } else if ( (funccode = LookKey(opernames,FALSE)) >=0 ) {
                                              /* унарные операции     */
    if(operprior[funccode] <= leftprior) {
      scan = savescan; ERROR(ErrSyntax);
    }

    if(opercodes[funccode] != 0) {            /* унарный плюс убираем */
      result = GenEXPR;
      result->mode   = opercodes[funccode];
      result->type   = 0;
      result->leng   = 0;
      result->source = savescan - begscan;
      result->body.unary = ParseExpr(operprior[funccode]);
    } else {                                  /* унарный плюс         */
      result = ParseUnary(operprior[funccode]);
    }

  } else if ( IsConst(*scan) ) {              /* константа            */
    ReadConst();
    result= GenConstant((consttype == Tchar ? savescan+1 : constbuffer),
                           constlength, consttype, savescan);

  } else if (CheckKey("EMPTY",FALSE)>=0) {    /* EMPTY - longdata     */
    ldataDescr emptyLdata;
    emptyLdata.size = 0;
    emptyLdata.table= NoFile;
    emptyLdata.addr = 0;
    result = GenConstant((char*)&emptyLdata,sizeof(emptyLdata),
                         Tldata,savescan);

  } else if ( *scan == '$' ) {                /* параметр запроса     */
    int         nameleng;
    dsc_column* paramadr;
    ++scan; nameleng = GetName(MaxColName);
    if( (paramadr = LookField(scan - nameleng, nameleng)) == NIL) {
      scan -= nameleng; ERROR(ErrUndefined);  /* параметр не найден   */
    }
    result = GenEXPR;
    result->mode   = Eparam;
    result->type   = paramadr->type;
    result->leng   = paramadr->length;
    result->source = scan-nameleng - begscan;
    result->body.parameter = paramadr;

  } else if ( (funccode = LookKey(typewords,FALSE)) >=0 ) {
    CheckSym( '(' );                         /* преобразование типа   */
    result = GenEXPR;
    result->mode   = Econv;
    result->source = savescan - begscan;
    result->body.unary  = ParseExpr(1);
    if( (result->type   = types [funccode])>= Tchar) {
      CheckSym( ',' );
      if((result->leng = GetNum()) >= MaxString) ERROR(ErrLeng);
    } else {
      result->leng   = lengths[funccode];
    }
    CheckSym( ')' );

  } else if ( (funccode = LookKey(scalarnames,FALSE)) >=0 ) {
    expr arg = NIL;
    CheckSym( '(' );
    result = GenEXPR;
    result->mode   = Efunc;
    result->type   = scalartype[funccode];
    result->leng   = result->type == Tvar ? 0 : lengths[result->type-Tshort];
    result->source = savescan - begscan;
    result->body.scalar.func = (scalarfunc) funccode;
    if((result->body.scalar.type = scalarargs[funccode]) != Tvoid) {
      if(result->body.scalar.type != Tvar) {
        arg = GenEXPR;
        arg->mode   = Econv;
        arg->type   = result->body.scalar.type;
        arg->leng   = lengths[result->body.scalar.type-Tshort];
        arg->source = scan - begscan;
        arg->body.unary  = ParseExpr(1);
      } else {
        arg = ParseExpr(1);
      }
    }
    result->body.scalar.arg = arg;
    CheckSym( ')' );

  } else if ( (funccode = LookKey(reducenames,FALSE)) >=0 ) {
    reduction theRed;
    if(EQchain->reductLevel != 1) {             /* редукция недопустима */
      scan = savescan; ERROR(ErrReduction);
    }
    theRed = GEN(reduction);
    theRed->next = CurrentNode->body.O.reductions;
    CurrentNode->body.O.reductions  = theRed;
    theRed->arg1     = theRed->arg2 = NIL;
    theRed->location = Cnil;
    theRed->source   = savescan - begscan;

    CheckSym( '(');                           /* функция редукции     */
    if(funccode == Rcount) {
      CheckSym( '*');                         /* COUNT(*)             */
    } else {
      ++EQchain->reductLevel;
      theRed->arg1 = ParseExpr(1);            /* читаем ее аргумент   */
      if((funccode == Rmax || funccode == Rmin) && CheckList()) {
        theRed->arg2 = ParseExpr(2);
        funccode += (Rmin2-Rmin);
      }
      --EQchain->reductLevel;
    }
    CheckSym( ')');
    theRed->func = (reducefunc) funccode;

    result = GenEXPR;
    result->mode   = Eredref;
    result->type   = Tvoid;
    result->leng   = 0;
    result->body.reduction = theRed;
    result->source = theRed->source;

  } else {                                    /* имя проекции         */
    result = GenProj(ReadProj(CurrentNode,NIL,0),savescan - begscan);
  }
  return(result);
}

/*
 * Чтение константы
 */
static void ReadConst() {
  char* savescan = scan;
  if ( IsDigit(*scan) || *scan == '.') {
    char floatbuf[MaxDigit];                  /* число                */
    int  power = -1;
    int  index =  0;
    int  point =  0;
    char symbol;
    for (;;) {                                /* читаем 1234.5678     */
      while( (symbol = *scan) >= '0' && symbol <= '9') {
        if (index != 0 || symbol != '0') {
          if(index<MaxDigit) floatbuf[index] = symbol;
          ++index;
        } else if (point) {
          --power;
        }
        ++scan;
      }
    if (symbol != '.') break;                 /* точка внутри числа   */
      if( point !=0 ) ERROR(ErrSyntax);
      power = index; point = 1;
      ++scan;
    }
    if (! point) power = index;               /* точки не было        */
    switch (symbol) {
      int sign;
    case 'E': case 'D': case 'e': case 'd':   /* 1234.45Е+34          */
      ++scan; point = sign = 1;
      switch( GetSym() ) {
      case '-': sign = -1;                    /* ...Е-..              */
      case '+': ++scan;                       /* ...Е+..              */
      }
      power += GetNum() * sign;               /* ...Е+23              */
    }
    if (point) {                              /* была точка -> float  */
#     if defined(MISS) || defined(macintosh)
        while (index < MaxDigit) floatbuf[index++] = '0';
        if(power < -MaxExponent || power > MaxExponent) ERROR(ErrNum);
        *(real*) constbuffer = _stfl(floatbuf,power);
#     else
        char* numend;
        char  saved = *scan;
        *scan = 0;
        *(real*) constbuffer = strtod(savescan,&numend);
        *scan = saved;
        if((index!=0 && *(real*)constbuffer==0.0)) ERROR(ErrNum);
#     endif
      consttype = Tfloat; constlength = sizeof(real);
    } else {                                  /* без точки -> long    */
      long  data = 0;
      int   i;
      for (i=0; i<index; i++) {
        if ( data >= 0x7FFFFFFF/10+1) ERROR(ErrNum);
        data = data * 10 + (floatbuf[i]-'0'); /* преобразовываем число*/
        if ( data<0) ERROR(ErrNum);
      }
      if (data >= (unsigned) 0x8000) {        /* это, возможно, short */
        *(long*) constbuffer = data;
        consttype = Tlong ; constlength = sizeof(long);
      } else {                                /* нет, все-таки  long  */
        *(short*) constbuffer = (short) data;
        consttype = Tshort; constlength = sizeof(short);
      }
    }

  } else if (*scan == '\'' || *scan == '"') { /* константная строка   */
    char  begsym  = *scan;

    savescan = ++scan;
    while (*scan != begsym) {                 /* ищем конец строки    */
      if(*(++scan) == 0) ERROR(ErrString);
    }
    constlength = scan - savescan;            /* длина строки         */
    if( constlength >= MaxString) ERROR(ErrString);
    consttype = Tchar;
    ++scan;                                   /* считали концевую "   */
  } else {
    ERROR(ErrSyntax);
  }
}

static expr GenConstant(char* value, int length, datatype type, char* source) {
  expr  result;
  char* body;
  if(type == Tchar) {                      /* текстовая константа     */
    body = GetMem(length+ sizeof(short));  /* перед ней слово с длиной*/
    *(short*) body = length; body += sizeof(short);
    MVS(value,length,body);                /* переносим тело          */
  } else {
    body = GetMem(length);                 /* для числовых констант   */
    MVS(value,length,body);                /* просто переносим тело   */
  }

  result = GenEXPR;
  result->mode            = Econst;
  result->type            = type;
  result->leng            = length;
  result->body.constvalue = body;
  result->source          = source - begscan;
  return(result);
}

static node GenNode() {
  node result = GEN(node);
  result->projlist = NIL;
  result->name     = NIL;
  result->eqLevel  = EQchain->level;
  result->pathSize = 0;
  return(result);
}

static void ERROR(code) int code; {
  ErrPosition = scan - begscan;
  longjmp(err_jump,code);
}
