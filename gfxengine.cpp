/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	Jacob's Matrix Development
 *
 * NAME:        gfxengine.h ( Jacob's Matrix, C++ )
 *
 * COMMENTS:
 * 	This is a very light wrapper around libtcod.
 */

#include <libtcod.hpp>
#undef CLAMP
#include "gfxengine.h"
#include "gamedef.h"

#if defined(_WIN32)
#include <windows.h>
#include <winuser.h>
#endif

extern void redrawWorldNoFlush();

TCODNoise		*glbPulseNoise = 0;
float			 glbPulseVal = 1.0f;

//#include <SDL.h>
#if defined(_WIN32)
//#include "SDL_syswm.h"
#endif

//SDL_Joystick		*glbJoyStick = 0;
int			 glbJoyStickNum = 0;
int			 glbJoyDelay = 100, glbJoyRepeat = 60, glbButtonRepeat = 300;

int
cookKey(TCOD_key_t key)
{
    switch (key.vk)
    {
	case TCODK_CHAR:
	{
	    // Sadly does not include shift...
	    if (key.c >= 'a' && key.c <= 'z' && key.shift)
	    {
		key.c += 'A' - 'a';
	    }
	    // Manual remap, not i18n friendly.
	    if (key.shift)
	    {
		switch (key.c)
		{
		    case ',':
			return '<';
		    case '.':
			return '>';
		    case '/':
			return '?';
		    case ';':
			return ':';
		    case '\'':
			return '"';
		    case '\\':
			return '|';
		    case '-':
			return '_';
		    case '=':
			return '+';
		    case '`':
			return '~';
		    case '[':
			return '{';
		    case ']':
			return '}';
		}
	    }
	    return key.c;
	}

	case TCODK_ENTER:
	case TCODK_KPENTER:
	    return '\n';

	case TCODK_BACKSPACE:
	    return '\b';
	
	case TCODK_ESCAPE:
	    return '\x1b';

	case TCODK_TAB:
	    return '\t';

	case TCODK_SPACE:
	    return ' ';

	case TCODK_UP:
	    return !key.shift ? GFX_KEYUP : GFX_KEYUP_SHIFT;
	case TCODK_DOWN:
	    return !key.shift ? GFX_KEYDOWN : GFX_KEYDOWN_SHIFT;
	case TCODK_LEFT:
	    return !key.shift ? GFX_KEYLEFT : GFX_KEYLEFT_SHIFT;
	case TCODK_RIGHT:
	    return !key.shift ? GFX_KEYRIGHT : GFX_KEYRIGHT_SHIFT;

	case TCODK_PAGEUP:
	    return !key.shift ? GFX_KEYPAGEUP : GFX_KEYPAGEUP_SHIFT;
	case TCODK_PAGEDOWN:
	    return !key.shift ? GFX_KEYPAGEDOWN : GFX_KEYPAGEDOWN_SHIFT;
	case TCODK_HOME:
	    return !key.shift ? GFX_KEYHOME : GFX_KEYHOME_SHIFT;
	case TCODK_END:
	    return !key.shift ? GFX_KEYEND : GFX_KEYEND_SHIFT;

	case TCODK_F1:
	    return key.shift ? GFX_KEYSF1 : GFX_KEYF1;
	case TCODK_F2:
	    return key.shift ? GFX_KEYSF2 : GFX_KEYF2;
	case TCODK_F3:
	    return key.shift ? GFX_KEYSF3 : GFX_KEYF3;
	case TCODK_F4:
	    return key.shift ? GFX_KEYSF4 : GFX_KEYF4;
	case TCODK_F5:
	    return key.shift ? GFX_KEYSF5 : GFX_KEYF5;
	case TCODK_F6:
	    return key.shift ? GFX_KEYSF6 : GFX_KEYF6;
	case TCODK_F7:
	    return key.shift ? GFX_KEYSF7 : GFX_KEYF7;
	case TCODK_F8:
	    return key.shift ? GFX_KEYSF8 : GFX_KEYF8;
	case TCODK_F9:
	    return key.shift ? GFX_KEYSF9 : GFX_KEYF9;
	case TCODK_F10:
	    return key.shift ? GFX_KEYSF10 : GFX_KEYF10;
	case TCODK_F11:
	    return key.shift ? GFX_KEYSF11 : GFX_KEYF11;
	case TCODK_F12:
	    return key.shift ? GFX_KEYSF12 : GFX_KEYF12;

	case TCODK_0:
	    if (key.shift) return ')';  // Damn, very anti i18n!
	case TCODK_1:
	    if (key.shift) return '!';
	case TCODK_2:
	    if (key.shift) return '@';
	case TCODK_3:
	    if (key.shift) return '#';
	case TCODK_4:
	    if (key.shift) return '$';
	case TCODK_5:
	    if (key.shift) return '%';
	case TCODK_6:
	    if (key.shift) return '^';
	case TCODK_7:
	    if (key.shift) return '&';
	case TCODK_8:
	    if (key.shift) return '*';
	case TCODK_9:
	    if (key.shift) return '(';
	    return key.c;
	    //return '0' + key.vk - TCODK_0;

	case TCODK_KP0:
	case TCODK_KP1:
	case TCODK_KP2:
	case TCODK_KP3:
	case TCODK_KP4:
	case TCODK_KP5:
	case TCODK_KP6:
	case TCODK_KP7:
	case TCODK_KP8:
	case TCODK_KP9:
	    return '0' + key.vk - TCODK_KP0;

	case TCODK_KPADD:
	    return '+';
	case TCODK_KPSUB:
	    return '-';
	case TCODK_KPDIV:
	    return '/';
	case TCODK_KPMUL:
	    return '*';

	// To match up the keypad, hopefully.
	case TCODK_INSERT:
	    return '0';

	case TCODK_KPDEC:	// I think keypad del?
	    return '.';
    }

    return 0;
}

bool
gfx_cookDir(int &key, int &dx, int &dy, GFX_Cookdir cookdir, bool allowshift, int shiftscale)
{
    // Check diagonals only if both x & y.
    if ((cookdir & GFX_COOKDIR_X) && (cookdir & GFX_COOKDIR_Y))
    {
	switch (key)
	{
	    case GFX_KEYHOME:
	    case '7':
	    case 'y':
		dx = -1;	dy = -1;
		key = 0;
		return true;
	    case GFX_KEYHOME_SHIFT:
	    case 'Y':
		if (allowshift)
		{
		    dx = -shiftscale;	dy = -shiftscale;
		    key = 0;
		    return true;
		}
		break;
	    case GFX_KEYPAGEUP:
	    case '9':
	    case 'u':
		dx = 1;	dy = -1;
		key = 0;
		return true;
	    case GFX_KEYPAGEUP_SHIFT:
	    case 'U':
		if (allowshift)
		{
		    dx = shiftscale;	dy = -shiftscale;
		    key = 0;
		    return true;
		}
		break;
	    case GFX_KEYEND:
	    case '1':
	    case 'b':
		dx = -1;	dy = 1;
		key = 0;
		return true;
	    case GFX_KEYEND_SHIFT:
	    case 'B':
		if (allowshift)
		{
		    dx = -shiftscale;	dy = shiftscale;
		    key = 0;
		    return true;
		}
		break;
	    case GFX_KEYPAGEDOWN:
	    case '3':
	    case 'n':
		dx = 1;	dy = 1;
		key = 0;
		return true;
	    case GFX_KEYPAGEDOWN_SHIFT:
	    case 'N':
		if (allowshift)
		{
		    dx = shiftscale;	dy = shiftscale;
		    key = 0;
		    return true;
		}
		break;
	}
    }

    // Check left/right
    if (cookdir & GFX_COOKDIR_X)
    {
	switch (key)
	{
	    case GFX_KEYLEFT:
	    case '4':
	    case 'h':
		dx = -1;	dy = 0;
		key = 0;
		return true;
	    case GFX_KEYLEFT_SHIFT:
	    case 'H':
		if (allowshift)
		{
		    dx = -shiftscale;	dy = 0;
		    key = 0;
		    return true;
		}
		break;
	    case GFX_KEYRIGHT:
	    case '6':
	    case 'l':
		dx = 1;	dy = 0;
		key = 0;
		return true;
	    case GFX_KEYRIGHT_SHIFT:
	    case 'L':
		if (allowshift)
		{
		    dx = shiftscale;	dy = 0;
		    key = 0;
		    return true;
		}
		break;
	}
    }

    // Check up/down
    if (cookdir & GFX_COOKDIR_Y)
    {
	switch (key)
	{
	    case GFX_KEYDOWN:
	    case '2':
	    case 'j':
		dx = 0;	dy = 1;
		key = 0;
		return true;
	    case GFX_KEYDOWN_SHIFT:
	    case 'J':
		if (allowshift)
		{
		    dx = 0;	dy = shiftscale;
		    key = 0;
		    return true;
		}
		break;
	    case GFX_KEYUP:
	    case '8':
	    case 'k':
		dx = 0;	dy = -1;
		key = 0;
		return true;
	    case GFX_KEYUP_SHIFT:
	    case 'K':
		if (allowshift)
		{
		    dx = 0;	dy = -shiftscale;
		    key = 0;
		    return true;
		}
		break;
	}
    }

    // Check stationary
    if (cookdir & GFX_COOKDIR_STATIONARY)
    {
	switch (key)
	{
	    case '5':
	    case '.':
	    case ' ':
		dx = 0;	dy = 0;
		key = 0;
		return true;
	}
    }

    return false;
}

int 
cookJoyKeys()
{
    return 0;
#if 0
    if (!glbJoyStick)
	return 0;
    SDL_JoystickUpdate();

    int		xstate, ystate;
    int		key = 0;
    int		nbutton = SDL_JoystickNumButtons(glbJoyStick);
    int		button;

    if (nbutton > 16)
	nbutton = 16;

    static int	olddir = 4;
    int		dir = 0;
    static int	dirstartms;
    static bool dirrepeating;

    static bool buttonrepeating[16];
    static bool buttonold[16];
    bool 	buttonstate[16];
    static int  buttonstartms[16];

    const int	keymapping[9] =
    { GFX_KEYHOME, GFX_KEYUP, GFX_KEYPAGEUP,
      GFX_KEYLEFT, 0, GFX_KEYRIGHT,
      GFX_KEYEND, GFX_KEYDOWN, GFX_KEYPAGEDOWN,
    };

    const int buttonmapping[16] =
    {
	'f', ' ', '+', '-',
	'/', '*', '/', '*',
	'v', 'O', 0, 0,
	0, 0, 0, 0
    };

    // Transform the axis direction into a 9-way direction, y high trit.

    xstate = SDL_JoystickGetAxis(glbJoyStick, 0);
    ystate = SDL_JoystickGetAxis(glbJoyStick, 1);

    if (xstate > 16384)
	dir += 2;
    else if (xstate > -16384)
	dir++;

    if (ystate > 16384)
	dir += 6;
    else if (ystate > -16384)
	dir += 3;

    // Check for key presses.
    if (dir != olddir)
    {
	olddir = dir;
	key = keymapping[dir];
	dirstartms = (int)TCOD_sys_elapsed_milli();
	dirrepeating = false;
    }
    else
    {
	int	delay = dirrepeating ? glbJoyRepeat : glbJoyDelay;

	if (((int) TCOD_sys_elapsed_milli() - dirstartms) > delay)
	{
	    key = keymapping[dir];
	    dirstartms = (int)TCOD_sys_elapsed_milli();
	    dirrepeating = true;
	}
    }

    if (key)
	return key;

    for (button = 0; button < nbutton; button++)
    {
	buttonstate[button] = SDL_JoystickGetButton(glbJoyStick, button) ? true : false;
	if (buttonstate[button] != buttonold[button])
	{
	    buttonold[button] = buttonstate[button];
	    if (buttonstate[button])
	    {
		buttonstartms[button] = (int) TCOD_sys_elapsed_milli();
		buttonrepeating[button] = false;
		key = buttonmapping[button];
	    }
	}
	else if (buttonstate[button])
	{
	    int		delay = glbButtonRepeat;

	    if (((int) TCOD_sys_elapsed_milli() - buttonstartms[button]) > delay)
	    {
		key = buttonmapping[dir];
		buttonstartms[button] = (int)TCOD_sys_elapsed_milli();
		buttonrepeating[button] = true;
	    }
	}

	if (key)
	    return key;
    }

    return key;
#endif
}

void
gfx_clearKeyBuf()
{
    // This is a nightmare.  There is no way to flush the damn
    // keyboard buffer!
    int		nullcount = 1;
    while (nullcount > 0)
    {
	if (gfx_getKey(false))
	{
	    nullcount = 1;
	}
	else
	{
	    nullcount --;
	}
    }
}

int
gfx_getKey(bool block)
{
    int		key = 0;
    TCOD_key_t  tkey;

    do
    {
	TCODConsole::flush();
	tkey = TCODConsole::checkForKeypress(TCOD_KEY_PRESSED);
	key = cookKey(tkey);
	if (!key)
	{
	    key = cookJoyKeys();
	}
    } while (block && !key);

    return key;
}

void
gfx_update()
{
    TCODConsole::flush();
}

void
gfx_init()
{
    glbPulseNoise = new TCODNoise(1, 0.5, 2.0);
#if 0
    glbJoyStick = 0;

    if (SDL_NumJoysticks() > glbJoyStickNum)
    {
	glbJoyStick = SDL_JoystickOpen(glbJoyStickNum);

	if (glbJoyStick)
	{
	    // printf("Opened joystick %s\n", SDL_JoystickName(glbJoyStickNum));
	}
	else
	{
	    printf("Failed to open joystick %d!\n", glbJoyStickNum);
	}
    }
#endif

#if defined(_WIN32) && defined(GCL_HICON)
    HINSTANCE handle = ::GetModuleHandle(nullptr);
    HICON icon = ::LoadIcon(handle, MAKEINTRESOURCEA(101));
    if(icon != nullptr)
    {
	HWND hwnd = ::GetActiveWindow();
	::SetClassLong(hwnd, GCL_HICON, reinterpret_cast<LONG>(icon));
    }
#endif
}

void
gfx_shutdown()
{
#if 0
    if (SDL_JoystickOpened(glbJoyStickNum))
	SDL_JoystickClose(glbJoyStick);
#endif
}

void
gfx_updatepulsetime()
{
    int 	timems = TCOD_sys_elapsed_milli();
    float	t;

    // Wrap every minute to keep noise in happy space.
    timems %= 60 * 1000;

    t = timems / 1000.0F;

    glbPulseVal = glbPulseNoise->getTurbulence(&t, 6, TCOD_NOISE_WAVELET) * 2 + 1.0f;
}


void 
gfx_printchar(int x, int y, u8 c, ATTR_NAMES attr)
{
    if (x < 0 || y < 0) return;
    if (x >= SCR_WIDTH || y >= SCR_HEIGHT) return;

    const ATTR_DEF	*def = GAMEDEF::attrdef(attr);

    TCODColor		fg(def->fg_r,
			   def->fg_g,
			   def->fg_b);

    if (def->pulse)
	fg = fg * glbPulseVal;

    TCODConsole::root->setCharBackground(x, y, TCODColor(def->bg_r,
					       def->bg_g,
					       def->bg_b));
    TCODConsole::root->setCharForeground(x, y, fg);
    TCODConsole::root->setChar(x, y, c);
}

void 
gfx_printattr(int x, int y, ATTR_NAMES attr)
{
    if (x < 0 || y < 0) return;
    if (x >= SCR_WIDTH || y >= SCR_HEIGHT) return;

    const ATTR_DEF	*def = GAMEDEF::attrdef(attr);
    TCODColor		fg(def->fg_r,
			   def->fg_g,
			   def->fg_b);

    if (def->pulse)
	fg = fg * glbPulseVal;

    TCODConsole::root->setCharBackground(x, y, TCODColor(def->bg_r,
					       def->bg_g,
					       def->bg_b));
    TCODConsole::root->setCharForeground(x, y, fg);
}

void 
gfx_printattrback(int x, int y, ATTR_NAMES attr)
{
    if (x < 0 || y < 0) return;
    if (x >= SCR_WIDTH || y >= SCR_HEIGHT) return;

    const ATTR_DEF	*def = GAMEDEF::attrdef(attr);
    TCODConsole::root->setCharBackground(x, y, TCODColor(def->bg_r,
					       def->bg_g,
					       def->bg_b));
}

void 
gfx_printattrfore(int x, int y, ATTR_NAMES attr)
{
    if (x < 0 || y < 0) return;
    if (x >= SCR_WIDTH || y >= SCR_HEIGHT) return;

    const ATTR_DEF	*def = GAMEDEF::attrdef(attr);
    TCODColor		fg(def->fg_r,
			   def->fg_g,
			   def->fg_b);

    if (def->pulse)
	fg = fg * glbPulseVal;

    TCODConsole::root->setCharForeground(x, y, fg);
}

void 
gfx_printchar(int x, int y, u8 c, u8 r, u8 g, u8 b)
{
    if (x < 0 || y < 0) return;
    if (x >= SCR_WIDTH || y >= SCR_HEIGHT) return;

    TCODConsole::root->setCharBackground(x, y, TCODColor(0, 0, 0));
    TCODConsole::root->setCharForeground(x, y, TCODColor(r, g, b));
    TCODConsole::root->setChar(x, y, c);
}

int glbFakeTerminalHeader = 0;

void
gfx_faketerminal_setheader(int header)
{
    glbFakeTerminalHeader = header;
}

int glbFakeTerminalCursorX = 0;

void
gfx_delay(int waitms)
{
    int		startms = (int) TCOD_sys_elapsed_milli();

    while ( (int) (TCOD_sys_elapsed_milli()) - startms < waitms)
    {
    }
}

void
gfx_faketerminal_scroll()
{
    for (int y = glbFakeTerminalHeader; y < SCR_HEIGHT-1; y++)
    {
	// Zero last line
	for (int x = 0; x < SCR_WIDTH; x++)
	{
	    TCODColor		bg = TCODConsole::root->getCharBackground(x, y+1);
	    TCODColor		fg = TCODConsole::root->getCharForeground(x, y+1);
	    u8			c = TCODConsole::root->getChar(x, y+1);

	    TCODConsole::root->setCharBackground(x, y, bg);
	    TCODConsole::root->setCharForeground(x, y, fg);
	    TCODConsole::root->setChar(x, y, c);
	}
    }
    // Zero last line
    for (int x = 0; x < SCR_WIDTH; x++)
    {
	gfx_printchar(x, SCR_HEIGHT-1, ' ', ATTR_TERMINAL);
    }
    TCODConsole::flush();
}

void
gfx_faketerminal_write(const char *text)
{
    const char *start = text;
    while (*text)
    {
	if (*text == '\n')
	{
	    gfx_faketerminal_scroll();
	    glbFakeTerminalCursorX = 0;
	}
	else
	{
	    if (glbFakeTerminalCursorX == SCR_WIDTH)
	    {
		// Overflow!
		// If spaces, eat and scroll.
		if (*text == ' ')
		{
		    gfx_faketerminal_scroll();
		    glbFakeTerminalCursorX = 0;
		    while (text[1] == ' ')
			text++;
		}
		else
		{
		    // CHeck for last word and remove.
		    const char *wordstart = text;
		    while (wordstart > start && *wordstart != ' ')
			wordstart--;
		    if (*wordstart == ' ')
		    {
			// erase...
			while (text > wordstart)
			{
			    gfx_printchar(--glbFakeTerminalCursorX, SCR_HEIGHT-1, ' ', ATTR_TERMINAL);
			    TCODConsole::flush();
			    gfx_delay(1); // Fast deletion...
			    text--;
			}
			// Now eat all spaces.
			while (*text == ' ')
			    text++;
			// And continue next line...
			gfx_faketerminal_scroll();
			glbFakeTerminalCursorX = 0;
			gfx_printchar(glbFakeTerminalCursorX++, SCR_HEIGHT-1, *text, ATTR_TERMINAL);
		    }
		    else
		    {
			// Failed, just wrap.
			gfx_faketerminal_scroll();
			glbFakeTerminalCursorX = 0;
			gfx_printchar(glbFakeTerminalCursorX++, SCR_HEIGHT-1, *text, ATTR_TERMINAL);
		    }
		}
	    }
	    else
	    {
		gfx_printchar(glbFakeTerminalCursorX++, SCR_HEIGHT-1, *text, ATTR_TERMINAL);
	    }
	    TCODConsole::flush();
	}
	gfx_delay(3); // About 300 baud.
	text++;
    }
}

void 
gfx_printchar(int x, int y, u8 c, u8 r, u8 g, u8 b, u8 br, u8 bg, u8 bb)
{
    if (x < 0 || y < 0) return;
    if (x >= SCR_WIDTH || y >= SCR_HEIGHT) return;

    TCODConsole::root->setCharBackground(x, y, TCODColor(br, bg, bb));
    TCODConsole::root->setCharForeground(x, y, TCODColor(r, g, b));
    TCODConsole::root->setChar(x, y, c);
}

void 
gfx_printchar(int x, int y, u8 c)
{
    if (x < 0 || y < 0) return;
    if (x >= SCR_WIDTH || y >= SCR_HEIGHT) return;

    TCODConsole::root->setChar(x, y, c);
}

void 
gfx_fadefromwhite(int x, int y, float fade)
{
    if (x < 0 || y < 0) return;
    if (x >= SCR_WIDTH || y >= SCR_HEIGHT) return;

    TCODColor		fg = TCODConsole::root->getCharForeground(x, y);
    int			grey;

    grey = (int)fg.r + fg.g + fg.b;
    grey /= 3;
    if (fade < 0.5)
    {
	fg.r = (u8)(255 * (1-fade*2) + grey * (fade*2));
	fg.g = (u8)(255 * (1-fade*2) + grey * (fade*2));
	fg.b = (u8)(255 * (1-fade*2) + grey * (fade*2));
    }
    else
    {
	fg.r = (u8)(grey * (1-(fade-0.5)*2) + fg.r * ((fade-0.5)*2));
	fg.g = (u8)(grey * (1-(fade-0.5)*2) + fg.g * ((fade-0.5)*2));
	fg.b = (u8)(grey * (1-(fade-0.5)*2) + fg.b * ((fade-0.5)*2));
    }

    TCODColor		bg = TCODConsole::root->getCharBackground(x, y);

    grey = (int)bg.r + bg.g + bg.b;
    grey /= 3;
    if (fade < 0.5)
    {
	bg.r = (u8)(255 * (1-fade*2) + grey * (fade*2));
	bg.g = (u8)(255 * (1-fade*2) + grey * (fade*2));
	bg.b = (u8)(255 * (1-fade*2) + grey * (fade*2));
    }
    else
    {
	bg.r = (u8)(grey * (1-(fade-0.5)*2) + bg.r * ((fade-0.5)*2));
	bg.g = (u8)(grey * (1-(fade-0.5)*2) + bg.g * ((fade-0.5)*2));
	bg.b = (u8)(grey * (1-(fade-0.5)*2) + bg.b * ((fade-0.5)*2));
    }
    TCODConsole::root->setCharBackground(x, y, bg);
    TCODConsole::root->setCharForeground(x, y, fg);
}

void
gfx_shadetoblack(int x, int y, float fade)
{
    if (x < 0 || y < 0) return;
    if (x >= SCR_WIDTH || y >= SCR_HEIGHT) return;

    fade = 1 - fade;

    TCODColor		fg = TCODConsole::root->getCharForeground(x, y);

    fg.r = (u8)(0 * (1-fade) + fg.r * (fade));
    fg.g = (u8)(0 * (1-fade) + fg.g * (fade));
    fg.b = (u8)(0 * (1-fade) + fg.b * (fade));

    TCODColor		bg = TCODConsole::root->getCharBackground(x, y);

    bg.r = (u8)(0 * (1-fade) + bg.r * (fade));
    bg.g = (u8)(0 * (1-fade) + bg.g * (fade));
    bg.b = (u8)(0 * (1-fade) + bg.b * (fade));

    TCODConsole::root->setCharBackground(x, y, bg);
    TCODConsole::root->setCharForeground(x, y, fg);
}

void 
gfx_fadetoblack(int x, int y, float fade)
{
    if (x < 0 || y < 0) return;
    if (x >= SCR_WIDTH || y >= SCR_HEIGHT) return;

    TCODColor		fg = TCODConsole::root->getCharForeground(x, y);
    int			grey;

    grey = (int)fg.r + fg.g + fg.b;
    grey /= 3;
    float origfade = fade;
    if (fade > 0.5)
    {
	fade -= 0.5f;
	fade = 0.5f - fade;
	fg.r = (u8)(0 * (1-fade*2) + grey * (fade*2));
	fg.g = (u8)(0 * (1-fade*2) + grey * (fade*2));
	fg.b = (u8)(0 * (1-fade*2) + grey * (fade*2));
    }
    else
    {
	fade = 0.5f - fade;
	fade += 0.5f;
	fg.r = (u8)(grey * (1-(fade-0.5)*2) + fg.r * ((fade-0.5)*2));
	fg.g = (u8)(grey * (1-(fade-0.5)*2) + fg.g * ((fade-0.5)*2));
	fg.b = (u8)(grey * (1-(fade-0.5)*2) + fg.b * ((fade-0.5)*2));
    }
    // All this madness just to not retype the lerp expressions that
    // should have been a lerp call anyways!!!
    fade = origfade;

    TCODColor		bg = TCODConsole::root->getCharBackground(x, y);

    grey = (int)bg.r + bg.g + bg.b;
    grey /= 3;
    if (fade > 0.5)
    {
	fade -= 0.5f;
	fade = 0.5f - fade;
	bg.r = (u8)(0 * (1-fade*2) + grey * (fade*2));
	bg.g = (u8)(0 * (1-fade*2) + grey * (fade*2));
	bg.b = (u8)(0 * (1-fade*2) + grey * (fade*2));
    }
    else
    {
	fade = 0.5f - fade;
	fade += 0.5;
	bg.r = (u8)(grey * (1-(fade-0.5)*2) + bg.r * ((fade-0.5)*2));
	bg.g = (u8)(grey * (1-(fade-0.5)*2) + bg.g * ((fade-0.5)*2));
	bg.b = (u8)(grey * (1-(fade-0.5)*2) + bg.b * ((fade-0.5)*2));
    }
    TCODConsole::root->setCharBackground(x, y, bg);
    TCODConsole::root->setCharForeground(x, y, fg);
}
void 
gfx_getString(int x, int y, ATTR_NAMES attr, char *buf, int maxlen, bool silent)
{
    buf[0] = 0;

    int		pos = 0;
    while (!TCODConsole::isWindowClosed())
    {
	if (!silent)
	{
	    redrawWorldNoFlush();
	}
	// We erased this redrawing the world.
	for (int i = 0; i < pos; i++)
	{
	    gfx_printchar(x + i, y, buf[i], attr);
	}
	TCODConsole::flush();
	int key = gfx_getKey(false);

	switch (key)
	{
	    case 0:
		continue;
	    case '\b':
		if (pos)
		{
		    pos--;
		    gfx_printchar(x + pos, y, ' ', attr);
		    buf[pos] = 0;
		}
		break;
	    case '\n':
	    case '\x1b':
		return;
	    default:
		if (key < 128)
		{
		    buf[pos] = key;
		    buf[pos+1] = 0;
		    gfx_printchar(x + pos, y, key, attr);
		    if (pos < maxlen-1)
			pos++;
		}
		break;
	}
    }
}
