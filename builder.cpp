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

#include "builder.h"
#include "roomdata.h"
#include "vec2.h"
#include "gamedef.h"
#include "config.h"
#include <fstream>

class RCT
{
public:
    int		x, y, w, h;

    bool	overlap(const RCT &o) const
    {
	if (MAX(x, o.x) > MIN(x+w, o.x+o.w))
	    return false;
	if (MAX(y, o.y) > MIN(y+h, o.y+o.h))
	    return false;
	return true;
    }
};

BUILDER::BUILDER(u8 *data, int width, int height, int stride, int depth)
{
    myData = data;
    myW = width;
    myH = height;
    myStride = stride;
    J_ASSERT(myStride >= myW);

    myDepth = depth;
}

void
BUILDER::clear(int x, int y, int w, int h, u8 val)
{
    for (int i = 0; i < h; i++)
	for (int j = 0; j < w; j++)
	    set(x+j, y+i, val);
}

void
BUILDER::replace(u8 val, u8 newval)
{
    int		x, y;
    FORALL_XY(x, y)
    {
	if (get(x, y) == val)
	    set(x, y, newval);
    }
}

void
BUILDER::replace_chance(u8 val, u8 newval, int chance)
{
    int		x, y;
    FORALL_XY(x, y)
    {
	if (get(x, y) == val)
	{
	    if (rand_chance(chance))
		set(x, y, newval);
	}
    }
}

void
BUILDER::build(ROOMTYPE_NAMES type)
{
    switch (type)
    {
	case ROOMTYPE_ROGUE:
	    buildRogue();
	    break;
	case ROOMTYPE_FACE:
	case ROOMTYPE_CORRIDOR:
	case ROOMTYPE_ATRIUM:
	case ROOMTYPE_BARRACKS:
	case ROOMTYPE_GUARDHOUSE:
	case ROOMTYPE_THRONEROOM:
	case ROOMTYPE_VAULT:
	case ROOMTYPE_KITCHEN:
	case ROOMTYPE_DININGROOM:
	case ROOMTYPE_PANTRY:
	case ROOMTYPE_LIBRARY:
	    buildFace(type);
	    break;
	case ROOMTYPE_DARKCAVE:
	case ROOMTYPE_CAVE:
	    // These use same generator, just change up the
	    // floor tiles
	    buildCave();
	    break;
	case ROOMTYPE_MAZE:
	    buildMaze();
	    break;
	case ROOMTYPE_BIGROOM:
	    buildBigRoom();
	    break;
	case ROOMTYPE_WILDERNESS:
	{
	    TERRAIN_NAMES	terrain[9];
	    for (int i = 0; i < 9; i++)
		terrain[i] = TERRAIN_PLAINS;
	    buildWilderness(terrain);
	    break;
	}
	default:
	    J_ASSERT(!"Unhandled roomtype!");
	    break;
    }
    // savemap("badconnect.map");
}

void
BUILDER::buildRogue()
{
    int		minrooms = 4;
    int		maxrooms = 30;
    while (1)
    {
	clear(' ');

	PTRLIST<RCT> 	roomlist;

	// Try to place rooms until failures accumulate.
	while (1)
	{
	    if (roomlist.entries() >= maxrooms)
		break;

	    int 		fails = 0;
	    int			ntry = 50;
	    RCT		rect;
	    
	    // Pick the room size, then the position, so we
	    // don't bias to small rooms.
	    rect.w = rand_range(3+3, 5+3) + rand_choice(4) + rand_choice(3);
	    rect.h = rand_range(3+3, 4+3) + rand_choice(3);
	    
	    while (fails < ntry)
	    {
		// We need walls and a single border on the + side
		// to ensure no two rooms abutt.
		// Our min size is a 2x2

		rect.x = rand_range(1, width()-rect.w-1);
		rect.y = rand_range(1, height()-rect.h-1);

		// See if we overlap
		bool		overlap = false;
		for (int i = 0; i < roomlist.entries(); i++)
		    if (rect.overlap(roomlist(i)))
		    {
			overlap = true;
			break;
		    }

		if (overlap)
		{
		    fails++;
		}
		else
		{
		    roomlist.append(rect);
		    break;
		}
	    }
	    if (fails == ntry)
		break;
	}

	// If too few rooms, quit.
	if (roomlist.entries() < minrooms)
	    continue;

	// Stamp all the rooms.
	for (int i = 0; i < roomlist.entries(); i++)
	{
	    clear(roomlist(i).x, roomlist(i).y, roomlist(i).w-1, roomlist(i).h-1,
		    '#');
	    clear(roomlist(i).x+1, roomlist(i).y+1, roomlist(i).w-3, roomlist(i).h-3,
		    '.');
	}

	// Ensure we are unbiased.
	roomlist.shuffle();

	// Connect all pairs
	bool		ok = true;
	for (int i = 1; i < roomlist.entries(); i++)
	{
	    RCT		a = roomlist(rand_choice(i));
	    RCT		b = roomlist(i);
	    // Remove padding
	    a.w--; a.h--;
	    b.w--; b.h--;
	    ok &= connectRooms(a, b);
	}

	// Failed to connect?
	if (!ok)
	    continue;

	if (!dressLevel())
	{
	    continue;
	}
	break;
    }
}

bool
BUILDER::findRandom(u8 val, int &x, int &y) const
{
    int		nfound = 0;

    for (int i = 0; i < height(); i++)
	for (int j = 0; j < width(); j++)
	    if (get(j, i) == val)
	    {
		if (!rand_choice(++nfound))
		{
		    x = j;
		    y = i;
		}
	    }
    return nfound > 0;
}

RCT
BUILDER::rand_rect(int minw, int minh, int maxw, int maxh) const
{
    RCT		result;

    J_ASSERT(minw < width()-2);
    J_ASSERT(minh < height()-2);

    result.w = rand_range(minw, maxw);
    result.h = rand_range(minh, maxh);

    result.w = MIN(result.w, width()-2);
    result.h = MIN(result.h, height()-2);

    result.x = rand_range(1, width()-result.w-1);
    result.y = rand_range(1, height()-result.h-1);

    return result;
}

bool
BUILDER::connectRooms(const RCT &a, const RCT &b)
{
    ROOMDATA<int>		dist(width(), height(), -1);

    // Find our starting location
    IVEC2		p_a, p_b;

    PTRLIST<IVEC2>	valid;
    for (int x = 1; x < a.w-1; x++)
    {
	valid.append(IVEC2(a.x+x, a.y));
	valid.append(IVEC2(a.x+x, a.y+a.h-1));
    }
    for (int y = 1; y < a.h-1; y++)
    {
	valid.append(IVEC2(a.x, a.y+y));
	valid.append(IVEC2(a.x+a.w-1, a.y+y));
    }

    p_a = valid(rand_choice(valid.entries()));

    valid.clear();
    for (int x = 1; x < b.w-1; x++)
    {
	valid.append(IVEC2(b.x+x, b.y));
	valid.append(IVEC2(b.x+x, b.y+b.h-1));
    }
    for (int y = 1; y < b.h-1; y++)
    {
	valid.append(IVEC2(b.x, b.y+y));
	valid.append(IVEC2(b.x+b.w-1, b.y+y));
    }

    p_b = valid(rand_choice(valid.entries()));

    // Ensure start and end are doors
    set(p_a, rand_choice(4) ? 'x' : '.');
    set(p_b, rand_choice(4) ? 'x' : '.');

    // Buid a distance map seeded at p_b
    valid.clear();
    valid.append(p_b);

    dist(p_b) = 0;

    while (valid.entries())
    {
	IVEC2		p = valid.removeFirst();
	int		d = dist(p);
	J_ASSERT(d >= 0);

	// Update four way neighbours.  By doing this we encourage
	// orthogonal lines.
	int		nd = d + 2 + rand_choice(4);

	for (int angle = 0; angle < 4; angle++)
	{
	    int		dx, dy;
	    rand_getdirection(angle, dx, dy);
	    IVEC2	np = p;
	    np.x() += dx;
	    np.y() += dy;

	    if (dist.inbounds(np))
	    {
		// Check walkable!
		int nnd = nd;
		// Cheaper to re-use ,
		if (get(np) == ',')
		    nnd--;

		if (strchr(", x", get(np)))
		{
		    if (dist(np) < 0 || dist(np) > nnd)
		    {
			dist(np) = nnd;
			valid.append(np);
		    }
		}
	    }
	}
    }

    if (dist(p_a) < 0)
    {
	// Failed to connect!
	return false;
    }

    // Starting with p_a we have max steps given by dist(p_a)
    int		maxdist = (dist(p_a) + 2) * 2;
    int		step = 0;
    IVEC2	p = p_a;
    int		dx = 0, dy = 1;

    while (step < maxdist)
    {
	int		curd = dist(p);

	if (p == p_b)
	    break;

	// try last direction.
	IVEC2		np = p;
	np.x() += dx;	np.y() += dy;
	int		nd = dist.inbounds(np) ? dist(np) : -1;

	// 4/5 times try last direction to keep straightish.
	if (rand_choice(5) && (nd >= 0 && nd < curd))
	{
	    if (nd)
	    {
		// Replace
		set(np, ',');
	    }
	    step++;
	    p = np;
	    continue;
	}

	// Look for a new downhill.
	int		angle = rand_choice(4);
	bool		found = false;
	for (int dir = 0; dir < 4; dir++)
	{
	    rand_getdirection(dir+angle, dx, dy);
	    np = p;
	    np.x() += dx;	np.y() += dy;
	    int		nd = dist.inbounds(np) ? dist(np) : -1;
	    if (nd >= 0 && nd < curd)
	    {
		if (nd)
		{
		    // Replace
		    set(np, ',');
		}
		step++;
		p = np;
		found = true;
		break;
	    }
	}

	if (!found)
	{
	    // Nothing downwind???
	    return false;
	}
    }

    return true;
}

void
BUILDER::buildFace(ROOMTYPE_NAMES type)
{
    while (1)
    {
	clear('#');

	if (type == ROOMTYPE_CORRIDOR)
	{
	    int	w = width()-6;
	    int	h = height()-6;

	    int		midx = width() / 2;
	    int		midy = height() / 2;
	    int		cx, cy;
	    int		hall_width = rand_range(1, 3);
	    int		hall_height = rand_range(1, 3);
	    int		merge_minx, merge_miny;
	    int		merge_maxx, merge_maxy;

	    // Proto portals.
	    cy = rand_range(4, height()-hall_height-4);
	    merge_miny = cy;
	    merge_maxy = cy+hall_height;
	    clear(2, cy, 1, hall_height, '+');
	    clear(1, cy, 1, hall_height, ' ');
	    clear(3, cy, midx-3+1, hall_height, '.');

	    cy = rand_range(4, height()-hall_height-4);
	    merge_miny = MIN(cy, merge_miny);
	    merge_maxy = MAX(cy+hall_height, merge_maxy);
	    clear(width()-3, cy, 1, hall_height, '+');
	    clear(width()-2, cy, 1, hall_height, ' ');
	    clear(midx, cy, width()-3 - midx, hall_height, '.');

	    cx = rand_range(4, width()-hall_width-4);
	    merge_minx = cx;
	    merge_maxx = cx + hall_width;
	    clear(cx, 2, hall_width, 1, '+');
	    clear(cx, 1, hall_width, 1, ' ');
	    clear(cx, 3, hall_width, midy-3+1, '.');

	    cx = rand_range(4, width()-hall_width-4);
	    merge_minx = MIN(cx, merge_minx);
	    merge_maxx = MAX(cx+hall_width, merge_maxx);
	    clear(cx, height()-3, hall_width, 1, '+');
	    clear(cx, height()-2, hall_width, 1, ' ');
	    clear(cx, midy, hall_width, height()-3 - midy, '.');

	    // Ensure central hall is open.
	    clear(merge_minx, merge_miny, merge_maxx-merge_minx, merge_maxy-merge_miny, '.');

	    if (!dressLevel(rand_chance(35), rand_chance(15)))
		continue;
	    break;
	}
	else
	{
	    clear(3, 3, width()-6, height()-6, '.');

	    int	w = width()-6;
	    int	h = height()-6;

	    int		hall_width = rand_range(1, 3);
	    int		hall_height = rand_range(1, 3);
	    if (type == ROOMTYPE_ATRIUM)
	    {
		hall_width = hall_height = 1;
	    }

	    int		midx = width() / 2;
	    int		midy = height() / 2;
	    int		cx, cy;

	    // Proto portals.
	    if (type == ROOMTYPE_ATRIUM)
		cy = height() / 2;
	    else
		cy = rand_range(4, height()-4-hall_height);
	    clear(2, cy, 1, hall_height, '+');
	    clear(1, cy, 1, hall_height, ' ');

	    if (type == ROOMTYPE_ATRIUM)
		cy = height() / 2;
	    else
		cy = rand_range(4, height()-4-hall_height);
	    clear(width()-3, cy, 1, hall_height, '+');
	    clear(width()-2, cy, 1, hall_height, ' ');

	    if (type == ROOMTYPE_ATRIUM)
		cx = width() / 2;
	    else
		cx = rand_range(4, width()-4-hall_width);
	    clear(cx, 2, hall_width, 1, '+');
	    clear(cx, 1, hall_width, 1, ' ');

	    if (type == ROOMTYPE_ATRIUM)
		cx = width() / 2;
	    else
		cx = rand_range(4, width()-4-hall_width);
	    clear(cx, height()-3, hall_width, 1, '+');
	    clear(cx, height()-2, hall_width, 1, ' ');

	    if (type == ROOMTYPE_ATRIUM)
	    {
		set(width()/2, height()/2, '<');

		set(3, 3, 'K');
		set(width()-4, height()-4, 'K');
		set(3, height()-4, 'K');
		set(width()-4, 3, 'K');

		if (!dressLevel(0, 1))
		    continue;
	    }
	    else if (type == ROOMTYPE_BARRACKS)
	    {
		int		nbed = rand_range(2, 4);
		for (int i = 0; i < nbed; i++)
		{
		    int		sx, sy;
		    bool found = findRandom('.', sx, sy);
		    if (found)
			set(sx, sy, '=');
		}
		if (!dressLevel(rand_range(1,3), rand_range(1,3)))
		    continue;
	    }
	    else if (type == ROOMTYPE_GUARDHOUSE)
	    {
		int		nchair = rand_range(2, 3);
		for (int i = 0; i < nchair; i++)
		{
		    int		sx, sy;
		    bool found = findRandom('.', sx, sy);
		    if (found)
			set(sx, sy, 'h');
		}
		if (!dressLevel(rand_range(2,4), rand_range(1, 3)))
		    continue;
	    }
	    else if (type == ROOMTYPE_THRONEROOM)
	    {
		int		nchair = rand_range(4, 6);
		for (int i = 0; i < nchair; i++)
		{
		    int		sx, sy;
		    bool found = findRandom('.', sx, sy);
		    if (found)
			set(sx, sy, 'h');
		}
		nchair = rand_range(0, 2);
		for (int i = 0; i < nchair; i++)
		{
		    int		sx, sy;
		    bool found = findRandom('.', sx, sy);
		    if (found)
			set(sx, sy, 'K');
		}
		if (!dressLevel(1, rand_range(1,5)))
		    continue;
	    }
	    else if (type == ROOMTYPE_VAULT)
	    {
		int		nshelf = rand_range(2, 6);
		for (int i = 0; i < nshelf; i++)
		{
		    int		sx, sy;
		    bool found = findRandom('.', sx, sy);
		    if (found)
			set(sx, sy, '?');
		}
		if (!dressLevel(0, rand_range(7,10)))
		    continue;
	    }
	    else if (type == ROOMTYPE_LIBRARY)
	    {
		int		nshelf = rand_range(3, 6);
		for (int i = 0; i < nshelf; i++)
		{
		    int		sx, sy;
		    bool found = findRandom('.', sx, sy);
		    if (found)
			set(sx, sy, '?');
		}
		if (!dressLevel(rand_range(0,1), 0))
		    continue;
	    }
	    else if (type == ROOMTYPE_KITCHEN)
	    {
		for (int i = 0; i < 1; i++)
		{
		    int		sx, sy;
		    bool found = findRandom('.', sx, sy);
		    if (found)
		    {
			if (get(sx-1, sy) == '.')
			    set(sx-1, sy, 'h');
			else if (get(sx+1, sy) == '.')
			    set(sx+1, sy, 'h');

			set(sx, sy, 'O');
		    }
		    found = findRandom('.', sx, sy);
		    if (found)
		    {
			set(sx, sy, '*');
		    }
		}
		if (!dressLevel(rand_range(1,2), rand_range(1,2)))
		    continue;
	    }
	    else if (type == ROOMTYPE_PANTRY)
	    {
		int		nshelf = rand_range(4, 6);
		for (int i = 0; i < nshelf; i++)
		{
		    int		sx, sy;
		    bool found = findRandom('.', sx, sy);
		    if (found)
			set(sx, sy, '?');
		}
		if (!dressLevel(1 +rand_chance(50), 0))
		    continue;
	    }
	    else if (type == ROOMTYPE_DININGROOM)
	    {
		int		ntable = rand_range(1, 3);
		for (int i = 0; i < ntable; i++)
		{
		    int		sx, sy;
		    bool found = findRandom('.', sx, sy);
		    if (found)
		    {
			int		dx, dy;
			FORALL_4DIR(dx, dy)
			{
			    if (get(sx+dx, sy+dy) == '.')
			    {
				if (rand_chance(75))
				    set(sx+dx, sy+dy, 'h');
			    }
			}
			set(sx, sy, 'O');
		    }
		}
		if (!dressLevel(0, rand_range(0,3)))
		    continue;
	    }

	    if (myDepth >= 5)
	    {
		dressLevel(rand_chance(50), 0);
	    }
	    if (myDepth >= 10)
	    {
		dressLevel(rand_chance(50), 0);
	    }
	    if (myDepth >= 15)
	    {
		dressLevel(rand_chance(50), 0);
	    }

	    break;
	}
    }
}

void
BUILDER::buildBigRoom()
{
    while (1)
    {
	clear('#');
	clear(3, 3, width()-6, height()-6, '.');

	if (!dressLevel())
	    continue;

	break;
    }
}

void
BUILDER::growBushes()
{
    // This is a GS process, so likely should reverse the sweeps.  Make
    // up for it by just randomizing the transition rules below unity.
    for (int y = 0; y < height(); y++)
    {
	for (int x = 0; x < width(); x++)
	{
	    int grass = 0, bush = 0, tree = 0, waste = 0;
	    // Gather neighbour counts.
	    for (int dy = -1; dy <= 1; dy++)
	    {
		for (int dx = -1; dx <= 1; dx++)
		{
		    switch (wrapget(x+dx, y+dy))
		    {
			case '_':
			    waste++;
			    break;
			case '.':
			    grass++;
			    break;
			case '*':
			    bush++;
			    break;
			case '&':
			    tree++;
			    break;
		    }
		}
	    }
	    switch (get(x, y))
	    {
		case '.':
		    // Grass will occassionally switch to *.
		    if (grass == 9)
		    {
			if (rand_chance(1))
			{
			    set(x, y, '*');
			}
			if (rand_chance(1))
			{
			    set(x, y, '_');
			}
		    }
		    else
		    {
			// Probability to * depends on neighbour count
			if (rand_chance( (bush + tree) * 10 ))
			    set(x, y, '*');
			else if (rand_chance( waste * 10 ))
			    set(x, y, '_');
		    }
		    break;
		case '*':
		    // Bushes grow to trees where dense.
		    if (bush == 9)
		    {
			if (rand_chance(90))
			    set(x, y, '&');
		    }
		    else
		    {
			// Trees convert bushes with strength.
			if (rand_chance( tree * 10 ))
			    set(x, y, '&');
		    }
		    break;
		case '&':
		    // Forests die off when crowded.
		    if (tree == 9)
		    {
			if (rand_chance(20))
			{
			    set(x, y, '.');
			}
		    }
		    break;
	    }
	}
    }
}

void
BUILDER::growForests()
{
    // This is a GS process, so likely should reverse the sweeps.  Make
    // up for it by just randomizing the transition rules below unity.
    for (int y = 0; y < height(); y++)
    {
	for (int x = 0; x < width(); x++)
	{
	    int grass = 0, bush = 0, tree = 0, waste = 0;
	    // Gather neighbour counts.
	    for (int dy = -1; dy <= 1; dy++)
	    {
		for (int dx = -1; dx <= 1; dx++)
		{
		    switch (wrapget(x+dx, y+dy))
		    {
			case '_':
			    waste++;
			    break;
			case '.':
			    grass++;
			    break;
			case '*':
			    bush++;
			    break;
			case '&':
			    tree++;
			    break;
		    }
		}
	    }
	    switch (get(x, y))
	    {
		case '.':
		    // Freeze plains...
		    break;
		case '*':
		    // Bushes grow to trees where dense.
		    if (bush == 9)
		    {
			if (rand_chance(90))
			    set(x, y, '&');
		    }
		    else
		    {
			// Trees convert bushes with strength.
			if (rand_chance( tree * 10 ))
			    set(x, y, '&');
		    }
		    break;
		case '&':
		    // Forests die off when crowded.
		    if (tree == 9)
		    {
			if (rand_chance(20))
			{
			    set(x, y, '.');
			}
		    }
		    break;
	    }
	}
    }
}

void
BUILDER::buildOvermap()
{
    while (1)
    {
	clear('.');

	for (int i = 0; i < 5; i++)
	{
	    growBushes();
	}
	for (int i = 0; i < 5; i++)
	{
	    growForests();
	}

	if (!dressLevel())
	    continue;

	break;
    }
}

struct TILE_WEIGHTS
{
public:
    TILE_WEIGHTS()
    {
	grass = bush = dirt = tree = 0;
    }
    TILE_WEIGHTS(TERRAIN_NAMES terrain)
    {
	grass = GAMEDEF::terraindef(terrain)->grass;
	bush = GAMEDEF::terraindef(terrain)->bush;
	dirt = GAMEDEF::terraindef(terrain)->dirt;
	tree = GAMEDEF::terraindef(terrain)->tree;
    }

    TILE_WEIGHTS &operator*=(int factor)
    {
	grass *= factor;
	bush *= factor;
	dirt *= factor;
	tree *= factor;
	return *this;
    }
    TILE_WEIGHTS &operator+=(const TILE_WEIGHTS &b)
    {
	grass += b.grass;
	bush += b.bush;
	tree += b.tree;
	dirt += b.dirt;
	return *this;
    }

    u8		pickRandom()
    {
	int		total = grass + bush + tree + dirt;
	int		choice = rand_choice(total);

	if (choice < grass)
	    return '.';
	choice -= grass;

	if (choice < bush)
	    return '*';
	choice -= bush;

	if (choice < tree)
	    return '&';
	choice -= tree;

	J_ASSERT(choice < dirt);
	return '_';
    }

    int		grass;
    int		bush;
    int		tree;
    int		dirt;
};

inline TILE_WEIGHTS operator*(const TILE_WEIGHTS &v1, int factor)
{ TILE_WEIGHTS result(v1);
  result *= factor;
  return result;
}

void
BUILDER::buildWilderness(const TERRAIN_NAMES *terrain)
{
    while (1)
    {
	TILE_WEIGHTS	corners[4];

	int		idx = 0;
	for (int dy = -1; dy <= 1; dy += 2)
	{
	    for (int dx = -1; dx <= 1; dx += 2)
	    {
		corners[idx] += TILE_WEIGHTS(terrain[ dx + dy * 3 + 4 ]);
		corners[idx] += TILE_WEIGHTS(terrain[ dx + 0  * 3 + 4 ]);
		corners[idx] += TILE_WEIGHTS(terrain[ 0  + dy * 3 + 4 ]);
		corners[idx] += TILE_WEIGHTS(terrain[ 0  + 0  * 3 + 4 ]);
		idx++;
	    }
	}

	int 		x, y;
	FORALL_XY(x, y)
	{
	    TILE_WEIGHTS	weight;
	    weight = corners[0] * (width() - x) * (height() - y);
	    weight += corners[1] * (x) * (height() - y);
	    weight += corners[2] * (width() - x) * (y);
	    weight += corners[3] * (x) * (y);

	    set(x, y, weight.pickRandom());
	}

	if (!dressLevel())
	    continue;

	break;
    }
}

struct WALLCUT
{
    WALLCUT() { x = -1; y = -1; horizontal =true; }
    WALLCUT(int _x, int _y, bool hor) { x = _x; y = _y; horizontal = hor; }

    int		x, y;
    bool	horizontal;
};

int
findClass(int search, PTRLIST<int> &classifier)
{
    if (search < 0)
	return search;

    int		rootclass = classifier(search);

    if (rootclass == search)
    {
	// Root!
	return search;
    }
    if (rootclass < 0)
    {
	// Invalid, done
	return rootclass;
    }

    // Now see if this is the proper root yet...
    int		newroot = findClass(rootclass, classifier);

    // Write back the new root
    classifier(search) = newroot;

    return newroot;
}

void
BUILDER::cutWall(const WALLCUT &cut)
{
    // Not too many doors.
    bool	usedoor = !rand_choice(5);

    if (cut.horizontal)
    {
	int x = cut.x * 3 + 2 + 2;
	int y = cut.y * 3 + 2;

	if (usedoor)
	{
	    y += rand_choice(2);
	    set(x, y, 'x');
	}
	else
	{
	    set(x, y, '.');
	    set(x, y+1, '.');
	}
    }
    else
    {
	int x = cut.x * 3 + 2;
	int y = cut.y * 3 + 2 + 2;

	if (usedoor)
	{
	    x += rand_choice(2);
	    set(x, y, 'x');
	}
	else
	{
	    set(x, y, '.');
	    set(x+1, y, '.');
	}
    }
}

void
BUILDER::buildMaze()
{
    PTRLIST<int>	classifier;
    PTRLIST<WALLCUT>	wallcuts, unusedcuts;
    while (1)
    {
	clear('#');

	// Determine number of maze blocks.
	// EAch one is 3x3 and start 2 in on each side.
	int		tw = (width()-4) / 3;
	int		th = (height()-4) / 3;

	wallcuts.clear();
	unusedcuts.clear();

	for (int ty = 0; ty < th; ty++)
	{
	    for (int tx = 0; tx < tw; tx++)
	    {
		clear(tx * 3 + 2, ty * 3 + 2, 2, 2, '.');
		if (tx < tw-1)
		{
		    wallcuts.append(WALLCUT(tx, ty, true));
		}
		if (ty < th-1)
		{
		    wallcuts.append(WALLCUT(tx, ty, false));
		}
	    }
	}

	classifier.resize(tw * th);
	classifier.constant(-1);

	// Start with each cell connected to itself.
	for (int i = 0; i < tw * th; i++)
	    classifier(i) = i;

	wallcuts.shuffle();
	for (int i = 0; i < wallcuts.entries(); i++)
	{
	    WALLCUT	cut = wallcuts(i);
	    int		dx = 0, dy = 0;
	    if (cut.horizontal)
		dx = 1;
	    else
		dy = 1;

	    int		thisclass = findClass(cut.x + cut.y * tw, classifier);
	    int		otherclass = findClass(cut.x+dx + (cut.y+dy)*tw, classifier);
	    if (thisclass != otherclass)
	    {
		cutWall(cut);
		classifier(otherclass) = thisclass;
	    }
	    else
	    {
		unusedcuts.append(cut);
	    }
	}

	unusedcuts.shuffle();
	// Extra cuts to make it less tree like.
	for (int i = 0; i < 8; i++)
	{
	    if (i >= unusedcuts.entries())
		break;
	    cutWall(unusedcuts(i));
	}

	if (!dressLevel())
	    continue;

	break;
    }
}

void
BUILDER::buildCave()
{
    while (1)
    {
	clear(' ');

	// Try to cut out a certain pecentage of the rock.
	clear(1, 1, width()-2, height()-2, '#');
	// Our seed.
	set(width()/2, height()/2, '.');
	int		cleared = 1;
	int		needtoclear = (width()-2) * (height()-2);

	// 1/3rd clearance.
	needtoclear = needtoclear / 3;

	PTRLIST<IVEC2>		startlist;	

	while (cleared < needtoclear)
	{
	    // Find a new drunk start location bordering a wall.
	    startlist.clear();
	    for (int y = 2; y < height()-3; y++)
	    {
		for (int x = 2; x < width()-3; x++)
		{
		    // Check if there is a neighbouring wall
		    if (get(x,y) == '.')
		    {
			bool	haswall = false;
			int		dx, dy;
			FORALL_4DIR(dx, dy)
			{
			    if (get(x+dx, y+dy) == '#')
			    {
				haswall = true;
				break;
			    }
			}

			if (haswall)
			    startlist.append(IVEC2(x, y));
		    }

		}
	    }

	    // Now we don't want to keep rebuilding that list,
	    // so we will pick a number of walks from this start.
	    for (int walk = 0; walk < 2; walk++)
	    {
		IVEC2	p = startlist(rand_choice(startlist.entries()));

		for (int dist = 0; dist < 15; dist++)
		{
		    int		dx, dy;
		    rand_direction(dx, dy);

		    p.x() += dx;
		    p.y() += dy;

		    if (!isValid(p))
			break;
		    if (get(p) == ' ')
			break;
		    if (get(p) == '#')
			cleared++;
		    set(p, '.');
		}
	    }
	}

	replace(' ', '#');

	if (!dressLevel())
	    continue;

	// All good.
	break;
    }
}

void
BUILDER::savemap(const char *fname) const
{
    ofstream	os(fname);

    for (int i = 0; i < height(); i++)
    {
	for (int j = 0; j < width(); j++)
	{
	    os << get(j, i);
	}
	os << "\n";
    }
}

bool
BUILDER::dressLevel()
{
    int		nmob = rand_range(5, 10);
    int		nitem = rand_range(10, 13);
    if (myDepth <= 0)
    {
	nmob = nitem = 0;
    }

    return dressLevel(nmob, nitem);
}

bool
BUILDER::dressLevel(int nmob, int nitem)
{
    if (myDepth > 0)
    {
	int		sx, sy;
	bool	found;

	// We only need a down stair if we aren't boss level
	if (myDepth < GAMEDEF::rules().bosslevel)
	{
	    found = findRandom('.', sx, sy);
	    J_ASSERT(found);
	    if (!found)
		return false;
	    set(sx, sy, '>');
	}
	found = findRandom('.', sx, sy);
	J_ASSERT(found);
	if (!found)
	    return false;
	set(sx, sy, '<');
    }

    // Add the boss!
    if (myDepth == GAMEDEF::rules().bosslevel)
    {
	int		sx, sy;
	bool found = findRandom('.', sx, sy);
	if (found)
	    set(sx, sy, 'B');
	else
	    return false;
    }

    // Add monsters
    for (int i = 0; i < nmob; i++)
    {
	int		sx, sy;
	bool found = findRandom('.', sx, sy);
	if (found)
	    set(sx, sy, 'A');
    }

    // Add items
    for (int i = 0; i < nitem; i++)
    {
	int		sx, sy;
	bool found = findRandom('.', sx, sy);
	if (found)
	    set(sx, sy, '(');
    }

    // Add guranteed items, one wish stone and portal stone per level!
    // Well, wishes are more rare and randomized as this is about gambling.
    // Soft pity is a stone every three levels.
    if (myDepth > 0 && (rand_chance(50) || (myDepth % 3 == 1)))
    {
	int		sx, sy;
	bool found = findRandom('.', sx, sy);
	if (found)
	    set(sx, sy, 'w');
    }
    if (myDepth > 0)
    {
	int		sx, sy;
	bool found = findRandom('.', sx, sy);
	if (found)
	    set(sx, sy, 'p');
    }

    return true;
}
