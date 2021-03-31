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

#pragma once

#include "ptrlist.h"

class DISUNION
{
public:
		DISUNION(int size);
    
    // Connect the two sets that a & b belong to.
    void 	unionClass(int aidx, int bidx);

    // Find the representative class of idx.
    int  	findClass(int idx);

    // Ensures all classes are depth 1 and are numbered 0..numclass-1.
    // After this it isn't a disjoint union as terminus are no longer
    // reflexive!!
    void	collapse();

    // Maximum class value.  Will be complete if collapsed.
    int		numClass() const;

    // Size of class that idx belongs to.
    int		classSize(int idx);

protected:
    PTRLIST<int>		myClasses;
    PTRLIST<int>		mySizes;	// Size of each class value.
    int				myNumClasses;	// Maximum class value.
    bool			myIsDisunion;
    bool			mySizeValid;
};

