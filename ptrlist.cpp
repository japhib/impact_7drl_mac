/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        ptrlist.cpp ( Utility Library, C++ )
 *
 * COMMENTS:
 * 	Implementation of the ptrlist functions
 */

#include <string.h>
#include "assert.h"
#include "ptrlist.h"
#include "rand.h"

#include <algorithm>

template <typename PTR>
PTRLIST<PTR>::PTRLIST()
{
    myEntries = 0;
    mySize = 0;
    myList = 0;
    myStartPos = 0;
}

template <typename PTR>
PTRLIST<PTR>::~PTRLIST()
{
    delete [] myList;
}

template <typename PTR>
PTRLIST<PTR>::PTRLIST(const PTRLIST<PTR> &ref)
{
    myList = 0;
    *this = ref;
}

template <typename PTR>
PTRLIST<PTR> &
PTRLIST<PTR>::operator=(const PTRLIST<PTR> &ref)
{
    int			i;

    if (this == &ref)
	return *this;
    if (!ref.myEntries)
    {
	// Quickly build empty list.
	myList = 0;
	myEntries = mySize = 0;
	myStartPos = 0;
	return *this;
    }
    delete [] myList;
    myList = new PTR[ref.myEntries];
    mySize = ref.myEntries;
    myEntries = ref.myEntries;
    // Reset our list.
    myStartPos = 0;

    for (i = 0; i < myEntries; i++)
    {
	set(i, ref(i));
    }

    return *this;
}

template <typename PTR>
int
PTRLIST<PTR>::append()
{
    return append(PTR{});
}

template <typename PTR>
int
PTRLIST<PTR>::append(PTR item)
{
    if (myEntries + myStartPos >= mySize)
    {
	// Need to expand the extra stack.
	PTR		*newlist;

	mySize = mySize * 2 + 10;
	newlist = new PTR[mySize];
	if (myList)
	{
	    for (int i = 0; i < myEntries; i++)
		newlist[i] = myList[i+myStartPos];
	}
	delete [] myList;
	myList = newlist;
	// Take this chance to reset our leading edge.
	myStartPos = 0;
    }
    
    myList[myEntries+myStartPos] = item;

    myEntries++;
    return myEntries-1;
}

template <typename PTR>
void
PTRLIST<PTR>::append(const PTRLIST<PTR> &list)
{
    int		i;

    for (i = 0; i < list.entries(); i++)
	append(list(i));
}

template <typename PTR>
void
PTRLIST<PTR>::resize(int size)
{
    while (myEntries < size)
	append(PTR{});

    myEntries = size;
}

template <typename PTR>
void
PTRLIST<PTR>::setCapacity(int size)
{
    myEntries = 0;
    mySize = size;
    delete [] myList;
    myList = new PTR[mySize];
    myStartPos = 0;
}

template <typename PTR>
void
PTRLIST<PTR>::set(int idx, PTR item)
{
    J_ASSERT(idx < entries());
    J_ASSERT(idx >= 0);
    if (idx < 0)
	return;
    if (idx >= entries())
	return;

    myList[idx + myStartPos] = item;
}

template <typename PTR>
void
PTRLIST<PTR>::insert(int idx, PTR item)
{
    int		offset;

    // Deal with simple case.
    if (idx == entries())
    {
	append(item);
	return;
    }

    J_ASSERT(idx < entries());
    J_ASSERT(idx >= 0);

    // Duplicate end.
    append(top());
    // Now make room
    for (offset = entries()-1; offset > idx; offset--)
    {
	set(offset, (*this)(offset-1));
    }
    set(idx, item);
}

template <typename PTR>
const PTR &
PTRLIST<PTR>::operator()(int idx) const
{
    J_ASSERT(idx >= 0 && idx < myEntries);

    return myList[idx + myStartPos];
}

template <typename PTR>
PTR &
PTRLIST<PTR>::operator()(int idx)
{
    J_ASSERT(idx >= 0 && idx < myEntries);

    return myList[idx + myStartPos];
}

template <typename PTR>
int
PTRLIST<PTR>::entries() const
{
    return myEntries;
}

template <typename PTR>
void
PTRLIST<PTR>::clear()
{
    myEntries = 0;
    // Likewise, might as well reset our offset
    myStartPos = 0;
}

template <typename PTR>
void
PTRLIST<PTR>::constant(PTR val)
{
    int		i;

    for (i = 0; i < myEntries; i++)
	set(i, val);
}

template <typename PTR>
void
PTRLIST<PTR>::reverse()
{
    PTR		 tmp;
    int		 i, n, r;

    n = entries();
    for (i = 0; i < n / 2; i++)
    {
	// Reversed entry.
	r = n - 1 - i;
	tmp = (*this)(i);
	set(i, (*this)(r));
	set(r, tmp);
    }
}

template <typename PTR>
void
PTRLIST<PTR>::removePtr(PTR item)
{
    int		s, d, n;

    n = entries();
    s = d = 0;
    while (s < n)
    {
	if ((*this)(s) == item)
	{
	    // Don't copy this value.
	}
	else
	{
	    // Copy this value.
	    if (s != d)
		set(d, (*this)(s));
	    d++;
	}
	// Go to next value.
	s++;
    }
    // Final n is d.
    myEntries = d;
}

template <typename PTR>
void
PTRLIST<PTR>::removeAt(int idx)
{
    int		s, n;

    J_ASSERT(idx >= 0);
    J_ASSERT(idx < entries());

    n = entries();
    for (s = idx; s < n-1; s++)
    {
	set(s, (*this)(s+1));
    }
    myEntries--;
}

template <typename PTR>
PTR
PTRLIST<PTR>::pop()
{
    int		n;
    PTR		result;
    
    n = entries();
    if (!n)
    {
	// We should do an inplace new on result here so it
	// works with all types.
	new (&result) PTR();
	return result;
    }
    
    result = (*this)(n-1);
    myEntries = n-1;
    return result;
}

template <typename PTR>
void
PTRLIST<PTR>::swapEntries(int i1, int i2)
{
    if (i1 == i2)
	return;

    PTR		tmp;

    tmp = (*this)(i1);
    set(i1, (*this)(i2));
    set(i2, tmp);
}

template <typename PTR>
void
PTRLIST<PTR>::shuffle()
{
    if (entries() < 2)
	return;

    int		i, j;

    for (i = entries()-1; i > 0; i--)
    {
	// Want to swap with anything earlier, including self!
	j = rand_choice(i+1);

	swapEntries(i, j);
    }
}

template <typename PTR>
PTR
PTRLIST<PTR>::removeFirst()
{
    int		n;
    PTR		result;
    
    n = entries();
    if (!n)
    {
	// We should do an inplace new on result here so it
	// works with all types.
	new (&result) PTR();
	return result;
    }
    
    result = (*this)(0);

    // A little bit more efficient than moving.
    // We wait until the next resize to bother with clearing up
    // since at that resize we have to move anyways.
    myStartPos++;
    myEntries--;
    return result;
}

template <typename PTR>
int
PTRLIST<PTR>::find(PTR item) const
{
    int		i, n;

    n = entries();
    for (i = 0; i < n; i++)
    {
	if ((*this)(i) == item)
	    return i;
    }
    return -1;
}

template <typename PTR>
void
PTRLIST<PTR>::collapse()
{
    int			i, j, n;

    n = entries();

    for (i = j = 0; i < n; i++)
    {
	if ((*this)(i))
	{
	    // Valid...
	    if (i != j)
		set(j, (*this)(i));
	    j++;
	}
	else
	{
	    // Do not copy over, do not inc j.
	}
    }

    myEntries = j;
}

template <typename PTR>
void
PTRLIST<PTR>::stableSort()
{
    if (myEntries < 2)
	return;

    std::stable_sort(myList + myStartPos, myList + myStartPos + myEntries);
}

template <typename PTR>
template <typename OP>
void
PTRLIST<PTR>::stableSort(OP op)
{
    if (myEntries < 2)
	return;

    std::stable_sort(myList + myStartPos, myList + myStartPos + myEntries, op);
}

//
// Fixed lists, much like pointer list but has a max capacity so
// can be stack alloced.
//

template <typename PTR, int SIZE>
FIXEDLIST<PTR, SIZE>::FIXEDLIST()
{
    myEntries = 0;
}

template <typename PTR, int SIZE>
FIXEDLIST<PTR, SIZE>::FIXEDLIST(const FIXEDLIST<PTR, SIZE> &ref)
{
    *this = ref;
}

template <typename PTR, int SIZE>
FIXEDLIST<PTR, SIZE> &
FIXEDLIST<PTR, SIZE>::operator=(const FIXEDLIST<PTR, SIZE> &ref)
{
    int			i;

    if (this == &ref)
	return *this;

    if (!ref.myEntries)
    {
	// Quickly build empty list.
	myEntries = 0;
	return *this;
    }
    myEntries = ref.myEntries();
    memcpy(myList, ref.myList, sizeof(PTR) * myEntries);

    return *this;
}

template <typename PTR, int SIZE>
int
FIXEDLIST<PTR, SIZE>::append()
{
    return append(PTR{});
}

template <typename PTR, int SIZE>
int
FIXEDLIST<PTR, SIZE>::append(PTR item)
{
    if (myEntries >= SIZE)
    {
	J_ASSERT(!"Overflowed a fixed list!");
	return myEntries-1;
    }
    
    myList[myEntries] = item;

    myEntries++;
    return myEntries-1;
}

template <typename PTR, int SIZE>
void
FIXEDLIST<PTR, SIZE>::resize(int size)
{
    if (size > SIZE)
    {
	J_ASSERT(!"Overflowed a fixed list!");
	size = SIZE;
    }
    while (myEntries < size)
	append(PTR{});

    myEntries = size;
}

template <typename PTR, int SIZE>
void
FIXEDLIST<PTR, SIZE>::setCapacity(int size)
{
    J_ASSERT(size <= SIZE);
    myEntries = 0;
}

template <typename PTR, int SIZE>
void
FIXEDLIST<PTR, SIZE>::set(int idx, PTR item)
{
    J_ASSERT(idx < entries());
    J_ASSERT(idx >= 0);
    if (idx < 0)
	return;
    if (idx >= entries())
	return;

    myList[idx] = item;
}

template <typename PTR, int SIZE>
void
FIXEDLIST<PTR, SIZE>::insert(int idx, PTR item)
{
    int		offset;

    // Deal with simple case.
    if (idx == entries())
    {
	append(item);
	return;
    }

    J_ASSERT(idx < entries());
    J_ASSERT(idx >= 0);

    // Duplicate end.
    append(top());
    // Now make room
    for (offset = entries()-1; offset > idx; offset--)
    {
	set(offset, (*this)(offset-1));
    }
    set(idx, item);
}

template <typename PTR, int SIZE>
const PTR &
FIXEDLIST<PTR, SIZE>::operator()(int idx) const
{
    J_ASSERT(idx >= 0 && idx < myEntries);

    return myList[idx];
}

template <typename PTR, int SIZE>
PTR &
FIXEDLIST<PTR, SIZE>::operator()(int idx)
{
    J_ASSERT(idx >= 0 && idx < myEntries);

    return myList[idx];
}

template <typename PTR, int SIZE>
int
FIXEDLIST<PTR, SIZE>::entries() const
{
    return myEntries;
}

template <typename PTR, int SIZE>
void
FIXEDLIST<PTR, SIZE>::clear()
{
    myEntries = 0;
}

template <typename PTR, int SIZE>
void
FIXEDLIST<PTR, SIZE>::constant(PTR val)
{
    int		i;

    for (i = 0; i < myEntries; i++)
	set(i, val);
}

template <typename PTR, int SIZE>
void
FIXEDLIST<PTR, SIZE>::reverse()
{
    PTR		 tmp;
    int		 i, n, r;

    n = entries();
    for (i = 0; i < n / 2; i++)
    {
	// Reversed entry.
	r = n - 1 - i;
	tmp = (*this)(i);
	set(i, (*this)(r));
	set(r, tmp);
    }
}

template <typename PTR, int SIZE>
void
FIXEDLIST<PTR, SIZE>::removePtr(PTR item)
{
    int		s, d, n;

    n = entries();
    s = d = 0;
    while (s < n)
    {
	if ((*this)(s) == item)
	{
	    // Don't copy this value.
	}
	else
	{
	    // Copy this value.
	    if (s != d)
		set(d, (*this)(s));
	    d++;
	}
	// Go to next value.
	s++;
    }
    // Final n is d.
    myEntries = d;
}

template <typename PTR, int SIZE>
void
FIXEDLIST<PTR, SIZE>::removeAt(int idx)
{
    int		s, n;

    J_ASSERT(idx >= 0);
    J_ASSERT(idx < entries());

    n = entries();
    for (s = idx; s < n-1; s++)
    {
	set(s, (*this)(s+1));
    }
    myEntries--;
}

template <typename PTR, int SIZE>
PTR
FIXEDLIST<PTR, SIZE>::pop()
{
    int		n;
    PTR		result;
    
    n = entries();
    if (!n)
    {
	// We should do an inplace new on result here so it
	// works with all types.
	result = PTR{};
	return result;
    }
    
    result = (*this)(n-1);
    myEntries = n-1;
    return result;
}

template <typename PTR, int SIZE>
void
FIXEDLIST<PTR, SIZE>::swapEntries(int i1, int i2)
{
    if (i1 == i2)
	return;

    PTR		tmp;

    tmp = (*this)(i1);
    set(i1, (*this)(i2));
    set(i2, tmp);
}

template <typename PTR, int SIZE>
void
FIXEDLIST<PTR, SIZE>::shuffle()
{
    if (entries() < 2)
	return;

    int		i, j;

    for (i = entries()-1; i > 0; i--)
    {
	// Want to swap with anything earlier, including self!
	j = rand_choice(i+1);

	swapEntries(i, j);
    }
}


template <typename PTR, int SIZE>
int
FIXEDLIST<PTR, SIZE>::find(PTR item) const
{
    int		i, n;

    n = entries();
    for (i = 0; i < n; i++)
    {
	if ((*this)(i) == item)
	    return i;
    }
    return -1;
}

template <typename PTR, int SIZE>
void
FIXEDLIST<PTR, SIZE>::collapse()
{
    int			i, j, n;

    n = entries();

    for (i = j = 0; i < n; i++)
    {
	if ((*this)(i))
	{
	    // Valid...
	    if (i != j)
		set(j, (*this)(i));
	    j++;
	}
	else
	{
	    // Do not copy over, do not inc j.
	}
    }

    myEntries = j;
}

template <typename PTR, int SIZE>
void
FIXEDLIST<PTR, SIZE>::stableSort()
{
    if (myEntries < 2)
	return;

    std::stable_sort(myList, myList + myEntries);
}

template <typename PTR, int SIZE>
template <typename OP>
void
FIXEDLIST<PTR, SIZE>::stableSort(OP op)
{
    if (myEntries < 2)
	return;

    std::stable_sort(myList, myList + myEntries, op);
}
