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

#pragma once

#include "mygba.h"
#include "ptrlist.h"

class INDEXTREE
{
public:
    using INDEX = u16;

    INDEXTREE()
    {
	// Default to empty tree
	myNodes.resize(1);
    }

    void clear()
    {
	myNodes.resize(1);

	myNodes(0).clear();
	myFreeList.clear();
    }

    class NODE
    {
    public:
	NODE()
	{
	    clear();
	}

	void	setChild(int x, int y, INDEX child)
	{ myChild[x + y * 16] = child;
	  if (child)
	      myActive[y] |= 1 << x;
	  else
	      myActive[y] &= ~(1 << x);
	}

	INDEX	getChild(int x, int y) const
	{ return myChild[x + y * 16]; }

	void    clear()
	{
	    memset(this, 0, sizeof(*this));
	}

	int 	findAll(const INDEXTREE &tree,
		        PTRLIST<int> &foundid,
			int sx, int sy,
			int ex, int ey,
			int depth) const;

	void	findClosest(const INDEXTREE &tree,
			int &bestindex,
			int &bestdist,
			int queryx, int queryy,

			int basex, int basey,
			int sx, int sy,
			int ex, int ey,
			int depth) const;

	/// OP is invoked with OP(x, y) for each non-zero child.
	/// Returning true will continue, returning false will exit.
	template <typename OP>
	void	forEachChild(OP &&op) const
	{
	    for (int y = 0; y < 16; y++)
	    {
		u32	mask = activeMask(y);
		while (mask)
		{
		    // Strip off the high bit by itself
		    u32 lowbit = mask & -mask;	// ignore MSVC's whines.
		    // X is the bit position 
		    int x = MYbsl(mask);
		    if (!op(x, y))
			return;
		    mask ^= lowbit;
		}
	    }
	}

	u16	activeMask(int y) const { return myActive[y]; }

    protected:
	u16	myActive[16];
	INDEX	myChild[16 * 16];
    };

#define TRAVERSE(curnode, x, y, keyx, keyy, depth) \
    { (keyx) = ((x) >> (depth*4)) & 0xf; \
      (keyy) = ((y) >> (depth*4)) & 0xf; \
      (curnode) = myNodes(curnode).getChild(keyx, keyy); \
    }

    INDEX	get(int x, int y) const
    {
	if (x < 0 || y < 0) return 0;

	INDEX		curnode = 0;		// Start at root.
	int		keyx, keyy;

	// Unrolled avoids variable bit shifts that are deadly.
	TRAVERSE(curnode, x, y, keyx, keyy, 3);
	if (!curnode)
	    return 0;
	TRAVERSE(curnode, x, y, keyx, keyy, 2);
	if (!curnode)
	    return 0;
	TRAVERSE(curnode, x, y, keyx, keyy, 1);
	if (!curnode)
	    return 0;
	TRAVERSE(curnode, x, y, keyx, keyy, 0);
	if (!curnode)
	    return 0;
	return curnode;
    }

    #define CREATEANDSET(curnode, parentnode, keyx, key) \
	{ if (!curnode) \
	{ \
	    /* Add child to parent. */ \
	    if (myFreeList.entries()) \
		curnode = myFreeList.pop(); \
	    else \
		curnode = myNodes.append(); \
	    myNodes(parent).setChild(keyx, keyy, curnode); \
	} \
	parent = curnode; }

    void	set(int x, int y, INDEX value)
    {
	J_ASSERT(x >= 0 && y >= 0);

	INDEX		curnode = 0;		// Start at root.
	int		keyx, keyy;

	INDEX 		parent = curnode;

	// Unrolled avoids variable bit shifts that are deadly.
	TRAVERSE(curnode, x, y, keyx, keyy, 3);
	CREATEANDSET(curnode, parentnode, keyx, keyy);

	TRAVERSE(curnode, x, y, keyx, keyy, 2);
	CREATEANDSET(curnode, parentnode, keyx, keyy);

	TRAVERSE(curnode, x, y, keyx, keyy, 1);
	CREATEANDSET(curnode, parentnode, keyx, keyy);

	// Now we are guaranteed curnode isn't null, so we can just
	// do the proper set.
	myNodes(curnode).setChild(x & 0xf, y & 0xf, value);
    }

    int		findAllClose(PTRLIST<int> &foundid, int x, int y, int rad) const;

    // Index of closest within rad.  0 if fails.
    int		findClosest(int x, int y, int rad) const;

    // Remove any empty tiles.
    void collapse()
    {
	// note even if root collapses, we don't do anything
	// as we always keep roots!
	collapseNode(0, 3);
    }

    bool collapseNode(INDEX curnode, int depth)
    {
	auto && node = myNodes(curnode);
	bool	anylive = false;
	for (int y = 0; y < 16; y++)
	{
	    if (node.activeMask(y))
	    {
		// TODO: Better iteration by bits!
		for (int x = 0; x < 16; x++)
		{
		    INDEX child = node.getChild(x, y);
		    if (child)
		    {
			if (depth > 0 && collapseNode(child, depth-1))
			{
			    node.setChild(x, y, 0);
			    // Should be all zero as we just finished...
			    myFreeList.append(child);
			}
			else
			{
			    anylive = true;
			}
		    }
		}
	    }
	}
	if (!anylive)
	{
	    // We've collapsed all our children, or we never had
	    // any to begin with!
	    return true;
	}

	// Still stuff.
	return false;
    }

#undef TRAVERSE
#undef CREATEANDSET

protected:
    const NODE &node(int idx) const { return myNodes(idx); }
    PTRLIST<NODE>	myNodes;
    PTRLIST<INDEX>	myFreeList;

    friend class NODE;
};

