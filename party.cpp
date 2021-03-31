/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        party.cpp ( Rogue Impact Library, C++ )
 *
 * COMMENTS:
 */

#include "mygba.h"
#include "party.h"
#include "map.h"
#include "mob.h"
#include "item.h"

#include <stdio.h>

#include <iostream>

//
// PARTY operators
//

PARTY::PARTY()
{
    myMap = nullptr;
    myActiveIdx = 0;
    for (int i = 0; i < MAX_PARTY; i++)
	myUID[i] = INVALID_UID;
}

void
PARTY::save(std::ostream &os) const
{
    int		val;

    val = myActiveIdx;
    os.write((const char *) &val, sizeof(int));
    for (int i = 0; i < MAX_PARTY; i++)
    {
	val = myUID[i];
	os.write((const char *) &val, sizeof(int));
    }
}

void
PARTY::load(std::istream &is)
{
    int		val;

    is.read((char *)&val, sizeof(int));
    myActiveIdx = val;
    for (int i = 0; i < MAX_PARTY; i++)
    {
	is.read((char *)&val, sizeof(int));
	myUID[i] = val;
    }
}

void
PARTY::setMap(MAP *map)
{
    myMap = map;
}

MOB *
PARTY::active() const
{
    int uid = myUID[myActiveIdx];
    if (uid == INVALID_UID)
	return nullptr;
    J_ASSERT(myMap);
    if (!myMap)
	return nullptr;
    return myMap->findMob(uid);
}

bool
PARTY::removeMob(MOB *mob)
{
    bool	removed = false;
    for (int i = 0; i < MAX_PARTY; i++)
    {
	if (member(i) == mob)
	{
	    // It is not expected to remove the leader this way,
	    // more care is needed then!
	    J_ASSERT(i != myActiveIdx);
	    replace(i, nullptr);
	    removed = true;
	    // Keep searching in case we somehow got duplicates.
	}
    }

    return removed;
}

MOB *
PARTY::removeActive()
{
    // If no active, trivial
    if (!active())
	return nullptr;

    MOB		*oldactive = active();
    int		 olduid = oldactive->getUID();

    // Must be at least one active, so this will succeed.
    int idx = myActiveIdx;
    do
    {
	idx++;
	if (idx >= MAX_PARTY)
	    idx = 0;

	// If this is a stale mob, remove from the list.  Should
	// have been culled on kill but it is important to at least
	// catch it here or we'll make a dead mob active, so trigger
	// TPK detection with other slots alive.
	if (myUID[idx] != INVALID_UID &&
	    myUID[idx] != olduid)
	{
	    if (!member(idx))
		myUID[idx] = INVALID_UID;
	}
    } while (myUID[idx] == INVALID_UID);

    // Remove the active.
    myUID[myActiveIdx] = INVALID_UID;

    // If we wrapped to ourself, fail.  But this is the
    // same as returning active after updating the active index
    // as we removed that slot.
    myActiveIdx = idx;

    MOB		*newactive = active();
    if (oldactive && newactive && oldactive != newactive)
    {
	newactive->stealNonEquipped(oldactive);
    }

    return active();
}

MOB *
PARTY::makeActive(int idx)
{
    if (idx < 0 || idx >= MAX_PARTY)
    {
	J_ASSERT(false);
	return active();
    }
    // Can't make a dead character active.
    if (myUID[idx] == INVALID_UID)
    {
	return active();
    }
    MOB		*oldactive = active();
    myActiveIdx = idx;
    MOB		*newactive = active();
    if (oldactive && newactive && oldactive != newactive)
    {
	newactive->stealNonEquipped(oldactive);
    }
    return active();
}

MOB *
PARTY::member(int idx) const
{
    if (idx < 0 || idx >= MAX_PARTY)
    {
	J_ASSERT(false);
	return nullptr;
    }

    int uid = myUID[idx];
    if (uid == INVALID_UID)
	return nullptr;
    J_ASSERT(myMap);
    if (!myMap)
	return nullptr;
    return myMap->findMob(uid);
}

void
PARTY::swap(int a, int b)
{
    if (a < 0 || a >= MAX_PARTY)
    {
	J_ASSERT(false);
	return;
    }
    if (b < 0 || b >= MAX_PARTY)
    {
	J_ASSERT(false);
	return;
    }

    int tmp = myUID[a];
    myUID[a] = myUID[b];
    myUID[b] = tmp;

    if (myActiveIdx == a)
	myActiveIdx = b;
    else if (myActiveIdx == b)
	myActiveIdx = a;
}

MOB *
PARTY::replace(int idx, MOB *newchar)
{
    if (idx < 0 || idx >= MAX_PARTY)
    {
	J_ASSERT(false);
	return nullptr;
    }

    MOB *old = member(idx);
    int uid = INVALID_UID;
    if (newchar)
	uid = newchar->getUID();
    myUID[idx] = uid;

    if (idx == myActiveIdx)
    {
	// Steal the items.
	if (newchar && old)
	    newchar->stealNonEquipped(old);
    }
    else
    {
	// The active player should steal the new player's stuff
	if (active() && newchar)
	{
	    active()->stealNonEquipped(newchar);
	}
    }
    return old;
}

