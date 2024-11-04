#include "Screen.h"
#include "Menu.h"
#include "extern.h"

static void displayName (Screen *scr, char *name, int normal, int bold)
{
	for (char *p=name; *p; ++p)
		if (*p == '~') {
			scr->Put (*++p, bold);
		} else
			scr->Put (*p, normal);
}

Menu::Menu (int c, int l, int d, int i, int li)
{
	menusz = nmenus = columnsz = 0;
	color = c;
	light = l;
	dim = d;
	inverse = i;
	lightinverse = li;
}

Menu::Menu ()
{
	menusz = nmenus = columnsz = 0;
	color = 0x70;
	light = 0xf0;
	dim = 0x30;
	inverse = 0x07;
	lightinverse = 0x0f;
}

void Menu::Palette (int c, int l, int d, int i, int li)
{
	color = c;
	light = l;
	dim = d;
	inverse = i;
	lightinverse = li;
}

Menu::~Menu ()
{
	if (menusz)
		delete menu;
	if (columnsz)
		delete column;
}

SubMenu *Menu::Add (char *name)
{
	if (! menusz) {
		menusz = 8;
		menu = new SubMenu* [menusz];
		if (! menu)
			return (0);
	}
	if (nmenus >= menusz) {
		menusz += 8;
		menu = (SubMenu**) realloc (menu, menusz * sizeof (menu[0]));
		if (! menu) {
			menusz = nmenus = 0;
			return (0);
		}
	}
	menu [nmenus] = new SubMenu (name);
	if (! menu [nmenus])
		return (0);
	return (menu [nmenus++]);
}

void Menu::Display (Screen *scr, int row)
{
	if (! nmenus)
		return;
	for (int i=0; i<nmenus; ++i)
		menu[i]->Init ();
	if (! columnsz)
		column = new int [columnsz = menusz];
	else if (columnsz < menusz)
		column = (int*) realloc (column, (columnsz = menusz) * sizeof (int));
	if (! column) {
		columnsz = 0;
		return;
	}
	column[0] = 1;
	for (int i=1; i<nmenus; ++i)
		column[i] = column[i-1] + menu[i-1]->NameLength();
	scr->ClearLine (row, 0, color);
	for (int i=0; i<nmenus; ++i) {
		scr->Put (row, column[i], ' ', color);
		displayName (scr, menu[i]->Name(), color, light);
		scr->Put (' ', color);
	}
}

void Menu::Run (Screen *scr, int row, int key, int subkey)
{
	if (! nmenus)
		return;
	Display (scr, row);
	current = 0;
	for (int i=0; i<nmenus; ++i)
		if (menu[i]->HotKey () == key) {
			current = i;
			menu[i]->SetKey (subkey);
		}
	for (;;) {
		scr->Put (row, column[current], ' ', inverse);
		displayName (scr, menu[current]->Name(), inverse, lightinverse);
		scr->Put (' ', inverse);
		menu[current]->Run (scr, row+1, column[current],
			color, inverse, dim, light, lightinverse);
		int k = scr->GetKey ();
		scr->Put (row, column[current], ' ', color);
		displayName (scr, menu[current]->Name(), color, light);
		scr->Put (' ', color);
		switch (k) {
		case cntrl ('M'):       // Enter
		case cntrl ('C'):       // ^C
		case cntrl ('['):       // Esc
		case meta ('J'):        // F10 - exit from menu
			return;
		case cntrl ('J'):       // Line feed - stay in menu
		default:
			break;
		case meta ('r'):        // right
			if (++current >= nmenus)
				current = 0;
			break;
		case meta ('l'):        // left
			if (--current < 0)
				current = nmenus - 1;
			break;
		}
	}
}

SubMenu::SubMenu (char *n)
{
	name = n;
	namelen = strlen (name) + 2;
	hotkey = 0;
	char *p = strchr (name, '~');
	if (p) {
		hotkey = lowercase (p[1]);
		--namelen;
	}
	height = 2;
	width = 2;
	current = 0;
	nitems = itemsz = 0;
}

SubMenu::~SubMenu ()
{
	if (itemsz)
		delete item;
}

MenuItem *SubMenu::Add (char *name, MenuFunction callback)
{
	if (! itemsz) {
		itemsz = 8;
		item = new MenuItem* [itemsz];
		if (! item)
			return (0);
	}
	if (nitems >= itemsz) {
		itemsz += 8;
		item = (MenuItem**) realloc (item, itemsz * sizeof (item[0]));
		if (! item) {
			itemsz = nitems = 0;
			return (0);
		}
	}
	item [nitems] = new MenuItem (name, callback);
	if (! item [nitems])
		return (0);
	return (item [nitems++]);
}

void SubMenu::SetKey (int key)
{
	current = 0;
	for (int i=0; i<nitems; ++i)
		if (item[i]->HotKey () == key)
			current = i;
}

void SubMenu::Init ()
{
	current = 0;
	height = nitems + 2;
	width = 0;
	for (int i=0; i<nitems; ++i)
		if (item[i]->Width () > width)
			width = item[i]->Width ();
	width += 2;
}

MenuItem::MenuItem (char *n, MenuFunction cb)
{
	name = n;
	callback = cb;
	isactive = 1;
	istagged = 0;
	width = strlen (name) + 4;
	hotkey = 0;
	char *p = strchr (name, '~');
	if (p) {
		hotkey = lowercase (p[1]);
		--width;
	}
}

void SubMenu::Run (Screen *scr, int row, int col,
	int color, int inverse, int dim, int light, int lightinverse)
{
	int i;

	Box box (*scr, row, col, height+1, width+1);
	for (;;) {
		Draw (scr, row, col, color, inverse, dim, light, lightinverse);
		scr->HideCursor ();
		scr->Sync ();
		int k = scr->GetKey ();
		switch (k) {
		default:
			for (i=0; i<nitems; ++i)
				if (item[i]->HotKey() == k) {
					current = i;
					break;
				}
			if (i >= nitems) {
				scr->Beep ();
				continue;
			}
			k = cntrl ('M');
			// fall through
		case cntrl ('M'):               // ^M - execute, exit
		case cntrl ('J'):               // ^J - execute, stay here
			item[current]->CallBack ();
			// fall through
		case cntrl ('C'):               // ^C - exit
		case cntrl ('['):               // Esc - exit
		case meta ('J'):                // F10 - exit
		case meta ('r'):                // right - move
		case meta ('l'):                // left - move
			scr->UngetKey (k);
			scr->Put (box);
			return;
		case meta ('u'):                // up
			--current;
			while (current>=0 && ! item[current]->IsActive())
				--current;
			if (current < 0) {
				current = nitems-1;
				while (current>0 && ! item[current]->IsActive())
					--current;
			}
			continue;
		case meta ('d'):                // down
			++current;
			while (current<nitems && ! item[current]->IsActive())
				++current;
			if (current >= nitems) {
				current = 0;
				while (current<nitems && ! item[current]->IsActive())
					++current;
				if (current >= nitems)
					current = 0;
			}
			continue;
		}
	}
}

void SubMenu::Draw (Screen *scr, int row, int col,
	int color, int inverse, int dim, int light, int lightinverse)
{
	scr->Clear (row, col, height, width, color, ' ');
	scr->DrawFrame (row, col, height, width, color);
	for (int i=0; i<nitems; ++i) {
		scr->Move (row+1+i, col+1);
		item[i]->Display (scr, col+width-1, i==current,
			color, inverse, dim, light, lightinverse);
	}

	// Draw shadow.
	for (int i=0; i<width; ++i)
		scr->AttrLow (row+height, col+i+1);
	for (int i=0; i<height; ++i)
		scr->AttrLow (row+i+1, col+width);
}

void MenuItem::Display (Screen *scr, int lim, int cur,
	int color, int inverse, int dim, int light, int lightinverse)
{
	if (cur) {
		scr->Put (!scr->HasInverse () ? '[' : ' ', inverse);
		scr->Put (istagged ? "* " : "  ", inverse);
		displayName (scr, name, inverse, lightinverse);
		while (scr->Col() < lim-1)
			scr->Put (' ', inverse);
		scr->Put (!scr->HasInverse () ? ']' : ' ', inverse);
	} else if (isactive) {
		scr->Put (istagged ? " * " : "   ", color);
		displayName (scr, name, color, light);
		while (scr->Col() < lim)
			scr->Put (' ', color);
	} else {
		scr->Put (istagged ? " * " : "   ", dim);
		displayName (scr, name, dim, dim);
		while (scr->Col() < lim)
			scr->Put (' ', dim);
	}
}
