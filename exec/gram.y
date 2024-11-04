%{
typedef struct node {
	int type;
	struct node *left;
	struct node *right;
} node;

node *tnode (int type, node *l, node *r);
void texec (node *);
char **tlist (node *);
extern void Connect (void);
extern void Disconnect (void);
%}
%union {
	char *string;
	char **stringtab;
	node *nodeptr;
}
%token PUT WITH FORALL USING
%token <string> QUERY STRING
%type <string> format filename
%type <stringtab> block
%type <nodeptr> program statement_list statement operator_list operator
%%

/*
 * Программа есть последовательность операторов.
 */
program:        statement_list                  {
				if ($1) {
					Connect ();
					texec ($1);
					Disconnect ();
				}
			}
	;

statement_list: /* void */                      { $$ = 0; }
	|       statement_list statement        { $$ = !$1 ? $2 : !$2 ? $1 :
						  tnode (';', $1, $2); }
	;

/*
 * Есть четыре вида операторов:
 * 1) простой запрос:
 *      query ;
 * 2) запись данных из базы в файл:
 *      put "filename" [ using "format" ] query ;
 * 3) чтение данных из файла в базу:
 *      with "filename" [ using "format" ] query ;
 *      with "filename" [ using "format" ] { query ; query... ; }
 * 4) извлечение данных из базы,
 *    обработка и занесение результата в базу:
 *      forall query ; query ;
 *      forall query ; { query ; query ... ; }
 */
statement:      ';'                             { $$ = 0; }
	|       operator                        { $$ = $1; }
	|       FORALL operator block           { $$ = tnode ('F', $2, (node*) $3); }
	|       PUT filename format operator    { $$ = tnode ('W', (node*) $2,
						  $3 ? tnode ('U', (node*) $3,
						  $4) : $4); }
	|       WITH filename format block      { $$ = tnode ('R', (node*) $2,
						  $3 ? tnode ('U', (node*) $3,
						  (node*) $4) : (node*) $4); }
	;

format:         USING STRING                    { $$ = $2; }
	|       /* empty */                     { $$ = 0; }
	;

filename:       STRING                          { $$ = $1; }
	|       '-'                             { $$ = 0; }
	;

block:          '{' operator_list '}'           { $$ = tlist ($2); }
	|       operator                        { $$ = tlist ($1); }
	;

operator_list:  operator_list operator          { $$ = tnode (';', $1, $2); }
	|       operator                        { $$ = $1; }
	;

operator:       QUERY ';'                       { $$ = tnode ('Q', (node*) $1, 0); }
	;

%%

#include <stdio.h>
#ifndef NO_STDLIB
#include <stdlib.h>
#endif
#include <string.h>

#define MAXQUERIES      20
#define MAXNODES        256

#define yygetchar()     (yyfd ? getc (yyfd) : *yyptr ? *yyptr++ : -1)
#define yyungetc(c)     (yyfd ? ungetc ((c), yyfd) : (*--yyptr = (c)))

#define isletter(c)     (c>='A' && c<='Z' || c>='a' && c<='z' ||\
				c>='0' && c<='9' || (unsigned char) c>=0200)

node nodetab [MAXNODES];
char lexbuf [1024];
int lexbuflen;
int yyline;
int yylexline;
FILE *yyfd = stdin;
char *yybuf, *yyptr;

struct {
	char *name;
	int key;
} keytab [] = {
	"with",         WITH,
	"With",         WITH,
	"WITH",         WITH,
	"put",          PUT,
	"Put",          PUT,
	"PUT",          PUT,
	"forall",       FORALL,
	"Forall",       FORALL,
	"FORALL",       FORALL,
	"using",        USING,
	"Using",        USING,
	"USING",        USING,
	0,              0,
};

extern void Execute (char *query);
extern void DoForAll (char *query, char **list);
extern void DoWrite (char *filename, char *format, char *query);
extern void DoRead (char *filename, char *format, char **list);

#ifdef NO_STRDUP
/* extern void *malloc (unsigned); */

char *strdup (char *str)
{
	int len = strlen (str) + 1;
	char *copy = malloc ((unsigned) len);

	if (! copy)
		return (0);
	memcpy (copy, str, len);
	return (copy);
}
#endif

void yyerror (char *s)
{
	fprintf (stderr, "line %d: %s\n", yylexline, s);
}

void yyinit (FILE *fd)
{
	yyfd = fd;
}

void yyinitstr (char *ptr)
{
	yyptr = yybuf = ptr;
	yyfd = 0;
}

void newline ()
{
	int c;

	++yyline;
	c = yygetchar ();
	if (c=='\\' || c=='%' || c=='#')
		while ((c = yygetchar ()) != EOF && c!='\n')
			continue;
	if (c != EOF)
		yyungetc (c);
}

int getword (int c, char *buf, int maxlen)
{
	int len;

	for (len=0; len<maxlen-1 && isletter (c); ++len) {
		buf[len] = c;
		c = yygetchar ();
	}
	buf[len] = 0;
	yyungetc (c);
	return (len);
}

char *strjoin (char *a, char *b)
{
	int alen = strlen (a);
	int blen = strlen (b);
	char *c = malloc (alen + blen + 1);

	if (! c) {
		yyerror ("out of memory");
		exit (-1);
	}
	strcpy (c, a);
	strcpy (c+alen, b);
	free (a);
	free (b);
	return (c);
}

char *getvar ()
{
	int c, len;
	char name [32], *val;

	c = yygetchar ();
	if (! isletter (c)) {
		yyerror ("invalid variable");
		exit (-1);
	}
	len = getword (c, name, sizeof (name));
	val = getenv (name);
	if (! val) {
		yyerror ("undefined variable");
		exit (-1);
	}
	return (val);
}

char *mkstring (int delim)
{
	char *p;

	for (p=lexbuf; ; p++) {
		int c = yygetchar ();

		switch (c) {
		case '\n':
		case EOF:
			yyerror ("unterminated string");
			exit (-1);
		default:
			if (c == delim) {
				*p = 0;
				return (strdup (lexbuf));
			}
			*p = c;
			break;
		case '@':
			strcpy (p, getvar ());
			p += strlen (p);
			break;
		case '\\':
			c = yygetchar ();
			switch (c) {
			case '\0':
				yyerror ("bad '\\' in string");
			case '\n':
				++yyline;
				--p;
				break;
			case 'n':
				*p = '\n';
				break;
			case 't':
				*p = '\t';
				break;
			case 'f':
				*p = '\f';
				break;
			case 'e':
				*p = '\33';
				break;
			case '\\':
				*p = '\\';
				break;
			case '\'':
				*p = '\'';
				break;
			default:
				*p = c;
				newline ();
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				*p = c - '0';
				c = yygetchar ();
				if (c<='7' && c>='0') {
					*p = (*p<<3) + c - '0';
					c = yygetchar ();
					if (c<='7' && c>='0') {
						*p = (*p<<3) + c - '0';
					} else
						yyungetc (c);
				} else
					yyungetc (c);
				break;
			}
			break;
		}
	}
}

char *mkquery ()
{
	char *p, *g;
	int space;

	for (; lexbuflen<sizeof(lexbuf)-1; ++lexbuflen) {
		int c = yygetchar ();
		switch (c) {
		case EOF:
			break;
		case ';':
			yyungetc (c);
			break;
		case '@':
			strcpy (&lexbuf[lexbuflen], getvar ());
			lexbuflen += strlen (&lexbuf[lexbuflen]) - 1;
			continue;
		case '\n':
			newline ();
			c = ' ';
		default:
			lexbuf[lexbuflen] = c;
			continue;
		}
		break;
	}
	lexbuf[lexbuflen] = 0;

	space = 0;
	for (p=g=lexbuf; *g; ++g) {
		switch (*g) {
		case ' ':
		case '\t':
		case '\n':
			space = 1;
			continue;
		}
		if (space) {
			if (p != lexbuf)
				*p++ = ' ';
			space = 0;
		}
		*p++ = *g;
	}
	*p = 0;
	return (strdup (lexbuf));
}

int findkey ()
{
	int n;

	for (n=0; keytab[n].name; ++n)
		if (!strcmp (keytab[n].name, lexbuf))
			return (keytab[n].key);
	return (0);
}

int yylex ()
{
	int c;

	if (! yyline)
		newline ();
	yylexline = yyline;
	for (;;) {
		c = yygetchar ();
		switch (c) {
		case EOF:
			return (0);
		case '\n':
			newline ();
			continue;
		case ' ':
		case '\t':
			continue;
		case '\'':
		case '"':
			yylval.string = mkstring (c);
			for (;;) {
				c = yygetchar ();
				switch (c) {
				default:
					yyungetc (c);
				case EOF:
					return (STRING);
				case '\n':
					newline ();
					continue;
				case ' ':
				case '\t':
					continue;
				case '\'':
				case '"':
					yylval.string = strjoin (yylval.string,
						mkstring (c));
					continue;
				}
			}
			/* NOTREACHED */
		case '{':
		case '}':
		case ';':
		case '-':
			return (c);
		default:
			lexbuflen = getword (c, lexbuf, sizeof (lexbuf));
			if (lexbuflen>1 && (c = findkey ()))
				return (c);
			yylval.string = mkquery ();
			return (QUERY);
		}
	}
}

node *tnode (int type, node *l, node *r)
{
	node *n;

	for (n=nodetab; n<nodetab+MAXNODES; ++n)
		if (! n->type)
			break;
	if (n > nodetab+MAXNODES)
		return (0);
	n->type = type;
	n->left = l;
	n->right = r;
	return (n);
}

char **tlist (node *n)
{
	node *ntab [MAXQUERIES], **p;
	char **ctab, **q;

	for (p=ntab; n->type==';'; ++p) {
		if (p >= ntab + MAXQUERIES - 1) {
			fprintf (stderr, "too many queries\n");
			exit (-1);
		}
		*p = n->right;
		n = n->left;
	}
	*p = n;
	q = ctab = (char**) malloc ((p-ntab+2) * sizeof (char*));
	if (! ctab) {
		fprintf (stderr, "out of memory\n");
		exit (-1);
	}
	while (p >= ntab)
		*q++ = (char *) (*p--)->left;
	*q = 0;
	return (ctab);
}

void texec (node *n)
{
	char *filename, *format = 0;

	switch (n->type) {
	case ';':
		texec (n->left);
		texec (n->right);
		break;
	case 'Q':
		Execute ((char*) n->left);
		break;
	case 'F':
		DoForAll ((char*) n->left->left, (char**) n->right);
		break;
	case 'W':
		filename = (char*) n->left;
		if (n->right->type == 'U') {
			format = (char*) n->right->left;
			n = n->right;
		}
		DoWrite (filename, format, (char*) n->right->left);
		break;
	case 'R':
		filename = (char*) n->left;
		if (n->right->type == 'U') {
			format = (char*) n->right->left;
			n = n->right;
		}
		DoRead (filename, format, (char**) n->right);
		break;
	}
}

#if 0
void tprint (node *n, int indent)
{
	switch (n->type) {
	default:
		if (indent)
			printf ("\t");
		printf ("?%d?\n", n->type);
		break;
	case 'F':
		printf ("FORALL ");
		tprint (n->left, indent);
		printf ("{\n");
		tprint (n->right, 1);
		printf ("}\n");
		break;
	case 'W':
		printf ("PUT '%s' ", (char*) n->left);
		n = n->right;
		if (n->type == 'U') {
			printf ("USING '%s' ", (char*) n->left);
			n = n->right;
		}
		printf ("\n");
		tprint (n, 1);
		break;
	case 'R':
		printf ("WITH '%s'", (char*) n->left);
		n = n->right;
		if (n->type == 'U') {
			printf (" USING '%s'", (char*) n->left);
			n = n->right;
		}
		printf ("\n");
		printf ("{\n");
		tprint (n, 1);
		printf ("}\n");
		break;
	case 'Q':
		if (indent)
			printf ("\t");
		printf ("%s;\n", (char*) n->left);
		break;
	case ';':
		tprint (n->left, indent);
		tprint (n->right, indent);
		break;
	}
}
#endif
