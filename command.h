/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	Jacob's Matrix Development
 *
 * NAME:        command.h ( Jacob's Matrix, C++ )
 *
 * COMMENTS:
 *	The command queue processing all the game commands
 *	into the game engine.
 */

#ifndef __command__
#define __command__

#include "glbdef.h"
#include "queue.h"

class COMMAND
{
public:
    COMMAND()
    {
	myAction = ACTION_NONE;
	myDx = myDy = myDz = myDw = 0;
    }
    explicit COMMAND(int blah)
    {
	myAction = ACTION_NONE;
	myDx = myDy = myDz = myDw = 0;
    }
    COMMAND(ACTION_NAMES action, int dx = 0, int dy = 0, int dz = 0, int dw = 0)
    {
	myAction = action;
	myDx = dx;
	myDy = dy;
	myDz = dz;
	myDw = dw;
    }

    ACTION_NAMES	action() const { return myAction; }
    int			dx() const { return myDx; }
    int			dy() const { return myDy; }
    int			dz() const { return myDz; }
    int			dw() const { return myDw; }
    ACTION_NAMES	&action() { return myAction; }
    int			&dx() { return myDx; }
    int			&dy() { return myDy; }
    int			&dz() { return myDz; }
    int			&dw() { return myDw; }

private:
    ACTION_NAMES		myAction;
    int				myDx;
    int				myDy;
    int				myDz;
    int				myDw;
};

typedef QUEUE<COMMAND> CMD_QUEUE;

#endif
