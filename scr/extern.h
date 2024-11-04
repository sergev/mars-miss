extern "C" {
	#include <stdlib.h>
#ifdef ISC
	int memcmp (const void *, const void *, int);
	void *memcpy (void *, const void *, int);
	int strcmp (const char *a, const char *b);
	char *strchr (const char *a, int);
	int access (const char *, int);
	char *strcpy (char *, const char *);
	char *strncpy (char *, const char *, int);
	int strlen (const char *);
#else
#ifndef unix
	int memcmp (void *, void *, int);
	void *memcpy (void *, void *, int);
	int strcmp (char *a, char *b);
	char *strcpy (char *, char *);
	int strlen (char *);
#endif
	int write (int, char *, int);
	char *strchr (char *a, int);
	int access (char *, int);
	char *strncpy (char *, char *, int);
	int open (char *, int);
	int read (int, char *, int);
	int vsprintf (char *, const char *, char *);
	int sprintf (char *, const char *, ...);
#endif
//	int atoi (const char *str);
	void *memset (void *, int, int);
	void exit (int rezult);
	void outerr (char *str);
	char *getenv (const char *str);
	int close (int fd);
	int isatty (int fd);
	unsigned alarm (unsigned timo);
	void *realloc (void *buf, unsigned len);
};

inline void outerr (char *str) { write (2, str, strlen (str)); }

inline char *strdup (char *str)
{
	int len = strlen (str) + 1;
	char *copy = new char [len];
	if (copy)
		memcpy (copy, str, len);
	return (copy);
}

inline int lowercase (int c)
{
	if (c>='A' && c<='Z')
		return (c+'a'-'A');
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
	return (c);
}
