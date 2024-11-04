#include <stdio.h>
#ifndef NO_STDLIB
#include <stdlib.h>
#endif
#include <stdarg.h>
#include <string.h>
#include "mars.h"
#include "intern.h"

/*
 * Выполнение скомпилированного запроса.
 */
short DBexec (DBdescriptor descr, ...)
{
	va_list dscan;
	int nelem, retcode, wasLong;
	DBelement* elem;

	*((short*)DBexecBuf + descr->nVars) = descr->fixparmleng;
	wasLong = 0;
	va_start (dscan, descr);
	elem = descr->elements;
	for (nelem=0; nelem<descr->paramNumber; ++nelem, ++elem) {
		short *data = (short*) (DBexecBuf + elem->shift);

		switch (elem->type) {
		case DBchar:
			memcpy (data, va_arg (dscan, char*), elem->length);
			break;
		case DBshort:
			*data = va_arg (dscan, int);
			break;
		case DBlong:
			*(long*) data = va_arg (dscan, long);
			break;
		case DBfloat:
			*(double*) data = va_arg (dscan, double);
			break;
		case DBvarchar: {
			char *params = va_arg (dscan, char*);
			char *endparams = memchr (params, 0, elem->length);
			short len = endparams ? endparams-params : elem->length;

			memcpy ((char*) (data+1) + data[1], params, len);
			*data = data[1] + len + SHORTSZ;
			break;
			}
		case DBlongdata: {
			DBlongPut putProc = va_arg (dscan, DBlongPut);

			wasLong = 1;
			((DBlongdataItem*)data)->length =
				(*putProc) (descr, nelem, (void*) 0, -1L, 0);
			break;
			}
		default:
			fprintf (stderr, "mars: DBexec: bad parameter type\n");
			break;
		}
	}
	va_end (dscan);

	retcode = DBlexec (descr);
	if (retcode || !wasLong)
		return (retcode);

	va_start (dscan, descr);
	elem = descr->elements;
	for (nelem=0; nelem<descr->paramNumber; ++nelem, ++elem) {
		switch (elem->type) {
		case DBvarchar: (void) va_arg (dscan, char*);  break;
		case DBchar:    (void) va_arg (dscan, char*);  break;
		case DBshort:   (void) va_arg (dscan, int);   break;
		case DBlong:    (void) va_arg (dscan, long);   break;
		case DBfloat:   (void) va_arg (dscan, double); break;
		case DBlongdata: {
			DBlongPut putProc = va_arg (dscan, DBlongPut);
			long shift = 0;
			for (;;) {
				unsigned int slice = (*putProc) (descr, nelem,
					DBexecBuf + SHORTSZ + LONGSZ, shift,
					MaxDBOutput - 2 - SHORTSZ - LONGSZ);
				if (slice == 0)
					break;
				*(short*) DBexecBuf = nelem;
				*(long*) (DBexecBuf+SHORTSZ) = slice;
				retcode = _DBio ('>', '0'+descr->identifier,
					2 + SHORTSZ + LONGSZ + slice);
				if (retcode)
					return (retcode);
				shift += slice;
			}
			break;
			}
		default:
			fprintf (stderr, "mars: DBexec: bad parameter type\n");
			break;
		}
	}
	va_end (dscan);

	*(short*) DBexecBuf = -1;
	retcode = _DBio ('>', '0'+descr->identifier, 2+SHORTSZ);
	if (retcode == SHORTSZ + LONGSZ) {
		DBinfo.nrecords = *(long*) (DBbuffer + SHORTSZ);
		retcode = 0;
	}
	return (retcode);
}
