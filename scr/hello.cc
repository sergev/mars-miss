#include "Screen.h"

Screen V;

main ()
{
	V.Put (V.Lines / 2, V.Columns / 2 - 6, "Hello, World!");
	V.Sync ();
	V.GetKey ();
	V.Move (V.Lines - 1, 0);
	V.Sync ();
	return (0);
}
