#define cntrl(c) ((c) & 037)
#define meta(c) ((c) | 0400)

typedef short far *ScreenPtr;

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

class Screen {
	friend class Cursor;
	friend class Box;
public:
	int Lines;
	int Columns;

	int ULC, UT, URC, CLT, CX, CRT, LLC, LT, LRC, VERT, HOR;

	Screen (int colormode=2, int graphmode=1);
	~Screen ();
	void Close ();
	void Restore () {}
	void Reopen () {}

	int Col () { return curx; }
	int Row () { return cury; }

	int HasDim ()  { return 1; }
	int HasBold ()  { return 1; }
	int HasInverse () { return 1; }
	int HasColors () { return ColorMode; }
	int HasGraph () { return 1; }

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
	void Put (Box &box, int ca=0);
	void Put (char *str, int attr=0)
		{ while (*str) Put (*str++, attr); }

	void Put (int y, int x, int ch, int attr=0)
		{ Move (y, x); Put (ch, attr); }
	void Put (int y, int x, char *str, int attr=0)
		{ Move (y, x); Put (str, attr); }
	void Put (int y, int x, Box &box, int ca=0)
		{ Move (y, x); Put (box, ca); }
	void PutLimited (char *str, int lim, int attr=0)
		{ while (--lim>=0 && *str) Put (*str++, attr); }
	void PutLimited (int y, int x, char *str, int lim, int attr=0)
		{ Move (y, x); while (--lim>=0 && *str) Put (*str++, attr); }

	void Print (int attr, char *fmt, ...);
	void Print (int y, int x, int attr, char *fmt, ...);
	void PrintVect (int attr, char *fmt, void *vect);
	void PrintVect (int y, int x, int attr, char *fmt, void *vect)
		{ Move (y, x); PrintVect (attr, fmt, vect); }

	int GetChar () { return GetChar (cury, curx); }
	int GetChar (int y, int x) { return scr[y][x]; }

	int GetKey ();
	void UngetKey (int key);
	void AddHotKey (int key, void (*f) (int));
	void DelHotKey (int key);

	void SetCursor (Cursor c) { Move (c.y, c.x); }
	void HideCursor () { hidden = 1; }

	void Sync (int y) { Sync (); }
	void Sync ();
	void Redraw ()    {}
	void Beep ();

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

private:
	struct callBack {
		int key;
		void (*func) (int);
	};

	short far **scr;
	int curx, cury;
	int flgs;
	int ColorMode;
	int NormalAttr;
	int hidden;
	int ungetkey, ungetflag;
	struct callBack callBackTable[20];
	int ncallb;

	int Init ();
	void Open ();

	void initChoice (int row, int col, char *c1, char *c2, char *c3);
	int menuChoice (int c, int i);
	char *editString (int r, int c, int w, char *str, int cp, int color);

	void pokeChar (int y, int x, int v) { scr[y][x] = v; }
};
