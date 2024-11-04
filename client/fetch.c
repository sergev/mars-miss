#ifndef NO_STDLIB
#include <stdlib.h>
#endif
#include <stdarg.h>
#include <string.h>
#include "mars.h"
#include "intern.h"

/*
 * Выборка записей (сканер).
 */
short DBfetch (DBdescriptor descr, ...)
{
	va_list dscan;
	short   retcode;
	long    Length [MaxGetLongData];
	int     Nlongdata, nelem;
	DBelement* elem;

	retcode = _DBio ('F', '0'+descr->identifier, 2);
	if (retcode <= 0)
		return (retcode);

	Nlongdata = 0;
	va_start (dscan, descr);
	elem = descr->elements + descr->paramNumber;
	for (nelem=0; nelem<descr->columnNumber; ++nelem, ++elem) {
		char *ptr = va_arg (dscan, char*);
		short *data = (short*) (DBbuffer + elem->shift);

		switch (elem->type) {
		case DBvarchar:
			if (ptr) {
				int len = data[0] - data[1] - SHORTSZ;
				memcpy (ptr, data[1] + (char*)(data+1), len);
				ptr [len] = 0;
			}
			break;
		case DBlongdata:
			if (Nlongdata < MaxGetLongData)
				Length [Nlongdata++] = ((DBlongdataItem*)data)->length;
			break;
		case DBshort:
			if (ptr)
				*(short*) ptr = *(short*) data;
			break;
		case DBlong:
			if (ptr)
				*(long*) ptr = *(long*) data;
			break;
		case DBfloat:
			if (ptr)
				*(double*) ptr = *(double*) data;
			break;
		default:
			if (ptr)
				memcpy (ptr, data, elem->length);
		}
	}
	va_end (dscan);

	if (! Nlongdata)
		return (1);

	Nlongdata = 0;
	va_start (dscan, descr);
	elem = descr->elements + descr->paramNumber;
	for (nelem=0; nelem<descr->columnNumber; ++nelem, ++elem) {
		long shift, length;
		DBlongGet getProc = va_arg (dscan, DBlongGet);

		if (elem->type != DBlongdata || !getProc)
			continue;
		shift = 0;
		length = Length [Nlongdata++];
		(*getProc) (descr, nelem, (void*) 0, length, 0);
		*(short*) DBexecBuf = nelem;
		while (length) {
			unsigned slice = (MaxDBInput-SHORTSZ);
			if (slice > length)
				slice = length;
			((long*)(DBexecBuf+SHORTSZ))[0] = shift;
			((long*)(DBexecBuf+SHORTSZ))[1] = slice;
			retcode = _DBio ('<', '0'+descr->identifier,
				2 + SHORTSZ + 2*LONGSZ);
			if (retcode < 0)
				return (retcode);
			if (slice > *(short*)DBbuffer - SHORTSZ) {
				slice = *(short*)DBbuffer - SHORTSZ;
			}
			(*getProc) (descr, nelem, DBbuffer + SHORTSZ, shift, slice);
			shift += slice;
			length -= slice;
		}
	}
	va_end (dscan);
	return (1);
}
