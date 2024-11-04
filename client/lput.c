#include <string.h>
#include "mars.h"
#include "intern.h"

short DBlput (DBdescriptor descr, short parmNum, void *addr, unsigned length)
{
	short retcode;

	*(short*) DBexecBuf = parmNum;
	if (parmNum < 0) {
		retcode = _DBio ('>', '0'+descr->identifier, 2+SHORTSZ);
		if (retcode == SHORTSZ+LONGSZ) {
		  DBinfo.nrecords  = *(long*) (DBbuffer + SHORTSZ);
		  retcode = 0;
		}
		return (retcode);
	}
	while (length > 0) {
		unsigned slice = MaxDBOutput - 2 - SHORTSZ - LONGSZ;
		if (length < slice)
			slice = length;
		*(long*) (DBexecBuf + SHORTSZ) = slice;
		memcpy (DBexecBuf+SHORTSZ+LONGSZ, addr, slice);
		retcode = _DBio ('>', '0'+descr->identifier,
			2 + SHORTSZ + LONGSZ + slice);
		if (retcode)
			return (retcode);
		addr = (char*) addr + slice;
		length -= slice;
	}
	return(0);
}
