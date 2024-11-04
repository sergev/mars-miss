#include "mars.h"
#include "intern.h"

/*
 * Освобождение оттранслированного кода.
 */
short DBkill (DBdescriptor descr)
{
	return (_DBio ('K', '0'+descr->identifier, 2));
}
