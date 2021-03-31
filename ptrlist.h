/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        ptrlist.h ( Letter Hunt Library, C++ )
 *
 * COMMENTS:
 *	Implements a simple list of pointers.
 */

#ifndef __ptrlist_h__
#define __ptrlist_h__

#include <iterator>
#include "mygba.h"

template <typename PTR>
class PTRLIST
{
public:
    PTRLIST();
    ~PTRLIST();

    // No copy constructors as you should be passing by reference to build!
    // That is a noble sentiment, but we live in a corrupt world
    PTRLIST(const PTRLIST<PTR> &ref);
    PTRLIST<PTR> &operator=(const PTRLIST<PTR> &ref);

    int		 	 append(PTR item);
    int		 	 append();
    void		 append(const PTRLIST<PTR> &list);

    // Reverses the order of the stack.
    void		 reverse();

    // Empties the stack
    void		 clear();

    // Sets array to constant value
    void		 constant(PTR ptr);

    // Sets entries to the given size.
    void		 resize(int size);

    // Resizes only if it is a grow
    void		 resizeIfNeeded(int newsize)
			 { if (newsize > entries()) resize(newsize); }

    // Allocates copacity
    // Only valid on empty list!
    void		 setCapacity(int size);
    int			 capacity() const { return mySize; }

    const PTR		 &operator()(int idx) const;

    // I've been avoiding adding this for a long time for what
    // I believed was a very good reason.  But I've given up.
    PTR			 &operator()(int idx);

    // Treats the array as padded with 0s after the end.
    // Note does not resize like indexAnywhere, as we'd then
    // have different side effects for const and non const
    // which will be very confusing.
    PTR			 safeIndexAnywhere(int idx) const
    { 
	if (idx < entries()) return (*this)(idx);

	PTR result;
	new (&result) PTR();
	return result;
    }

    // Will grow the array to contain the given index.
    // Invalid if negative.
    PTR			&indexAnywhere(int idx) 
    { 
	resizeIfNeeded(idx+1);
	return (*this)(idx);
    }


    int			 entries() const;

    void		 set(int idx, PTR item);

    void		 insert(int idx, PTR item);

    // Removes all instances of "item" from
    // this list.
    void		 removePtr(PTR item);

    // Removes a given index
    void		 removeAt(int idx);

    // Returns the last item on this list and decreases
    // the length.
    PTR			 pop();

    // Swaps two entries
    void		 swapEntries(int i1, int i2);

    // Shuffles.
    void		 shuffle();

    // Returns the last element.
    // The index allows you to specify N elements from the end, where
    // 0 is the last element and 1 second last.
    PTR			 top(int idx = 0) const { return (*this)(entries()-1-idx); }

    // Returns the first item from this list and
    // removes it.
    PTR			 removeFirst();

    // Finds the given item, returns -1 if fails.
    int			 find(PTR item) const;

    // Deletes all zero entries.
    void		 collapse();

    // I should not have to explain why you should never call this.
    PTR			*rawptr(int idx=0) const { return &myList[idx]; }

    void		 stableSort();
    template <typename OP>
    void		 stableSort(OP op);

    // Iterators (So I can join the modern world and use range
    // based for loops!)
    template <typename IT>
    class ITERATOR_T : std::iterator<std::random_access_iterator_tag, IT, int>
    {
    public:
	using pointer = IT *;
	using reference = IT &;

	ITERATOR_T()
	{
	    myCur = nullptr;
	    myEnd = nullptr;
	}

	pointer	operator->() const
	{ return myCur; }

	reference operator*() const
	{ return *myCur; }

	// Pre-inc (only supported)
	ITERATOR_T &operator++()
	{
	    myCur++;
	    return *this;
	}

	// I long for spaceship operators!
	template <typename ITR>
	bool operator==(const ITERATOR_T<ITR> &it) const
	{ return myCur == it.myCur; }
	template <typename ITR>
	bool operator!=(const ITERATOR_T<ITR> &it) const
	{ return myCur != it.myCur; }
	template <typename ITR>
	bool operator<(const ITERATOR_T<ITR> &it) const
	{ return myCur < it.myCur; }
	template <typename ITR>
	bool operator>(const ITERATOR_T<ITR> &it) const
	{ return myCur > it.myCur; }
	template <typename ITR>
	bool operator<=(const ITERATOR_T<ITR> &it) const
	{ return myCur <= it.myCur; }
	template <typename ITR>
	bool operator>=(const ITERATOR_T<ITR> &it) const
	{ return myCur >= it.myCur; }

    protected:
	friend class PTRLIST<PTR>;

	ITERATOR_T(pointer cur, pointer end)
	{
	    myCur = cur;
	    myEnd = end;
	}

	pointer		myCur, myEnd;
    };

    using ITERATOR = ITERATOR_T<PTR>;
    using CITERATOR = ITERATOR_T<const PTR>;

    ITERATOR		 begin()
    { 
	return ITERATOR(myList + myStartPos,
			myList + myStartPos + myEntries); 
    }
    ITERATOR		 end()
    { 
	return ITERATOR(myList + myStartPos + myEntries,
			myList + myStartPos + myEntries); 
    }
    CITERATOR		 begin() const
    { 
	return CITERATOR(myList + myStartPos,
			myList + myStartPos + myEntries); 
    }
    CITERATOR		 end() const
    { 
	return CITERATOR(myList + myStartPos + myEntries,
			myList + myStartPos + myEntries); 
    }


private:
    PTR			 *myList;
    int			  myStartPos;
    int			  myEntries;
    int			  mySize;
};

template <typename PTR, int SIZE>
class FIXEDLIST
{
public:
    FIXEDLIST();

    // Don't use the default copy as we can know to only copy
    // the allocated entries.
    FIXEDLIST(const FIXEDLIST<PTR, SIZE> &ref);
    FIXEDLIST<PTR, SIZE> &operator=(const FIXEDLIST<PTR, SIZE> &ref);

    int		 	 append(PTR item);
    int		 	 append();
    void		 append(const FIXEDLIST<PTR, SIZE> &list);

    // Reverses the order of the stack.
    void		 reverse();

    // Empties the stack
    void		 clear();

    // Sets array to constant value
    void		 constant(PTR ptr);

    // Sets entries to the given size.
    void		 resize(int size);

    // Resizes only if it is a grow
    void		 resizeIfNeeded(int newsize)
			 { if (newsize > entries()) resize(newsize); }

    // Allocates copacity
    // Only valid on empty list!
    void		 setCapacity(int size);
    int			 capacity() const { return SIZE; }

    const PTR		 &operator()(int idx) const;

    // I've been avoiding adding this for a long time for what
    // I believed was a very good reason.  But I've given up.
    PTR			 &operator()(int idx);

    // Treats the array as padded with 0s after the end.
    // Note does not resize like indexAnywhere, as we'd then
    // have different side effects for const and non const
    // which will be very confusing.
    PTR			 safeIndexAnywhere(int idx) const
    { 
	if (idx < entries()) return (*this)(idx);

	PTR result;
	new (&result) PTR();
	return result;
    }

    // Will grow the array to contain the given index.
    // Invalid if negative.
    PTR			&indexAnywhere(int idx) 
    { 
	J_ASSERT(idx >= 0 && idx < SIZE);
	resizeIfNeeded(idx+1);
	return (*this)(idx);
    }


    int			 entries() const;

    void		 set(int idx, PTR item);

    void		 insert(int idx, PTR item);

    // Removes all instances of "item" from
    // this list.
    void		 removePtr(PTR item);

    // Removes a given index
    void		 removeAt(int idx);

    // Returns the last item on this list and decreases
    // the length.
    PTR			 pop();

    // Swaps two entries
    void		 swapEntries(int i1, int i2);

    // Shuffles.
    void		 shuffle();

    // Returns the last element.
    // The index allows you to specify N elements from the end, where
    // 0 is the last element and 1 second last.
    PTR			 top(int idx = 0) const { return (*this)(entries()-1-idx); }

    // Finds the given item, returns -1 if fails.
    int			 find(PTR item) const;

    // Deletes all zero entries.
    void		 collapse();

    // I should not have to explain why you should never call this.
    PTR			*rawptr(int idx=0) const { return &myList[idx]; }

    void		 stableSort();
    template <typename OP>
    void		 stableSort(OP op);

    // Iterators (So I can join the modern world and use range
    // based for loops!)
    template <typename IT>
    class ITERATOR_T : std::iterator<std::random_access_iterator_tag, IT, int>
    {
    public:
	using pointer = IT *;
	using reference = IT &;

	ITERATOR_T()
	{
	    myCur = nullptr;
	    myEnd = nullptr;
	}

	pointer	operator->() const
	{ return myCur; }

	reference operator*() const
	{ return *myCur; }

	// Pre-inc (only supported)
	ITERATOR_T &operator++()
	{
	    myCur++;
	    return *this;
	}

	// I long for spaceship operators!
	template <typename ITR>
	bool operator==(const ITERATOR_T<ITR> &it) const
	{ return myCur == it.myCur; }
	template <typename ITR>
	bool operator!=(const ITERATOR_T<ITR> &it) const
	{ return myCur != it.myCur; }
	template <typename ITR>
	bool operator<(const ITERATOR_T<ITR> &it) const
	{ return myCur < it.myCur; }
	template <typename ITR>
	bool operator>(const ITERATOR_T<ITR> &it) const
	{ return myCur > it.myCur; }
	template <typename ITR>
	bool operator<=(const ITERATOR_T<ITR> &it) const
	{ return myCur <= it.myCur; }
	template <typename ITR>
	bool operator>=(const ITERATOR_T<ITR> &it) const
	{ return myCur >= it.myCur; }

    protected:
	friend class PTRLIST<PTR>;

	ITERATOR_T(pointer cur, pointer end)
	{
	    myCur = cur;
	    myEnd = end;
	}

	pointer		myCur, myEnd;
    };

    using ITERATOR = ITERATOR_T<PTR>;
    using CITERATOR = ITERATOR_T<const PTR>;

    ITERATOR		 begin()
    { 
	return ITERATOR(myList,
			myList + myEntries); 
    }
    ITERATOR		 end()
    { 
	return ITERATOR(myList + myEntries,
			myList + myEntries); 
    }
    CITERATOR		 begin() const
    { 
	return CITERATOR(myList,
			myList + myEntries); 
    }
    CITERATOR		 end() const
    { 
	return CITERATOR(myList + myEntries,
			myList + myEntries); 
    }


private:
    PTR			  myList[SIZE];
    int			  myEntries;
};

// For crappy platforms:
#include "ptrlist.cpp"

#endif

