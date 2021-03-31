/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        level.h ( Rogue Impact, C++ )
 *
 * COMMENTS:
 * 	Mostly a poor-man's namespace class to centralize level
 * 	related code as everything has a level in Genshin Impact.
 */

#ifndef __level__
#define __level__

class LEVEL
{
public:
    static int	levelFromExp(int exp);		// What level this exp is.
    static int	expFromLevel(int level);	// Exp required for this level

    // Initially all the boosts are the same, but this allows some
    // semantics later.
    static int	boostHP(int basehp, int level);
    static int	boostResist(int baseresist, int level);
    static int	boostDamage(int basedamage, int level);
    static int	boostDuration(int baseduration, int level);
    static int	boostHeal(int baseduration, int level);

    static constexpr int MAX_LEVEL = 100;
};

#endif

