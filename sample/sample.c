/*
 * Пример программы, использующей СУБД MARS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mars.h>

/* запрос на идентификацию */
char LogonReq[] = "LOGON SYS SYSTEM";

/* запрос на создание таблицы */
char CreateReq[] =
	"Create table test"
	"(a:short,b:long,c:float,d:char(10),e:varchar(20))";

/* запрос на вставку записей (параметризованный) */
char InsertReq[] =
	"(a:short,b:long,c:float,d:char(10),e:varchar(20))"
	"Insert values $a,$b,$c,$d,$e into test";

/* запрос на выборку записей */
char SelectReq[] = "Select test for a,b,c,d,e";

/* запрос на удаление части записей */
char DeleteReq[] = "Delete test where a<50 or e='leng 6'";

/*
 * Выдача сообщения об ошибке
 */
void ErrMessage ()
{
	printf ("Data Base error: %d: %s\n", DBinfo.errCode,
		DBerror (DBinfo.errCode));
	DBdisconn ();                   /* отключение от СУБД */
	exit (1);                       /* откат произойдет автоматически */
}

int main ()
{
	/* дескрипторы кодов запросов */
	char    insertDescrBuf [200];
	char    selectDescrBuf [200];
	char    deleteDescrBuf [100];
	DBdescriptor insertDescr = (DBdescriptor) insertDescrBuf;
	DBdescriptor selectDescr = (DBdescriptor) selectDescrBuf;
	DBdescriptor deleteDescr = (DBdescriptor) deleteDescrBuf;

	short ret, count;
	short   ar;
	long    br;
	double  cr;
	char    dr[10], er[21];

	/* установление связи с СУБД */
	printf ("Connecting to Data Base ...\n");

	if (DBconnect () < 0)
		ErrMessage ();

	/* выполнение интерпретируемых запросов */

	if (DBcompile (LogonReq, strlen (LogonReq), (void*) 0, 0) < 0)
		ErrMessage ();

	if (DBcompile (CreateReq, strlen (CreateReq), (void*) 0, 0) < 0)
		ErrMessage ();

	/* компиляция запросов на манипуляцию данными */
	printf ("Requests compiling ...\n");

	if (DBcompile (InsertReq, strlen (InsertReq),
	    insertDescr, sizeof (insertDescrBuf)) < 0)
		ErrMessage ();
	if (DBcompile (SelectReq, strlen (SelectReq),
	    selectDescr, sizeof (selectDescrBuf)) < 0)
		ErrMessage ();
	if (DBcompile (DeleteReq, strlen (DeleteReq),
	    deleteDescr, sizeof (deleteDescrBuf)) < 0)
		ErrMessage ();

	/* вставка тестовой информации в созданную таблицу */
	printf ("Records inserting ...\n");

	for (count=1; count<100; ++count) {
		static char* vars [3] = { "length 9", "length 10", "leng 6" };

		if (DBexec (insertDescr, count, count + 1000000L,
		    count / 1000.0, "testline10", vars [count%3]) < 0)
			ErrMessage ();
	}
	DBkill (insertDescr);           /* этот код не нужен */

	/* выборка данных из базы данных */
	printf ("Reading results ...\n");

	if (DBexec (selectDescr) < 0)
		ErrMessage ();          /* инициация сканера */

	while ((ret = DBfetch (selectDescr, &ar, &br, &cr, &dr, &er)) != 0) {
		if (ret < 0)
			ErrMessage ();
		printf ("data: a=%3d, b=%8ld, c=%8.4g, d=%.10s, e='%s'\n",
			ar, br, cr, dr, er);
	}

	/* стирание части записей */
	printf ("Deleting ...\n");
	if (DBexec (deleteDescr) < 0)
		ErrMessage ();

	/* вновь выборка данных (по тому же запросу) */
	printf ("Reading results ...\n");

	if (DBexec (selectDescr) < 0)
		ErrMessage ();

	while ((ret = DBfetch (selectDescr, &ar, &br, &cr, &dr, &er)) != 0) {
		if (ret < 0)
			ErrMessage ();
		printf ("data: a=%3d, b=%8ld, c=%8.4g, d=%.10s, e='%s'\n",
			ar, br, cr, dr, er);
	}

	printf ("Disconnecting ...\n");

	DBcommit (0);                   /* откат нашей транзакции */
	DBdisconn ();                   /* отключение от базы данных */
	return (0);
}
