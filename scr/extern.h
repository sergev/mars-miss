#include <stdlib.h>
#include <string.h>
#include <unistd.h>

inline void outerr (char *str) { write (2, str, strlen (str)); }

inline int lowercase (int c)
{
	if (c>='A' && c<='Z')
		return (c+'a'-'A');
#if 0
	switch (c) {
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	case '�': return ('�');
	}
#endif
	return (c);
}
