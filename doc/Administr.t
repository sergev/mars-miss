

+   %% = = = = = = = = = = = = = = = = = = = = = = = = = %%
+   %%                                                   %%
+   %%   MULTIPURPOSE INTERACTIVE TIME SHARING SYSTEM    %%
+   %%                                                   %%
+   %%       MultiAccess Relational data System          %%
+   %%           Руководство администратора              %%
+   %%                                                   %%
+   %% = = = = = = = = = = = = = = = = = = = = = = = = = %%



+               I. Включение в Систему

     Для включения Подсистемы "MARS" в Систему необходимо:

     В    файле    конфигурации   описать   сегмент   данных,
 используемый в качестве CASH-памяти. Имя сегмента - DB:
 +COMMON DATA=DB(&FC00,S)

     Необходимо также описать каналы доступа к Подсистеме:

 Случай 1. Локальная Подсистема

 * PIPE01 * PIPE       * DEVICE=&01        % или любой другой
 * DBMAIN * LINE:DRIVE * CHANNEL=PIPE01.LX ; CLASS="S"
 * DBMULT * MULTI:DIAL * CHANNEL=PIPE01.LY ; NLINES=n
 * DBASE0 * LINE:DIAL  * CHANNEL=DBMULT.L0 ; CLASS="D"
 * DBASE1 * LINE:DIAL  * CHANNEL=DBMULT.L1 ; CLASS="D"
     . . . . . . . . . . . . . . . . . . . . . . . .
 * DBASEn * LINE:DIAL  * CHANNEL=DBMULT.Ln ; CLASS="D"

 Общий случай рассмотрим для конфигурации из 4 машин:
 +-------+     +---------+     +---------+     +--------+
 | Guest | --- |   DB    | --- | User1   | --- | User2  |
 +-------+     +---------+     +---------+     +--------+
 где DB   - машина с базой данных, для локальных пользователей
            выделено 3 канала
     User1, User2 - MISS-машины, на каждой по 2 канала;
     Guest- не-MISS машина, которой предоставлен 1 канал

 Описания конфигураций машин:

                    машина "User2"
 %
 % Связь с User1
 %
 * MULTIA * MULTIPLEX  * CHANNEL=.... ; NLINES=...
                         IDENTS[]=.....,&81,&82; ....
 * DBASE0 * LINE:DIAL  * CHANNEL=MULTIA.81 ; CLASS="D"
 * DBASE1 * LINE:DIAL  * CHANNEL=MULTIA.82 ; CLASS="D"

                    машина "User1"
 %
 % Связь с User2
 %
 * MULTIA * MULTIPLEX  * CHANNEL=.... ; NLINES=...
                         IDENTS[]=.....,&81,&82; ....
 %
 % Связь с DB
 %
 * MULTIB * MULTIPLEX  * CHANNEL=.... ; NLINES=...
                         IDENTS[]=.....,&81,&82,&83,&84;
 * DBASE0 * LINE:DIAL  * CHANNEL=MULTIB.83 ; CLASS="D"
 * DBASE1 * LINE:DIAL  * CHANNEL=MULTIB.84 ; CLASS="D"
 *        * COMMUTATOR * CHANI=MULTIA.81; CHANO=MULTIB.81
 *        * COMMUTATOR * CHANI=MULTIA.82; CHANO=MULTIB.82

                     машина "DB"
 %
 % Связь с User1
 %
 * MULTIA * MULTIPLEX  * CHANNEL=.... ; NLINES=...
                         IDENTS[]=.....,&81,&82,&83,&84; ....
 %
 % Связь с GUEST
 %
 * GUEST0 * .......... *

 * PIPE01 * PIPE       * DEVICE=&01
 * DBMAIN * LINE:DRIVE * CHANNEL=PIPE01.LX ; CLASS="S"
 * DBMULT * MULTI:DIAL * CHANNEL=PIPE01.LY ; NLINES=8
 *        * COMMUTATOR * CHANI=GUEST0.L0; CHANO=DBMULT.L0
 *        * COMMUTATOR * CHANI=MULTIA.81; CHANO=DBMULT.L1
 *        * COMMUTATOR * CHANI=MULTIA.82; CHANO=DBMULT.L2
 *        * COMMUTATOR * CHANI=MULTIA.81; CHANO=DBMULT.L3
 *        * COMMUTATOR * CHANI=MULTIA.82; CHANO=DBMULT.L4
 * DBASE0 * LINE:DIAL  * CHANNEL=DBMULT.L5 ; CLASS="D"
 * DBASE1 * LINE:DIAL  * CHANNEL=DBMULT.L6 ; CLASS="D"
 * DBASE2 * LINE:DIAL  * CHANNEL=DBMULT.L7 ; CLASS="D"


     Далее   необходимо   создать  "пользователя",  указав  в
 качестве   программы-посредника  "B.MARS",  и  включить  имя
 "пользователя"    в    файл   "@AUTOLOG".
     В     каталоге     пользователя    необходимо    создать
 каталоги    "DATA"   и   "ARCHIVE",   защитив   их   паролем
 "пользователя",  в каталог "DATA" поместить файл "Updates",
 защитив его тем же паролем.
\
     В  каталоге   пользователя  можно   создать  также  файл
 параметров СУБД  - "PARAMETERS",  формат которого следующий:
 строки, начинающиеся  с символа "%", являются комментариями,
 остальные строки  содержат имя  параметра  и  его  значение,
 разделенные  символом   "=".  При   отсутствии   файла   или
 отсутствии в нем описания какого-либо параметра используются
 значения по умолчанию. Обрабатываются следующие параметры:

   UPDATEFILE  =  <составное имя файла> - имя файла типа 'D',
 содержащего  каталог  базы  данных  и модифицированные блоки
 данных. По умолчанию - "Updates" (в рабочем каталоге).

   TEMPFILE = <составное имя файла> - имя рабочего файла СУБД
 типа  'D'  (файл  может располагаться на электронных дисках,
 т.к. содержимое файла не используется при перезапуске СУБД).
 По умолчанию - "TempFile" (в рабочем каталоге).

   SWAPFILE = <составное имя файла> - имя рабочего файла СУБД
 типа  'D'  (файл  может располагаться на электронных дисках,
 т.к. содержимое файла не используется при перезапуске СУБД).
 По умолчанию - "SwapFile" (в рабочем каталоге).

   ARCHIVE   = <составное имя файла> - имя архивного каталога
 СУБД   -   используется  при  архивации/восстановлении  базы
 данных; по умолчанию - "$ARCHIVE"

   DATACATL  =  <составное  имя  файла>  -  имя  каталога,  в
 котором  находятся  файлы  таблицы  и  индексов базы данных.
 Таких  каталогов  может  быть  задано несколько. При запуске
 СУБД  файлы с таблицами и индексами ищутся сначала в рабочем
 каталоге,  а  затем в каталогах, заданных данным параметром.
 Такие  каталоги  могут  быть созданы на других дисках, в них
 копируется  часть файлов базы данных, и во время работы СУБД
 эти  диски  могут  быть  защищены  от записи (кроме моментов
 архивации).

   AGENTS    = <число>          -   максимальное   количество
 пользователей,  одновременно  работающих  с СУБД. Необходимо
 обеспечить  соответствующее  число  каналов  в  конфигурации
 системы  (см.  выше).Значение по умолчанию - 4. Максимальное
 значение - 31.

   WHITELOCKS= <число>          -   максимальное   количество
 немодифицированных блоков таблиц и индексов, захваченных для
 совместного    использования    (Shared-locks).   Задаваемое
 значение  ограничено   об'емом  ОЗУ,   слишком  малое  число
 White-Lock -  элементов приводит  к переходу  от  страничной
 блокировки таблиц и индексов к блокировкам на уровне таблиц.

   TEMPSIZE  = <число> - максимальный размер файла TempFile в
 килобайтах. Значение по умолчанию - 800.

   CODEBUFS  =   <число>  -   количество  буферов  для  кодов
 запросов.  Задаваемое   значение  ограничено   об'емом  ОЗУ,
 слишком малое  число буферов  приводит к  перекачке кодов из
 памяти в  Файл SwapFile  и  обратно,  что  замедляет  работу
 системы. Значение по умолчанию - 10.
\
+                   II. Консоль оператора.

     При  запуске  подсистемы в "подключенном" режиме (т.е. с
 дисплеем) на экран выдается меню возможных режимов запуска:
     "Commands"   -   запускается   корневая   программа  для
 выполнения  каких-либо  действий  при  помощи  общесистемных
 программ, после чего вновь высвечивается меню режимов.
     "Start   "   -   производится   запуск  подсистемы;  при
 этом  производится откат всех транзакций, не завершившихся к
 моменту  останова  подсистемы.  Выдается сообщение о времени
 окончания последней зафиксированной транзакции.
     "Debug   "   -   производится   запуск   подсистемы    в
 "отладочном" режиме: запросы считываются не из канала связи,
 а из  файла "  TestStream", а  по его  окончании или при его
 отстутствии -  с экрана.  Первый символ  вводимой информации
 задает номер агента ("номер канала связи").
     "Restore  "  -  производится  восстановление базы данных
 при помощи файлов архивации (см. ниже), после восстановления
 система начинает нормальную работу.
     "Init     "  -  производится  инициализация базы данных:
 с дисплея запрашиваются парамтеры баззы данных:
 "Maximal number of database  files:"
 максимальное  число  файлов  в  базе  данных (т.е. суммарное
 число таблиц и индексов).
 "Maximal number of modified blocks:"
 максимальное количество модифицированных блоков в таблицах и
 индексах базы  данных (как зафиксированных операцией COMMIT,
 так  и   еще  не   зафиксированных).   Задаваемое   значение
 ограничено  доступным   об'емом  ОЗУ,  слишком  малое  число
 Black-Lock - элементов требует частой архивации базы данных.
 Далее формируется  файл "Updates", содержащий пустой каталог
 и  пустой   список  Black-Lock-элементов.  Создается  список
 пользователей  ("таблица"  "&USERS"),  в  которую  заносятся
 стандартные  имя  (SYS) и пароль пользователя-администратора
 (SYSTEM).
     После инициации система запускется в нормальном режиме.


     В системный журнал заносится сообщение о запуске системы
 управления базой данных следующего формата:
 MARS    (nnnnnnnnnn) started
 где    nnnnnnnnn - имя базы данных.

     Если  во  время  работы  подсистемы  возникает аварийная
 ситуация,  то  она  заканчивает работу. При этом в системный
 журнал и на консоль оператора базы выдается сообщение о коде
 аварийной ситуации. Формат сообщения в системном журнале:
 MARS    (nnnnnnnnnn) aborted with code=sccccc
 где    nnnnnnnnn - имя базы данных
        s         - знак кода ошибки ("-" или пробел)
        ccccc     - код ошибки
 Вместо "sccccc" может стоять текст системного сообщения.

\

     После  запуска  подсистемы  консоль  делится на 2 части:
 в  нижней  части  идет  трассировка  заказанных  событий,  в
 верхней возможен просмотр системных таблиц СУБД.
     Трассировка  управляется запросами, содержащими операцию
 "TRACE":

     TRACE <режим трассировки> [ ON | OFF]

 задание опции "OFF" отключает трассировку указанного режима,
 задание опции "ON" или запрос без опций - включает ее.
     Режимы  трассировки следующие (каждый может быть включен
 и выключен независимо):


 IO - трассировка ввода/вывода:

     reading (bb) tt pppp
     reading (bb) tt pppp <- uuuu

 чтение  в  cash-буфер  #bb из файла #tt страницы #pppp; если
 эта  страница была модифицирована и ее актуальное содержимое
 находится   в   Update-файле,   то  uuuu  -  номер  страницы
 Update-файла.

     writing (bb) tt pppp -> uuuu

 запись  из  cash-буфера  #bb страницы #pppp файла #tt новое,
 модифицированное  содержимое  записывается  в Update-файл, в
 страницу #uuuu.


 INPUT - трассировка ввода в базу; вводимые команды выводятся
 с  префиксом  "Input:";  0-ой  байт  задает номер канала, по
 которому принята информация.


 OUTPUT  -  трассировка  вывода информации из базы: выводится
 16-ный дамп выводной записи.

     Result =dddd dddd dddd

 SORT   - трассировка сортировки:

     Sorting: Nrecords=xxxx

 начало   сортировки  (всех  данных  или  только  части  их),
 выводится число сортируемых записей.

     Merging of xx streams to level yy

 начало  слияния  xx отсортированных частей таблицы (потоков)
 в поток уровня yy.



 CODE   - трассировка генерации кода

 code: aaaa  cccc

 выводится  16-ный дамп генерируемых "машинных" команд (cccc)
 и их адрес (aaaa).

\
 SWAP  - трассировка swapping-а

     Swap Out: agent #nn, code=cc, mem=mm, disk=ddd

 выгрузка на диск страницы программы запроса #cc, агента #nn;
 страница  находилась в памяти в блоке #mm, на диске (в файле
 "Swapping") оказалась в блоке #ddd.

     Swap  In: agent #nn, code=cc, mem=mmmm, disk=ddd

 загрузка с диска страницы программы запроса #cc, агента #nn;
 страница считавается в блок памяти #mm, с блока диска #ddd.


 LOCK  - трассировка блокировок

   Locked at &aaaa S(bbbbbbbbbbbbbbb)
   Locked at &aaaa X(bbbbbbbbbbbbbbb)

 агент   заблокирован,  aaaa  -  адрес  элемента  блокировки,
 bbbbbbbb  - гребенка битов агентов, заблокировавших элемент,
 S или X - тип блокировки

   Unlocked nn from &aaaa

 агент,  снимая  свою  блокировку  элемента  с  адресом aaaa,
 разблокировал агент #nn.


 CACHE - трассировка обращений к Cache-памяти

   Cache (bb): tt pppp (r)

 обращение  к  блоку  cache-памяти  номер bb, в котором лежит
 страница #pppp таблицы #tt, режим обращения (r):
 S - shared,  X - exclusive, N - new.

   Cache (bb): in tail (r)

 установ  блока  cache-памяти  в  хвост  списка LRU, при этом
 в зависимости от режима (r):
 S - флаг модификации не изменяется,
 C - флаг модификации обнуляется;

   Cache (bb): clear

 освобождение блока cache #bb

\
 COMMANDS - восприятие команд:

 при  отсутствии  готовых  к  выполнению  агентов  подсистема
 обычно  стоит  на ожидании прихода запроса из каналов связи.
 При этом восприятие команд с консоли оператора невозможно.
     "Трассировка"  команд  приводит  к  тому, что подсистема
 циклически (цикл - 0.3 сек) опрашивает консоль, паралельно с
 опросами  каналов  (но  это  замедляет реакцию на запросы из
 канала).

     При  отсоединении  дисплея от подсистемы все трассировки
 отключаются.



     Для  просмотра  системных  таблиц  на клавиатуре консоли
 можно набрать одну из следующих букв:

     D - просмотр каталога базы данных
     L - просмотр страничных блокировок
     A - просмотр состояний агентов
     P - просмотр состояний блоков памяти программ
     C - просмотр состояний cash-блоков
     W - просмотр состояний РАБОЧИХ ФАЙЛОВ

     Eсли  таблица  целиком поместилась на экране, то в конце
 ее выдается черта, иначе, нажатием символов ">" (или стрелка
 вниз)  и  "<" (или стрелка вверх) можно "листать" таблицу на
 экране оператора.

     Показ   таблицы   можно   вызвать   также   запросом  от
 пользователя,   имеющим   квалификацию   "OPER"  при  помощи
 операции "SHOW":

     SHOW <символ>
\
+             III. Архивация и дампирование базы.

     Во время  работы система не изменяет файлы с таблицами и
 индексами.   Вместо   этого   все  исправления  заносятся  в
 специальный  "Update-файл".  В нем же хранится каталог базы,
 таблицы  для  доступа  к  модифицированным  блокам  и  карта
 свободных   блоков   в   Update-файле  (все  это  называется
 синхро-информацией).
     Периодически   происходит    "архивация"   произведенных
 изменений,   которая   заключается  в  следующем.  Архивация
 запускается  пользователем,  имеющим квалификацию "OPER" при
 помощи операции "ARCHIVE":

     ARCHIVE

     В каталоге  архивации создается  файл с зафиксированными
 на данный  момент (операцией  COMMIT) изменениями  в  файлах
 базы данных.  Этот  файл  соответствует  текущему  состоянию
 Update-файла, но  не содержит блоков рабочих файлов и блоков
 с  незафиксированными  изменениями.  (ЗАМЕЧАНИЕ.  Содержимое
 файла идентично  тому состоянию  Update-файла, в  которое он
 был бы  приведен после  перезапуска системы,  если  бы  сбой
 произошел в  данный момент). Имена файлов архивации содержат
 текущий "номер архивации"
     После  этого  производится  запись  блоков,   содержащих
 зафиксированные изменения, на полагающееся им место в файлах
 таблиц  и  индексов,  а  соответствующие записи удаляются из
 таблицы   переадресации   блоков.  Файлы  логически  стертых
 таблиц уничтожаются,  для вновь  созданных таблиц  создаются
 новые  файлы.   После  завершения   указанных  действий  так
 называемый  "номер   архивации"   увеличивается   на   1   и
 производится    сброс   синхро-информации   в   Update-файл.
 Созданные  таким  образом  файлы  архивации нужны только для
 восстановления, поэтому они могут, например, копироваться на
 магнитную   ленту   с   последующим  стиранием  из  каталога
 "ARCHIVE".  Эти  операции  можно производить во времы работы
 базы  данных,  единственной  ограничение - каталог "ARCHIVE"
 не  должен  быть  заблокирован,  чтобы  позволить  нормально
 провести очередную архивацию.

     Если разносить измененные блоки по таблицам не нужно, то
 можно заказать операцию:
     ARCHIVE NOMOD
 при  этом  все  изменненные блоки остаются в Update-файле, а
 номер  архива  в  синхро-информации  не  увеличивается; если
 соответствующих  ARCHIVE-файл  уже  существует, то архивация
 производится в него. Таким образом можно сохранять изменения
 в  базе  данных  на  случай аппаратных сбоев, не модифицируя
 файлы   базы  (например,  если  они  аппаратно  защищены  от
 записи).  Однако  в  этом  случае  место  в  Update-файле не
 освобождается.
     Если  сохранение  архивных файлов не требуется, то можно
 заказать операцию:
     ARCHIVE NOFILE
 при  этом все измененные блоки будут записаны в файлы таблиц
 и  индексов,  но  не будет создан файл архивации, хотя номер
 архива  будет  увеличен.  Использование  хотя  бы раз такого
 режима  приведет  к  невозможности в дальнейшем восстановить
 базу  данных  по  ее  дампу  и  архивной информации. Поэтому
 желательно  использовать  такой  режим  только  незадолго до
 очередного дампа всей базы (см. ниже).

     При   интенсивной  модификации  данных  суммарный  об'ем
 файлов  архивации  может  стать  соизмеримым с об'емом самой
 базы, при этом становится эффективным произвести копирование
 всей базы (дамп базы). Это производится при помощи системных
 средств при  запуске  СУБД  с  дисплея  в режиме "Commands":
 необходимо  скопировать  все  файлы  "D.Table  xxx"  и  файл
 "D.Updates". После этого все файлы архивации можно удалить.

\
+               IV. Восстановление базы данных

     В  случае неисправимой ошибки носителя, содержащего базу
 данных,  ее  необходимо восстановить следующим образом (СУБД
 должна быть неактивной или работать в режиме "Commands").

     1.  Загрузить  в  каталог архивации все файлы архивации,
 созданные после дампа.

     2.  Если текущий  Update-файл (" Updates") не поврежден,
 то скопировать его в кталог архивации под именем "ArcNNNNN",
 где NNNNN - число, на 1 большее номера последнего архива.

     3. Стереть  из каталога  ".DATA" все  файла базы  данных
 ("D.Tablexxx"),  а   также  Update-файл,  загрузить  в  этот
 каталог файлы  базы данных и Update-файл последней спасенной
 копии всей базы (последний дамп).

     4.  Запустить  СУБД  в  "подключенном"  режиме,  т.е.  с
 дисплеем,    для   чего   войти   в   систему   под   именем
 "пользователя"-базы данных на одном из дисплеев.

     5.   В   качестве   режима   запуска  выбрать  "Restore"
 (Восстановление).  На  экран  будут  выдаваться сообщения об
 обработанных  файлах  архивации,  в  том  числе дата и время
 архивации.

     6.  После  окончания  восстановления система переходит в
 нормальный режим работы.
