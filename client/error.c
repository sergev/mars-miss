#include <stdio.h>
#include "mars.h"
#include "intern.h"

char *DBerror (int err)
{
	static char buf [16];

	switch (err) {
	case 0:                 return ("O.K.");
	case ErrOverflow:       return ("Numeric overflow");
	case ErrSyntax:         return ("Invalid syntax");
	case ErrName:           return ("Bad name");
	case ErrSyst:           return ("Internal inconsistency");
	case ErrSymbol:         return ("Invalid symbol");
	case ErrKey:            return ("Invalid key");
	case ErrNum:            return ("Bad number");
	case ErrLeng:           return ("Bad string length");
	case ErrString:         return ("Invalid string");
	case ErrComplicated:    return ("Too complicated query");
	case ErrDoubleDef:      return ("Doubly defined object");
	case ErrUndefined:      return ("Undefined object");
	case ErrExists:         return ("Object already exists");
	case ErrFull:           return ("Object table overflow");
	case ErrNoExists:       return ("Object does not exist");
	case ErrType:           return ("Invalid type");
	case ErrNoCode:         return ("Code not found");
	case ErrModified:       return ("Cannot use modified object");
	case ErrColNum:         return ("Invalid number of columns");
	case ErrColName:        return ("Bad column name");
	case ErrDeadlock:       return ("Deadlock");
	case ErrInit:           return ("Initialization failed");
	case ErrReduction:      return ("Invalid reduction");
	case ErrGroup:          return ("Nonconstant value for group");
	case ErrNoTempFile:     return ("No temporary file");
	case ErrTableSpace:     return ("Too big table");
	case ErrIndexSpace:     return ("Too big index");
	case ErrModSpace:       return ("Update file overflow");
	case ErrNotuniq:        return ("Object is not unique");
	case ErrLockSpace:      return ("Out of lock space");
	case ErrCommand:        return ("Bad command");
	case ErrParameters:     return ("Invalid parameters");
	case ErrClosed:         return ("Cursor closed");
	case ErrManyCodes:      return ("Too many codes");
	case ErrRights:         return ("Permission denied");
	case ErrUser:           return ("Invalid user");
	case ErrPassw:          return ("Invalid password");
	case ErrTempSpace:      return ("Out of temporary space");
	case ErrEquery:         return ("Query error");
	case ErrEqmany:         return ("Not unique: many");
	case ErrEqempty:        return ("Not unique: empty");
	case ErrBigSort:        return ("Big sort failed");
	case ErrInterrupted:    return ("Query interrupted");
	case ErrLongSpace:      return ("No long data space");
	}
	sprintf (buf, "#%d", err);
	return (buf);
}
