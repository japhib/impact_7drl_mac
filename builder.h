/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7 Day Band Development
 *
 * NAME:        builder.h ( 7 Day Band, C++ )
 *
 * COMMENTS:
 *	Verbish class to build ascii maps
 */

#ifndef __builder_h__
#define __builder_h__

#include "glbdef.h"
#include "vec2.h"
#include <assert.h>

class RCT;

struct WALLCUT;

class BUILDER
{
public:
    BUILDER(u8 *data, int width, int height, int stride, int depth);

    void		build(ROOMTYPE_NAMES type);

    void		buildRogue();
    void		buildCave();
    void		buildBigRoom();
    void		buildMaze();
    void		buildFace(ROOMTYPE_NAMES type);
    void		buildWilderness(const TERRAIN_NAMES *terrain);
    void		buildOvermap();

    bool		connectRooms(const RCT &a, const RCT &b);

    int			width() const { return myW; }
    int			height() const { return myH; }

    bool		isValid(int x, int y) const { return (x >= 0 && x < myW && y >= 0 && y < myH); }
    bool		isValid(IVEC2 p) const { return isValid(p.x(), p.y()); }
    u8			get(int x, int y) const
			{ J_ASSERT(isValid(x, y)); return myData[x+y*myStride]; }
    u8			get(IVEC2 p) const
			{ return get(p.x(), p.y()); }
    u8			wrapget(int x, int y) const
			{ wrap(x, y); return myData[x+y*myStride]; }
    u8			wrapget(IVEC2 p) const
			{ wrap(p); return get(p.x(), p.y()); }
    void		set(int x, int y, u8 val)
			{ J_ASSERT(isValid(x, y)); myData[x+y*myStride] = val; }
    void		set(IVEC2 p, u8 val)
			{ set(p.x(), p.y(), val); }

    void		clear(u8 val)
			{ clear(0, 0, myW, myH, val); }
    void		clear(int x, int y, int w, int h, u8 val);

    void		replace(u8 orig, u8 newval);
    void		replace_chance(u8 orig, u8 newval, int chance);

    RCT			rand_rect(int minw, int minh, int maxw, int maxh) const;

    bool		findRandom(u8 val, int &x, int &y) const;
			
    void		savemap(const char *fname) const;

    void		cutWall(const WALLCUT &cut);

    // Run forest growth logic.
    void		growBushes();
    void		growForests();

    // ADds mobs, stairs, etc.
    bool		dressLevel();
    bool		dressLevel(int nmob, int nitem);

    void		wrap(int &x, int &y) const
    { x = x % width();  if (x < 0) x += width();
      y = y % height(); if (y < 0) y += height(); }
    void		wrap(IVEC2 &p) const
    { wrap(p.x(), p.y()); }


protected:
    u8		*myData;
    int		 myW, myH, myStride;
    int	 	 myDepth;
};

#endif
