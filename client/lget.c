#include <string.h>
#include "mars.h"
#include "intern.h"

short DBlget (DBdescriptor descr, short parmNum, void *addr, long shift, unsigned length)
{
	short retcode;

	*(short*) DBexecBuf = parmNum;
	while (length > 0) {
		unsigned slice = MaxDBInput - SHORTSZ;
		if (length < slice)
			slice = length;
		((long*) (DBexecBuf+SHORTSZ)) [0] = shift;
		((long*) (DBexecBuf+SHORTSZ)) [1] = slice;
		retcode = _DBio ('<', '0'+descr->identifier, 2+SHORTSZ+LONGSZ*2);
		if (retcode < 0)
			return (retcode);
		if (slice > *(short*)DBbuffer - SHORTSZ) {
		    slice = *(short*)DBbuffer - SHORTSZ;
		}
		memcpy (addr, DBbuffer+SHORTSZ, slice);
		shift += slice;
		addr = (char*) addr + slice;
		length -= slice;
	}
	return(0);
}
