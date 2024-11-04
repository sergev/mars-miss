#include "mars.h"
#include "intern.h"

/*
 * Динамическое выполнение.
 */
short DBlexec (DBdescriptor descr)
{
	short retcode = _DBio ('E', '0'+descr->identifier, 2 + *(short*)DBexecBuf);
	if (retcode == SHORTSZ+LONGSZ) {
		DBinfo.nrecords = *(long*) (DBbuffer + SHORTSZ);
		retcode = 0;
	}
	return (retcode);
}
