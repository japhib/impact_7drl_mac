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

#ifndef __gfxengine__
#define __gfxengine__

#include "mygba.h"
#include "glbdef.h"

#define SCR_WIDTH 140
#define SCR_HEIGHT 40
#define WORDWRAP_LEN 72

// Key defines.
enum GFX_Keydefine
{
    GFX_KEYSTART = 256,
    GFX_KEYUP,
    GFX_KEYLEFT,
    GFX_KEYRIGHT,
    GFX_KEYDOWN,
    GFX_KEYPAGEUP,
    GFX_KEYPAGEDOWN,
    GFX_KEYHOME,
    GFX_KEYEND,
    GFX_KEYUP_SHIFT,
    GFX_KEYLEFT_SHIFT,
    GFX_KEYRIGHT_SHIFT,
    GFX_KEYDOWN_SHIFT,
    GFX_KEYPAGEUP_SHIFT,
    GFX_KEYPAGEDOWN_SHIFT,
    GFX_KEYHOME_SHIFT,
    GFX_KEYEND_SHIFT,
    GFX_KEYF1,
    GFX_KEYF2,
    GFX_KEYF3,
    GFX_KEYF4,
    GFX_KEYF5,
    GFX_KEYF6,
    GFX_KEYF7,
    GFX_KEYF8,
    GFX_KEYF9,
    GFX_KEYF10,
    GFX_KEYF11,
    GFX_KEYF12,
    GFX_KEYSF1,
    GFX_KEYSF2,
    GFX_KEYSF3,
    GFX_KEYSF4,
    GFX_KEYSF5,
    GFX_KEYSF6,
    GFX_KEYSF7,
    GFX_KEYSF8,
    GFX_KEYSF9,
    GFX_KEYSF10,
    GFX_KEYSF11,
    GFX_KEYSF12,
    GFX_KEYLAST
};

void gfx_init();
void gfx_shutdown();
void gfx_update();
void gfx_updatepulsetime();

int gfx_getKey(bool block = true);

void gfx_clearKeyBuf();

// Returns true if key is a direction key. 0's key in this case
// and initializes dx/dy
enum GFX_Cookdir
{
    GFX_COOKDIR_NONE = 0,
    GFX_COOKDIR_X = 1,
    GFX_COOKDIR_Y = 2,
    GFX_COOKDIR_ONLYMOVE = 3,
    GFX_COOKDIR_STATIONARY = 4,
    GFX_COOKDIR_ALL = 7,
};

bool gfx_cookDir(int &key, int &dx, int &dy, GFX_Cookdir cookdir = GFX_COOKDIR_ALL,
	bool allowshift = false, int shiftscale = 5);

void gfx_getString(int x, int y, ATTR_NAMES attr, char *buf, int maxlen, bool silent=false);

void gfx_printchar(int x, int y, u8 c, ATTR_NAMES attr);
// Uses black background.
void gfx_printchar(int x, int y, u8 c, u8 r, u8 g, u8 b);
// Fully specify
void gfx_printchar(int x, int y, u8 c, u8 r, u8 g, u8 b, u8 br, u8 bg, u8 bb);
void gfx_printattr(int x, int y, ATTR_NAMES attr);
// Only does back/fore
void gfx_printattrback(int x, int y, ATTR_NAMES attr);
void gfx_printattrfore(int x, int y, ATTR_NAMES attr);
// Fades from pure white -> mono -> color
// 0.0 means white
void gfx_fadefromwhite(int x, int y, float fade);
// Fades from pure colour -> mono -> black
// 0.0 means colour
void gfx_fadetoblack(int x, int y, float fade);
// Fades from pure -> black.
// 0.0 means colur.
void gfx_shadetoblack(int x, int y, float fade);
// Prints with no attribute change.
void gfx_printchar(int x, int y, u8 c);

// Treats the screen as a terminal, writing to bottom & scrolling.
void gfx_faketerminal_setheader(int header);
void gfx_faketerminal_write(const char *text);
void gfx_faketerminal_scroll();

void gfx_delay(int waitms);

#endif
