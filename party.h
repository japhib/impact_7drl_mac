/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        party.h ( Rogue Impact, C++ )
 *
 * COMMENTS:
 */

#ifndef __party__
#define __party__

class MAP;
class MOB;

#include <iostream>

#define MAX_PARTY 4

class PARTY
{
public:
    PARTY();

    void		 save(std::ostream &os) const;
    void		 load(std::istream &is);

    void		 setMap(MAP *map);

    // Provides currently active mob.
    MOB			*active() const;

    // Removes the active player from the party, returns the new
    // active player, if any.
    MOB			*removeActive();

    // Removes a given player, if they are in the party.  True
    // if done.
    bool		 removeMob(MOB *mob);

    // Returns mob now active.
    MOB			*makeActive(int idx);

    // Gives the nth party member
    MOB			*member(int idx) const;

    // Swaps two party member from their indices.  No op if out of range
    // If active is one of these, the active is also changed.
    void		 swap(int a, int b);

    // Returns mob ejected.
    MOB			*replace(int idx, MOB *newchar);

protected:
    MAP			*myMap;
    int			 myActiveIdx;
    int			 myUID[MAX_PARTY];

};

#endif
