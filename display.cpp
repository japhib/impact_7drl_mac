/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	Jacob's Matrix Development
 *
 * NAME:        display.h ( Jacob's Matrix, C++ )
 *
 * COMMENTS:
 * 	Draws a view into a map.  Handles timed effects, map memory, etc.
 */


#include <libtcod.hpp>
#include "display.h"
#include "scrpos.h"
#include "gfxengine.h"
#include "mob.h"
#include "item.h"
#include "config.h"

#define FADE_TO_BLACK_TIME	3000

DISPLAY::DISPLAY(int x, int y, int w, int h, int border)
{
    myX = x;
    myY = y;
    myW = w;
    myH = h;
    myBorder = border;
    myMapId = -1;

    myMemory = new u8[bwidth() * bheight()];
    myMemoryPos = new POS[bwidth() * bheight()];
    myYellFlags = new u8[myW * myH];
    myFadeFromWhite = false;
    myFadeToBlack = false;
    myPosCache = 0;
    myPendingForgetAll = false;
    clear();
}

DISPLAY::~DISPLAY()
{
    delete myPosCache;
    delete [] myMemory;
    delete [] myMemoryPos;
    delete [] myYellFlags;
}

void
DISPLAY::fadeFromWhite()
{
    myFadeFromWhite = true;
    myWhiteFadeTS = TCOD_sys_elapsed_milli();
}

bool
DISPLAY::fadeToBlack(bool enable)
{
    if (!enable)
    {
	myFadeToBlack = false;
	return false;
    }
    if (!myFadeToBlack)
    {
	myFadeToBlack = true;
	myBlackFadeTS = TCOD_sys_elapsed_milli();
	return true;
    }
    // See if still active.
    int timems = TCOD_sys_elapsed_milli();
    if (timems - myBlackFadeTS > FADE_TO_BLACK_TIME)
	return false;
    return true;
}

void
DISPLAY::clear()
{
    memset(myMemory, ' ', bwidth() * bheight());
    for (int i = 0; i < bwidth() * bheight(); i++)
	myMemoryPos[i] = POS();
    delete myPosCache;

    myPosCache = 0;
}

POS
DISPLAY::lookup(int x, int y) const
{
    POS		p;

    x -= myX + myW/2;
    y -= myY + myH/2;

    if (myPosCache)
	p = myPosCache->lookup(x, y);

    return p;
}

void
DISPLAY::scrollMemory(int dx, int dy)
{
    if (!dx && !dy)
	return;
    u8		*newmem = new u8[bwidth() * bheight()];
    POS		*newpos = new POS[bwidth() * bheight()];
    int		x, y;

    memset(newmem, ' ', bwidth() * bheight());

    for (y = 0; y < bheight(); y++)
    {
	if (y + dy < 0 || y + dy >= bheight())
	    continue;
	for (x = 0; x < bwidth(); x++)
	{
	    if (x + dx < 0 || x + dx >= bwidth())
		continue;

	    newmem[x+dx + (y+dy)*bwidth()] = myMemory[x + y*bwidth()];
	    newpos[x+dx + (y+dy)*bwidth()] = myMemoryPos[x + y*bwidth()];
	}
    }
    delete [] myMemory;
    myMemory = newmem;
    delete [] myMemoryPos;
    myMemoryPos = newpos;
}

void
DISPLAY::rotateMemory(int angle)
{
    angle = angle & 3;
    if (!angle)
	return;

    u8		*newmem = new u8[bwidth() * bheight()];
    POS		*newpos = new POS[bwidth() * bheight()];
    int		x, y;
    int		offx, offy;

    offx = bwidth()/2;
    offy = bheight()/2;

    memset(newmem, ' ', bwidth() * bheight());

    for (y = 0; y < bheight(); y++)
    {
	for (x = 0; x < bwidth(); x++)
	{
	    switch (angle)
	    {
		case 1:
		    newmem[x + y*bwidth()] = recall(y-offy+offx, bwidth() - x-offx+offy);
		    newpos[x + y*bwidth()] = recallPos(y-offy+offx, bwidth() - x-offx+offy);
		    break;

		case 2:
		    newmem[x + y*bwidth()] = recall(bwidth() - x, bheight() - y);
		    newpos[x + y*bwidth()] = recallPos(bwidth() - x, bheight() - y);
		    break;

		case 3:
		    newmem[x + y*bwidth()] = recall(bheight() - y-offy+offx, x-offx+offy);
		    newpos[x + y*bwidth()] = recallPos(bheight() - y-offy+offx, x-offx+offy);
		    break;

	    }
	}
    }
    delete [] myMemory;
    myMemory = newmem;
    delete [] myMemoryPos;
    myMemoryPos = newpos;
}

u8
DISPLAY::recall(int x, int y) const
{
    if (x < 0 || x >= bwidth()) return ' ';
    if (y < 0 || y >= bheight()) return ' ';
    return myMemory[x + y * bwidth()];
}

POS
DISPLAY::recallPos(int x, int y) const
{
    if (x < 0 || x >= bwidth()) return POS();
    if (y < 0 || y >= bheight()) return POS();
    return myMemoryPos[x + y * bwidth()];
}

void
DISPLAY::note(int x, int y, u8 val, POS pos)
{
    if (x < 0 || x >= bwidth()) return;
    if (y < 0 || y >= bheight()) return;

    myMemory[x + y * bwidth()] = val;
    myMemoryPos[x + y * bwidth()] = pos;
}

void
DISPLAY::drawYellMessage(int px, int py, int dx, int dy,
			const char *text,
			ATTR_NAMES attr)
{
    int		textlen = MYstrlen(text);

    YELL_STATE	flag;

    flag = yellFlag(px+dx, py+dy);

    // We will draw the slash over undrawn space.
    // we can also draw it immediately beside text without ambiguity.
    if (flag == YELLSTATE_NONE || flag == YELLSTATE_PAD)
    {
	gfx_printchar( px+dx + x(), py+dy + y(),
			// The following line deserves extra commenting.
			// This text is that extra commenting.
		       ((dx+1)^(dy+1)) ? '/' : '\\',
		       attr );
	// Note what is there.
	setYellFlag(px+dx, py+dy, YELLSTATE_SLASH);
    }

    // Get proposed center of text
    px += dx * 2;
    py += dy * 2;

    // Now search for fitting room.
    // We allow it to migrate in any direction matching our dy.
    findTextSpace(px, py, dy, textlen);

    // We always succeed (but px/py may be out of bounds, but we don't
    // care, because print char will ignore those)
    for (int i = 0; i < textlen; i++)
    {
	gfx_printchar( px - textlen/2 + i + x(),
			py + y(),
			text[i],
			attr );
	setYellFlag(px - textlen/2 + i, py, YELLSTATE_TEXT);
    }
    // Add our pads.
    setYellFlag(px - textlen/2 - 1, py, YELLSTATE_PAD);
    setYellFlag(px - textlen/2 + textlen, py, YELLSTATE_PAD);
}

void
DISPLAY::setYellFlag(int x, int y, YELL_STATE state)
{
    if (x < 0 || y < 0)
	return;

    if (x >= width() || y >= height())
	return;

    myYellFlags[x + y * width()] = state;
}

DISPLAY::YELL_STATE
DISPLAY::yellFlag(int x, int y) const
{
    if (x < 0 || y < 0)
	return YELLSTATE_NONE;

    if (x >= width() || y >= height())
	return YELLSTATE_NONE;

    return (YELL_STATE) myYellFlags[x + y * width()];
}

bool
DISPLAY::checkRoomForText(int px, int py, int textlen)
{
    // Make room for padding
    textlen += 2;

    // Always fit off screen.
    if (py < 0 || py >= height())
	return true;

    YELL_STATE		flag;

    for (int i = 0; i < textlen; i++)
    {
	flag = yellFlag(px - textlen/2 + i, py);

	if (flag == YELLSTATE_PAD || flag == YELLSTATE_TEXT)
	    return false;
    }

    // We can write over everything else
    return true;

}

void
DISPLAY::findTextSpace(int &px, int &py, int dy, int textlen)
{
    int			r, ir;

    for (r = 0; r < width()+height(); r++)
    {
	// Check vertical shift
	for (ir = 0; ir <= r; ir++)
	{
	    if (checkRoomForText(px+ir, py+dy*r, textlen))
	    {
		px += ir;
		py += dy*r;
		return;
	    }
	    if (checkRoomForText(px-ir, py+dy*r, textlen))
	    {
		px -= ir;
		py += dy*r;
		return;
	    }
	}

	// Check horizontal shift
	for (ir = 0; ir <= r; ir++)
	{
	    if (checkRoomForText(px+r, py+ir, textlen))
	    {
		px += r;
		py += ir;
		return;
	    }
	    if (checkRoomForText(px-r, py+ir, textlen))
	    {
		px -= r;
		py += ir;
		return;
	    }
	}
    }

    J_ASSERT(!"We should always hit the auto-none areas and succeed");
    return;
}

void
DISPLAY::display(POS pos, bool isblind)
{
    int		dx, dy;
    int		x, y;

    if (!pos.map() || pos.map()->getId() != myMapId || myPendingForgetAll)
    {
	myPendingForgetAll = false;
	clear();
	if (pos.map())
	    myMapId = pos.map()->getId();
	else
	    myMapId = -1;
    }

    if (myPosCache && myPosCache->find(pos, dx, dy))
    {
	POS		oldpos = myPosCache->lookup(dx, dy);

	// Make pos the center
	scrollMemory(-dx, -dy);
	// Now rotate by it.
	// NOTE: This is likely needed, but screwed up the six-two-one
	// engine so is removed.
	//rotateMemory(oldpos.angle() - pos.angle());
    }
    else
    {
	// We made too big a jump, clear!
	clear();
    }

    delete myPosCache;
    myPosCache = new SCRPOS(pos, (myW+1)/2+myBorder, (myH+1)/2+myBorder);

    // Clear all memorized spots that don't line up with visible spots
    for (y = 0; y < bheight(); y++)
    {
	for (x = 0; x < bwidth(); x++)
	{
	    pos = myPosCache->lookup(x-myBorder-myW/2, y-myBorder-myH/2);
	    if (!pos.valid())
		continue;
	    if (!pos.isFOV())
		continue;
	    if ((isblind && !(x == myW/2+myBorder && y == myH/2+myBorder)))
		continue;

	    // This spot is visible
	    FORALL_8DIR(dx, dy)
	    {
		// Make sure the neighbours still match what we
		// recorded.
		if (pos.delta(0, dy).delta(dx, 0) != recallPos(x+dx, y+dy)
			&&
		    pos.delta(dx, 0).delta(0, dy) != recallPos(x+dx, y+dy))
		{
		    note(x+dx, y+dy, ' ', POS());
		}
	    }
	}
    }

    float		fade;

    fade = 0.0f;
    if (myFadeFromWhite)
    {
	int timems = TCOD_sys_elapsed_milli();
	fade = (timems - myWhiteFadeTS) / 10000.F;
	if (fade > 1.0f)
	{
	    myFadeFromWhite = false;
	}
    }
    if (myFadeToBlack)
    {
	int timems = TCOD_sys_elapsed_milli();
	fade = (timems - myBlackFadeTS) / (float)FADE_TO_BLACK_TIME;
	if (fade > 1.0f)
	{
	    // We stick to black...
	    fade = 1.0;
	}
    }

    for (y = 0; y < myH; y++)
    {
	for (x = 0; x < myW; x++)
	{
	    int		smoke = 0;
	    pos = myPosCache->lookup(x-myW/2, y-myH/2);
	    if (!pos.valid() || !pos.isFOV() || (isblind && !(x == myW/2 && y == myH/2)))
	    {
		if (pos.valid() && pos.isMapped())
		{
		    // Always re-note mapped locations.
		    note(x+myBorder, y+myBorder, pos.defn().symbol, pos);
		}
		u8	symbol = recall(x+myBorder, y+myBorder);
		// Keep stair cases obvious always.
		if (symbol == '>' || symbol == '<')
		    gfx_printchar(x+myX, y+myY, recall(x+myBorder, y+myBorder), ATTR_WHITE);
		else
		    gfx_printchar(x+myX, y+myY, recall(x+myBorder, y+myBorder), ATTR_OUTOFFOV);
	    }
	    else
	    {
		u8		symbol;
		ATTR_NAMES	attr;
		smoke = pos.smoke();

		note(x+myBorder, y+myBorder, pos.defn().symbol, pos);
		if (pos.mob())
		{
		    pos.mob()->getLook(symbol, attr);
		    gfx_printchar(x+myX, y+myY, symbol, attr);
		}
		else if (pos.item())
		{
		    pos.item()->getLook(symbol, attr);
		    gfx_printchar(x+myX, y+myY, symbol, attr);
		    note(x+myBorder, y+myBorder, pos.item()->defn().symbol, pos);
		}
		else
		{
		    u8		symbol = pos.defn().symbol;
		    if (pos.defn().roomsymbol)
			symbol = pos.room_symbol();
		    if (pos.defn().roomcolor)
		    {
			if (glbConfig->myColorBlind)
			    symbol = pos.colorblind_symbol(symbol);
			gfx_printchar(x+myX, y+myY, symbol, 
					(ATTR_NAMES) pos.room_color());
		    }
		    else
			gfx_printchar(x+myX, y+myY, symbol, 
					(ATTR_NAMES) pos.defn().attr);
		}
	    }
	    if (myFadeFromWhite)
		gfx_fadefromwhite(x+myX, y+myY, fade);
	    if (myFadeToBlack)
		gfx_fadetoblack(x+myX, y+myY, fade);
	    if (smoke)
	    {
		smoke = MIN(smoke, 100);
		gfx_fadefromwhite(x+myX, y+myY, 1.0f - smoke / 100.0f);
	    }
	}
    }

    // Apply events...
    int		timems = TCOD_sys_elapsed_milli();

    // First process new events.
    EVENT	e;

    while (queue().remove(e))
    {
	e.setTime(timems);
	myEvents.append(e);
    }

    int		i;

    // The events list is in chronological order.  This means
    // new events draw over old events.
    // Yell messages, however, are weird, because they need to be
    // drawn in the opposite order so the newest shows up nearest
    // the yeller and pushes out old yells.
    // Thus two loops.
    // MOB lookup, and culling too old events, are handled
    // in the first loop.
    // To avoid n^2, we track our write-back target.

    int		writeback = 0;
    for (i = 0; i < myEvents.entries(); i++)
    {
	e = myEvents(i);
	
	if (writeback != i)
	{
	    myEvents(writeback) = e;
	}
	writeback++;

	int		len = 150;
	if (e.type() & EVENTTYPE_LONG)
	    len = 2000;

	if (timems - e.timestamp() > len)
	{
	    writeback--;
	    myEvents(writeback).clearText();
	    continue;
	}

	if (e.mobuid() != INVALID_UID)
	{
	    MOB		*mob;
	    mob = pos.map()->findMob(e.mobuid());
	    if (!mob)
	    {
		writeback--;
		myEvents(writeback).clearText();
		continue;
	    }
	    e.setPos(mob->pos());
	    // We've already inced here.
	    myEvents(writeback-1) = e;
	}

	if (!myPosCache->find(e.pos(), x, y))
	{
	    // No longer on screen.
	    // Do not remove, it may come back on screen
	    // for long events!
	    // But do remove short events or we quickly fill the buffer.
	    if (!(e.type() & EVENTTYPE_LONG))
	    {
		writeback--;
		myEvents(writeback).clearText();
		continue;
	    }
	    continue;
	}

	// Get the actual pos which we can use for FOV
	pos = myPosCache->lookup(x, y);
	if (pos.isFOV())
	{
	    int		px = x + myX + myW/2;
	    int		py = y + myY + myH/2;

	    // Draw the event.
	    if (e.type() & EVENTTYPE_SHOUT)
	    {
		// Ignore shouts in this pass.
	    }
	    else
	    {
		// Just a symbol
		if (e.type() & EVENTTYPE_FORE)
		    gfx_printattrfore(px, py, e.attr());
		if (e.type() & EVENTTYPE_BACK)
		    gfx_printattrback(px, py, e.attr());
		if (e.type() & EVENTTYPE_SYM)
		    gfx_printchar(px, py, e.sym());
	    }
	}
    }
    // Truncate back!
    myEvents.resize(writeback);

    // Backwards loop for yells.
    // Clear our yell state
    memset(myYellFlags, YELLSTATE_NONE, width()*height());
    for (i = myEvents.entries(); i --> 0; )
    {
	e = myEvents(i);

	if (!(e.type() & EVENTTYPE_SHOUT))
	    continue;

	if (!myPosCache->find(e.pos(), x, y))
	{
	    // No longer on screen.
	    // Do not remove, it may come back on screen
	    // for long events!
	    continue;
	}

	// Get the actual pos which we can use for FOV
	pos = myPosCache->lookup(x, y);
	if (pos.isFOV())
	{
	    int		px = x + myW/2;
	    int		py = y + myH/2;

	    // Draw the shout text.
	    if (e.text())
	    {
		int		dx = SIGN(x);
		int		dy = SIGN(y);

		if (!dx)
		    dx = 1;

		if (!dy)
		    dy = -1;

		drawYellMessage(px, py, dx, dy, e.text(), e.attr());
	    }
	}
    }
}
