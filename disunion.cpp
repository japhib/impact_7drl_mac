/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	1812 Development
 *
 * NAME:        disunion.h ( 1812, C++ )
 *
 * COMMENTS:
 * 	Supports disjoint union operations of integer classes.
 */

#include "disunion.h"

DISUNION::DISUNION(int size)
{
    myClasses.resize(size);
    for (int i = 0; i < size; i++)
	myClasses(i) = i;
    myNumClasses = size;
    myIsDisunion = true;
    mySizeValid = false;
}

int
DISUNION::findClass(int idx)
{
    if (idx < 0)
	return idx;

    if (!myIsDisunion)
	return myClasses(idx);

    int		c = myClasses(idx);

    if (c < 0)
	return c;		// terminus

    if (c == idx)
	return c;

    c = findClass(c);
    // Write back result.
    myClasses(idx) = c;

    return c;
}

void
DISUNION::unionClass(int aidx, int bidx)
{
    J_ASSERT(myIsDisunion);		// Cannot use after collapse!

    mySizeValid = false;
    int ca = findClass(aidx);
    int cb = findClass(bidx);
    if (ca == cb)
	return;

    if (ca < cb)
	myClasses(cb) = ca;
    else
	myClasses(ca) = cb;

    // Re-collapse after the union:
    findClass(aidx);
    findClass(bidx);
    J_ASSERT(findClass(aidx) == findClass(bidx));
}

int
DISUNION::classSize(int idx)
{
    if (!mySizeValid)
    {
	mySizes.resize(myNumClasses);
	mySizes.constant(0);
	for (int i = 0; i < myClasses.entries(); i++)
	{
	    int c = findClass(i);
	    if (c >= 0)
		mySizes(i) += 1;
	}
	mySizeValid = true;
    }

    int c = findClass(idx);
    if (c < 0)
	return 0;
    return mySizes(c);
}

void
DISUNION::collapse()
{
    if (!myIsDisunion)
    {
	// Already collaped!
	return;
    }
    mySizeValid = false;
    // First ensure all are normalized.
    for (int i = 0; i < myClasses.entries(); i++)
    {
	findClass(i);
    }

    // Now build our list of unique classes.
    PTRLIST<int>	uniqueclass;
    PTRLIST<int>	finalnames;
    finalnames.resize(myClasses.entries());
    for (int i = 0; i < myClasses.entries(); i++)
    {
	int c = findClass(i);
	if (c == i)
	{
	    // This is a root class.
	    finalnames(i) = uniqueclass.entries();
	    uniqueclass.append(i);
	}
    }
    // Our re-numbering is simply to copy from the final names
    // list.
    for (int i = 0; i < myClasses.entries(); i++)
    {
	int c = findClass(i);
	if (c < 0)
	    finalnames(i) = c;
	else
	    finalnames(i) = finalnames(c);
    }

    // finalnames now becomes our class names.
    myClasses = finalnames;
    myIsDisunion = false;

    myNumClasses = uniqueclass.entries();
}
