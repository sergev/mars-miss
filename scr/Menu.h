typedef void (*MenuFunction) ();

class MenuItem {
private:
	char *name;                     // name of submenu
	int width;                      // length of name
	int hotkey;                     // return key
	int isactive;                   // is it active for now
	int istagged;                   // tag of submenu
	MenuFunction callback;          // function to execute
public:
	MenuItem (char *name, MenuFunction callback);
	void Display (Screen *scr, int lim, int cur, int c, int i, int d, int l, int li);
	void Activate (int yes=1) { istagged = yes; }
	void Deactivate () { Activate (); }
	void Tag (int yes=1) { istagged = yes; }
	void Untag () { Tag (0); }
	int HotKey () { return hotkey; }
	int Width () { return width; }
	int IsActive () { return isactive; }
	void CallBack () { callback (); }
};

class SubMenu {
private:
	char *name;                     // name of menu
	int namelen;                    // length of name
	int hotkey;                     // return key
	int height;                     // height of submenu window
	int width;                      // width of submenu window
	int current;                    // current submenu
	MenuItem **item;                // array of submenus
	int nitems;
	int itemsz;
public:
	SubMenu (char *name);
	~SubMenu ();
	MenuItem *Add (char *name, MenuFunction callback);
	void Init ();
	void SetKey (int key);
	void Draw (Screen *scr, int row, int col, int c, int i, int d, int l, int li);
	void Run (Screen *scr, int row, int col, int c, int i, int d, int l, int li);
	char *Name () { return name; }
	int NameLength () { return namelen; }
	int HotKey () { return hotkey; }
};

class Menu {
private:
	int nmenus;             // number of submenus
	int menusz;             // allocated size of submenu array
	int current;            // current submenu
	SubMenu **menu;         // array of submenus
	int *column;            // base columns of submenus
	int columnsz;           // allocated size of column array
	int color;
	int light;
	int inverse;
	int lightinverse;
	int dim;
public:
	Menu ();
	Menu (int c, int l, int d, int i, int li);
	~Menu ();
	void Palette (int c, int l, int i, int li, int di);
	SubMenu *Add (char *name);
	void Display (Screen *scr, int row);
	void Run (Screen *scr, int row, int key=-1, int subkey=-1);
};
