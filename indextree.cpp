/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	1812 Development
 *
 * NAME:        indextree.cpp ( Jacob's Matrix, C++ )
 *
 * COMMENTS:
 *		Searchable index tree.  Pointer free, supports
 *		locality queries.
 */

#include "indextree.h"
#include "pq.h"

int
INDEXTREE::findAllClose(PTRLIST<int> &foundid, int x, int y, int rad) const
{
    int		nfound = 0;

    // Inclusive coordinates.
    int		sx, sy, ex, ey;

    sx = x - rad;
    sy = y - rad;
    ex = x + rad;
    ey = y + rad;

    // Clip to root:
    sx = MAX(0, sx);
    sy = MAX(0, sy);
    ex = MIN(65535, ex);
    ey = MIN(65535, ey);

    // Reject if out of bounds.
    if (sx > ex)
	return 0;
    if (sy > ey)
	return 0;

    nfound = myNodes(0).findAll(*this,
			foundid, 
			sx, sy, ex, ey,
			3);

    return nfound;
}

int
INDEXTREE::NODE::findAll(const INDEXTREE &tree,
		PTRLIST<int> &foundid,
		int sx, int sy,
		int ex, int ey,
		int depth) const
{
    int		nfound = 0;

    if (!depth)
    {
	// Straightforward accumulate anything in inclusive sx..sy range.
	forEachChild([&](int x, int y) -> bool
	{
	    if (x >= sx && x <= ex && y >= sy && y <= ey)
	    {
		foundid.append(getChild(x, y));
		nfound++;
	    }
	    return true;
	});
    }
    else
    {
	// Find coordinates within this tile
	int	tile_sx = sx >> (depth * 4);
	int	tile_sy = sy >> (depth * 4);
	int	tile_ex = ex >> (depth * 4);
	int	tile_ey = ey >> (depth * 4);

	forEachChild([&](int x, int y) -> bool
	{
	    if (x >= tile_sx && x <= tile_ex && y >= tile_sy && y <= tile_ey)
	    {
		// Potential call here
		auto && node = tree.node(getChild(x, y));

		// Clip to our node.
		int clip_sx = MAX(sx, x << (depth*4));
		int clip_ex = MIN(ex, ((x+1) << (depth*4)) - 1);
		int clip_sy = MAX(sy, y << (depth*4));
		int clip_ey = MIN(ey, ((y+1) << (depth*4)) - 1);

		// Mask out the now irrelevant bits.
		int mask = 0xf << (depth * 4);
		clip_sx &= ~mask;
		clip_ex &= ~mask;
		clip_sy &= ~mask;
		clip_ey &= ~mask;

		nfound += node.findAll(tree, foundid, clip_sx, clip_sy, clip_ex, clip_ey, depth-1);
	    }
	    return true;
	});
    }
    return nfound;
}

int
INDEXTREE::findClosest(int x, int y, int rad) const
{
    int		bestindex = 0;
    int		bestdist = 0;

    // Inclusive coordinates.
    int		sx, sy, ex, ey;

    sx = x - rad;
    sy = y - rad;
    ex = x + rad;
    ey = y + rad;

    // Clip to root:
    sx = MAX(0, sx);
    sy = MAX(0, sy);
    ex = MIN(65535, ex);
    ey = MIN(65535, ey);

    // Reject if out of bounds.
    if (sx > ex)
	return 0;
    if (sy > ey)
	return 0;

    myNodes(0).findClosest(*this,
			bestindex, bestdist,
			x, y,
			0, 0,
			sx, sy, ex, ey,
			3);

    return bestindex;
}

static int
idxtree_computeDistToPoint(int x, int y, int x2, int y2)
{
    x -= x2;
    y -= y2;
    return x * x + y * y;
}

static int
idxtree_computeDistToRect(int x, int y, int sx, int sy, int ex, int ey)
{
    if (x < sx)
	x -= sx;
    else if (x > ex)
	x -= ex;
    else
	x = 0;

    if (y < sy)
	y -= sy;
    else if (y > ey)
	y -= ey;
    else
	y = 0;

    return x * x + y * y;
}

void
INDEXTREE::NODE::findClosest(const INDEXTREE &tree,
		int &bestindex, int &bestdist,
		int queryx, int queryy,
		int basex, int basey,
		int sx, int sy,
		int ex, int ey,
		int depth) const
{
    if (!depth)
    {
	// Straightforward accumulate anything in inclusive sx..sy range.
	// No need for a PQ here.
	forEachChild([&](int x, int y) -> bool
	{
	    if (x >= sx && x <= ex && y >= sy && y <= ey)
	    {
		int dist = idxtree_computeDistToPoint(queryx, queryy,
					basex + x, basey + y);
		if (!bestindex || dist < bestdist)
		{
		    bestindex = getChild(x, y);
		    bestdist = dist;
		}
	    }
	    return true;
	});
    }
    else
    {
	PQ<int, int, FIXEDLIST<PQ_ITEM<int, int>, 64>> pq;

	// Find coordinates within this tile
	int	tile_sx = sx >> (depth * 4);
	int	tile_sy = sy >> (depth * 4);
	int	tile_ex = ex >> (depth * 4);
	int	tile_ey = ey >> (depth * 4);
	
	// First iterate over each child creating a pq so we can
	// process from closest to farthest.
	forEachChild([&](int x, int y) -> bool
	{
	    if (x >= tile_sx && x <= tile_ex && y >= tile_sy && y <= tile_ey)
	    {
		// We haven't rejected this tile out-right.
		int clip_sx = MAX(sx, x << (depth*4));
		int clip_ex = MIN(ex, ((x+1) << (depth*4)) - 1);
		int clip_sy = MAX(sy, y << (depth*4));
		int clip_ey = MIN(ey, ((y+1) << (depth*4)) - 1);

		int mindist = idxtree_computeDistToRect(queryx, queryy,
				    basex + clip_sx, basey + clip_sy,
				    basex + clip_ex, basey + clip_ey);
		// Cull if we have a bestdist.
		if (bestindex && mindist >= bestdist)
		{
		    // Do nothing.
		}
		else
		{
		    pq.append(mindist, (y << 4) + x);
		}
	    }
	    return true;
	});

	// Now run over our resulting nodes in increasing distance.
	while (pq.entries())
	{
	    int		topdist = pq.topPriority();
	    // Early exit if we found something better than we are capable
	    // of.
	    if (bestindex && topdist >= bestdist)
		break;


	    int	pqnode = pq.pop();
	    int x = pqnode & 0xf;
	    int y = pqnode >> 4;

	    // Potential call here
	    auto && node = tree.node(getChild(x, y));

	    // Clip to our node.
	    int clip_sx = MAX(sx, x << (depth*4));
	    int clip_ex = MIN(ex, ((x+1) << (depth*4)) - 1);
	    int clip_sy = MAX(sy, y << (depth*4));
	    int clip_ey = MIN(ey, ((y+1) << (depth*4)) - 1);

	    // Mask out the now irrelevant bits.
	    int mask = 0xf << (depth * 4);
	    clip_sx &= ~mask;
	    clip_ex &= ~mask;
	    clip_sy &= ~mask;
	    clip_ey &= ~mask;

	    node.findClosest(tree,
				bestindex, bestdist,
				queryx, queryy,

				basex + (x << (depth*4)),
				basey + (y << (depth*4)),
				clip_sx, clip_sy, 
				clip_ex, clip_ey, 
				depth-1);
	}
    }
    return;
}

