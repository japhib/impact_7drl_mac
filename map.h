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

#ifndef __map__
#define __map__

#include <iostream>
using namespace std;

#include "ptrlist.h"
#include "thread.h"
#include "glbdef.h"
#include "gamedef.h"
#include "texture.h"
#include "indextree.h"
#include "vec2.h"
#include "party.h"

class POS;
class ROOM;
class MOB;
class ITEM;
class MAP;
class FRAGMENT;
class DISPLAY;
class SCRPOS;
class MOBDELAY;
class MESSAGE_PACKET;

typedef PTRLIST<ROOM *> ROOMLIST;
typedef PTRLIST<MOB *> MOBLIST;
typedef PTRLIST<ITEM *> ITEMLIST;
typedef PTRLIST<POS> POSLIST;
typedef PTRLIST<FRAGMENT *> FRAGMENTLIST;
typedef PTRLIST<MOBDELAY> MOBDELAYLIST;

// Number of distance map caches we maintain.
#define DISTMAP_CACHE	10
#define DISTMAP_CACHE_ALLOC	DISTMAP_CACHE

extern PTRLIST<int>	glbMobsInFOV;


// The position is the only way to access elements of the map
// because it can adjust for orientation, etc.
// Functions that modify the underlying square are const if they
// leave this unchanged...
class POS
{
public:
    POS();
    // Required for ptrlist, sigh.
    explicit POS(int blah);

    POS  	goToCanonical(int x, int y) const;

    /// Equality ignores angles.
    bool 	operator==(const POS &cmp) const
    {	return cmp.myX == myX && cmp.myY == myY && cmp.myRoomId == myRoomId; }
    bool 	operator!=(const POS &cmp) const
    {	return cmp.myX != myX || cmp.myY != myY || cmp.myRoomId != myRoomId; }

    POS		left() const { return delta(-1, 0); }
    POS		up() const { return delta(0, -1); }
    POS		down() const { return delta(0, 1); }
    POS		right() const { return delta(1, 0); }

    int		angle() const { return myAngle; }

    POS		delta4Direction(int angle) const;
    POS		delta(int dx, int dy) const;

    POS		rotate(int angle) const;
    void	setAngle(int angle);

    bool	valid() const;

    /// Compute number of squares to get to goal, 8 way metric.
    /// This is an approximation, ignoring obstacles
    int		dist(POS goal) const;

    ROOMTYPE_NAMES roomType() const;
    const ROOMTYPE_DEF &roomDefn() const { return glb_roomtypedefs[roomType()]; }

    /// Returns an estimate of what should be delta()d to this
    /// to move closer to goal.  Again, an estimate!  Traditionally
    /// just SIGN.
    void	dirTo(POS goal, int &dx, int &dy) const;

    /// Returns the first mob along the given vector from here.
    MOB		*traceBullet(int range, int dx, int dy, int *rangeleft=0) const;

    /// Returns the last valid pos before we hit a wall, if stop
    /// before wall set.  Otherwise returns the wall hit.
    POS		 traceBulletPos(int range, int dx, int dy, bool stopbeforewall, bool stopatmob = true) const;

    template <typename OP>
    void	 fireball(MOB *caster, int rad, u8 sym, ATTR_NAMES attr, OP op) const
    {
	// Boy do I hate C++ templates not supporting proper exports.
	int		dx, dy;

	POS		target;

	for (dy = -rad+1; dy <= rad-1; dy++)
	{
	    for (dx = -rad+1; dx <= rad-1; dx++)
	    {
		target = delta(dx, dy);
		if (target.valid())
		{
		    target.postEvent(EVENTTYPE_FORESYM, sym, attr);

		    // It is too hard to play if you harm yourself.
		    op(target);
		}
	    }
	}
    }

    /// Dumps to message description fo this square
    void	 describeSquare(bool blind) const;

    /// Draws the given bullet to the screen.
    void	 displayBullet(int range, int dx, int dy, u8 sym, ATTR_NAMES attr, bool stopatmob) const;

    void	 postEvent(EVENTTYPE_NAMES type, u8 sym, ATTR_NAMES attr) const;
    void	 postEvent(EVENTTYPE_NAMES type, u8 sym, ATTR_NAMES attr, const char *shout) const;

    /// Returns current distance map here.
    int		 getDistance(int mapnum) const;
    void	 setDistance(int mapnum, int dist) const;

    TILE_NAMES	tile() const;
    void	setTile(TILE_NAMES tile) const;
    MAPFLAG_NAMES		flag() const;
    void	setFlag(MAPFLAG_NAMES flag, bool state) const;

    int		smoke() const;
    void	setSmoke(int smoke);

    TERRAIN_NAMES terrain() const;

    /// prepares a square to be floored, ie, wall all surrounding
    /// and verify no portals.  Returns false if fails, note
    /// some walls may be created.
    bool	prepSquareForDestruction() const;

    /// Finds the room we are in and opens exits in all four directions,
    /// forcibly.
    bool	forceConnectNeighbours() const;

    /// Digs out this square
    /// Returns false if undiggable.  Might also fail if adjacent
    /// true squares aren't safe as we don't want to dig around portals,
    /// etc
    bool	digSquare() const;

    bool	isFOV() const { return flag() & MAPFLAG_FOV ? true : false; }
    bool	isFOVCache() const { return flag() & MAPFLAG_FOVCACHE ? true : false; }
    bool	isTrap() const { return flag() & MAPFLAG_TRAP ? true : false; }
    void	clearTrap() { setFlag(MAPFLAG_TRAP, false); }

    bool	isMapped() const { return flag() & MAPFLAG_MAPPED ? true: false; }

    MOB 	*mob() const;
    ITEM 	*item() const;
    int	 	 allItems(ITEMLIST &items) const;

    MAP		*map() const { return myMap; }

    // Query functoins
    const TILE_DEF &defn() const { return defn(tile()); }
    static const TILE_DEF &defn(TILE_NAMES tile) { return *GAMEDEF::tiledef(tile); }

    bool	 isPassable() const { return defn().ispassable; }

    int		 depth() const;

    void	 setMap(MAP *map) { myMap = map; if (!map) myRoomId = -1; }

    void	 save(ostream &os) const;
    // In place load
    void	 load(istream &is);

    // A strange definition of const..
    void	 removeMob(MOB *mob) const;
    void	 addMob(MOB *mob) const;
    void	 moveMob(MOB *mob, POS oldpos) const;
    void	 removeItem(ITEM *item) const;
    // DO NOT ALL THIS, use move() on item.
    // Why?  Because it may delete the item.
    void	 addItem(ITEM *item) const;

    int 	 getAllMobs(MOBLIST &list) const;
    int 	 getAllItems(ITEMLIST &list) const;

    // Returns colour of the room we are in.  Do not stash this
    // pointer.
    ATTR_NAMES	 room_color() const;
    u8		 room_symbol() const;
    u8		 colorblind_symbol(u8 oldsym) const;
    int		*jacobian() const;

    // Use with much discretion!
    int		 roomId() const { return myRoomId; }
    int		 roomSeed() const;
    int		 getRoomDepth() const;

private:
    ROOM	*room() const;
    // Transforms dx & dy into our local facing.
    void	 rotateToLocal(int &dx, int &dy) const;
    // And back to world.
    void	 rotateToWorld(int &dx, int &dy) const;
    // Same sense as rotateToLocal:
    // (Welcome to transform hell!)
    static void	 rotateByAngle(int angle, int &dx, int &dy);

    // Null if not on a map.
    int		 myRoomId;
    MAP		*myMap;

    // The room relative coords
    int			 myX, myY;
    // Which way we face in the room.
    int			 myAngle;

    friend class ROOM;
    friend class FRAGMENT;
    friend class MAP;
};

/// Note that portals are directed.  mySrc is assuemd to have angle of 0
/// as the turning effect is stored in myDst's angle.
class PORTAL
{
public:
    void	 save(ostream &os) const
    { mySrc.save(os);  myDst.save(os); }
    static PORTAL load(istream &is)
    {
	PORTAL		result;
	result.mySrc.load(is);
	result.myDst.load(is);
	return result;
    }

    POS		mySrc, myDst;
};

class ROOM
{
    ROOM();
    ROOM(const ROOM &room);
    ROOM &operator=(const ROOM &room);

    ~ROOM();

    int		width() const { return myWidth; }
    int		height() const { return myHeight; }

    static bool link(ROOM *a, int dira, ROOM *b, int dirb);
    static bool buildPortal(POS a, int dira, POS b, int dirb, bool settile=true);

    // mySrc is the portal square to launch from, myDst is the
    // destination square to land on
    POS		findProtoPortal(int dir);

    // Convert all of our portals back into proto portals so
    // we can relink
    void	revertToProtoPortal();

    POS		findUserProtoPortal(int dir);

    ROOMTYPE_NAMES	type() const { return myType; }
    const ROOMTYPE_DEF &defn() const { return glb_roomtypedefs[type()]; }

    POS		findCentralPos() const;
    // Finds passable location near the start, but not nearer than
    // startrad.
    POS		spiralSearch(POS start, int startrad = 0, bool avoidmob=true) const;

    void	save(ostream &os) const;
    static ROOM	*load(istream &is);

    int		getDepth() const { return myDepth; }
    int		getId() const { return myId; }
    u8		getSymbol() const { return mySymbol; }
    void	setSymbol(u8 sym) { mySymbol = sym; }

    int		getSeed() const { return mySeed; }

    int		atlasX() const { return myAtlasX; }
    int		atlasY() const { return myAtlasY; }

    bool	forceConnectNeighbours();

    MAP		*map() const { return myMap; }

private:
    // These all work in the room's local coordinates.
    TILE_NAMES		getTile(int x, int y) const;
    void		setTile(int x, int y, TILE_NAMES tile);
    MAPFLAG_NAMES	getFlag(int x, int y) const;

    void		setFlag(int x, int y, MAPFLAG_NAMES flag, bool state);
    void		setAllFlags(MAPFLAG_NAMES flag, bool state);

    int			getSmoke(int x, int y) const;
    void		setSmoke(int x, int y, int smoke);
    void		updateMaxSmoke();
    void		simulateSmoke(int colour);
    int			maxSmoke() const { return myMaxSmoke; }

    int			getDistance(int mapnum, int x, int y) const;
    void		setDistance(int mapnum, int x, int y, int dist);
    void		clearDistMap(int mapnum);

    PORTAL		*getPortal(int x, int y) const;

    // Remvoes the portal that lands at dst.
    void		 removePortal(POS dst);

    // Removes all portals in this room
    void		 removeAllPortals();
    // Removes all portals out of this roomtype
    void		 removeAllForeignPortals();
    // Removes all portals heading into the given type.
    void		 removeAllPortalsTo(ROOMTYPE_NAMES type);

    void	 	 getAllItems(int x, int y, ITEMLIST &list) const;

    void		 setMap(MAP *map);

    void		 resize(int w, int h);

    POS			 buildPos(int x, int y) const;
    POS			 getRandomPos(POS p, int *n) const;
    POS			 getRandomTile(TILE_NAMES tile, POS p, int *n) const;

    void		 deleteContents();

    int			 getConnectedRooms(ROOMLIST &list) const;

    void		 rotateEntireRoom(int angle);

    int			myId;
    PTRLIST<PORTAL>	myPortals;

    int			myAtlasX, myAtlasY;
    int			mySeed;

    // 0: Disconnected.  1: Connected.  2: Limbo.  3: Implicit connection
    enum
    {
	DISCONNECTED,
	CONNECTED,
	UNDECIDED,
	IMPLICIT
    };
    u8			myConnectivity[4];

    int			 myWidth, myHeight;
    TEXTURE8		 myTiles;
    TEXTURE8		 myFlags;
    TEXTURE8		 mySmoke;
    u8			 myConstantFlags;
    u8			 myMaxSmoke;
    TEXTUREI		 myDists;
    MAP			*myMap;

    ROOMTYPE_NAMES	 myType;
    int			 myDepth;
    u8			 mySymbol;

    // Maybe null after load!
    const FRAGMENT	*myFragment;

    friend class POS;
    friend class FRAGMENT;
    friend class MAP;
};

// Stores the fact that a mob is being delayed and where it wants
// to go.
class MOBDELAY
{
public:
    MOBDELAY() { myMob = 0; myDx = 0; myDy = 0; }
    explicit MOBDELAY(int foo) { myMob = 0; myDx = 0; myDy = 0; }
    MOBDELAY(MOB *mob, int dx, int dy)
    { myMob = mob; myDx = dx; myDy = dy; }

    MOB		*myMob;
    int		 myDx, myDy;
};

class MAP
{
public:
    explicit MAP(int depth, MOB *avatar, DISPLAY *display);
    explicit MAP(istream &is);
    MAP(const MAP &map);
    MAP &operator=(const MAP &map);

    static void		 	 buildLevelList();

    void			 incRef();
    void			 decRef();
    int			 	 refCount() const;

    MOB				*avatar() const { return myAvatar; }
    void			 setAvatar(MOB *mob) { myAvatar = mob; }

    PARTY			&party() { return myParty; }
    const PARTY			&party() const { return myParty; }

    void			 doHeartbeat();
    void			 doMoveNPC();

    // Rebuilds this map in place with the new data.
    void			 moveAvatarToDepth(int newdepth);

    int				 getNumMobs() const;
    int				 getAllMobs(MOBLIST &list) const;
    int				 getAllMobsOfType(MOBLIST &list, MOB_NAMES type) const;
    int				 getAllItems(ITEMLIST &list) const;

    void			 setAllFlags(MAPFLAG_NAMES flag, bool state);

    int			 	 buildDistMap(POS center);

    static ROOMTYPE_NAMES		 transitionRoom(ROOMTYPE_NAMES lastroom);

    // Adds a mob to our dead list.
    // This way we can delay actual desctruction to a safe time.
    void			 addDeadMob(MOB *mob);

    // Notes that this mob wants to move in this direction but
    // there is a mob in the way.  The collision resolution subsystem
    // should re-run the AI if it can clear out its destination.
    // If delays are disabled, returns false.  Otherwise true to
    // mark this ai complete.
    bool			 requestDelayedMove(MOB *mob, int dx, int dy);

    int				 getId() const { return myUniqueId; }

    int		getMobCount(MOB_NAMES mob) const { if (mob >= myMobCount.entries()) return 0; return myMobCount(mob); }

    static void			 init();

    // Computes the delta to add to a to get to b.
    // Returns false if unable to compute it.  Things outside of FOV
    // are flakey as we don't cache them as I'm very, very, lazy.
    // And I already burned all the free cycles in the fire sim.
    bool			 computeDelta(POS a, POS b, int &dx, int &dy);

    // Reinitailizes FOV around avatar.
    void			 rebuildFOV();

    // Cache all the item position distances as we will likely
    // want them soon enough
    void			 cacheItemPos();

    void			 buildReflexivePortal(POS pos, int dir);
    void			 buildUserPortal(POS pos, int pnum, int dir,
						int avatardir);

    // Final dungeon collapse
    void			 triggerOrcInvasion();

    // Returns a central location in a random room of that type.
    POS				 findRoomOfType(ROOMTYPE_NAMES roomtype) const;

    // Returns a room at the given atlas location, null if it doesn't
    // exist.
    ROOM			*findAtlasRoom(int x, int y) const;

    // Returns a location in astral plane.
    POS				 astralLoc() const;

    int				 worldLevel() const { return myWorldLevel; }
    void			 ascendWorld();

    // Finds matching item from uid, assuming it is on the ground.
    ITEM			*findItem(int uid) const;

    // Finds matching mob from a uid.
    MOB				*findMob(int uid) const;
    MOB				*findMobByType(MOB_NAMES type) const;
    MOB				*findMobByHero(HERO_NAMES type) const;

    void			 findCloseMobs(MOBLIST &close, POS pos, int rad) const;

    POS			 	 getRandomTile(TILE_NAMES tile) const;
    // Finds passable location near the start, but not nearer than
    // startrad.
    POS		spiralSearch(POS start, int startrad = 0, bool avoidmob=true) const;

    // Do not use.
    DISPLAY			*getDisplay() const { return myDisplay; }
    void			 setDisplay(DISPLAY *disp) { myDisplay = disp; }

    void			 save() const;
    static MAP			*load();

    int				 getTime() const;
    PHASE_NAMES			 getPhase() const;

    static ATTR_NAMES		 depthColor(int depth, bool invert);

    void	expandActiveRooms(int roomid, int depth);

protected:
    ~MAP();

    // You need to delete contents first!
    void	rebuild(int depth, MOB *avatar, DISPLAY *display, bool climbeddown);

    int				allocDistMap(POS center);
    int				lookupDistMap(POS center) const;
    void			clearDistMap(int map);

    void			buildSquare(ROOMTYPE_NAMES roomtype,
				    ROOM *entrance, int enterdir,
				    ROOM *&exit, int &exitdir,
				    int difficulty,
				    int depth,
				    int w, int h);

    // Remove all dead mobs from our mob list.
    void			reapMobs();

    void			updateChainOfCommand(MOBLIST &officers);
    void			updateComSet();

    void			getCanonicalCoord(POS pos, int &x, int &y) const;
    POS				buildFromCanonical(int x, int y) const;

    // Register or move mobs, called from POS.
    void			removeMob(MOB *mob, POS pos);
    void			addMob(MOB *mob, POS pos);
    void			moveMob(MOB *mob, POS pos, POS oldpos);

    // Register or move items.  Called from POS
    void	 		removeItem(ITEM *item, POS pos);
    void	 		addItem(ITEM *item, POS pos);

    // Search our mob list, ideally has accel structure in it.
    // (This is where conversion to canonical and mob tree should have
    // gone)
    MOB				*findMobByLoc(ROOM *room, int x, int y) const;
    int				 findAllMobsByLoc(MOBLIST &list, ROOM *room, int x, int y) const;
    ITEM			*findItemByLoc(ROOM *room, int x, int y) const;
    int				 findAllItemsByLoc(ITEMLIST &list, ROOM *room, int x, int y) const;

    void			deleteContents();

    MOB				*findAvatar();

    ATOMIC_INT32		myRefCnt;

    int				myUniqueId;

    POS				myDistMapCache[DISTMAP_CACHE_ALLOC];
    mutable double		myDistMapWeight[DISTMAP_CACHE_ALLOC];

    POS				myUserPortal[2];
    int				myUserPortalDir[2];

    ROOMLIST			myRooms;

    MOBLIST			myDeadMobs;
    MOBLIST			myLiveMobs;
    ITEMLIST			myItems;

    int				myWorldLevel;
    PARTY			myParty;
    MOB				*myAvatar;

    // The map as seen from our FOV.
    SCRPOS			*myFOVCache;

    DISPLAY			*myDisplay;

    bool			 myAllowDelays;
    MOBDELAYLIST		 myDelayMobList;
    PTRLIST<int> 		 myMobCount;

    static FRAGMENTLIST		 theFragRooms[NUM_ROOMTYPES];

    friend class FRAGMENT;
    friend class ROOM;
    friend class POS;
};


#endif
