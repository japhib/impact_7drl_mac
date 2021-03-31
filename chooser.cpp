/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        chooser.cpp ( Jacob's Matrix, C++ )
 *
 * COMMENTS:
 *	Defines a text panel that you can select an entry from.
 *	Sizes itself to fit, you specify how centering should
 *	work.
 */

#include "chooser.h"
#include "gfxengine.h"

CHOOSER::CHOOSER()
{
    myW = myH = 0;
    myIntX = myIntY = 0;
    myIndent = myRightMargin = 0;

    myTextAttr = ATTR_NORMAL;
    myPrequelAttr = ATTR_NORMAL;
    myChosenAttr = ATTR_HILITE;
    myPercentAttr = ATTR_HILITE;
    myChosenPercentAttr = ATTR_HILITE;

    myCenterX = JUSTCENTER;
    myCenterY = JUSTCENTER;

    myBorder = false;

    myMinW = 0;
    myMaxW = 0;
    myScrollPos = 0;

    myChoice = -1;
}

CHOOSER::~CHOOSER()
{
    clear();
}

void
CHOOSER::clear()
{
    int		i;
    myW = myH = 0;

    myW = myMinW;

    for (i = 0; i < myChoices.entries(); i++)
	free(myChoices(i));
    myChoices.clear();
    for (i = 0; i < myPrequel.entries(); i++)
	free(myPrequel(i));
    myPrequel.clear();
    myPercent.clear();
    myAttr.clear();
    myChoice = -1;
    myScrollPos = 0;
}

int
CHOOSER::appendChoice(const char *choice, int percent)
{
    myW = MAX(MYstrlen(choice) + myIndent + myRightMargin, myW);
    myChoices.append(MYstrdup(choice));
    myPercent.append(percent);
    myAttr.append(myTextAttr);
    myH++;
    // Clamp our height so we don't scroll off.
    myH = MIN(myH, SCR_HEIGHT-4);
    return entries()-1;
}

void
CHOOSER::setPrequel(const char *text)
{
    myH -= myPrequel.entries();

    for (int i = 0; i < myPrequel.entries(); i++)
	free(myPrequel(i));
    myPrequel.clear();

    if (!text)
	return;

    BUF		line, buf;

    buf.strcpy(text);

    while (buf.isstring())
    {
	line = buf.extractLine();

	if (!line.isstring())
	    line.reference("");
	myPrequel.append(MYstrdup(line.buffer()));
	if (myMaxW)
	    myW = MAX(MAX(line.strlen() + myIndent + myRightMargin, myMaxW), myW);
	else
	    myW = MAX(line.strlen() + myIndent + myRightMargin, myW);
    }

    myH += myPrequel.entries();
}

int
CHOOSER::getChoice() const
{
    return myChoice;
}

void
CHOOSER::setChoice(int choice)
{
    myChoice = BOUND(choice, 0, entries()-1);

}

void
CHOOSER::setNoChoice()
{
    myChoice = -1;
}

bool
CHOOSER::processKey(int &key)
{
    bool	process = false;
    int		dx, dy;

    if (gfx_cookDir(key, dx, dy, GFX_COOKDIR_Y))
    {
	dy += getChoice();
	if (dy < 0)
	    dy += entries();
	if (dy >= entries())
	    dy -= entries();

	setChoice(dy);
	
	return true;
    }

    switch (key)
    {
	case GFX_KEYHOME:
	    setChoice(0);
	    process = true;
	    break;
	case GFX_KEYEND:
	    setChoice(entries()-1);
	    process = true;
	    break;
	case GFX_KEYPAGEUP:
	    setChoice(getChoice() - 10);
	    process = true;
	    break;
	case GFX_KEYPAGEDOWN:
	    setChoice(getChoice() + 10);
	    process = true;
	    break;
    }

    if (process)
    {
	key = 0;
	return true;
    }

    return false;
}

void
CHOOSER::redraw() const
{
    int		xoff = getX(), yoff = getY();
    int		x, y, ay, sx, sy, clen, perlen;
    ATTR_NAMES	attr;

    // First we ensure our current choice is centered.
    if (myChoices.entries() && myChoice >= 0)
    {
	// See if we show up on our current system.
	int		effective_y = myChoice - myScrollPos + myPrequel.entries();

	int		up_scroll = myScrollPos != 0;
	int		down_scroll = (height() - myPrequel.entries()) - (myChoices.entries() - myScrollPos) < 0;

	if (effective_y < myPrequel.entries() + up_scroll)
	{
	    // Adjust scroll position to compensate.
	    myScrollPos -= myPrequel.entries() + up_scroll - effective_y;
	    myScrollPos = MAX(0, myScrollPos);
	}
	// We hope we don't fail both at once...
	else if (effective_y >= height() - down_scroll)
	{
	    myScrollPos += effective_y + 1 - (height() - down_scroll);
	    myScrollPos = MIN(myScrollPos, myChoices.entries() - (height() - myPrequel.entries()));
	}
    }

    for (ay = 0; ay < height(); ay++)
    {
	sy = ay + yoff;
	if (sy < 0 || sy >= SCR_HEIGHT)
	    continue;

	if (ay < myPrequel.entries())
	{
	    y = ay;

	    attr = myPrequelAttr;
	    if (myPrequel(y))
		clen = MYstrlen(myPrequel(y));
	    else
		clen = 0;
	    for (x = 0; x < width(); x++)
	    {
		sx = x + xoff;
		if (sx < 0 || sx >= SCR_WIDTH)
		    continue;

		// Indents...
		if (x < myIndent)
		    gfx_printchar(sx, sy, ' ', attr);
		else if (x-myIndent < clen)
		    gfx_printchar(sx, sy, myPrequel(y)[x-myIndent], attr);
		else
		    gfx_printchar(sx, sy, ' ', attr);
	    }
	}
	else
	{
	    // A choice
	    y = ay - myPrequel.entries() + myScrollPos;

	    // See if we are the final row, and if so, if we have
	    // scrolling to do.
	    if (ay == height()-1)
	    {
		if (y == myChoices.entries()-1)
		{
		    // Valid, draw normally
		}
		else
		{
		    // Out of range, draw as a VVVV
		    for (x = 0; x < width(); x++)
		    {
			sx = x + xoff;
			if (sx < 0 || sx >= SCR_WIDTH)
			    continue;

			attr = myTextAttr;
			gfx_printchar(sx, sy, 'V', attr);
		    }
		    continue;
		}
	    }
	    if (ay == myPrequel.entries() && myScrollPos)
	    {
		// First line, if we have scrolled at all, present
		// the ^^^ line.
		for (x = 0; x < width(); x++)
		{
		    sx = x + xoff;
		    if (sx < 0 || sx >= SCR_WIDTH)
			continue;

		    attr = myTextAttr;
		    gfx_printchar(sx, sy, '^', attr);
		}
		continue;
	    }

	    if (y >= myChoices.entries() || y < 0)
	    {
		// This should not happen, but we fill white white.
		attr = myTextAttr;
		for (x = 0; x < width(); x++)
		{
		    sx = x + xoff;
		    gfx_printchar(sx, sy, ' ', attr);
		}
		continue;
	    }

	    clen = MYstrlen(myChoices(y));
	    perlen = 0;
	    if (myPercent(y))
	    {
		perlen = (width() * myPercent(y) + 50) / 100;
	    }
	    for (x = 0; x < width(); x++)
	    {
		sx = x + xoff;
		if (sx < 0 || sx >= SCR_WIDTH)
		    continue;

		attr = myAttr(y);
		if (getChoice() == y)
		    attr = myChosenAttr;
		if (x < perlen)
		{
		    if (getChoice() == y)
			attr = myChosenPercentAttr;
		    else
			attr = myPercentAttr;
		}

		// Indents...
		if (x < myIndent)
		    gfx_printchar(sx, sy, ' ', attr);
		else if (x-myIndent < clen)
		    gfx_printchar(sx, sy, myChoices(y)[x-myIndent], attr);
		else
		    gfx_printchar(sx, sy, ' ', attr);
	    }
	}
    }

    if (myBorder)
    {
	for (y = -1; y < height()+1; y++)
	{
	    if (y + yoff < 0 || y + yoff >= SCR_HEIGHT)
		continue;

	    if (xoff-1 > 0 && xoff-1 < SCR_WIDTH)
		gfx_printchar(xoff-1, y + yoff, myBorderSym, myBorderAttr);
	    if (xoff+myW > 0 && xoff+myW < SCR_WIDTH)
		gfx_printchar(xoff+myW, y + yoff, myBorderSym, myBorderAttr);
	}
	for (x = -1; x < myW+1; x++)
	{
	    if (x + xoff < 0 || x + xoff >= SCR_WIDTH)
		continue;

	    if (yoff-1 > 0 && yoff-1 < SCR_HEIGHT)
		gfx_printchar(x + xoff, yoff - 1, myBorderSym, myBorderAttr);
	    if (yoff+height() > 0 && yoff+height() < SCR_WIDTH)
		gfx_printchar(x + xoff, yoff+height(), myBorderSym, myBorderAttr);
	}
    }
}

void
CHOOSER::move(int x, int y, Justify cx, Justify cy)
{
    myIntX = x;
    myIntY = y;

    myCenterX = cx;
    myCenterY = cy;
}

void
CHOOSER::setBorder(bool enable, u8 sym, ATTR_NAMES attr)
{
    myBorder = enable;
    myBorderSym = sym;
    myBorderAttr = attr;
}

void
CHOOSER::setAttr(ATTR_NAMES texet, ATTR_NAMES chosen, ATTR_NAMES per, ATTR_NAMES chosenper)
{
    myTextAttr = texet;
    myPrequelAttr = texet;
    myChosenAttr = chosen;
    myPercentAttr = per;
    myChosenPercentAttr = chosenper;
}

void
CHOOSER::setIndent(int indent)
{
    myIndent = indent;
}

void
CHOOSER::setRigthMargin(int margin)
{
    myRightMargin = margin;
}

int
CHOOSER::getX() const
{
    if (myCenterX == JUSTMIN)
	return myIntX;
    if (myCenterX == JUSTMAX)
	return myIntX - myW;
    return myIntX - myW/2;
}

int
CHOOSER::getY() const
{
    if (myCenterY == JUSTMIN)
	return myIntY;
    if (myCenterY == JUSTMAX)
	return myIntY-height();
    return myIntY - height()/2;
}
