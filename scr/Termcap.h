const int CAPNUM = 1;
const int CAPFLG = 2;
const int CAPSTR = 3;

struct Captab {
	char tname [3];
	char ttype;
	char tdef;
	char *tc;
	int *ti;
	char **ts;
};

