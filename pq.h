/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        pq.h ( Sword in Hand Library, C++ )
 *
 * COMMENTS:
 *	Implements a priority queue
 */

#ifndef __pq_h__
#define __pq_h__

#include "ptrlist.h"

template <typename T, typename PT>
class PQ_ITEM
{
public:
    PT	priority;
    T	data;
};

template <typename T, typename PT, typename ARRAY>
class PQ
{
public:
    using ITEM = PQ_ITEM<T, PT>;
    PQ();
    ~PQ();
    PQ(const PQ<T, PT, ARRAY> &ref);
    PQ<T, PT, ARRAY> &operator=(const PQ<T, PT, ARRAY> &ref);

    // Adds a new item with a given priority.
    // We extract return the *lowest* priority.
    int		append(PT priority, T item);

    // Empties
    void	clear() { myQ.clear(); }

    int		entries() const { return myQ.entries(); }

    // Remove the lowest priority item.
    T		pop();
    T		top() const { return myQ(0).data; }
    PT		topPriority() const { return myQ(0).priority; }

    // Random access, likely only meaningful for saving!
    T		dataAt(int idx) const { return myQ(idx).data; }
    PT		priorityAt(int idx) const { return myQ(idx).priority; }

protected:
    // idx may be lower than its parents, raise it!
    void	bubbleUp(int idx);
    // idx is *invalid*, raise its children into its spot.
    void	bubbleDown(int idx);


    ARRAY	myQ;
};

// All platforms are crappy now.
#include "pq.cpp"

#endif


