#include "mars.h"
#include "intern.h"

/*
 * Динамическая выборка.
 */
short DBlfetch (DBdescriptor descr)
{
	return (_DBio ('F', '0'+descr->identifier, 2));
}
