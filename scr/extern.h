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
	case 'á': return ('Á');
	case 'â': return ('Â');
	case '÷': return ('×');
	case 'ç': return ('Ç');
	case 'ä': return ('Ä');
	case 'å': return ('Å');
	case 'ö': return ('Ö');
	case 'ú': return ('Ú');
	case 'é': return ('É');
	case 'ê': return ('Ê');
	case 'ë': return ('Ë');
	case 'ì': return ('Ì');
	case 'í': return ('Í');
	case 'î': return ('Î');
	case 'ï': return ('Ï');
	case 'ð': return ('Ð');
	case 'ò': return ('Ò');
	case 'ó': return ('Ó');
	case 'ô': return ('Ô');
	case 'õ': return ('Õ');
	case 'æ': return ('Æ');
	case 'è': return ('È');
	case 'ã': return ('Ã');
	case 'þ': return ('Þ');
	case 'û': return ('Û');
	case 'ý': return ('Ý');
	case 'š': return ('ß');
	case 'ù': return ('Ù');
	case 'ø': return ('Ø');
	case 'ü': return ('Ü');
	case 'à': return ('À');
	case 'ñ': return ('Ñ');
	}
#endif
	return (c);
}
