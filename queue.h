/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        queue.h ( Letter Hunt Library, C++ )
 *
 * COMMENTS:
 *	Implements a simple queue.
 *	Is threadsafe.  Has the ability to block until an item
 * 	is available.
 */


#ifndef __queue_h__
#define __queue_h__

#include "ptrlist.h"
#include "thread.h"

template <typename PTR>
class QUEUE
{
public:
    QUEUE() {}
    ~QUEUE() {}

    // Because we are multithreaded, this is not meant to remain valid
    // outside of a lock.
    bool		isEmpty() const
    {
	return myList.entries() == 0;
    }

    int			entries() const
    {
	return myList.entries();
    }

    // Removes item, returns false if failed because queue is empty.
    bool		remove(PTR &item)
    {
	AUTOCONDLOCK	l(myLock);

	return lockedRemove(item);
    }

    // Blocks till an element is ready.
    PTR			waitAndRemove()
    {
	AUTOCONDLOCK	l(myLock);
	PTR		result;

	while (!lockedRemove(result))
	{
	    myCond.wait(l);
	}
	return result;
    }
    
    // Empties the queue.
    void		clear()
    {
	AUTOCONDLOCK	l(myLock);
	myList.clear();
	// Don't trigger!
    }

    void		append(const PTR &item)
    {
	AUTOCONDLOCK	l(myLock);
	myList.append(item);
	myCond.trigger();
    }

    bool		appendIfEmpty(const PTR &item)
    {
	AUTOCONDLOCK	l(myLock);
	if (isEmpty())
	{
	    myList.append(item);
	    myCond.trigger();
	    return true;
	}
	return false;
    }

private:
    // Copying locks scares me.
    QUEUE(const QUEUE<PTR> &ref) {}
    QUEUE<PTR> &operator=(const QUEUE<PTR> &ref) {}

    /// Requires we already have the lock since std::condition_value sucks.
    bool		lockedRemove(PTR &item)
    {
	if (isEmpty())
	    return false;
	item = myList.removeFirst();
	return true;
    }


    PTRLIST<PTR>	myList;
    CONDLOCK		myLock;
    CONDITION		myCond;
};

#endif
