

+   %% = = = = = = = = = = = = = = = = = = = = = = = = = %%
+   %%                                                   %%
+   %%   MULTIPURPOSE INTERACTIVE TIME SHARING SYSTEM    %%
+   %%                                                   %%
+   %%       MultiAccess Relational data System          %%
+   %%       Интерфейс с прикладными программами         %%
+   %%                                                   %%
+   %% = = = = = = = = = = = = = = = = = = = = = = = = = %%


+                 I. Принципы взаимодействия

     Система  управления   базами   данных   "MARS"   является
 отдельным системным  процессом, обменивающимся  информацией с
 другими   процессами   системы   (как   локальными,   так   и
 выполняющимися на других машинах).
     Обмен информацией  с каждым  отдельным процессом построен
 по принципу "диалога": процесс обращается к СУБД с запросом и
 ожидает ответного  сообщения. Таким образом, процесс не может
 выдать запрос,  на который не будет ответа, и не может прийти
 сообщение от СУБД без явного запроса от процесса.
     Для    удобства    использования    СУБД    рекомендуется
 использовать  специальный   набор  подпрограмм,   формирующих
 сообщения-запросы к СУБД и преобразующих полученные ответы.

\
+            II. Интерфейс для программ на языке C

     Использование  СУБД  в  программах  на  языке  C  требует
 включения в текст системного include-файла "DBdescr":

 #include <DBdescr>
^          ^^^^^^^

     Для редактора связей при помощи файла "LINKLIBRS" должна
 быть сделана доступной системная библиотека "!MARS".
^                                             ^^^^^^

     Для  всех   приведенных  ниже   функций   типа   "short"
 отрицательное значение  указывает на обнаружение ошибки. Код
 ошибки   помещается    в   поле    "DBinfo.errCode".    Поле
 "DBinfo.errPosition" содержит  при этом  -1, кроме  случаев,
 оговоренных ниже.  Соответствующие  кодам  ошибок  сообщения
 записаны в  файле "@TEXTS.MARS".  Мнемонические имена ошибок
 описаны в include-файле <DBerrors>.

     Поле DBinfo.nrecords содержит число вставленных, стертых
 или   модифицированных  записей  после  выполнения  операций
 INSERT, DELETE или UPDATE.

 extern struct DBinfo {
^             ^^^^^^^^^
  short errCode;
^       ^^^^^^^
  short errPosition;
^       ^^^^^^^^^^^
  long  nrecords;
^       ^^^^^^^^
 } DBinfo;

 \   Доступны следующие функции:

  1. Установление связи с СУБД

  short DBconnect()
^       ^^^^^^^^^
    Функция используется для начала сеанса работы с СУБД.

 \2. Отключение связи с СУБД;

  void  DBdisconn()
^       ^^^^^^^^^
    Функция  должна использоваться для окончания сеанса работы
 с СУБД; ею освобождается канал связи и "агент" в СУБД.


 \3. Обработка текста запроса

  short DBcompile(text,length,descr,descrsize)
^       ^^^^^^^^^
      char* text; short length;
      DBdescriptor descr; short descrsize;

    Функции  передается  текст  запроса,  адрес  его  задается
 параметром  "text",  длина  -  параметром  length. Если адрес
 текста   не  задан  (значение  NIL  (0)),  то  текст  запроса
 считается  помещенным  в  буфер  "DBexecBuf".  Если  длина не
 задана (значение 0), то используется длина до символа 0x00.
     Переменная   "descr"   задает   адрес   области   памяти,
 используемый   для   хранения   описателя  оттранслированного
 запроса, "descrsize" - размер этой области.
     Если  значение  функции == 0, то это означает, что запрос
 был интерпретируемым и уже успешно выполнен СУБД.
     Если значение  функции > 0, то это значит, что запрос был
 запросом   на    манипуляцию   данными;    он   был   успешно
 оттранслирован СУБД, описатель запроса помещен в "descr". Код
 запроса сохраняется  в СУБД  до конца  транзакции или  до его
 явного уничтожения  вызовом "DBkill"  и может  использоваться
 многократно.
     При  обнаружении ошибки (значение функции < 0) переменная
 "DBerrPosition"  может  содержать  неотрицательное  значение,
 указывающее   на   позицию  ошибочной  конструкции  в  тексте
 запроса.

 \4. Выполнение запроса (EXEC)

 short DBexec(descr,parm1,parm2,...) DBdescriptor descr;
^      ^^^^^^

     После успешной  компиляции запроса  его можно  выполнить,
 т.е.   произвести    заказанную   модификацию    данных   или
 инициализировать выборку  данных. Функции  указывается  адрес
 соответствующего  дескриптора   ("descr")  и  соответствующее
 количество значений  параметров запроса. Типы значений должны
 строго соответствовать  типам параметров  запроса.  Параметры
 типа VARCHAR(n)  задаются как  строки,  ограниченные  нулевым
 байтом.


 \5. Выборка записей (FETCH)

 short DBfetch(descr,&res1,&res2...) DBdescriptor descr;
^      ^^^^^^^

     Функция  заносит  значения полей результирующей таблицы в
 переменные-приемники.    Типы    переменных   должны   строго
 соответствовать   типам   столбцов.  Данные  типа  VARCHAR(n)
 заносятся   с   ограничивающим  нулевым  байтом  (т.e.  длина
 переменной-приемника должна быть не меньше n+1).

     Нулевое   значение   функции  означает  достижение  конца
 результирующей таблицы.


 \6. Освобождение кода

 short DBkill(descr) DBdescriptor descr;
^      ^^^^^^

     Функция уничтожает  оттранслированный и хранящийся в СУБД
 код запроса,  соответствующий дескриптору  "descr".  Заметим,
 что все  коды уничтожаются  при выполнении  операции COMMIT и
 операции ROLLBACK  (в том  числе и  неявной, выполненной СУБД
 из-за обнаруженной ошибки).


 \7. Окончание транзакции.

 void DBcommit(flag) int flag;
^     ^^^^^^^^

     Функция  выполняет операцию "COMMIT", если "flag" != 0, и
 операцию "ROLLBACK", если "flag" == 0.

 \8. Обработка длительных операций

 typedef short (*DBtrap)(short);
^                ^^^^^^
 DBtrap DBwait(proc) DBtrap proc;
^       ^^^^^^
     Если   операция   выполняется  долго  (дольше  интервала
 времени,  заданного  в  СУБД,  обычно - 15 сек.), то система
 отвечает    "фиктивным   ответом",   сообщая,   что   запрос
 выполняется слишком долго и информируя о статусе транзакции:
 готова  к продолжению или заблокирована. Интерфейсный модуль
 автоматически выдает в ответ команду "продолжай выполнение",
 а  если  на  терминале  был  нажат символ <HOME>, то команду
 "прерви  выполнение". В последнем случае выполнение операции
 завершается   и   программе  пользователя  возвращается  код
 "операция прервана".
     Можно  установить свой обработчик подобных ситуаций. Для
 этого  необходимо написать функцию типа DBtrap (т.е. имеющую
 параметр типа  short  и  возвращающую  результат типа short)
 и передать ее в качестве параметра процедуре DBwait.
     Подобная    функция    может   быть   использована   для
 информирования пользователя о состоянии транзакции.
     При выполнении  последующих "долгих"  операций указанная
 функция будет  вызываться со  значением  параметра  1,  если
 транзакция заблокирована, или со значением параметра 0, если
 транзакция  готова  к  продолжению  работы.  Функция  должна
 вернуть значение  0, если транзакция должна быть прервана, и
 ненулевое значение  для того,  чтобы  продолжить  выполнение
 операции.
     По окончании  выполнения операции  функция вызывается со
 значением  параметра   -1,  если  она  вызывалась  во  время
 выполнения  данной операции. Это используется, например, для
 очистки экрана, т.е. стирания сообщения о блокировке и т.п.

 \9. Пример программы

 /*
  * Пример программы, использующей СУБД MARS
  */
 #include <DBdescr>
 #include <stdio.h>

     void ErrMessage();       /* выдача сообщений об ошибках*/

 main() {

                        /* тексты запросов */

   /* запрос на идентификацию */
   static char LogonReq[] = {
         "LOGON USER  PASSWORD",};

   /* запрос на создание таблицы */
   static char CreateReq[] = {"Create table test ",
        "(a:short,b:long,c:float,d:char(10),e:varchar(20))",};

   /* запрос на вставку записей (параметризованный) */
   static char InsertReq[] = {
        "(a:short,b:long,c:float,d:char(10),e:varchar(20))",
        "Insert values $a,$b,$c,$d,$e into test",};

   /* запрос на выборку записей */
   static char SelectReq[] = {
        "Select test for a,b,c,d,e",};

   /* запрос на удаление части записей */
   static char DeleteReq[] = {
        "Delete test where a<50 or e='leng 6'",};

   /* дескрипторы кодов запросов */
   char         DescrBuf1[200];
   char         DescrBuf2[200];
   char         DescrBuf3[100];
   DBdescriptor descr1 = (DBdescriptor) DescrBuf1;
   DBdescriptor descr2 = (DBdescriptor) DescrBuf2;
   DBdescriptor descr3 = (DBdescriptor) DescrBuf3;

   short ret,count;
   short     ar;
   long      br;
   float     cr;
   char      dr[10],er[21];

   /* установление связи с СУБД */
   printf("Connecting to Data Base ...\n");

   if (DBconnect() < 0) ErrMessage();

   /* выполнение интерпретируемых запросов */

   if (DBcompile(LogonReq, sizeof(LogonReq) ,(char*) 0, 0) <0)
        ErrMessage();

   if (DBcompile(CreateReq,sizeof(CreateReq),(char*) 0, 0) <0)
        ErrMessage();

   /* компиляция запросов на манипуляцию данными */
   printf("Requests compiling ...\n");

   if (DBcompile(InsertReq,sizeof(InsertReq),
                   descr1,sizeof(DescrBuf1)) <0) ErrMessage();
   if (DBcompile(SelectReq,sizeof(SelectReq),
                   descr2,sizeof(DescrBuf2)) <0) ErrMessage();
   if (DBcompile(DeleteReq,sizeof(DeleteReq),
                   descr3,sizeof(DescrBuf3)) <0) ErrMessage();

   /* вставка тестовой информации в созданную таблицу */
   printf("Records inserting ...\n");

   for (count=1;count<100; ++count) {
     static char* vars[3] = {"length 9","length 10","leng 6"};

     if(DBexec(descr1,count,(long)count+1000000,count/1000.0,
            "testline10",vars[count%3])<0) ErrMessage();
   }
   DBkill(descr1);                     /* этот код не нужен */

   /* выборка данных из базы данных */
   printf("Reading results ...\n");

   if(DBexec(descr2)<0) ErrMessage();  /* инициация сканера */

   while ( (ret=DBfetch(descr2,&ar,&br,&cr,&dr,&er)) !=0 ) {
     if (ret<0) ErrMessage();
     printf("data: a=%3d, b=%8ld, c=%8.4g, d=%.10s, e='%s'\n",
             ar,br,cr,dr,er);
   }

   /* стирание части записей */
   printf("Deleting ...\n");
   if(DBexec(descr3)<0) ErrMessage();

   /* вновь выборка данных (по тому же запросу) */
   printf("Reading results ...\n");

   if(DBexec(descr2)<0) ErrMessage();

   while ( (ret=DBfetch(descr2,&ar,&br,&cr,&dr,&er)) !=0 ) {
     if (ret<0) ErrMessage();
     printf("data: a=%3d, b=%8ld, c=%8.4g, d=%.10s, e='%s'\n",
             ar,br,cr,dr,er);
   }

   printf("Disconnecting ...\n");

   DBcommit(0);                 /* откат нашей транзакции   */
   DBdisconn();                 /* отключение от базы данных*/
 }

 /*
  * Выдача сообщения об ошибке
  */
 void ErrMessage() {
   printf("Data Base error %d",DBinfo.errCode);
   DBdisconn();                /* отключение от СУБД        */
   exit(1);                    /* ROLLBACK сделает сама     */
 }

 \10. Динамические запросы

     При использовании  запросов, тексты  которых  динамически
 формируются  программой,  бывает  невозможно  заранее  узнать
 количество и  типы параметров  запроса, а  также количество и
 типы столбцов  результирующей таблицы.  Для  этого  программа
 должна проанализировать  полученный дескриптор  кода  и  сама
 построить  запись   со   значениями   параметров   в   буфере
 "DBexecBuf",  а  также  должна  сама  "вытаскивать"  значения
 полей-атрибутов  из  результирующей  записи,  возвращаемой  в
 буфере "DBbuffer".

     extern  char* DBexecBuf;
^                  ^^^^^^^^^
     extern  char  DBbuffer[];
^                  ^^^^^^^^

 \   Структуры  записи  с  параметрами  запроса  и  записи  со
 строкой результирующей таблицы совпадают. Запись состоит из 5
 частей:

     +-------------------------------------+
     | список смещений  концов полей типа  |  одно short-число
     | VARCHAR относительно данного слова; |   на каждое
     | 0-ое слово содержит длину записи    |    VARCHAR-данное
     |  (адрес конца последнего VARCHAR)   |
     +-------------------------------------+
     |   смещение  начала первого VARCHAR  |  одно short-слово
     |   (длина всей записи, если их нет)  |
     +-------------------------------------+
     |       данные числовых типов         |
     |    (выравнены на short-границу)     |
     +-------------------------------------+
     |         данные типа CHAR            |
     |  (не выравнены на short-границу)    |
     +-------------------------------------+
     |         данные типа VARCHAR         |
     |  (не выравнены на short-границу)    |
     |    порядок в записи соответствует   |
     | порядку указания в списке параметров|
     |        или в списке проекций        |
     +-------------------------------------+

 \   Конкретная структура определяется дескриптором кода:

 /* типы данных */
 enum {DBshort = 1, DBlong, DBfloat, DBchar, DBvarchar, DBlongdata};
^      ^^^^^^^      ^^^^^^  ^^^^^^^  ^^^^^^  ^^^^^^^^^  ^^^^^^^^^^

 typedef struct {           /* описатель колонки/параметра  */
   short type;              /* тип данного (DBshort и т.п.) */
   short length;            /* длина данного в байтах, для
                               DBvarchar - максимальная     */
   short shift;             /* смещение данного в записи, для
                               DBvarchar - смещение слова с
                               указателем на конец данного  */
 } DBelement;
^  ^^^^^^^^^

 typedef struct {             /* описатель кода               */
   short descrLength;         /* полная длина дескриптора     */
   short identifier;          /* идентификатор кода в базе    */
   short paramNumber;         /* число параметров кода        */
   short fixparmleng;         /* длина фикс. пар-ов (без var) */
   short nVars;               /* число параметров типа VARCHAR*/
   short columnNumber;        /* число выходных столбцов      */
   DBelement elements[]       /* описатели парам-ов, за ними- */
                              /* описатели выходных столбцов  */
 }* DBdescriptor;
^   ^^^^^^^^^^^^

 typedef struct {
   char    int1,int2;
   unsigned short int3;
   long    length;
 } DBlongdataItem;
^  ^^^^^^^^^^^^^^

 \

     Для   работы   с   динамическими  запросами  используются
 следующие функции:

 short DBlexec(descr) DBdescriptor* descr;
^      ^^^^^^^

     Аналогично "DBexec", но параметры запроса должны быть уже
 занесены в "DBexecBuf".


 short DBlfetch(descr) DBdescriptor* descr;
^      ^^^^^^^^

     Аналогично  "DBfetch",  но  полученная  запись со строкой
 результирующей таблицы просто остается в "DBbuffer".

\
+       III. Интерфейс для программ на языке FORTRAN-77

     Использование  СУБД в программах на языке FORTRAN требует
 включения в текст системного include-файла "DBDESCR":

 PRAGMA INCLUDE '!DBDESCR'
^                ^^^^^^^^

     Для  редактора связей при помощи файла "LINKLIBRS" должна
 быть сделана доступной системная библиотека "!MARS".
^                                             ^^^^^^

     Дополнительная    информация    о   результатах   запроса
содержится в COMMON-блоке:

    COMMON /DBINFO/DBERRC,DBERRP,DBNRC0,DBNRC1
^           ^^^^^^
    INTEGER  DBERRC                %код ошибки, выданный СУБД
^           ^^^^^^^
    INTEGER  DBERRP                %позиция ошибки в тексте
^           ^^^^^^^
    INTEGER  DBNRC0,DBNRC1         %число модифицированных
^           ^^^^^^^ ^^^^^^

     Переменная  DBNRC1  содержит  младшие  16  разрядов число
 модифицированных  записей,  переменная  DBNRC0  -  старшие 16
 разрядов.

     В  связи  с  ограничениями,  имеющимися  в  языке FORTRAN
 (отсутствие  динамической  памяти  и  т.п.),  на  запросы  из
 FORTRAN-программ накладываются следующие ограничения:
     1.  параметры  запроса и выходные столбцы не должны иметь
 тип LONG или VARCHAR; тип CHAR допустим, если длина строки не
 более 200 символов;
     2.  динамические запросы не поддерживаются.

 \   Доступны следующие функции:

  1. Установление связи с СУБД

  INTEGER FUNCTION DBCONN()
^                  ^^^^^^
    Функция используется для начала сеанса работы с СУБД.

 \2. Отключение связи с СУБД;

  INTEGER FUNCTION DBDISC()
^                  ^^^^^^
    Функция  должна использоваться для окончания сеанса работы
 с СУБД; ею освобождается канал связи и "агент" в СУБД.


 \3. Обработка текста запроса

  INTEGER FUNCTION FDBTRN(TEXT,DESCR)
  CHARACTER*(*) TEXT       или   DIMENSION TEXT(*)
  DIMENSION     DESCR(*)

    Функции передается текст запроса в виде CHARACTER-значения
 или в  виде массива  произвольного  типа.  Параметр  DESCR  -
 массив  произвольного   типа,   используемый   для   хранения
 описателя оттранслированного запроса.
     Если  значение  функции == 0, то это означает, что запрос
 был интерпретируемым и уже успешно выполнен СУБД.
     Если значение  функции > 0, то это значит, что запрос был
 запросом   на    манипуляцию   данными;    он   был   успешно
 оттранслирован СУБД, описатель запроса помещен в "DESCR". Код
 запроса сохраняется  в СУБД  до конца  транзакции или  до его
 явного уничтожения  вызовом "FDBFIL"  и может  использоваться
 многократно.
     При обнаружении  ошибки (значение функции < 0) переменная
 "DBERRP"    в    COMMON-блоке    DBINFO    может   содержать
 неотрицательное значение,  указывающее на  позицию  ошибочной
 конструкции в тексте запроса.

 \4. Выполнение запроса (EXEC)

  INTEGER FUNCTION FDBEXE(DESCR,PARM1,PARM2,...,PARMn)
^                  ^^^^^^
  DIMENSION DESCR(*)

     После успешной  компиляции запроса  его можно  выполнить,
 т.е.   произвести    заказанную   модификацию    данных   или
 инициализировать выборку  данных. Функции  указывается  адрес
 соответствующего  дескриптора   ("DESCR")  и  соответствующее
 количество значений  параметров запроса. Типы значений должны
 строго  соответствовать  типам  параметров запроса. Параметры
 могут быть выражениями, переменными или элементами массивов.


 \5. Выборка записей (FETCH)

  INTEGER FUNCTION FDBFET(DESCR,PARM1,PARM2,...,PARMn)
^                  ^^^^^^
  DIMENSION DESCR(*)

     Функция заносит  значения полей  результирующей таблицы в
 переменные-приемники. Их  типы должны  строго соответствовать
 типам  столбцов.   Параметры  могут   быть  переменными   или
 элементами массивов.

     Нулевое   значение   функции  означает  достижение  конца
 результирующей таблицы.


 \6. Освобождение кода

  INTEGER FUNCTION FDBKIL(DESCR)
^                  ^^^^^^
  DIMENSION DESCR(*)

     Функция уничтожает  оттранслированный и хранящийся в СУБД
 код запроса,  соответствующий дескриптору  "DESCR".  Заметим,
 что все  коды уничтожаются  при выполнении  операции COMMIT и
 операции ROLLBACK  (в том  числе и  неявной, выполненной СУБД
 из-за обнаруженной ошибки).


 \7. Окончание транзакции.

 SUBROUTINE FDBCOM(FLAG)
^           ^^^^^^
 LOGICAL FLAG

     Функция  выполняет   операцию  "COMMIT",  если  "FLAG"  -
 .TRUE., и операцию "ROLLBACK", если "FLAG" - .FALSE.

 \8. Обработка "долгих" транцакций

  SUBROUTINE FDBWAI(MYFUNC)
  EXTERNAL MYFUNC
  LOGICAL  MYFUNC
     Для  обработки  "долгих"  операций можно написать функцию
 типа  LOGICAL  с  параметром  типа  INTEGER  и  передать ее в
 качестве параметра процедуре FDBWAI.
     Указанная  функция будет вызываться периодически во время
 выполнения  долгих  операций. Параметр функции имеет значение
 1, если транзакция заблокирована, и 0, если транзакция готова
 к продолжению работы.
     Функция   должна   возвращать   значение   .TRUE.,   если
 транзакция  должна  быть продолжена, и значение .FALSE., если
 транзакция должна быть прервана.
     Если  функция  вызывалась во время выполнения транзакции
 хотя  бы  раз,  то  по  окончании операции она вызывается со
 значением  параметра  -1.  Это  может  быть использовано для
 стирания с экрана сообщений о блокировке и т.п.

 \9. Пример программы
 C
 C Пример программы, использующей СУБД MARS
 C
       PRAGMA INCLUDE '!DBDESCR'
 C
 C Тексты запросов
 C
       CHARACTER*(*) LOGON,CREATE,INSERT,SELECT,DELETE
 C запрос на идентификацию
       PARAMETER (LOGON ='LOGON USER PASSWORD')
 C запрос на создание таблицы
       PARAMETER (CREATE=
      _      'Create table test (a:short,c:float,d:char(10))')
 C запрос на вставку записей (параметризованный)
       PARAMETER (INSERT=        '(a:short,c:float,d:char(10))
      _Insert values a=$a,c=$c,d=$d into test')
 C запрос на выборку записей
       PARAMETER (SELECT='Select test for a,c,d')
 C запрос на удаление части записей */
       PARAMETER (DELETE='Delete test where a<50 or d<''L''')
 C
 C Обработчик ожиданий
 C
       LOGICAL  MYWAIT
       EXTERNAL MYWAIT

 C
 C Дескрипторы кодов запросов
 C
       CHARACTER DESCR1(200),DESCR2(200),DESCR3(200)
       INTEGER RET,COUNT,AR
       REAL    CR
       CHARACTER*10 TEXT(0:2),DR
       DATA TEXT/'ANNA      ','HELEN     ','SALLY     '/
 C
 C Установление связи с СУБД
 C
       PRINT *,'Connecting to Data Base ...'
       CALL DBCONN

 C
 C Установ обработчика
 C
       CALL FDBWAI(MYWAIT)

 C
 C Выполнение интерпретируемых запросов */
 C
       IF(FDBTRN(LOGON ,DESCR1).LT.0) CALL ERRMES
       IF(FDBTRN(CREATE,DESCR1).LT.0) CALL ERRMES
 C
 C Компиляция запросов на манипуляцию данными
 C
       PRINT *,'Requests compiling ...'
       IF(FDBTRN(INSERT,DESCR1).LT.0) CALL ERRMES
       IF(FDBTRN(SELECT,DESCR2).LT.0) CALL ERRMES
       IF(FDBTRN(DELETE,DESCR3).LT.0) CALL ERRMES
 C
 C Вставка тестовой информации в созданную таблицу */
 C
       PRINT *,'Records inserting ...'
       DO FOR COUNT=1,100
         IF(FDBEXE(DESCR1,COUNT,COUNT/1000.0,TEXT(MOD(COUNT,3)))
      _     .LT.0) CALL ERRMES
       END DO
 C этот код более не нужен
       CALL FDBKIL(DESCR1)
 C
 C Выборка данных из базы данных
 C
       PRINT *,'Reading results ...'
 C инициация сканера
       IF(FDBEXE(DESCR2).LT.0) CALL ERRMES
       DO WHILE .TRUE.
         RET = FDBFET(DESCR2,AR,CR,DR)
         IF (RET.LT.0) CALL ERRMES
       WHILE RET.NE.0
         PRINT 100,AR,CR,DR
 100     FORMAT(' Data: A=',I3,', C=',G11.4,', D=',A)
       END DO
 C
 C Стирание части записей */
 C
       PRINT *,'Deleting ...'
       IF(FDBEXE(DESCR3).LT.0) CALL ERRMES
       PRINT *,DBNRC1,' records deleted'
 C
 C Вновь выборка данных (по тому же запросу)
 C
       PRINT *,'Reading results ...'
 C инициация сканера
       IF(FDBEXE(DESCR2).LT.0) CALL ERRMES
       DO WHILE .TRUE.
         RET = FDBFET(DESCR2,AR,CR,DR)
         IF (RET.LT.0) CALL ERRMES
       WHILE RET.NE.0
         PRINT 100,AR,CR,DR
       END DO
 C
 C Отсоединение от базы данных
 C
       PRINT *,'Disconnecting ...'
 C откат нашей транзакции
       CALL FDBCOM(.FALSE.)
 C отключение от базы данных
       CALL DBDISC
       END
 C
 C Обработка ожиданий
 C
       LOGICAL FUNCTION MYWAIT(FLAG)
       PRAGMA INCLUDE '!DSP:CONTR'
       INTEGER FLAG
       IF(FLAG.EQ.1) THEN
         PRINT *,'Transaction is locked. Wait ?'
       ELSE IF(FLAG.EQ.0)
         PRINT *,'Transaction is running. Wait ?'
       ENDIF
       MYWAIT = DPM().NE.HO
       END
 C
 C Выдача сообщения об ошибке
 C
       SUBROUTINE ERRMES
       PRAGMA INCLUDE '!DBDESCR'
       PRINT *,'Data Base error ',DBERRC,' position',DBERRP
 C отключение от СУБД
       CALL DBDISC
 C ROLLBACK сделает сама
       STOP 1
       END