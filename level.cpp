/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        level.cpp ( Rogue Impact, C++ )
 *
 * COMMENTS:
 * 	Mostly a poor-man's namespace class to centralize level
 * 	related code as everything has a level in Genshin Impact.
 */

#include "level.h"

class LEVELTABLE
{
public:
    LEVELTABLE()
    {
	int		curlevel = 0;		// Current level
	int		levelreq = 0;		// Break point to go up a level
	int		levelcost = 100;	// Current leveling cost
	int		levelinc = 10;		// Increase in leveling cost per level

	// Triangle numbers are usually a good approach...
	// I could close form this, but where's the fun in that?
	while (curlevel <= LEVEL::MAX_LEVEL)
	{
	    myLevelThresh[curlevel] = levelreq;

	    curlevel++;
	    levelreq += levelcost;
	    levelcost += levelinc;
	}
    }

    int		operator()(int level) const
    {
	if (level <= 1)
	    return 0;
	if (level > LEVEL::MAX_LEVEL)
	    return 1 << 30;
	return myLevelThresh[level-1];
    }

protected:
    int		myLevelThresh[LEVEL::MAX_LEVEL+1];
};

int
LEVEL::expFromLevel(int level)
{
    if (level <= 1)
	return 0;

    static LEVELTABLE leveltable;

    return leveltable(level);
}

int
LEVEL::levelFromExp(int exp)
{
    if (exp <= 0)
	return 1;

    int		level = 1;
    while (exp >= expFromLevel(level+1))
    {
	level++;
	if (level == LEVEL::MAX_LEVEL)
	    return level;
    }

    return level;
}

int
LEVEL::boostHP(int basehp, int level)
{
    if (level <= 1)
	return basehp;
    while (level > 1)
    {
	level--;
	basehp += (basehp + 9) / 10;
    }
    return basehp;
}

int
LEVEL::boostResist(int basehp, int level)
{
    return boostHP(basehp, level);
}

int
LEVEL::boostDamage(int basehp, int level)
{
    return boostHP(basehp, level);
}

int
LEVEL::boostDuration(int basehp, int level)
{
    return boostHP(basehp, level);
}

int
LEVEL::boostHeal(int basehp, int level)
{
    return boostHP(basehp, level);
}
