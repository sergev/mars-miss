/*
 * Интерфейс с СУБД MARS.
 */

#ifndef __mars_h__
#define __mars_h__

#ifdef __cplusplus
extern "C" {
#endif

#define MaxDBInput  1000                /* размер буфера результата */
#define MaxDBOutput 6000 /* было 2000 *//* размер буфера параметров */

#define MaxGetLongData 10               /* макс.число LongData в fetch */

/* типы данных */
enum { DBshort = 1, DBlong, DBfloat, DBchar, DBvarchar, DBlongdata };

/* функция-обработки "proceed" */
typedef short (*DBtrap) (short);

/* описатель колонки/параметра*/
typedef struct {
	short           type;
	short           length;
	short           shift;
} DBelement;

typedef struct {                        /* описатель кода */
	short           descrLength;    /* полная длина дескриптора */
	short           identifier;     /* идентификатор кода в базе */
	short           paramNumber;    /* число параметров кода */
	short           fixparmleng;    /* длина фикс. пар-ов (без var) */
	short           nVars;          /* число параметров типа varchar */
	short           columnNumber;   /* число выходных колонок */
	DBelement       elements[1];    /* описатели парам-ов/колонок */
} *DBdescriptor;

typedef struct {
	char            int1;
	char            int2;
	unsigned short  int3;
	long            length;
} DBlongdataItem;

extern struct DBinfo {
	short           errCode;        /* код ошибки */
	short           errPosition;    /* ее позиция в тексте или -1 */
	long            nrecords;       /* счетчик модифицированных */
} DBinfo;

typedef long (*DBlongPut) (DBdescriptor, short, void*, long, unsigned);
typedef void (*DBlongGet) (DBdescriptor, short, void*, long, unsigned);

extern char DBbuffer [];                /* буфер ввода */
extern char *DBexecBuf;                 /* буфер запроса */
extern char *DBhost;                    /* имя машины сервера */
extern int DBport;                      /* номер порта сервера */

/* подключение к СУБД */
extern short    DBconnect (void);

/* отключение от СУБД */
extern void     DBdisconn (void);

/* окончание транзакции */
extern void     DBcommit (int);

/* обработка текста запроса */
extern short    DBcompile (char*, unsigned, DBdescriptor, unsigned);

/* выполнение/инициация кода  */
extern short    DBexec (DBdescriptor, ...);

/* получение очередной записи */
extern short    DBfetch (DBdescriptor, ...);

/* убирание кода */
extern short    DBkill (DBdescriptor);

/* установ п/п обраб. "proceed" */
extern DBtrap   DBwait (DBtrap);

/* динамические операции */
extern short    DBlexec (DBdescriptor);
extern short    DBlfetch (DBdescriptor);
extern short    DBlput (DBdescriptor, short, void*, unsigned);
extern short    DBlget (DBdescriptor, short, void*, long, unsigned);

/* Коды ошибок */
enum {
	ErrOverflow = 1,        ErrSyntax,
	ErrName,                ErrSyst,
	ErrSymbol,              ErrKey,
	ErrNum,                 ErrLeng,
	ErrString,              ErrComplicated,
	ErrDoubleDef,           ErrUndefined,
	ErrExists,              ErrFull,
	ErrNoExists,            ErrType,
	ErrNoCode,              ErrModified,
	ErrColNum,              ErrColName,
	ErrDeadlock,            ErrInit,
	ErrReduction,           ErrGroup,
	ErrNoTempFile,          ErrTableSpace,
	ErrIndexSpace,          ErrModSpace,
	ErrNotuniq,             ErrLockSpace,
	ErrCommand,             ErrParameters,
	ErrClosed,              ErrManyCodes,
	ErrRights,              ErrUser,
	ErrPassw,               ErrTempSpace,
	ErrEquery,              ErrEqmany,
	ErrEqempty,             ErrBigSort,
	ErrInterrupted,         ErrLongSpace,
};

/* диагностика ошибки */
extern char *DBerror (int);

#ifdef __cplusplus
};
#endif

#endif __mars_h__
