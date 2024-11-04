#define cntrl(c) ((c) & 037)
#define meta(c) ((c) | 0400)

typedef short *ScreenPtr;

class Box {
	friend class Screen;
private:
	short y, x;
	short ny, nx;
	short *mem;
public:
	Box (Screen &scr, int by, int bx, int bny, int bnx);
	~Box () { if (mem) delete mem; }

};

class Cursor {
	friend class Screen;
private:
	short y, x;
public:
	Cursor (Screen &scr);
};

struct Keytab {
	char *tcap;
	char *str;
	short val;
};

class Screen {
	friend class Cursor;
	friend class Box;

private:
	int cury, curx;
	unsigned flgs;
	int clear;
	ScreenPtr *scr;
	short *firstch;
	short *lastch;
	short *lnum;

	int oldy, oldx;
	unsigned oldflgs;
	short **oldscr;

	int Visuals;                    // Маска видео возможностей дисплея
	int VisualMask;                 // Маска разрешенных атрибутов

	int NormalAttr;
	int ClearAttr;
	const int GraphAttr     = 0x8000;

	const int VisualBold    = 1;    // Есть повышенная яркость (md)
	const int VisualDim     = 2;    // Есть пониженная яркость (mh)
	const int VisualInverse = 4;    // Есть инверсия (so или mr)
	const int VisualColors  = 8;    // Есть цвета (NF, NB, CF, CB, C2, MF, MB)
	const int VisualGraph   = 16;   // Есть псевдографика (G1, G2, GT)

	int UpperCaseMode;

	int scrool, rscrool;
	int beepflag;
	int hidden;

	short ctab [16], btab [16];
	char outbuf [256], *outptr;

	unsigned char linedraw [11];

	#ifdef CYRILLIC
	unsigned char CyrInputTable ['~' - ' ' + 1];
	char CyrOutputTable [64];
	int cyrinput;
	int cyroutput;
	void cyr (int cflag);
	#endif

	struct TtyPrivate *ttydata;
	struct KeyPrivate *keydata;

	friend void tstp ();

	int Init ();
	void Open ();
	void Flush ();

	void InitKey (Keytab *keymap=0);

	int InitCap (char *buf);
	void GetCap (struct Captab *tab);
	char *Goto (char *movestr, int x, int y);

	void SetTty ();
	void ResetTty ();
	void FlushTtyInput ();
	int InputChar ();

	void setAttr (int attr);
	void setColor (int fg, int bg);
	void moveTo (int y, int x);
	void pokeChar (int y, int x, int c);
	void putChar (unsigned char c);
	int putRawChar (unsigned char c);
	void putStr (char *s);
	void clearScreen ();
	char *skipDelay (char *str);
	void scroolScreen ();
	void initChoice (int row, int col, char *c1, char *c2, char *c3);
	int menuChoice (int c, int i);
	char *editString (int r, int c, int w, char *str, int cp, int color);
	int inputKey (struct Keytab *kp);

public:
	int Lines;
	int Columns;

	const int ULC           = '\20';
	const int UT            = '\21';
	const int URC           = '\22';
	const int CLT           = '\23';
	const int CX            = '\24';
	const int CRT           = '\25';
	const int LLC           = '\26';
	const int LT            = '\27';
	const int LRC           = '\30';
	const int VERT          = '\31';
	const int HOR           = '\32';

	Screen (int colormode=2, int graphmode=1);
	~Screen ();
	void Close ();
	void Restore ();
	void Reopen ();

	int Col () { return (curx); }
	int Row () { return (cury); }

	int HasDim ()
		{ return (Visuals & VisualMask & (VisualColors | VisualDim)); }
	int HasBold ()
		{ return (Visuals & VisualMask & (VisualColors | VisualBold)); }
	int HasInverse ()
		{ return (Visuals & VisualMask & (VisualColors | VisualInverse)); }
	int HasColors ()
		{ return (Visuals & VisualMask & VisualColors); }
	int HasGraph ()
		{ return (Visuals & VisualMask & VisualGraph); }

	void Clear (int attr=0);
	void Clear (int y, int x, int ny, int nx, int attr=0, int sym=' ');
	void ClearLine (int y, int x, int attr=0);
	void ClearLine (int attr)
		{ ClearLine (cury, curx, attr); }

	// It is possible to set current column
	// to the left from the left margin
	// or to the right of the right margin.
	// Printed text will be clipped properly.
	void Move (int y, int x)
		{ curx = x; cury = y; hidden = 0; }

	void Put (int c, int attr=0);
	void Put (Box &box, int attr=0);
	void Put (char *str, int attr=0)
		{ while (*str) Put (*str++, attr); }

	void Put (int y, int x, int ch, int attr=0)
		{ Move (y, x); Put (ch, attr); }
	void Put (int y, int x, char *str, int attr=0)
		{ Move (y, x); Put (str, attr); }
	void Put (int y, int x, Box &box, int attr=0)
		{ Move (y, x); Put (box, attr); }
	void PutLimited (char *str, int lim, int attr=0)
		{ while (--lim>=0 && *str) Put (*str++, attr); }
	void PutLimited (int y, int x, char *str, int lim, int attr=0)
		{ Move (y, x); while (--lim>=0 && *str) Put (*str++, attr); }

	void Print (int attr, char *fmt, ...);
	void Print (int y, int x, int attr, char *fmt, ...);
	void PrintVect (int attr, char *fmt, void *vect);
	void PrintVect (int y, int x, int attr, char *fmt, void *vect)
		{ Move (y, x); PrintVect (attr, fmt, vect); }

	int GetChar (int y, int x) { return scr[y][x]; }
	int GetChar () { return GetChar (cury, curx); }

	int GetKey ();
	void UngetKey (int key);
	void AddHotKey (int key, void (*f) (int));
	void DelHotKey (int key);

	void SetCursor (Cursor c) { Move (c.y, c.x); }
	void HideCursor () { hidden = 1; }

	void Sync (int y);
	void Sync ();
	void Redraw () { clearScreen (); Sync (); }
	void Beep () { beepflag = 1; }

	void DelLine (int line, int attr=0);
	void InsLine (int line, int attr=0);

	void Error (int c, int i, char *head, char *reply, char *s, ...);
	int Popup (char *head, char *mesg, char *mesg2,
		char *c1, char *c2, char *c3, int c, int i);
	char *GetString (int w, char *str, char *head, char *mesg,
		int c, int i);

	void HorLine (int y, int x, int nx, int attr=0);
	void VertLine (int x, int y, int ny, int attr=0);
	void DrawFrame (int y, int x, int ny, int nx, int attr=0);

	void AttrSet (int y, int x, int ny, int nx, int attr=0);
	void AttrLow (int y, int x);
};

inline Cursor::Cursor (Screen &scr) { y = scr.cury; x = scr.curx; }
