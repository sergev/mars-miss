/*
 *
 * Переменные/поля:            ДАННЫЕ:
 *
 * +-----------------------+
 * | вид переменной:       |    Результирующая запись из СУБД
 * | (data,builtin,summ,gr)|    (DBsaved)
 * +-----------------------+    +------+----------+----------------+
 * | описание спецификаций |    |      |          |                |
 * | формата               |    |      |          |                |
 * +-----------------------+    +------+----------+----------------+
 * | высота переменной     |
 * | для текстовых констант|    Встроенные переменные:
 * +-----------------------+
 * | указатель на данное   |    +------+ +--------+ +------+ +------+
 * +-----------------------+    | PAGE | | RECORD | | TIME | | DATE |
 * | указатель на массив   |    +------+ +--------+ +------+ +------+
 * | сумм или на хранимое  |
 * | значение для группир. |
 * +-----------------------+    Массивы суммируемых значений:
 *
 *                              +-----+              +-------+
 *                              |     |<- по отчету->|       |
 *                              +-----+              +-------+
 *                              |     |<- по группе->|       |
 *                              +-----+              +-------+
 *                              |     |<- по подгр.->|       |
 *                              +-----+              +-------+
 *                              |     |<- по стр.  ->|       |
 *                              +-----+              +-------+
 *
 *
 *                              Значения полей, опред-щих группы
 *
 *                              +-----+     +------------+
 *                              |     |     |            |
 *                              +-----+     +------------+
 */

#include <mars.h>

#define MaxDbOut        1000            /* лимит результ. записи СУБД */
#define MaxSource       500             /* лимит длины входной строки */
#define MaxColumns      80              /* лимит кол-ва столбцов */
#define MaxGroups       64              /* лимит уровней группиров-ия */
#define MaxHigh         20              /* лимит высоты записи */
#define Nbuiltin        5               /* число встроенных полей */
#define NIL             0

#define LineStep (MaxSource+2)

enum {
	Lpwd = 8,                       /* длина пароля */
	Lname = 10,                     /* длина имени пользователя */
};

enum fclass {                           /* описатель выводного поля */
	Edata,                          /* простое поле */
	Egroup,                         /* группируемое */
	Esumm,                          /* суммируемое */
	Ebuiltin,                       /* встроенное */
};

typedef enum {
	FALSE=0,                        /* ложь */
	TRUE=1,                         /* истина */
} boolean;

enum bmode {                            /* описатель блока */
	Rnormal,                        /* обычный */
	Ralone,                         /* конец страницы */
	Rpage,                          /* начало страницы */
};

/*
 * Коды наших ошибок
 */
enum {
	RErrSyntax=1, RErrNotSelect, RErrManyColumns, RErrTooLong, RErrName,
	RErrBigLine,  RErrFormat,    RErrManyGroups,  RErrGroup,   RErrEmpty,
	RErrGrValue,  RErrVarchar,   RErrSumm,        RErrKeyword, RErrHigh,
	RErrPSmult,
};

typedef struct {
	enum fclass     fclass;         /* вид обработки поля */
	char            suppress;       /* подавление поля группирования */
	char            formatsign;     /* символ форматирования */
	short           type;           /* тип данных */
	short           length;         /* длина двоичных данных */
	short           formatwidth;    /* ширина поля */
	short           suplinfo;       /* d-формат или высота текста */
	char*           value;          /* текущее значение */
	char*           svalue;         /* значение группируемых, сумм */
} desc_elem;

typedef struct textfield {              /* описатель поля в шапке/концевике */
	struct textfield* next;         /* следующее поле */
	short           position;       /* позиция в строке */
	desc_elem*      data;           /* адрес значения */
} textfield;

typedef struct textline {               /* описатель строки шапки/концевика */
	struct textline* next;          /* следующая строка */
	textfield*      fields;         /* список полей в тексте */
	char*           text;           /* адрес текста */
} textline;

typedef struct {
	textline*       lines;          /* голова списка строк текста*/
	short           nlines;         /* число строк в тексте */
	enum bmode      mode;           /* вид выдачи текста */
} text_block;

extern short            Groups [MaxGroups];     /* номера группируемых полей */
extern text_block*      GroupHeads [MaxGroups];
extern text_block*      GroupTails [MaxGroups];

extern int              Ngroups;                /* количество группируемых */
extern int              PSlevel;                /* уровень страничных сумм */

extern char*            OutputFileName;         /* имя файла или трубы */
extern char*            UserName;               /* имя пользователя */
extern char*            Password;               /* пароль */

extern char*            Builtins [Nbuiltin];

extern desc_elem**      Columns;                /* данные из вых. строки */

extern DBdescriptor     Mdescr;
extern DBelement*       Cdescr;

extern int              PageSize;
extern short            LineCount;
extern short            PrecCount;
extern short            RecCount;
extern short            PageCount;

extern text_block       DataLine;               /* запись данных */
extern text_block       PageSumm;               /* страничный итог */
extern text_block       PageHead;               /* заголовок страницы */
extern text_block       PageTail;               /* концевик страницы */
extern text_block       InfoHead;               /* заголовок информации */
extern text_block       InfoTail;               /* концевик информации */

extern boolean          Ualign;                 /* выравнивание к верху */
extern int              DBtextlen;
extern char             DBsaved [];

extern void             dberror (void);         /* фатальная ошибка СУБД */
extern void             MyError (int);          /* наша фатальная ошибка */
extern void             Translate (void);       /* чтение и трансляция запроса */
extern void             ScanFile (void);        /* чтение описания отчета */
extern void             FormPaper (void);       /* формированаие текста отчета */
extern void             PutLine (char*);        /* запись строки на выход */
extern int              GetChar (void);         /* чтение символа со входа */
extern int              EndOfFile (void);       /* конец входного потока? */
