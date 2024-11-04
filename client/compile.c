#include <stdio.h>
#ifndef NO_STDLIB
#include <stdlib.h>
#endif
#include <string.h>
#include "mars.h"
#include "intern.h"

/*
 * Обработка текста запроса.
 */
short DBcompile (char *text, unsigned len, DBdescriptor descr, unsigned dlen)
{
	short retcode;

	if (! text)
	      text = DBexecBuf;
	if (text != DBexecBuf)
		strncpy (DBexecBuf, text, MaxDBOutput);
	DBexecBuf [MaxDBOutput] = 0;
	if (! len)
		len = strlen (DBexecBuf);

	retcode = _DBio (' ', ' ', len + 2);
	if (retcode <= 0)
		return (retcode);               /* О.К. message */

	if (descr) {
		short *reply = (short*) DBbuffer;
		if (*reply > dlen) {
			fprintf (stderr, "mars: DBcompile: out of memory\n");
			return (-1);
		}
		memcpy (descr, reply, *reply);
	}
	return (1);
}
