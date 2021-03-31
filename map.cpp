/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	Jacob's Matrix Development
 *
 * NAME:        map.h ( Jacob's Matrix, C++ )
 *
 * COMMENTS:
 */

#include <libtcod.hpp>
#undef CLAMP

#ifdef WIN32
#else
#include <unistd.h>
#endif

#include "map.h"
#include "speed.h"
#include "config.h"

#include "mob.h"
#include "item.h"
#include "level.h"

#include "text.h"

#include "dircontrol.h"
#include "scrpos.h"
#include "display.h"
#include "disunion.h"
#include "msg.h"
#include "builder.h"
#include "engine.h"
#include "vec2.h"

#include <fstream>
using namespace std;

// #define DO_TIMING

#define ORIENT_FLIPX	1
#define ORIENT_FLIPY	2
#define ORIENT_FLOP	4

#define ATLAS_WIDTH	100
#define ATLAS_HEIGHT	100

PTRLIST<class FRAGMENT *> MAP::theFragRooms[NUM_ROOMTYPES];

PTRLIST<int>	glbMobsInFOV;
extern const char *glbWorldName;
extern volatile bool glbAutoPilot;

class FRAGMENT
{
public:
    FRAGMENT(const char *fname);
    FRAGMENT(int width, int height);
    ~FRAGMENT();

    ROOM		*buildRoom(MAP *map, ROOMTYPE_NAMES type, int difficulty, int depth, int atlasx, int atlasy) const;

protected:
    void		 swizzlePos(int &x, int &y, int orient) const;

    int		myW, myH;
    u8		*myTiles;
    BUF		myFName;
    friend class ROOM;
};

POS::POS()
{
    myRoomId = -1;
    myMap = 0;
    myX = myY = myAngle = 0;
}

POS::POS(int blah)
{
    myRoomId = -1;
    myMap = 0;
    myX = myY = myAngle = 0;
}

POS
POS::goToCanonical(int x, int y) const
{
    POS result;
    if (!map())
	return result;

    result = map()->buildFromCanonical(x, y);
    return result;
}

bool	
POS::valid() const 
{ 
    if (tile() == TILE_INVALID) return false; 
    return true; 
}

TILE_NAMES	
POS::tile() const 
{ 
    if (!room()) return TILE_INVALID; 
    TILE_NAMES		tile = room()->getTile(myX, myY); 

    return tile;
}

void	
POS::setTile(TILE_NAMES tile) const
{ 
    if (!room()) return; 
    return room()->setTile(myX, myY, tile); 
}

int
POS::smoke() const
{
    if (!valid())
	return 0;
    return room()->getSmoke(myX, myY);
}

void
POS::setSmoke(int smoke)
{
    if (!valid())
	return;
    room()->setSmoke(myX, myY, smoke);
}

MAPFLAG_NAMES 		
POS::flag() const 
{ 
    if (!room()) return MAPFLAG_NONE; 
    return room()->getFlag(myX, myY); 
}

void	
POS::setFlag(MAPFLAG_NAMES flag, bool state) const
{ 
    if (!room()) return; 
    return room()->setFlag(myX, myY, flag, state); 
}

bool
POS::prepSquareForDestruction() const
{
    ROOM		*r = room();
    int			dx, dy;

    if (!valid())
	return false;

    if (!r)
	return false;

    if (!myX || !myY || myX == r->width()-1 || myY == r->height()-1)
    {
	// Border tile, borders to nothing, so forbid dig
	return false;
    }

    TILE_NAMES	wall_tile = (TILE_NAMES) roomDefn().wall_tile;
    TILE_NAMES	tunnelwall_tile = (TILE_NAMES) roomDefn().tunnelwall_tile;
    TILE_NAMES	floor_tile = (TILE_NAMES) roomDefn().floor_tile;

    FORALL_8DIR(dx, dy)
    {
	if (r->getTile(myX+dx, myY+dy) == TILE_INVALID)
	{
	    // Expand our walls.
	    r->setTile(myX+dx, myY+dy, wall_tile);
	}
	// Do not dig around portals!
	if (r->getFlag(myX+dx, myY+dy) & MAPFLAG_PORTAL)
	    return false;
    }

    // If this square is a portal, don't dig!
    if (r->getFlag(myX, myY) & MAPFLAG_PORTAL)
	return false;

    return true;
}

bool
POS::forceConnectNeighbours() const
{
    if (!map())
	return false;
    if (!room())
	return false;

    return room()->forceConnectNeighbours();
}


bool
POS::digSquare() const
{
    if (!defn().isdiggable)
	return false;

    if (!prepSquareForDestruction())
	return false;

    TILE_NAMES		nt;

    nt = TILE_FLOOR;
    if (tile() == TILE_WALL)
	nt = TILE_BROKENWALL;

    setTile(nt);

    return true;
}

ROOMTYPE_NAMES
POS::roomType() const
{
    if (!room()) return ROOMTYPE_NONE;
    return room()->type();
}

MOB *
POS::mob() const 
{ 
    ROOM	*thisroom = room();
    if (!thisroom) return 0; 

    if (! (thisroom->getFlag(myX, myY) & MAPFLAG_MOB) )
	return 0;

    return myMap->findMobByLoc(thisroom, myX, myY);
}

int
POS::getAllMobs(MOBLIST &list) const
{
    // Can't retrieve multiple mobs at the moment!
    if (!room()) return list.entries();

    ROOM	*thisroom = room();
    if (! (thisroom->getFlag(myX, myY) & MAPFLAG_MOB) )
	return 0;

    return myMap->findAllMobsByLoc(list, thisroom, myX, myY);
}

int
POS::getDistance(int distmap) const 
{ 
    if (!room()) return -1; 

    return room()->getDistance(distmap, myX, myY); 
}

void
POS::setDistance(int distmap, int dist) const 
{ 
    if (!room()) return; 

    return room()->setDistance(distmap, myX, myY, dist); 
}

int
POS::getAllItems(ITEMLIST &list) const
{
    ROOM	*thisroom = room();
    if (!thisroom) return list.entries(); 

    if (! (thisroom->getFlag(myX, myY) & MAPFLAG_ITEM) )
	return list.entries();

    return myMap->findAllItemsByLoc(list, thisroom, myX, myY);
}

ITEM *
POS::item() const 
{ 
    ROOM	*thisroom = room();
    if (!thisroom) return 0; 

    if (! (thisroom->getFlag(myX, myY) & MAPFLAG_ITEM) )
	return 0;

    return myMap->findItemByLoc(thisroom, myX, myY);
}

int
POS::allItems(ITEMLIST &items) const 
{ 
    items.clear();
    if (!room()) return 0; 
    getAllItems(items);
    return items.entries();
}

int
POS::depth() const
{
    ROOM *r = room();
    if (!r)
	return 0;
    return r->getDepth();
}

ROOM *
POS::room() const 
{ 
    if (myRoomId < 0) return 0; 
    return myMap->myRooms(myRoomId); 
}

u8
POS::room_symbol() const
{
    if (room())
	return room()->getSymbol();
    return 'X';
}

ATTR_NAMES
POS::room_color() const
{
    ATTR_NAMES		result = ATTR_NORMAL;

    if (room())
    {
	result = MAP::depthColor(room()->getDepth(), false);
    }

    return result;
}

u8
POS::colorblind_symbol(u8 oldsym) const
{
    if (room())
    {
	if (oldsym == '#')
	{
	    static const char *wallsym = "#%&*O+";
	    int	depth = BOUND(room()->getDepth(), 0, 5);
	    oldsym = wallsym[depth];
	}
    }
    return oldsym;
}


int		 
POS::roomSeed() const 
{ 
    if (room()) 
	return room()->getSeed(); 
    return -1; 
}

int
POS::getRoomDepth() const
{
    return depth();
}


void
POS::moveMob(MOB *mob, POS oldpos) const
{
    // If wasn't in a room, add it!
    if (oldpos.myRoomId < 0)
    {
	if (myRoomId >= 0)
	{
	    addMob(mob);
	}
    }
    else
    {
	// Was in a room before.  Either a delete, or a real move.
	if (myRoomId < 0)
	{
	    // Removal.
	    oldpos.removeMob(mob);
	}
	else
	{
	    // A proper move.
	    myMap->moveMob(mob, *this, oldpos);

	    if (!oldpos.mob())
		oldpos.setFlag(MAPFLAG_MOB, false);
	    setFlag(MAPFLAG_MOB, true);
	}
    }
}

void
POS::removeMob(MOB *m) const
{
    if (myRoomId < 0) return; 

    myMap->removeMob(m, *this);
    if (!mob())
	setFlag(MAPFLAG_MOB, false);
}

void
POS::addMob(MOB *m) const
{
    if (myRoomId < 0) return; 

    myMap->addMob(m, *this);
    setFlag(MAPFLAG_MOB, true);
}

void
POS::removeItem(ITEM *it) const
{
    if (myRoomId < 0) return; 

    myMap->removeItem(it, *this);

    if (!item())
	setFlag(MAPFLAG_ITEM, false);
}

void
POS::addItem(ITEM *it) const
{
    if (myRoomId < 0) return; 

    ITEMLIST its;
    getAllItems(its);

    if (!its.entries())
	setFlag(MAPFLAG_ITEM, true);

    for (int i = 0; i < its.entries(); i++)
    {
	if (it->canStackWith(its(i)))
	{
	    its(i)->combineItem(it);
	    delete it;
	    return;
	}
    }

    myMap->addItem(it, *this);
}

void
POS::save(ostream &os) const
{
    os.write((const char *)&myX, sizeof(int));
    os.write((const char *)&myY, sizeof(int));
    os.write((const char *)&myAngle, sizeof(int));
    os.write((const char *)&myRoomId, sizeof(int));
}

void
POS::load(istream &is)
{
    is.read((char *)&myX, sizeof(int));
    is.read((char *)&myY, sizeof(int));
    is.read((char *)&myAngle, sizeof(int));
    is.read((char *)&myRoomId, sizeof(int));
}

MOB *
POS::traceBullet(int range, int dx, int dy, int *rangeleft) const
{
    if (rangeleft)
	*rangeleft = 0;
    if (!dx && !dy)
	return mob();

    POS		next = *this;

    while (range > 0)
    {
	range--;
	next = next.delta(dx, dy);
	if (next.mob())
	{
	    if (rangeleft)
		*rangeleft = range;
	    return next.mob();
	}

	// Stop at a wall.
	if (!next.defn().ispassable)
	    return 0;
    }
    return 0;
}

POS
POS::traceBulletPos(int range, int dx, int dy, bool stopbeforewall, bool stopatmob) const
{
    if (!dx && !dy)
	return *this;

    POS		next = *this;
    POS		last = *this;

    while (range > 0)
    {
	range--;
	next = next.delta(dx, dy);

	// Stop at a mob.
	if (stopatmob && next.mob())
	    return next;

	if (!next.defn().ispassable)
	{
	    // Hit a wall.  Either return next or last.
	    if (stopbeforewall)
		return last;
	    return next;
	}
	last = next;
    }
    return next;
}

void
POS::postEvent(EVENTTYPE_NAMES type, u8 sym, ATTR_NAMES attr) const
{
    map()->myDisplay->queue().append(EVENT(*this, sym, attr, type));
}

void
POS::postEvent(EVENTTYPE_NAMES type, u8 sym, ATTR_NAMES attr, const char *text) const
{
    map()->myDisplay->queue().append(EVENT(*this, sym, attr, type, text));
}

void
POS::displayBullet(int range, int dx, int dy, u8 sym, ATTR_NAMES attr, bool stopatmob) const
{
    if (!dx && !dy)
	return;

    POS		next = delta(dx, dy);

    while (range > 0)
    {
	if (!next.valid())
	    return;
	
	// Stop at walls.
	if (!next.isPassable())
	    return;

	// Stop at mobs
	if (stopatmob && next.mob())
	    return;

	next.postEvent(EVENTTYPE_FORESYM, sym, attr);

	range--;
	next = next.delta(dx, dy);
    }
}

void
POS::rotateByAngle(int angle, int &dx, int &dy)
{
    int		odx = dx;
    int		ody = dy;
    switch (angle & 3)
    {
	case 0:			// ^
	    break;		
	case 1:			// ->
	    dx = -ody;
	    dy = odx;
	    break;
	case 2:			// V
	    dx = -odx;
	    dy = -ody;
	    break;
	case 3:			// <-
	    dx = ody;
	    dy = -odx;
	    break;
    }
}

void
POS::rotateToLocal(int &dx, int &dy) const
{
    rotateByAngle(myAngle, dx, dy);
}

void
POS::rotateToWorld(int &dx, int &dy) const
{
    int		odx = dx;
    int		ody = dy;
    switch (myAngle & 3)
    {
	case 0:			// ^
	    break;		
	case 1:			// ->
	    dx = ody;
	    dy = -odx;
	    break;
	case 2:			// V
	    dx = -odx;
	    dy = -ody;
	    break;
	case 3:			// <-
	    dx = -ody;
	    dy = odx;
	    break;
    }
}

int
POS::dist(POS goal) const
{
    if (!goal.valid() || !valid())
    {
	if (!room())
	    return 1000;
	return room()->width() + room()->height();
    }
    if (goal.myRoomId == myRoomId)
    {
	return MAX(ABS(goal.myX-myX), ABS(goal.myY-myY));
    }

    // Check to see if we are in the map cache.
    int		dx, dy;
    if (map()->computeDelta(*this, goal, dx, dy))
    {
	// Yah, something useful.
	return MAX(ABS(dx), ABS(dy));
    }

    // Consider it infinitely far.  Very dangerous as monsters
    // will get stuck on portals :>
    return room()->width() + room()->height();
}

void
POS::dirTo(POS goal, int &dx, int &dy) const
{
    if (!goal.valid() || !valid())
    {
	// Arbitrary...
	dx = 0;
	dy = 1;
	rotateToWorld(dx, dy);
    }

    if (goal.myRoomId == myRoomId)
    {
	dx = SIGN(goal.myX - myX);
	dy = SIGN(goal.myY - myY);

	// Convert to world!
	rotateToWorld(dx, dy);
	return;
    }

    // Check to see if we are in the map cache.
    if (map()->computeDelta(*this, goal, dx, dy))
    {
	// Map is nice enough to give it in world coords.
	dx = SIGN(dx);
	dy = SIGN(dy);
	return;
    }

    // Umm.. Be arbitrary?
    rand_direction(dx, dy);
}

void
POS::setAngle(int angle)
{
    myAngle = angle & 3;
}

POS
POS::rotate(int angle) const
{
    POS		p;

    p = *this;
    p.myAngle += angle;
    p.myAngle &= 3;

    return p;
}

POS
POS::delta4Direction(int angle) const
{
    int		dx, dy;

    rand_getdirection(angle, dx, dy);

    return delta(dx, dy);
}

POS
POS::delta(int dx, int dy) const
{
    if (!valid())
	return *this;

    if (!dx && !dy)
	return *this;

    // Fast check for simple cases without portals.
    ROOM		*thisroom = room();
    if (!thisroom->myPortals.entries())
    {
	int		 ldx = dx, ldy = dy;
	rotateToLocal(ldx, ldy);

	int		nx = myX + ldx, ny = myY + ldy;

	if (nx >= 0 && nx < thisroom->width() &&
	    ny >= 0 && ny < thisroom->height())
	{
	    // Trivial update!
	    POS		dst;
	    dst = *this;
	    dst.myX = nx;
	    dst.myY = ny;
	    return dst;
	}
    }

    // We want to make a single step in +x or +y.
    // The problem is diagonals.  We want to step in both xy and yx orders
    // and take whichever is valid.  In most cases they will be equal...
    if (dx && dy)
    {
	POS	xfirst, yfirst;
	int	sdx = SIGN(dx);
	int	sdy = SIGN(dy);

	xfirst = delta(sdx, 0).delta(0, sdy);
	yfirst = delta(0, sdy).delta(sdx, 0);

	if (!xfirst.valid())
	{
	    // Must take yfirst
	    return yfirst.delta(dx - sdx, dy - sdy);
	}
	if (!yfirst.valid())
	{
	    // Must take xfirst.
	    return xfirst.delta(dx - sdx, dy - sdy);
	}

	// Both are valid.  In all likeliehoods, identical.
	// But if one is wall and one is not, we want the non-wall!
	if (!xfirst.defn().ispassable)
	    return yfirst.delta(dx - sdx, dy - sdy);

	// WLOG, use xfirst now.
	return xfirst.delta(dx - sdx, dy - sdy);
    }

    // We now have a simple case of a horizontal or vertical step
    rotateToLocal(dx, dy);

    int		sdx = SIGN(dx);
    int		sdy = SIGN(dy);

    PORTAL	*portal;

    portal = thisroom->getPortal(myX + sdx, myY + sdy);
    if (portal)
    {
	// Teleport!
	POS		dst = portal->myDst;

	// We should remain on the same map!
	J_ASSERT(dst.map() == myMap);

	// Apply the portal's twist.
	dst.myAngle += myAngle;
	dst.myAngle &= 3;

	// Portals have no thickness so we re-apply our original delta!
	// Actually, I changed my mind since then, so it is rather important
	// we actually do dec dx/dy.
	dx -= sdx;
	dy -= sdy;
	rotateToWorld(dx, dy);
	return dst.delta(dx, dy);
    }

    // Find the world delta to apply to our next square.
    dx -= sdx;
    dy -= sdy;
    rotateToWorld(dx, dy);

    // Get our next square...
    POS		dst = *this;

    dst.myX += sdx;
    dst.myY += sdy;

    if (dst.myX < 0 || dst.myX >= thisroom->width() ||
	dst.myY < 0 || dst.myY >= thisroom->height())
    {
	// An invalid square!
	// Check our connectivity for implicit connections, for these
	// we jump up to the atlas layer.
	int angle = rand_dir4_toangle(sdx, sdy);
	if (thisroom->myConnectivity[angle] == ROOM::IMPLICIT)
	{
	    int		newatlasx = thisroom->atlasX() + sdx;
	    int		newatlasy = thisroom->atlasY() + sdy;

	    ROOM	*droom = map()->findAtlasRoom(newatlasx, newatlasy);

	    if (!droom)
	    {
		// Failed!
		dst.myRoomId = -1;
	    }
	    else
	    {
		// Reset the coordinate that left our range with the
		// equivalnet maximal in destination.
		if (dst.myX < 0)
		    dst.myX = droom->width()-1;
		else if (dst.myX >= thisroom->width())
		    dst.myX = 0;
		if (dst.myY < 0)
		    dst.myY = droom->height()-1;
		else if (dst.myY >= thisroom->height())
		    dst.myY = 0;
		// Now validate we are inside the dest room coordinates
		// (as we assume the non-wrapped will matchup, but it
		// might not!
		if (dst.myX < 0 || dst.myX >= droom->width() ||
		    dst.myY < 0 || dst.myY >= droom->height())
		{
		    // Failed!
		    dst.myRoomId = -1;
		}
		else
		{
		    // We have a wrap!
		    dst.myRoomId = droom->getId();
		}
	    }
	}
	else
	{
	    // either should have gone through a portal or closed.
	    // Limbo might technically need to expand, but delta()
	    // is expected to be read only on the maps.
	    dst.myRoomId = -1;
	}
	return dst.delta(dx, dy);
    }

    return dst.delta(dx, dy);
}

void
POS::describeSquare(bool isblind) const
{
    BUF		 buf;
    buf.reference(defn().legend);
    ITEMLIST	 itemlist;
    if (!isblind)
    {
	msg_format("You see %O.", 0, buf);
	
	if (mob())
	    msg_format("You see %O.", 0, mob());

	allItems(itemlist);
	if (itemlist.entries())
	{
	    BUF		msg;
	    msg.strcpy("You see ");
	    for (int i = 0; i < itemlist.entries(); i++)
	    {
		if (i)
		{
		    if (i == itemlist.entries()-1)
			msg.strcat(" and ");
		    else
			msg.strcat(", ");
		}

		msg.strcat(itemlist(i)->getArticleName());
	    }

	    msg.strcat(".");
	    msg_report(msg);
	}
    }
}

///
/// ROOM functions
///

ROOM::ROOM()
{
    myId = -1;
    myFragment = 0;
    myType = ROOMTYPE_NONE;
    mySymbol = 'X';
    myAtlasX = myAtlasY = 0;
    mySeed = rand_int();
    myConstantFlags = 0;
    myMaxSmoke = 0;

    memset(myConnectivity, 0, sizeof(u8) * 4);
}

ROOM::ROOM(const ROOM &room)
{
    *this = room;
}

void
ROOM::save(ostream &os) const
{
    s32		val;
    u8		c;
    int			i;

    val = myId;
    os.write((const char *) &val, sizeof(s32));

    val = mySeed;
    os.write((const char *) &val, sizeof(s32));

    c = mySymbol;
    os.write((const char *) &c, sizeof(u8));

    val = myDepth;
    os.write((const char *) &val, sizeof(s32));

    val = myType;
    os.write((const char *) &val, sizeof(s32));

    val = myWidth;
    os.write((const char *) &val, sizeof(s32));
    val = myHeight;
    os.write((const char *) &val, sizeof(s32));

    val = myPortals.entries();
    os.write((const char *) &val, sizeof(s32));
    for (i = 0; i < myPortals.entries(); i++)
    {
	myPortals(i).save(os);
    }

    os.write((const char *) myTiles.data(), myWidth * myHeight);
    os.write((const char *) myFlags.data(), myWidth * myHeight);
    os.write((const char *) mySmoke.data(), myWidth * myHeight);

    val = myAtlasX;
    os.write((const char *) &val, sizeof(s32));
    val = myAtlasY;
    os.write((const char *) &val, sizeof(s32));
    for (i = 0; i < 4; i++)
    {
	val = myConnectivity[i];
	os.write((const char *) &val, sizeof(s32));
    }

}

ROOM *
ROOM::load(istream &is)
{
    ROOM		*result;
    s32		val;
    u8		c;
    int			i, w, h, n;

    result = new ROOM();

    is.read((char *) &val, sizeof(s32));
    result->myId = val;

    is.read((char *) &val, sizeof(s32));
    result->mySeed = val;

    is.read((char *) &c, sizeof(u8));
    result->mySymbol = c;

    is.read((char *) &val, sizeof(s32));
    result->myDepth = val;
    is.read((char *) &val, sizeof(s32));
    result->myType = (ROOMTYPE_NAMES) val;

    is.read((char *) &val, sizeof(s32));
    w = val;
    is.read((char *) &val, sizeof(s32));
    h = val;

    result->resize(w, h);

    is.read((char *) &val, sizeof(s32));
    n = val;
    for (i = 0; i < n; i++)
    {
	result->myPortals.append(PORTAL::load(is));
    }

    is.read( (char *) result->myTiles.writeableData(), w * h);
    is.read( (char *) result->myFlags.writeableData(), w * h);
    is.read( (char *) result->mySmoke.writeableData(), w * h);

    is.read((char *) &val, sizeof(s32));
    result->myAtlasX = val;
    is.read((char *) &val, sizeof(s32));
    result->myAtlasY = val;

    for (i = 0; i < 4; i++)
    {
	is.read((char *) &val, sizeof(s32));
	result->myConnectivity[i] = val;
    }

    return result;
}

ROOM &
ROOM::operator=(const ROOM &room)
{
    if (this == &room)
	return *this;

    deleteContents();

    myFragment = room.myFragment;

    myTiles = room.myTiles;
    myFlags = room.myFlags;
    mySmoke = room.mySmoke;
    myDists = room.myDists;
    resize(room.myWidth, room.myHeight);

    myConstantFlags = room.myConstantFlags;
    myMaxSmoke = room.myMaxSmoke;

    myPortals = room.myPortals;
    myId = room.myId;
    mySeed = room.mySeed;
    myType = room.myType;
    myDepth = room.myDepth;
    mySymbol = room.mySymbol;

    myAtlasX = room.myAtlasX;
    myAtlasY = room.myAtlasY;
    memcpy(myConnectivity, room.myConnectivity, sizeof(u8) * 4 );

    // This usually requires some one to explicity call setMap()
    myMap = 0;

    return *this;
}


ROOM::~ROOM()
{
    deleteContents();
}

void
ROOM::deleteContents()
{
    myPortals.clear();

    myTiles.reset();
    myFlags.reset();
    mySmoke.reset();
    myDists.reset();
}


TILE_NAMES
ROOM::getTile(int x, int y) const
{
    if (x < 0 || x >= width() || y < 0 || y >= height())
	return TILE_INVALID;

    return (TILE_NAMES) myTiles.get(x, y);
}

void
ROOM::setTile(int x, int y, TILE_NAMES tile)
{
    J_ASSERT(x >= 0 && x < width() && y >= 0 && y < height());
    TILE_NAMES oldtile = getTile(x, y);
    if (tile != oldtile)
    {
	myTiles.makeUnique();
	myTiles.set(x, y, tile);
    }
}

MAPFLAG_NAMES
ROOM::getFlag(int x, int y) const
{
    if (x < 0 || x >= width() || y < 0 || y >= height())
	return MAPFLAG_NONE;
    return (MAPFLAG_NAMES) myFlags.get(x, y);
}

void
ROOM::setSmoke(int x, int y, int smoke)
{
    J_ASSERT(x >= 0 && x < width() && y >= 0 && y < height());
    if (smoke < 0) smoke = 0;
    if (smoke > 255) smoke = 255;

    int oldsmoke = getSmoke(x, y);

    if (oldsmoke != smoke)
    {
	mySmoke.makeUnique();
	mySmoke.set(x, y, smoke);
	myMaxSmoke = MAX(myMaxSmoke, smoke);
    }
}

int
ROOM::getSmoke(int x, int y) const
{
    if (x < 0 || x >= width() || y < 0 || y >= height())
	return 0;
    if (!myMaxSmoke)
	return 0;
    return mySmoke.get(x, y);
}

void
ROOM::simulateSmoke(int colour)
{
    int		x, y;

    // Trivial if we have no smoke
    if (!myMaxSmoke)
	return;
    const int SMOKE_FADE = 1;
    const int SMOKE_SPREAD = 5;

    // Always will change and we write directly
    mySmoke.makeUnique();

    POS		pos;

    FORALL_XY(x, y)
    {
	if ( ((x ^ y) & 1) != colour )
	    continue;

	int		smoke = mySmoke.get(x, y);
	if (!smoke)
	    continue;

	smoke -= SMOKE_FADE;
	if (smoke < 0)
	{
	    smoke = 0;
	    mySmoke.set(x,y, smoke);
	    continue;
	}
	int spread = (smoke * SMOKE_SPREAD) / 100;

	// Divide in each direction.

	int totalspread = 0;
	int		dx, dy;
	FORALL_4DIR(dx, dy)
	{
	    pos = buildPos(x, y).delta(dx, dy);
	    int		destsmoke = pos.smoke();

	    int		newdest = destsmoke + spread;
	    if (newdest > 255)
		newdest = 255;

	    // Recompute how much actually spread.
	    totalspread += newdest - destsmoke;

	    pos.setSmoke(newdest);
	}
	mySmoke.set(x, y, smoke - totalspread);
    }
}

void
ROOM::updateMaxSmoke()
{
    int		x, y;
    // Can't update.
    if (!myMaxSmoke)
	return;
    myMaxSmoke = 0;
    FORALL_XY(x, y)
    {
	int	smoke = mySmoke.get(x, y);
	myMaxSmoke = MAX(smoke, myMaxSmoke);
    }
}

void
ROOM::setFlag(int x, int y, MAPFLAG_NAMES flag, bool state)
{
    J_ASSERT(x >= 0 && x < width() && y >= 0 && y < height());

    u8 oldflag = myFlags.get(x, y);
    if (state)
    {
	if (oldflag & flag)
	{
	    // No change
	}
	else
	{
	    // Set
	    oldflag |= flag;
	    myFlags.makeUnique();
	    myFlags.set(x, y, oldflag);
	    myConstantFlags &= ~flag;
	}
    }
    else
    {
	if (oldflag & flag)
	{
	    // Clear
	    oldflag &= ~flag;
	    myFlags.makeUnique();
	    myFlags.set(x, y, oldflag);
	    myConstantFlags &= ~flag;
	}
	else
	{
	    // No change.
	}
    }
}

void
ROOM::setAllFlags(MAPFLAG_NAMES flag, bool state)
{
    if (myConstantFlags & flag)
    {
	setFlag(0, 0, flag, state);

	// If not cleared out on set, no one will clear it!
	if (myConstantFlags & flag)
	    return;
    }

    for (int y = 0; y < height(); y++)
	for (int x = 0; x < width(); x++)
	    setFlag(x, y, flag, state);
    // At this point we are constant!
    myConstantFlags |= flag;
}

int
ROOM::getDistance(int mapnum, int x, int y) const
{
    J_ASSERT(x >= 0 && x < width() && y >= 0 && y < height());
    return myDists.get(x, y + mapnum * height());
}

void
ROOM::setDistance(int mapnum, int x, int y, int dist)
{
    J_ASSERT(x >= 0 && x < width() && y >= 0 && y < height());
    myDists.makeUnique();
    myDists.set(x, y + mapnum * height(), dist);
}

void
ROOM::clearDistMap(int mapnum)
{
    myDists.makeUnique();
    memset(&myDists.writeableData()[mapnum * width() * height()], 0xff, sizeof(int) * width() * height());
}

void
ROOM::setMap(MAP *map)
{
    int			i;

    myMap = map;

    for (i = 0; i < myPortals.entries(); i++)
    {
	PORTAL	p = myPortals(i);
	p.mySrc.setMap(map);
	p.myDst.setMap(map);
	myPortals.set(i, p);
    }
}

void
ROOM::resize(int w, int h)
{
    myWidth = w;
    myHeight = h;

    myTiles.resize(myWidth, myHeight);
    myFlags.resize(myWidth, myHeight);
    mySmoke.resize(myWidth, myHeight);

    // We are now constant!
    myConstantFlags = 0xff;

    // If we have distances, we'd want this.
    myDists.resize(myWidth, myHeight * DISTMAP_CACHE);
}

void
ROOM::rotateEntireRoom(int angle)
{
    // Ensure we are odd!
    J_ASSERT(width() & 1);
    J_ASSERT(height() & 1);
    J_ASSERT(width() == height());

    int		cx = width()/2;
    int		cy = height()/2;

    TEXTURE8 	newtiles(height(), width());
    TEXTURE8 	newflags(height(), width());
    TEXTUREI	newdists(height(), width() * DISTMAP_CACHE);

    // Flop everything.
    for (int y = 0; y < height(); y++)
    {
	for (int x = 0; x < width(); x++)
	{
	    int		nx, ny;
	    nx = x;
	    ny = y;
	    nx -= cx;
	    ny -= cy;
	    POS::rotateByAngle(angle, nx, ny);
	    nx += cx;
	    ny += cy;

	    newtiles.set(nx, ny, getTile(x, y));
	    newflags.set(nx, ny, getFlag(x, y));
	    for (int m = 0; m < DISTMAP_CACHE; m++)
	    {
		newdists.set(nx, ny + m * width(), 
			     myDists.get(x, y + m * height()));
	    }
	}
    }

    myDists = newdists;
    myTiles = newtiles;
    myFlags = newflags;
    // Note constant flags remain invariant under rotation.

    // TODO: Possibly rotate our width/ehgiht!

    // TODO: Portals are orphaned!

    // Cannot rotate mobs as they are globally registered!
    J_ASSERT(false);

#if 0
    for (int i = 0; i < myItems.entries(); i++)
    {
	POS		p = myItems(i)->pos();

	int	nx, ny;
	nx = p.myX;
	ny = p.myY;
	nx -= cx;
	ny -= cy;
	POS::rotateByAngle(angle, nx, ny);
	nx += cx;
	ny += cy;

	p = buildPos(nx, ny);
	myItems(i)->silentMove(p);
    }

    for (int i = 0; i < myMobs.entries(); i++)
    {
	POS		p = myMobs(i)->pos();

	int	nx, ny;
	nx = p.myX;
	ny = p.myY;
	nx -= cx;
	ny -= cy;
	POS::rotateByAngle(angle, nx, ny);
	nx += cx;
	ny += cy;

	POS		np = buildPos(nx, ny);
	np.setAngle(p.angle() + angle);

	myMobs(i)->silentMove(np);
    }
#endif
}

POS
ROOM::buildPos(int x, int y) const
{
    POS		p;

    p.myMap = myMap;
    p.myRoomId = myId;
    p.myX = x;
    p.myY = y;
    p.myAngle = 0;

    return p;
}

POS
ROOM::getRandomPos(POS p, int *n) const
{
    POS		np;
    int		x, y;
    int		ln = 1;

    if (!n)
	n = &ln;

    np.myMap = myMap;
    np.myRoomId = myId;
    np.myAngle = rand_choice(4);

    for (y = 0; y < height(); y++)
    {
	for (x = 0; x < width(); x++)
	{
	    np.myX = x; np.myY = y;

	    if (np.isPassable() && !np.mob())
	    {
		if (!rand_choice(*n))
		{
		    p = np;
		}
		// I hate ++ on * :>
		*n += 1;
	    }
	}
    }

    return p;
}

POS
ROOM::getRandomTile(TILE_NAMES tile, POS p, int *n) const
{
    POS		np;
    int		x, y;
    int		ln = 1;

    if (!n)
	n = &ln;

    np.myMap = myMap;
    np.myRoomId = myId;
    np.myAngle = rand_choice(4);

    for (y = 0; y < height(); y++)
    {
	for (x = 0; x < width(); x++)
	{
	    np.myX = x; np.myY = y;

	    if (np.tile() == tile)
	    {
		if (!rand_choice(*n))
		{
		    p = np;
		}
		// I hate ++ on * :>
		*n += 1;
	    }
	}
    }

    return p;
}

int
ROOM::getConnectedRooms(ROOMLIST &list) const
{
    for (int i = 0; i < myPortals.entries(); i++)
    {
	ROOM	*dst = myPortals(i).myDst.room();

	if (list.find(dst) < 0)
	{
	    list.append(dst);
	}
    }

    return list.entries();
}

PORTAL *
ROOM::getPortal(int x, int y) const
{
    int			i;

    // Quick test.
    if (!(getFlag(x, y) & MAPFLAG_PORTAL))
	return 0;

    for (i = 0; i < myPortals.entries(); i++)
    {
	if (myPortals(i).mySrc.myX == x && myPortals(i).mySrc.myY == y)
	{
	    return myPortals.rawptr(i);
	}
    }

    return 0;
}

void
ROOM::removePortal(POS dst)
{
    int			i;

    for (i = 0; i < myPortals.entries(); i++)
    {
	if (myPortals(i).myDst == dst)
	{
	    myPortals(i).mySrc.setFlag(MAPFLAG_PORTAL, false);
	    myPortals(i).myDst.setFlag(MAPFLAG_PORTAL, false);
	    myPortals.removeAt(i);
	    i--;
	}
    }
}

void
ROOM::removeAllForeignPortals()
{
    int			i;

    for (i = 0; i < myPortals.entries(); i++)
    {
	if (myPortals(i).myDst.roomType() != type())
	{
	    myPortals(i).mySrc.setFlag(MAPFLAG_PORTAL, false);
	    myPortals(i).myDst.setFlag(MAPFLAG_PORTAL, false);
	    myPortals.removeAt(i);
	    i--;
	}
    }
}

void
ROOM::removeAllPortalsTo(ROOMTYPE_NAMES roomtype)
{
    int			i;

    for (i = 0; i < myPortals.entries(); i++)
    {
	if (myPortals(i).myDst.roomType() == roomtype)
	{
	    myPortals(i).mySrc.setFlag(MAPFLAG_PORTAL, false);
	    myPortals(i).myDst.setFlag(MAPFLAG_PORTAL, false);
	    myPortals.removeAt(i);
	    i--;
	}
    }
}

void
ROOM::removeAllPortals()
{
    myPortals.clear();
}

POS
ROOM::findProtoPortal(int dir)
{
    int		dx, dy, x, y, nfound;
    POS	result;

    rand_getdirection(dir, dx, dy);

    nfound = 0;

    for (y = 0; y < height(); y++)
    {
	for (x = 0; x < width(); x++)
	{
	    if (getTile(x, y) == TILE_PROTOPORTAL ||
	        getTile(x, y) == TILE_MOUNTAINPROTOPORTAL)
	    {
		if ((getTile(x+dx, y+dy) == TILE_INVALID)
			&&
		    ( getTile(x-dx, y-dy) == TILE_FLOOR ||
		      getTile(x-dx, y-dy) == TILE_ALTAR ||
		      getTile(x-dx, y-dy) == TILE_GRASS
		     ))
		{
		    nfound++;
		    if (!rand_choice(nfound))
		    {
			result = buildPos(x, y);
		    }
		}
	    }
	}
    }

    if (nfound)
    {
    }
    else
    {
	{
	    cerr << "Failed to locate on map " << "Damn so that is what my fragment was used for!!!" << endl;
	    if (myFragment)
	    {
		if (myFragment->myFName.buffer())
		    cerr << myFragment->myFName.buffer() << endl;
		cerr << "Direction was " << dir << " type is " << glb_roomtypedefs[myType].prefix << endl;
	    }
	}
    }
    return result;
}

void
ROOM::revertToProtoPortal()
{
    int		x, y;

    for (y = 0; y < height(); y++)
    {
	for (x = 0; x < width(); x++)
	{
	    if (getFlag(x, y) & MAPFLAG_PORTAL)
	    {
		if (getTile(x, y) == TILE_FLOOR)
		    setTile(x, y, TILE_PROTOPORTAL);
		else if (getTile(x, y) == TILE_SOLIDWALL)
		    setTile(x, y, TILE_INVALID);
		setFlag(x, y, MAPFLAG_PORTAL, false);
	    }
	}
    }
    // Clear our portal list now.
    myPortals.clear();
}

POS
ROOM::findUserProtoPortal(int dir)
{
    int		dx, dy, x, y, nfound;
    POS	result;

    rand_getdirection(dir, dx, dy);

    nfound = 0;

    for (y = 0; y < height(); y++)
    {
	for (x = 0; x < width(); x++)
	{
	    if (getTile(x, y) == TILE_USERPROTOPORTAL)
	    {
		if (( getTile(x+dx, y+dy) == TILE_INVALID ||
		      getTile(x+dx, y+dy) == defn().wall_tile ||
		      getTile(x+dx, y+dy) == defn().tunnelwall_tile
		     )
		     &&
		    ( getTile(x-dx, y-dy) == TILE_FLOOR ||
		      getTile(x-dx, y-dy) == TILE_GRASS ||
		      getTile(x-dx, y-dy) == TILE_FOREST
		     ))
		{
		    nfound++;
		    if (!rand_choice(nfound))
		    {
			result = buildPos(x, y);
		    }
		}
	    }
	}
    }

    if (nfound)
    {
    }
    else
    {
	{
	    cerr << "Failed to locate on map " << "Damn so that is what my fragment was used for!!!" << endl;
	    if (myFragment)
	    {
		cerr << myFragment->myFName.buffer() << endl;
		cerr << "Direction was " << dir << " type is " << glb_roomtypedefs[myType].prefix << endl;
	    }
	}
    }
    return result;
}

POS
ROOM::findCentralPos() const
{
    POS		center;

    center = buildPos(width()/2, height()/2);
    return spiralSearch(center, 0, false);
}

#define TEST_TILE(x, y) \
if ( POS::defn(getTile(x, y)).ispassable && (!avoidmob || !buildPos(x, y).mob()) )\
    return buildPos(x, y);

POS
ROOM::spiralSearch(POS start, int startradius, bool avoidmob) const
{
    // Spiral search for passable tile from the center.
    int		x = start.myX, y = start.myY;
    int		ir, r, maxr = MAX(width(), height())+1;
    POS		result;

    for (r = startradius; r < maxr; r++)
    {
	for (ir = -r; ir <= r; ir++)
	{
	    TEST_TILE(x+ir, y+r)
	    TEST_TILE(x+ir, y-r)
	    TEST_TILE(x+r, y+ir)
	    TEST_TILE(x-r, y+ir)
	}
    }

    J_ASSERT(!"No valid squares in a room.");
    return result;
}
#undef TEST_TILE

bool
ROOM::buildPortal(POS a, int dira, POS b, int dirb, bool settiles)
{
    PORTAL		 patob, pbtoa;

    if (!a.valid() || !b.valid())
	return false;

    patob.mySrc = a.delta4Direction(dira);
    patob.myDst = b;

    pbtoa.mySrc = b.delta4Direction(dirb);
    pbtoa.myDst = a;

    if (settiles)
    {
	patob.mySrc.setTile(TILE_SOLIDWALL);
	pbtoa.mySrc.setTile(TILE_SOLIDWALL);
    }

    patob.mySrc.myAngle = 0;
    pbtoa.mySrc.myAngle = 0;

    if (settiles)
    {
	patob.myDst.setTile(TILE_FLOOR);
	pbtoa.myDst.setTile(TILE_FLOOR);
    }
    patob.myDst.setFlag(MAPFLAG_PORTAL, true);
    patob.mySrc.setFlag(MAPFLAG_PORTAL, true);
    pbtoa.myDst.setFlag(MAPFLAG_PORTAL, true);
    pbtoa.mySrc.setFlag(MAPFLAG_PORTAL, true);

    patob.myDst.myAngle = (dira - dirb + 2) & 3;
    pbtoa.myDst.myAngle = (dirb - dira + 2) & 3;

    a.room()->myPortals.append(patob);
    b.room()->myPortals.append(pbtoa);

    return true;
}



bool
ROOM::link(ROOM *a, int dira, ROOM *b, int dirb)
{
    POS		aseed, bseed;
    POS		an, bn;

    aseed = a->findProtoPortal(dira);
    bseed = b->findProtoPortal(dirb);

    if (!aseed.valid() || !bseed.valid())
	return false;

    // This code is to ensure we do as thick a match
    // as possible.
#if 1
    // Grow to the top.
    an = aseed;
    while (an.tile() == TILE_PROTOPORTAL)
    {
	aseed = an;
	an = an.delta4Direction(dira+1);
    }
    bn = bseed;
    while (bn.tile() == TILE_PROTOPORTAL)
    {
	bseed = bn;
	bn = bn.delta4Direction(dirb-1);
    }

#if 1
    int asize = 0, bsize = 0;

    an = aseed;
    while (an.tile() == TILE_PROTOPORTAL)
    {
	asize++;
	an = an.delta4Direction(dira-1);
    }
    bn = bseed;
    while (bn.tile() == TILE_PROTOPORTAL)
    {
	bsize++;
	bn = bn.delta4Direction(dirb+1);
    }

    if (asize > bsize)
    {
	int		off = rand_choice(asize - bsize);

	while (off)
	{
	    off--;
	    aseed = aseed.delta4Direction(dira-1);
	    J_ASSERT(aseed.tile() == TILE_PROTOPORTAL);
	}
    }
    else if (bsize > asize)
    {
	int		off = rand_choice(bsize - asize);

	while (off)
	{
	    off--;
	    bseed = bseed.delta4Direction(dirb+1);
	    J_ASSERT(bseed.tile() == TILE_PROTOPORTAL);
	}
    }
#endif
#endif

    ROOM::buildPortal(aseed, dira, bseed, dirb);

    // Grow our portal in both directions as far as it will go.
    an = aseed.delta4Direction(dira+1);
    bn = bseed.delta4Direction(dirb-1);
    while (an.tile() == TILE_PROTOPORTAL && bn.tile() == TILE_PROTOPORTAL)
    {
	ROOM::buildPortal(an, dira, bn, dirb);
	an = an.delta4Direction(dira+1);
	bn = bn.delta4Direction(dirb-1);
    }

    an = aseed.delta4Direction(dira-1);
    bn = bseed.delta4Direction(dirb+1);
    while (an.tile() == TILE_PROTOPORTAL && bn.tile() == TILE_PROTOPORTAL)
    {
	ROOM::buildPortal(an, dira, bn, dirb);
	an = an.delta4Direction(dira-1);
	bn = bn.delta4Direction(dirb+1);
    }

    return true;
}

bool
ROOM::forceConnectNeighbours()
{
    bool		interesting = false;

    int		dx, dy;
    FORALL_4DIR(dx, dy)
    {
	if (myConnectivity[lcl_angle] == ROOM::CONNECTED)
	{
	    continue;
	}
	if (myConnectivity[lcl_angle] == ROOM::UNDECIDED)
	{
	    // We are doing pure implicit connections here.
	    myConnectivity[lcl_angle] = ROOM::IMPLICIT;
	}

	if (myConnectivity[lcl_angle] == ROOM::DISCONNECTED)
	{
	    continue;
	}
    }

    map()->expandActiveRooms(getId(), 2);

    return interesting;
}

///
/// FRAGMENT definition and functions
///


FRAGMENT::FRAGMENT(const char *fname)
{
    ifstream		is(fname);
    char		line[500];
    PTRLIST<char *>	l;
    int			i, j;

    myFName.strcpy(fname);

    while (is.getline(line, 500))
    {
	text_striplf(line);

	// Ignore blank lines...
	if (line[0])
	    l.append(glb_strdup(line));
    }

    if (l.entries() < 1)
    {
	cerr << "Empty map fragment " << fname << endl;
	exit(-1);
    }

    // Find smallest non-whitespace square.
    int			minx, maxx, miny, maxy;

    for (miny = 0; miny < l.entries(); miny++)
    {
	if (text_hasnonws(l(miny)))
	    break;
    }

    for (maxy = l.entries(); maxy --> 0; )
    {
	if (text_hasnonws(l(maxy)))
	    break;
    }

    if (miny > maxy)
    {
	cerr << "Blank map fragment " << fname << endl;
	exit(-1);
    }

    minx = MYstrlen(l(miny));
    maxx = 0;
    for (i = miny; i <= maxy; i++)
    {
	minx = MIN(minx, text_firstnonws(l(i)));
	maxx = MAX(maxx, text_lastnonws(l(i)));
    }

    // Joys of rectangles with inconsistent exclusivity!
    // pad everything by 1.
    myW = maxx - minx + 1 + 2;
    myH = maxy - miny + 1 + 2;

    myTiles = new u8 [myW * myH];

    memset(myTiles, ' ', myW * myH);

    for (i = 1; i < myH-1; i++)
    {
	if (MYstrlen(l(i+miny-1)) < myW + minx - 2)
	{
	    cerr << "Short line in fragment " << fname << endl;
	    exit(-1);
	}
	for (j = 1; j < myW-1; j++)
	{
	    myTiles[i * myW + j] = l(i+miny-1)[j+minx-1];
	}
    }

    for (i = 0; i < l.entries(); i++)
	free(l(i));
}

FRAGMENT::FRAGMENT(int w, int h)
{
    myW = w;
    myH = h;
    myTiles = new u8 [myW * myH];

    memset(myTiles, ' ', myW * myH);
}

FRAGMENT::~FRAGMENT()
{
    delete [] myTiles;
}

void
FRAGMENT::swizzlePos(int &x, int &y, int orient) const
{
    if (orient & ORIENT_FLIPX)
    {
	x = myW - x - 1;
    }

    if (orient & ORIENT_FLIPY)
    {
	y = myH - y - 1;
    }

    if (orient & ORIENT_FLOP)
    {
	int	t = y;
	y = x;
	x = t;
    }
}

ROOM *
FRAGMENT::buildRoom(MAP *map, ROOMTYPE_NAMES type, int difficulty, int depth, int atlasx, int atlasy) const
{
    ROOM	*room;
    int		 x, y, rx, ry;
    MOB		*mob;
    ITEM	*item;
    TILE_NAMES	 tile;
    MAPFLAG_NAMES flag;
    int		 orient = 0;

    if (glb_roomtypedefs[type].randomorient)
	orient = rand_choice(7);

    room = new ROOM();
    room->myFragment = this;

    if (orient & ORIENT_FLOP)
	room->resize(myH, myW);
    else
	room->resize(myW, myH);
    room->setMap(map);
    room->myType = type;
    room->myDepth = depth;
    room->myAtlasX = atlasx;
    room->myAtlasY = atlasy;

    for (int angle = 0; angle < 4; angle++)
    {
	if (glb_roomtypedefs[type].deadend)
	    room->myConnectivity[angle] = ROOM::DISCONNECTED;
	else
	    room->myConnectivity[angle] = ROOM::UNDECIDED;
    }

    // If this is a generator, build a new room!
    if (glb_roomtypedefs[type].usegenerator)
    {
	BUILDER		builder(myTiles, myW, myH, myW, depth);

	if (type == ROOMTYPE_WILDERNESS)
	{
	    TERRAIN_NAMES		terrain[9];
	    for (int i = 0; i < 9; i++)
		terrain[i] = TERRAIN_PLAINS;
	    builder.buildWilderness(terrain);
	}
	else
	{
	    builder.build(type);
	}
    }

    room->myId = map->myRooms.entries();
    map->myRooms.append(room);

    int		ngold = 0;
    int		nitem = 0;
    int		nmob = 0;
    
    for (y = 0; y < myH; y++)
    {
	for (x = 0; x < myW; x++)
	{
	    rx = x;
	    ry = y;
	    swizzlePos(rx, ry, orient);

	    tile = TILE_INVALID;
	    flag = MAPFLAG_NONE;
	    TILE_NAMES floortile = (TILE_NAMES) glb_roomtypedefs[type].floor_tile;
	    switch (myTiles[x + y * myW])
	    {
		case 'A':		// any monster
		{
		    int		threat = difficulty;
		    mob = MOB::createNPC(threat);
		    if (mob)
			mob->move(room->buildPos(rx, ry));
		    tile = floortile;
		    break;
		}

		case 'B':		// The BOSS!
		    mob = MOB::create(MOB::getBossName());
		    if (mob)
		    {
			mob->move(room->buildPos(rx, ry));
			mob->giftItem(ITEM_MACGUFFIN);
			mob->gainExp(LEVEL::expFromLevel(difficulty), /*silent=*/true);
		    }
		    tile = floortile;
		    break;

		case '(':
		    item = ITEM::createRandomBalanced(difficulty);
		    if (item)
			item->move(room->buildPos(rx, ry));
		    nitem++;
		    tile = floortile;
		    break;

		case 'S':
		    tile = TILE_SECRETDOOR;
		    break;

		case '%':
		{
#if 0
		    ITEM_NAMES		list[5] =
		    {
			ITEM_SAUSAGES,
			ITEM_BREAD,
			ITEM_MUSHROOMS,
			ITEM_APPLE,
			ITEM_PICKLE
		    };
		    item = ITEM::create(list[rand_choice(5)]);
		    item->move(room->buildPos(rx, ry));
#endif
		    tile = floortile;
		    break;
		}

		case 'Q':
		    tile = TILE_MEDITATIONSPOT;
		    break;

		case '$':
		    item = ITEM::create(ITEM_COIN);
		    item->setMaterial(MATERIAL_GOLD);
		    // average 13, median 15!
		    item->setStackCount(rand_roll(15+difficulty, 1));
		    item->move(room->buildPos(rx, ry));
		    ngold++;
		    tile = floortile;
		    break;

		case ';':
		{
		    tile = TILE_SNOWYPATH;
		    break;
		}

		case '.':
		    tile = floortile;
		    break;

		case '\'':
		    tile = TILE_GRASS;
		    break;

		case 'x':
		    tile = (TILE_NAMES) glb_roomtypedefs[type].door_tile;
		    break;

		case 'K':
		    item = ITEM::create(ITEM_STATUE, depth);
		    item->move(room->buildPos(rx, ry));
		    tile = floortile;
		    break;

		case '_':
		    tile = TILE_DIRT;
		    break;

		case '?':
		    item = ITEM::create(ITEM_SHELF, depth);
		    item->move(room->buildPos(rx, ry));
		    tile = floortile;
		    break;

		case '#':
		    tile = (TILE_NAMES) glb_roomtypedefs[type].wall_tile;
		    break;

		case 'F':
		    tile = TILE_FUTURE_FORGE;
		    break;

		case '&':
		    tile = TILE_FOREST;
		    break;

		case '*':
		    tile = TILE_BUSH;
		    break;

		case 'O':
		    item = ITEM::create(ITEM_TABLE, depth);
		    item->move(room->buildPos(rx, ry));
		    tile = floortile;
		    break;

		case 'h':
		    item = ITEM::create(ITEM_CHAIR, depth);
		    item->move(room->buildPos(rx, ry));
		    tile = floortile;
		    break;

		case '+':
		    if (type == ROOMTYPE_VILLAGE)
			tile = TILE_MOUNTAINPROTOPORTAL;
		    else
			tile = TILE_PROTOPORTAL;
		    break;

		case '>':
		    tile = TILE_DOWNSTAIRS;
		    break;

		case '<':
		    tile = TILE_UPSTAIRS;
		    break;

		case ' ':
		    tile = (TILE_NAMES) glb_roomtypedefs[type].tunnelwall_tile;
		    break;
		case '"':
		    tile = TILE_FIELD;
		    break;
		case ',':
		    tile = (TILE_NAMES) glb_roomtypedefs[type].path_tile;
		    break;
		case '~':
		    tile = TILE_WATER;
		    break;
		case '^':
		    tile = TILE_MOUNTAIN;
		    break;
		case 'P':
		    tile = TILE_USERPROTOPORTAL;
		    break;
		case 'V':
		    tile = TILE_ICEMOUNTAIN;
		    break;
		case 'W':
		    tile = TILE_SNOWYPASS;
		    break;
		case '=':
		    tile = TILE_BRIDGE;
		    break;
		case ':':
		    item = ITEM::create(ITEM_BED, depth);
		    item->move(room->buildPos(rx, ry));
		    tile = floortile;
		    break;
		case '1':
		case '2':
		case '3':
		case '4':
		    tile = floortile;
		    mob = MOB::createHero((HERO_NAMES) (myTiles[x + y * myW] - '0' + HERO_NONE + 1));
		    if (mob)
			mob->move(room->buildPos(rx, ry));
		    break;

		case 'w':
		    item = ITEM::create(ITEM_WISH_STONE, difficulty);
		    item->move(room->buildPos(rx, ry));
		    tile = floortile;
		    break;

		case 'p':
		    item = ITEM::create(ITEM_PORTAL_STONE, difficulty);
		    item->move(room->buildPos(rx, ry));
		    tile = floortile;
		    break;

		default:
		    fprintf(stderr, "Unknown map char %c\r\n",
			    myTiles[x + y * myW]);
	    }

	    // While I like the traps, they need to be rare enough
	    // not to force people to search all the time.  But then
	    // they just become surprise insta-deaths?
	    // So, compromise: traps do very little damage so act
	    // primarily as flavour.  Which in traditional roguelike
	    // fashion is always excused.
	    if (tile == floortile && 
		!rand_choice(500) && type != ROOMTYPE_VILLAGE
		&& NUM_TRAPS > 1)
	    {
		flag = MAPFLAG_TRAP;
	    }
	    room->setTile(rx, ry, tile);
	    room->setFlag(rx, ry, flag, true);
	}
    }

    return room;
}


static ATOMIC_INT32 glbMapId;

///
/// MAP functions
///
MAP::MAP(int depth, MOB *avatar, DISPLAY *display)
{
    myRefCnt.set(0);
    myParty.setMap(this);
    myWorldLevel = 1;

    myUniqueId = glbMapId.add(1);
    myAllowDelays = false;

    // We have a zero at the front to keep the uid == 0 free.
    myLiveMobs.append(nullptr);

    rebuild(depth, avatar, display, true);
}

void
MAP::rebuild(int depth, MOB *avatar, DISPLAY *display, bool climbeddown)
{
    myFOVCache = 0;
    myAvatar = avatar;

    for (int i = 0; i < 2; i++)
	myUserPortalDir[i] = 0;

    myMobCount.clear();

    FRAGMENT		*frag;
    ROOM		*village = nullptr;
    ROOM		*astralplane = nullptr;

    // Ensure seed room is built.
    {
	ROOMTYPE_NAMES roomtype = ROOMTYPE_VILLAGE;

	frag = theFragRooms[roomtype](rand_choice(theFragRooms[roomtype].entries()));
	village = frag->buildRoom(this, roomtype, 1, depth, 0, 0);
    }

    {
	ROOMTYPE_NAMES roomtype = ROOMTYPE_BIGROOM;

	frag = theFragRooms[roomtype](rand_choice(theFragRooms[roomtype].entries()));
	astralplane = frag->buildRoom(this, roomtype, 0, 0, -1, -1);
    }

    if (avatar)
    {
	myAvatar = avatar;
	avatar->setHero(HERO_AVATAR);
	POS		goal;

	// Build a useless room to hold the avatar.

	ROOMTYPE_NAMES roomtype = ROOMTYPE_WILDERNESS;

	frag = theFragRooms[roomtype](rand_choice(theFragRooms[roomtype].entries()));
	goal = village->getRandomTile(
			climbeddown ? TILE_UPSTAIRS
				    : TILE_DOWNSTAIRS,
			POS(), 0);
	if (goal.mob())
	{
	    // Find non-mob spot at the ladder
	    goal = village->spiralSearch(goal, 1, true);
	}
	if (!goal.valid())
	    goal = village->getRandomPos(POS(), 0);
	// Keep us oriented consistently.
	//goal = goal.rotate(rand_choice(4));
	goal.setAngle(0);
	avatar->move(goal);

	// Create our default party of one person.
	myParty.replace(0, avatar);
	myParty.makeActive(0);

	// Ensure our avatar is legal and flagged.
	avatar->updateEquippedItems();
    }

    myDisplay = display;

    for (int i = 0; i < DISTMAP_CACHE; i++)
    {
	myDistMapWeight[i] = 0.0;
    }
}

void
MAP::ascendWorld()
{

    msg_report("The ground shakes as the world transforms into a darker, more dangerous, place.");

    myWorldLevel++;

    // As a neat trick, your portals into the old world are valid!
    // But the stair case will now take you to the new world.  This
    // is done by incrementing the y atlas.  (it's stair cases are also
    // broken... oh well!)
    for (int i = 0; i < myRooms.entries(); i++)
    {
	ROOM *room = myRooms(i);
	if (!room)
	    continue;
	// Ignore astral plane
	if (room->atlasX() < 0 || room->atlasY() < 0)
	    continue;
	// Ignore the village
	if (room->atlasX() == 0 && room->atlasY() == 0)
	    continue;
	// Increment the atlas loc
	room->myAtlasY++;
    }
}

void
MAP::expandActiveRooms(int roomid, int depth)
{
    if (roomid < 0)
    {
	// Invalid location, no expand
	return;
    }

    ROOM	*thisroom = myRooms(roomid);
    J_ASSERT(thisroom);
    
    if (depth <= 0)
	return;

#if 0
    // If we want to allow auto-expansion we do this, and need to
    // build cannoical atlas coords.
    int		dx, dy;

    FORALL_4DIR(dx, dy)
    {
	int		x = atlasx + dx;
	int		y = atlasy + dy;
	ROOM		*newroom = findAtlasRoom(x, y);

	if (thisroom->myConnectivity[lcl_angle] == ROOM::UNDECIDED)
	{
	    // It is time to decide!
	    // First we see if the decision has been made.
	    // I think this has to be closed or else we'd already
	    // have generated ourself.
	    if (newroom && newroom->myConnectivity[(lcl_angle+2)%4] != ROOM::UNDECIDED)
	    {
		thisroom->myConnectivity[lcl_angle] = newroom->myConnectivity[(lcl_angle+2)%4];
	    }
	    else
	    {
		if (x >= 0 && x < ATLAS_WIDTH && y >= 0 && y < ATLAS_HEIGHT)
		{
		    thisroom->myConnectivity[lcl_angle] = ROOM::IMPLICIT;
		}
		else
		{
		    thisroom->myConnectivity[lcl_angle] = ROOM::DISCONNECTED;
		}
	    }
	    if (newroom && newroom->myConnectivity[(lcl_angle+2)%4] == ROOM::UNDECIDED)
	    {
		// We need to update the new room with our choice.
		// We can technically wait to actually update its status
		// as it will inherit it from us.
		// However, it is important we build the link if
		// we discover it here!
		newroom->myConnectivity[(lcl_angle + 2)%4] = thisroom->myConnectivity[lcl_angle];
		if (thisroom->myConnectivity[lcl_angle] == ROOM::CONNECTED)
		{
		    // Force the new room to be connected to us!
		    // We ignore its other outlets as they will all be UNDECIDED.
		    bool		built;
		    built = ROOM::link( thisroom, lcl_angle,
				newroom, (lcl_angle+2) % 4);
		}
	    }
	}

	// Only expand if we want to be connected this way?
	if (thisroom->myConnectivity[lcl_angle] == ROOM::DISCONNECTED)
	{
	    // Chose not to be connected.
	    continue;
	}

	if (!newroom)
	{
	    // Must build a new room at this loc.
	    ROOMTYPE_NAMES	roomtype = transitionRoom(thisroom->type());

	    FRAGMENT *frag = theFragRooms[roomtype](rand_choice(theFragRooms[roomtype].entries()));

	    newroom = frag->buildRoom(this, roomtype, MAX(ABS(x), ABS(y)), x, y);
	    // Force the new room to be connected to us!
	    // We ignore its other outlets as they will all be UNDECIDED.
	    // Note this may be implicit or connected
	    newroom->myConnectivity[(lcl_angle + 2)%4] = thisroom->myConnectivity[lcl_angle];

	    if (thisroom->myConnectivity[lcl_angle] == ROOM::CONNECTED)
	    {
		// Requires explicit link!
		bool		built;
		built = ROOM::link( thisroom, lcl_angle,
			    newroom, (lcl_angle+2) % 4);
	    }
	}

	// In any case, expand from here.
	expandActiveRooms(x, y, depth-1);
    }
#endif
}

MAP::MAP(const MAP &map)
{
    myRefCnt.set(0);
    myFOVCache = 0;
    myAllowDelays = false;
    *this = map;
    myParty.setMap(this);
}

MAP &
MAP::operator=(const MAP &map)
{
    if (this == &map)
	return *this;

    deleteContents();

    myMobCount.clear();

    for (int i = 0; i < map.myRooms.entries(); i++)
    {
	myRooms.append(new ROOM(*map.myRooms(i)));
	myRooms(i)->setMap(this);
    }

    for (int i = 0; i < map.myLiveMobs.entries(); i++)
    {
	if (map.myLiveMobs(i))
	{
	    myLiveMobs.append(map.myLiveMobs(i)->copy());
	    myLiveMobs(i)->setMap(this);
	    myMobCount.indexAnywhere(myLiveMobs(i)->getDefinition())++;
	}
	else
	    myLiveMobs.append(nullptr);
    }

    for (int i = 0; i < map.myItems.entries(); i++)
    {
	if (map.myItems(i))
	{
	    myItems.append(map.myItems(i)->copy());
	    myItems(i)->setMap(this);
	}
	else
	    myItems.append(nullptr);
    }

    for (int i = 0; i < 2; i++)
    {
	myUserPortal[i] = map.myUserPortal[i];
	myUserPortal[i].setMap(this);
	myUserPortalDir[i] = map.myUserPortalDir[i];
    }

    for (int i = 0; i < DISTMAP_CACHE; i++)
    {
	myDistMapCache[i] = map.myDistMapCache[i];
	myDistMapCache[i].setMap(this);
	// This is our decay coefficient so stuff eventually will
	// flush out regardless of how popular they were.
	myDistMapWeight[i] = map.myDistMapWeight[i] * 0.75;
    }

    myParty = map.party();
    myParty.setMap(this);
    myWorldLevel = map.worldLevel();
    myAvatar = findAvatar();
    
    myUniqueId = map.getId();

    myDisplay = map.myDisplay;

    // We need to build our own FOV cache to ensure poses refer
    // to us.
    myFOVCache = 0;

    return *this;
}

void
MAP::deleteContents()
{
    reapMobs();

    for (int i = 0; i < myRooms.entries(); i++)
    {
	delete myRooms(i);
    }
    myRooms.clear();

    // We must do a backwards loop as invoking delete on a MOB
    // will then invoke removeMob() on ourselves.
    for (int i = myLiveMobs.entries(); i --> 0; )
    {
	if (myLiveMobs(i))
	{
	    myLiveMobs(i)->clearAllPos();
	    delete myLiveMobs(i);
	}
    }
    myLiveMobs.clear();

    // Must do a backwards loop as delete item will invoke removeItem
    for (int i = myItems.entries(); i --> 0; )
    {
	if (myItems(i))
	{
	    myItems(i)->clearAllPos();
	    delete myItems(i);
	}
    }
    myItems.clear();

    for (int i = 0; i < DISTMAP_CACHE; i++)
    {
	myDistMapWeight[i] = 0.0;
	myDistMapCache[i] = POS();
    }

    for (int i = 0; i < 2; i++)
    {
	myUserPortal[i] = POS();
	myUserPortalDir[i] = 0;
    }

    myAvatar = 0;

    delete myFOVCache;
    myFOVCache = 0;

    // NOTE: Keep the display

    myDelayMobList.clear();
    myMobCount.clear();
}

MAP::~MAP()
{
    deleteContents();
}

void map_buildrandomfloor(TILE_NAMES tile)
{
    TILE_DEF *def = GAMEDEF::tiledef(tile);

    const char *symbols = ".,_'~..-";
    ATTR_NAMES attrs[8] =
    {
	ATTR_BROWN,
	ATTR_WHITE,
	ATTR_DKGREEN,
	ATTR_GREEN,
	ATTR_LIGHTBLACK,
	ATTR_NORMAL,
	ATTR_LTGREY,
	ATTR_LIGHTBROWN
    };


    def->symbol = symbols[rand_choice((int)strlen(symbols))];
    def->attr = attrs[rand_choice(8)];
}

void map_buildrandomdoor(TILE_NAMES tile)
{
    TILE_DEF *def = GAMEDEF::tiledef(tile);

    const char *symbols = "+++XX/\\";
    ATTR_NAMES wallattrs[5] =
    {
	ATTR_BROWN,
	ATTR_WHITE,
	ATTR_NORMAL,
	ATTR_LTGREY,
	ATTR_LIGHTBROWN
    };

    def->symbol = symbols[rand_choice((int)strlen(symbols))];
    def->attr = wallattrs[rand_choice(5)];
}

void map_buildrandomwall(TILE_NAMES tile)
{
    TILE_DEF *def = GAMEDEF::tiledef(tile);

    const char *symbols = "##  &";
    ATTR_NAMES wallattrs[5] =
    {
	ATTR_BROWN,
	ATTR_WHITE,
	ATTR_NORMAL,
	ATTR_LTGREY,
	ATTR_LIGHTBROWN
    };
    ATTR_NAMES treeattrs[4] =
    {
	ATTR_BROWN,
	ATTR_GREEN,
	ATTR_DKGREEN,
	ATTR_LIGHTBROWN
    };
    ATTR_NAMES spaceattrs[4] =
    {
	ATTR_INVULNERABLE,
	ATTR_HILITE,
	ATTR_DKHILITE,
	ATTR_FOOD
    };

    def->symbol = symbols[rand_choice((int)strlen(symbols))];
    if (def->symbol == '#')
	def->attr = wallattrs[rand_choice(5)];
    if (def->symbol == '&')
    {
	def->attr = treeattrs[rand_choice(4)];
	def->legend = "impassable woods";
    }
    if (def->symbol == ' ')
	def->attr = spaceattrs[rand_choice(4)];
}

void map_buildrandomtunnelwall(TILE_NAMES tile)
{
    TILE_DEF *def = GAMEDEF::tiledef(tile);

    const char *symbols = "##    &";
    ATTR_NAMES wallattrs[5] =
    {
	ATTR_BROWN,
	ATTR_WHITE,
	ATTR_NORMAL,
	ATTR_LTGREY,
	ATTR_LIGHTBROWN
    };
    ATTR_NAMES treeattrs[4] =
    {
	ATTR_BROWN,
	ATTR_GREEN,
	ATTR_DKGREEN,
	ATTR_LIGHTBROWN
    };
    ATTR_NAMES spaceattrs[8] =
    {
	ATTR_NORMAL,
	ATTR_NORMAL,
	ATTR_NORMAL,
	ATTR_NORMAL,
	ATTR_INVULNERABLE,
	ATTR_HILITE,
	ATTR_DKHILITE,
	ATTR_FOOD
    };

    def->symbol = symbols[rand_choice((int)strlen(symbols))];
    if (def->symbol == '#')
	def->attr = wallattrs[rand_choice(5)];
    if (def->symbol == '&')
    {
	def->attr = treeattrs[rand_choice(4)];
	def->legend = "impassable woods";
    }
    if (def->symbol == ' ')
	def->attr = spaceattrs[rand_choice(8)];
}

void
MAP::buildLevelList()
{
    const static ROOMTYPE_NAMES levellist[10] =
    {
	ROOMTYPE_ROGUE,
	ROOMTYPE_ROGUE,
	ROOMTYPE_ROGUE,
	ROOMTYPE_CAVE,
	ROOMTYPE_CAVE,
	ROOMTYPE_CAVE,
	ROOMTYPE_DARKCAVE,
	ROOMTYPE_MAZE,
	ROOMTYPE_MAZE,
	ROOMTYPE_BIGROOM		// Rename fortnite :>
    };
    for (int lvl = 0; lvl < GAMEDEF::rules().bosslevel; lvl++)
    {
	LEVEL_NAMES level = GAMEDEF::createNewLevelDef();
	LEVEL_DEF *def = GAMEDEF::leveldef(level);
	// Don't use big room for boss level as it is unplayable.
	def->roomtype = levellist[rand_choice(10 - (lvl == GAMEDEF::rules().bosslevel-1))];
    }

    // Build our look.
    map_buildrandomwall(TILE_WALL);
    map_buildrandomwall(TILE_MAZEWALL);
    map_buildrandomwall(TILE_CAVEWALL);
    map_buildrandomtunnelwall(TILE_TUNNELWALL);
    
    map_buildrandomfloor(TILE_FLOOR);
    map_buildrandomfloor(TILE_CAVEFLOOR);
    map_buildrandomfloor(TILE_DARKCAVEFLOOR);
    map_buildrandomfloor(TILE_MAZEFLOOR);
    map_buildrandomfloor(TILE_PATH);

    map_buildrandomdoor(TILE_DOOR);
    map_buildrandomdoor(TILE_MAZEDOOR);

    // Copy wall colour into broken wall colour.
    GAMEDEF::tiledef(TILE_BROKENWALL)->attr = GAMEDEF::tiledef(TILE_WALL)->attr;
}

void
MAP::incRef()
{
    myRefCnt.add(1);
}

void
MAP::decRef()
{
    int		nval;
    nval = myRefCnt.add(-1);

    if (nval <= 0)
	delete this;
}

int
MAP::refCount() const
{
    return myRefCnt;
}

void
MAP::buildSquare(ROOMTYPE_NAMES roomtype,
	    ROOM *entrance, int enterdir,
	    ROOM *&exit, int &exitdir,
	    int difficulty,
	    int depth,
	    int w, int h)
{
    ROOM		**rooms;

    rooms = new ROOM *[w * h];

    for (int y = 0; y < h; y++)
	for (int x = 0; x < w; x++)
	{
	    FRAGMENT		*frag;

	    frag = theFragRooms[roomtype](rand_choice(theFragRooms[roomtype].entries()));
	    rooms[x + y * w] = frag->buildRoom(this, roomtype, difficulty, depth, x, y);
	}

    // Vertical links
    for (int y = 0; y < h-1; y++)
	for (int x = 0; x < w; x++)
	{
	    ROOM::link( rooms[x + y * w], 2,
			rooms[x + (y+1)*w], 0 );
	}

    // Horizontal links
    for (int y = 0; y < h; y++)
	for (int x = 0; x < w-1; x++)
	{
	    ROOM::link( rooms[x + y * w], 1,
			rooms[x+1 + y * w], 3 );
	}

    // Link in the entrance.
    // Since we are all orientation invariant, we can always link into
    // our LHS.
    int x, y;
    x = 0;
    y = rand_choice(h);
    ROOM::link( entrance, enterdir,
		rooms[x + y * w], 3 );
    // Now choose an exit...
    int		v = rand_choice( (w-1) * 2 + h );
    if (v < w-1)
    {
	exit = rooms[v + 1 + 0 * w];
	exitdir = 0;
    }
    else
    {
	v -= w-1;
	if (v < w -1)
	{
	    exit = rooms[v + 1 + (h-1) * w];
	    exitdir = 2;
	}
	else
	{
	    v -= w - 1;
	    J_ASSERT(v < h);
	    exit = rooms[(w-1) + v * w];
	    exitdir = 1;
	}
    }

    delete [] rooms;
}

void
MAP::moveAvatarToDepth(int newdepth)
{
    MOB		*thisavatar = avatar();

    if (!thisavatar)
	return;

    // Clamp the lowest depth as we index the level array.
    if (newdepth < 0)
	newdepth = 0;

    DISPLAY	*display = myDisplay;
    bool	 down = newdepth > thisavatar->pos().depth();
    bool	 newlevel = false;

    // We grow faster than the actual levels conquered to keep things
    // spicy.
    int		 difficulty = newdepth + (worldLevel() -1) * 13;

    // Query about new level.
    while (GAMEDEF::getNumLevel() <= newdepth)
    {
	newlevel = true;
	GAMEDEF::createNewLevelDef();
    }
    LEVEL_DEF	*def = GAMEDEF::leveldef(newdepth);
    def->numcreated++;

    ROOM *level = findAtlasRoom(newdepth, 0);
    if (!level)
    {
	ROOMTYPE_NAMES roomtype = (ROOMTYPE_NAMES) GAMEDEF::leveldef(newdepth)->roomtype;
	FRAGMENT		*frag;
	frag = theFragRooms[roomtype](rand_choice(theFragRooms[roomtype].entries()));
	frag->buildRoom(this, roomtype, difficulty, newdepth, newdepth, 0);
	level = findAtlasRoom(newdepth, 0);
    }

    // Move the avatar out of the map.
    thisavatar->setPos(POS());
    thisavatar->clearAllPos();

    POS		goal;

    goal = level->getRandomTile(
		    down ? TILE_UPSTAIRS
			 : TILE_DOWNSTAIRS,
		    POS(), 0);
    if (goal.mob())
    {
	// Find non-mob spot at the ladder
	goal = level->spiralSearch(goal, 1, true);
    }
    if (!goal.valid())
	goal = level->getRandomPos(POS(), 0);
    goal.setAngle(0);
    thisavatar->move(goal);

    // Ensure our avatar is legal and flagged.
    thisavatar->updateEquippedItems();

    rebuildFOV();
    display->pendingForgetAll();
}

int
MAP::getNumMobs() const
{
    int		i, total = 0;

    for (i = 0; i < myLiveMobs.entries(); i++)
    {
	if (myLiveMobs(i))
	    total++;
    }

    return total;
}

int
MAP::getAllItems(ITEMLIST &list) const
{
    for (int i = 0; i < myItems.entries(); i++)
    {
	if (myItems(i))
	    list.append(myItems(i));
    }

    return list.entries();
}

int
MAP::getAllMobs(MOBLIST &list) const
{
    for (int i = 0; i < myLiveMobs.entries(); i++)
    {
	if (myLiveMobs(i))
	    list.append(myLiveMobs(i));
    }

    return list.entries();
}

int
MAP::getAllMobsOfType(MOBLIST &list, MOB_NAMES type) const
{
    for (int i = 0; i < myLiveMobs.entries(); i++)
    {
	if (myLiveMobs(i))
	{
	    if (myLiveMobs(i)->getDefinition() == type)
		list.append(myLiveMobs(i));
	}
    }

    return list.entries();
}

POS
MAP::getRandomTile(TILE_NAMES tile) const
{
    int			n = 1;
    POS			p;

    for (int i = 0; i < myRooms.entries(); i++)
    {
	p = myRooms(i)->getRandomTile(tile, p, &n);
    }
    return p;
}

POS
MAP::spiralSearch(POS start, int startrad, bool avoidmob) const
{
    return start.room()->spiralSearch(start, startrad, avoidmob);
}

void
MAP::setAllFlags(MAPFLAG_NAMES flag, bool state)
{
    int		i;

    for (i = 0; i < myRooms.entries(); i++)
    {
	myRooms(i)->setAllFlags(flag, state);
    }
}

ROOMTYPE_NAMES
MAP::transitionRoom(ROOMTYPE_NAMES lastroom)
{
    switch (lastroom)
    {
	case ROOMTYPE_WILDERNESS:
	    return ROOMTYPE_WILDERNESS;

	case ROOMTYPE_ATRIUM:
	    return ROOMTYPE_CORRIDOR;
	
	case ROOMTYPE_CORRIDOR:
	{
	    ROOMTYPE_NAMES	transitions[] =
	    {
		ROOMTYPE_CORRIDOR,
		ROOMTYPE_CORRIDOR,
		ROOMTYPE_CORRIDOR,
		ROOMTYPE_CORRIDOR,
		ROOMTYPE_DININGROOM,
		ROOMTYPE_BARRACKS,
		ROOMTYPE_LIBRARY,
		ROOMTYPE_GUARDHOUSE
	    };

	    return transitions[rand_choice(sizeof(transitions)/sizeof(ROOMTYPE_NAMES))];
	}
	case ROOMTYPE_BARRACKS:
	{
	    ROOMTYPE_NAMES	transitions[] =
	    {
		ROOMTYPE_CORRIDOR,
		ROOMTYPE_CORRIDOR,
		ROOMTYPE_BARRACKS,
		ROOMTYPE_GUARDHOUSE
	    };

	    return transitions[rand_choice(sizeof(transitions)/sizeof(ROOMTYPE_NAMES))];
	}
	case ROOMTYPE_GUARDHOUSE:
	{
	    ROOMTYPE_NAMES	transitions[] =
	    {
		ROOMTYPE_THRONEROOM,
		ROOMTYPE_VAULT,
	    };

	    return transitions[rand_choice(sizeof(transitions)/sizeof(ROOMTYPE_NAMES))];
	}
	case ROOMTYPE_THRONEROOM:
	{
	    ROOMTYPE_NAMES	transitions[] =
	    {
		ROOMTYPE_CORRIDOR,
		ROOMTYPE_VAULT,
	    };

	    return transitions[rand_choice(sizeof(transitions)/sizeof(ROOMTYPE_NAMES))];
	}
	case ROOMTYPE_LIBRARY:
	{
	    ROOMTYPE_NAMES	transitions[] =
	    {
		ROOMTYPE_CORRIDOR,
		ROOMTYPE_CORRIDOR,
		ROOMTYPE_CORRIDOR,
		ROOMTYPE_LIBRARY,
	    };

	    return transitions[rand_choice(sizeof(transitions)/sizeof(ROOMTYPE_NAMES))];
	}
	case ROOMTYPE_KITCHEN:
	{
	    ROOMTYPE_NAMES	transitions[] =
	    {
		ROOMTYPE_CORRIDOR,
		ROOMTYPE_CORRIDOR,
		ROOMTYPE_PANTRY,
	    };

	    return transitions[rand_choice(sizeof(transitions)/sizeof(ROOMTYPE_NAMES))];
	}
	case ROOMTYPE_DININGROOM:
	{
	    ROOMTYPE_NAMES	transitions[] =
	    {
		ROOMTYPE_CORRIDOR,
		ROOMTYPE_CORRIDOR,
		ROOMTYPE_KITCHEN,
	    };

	    return transitions[rand_choice(sizeof(transitions)/sizeof(ROOMTYPE_NAMES))];
	}
    }

    return ROOMTYPE_CORRIDOR;
}

int
MAP::lookupDistMap(POS goal) const
{
    int		i;

    for (i = 0; i < DISTMAP_CACHE; i++)
    {
	if (goal == myDistMapCache[i])
	{
	    // Successful find!
	    myDistMapWeight[i] += 1.0;
	    return i;
	}
    }
    return -1;
}

int
MAP::allocDistMap(POS goal)
{
    int			i;
    double		bestweight = 1e32;
    int			bestindex = 0;

    for (i = 0; i < DISTMAP_CACHE; i++)
    {
	// Trivial success!
	if (!myDistMapCache[i].valid())
	{
	    bestindex = i;
	    break;
	}

	if (myDistMapWeight[i] < bestweight)
	{
	    bestindex = i;
	    bestweight = myDistMapWeight[i];
	}
    }

    myDistMapWeight[bestindex] = 1.0;
    myDistMapCache[bestindex] = goal;

    clearDistMap(bestindex);

    return bestindex;
}

void
MAP::clearDistMap(int distmap)
{
    int			i;

    for (i = 0; i < myRooms.entries(); i++)
    {
	myRooms(i)->clearDistMap(distmap);
    }
}

int
MAP::buildDistMap(POS goal)
{
    POSLIST		stack;
    POS			p, np;
    int			dist, dx, dy;
    int			distmap;

    J_ASSERT(goal.valid());

    stack.setCapacity(1000);

    distmap = lookupDistMap(goal);
    if (distmap >= 0)
	return distmap;

#ifdef DO_TIMING
    cerr << "Build distance map" << endl;
#endif

    distmap = allocDistMap(goal);

    goal.setDistance(distmap, 0);

    stack.append(goal);
    while (stack.entries())
    {
	p = stack.removeFirst();

	dist = p.getDistance(distmap);
	dist++;

	FORALL_8DIR(dx, dy)
	{
	    np = p.delta(dx, dy);

	    if (!np.valid())
		continue;

	    if (!np.isPassable())
		continue;

	    if (np.getDistance(distmap) >= 0)
		continue;

	    np.setDistance(distmap, dist);

	    stack.append(np);
	}
    }
    return distmap;
}

void
MAP::addDeadMob(MOB *mob)
{
    if (mob)
    {
	myDeadMobs.append(mob);
    }
}

void
MAP::reapMobs()
{
    for (int i = 0; i < myDeadMobs.entries(); i++)
	delete myDeadMobs(i);

    myDeadMobs.clear();
}

void
MAP::removeMob(MOB *mob, POS pos)
{
    if (!mob)
	return;
    int		uid = mob->getUID();

    if (uid == INVALID_UID)
	return;

    MOB		*livemob = myLiveMobs(uid);
    J_ASSERT(livemob == mob);
    myLiveMobs(uid) = nullptr;

    J_ASSERT(myMobCount.entries() > mob->getDefinition());
    myMobCount.indexAnywhere(mob->getDefinition())--;
}

void
MAP::addMob(MOB *mob, POS pos)
{
    if (!mob)
	return;
    int		uid = mob->getUID();

    if (uid == INVALID_UID)
    {
	uid = myLiveMobs.append(mob);
	mob->setUID(uid);
    }
    myLiveMobs(uid) = mob;

    myMobCount.indexAnywhere(mob->getDefinition())++;
}

void
MAP::moveMob(MOB *mob, POS pos, POS oldpos)
{
    if (!mob)
	return;
    if (pos == oldpos)
	return;
    int		uid = mob->getUID();

    J_ASSERT(uid > 0);
    J_ASSERT(myLiveMobs(uid) == mob);
}

void
MAP::removeItem(ITEM *item, POS pos)
{
    if (!item)
	return;
    int		uid = item->getUID();

    if (uid == INVALID_UID)
	return;

    ITEM		*liveitem = myItems.safeIndexAnywhere(uid);
    J_ASSERT(liveitem == item || liveitem == nullptr);
    myItems.indexAnywhere(uid) = nullptr;
}

void
MAP::addItem(ITEM *item, POS pos)
{
    if (!item)
	return;
    int		uid = item->getUID();

    if (uid == INVALID_UID)
    {
	// Unlike mobs, items have uids...
	J_ASSERT(false);
    }
    myItems.indexAnywhere(uid) = item;
}

bool
MAP::requestDelayedMove(MOB *mob, int dx, int dy)
{
    if (!myAllowDelays)
	return false;

    myDelayMobList.append( MOBDELAY(mob, dx, dy) );
    return true;
}

void
mapAddAllToFinal(MOB *mob, MOBDELAYLIST &final, const MOBDELAYLIST &list)
{
    int		idx;

    if (!mob->isDelayMob())
	return;

    idx = mob->delayMobIdx();
    final.append(list(idx));
    mob->setDelayMob(false, -1);

    for (int j = 0; j < mob->collisionSources().entries(); j++)
    {
	MOB	*parent;

	parent = mob->collisionSources()(j);
	parent->setCollisionTarget(0);

	mapAddAllToFinal(parent, final, list);
    }
}

MOB *
mapFindCycle(MOB *mob)
{
    // Yay for wikipedia!
    MOB		*tortoise, *hare;

    tortoise = mob;
    hare = tortoise->collisionTarget();

    while (hare)
    {
	if (tortoise == hare)
	    return tortoise;		// Tortoise always wins :>
	tortoise = tortoise->collisionTarget();
	if (!tortoise)
	    return 0;
	hare = hare->collisionTarget();
	if (!hare)
	    return 0;
	hare = hare->collisionTarget();
	if (!hare)
	    return 0;
    }
    return 0;
}

void
mapBuildContactGraph(const MOBDELAYLIST &list)
{
    int		i;

    // Clear our old tree.
    for (i = 0; i < list.entries(); i++)
    {
	MOB		*mob = list(i).myMob;
	mob->clearCollision();
    }
    // Build our tree.
    for (i = 0; i < list.entries(); i++)
    {
	MOB		*mob = list(i).myMob;
	if (!mob->alive() || !mob->isDelayMob())
	    continue;
	mob->setCollisionTarget(mob->pos().delta( list(i).myDx,
						  list(i).myDy ).mob() );
    }
}

void
mapResolveCycle(MOB *mob)
{
    MOB		*next, *start;
    MOBLIST	cycle;
    POSLIST	goals;

    // Compute the cycle
    start = mob;
    do
    {
	next = mob->collisionTarget();

	// Disconnect anyone pointing to me.
	for (int i = 0; i < mob->collisionSources().entries(); i++)
	{
	    mob->collisionSources()(i)->setCollisionTarget(0);
	}

	cycle.append(mob);
	// We can't use next->pos as next will be null when we complete
	// our loop because the above code will disconnect the back pointer!
	// Thus the goal store *our* position.
	goals.append(mob->pos());
	mob->setDelayMob(false, -1);

	mob = next;
    } while (mob && mob != start);


    // We do this in two steps to avoid any potential issues of
    // the underlying map not liking two mobs on the same square.

    // Yank all the mobs off the map
    for (int i = 0; i < cycle.entries(); i++)
    {
	cycle(i)->setPos(POS());
    }

    // And drop them back on their goal pos.
    for (int i = 0; i < cycle.entries(); i++)
    {
	// The goals are off by one, everyone wants to get to the
	// next goal
	cycle(i)->setPos(goals((i + 1) % cycle.entries()));
    }
}

bool
mapRandomPushMob(MOB *mob, int pdx, int pdy)
{
    POSLIST		pushlist;
    POS			target, p, nt;

    pushlist.append(mob->pos());

    // We demand no pushbacks
    // So we do something like 
    int			vdx[3], vdy[3];

    if (pdx && pdy)
    {
	// Diagonal!
	vdx[0] = pdx;
	vdy[0] = pdy;
	vdx[1] = pdx;
	vdy[1] = 0;
	vdx[2] = 0;
	vdy[2] = pdy;
    }
    else
    {
	// Horizontal!
	// Too clever wall sliding code
	int			sdx = !pdx;
	int			sdy = !pdy;

	vdx[0] = pdx;
	vdy[0] = pdy;
	vdx[1] = pdx-sdx;
	vdy[1] = pdy-sdy;
	vdx[2] = pdx+sdx;
	vdy[2] = pdy+sdy;
    }

    target = mob->pos().delta(pdx, pdy);
    while (target.mob())
    {
	bool		found = false;
	int		nfound;

	if (pushlist.find(target) >= 0)
	{
	    // We hit an infinite loop, no doubt due to a looped room.
	    return false;
	}

	pushlist.append(target);

	// Find a place to push target to.
	// We only ever move in vdx possible directions.  We also
	// verify we don't end up beside the mob.  Thanks to looping rooms
	// this could happen!

	nfound = 0;
	for (int i = 0; i < 3; i++)
	{
	    p = target.delta(vdx[i], vdy[i]);
	    if (target.mob()->canMove(p))
	    {
		// Yay!
		nfound++;
		if (!rand_choice(nfound))
		{
		    nt = p;
		    found = true;
		}
	    }
	}
	if (found)
	{
	    target = nt;
	    J_ASSERT(!target.mob());
	    continue;		// But should exit!
	}

	// Try again, but find a mob to push.
	nfound = 0;
	for (int i = 0; i < 3; i++)
	{
	    p = target.delta(vdx[i], vdy[i]);
	    if (target.mob()->canMove(p, false))
	    {
		// Yay!
		nfound++;
		if (!rand_choice(nfound))
		{
		    nt = p;
		    found = true;
		}
	    }
	}
	if (!found)
	{
	    // OKay, this path failed to push.  Note that there might
	    // be other paths that *could* push.  We handle this
	    // by invoking this multiple times and having
	    // each invocation pick a random push path.
	    return false;
	}
	target = nt;
    }

    J_ASSERT(!target.mob());
    // Now we should have a poslist chain to do pushing with.
    // We start with the end and move backwards.
    while (pushlist.entries())
    {
	MOB		*pushed;

	p = pushlist.pop();

	J_ASSERT(!target.mob());

	// Move the mob from p to target.
	pushed = p.mob();
	if (pushed)
	{
	    pushed->setPos(target);
	    if (pushed->isDelayMob())
		pushed->setDelayMob(false, -1);
	    else
	    {
		// This is so no one can say that the kobolds are cheating!
		pushed->skipNextTurn();
	    }
	}

	// Now update target.
	target = p;
    }
    return true;
}

bool
mapPushMob(MOB *mob, int pdx, int pdy)
{
    // First, we only start a push if we are in one square
    // of the avatar.

    // This shouldn't happen.
    if (!pdx && !pdy)
	return false;

#if 0
    int			dx, dy;
    POS			avatarpos;
    FORALL_8DIR(dx, dy)
    {
	if (mob->pos().delta(dx, dy).mob() &&
	    mob->pos().delta(dx, dy).mob()->isAvatar())
	{
	    avatarpos = mob->pos().delta(dx, dy);
	    break;
	}
    }
    // Refuse to push, no avatar nearby.
    if (!avatarpos.valid())
	return false;
#endif

    // Each push tries a different path, we pick 10 as a good test
    // to see if any of the paths are pushable.
    for (int i = 0; i < 10; i++)
    {
	if (mapRandomPushMob(mob, pdx, pdy))
	    return true;
    }
    return false;
}


void
mapReduceList(const MOBLIST &allmobs, MOBDELAYLIST &list)
{
    // Build our collision graph.
    int			i, j;
    MOBLIST		roots;

    MOBDELAYLIST	final;

    for (i = 0; i < allmobs.entries(); i++)
    {
	allmobs(i)->clearCollision();
	allmobs(i)->setDelayMob(false, -1);
    }

    for (i = 0; i < list.entries(); i++)
    {
	MOB		*mob = list(i).myMob;
	mob->setDelayMob(true, i);
    }

    // We alternately clear out all roots, then destroy a cycle,
    // until nothing is left.
    bool		mobstoprocess = true;
    while (mobstoprocess)
    {
	mapBuildContactGraph(list);

	// Find all roots.
	roots.clear();
	for (i = 0; i < list.entries(); i++)
	{
	    MOB		*mob = list(i).myMob;
	    if (!mob->alive())
		continue;
	    if (!mob->isDelayMob())
		continue;
	    if (!mob->collisionTarget() || !mob->collisionTarget()->isDelayMob())
		roots.append(mob);
	}

	// Note that as we append to roots, this will grow as we add more
	// entries.
	for (i = 0; i < roots.entries(); i++)
	{
	    MOB		*mob = roots(i);
	    bool	 	 resolved = false;

	    if (!mob->alive())
	    {
		resolved = true;
	    }
	    // Mobs may have lots theyr delay flag if they got pushed.
	    else if (mob->isDelayMob())
	    {
		int		 dx, dy, idx;
		MOB		*victim;

		idx = mob->delayMobIdx();
		dx = list(idx).myDx;
		dy = list(idx).myDy;

		victim = mob->pos().delta(dx, dy).mob();

		// If there is no collision target, easy!
		// Note that no collision target doesn't mean no victim!
		// Someone could have moved in the mean time or died.
		if (!victim)
		{
		    resolved = mob->actionWalk(dx, dy);
		}
		else
		{
		    // Try push?
		    // We push according to rank.
		    if (mob->shouldPush(victim))
			resolved = mapPushMob(mob, dx, dy);
		}
	    }

	    if (resolved)
	    {
		mob->setDelayMob(false, -1);
		for (j = 0; j < mob->collisionSources().entries(); j++)
		{
		    // We moved out!
		    mob->collisionSources()(j)->setCollisionTarget(0);
		    roots.append(mob->collisionSources()(j));
		}
		mob->collisionSources().clear();
	    }
	    else
	    {
		// Crap, add to final.
		mapAddAllToFinal(mob, final, list);
	    }
	}

	// Rebuild our contact graph since various folks have moved or died.
	mapBuildContactGraph(list);

	// There are no roots, so either we have everything accounted for,
	// or there is a cycle.
	mobstoprocess = false;
	for (i = 0; i < list.entries(); i++)
	{
	    MOB		*mob = list(i).myMob;
	    if (!mob->alive())
		continue;
	    if (!mob->isDelayMob())
		continue;

	    // Finds a cycle, the returned mob is inside that cycle.
	    mob = mapFindCycle(mob);

	    if (mob)
	    {
		// Resolving this cycle may create new roots.
		mobstoprocess = true;
		mapResolveCycle(mob);
	    }
	}
    }

    // Only keep unprocessed delays.
    for (i = 0; i < list.entries(); i++)
    {
	MOB		*mob = list(i).myMob;
	if (mob->alive() && mob->isDelayMob())
	    final.append( list(i) );

	mob->setDelayMob(false, -1);
    }

    list = final;
}

void
MAP::doHeartbeat()
{
    spd_inctime();

    // Our simulation for smoke we want to be unbiased, but we
    // write & read.  We want to splat outwards only, so run red/black
    // to decouple the passes.
    if (getPhase() == PHASE_NORMAL)
    {
	static int	toggle = 0;
	toggle = !toggle;
	for (int colour = 0; colour < 4; colour++)
	{
	    for (auto && room : myRooms)
	    {
		room->simulateSmoke((colour ^ toggle) & 1);
	    }
	}
    }
    for (auto && room : myRooms)
    {
	room->updateMaxSmoke();
    }
}

POS
MAP::buildFromCanonical(int x, int y) const
{
    int		atlasx, atlasy;
    if (x < 0 || y < 0)
	return POS();

    atlasx = x / 100;
    atlasy = y / 100;
    ROOM	*room = findAtlasRoom(atlasx, atlasy);
    if (!room)
	return POS();
    J_ASSERT(x >= 0 && y >= 0);
    return room->buildPos(x % 100, y % 100);
}


void
MAP::doMoveNPC()
{
    MOBLIST		allmobs;
#ifdef DO_TIMING
    int 		movestart = TCOD_sys_elapsed_milli();
#endif

    // Note we don't need to pre-reap dead mobs since they won't
    // be in a room, so not be picked up in getAllMobs.

    // Pregather in case mobs move through portals.
    getAllMobs(allmobs);

    // Ensure we are somewhat fair.
    // Otherwise orange beats out cyan!
    allmobs.shuffle();

    // The avatar may die during this process so can't test against him
    // in the inner loop.
    allmobs.removePtr(avatar());

    myDelayMobList.clear();
    myAllowDelays = true;

    // TODONE: A mob may kill another mob, causing this to die!
    // Our solution is just to check the alive flag.
    for (int i = 0; i < allmobs.entries(); i++)
    {
	if (!allmobs(i)->alive())
	    continue;
	if (!allmobs(i)->aiForcedAction())
	    allmobs(i)->aiDoAI();
    }

#ifdef DO_TIMING
    int		movedoneai = TCOD_sys_elapsed_milli();
#endif

    int		delaysize = myDelayMobList.entries();
    int		prevdelay = delaysize+1;

    // So long as we are converging, it is good.
    while (delaysize && delaysize < prevdelay)
    {
	MOBDELAYLIST	list;

	// Copy the list as we will be re-registering everyone.
	list = myDelayMobList;

	// Randomizing the list helps avoid any problems from biased
	// input.
	// (A better approach would be to build a contact graph)
	list.shuffle();

	myDelayMobList.clear();

	mapReduceList(allmobs, list);

	for (int i = 0; i < list.entries(); i++)
	{
	    // One way to reduce the list :>
	    if (!list(i).myMob->alive())
		continue;

	    // No need for force action test as it must have passed and
	    // no double jeapordy.
	    list(i).myMob->aiDoAI();
	}

	prevdelay = delaysize;

	delaysize = myDelayMobList.entries();
    }

    // Convergence!  Any left over mobs may still have another thing
    // they want to do, however, so we reinvoke with delay disabled.
    myAllowDelays = false;
    if (myDelayMobList.entries())
    {
	MOBDELAYLIST	list;

	// Copy the list as we will be re-registering everyone.
	list = myDelayMobList;

	// No need to randomize or anything.
	myDelayMobList.clear();

	for (int i = 0; i < list.entries(); i++)
	{
	    // One way to reduce the list :>
	    if (!list(i).myMob->alive())
		continue;

	    // No need for force action test as it must have passed and
	    // no double jeapordy.
	    list(i).myMob->aiDoAI();
	}
    }

    // Delete any dead mobs
    reapMobs();

#ifdef DO_TIMING
    int		movedoneall = TCOD_sys_elapsed_milli();

    static int	movetime, posttime;

    movetime = movedoneai - movestart;
    posttime = movedoneall - movedoneai;

    cerr << "move time " << movetime << " post time " << posttime << endl;
    cerr << "Mob count " << getNumMobs() << endl;
#endif
}

MOB *
MAP::findAvatar()
{
    // Pregather in case mobs move through portals.
    for (int i = 0; i < myLiveMobs.entries(); i++)
    {
	if (myLiveMobs(i))
	{
	    if (myLiveMobs(i)->getDefinition() == MOB_AVATAR)
	    {
		if (myLiveMobs(i) == party().active())
		    return myLiveMobs(i);
	    }
	}
    }
    return 0;
}

void
MAP::init()
{
    spd_init();

    DIRECTORY		dir;
    const char		*f;
    BUF			buf;

    dir.opendir("../rooms");

    while (f = dir.readdir())
    {
	buf.strcpy(f);
	if (buf.extensionMatch("map"))
	{
	    ROOMTYPE_NAMES		roomtype;

	    FOREACH_ROOMTYPE(roomtype)
	    {
		if (buf.startsWith(glb_roomtypedefs[roomtype].prefix))
		{
		    BUF		fname;
		    fname.sprintf("../rooms/%s", f);
		    theFragRooms[roomtype].append(new FRAGMENT(fname.buffer()));
		}
	    }
#if 1
	    // Let all unmatched become generic rooms
	    if (roomtype == NUM_ROOMTYPES)
	    {
		buf.sprintf("../rooms/%s", f);
		theFragRooms[ROOMTYPE_NONE].append(new FRAGMENT(buf.buffer()));
	    }
#endif
	}
    }

    // Create our generators
    ROOMTYPE_NAMES		roomtype;
    FOREACH_ROOMTYPE(roomtype)
    {
	if (glb_roomtypedefs[roomtype].usegenerator)
	{
#if 0
	    if (roomtype == ROOMTYPE_CORRIDOR)
	    {
		theFragRooms[roomtype].append(new FRAGMENT(17, 13));
		theFragRooms[roomtype].append(new FRAGMENT(16, 15));
		theFragRooms[roomtype].append(new FRAGMENT(15, 17));
	    }
	    else
	    {
		theFragRooms[roomtype].append(new FRAGMENT(13, 13));
		theFragRooms[roomtype].append(new FRAGMENT(12, 11));
		theFragRooms[roomtype].append(new FRAGMENT(11, 12));
	    }
#else
	    if (roomtype == ROOMTYPE_BIGROOM)
	    {
		theFragRooms[roomtype].append(new FRAGMENT(60, 30));
	    }
	    else if (roomtype == ROOMTYPE_MAZE)
	    {
		theFragRooms[roomtype].append(new FRAGMENT(45, 30));
	    }
	    else if (roomtype == ROOMTYPE_WILDERNESS)
	    {
		theFragRooms[roomtype].append(new FRAGMENT(100, 100));
	    }
	    else
	    {
		theFragRooms[roomtype].append(new FRAGMENT(60, 30));
	    }
#endif
	}
    }

    if (!theFragRooms[ROOMTYPE_NONE].entries())
    {
	cerr << "No .map files found in ../rooms" << endl;
	exit(-1);
    }
}

extern volatile int glbItemCache;

void
MAP::cacheItemPos()
{
    ITEMLIST		list;

#if 0
    getAllItems(list);

    for (int i = 0; i < list.entries(); i++)
    {
	glbItemCache = (int) (100 * (i / (float)list.entries()));
	buildDistMap(list(i)->pos());
    }

#endif
    glbItemCache = -1;
}

void
MAP::rebuildFOV()
{
    // If we have a sleeeping avatar we don't rebuild the fov
    if (avatar() && avatar()->isWaiting())
    {
	return;
    }

    setAllFlags(MAPFLAG_FOV, false);
    setAllFlags(MAPFLAG_FOVCACHE, false);

    glbMobsInFOV.clear();

    if (!avatar())
	return;

    delete myFOVCache;

    myFOVCache = new SCRPOS(avatar()->meditatePos(), 50, 50);
    POS			p;

    TCODMap		tcodmap(101, 101);
    int			x, y;

    for (y = 0; y < 101; y++)
    {
	for (x = 0; x < 101; x++)
	{
	    p = myFOVCache->lookup(x-50, y - 50);
	    p.setFlag(MAPFLAG_FOVCACHE, true);
	    // Force our own square to be visible.
	    if (x == 50 && y == 50)
	    {
		tcodmap.setProperties(x, y, true, true);
	    }
	    else if (ABS(x-50) < 2 && ABS(y-50) < 2)
	    {
		if (p.defn().semitransparent)
		    tcodmap.setProperties(x, y, true, true);
		else
		    tcodmap.setProperties(x, y, p.defn().istransparent, p.isPassable());
	    }
	    else
	    {
		tcodmap.setProperties(x, y, p.defn().istransparent, p.isPassable());
	    }
	}
    }

    tcodmap.computeFov(50, 50);

    bool		anyhostiles = false;

    for (y = 0; y < 101; y++)
    {
	for (x = 0; x < 101; x++)
	{
	    p = myFOVCache->lookup(x-50, y - 50);
	    // We might see this square in more than one way.
	    // We don't want to mark ourselves invisible if
	    // there is someway that did see us.
	    // This will result in too large an FOV, but the "free" squares
	    // are those that you could deduce from what you already 
	    // can see, so I hope to be benign?
	    if (tcodmap.isInFov(x, y))
	    {
		p.setFlag(MAPFLAG_FOV, true);
		p.setFlag(MAPFLAG_MAPPED, true);
		if (p.mob())
		{
		    if (!p.mob()->isFriends(avatar()))
		    {
			anyhostiles = true;
		    }
		    // This is slightly wrong as we could double
		    // count, but I'd argue that the rat seeing
		    // its portal rat honestly thinks it is an ally!
		    glbMobsInFOV.indexAnywhere(p.mob()->getDefinition())++;
		}
	    }
	}
    }

    if (anyhostiles)
    {
	avatar()->stopWaiting("Sensing a foe, %S <end> %r rest.");
    }
}

bool
MAP::computeDelta(POS a, POS b, int &dx, int &dy)
{
    int		x1, y1, x2, y2;

    if (!myFOVCache)
	return false;

    // This early exits the linear search in myFOVCache find.
    if (!a.isFOVCache() || !b.isFOVCache())
	return false;

    if (!myFOVCache->find(a, x1, y1))
	return false;
    if (!myFOVCache->find(b, x2, y2))
	return false;

    dx = x2 - x1;
    dy = y2 - y1;

    POS		ap, at;

    // These are all in the coordinates of whoever built the scrpos,
    // ie, scrpos(0,0)
    //
    // This is a very confusing situation.
    // I think we have:
    // a - our source pos
    // @ - center of the FOV, myFOVCache->lookup(0,0)
    // a' - our source pos after walking from @, ie myFOVCache->lookup(x1,y1)
    //
    // dx, dy are in world space of @.  We want world space of a.
    // call foo.W the toWorld and foo.L the toLocal.
    // First apply the twist that @ got going to a'.
    // a'.W(@.L())
    // Next apply the transform into a.
    // a.W(a'.L())
    // Next we see a' cancels, leaving
    // a.W(@.L())
    //
    // Okay, that is bogus.  It is obvious it can't work as it
    // does not depend on a', and a' is the only way to include
    // the portal twist invoked in the walk!  I think I have the
    // twist backwards, ie, @.W(a'.L()), which means the compaction
    // doesn't occur.

    //
    // Try again.
    // @.L is needed to get into map space.
    // Then apply twist of @ -> a'   This is in Local coords!
    // So we just apply a.W
    // a.W(@.L(a'.W(@.L())))

    // Hmm.. 
    // ap encodes how to go from @ based world coords into
    // local a room map coords.  Thus ap.L gets us right there,
    // with only an a.W needed.

    ap = myFOVCache->lookup(x1, y1);
    // at = myFOVCache->lookup(0, 0);

    ap.rotateToLocal(dx, dy);
    a.rotateToWorld(dx, dy);

    return true;
}

void
MAP::buildReflexivePortal(POS pos, int dir)
{
    dir = (dir + 2);

    dir += pos.angle();

    dir %= 4;
    
    POS		backpos;

    ROOM	*home = findAtlasRoom(0, 0);

    if (home)
    {
	backpos = home->findUserProtoPortal(dir);
	buildUserPortal(backpos, 1, dir, 0);
    }
}

void
MAP::buildUserPortal(POS pos, int pnum, int dir, int avatardir)
{
    if (!pos.valid())
	return;

    if (!pnum)
    {
	buildReflexivePortal(pos, dir + avatardir);
    }

    if (myUserPortal[pnum].valid())
    {
	if (myUserPortal[0].valid() && myUserPortal[1].valid())
	{
	    // Both existing portals are wiped.
	    // Note the reversal of directins here!  This is because we
	    // have the coordinates of the landing zone and the portal
	    // only exists in the source room!
	    myUserPortal[1].room()->removePortal(myUserPortal[0]);
	    myUserPortal[0].room()->removePortal(myUserPortal[1]);
	}

	// Restore the wall, possibly trapping people :>
	if (myUserPortal[pnum].roomType() == ROOMTYPE_VILLAGE)
	    myUserPortal[pnum].setTile(TILE_USERPROTOPORTAL);
	else
	    myUserPortal[pnum].setTile((TILE_NAMES)myUserPortal[pnum].roomDefn().wall_tile);
	myUserPortal[pnum] = POS();
    }

    // Construct our new portal...
    pos.setTile(pnum ? TILE_ORANGEPORTAL : TILE_BLUEPORTAL);
    pos.setFlag(MAPFLAG_PORTAL, true);
    myUserPortal[pnum] = pos;
    myUserPortalDir[pnum] = dir;

    // Build the link
    if (myUserPortal[0].valid() && myUserPortal[1].valid())
    {
	ROOM::buildPortal(myUserPortal[0], myUserPortalDir[0],
			  myUserPortal[1], myUserPortalDir[1],
			    false);	
    }
}

ITEM *
MAP::findItem(int uid) const
{
    if (uid < 0 || uid >= myItems.entries())
	return nullptr;

    return myItems(uid);
}

MOB *
MAP::findMob(int uid) const
{
    if (uid < 0 || uid >= myLiveMobs.entries())
	return 0;

    return myLiveMobs(uid);
}

MOB *
MAP::findMobByType(MOB_NAMES type) const
{
    for (int i = 0; i < myLiveMobs.entries(); i++)
    {
	if (myLiveMobs(i) && myLiveMobs(i)->getDefinition() == type)
	{
	    return myLiveMobs(i);
	}
    }

    return 0;
}

MOB *
MAP::findMobByHero(HERO_NAMES type) const
{
    for (int i = 0; i < myLiveMobs.entries(); i++)
    {
	if (myLiveMobs(i) && myLiveMobs(i)->getDefinition() == MOB_AVATAR && myLiveMobs(i)->getHero() == type)
	{
	    return myLiveMobs(i);
	}
    }

    return 0;
}


MOB *
MAP::findMobByLoc(ROOM *room, int x, int y) const
{
    if (!room)
	return nullptr;
    int roomid = room->getId();

    for (int i = 0; i < myLiveMobs.entries(); i++)
    {
	if (!myLiveMobs(i))
	    continue;
	if (myLiveMobs(i)->pos().roomId() == roomid &&
	    myLiveMobs(i)->pos().myX == x &&
	    myLiveMobs(i)->pos().myY == y)
	{
	    return myLiveMobs(i);
	}
    }

    return nullptr;
}

int
MAP::findAllMobsByLoc(MOBLIST &list, ROOM *room, int x, int y) const
{
    if (!room)
	return list.entries();
    int roomid = room->getId();

    for (int i = 0; i < myLiveMobs.entries(); i++)
    {
	if (!myLiveMobs(i))
	    continue;
	if (myLiveMobs(i)->pos().roomId() == roomid &&
	    myLiveMobs(i)->pos().myX == x &&
	    myLiveMobs(i)->pos().myY == y)
	{
	    list.append(myLiveMobs(i));
	}
    }

    return list.entries();
}

ITEM *
MAP::findItemByLoc(ROOM *room, int x, int y) const
{
    if (!room)
	return nullptr;
    int roomid = room->getId();

    for (int i = 0; i < myItems.entries(); i++)
    {
	if (!myItems(i))
	    continue;
	if (myItems(i)->pos().roomId() == roomid &&
	    myItems(i)->pos().myX == x &&
	    myItems(i)->pos().myY == y)
	{
	    return myItems(i);
	}
    }

    return nullptr;
}

int
MAP::findAllItemsByLoc(ITEMLIST &list, ROOM *room, int x, int y) const
{
    if (!room)
	return list.entries();
    int roomid = room->getId();

    for (int i = 0; i < myItems.entries(); i++)
    {
	if (!myItems(i))
	    continue;
	if (myItems(i)->pos().roomId() == roomid &&
	    myItems(i)->pos().myX == x &&
	    myItems(i)->pos().myY == y)
	{
	    list.append(myItems(i));
	}
    }

    return list.entries();
}
POS
MAP::findRoomOfType(ROOMTYPE_NAMES roomtype) const
{
    POS		result;
    int		nfound = 0;

    for (int i = 0; i < myRooms.entries(); i++)
    {
	if (myRooms(i)->type() == roomtype)
	{
	    nfound++;
	    if (!rand_choice(nfound))
	    {
		result = myRooms(i)->findCentralPos();
	    }
	}
    }
    return result;
}

POS
MAP::astralLoc() const
{
    ROOM *astral = findAtlasRoom(-1, -1);

    if (!astral)
	return POS();

    POS astralpos = astral->findCentralPos();

    return astral->spiralSearch(astralpos, 0, true);
}

ROOM *
MAP::findAtlasRoom(int x, int y) const
{
    for (int i = 0; i < myRooms.entries(); i++)
    {
	if (myRooms(i)->atlasX() == x && myRooms(i)->atlasY() == y)
	{
	    return myRooms(i);
	}
    }
    return 0;
}

void
MAP::save() const
{
    // Did I mention my hatred of streams?
    BUF		path;
    path.sprintf("../save/%s.sav", glbWorldName);
#ifdef WIN32
    ofstream	os(path.buffer(), ios::out | ios::binary);
#else
    ofstream	os(path.buffer());
#endif

    s32			val;
    int			i;

    val = spd_gettime();
    os.write((const char *) &val, sizeof(s32));

    ITEM::saveGlobal(os);
    MOB::saveGlobal(os);

    for (i = 0; i < 2; i++)
    {
	myUserPortal[i].save(os);
	val = myUserPortalDir[i];
	os.write((const char *) &val, sizeof(s32));
    }

    val = myRooms.entries();
    os.write((const char *) &val, sizeof(s32));
    for (i = 0; i < myRooms.entries(); i++)
    {
	myRooms(i)->save(os);
    }

    val = myItems.entries();
    os.write((const char *) &val, sizeof(s32));
    for (i = 0; i < myItems.entries(); i++)
    {
	if (myItems(i))
	{
	    val = 1;
	    os.write((const char *) &val, sizeof(s32));
	    myItems(i)->save(os);
	}
	else
	{
	    val = 0;
	    os.write((const char *) &val, sizeof(s32));
	}
    }

    val = myLiveMobs.entries();
    os.write((const char *) &val, sizeof(s32));
    for (i = 0; i < myLiveMobs.entries(); i++)
    {

	if (!myLiveMobs(i))
	{
	    val = 0;
	    os.write((const char *) &val, sizeof(s32));
	}
	else
	{
	    val = 1;
	    os.write((const char *) &val, sizeof(s32));
	    myLiveMobs(i)->save(os);
	}
    }

    myParty.save(os);
    val = myWorldLevel;
    os.write((const char *) &val, sizeof(s32));
}

MAP *
MAP::load()
{
    MAP		*map;

    BUF		path;
    path.sprintf("../save/%s.sav", glbWorldName);

    // Did I mention my hatred of streams?
    {
#ifdef WIN32
	ifstream	is(path.buffer(), ios::in | ios::binary);
#else
	ifstream	is(path.buffer());
#endif

	if (!is)
	    return 0;

	map = new MAP(is);
    }

    // Scope out the stream so we can have it unlocked to delete.
    MYunlink(path.buffer());

    return map;
}

MAP::MAP(istream &is)
{
    s32			val;
    int			i, n;

    myRefCnt.set(0);

    myUniqueId = glbMapId.add(1);
    myFOVCache = 0;
    myAllowDelays = false;

    for (i = 0; i < DISTMAP_CACHE; i++)
    {
	myDistMapWeight[i] = 0.0;
	myDistMapCache[i].setMap(this);
    }

    is.read((char *) &val, sizeof(s32));
    spd_settime(val);

    ITEM::loadGlobal(is);
    MOB::loadGlobal(is);

    for (i = 0; i < 2; i++)
    {
	myUserPortal[i].load(is);
	myUserPortal[i].setMap(this);
	is.read((char *) &val, sizeof(s32));
	myUserPortalDir[i] = val;
    }

    is.read((char *) &val, sizeof(s32));
    n = val;
    for (i = 0; i < n; i++)
    {
	myRooms.append(ROOM::load(is));
	myRooms(i)->setMap(this);
    }

    is.read((char *) &val, sizeof(s32));
    n = val;
    for (i = 0; i < n; i++)
    {
	is.read((char *) &val, sizeof(s32));
	if (val)
	{
	    myItems.append(ITEM::load(is));
	    myItems.top()->setMap(this);
	}
	else
	    myItems.append(nullptr);
    }

    is.read((char *) &val, sizeof(s32));
    n = val;
    for (i = 0; i < n; i++)
    {
	is.read((char *) &val, sizeof(s32));
	if (val)
	{
	    myLiveMobs.append(MOB::load(is));
	    myLiveMobs.top()->setMap(this);
	}
	else
	    myLiveMobs.append(nullptr);
    }

    myParty.load(is);
    myParty.setMap(this);
    is.read((char *) &val, sizeof(s32));
    myWorldLevel = val;

    myAvatar = findAvatar();
}

int
MAP::getTime() const
{
    return spd_gettime();
}

PHASE_NAMES
MAP::getPhase() const
{
    return spd_getphase();
}

ATTR_NAMES
MAP::depthColor(int depth, bool invert)
{
    ATTR_NAMES		result = ATTR_NORMAL;

    if (depth < 5)
    {
	result = ATTR_WHITE;
    }
    else if (depth < 10)
    {
	result = ATTR_GREY;
    }
    else if (depth < 15)
    {
	result = ATTR_DKGREY;
    }
    else
    {
	result = ATTR_LIGHTBLACK;
    }

    return result;
}
