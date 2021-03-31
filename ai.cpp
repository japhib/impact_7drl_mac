/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        ai.cpp ( Save Scummer Library, C++ )
 *
 * COMMENTS:
 */

#include <libtcod.hpp>
#undef CLAMP

#include "mob.h"

#include "map.h"
#include "msg.h"
#include "text.h"
#include "item.h"
#include "config.h"

bool
MOB::aiAvoidDirection(int dx, int dy) const
{
    // Kobolds found it superstitious to step on corpses.
    // *Band mobs are not so choosy.
#if 0
    POS	qp = pos().delta(dx, dy);
    ITEMLIST	items;

    if (qp.allItems(items))
    {
	for (int i = 0; i < items.entries(); i++)
	{
	    if (items(i)->getDefinition() == ITEM_CORPSE)
		return true;
	}
    }
#endif
    return false;
}

bool
MOB::aiForcedAction()
{
    // Check for the phase...
    PHASE_NAMES		phase;

    phase = map()->getPhase();

    if (!alive())
	return false;

    myYellHystersis--;
    if (myYellHystersis < 0)
	myYellHystersis = 0;

    myBoredom++;
    if (myBoredom > 1000)
	myBoredom = 1000;

    // On normal & slow phases, items get heartbeat.
    if (phase == PHASE_NORMAL || phase == PHASE_SLOW)
    {
	myRangeTimeout--;
	if (myRangeTimeout < 0)
	    myRangeTimeout = 0;

	mySpellTimeout--;
	if (mySpellTimeout < 0)
	    mySpellTimeout = 0;

	// We don't want magic rings in other people's pockets
	// to expire.  But we do want flags to be processed!
	{
	    int i;
	    for (i = myInventory.entries(); i --> 0;)
	    {
		if (!isAvatar())
		{
		    if (!myInventory(i)->isFlag())
			continue;
		}
		if (myInventory(i)->runHeartbeat())
		{
		    ITEM		*item = myInventory(i);

		    removeItem(item);
		    delete item;
		}
	    }
	}
	// We carry all the food we need with us!
	if (inParty())
	{
	    // Eat food.
	    // No actual malus or benefit, but acts as a timeout
	    // for using food to heal.
	    eatFood(1);
	}

	if (0) //isAvatar())
	{
	    // Eat food.
	    eatFood(1);

	    // If you are bruised, you can still heal.
	    if (getFood() && getHP() < getMaxHP())
	    {
		if (!rand_choice(4))
		{
		    gainHP(1);
		    eatFood(1);
		}
	    }
	    if (getFood() && getMP() < getMaxMP())
	    {
		gainMP(1);
		eatFood(1);
	    }
	    if (!getFood())
		stopWaiting("%S <be> too hungry to rest!");
	}

	// Regenerate if we are capable of it
	if (defn().isregenerate || hasItem(ITEM_REGEN))
	{
	    gainHP(1);
	}

	// Check if we are at full stats
	if (getHP() == getMaxHP() && getMP() == getMaxMP())
	{
	    // We never stop waiting!
	    // stopWaiting("%S <be> at full health, so <stop> resting.");
	}

	// Apply damage if inside.
	if (isSwallowed())
	{
	    formatAndReport("%S <be> digested!");

	    // Make it a force action if it kills us.
	    // This is a bit weird where damage depends on your level,
	    // not the attackers... Ah vell, perhaps I'lll retcon it
	    // that dissolving is a percentage attack?
	    if (applyDamage(0, rand_range(1, 10), ELEMENT_ACID, ATTACKSTYLE_INTERNAL))
		return true;
	}

	// Apply poison damage
	if (hasItem(ITEM_POISON))
	{
	    if (rand_chance(50))
	    {
		formatAndReport("%S <be> poisoned!", this);
		// AGain, like digestion, this scales to your level
		if (applyDamage(0, rand_range(1, 5), ELEMENT_POISON, ATTACKSTYLE_INTERNAL))
		    return true;
	    }
	}
    }


    // People waiting twiddle their thumbs!
    if (isWaiting())
    {
	// Effectively get a free skip next turn!
	mySkipNextTurn = false;
	return true;
    }
    else if (isMeditating())
    {
	// People meditating move very fast.
	// Nah, we want them the same speed!  Slow thinkers think slow!
    }
    switch (phase)
    {
	case PHASE_FAST:
	    // Not fast, no phase
	    if (defn().isfast || hasItem(ITEM_HASTED))
		break;
	    return true;

	case PHASE_QUICK:
	    // Not quick, no phase!
	    if (hasItem(ITEM_QUICKBOOST))
		break;
	    return true;

	case PHASE_SLOW:
	    if (defn().isslow)
		return true;
	    if (hasItem(ITEM_SLOW))
		return true;
	    break;

	case PHASE_NORMAL:
	    break;
    }
    if (mySkipNextTurn)
    {
	mySkipNextTurn = false;
	return true;
    }
    return false;
}

bool
MOB::aiDoAI()
{
    AI_NAMES		ai = getAI();

    return aiDoAIType(ai);
}

void
MOB::aiTrySpeaking()
{
    if (isAvatar())
	return;

    MOB		*avatar = getAvatar();
    if (!avatar || !avatar->alive())
	return;

    if (!pos().isFOV())
	return;

    if (!isFriends(avatar))
	return;

    // This is based on the turn time, however, which is not
    // very useful when we accelerate time - we get spam galore!
    // Instead we throw a 10 second real window on it.
    static int	lastspeak = -10;
    static int	lastspeakms = -10;

    if ((lastspeak < map()->getTime() - 15) &&
	(TCOD_sys_elapsed_milli() - lastspeakms > 10000) &&
	rand_chance(10))
    {
	const char		*speach = 0;
	bool			 emote = false;

	if (hasItem(ITEM_PLAGUE))
	{
	    speach = "*cough*";
	    emote = true;
	}

	if (speach)
	{
	    lastspeak = map()->getTime();
	    lastspeakms = TCOD_sys_elapsed_milli();
	    if (emote)
		doEmote(speach);
	    else
		doShout(speach);
	}
    }

}

bool
MOB::aiDoAIType(AI_NAMES aitype)
{
    // Rebuild our home location.
    if (!myHome.valid())
    {
	myHome = pos();
    }

    // If we are in limbo do nothing.
    if (!pos().valid())
	return true;

    if (myHeardYell[YELL_MURDERER] && !mySawMurder)
    {
	// Hearsay!
	mySawMurder = true;
	giftItem(ITEM_ENRAGED);
    }

    if (myFleeCount > 0)
    {
	myFleeCount--;
	if (!isAvatar() && aiFleeFromAvatar())
	    return true;
    }

    aiTrySpeaking();

    if (hasItem(ITEM_ENRAGED))
	aitype = AI_CHARGE;

    if (aitype == AI_PEACEFUL && myAngryWithAvatar)
	aitype = AI_CHARGE;

    switch (aitype)
    {
	case AI_PEACEFUL:
	    return aiRandomWalk();

	case AI_NONE:
	    // Just stay put!
	    return true;

	case AI_STAYHOME:
	    // If we are at home stay put.
	    if (pos() == myHome)
		return true;

	    // Otherwise, go home.
	    // FALL THROUGH

	case AI_HOME:
	    // Move towards our home square.
	    int		dist;

	    dist = pos().dist(myHome);

	    // If we can shoot the avatar or charge him, do so.
	    if (!defn().isfriendly)
	    {
		if (aiRangeAttack())
		    return true;
		if (aiCharge(getAvatar(), AI_CHARGE))
		    return true;
	    }

	    // A good chance to just stay put.
	    if (rand_chance(70))
	    {
		return actionBump(0, 0);
	    }

	    if (rand_choice(dist))
	    {
		// Try to home.
		if (aiPathFindTo(myHome))
		    return true;
	    }
	    // Default to wandering
	    return aiRandomWalk();

	case AI_ORTHO:
	    if (aiRangeAttack())
		return true;
	    if (aiCharge(getAvatar(), AI_CHARGE, true))
		return true;
	    // Default to wandering
	    return aiRandomWalk(true);

	case AI_CHARGE:
	    if (aiRangeAttack())
		return true;
	    if (aiCharge(getAvatar(), AI_CHARGE))
		return true;
	    // Default to wandering
	    return aiRandomWalk();

	case AI_PATHTO:
	    if (aiRangeAttack())
		return true;
	    if (aiKillPathTo(getAvatar()))
		return true;
		    
	    // Default to wandering
	    return aiRandomWalk();

	case AI_FLANK:
	    if (aiRangeAttack())
		return true;
	    if (aiCharge(getAvatar(), AI_FLANK))
		return true;
	    // Default to wandering
	    return aiRandomWalk();

	case AI_RANGECOWARD:
	    // If we can see avatar, shoot at them!
	    // We keep ourselves at range if possible, unless
	    // retreat is blocked, in which case we will melee.
	    if (pos().isFOV())
	    {
		int		 dist, dx, dy;

		if (aiAcquireAvatar())
		{
		    pos().dirTo(myTarget, dx, dy);
		    dist = pos().dist(myTarget);

		    if (dist == 1)
		    {
			// We are in melee range.  Try to flee.
			if (aiFleeFrom(myTarget))
			    return true;

			// Failed to flee.  Kill!
			return actionMelee(dx, dy);
		    }

		    // Try ranged attack.
		    if (aiRangeAttack())
			return true;

		    // Failed range attack.  If outside of range, charge.
		    if (dist > getRangedRange())
		    {
			if (aiCharge(getAvatar(), AI_FLANK))
			    return true;
		    }

		    // Otherwise, wander within the range hoping to line up.
		    // Trying to double think the human is likely pointless
		    // as they'll be trying to avoid lining up to.
		    return aiRandomWalk();
		}
	    }
		
	    if (aiCharge(getAvatar(), AI_FLANK))
		return true;

	    // Default to wandering
	    return aiRandomWalk();

	    
	case AI_COWARD:
	    if (aiRangeAttack())
		return true;

	    if (aiFleeFromAvatar())
		return true;
	    // Failed to flee, cornered, so charge!
	    if (aiCharge(getAvatar(), AI_CHARGE))
		return true;

	    return aiRandomWalk();

	case AI_MOUSE:
	    return aiDoMouse();

	case AI_ORC:
	    return aiDoOrc();

	case AI_RAT:
	    return aiDoRat();

	case AI_STRAIGHTLINE:
	    return aiStraightLine();
    }

    return false;
}

bool
MOB::aiBattlePrep()
{
    for (int i = 0; i < myInventory.entries(); i++)
    {
	ITEM		*item = myInventory(i);

	if (item->isPotion())
	{
	    return actionQuaff(item);
	}
    }
    return false;
}

bool
MOB::aiDestroySomethingGood(MOB *denyee)
{
    ITEM		*wand = lookupWand();
    ITEM		*enemywand = denyee->lookupWand();

    if (wand)
    {
	// Only bother wasting our turn destroying it if the
	// avatar hasn't yet acquired one.
	if (aiLikeMoreWand(wand, enemywand) == wand)
	{
	    return actionBreak(wand);
	}
    }
    // Deal with any left over potions
    return aiBattlePrep();
}

bool
MOB::aiTwitch(MOB *avatarvis)
{
    return false;
}


bool
MOB::aiTactics(MOB *avatarvis)
{
    return false;
}

bool
MOB::aiWantsAnyMoreItems() const
{
    // Is there anything we want for christmas?
    if (!lookupWeapon())
	return true;

    if (!lookupWand())
	return true;

    return false;
}

bool
MOB::aiWantsItem(ITEM *item) const
{
    if (!item)
	return false;

    if (item->isWeapon() && !lookupWeapon())
	return true;

    if (item->isRanged() && !lookupWand())
	return true;

    // All else is fodder.
    // "Everything is fodder" was Megatron's favorite saying according
    // to the summary comic books I have.
    return false;
}

bool
MOB::aiDoRat()
{
    // Rats only attack if they have a crowd.
    // They will fight if cornered, though!

    if (aiRangeAttack())
	return true;

    // If we are in FOV but don't see enough rats, run!
    if (pos().isFOV() && (glbMobsInFOV.safeIndexAnywhere(getDefinition()) <= 1))
    {
	if (aiFleeFromAvatar())
	    return true;
    }

    // Failed to flee, cornered
    // Or brave.
    // Attack!
    if (aiCharge(getAvatar(), AI_CHARGE))
	return true;

    return aiRandomWalk();
}

bool
MOB::aiDoOrc()
{
    // Find all people and attack closest!
    MOBLIST		allmobs;
    MOB			*mob = 0;
    int			dist = -1;
    
    map()->getAllMobs(allmobs);
    for (int i = 0; i < allmobs.entries(); i++)
    {
	if (!allmobs(i)->pos().valid())
	    continue;

	if (allmobs(i)->defn().isfriendly)
	{
	    int	map = pos().map()->buildDistMap(allmobs(i)->pos());
	    if (map < 0)
		continue;

	    if (!mob || dist > pos().getDistance(map))
	    {
		mob = allmobs(i);
		dist = pos().getDistance(map);
	    }
	}
    }

    if (!mob)
    {
	// No one to chase!
	return aiRandomWalk();
    }

    return aiKillPathTo(mob);
}

double
avatar_weight_weapon(ITEM *item)
{
    double	weight;

    weight = item->getDepth();
    return weight;
}

double
avatar_weight_wand(ITEM *item)
{
    int		ia, ir;
    double	weight;
	
    item->getRangeStats(ir, ia);

    // Estimate average damage for our weight.
    // Really should do DPF here, but we don't use this and melee isn't
    // upgraded yet.
    weight = item->getDepth();
    // Area technically improves with the square, but in practice
    // you don't have everyone bunched, so make it just
    // the linear
    weight *= ia;

    // Each two points of range gives us another attack on a charging
    // foe.
    weight *= ir * 0.5;

    return weight;
}

ITEM *
MOB::aiLikeMoreWeapon(ITEM *a, ITEM *b) const
{
    if (b && b->isBroken()) return a;

    // Trivial case
    if (!a) return b;
    if (!b) return a;

    if (a->isBroken() && !b->isBroken()) return b;
    
    if (avatar_weight_weapon(b) > avatar_weight_weapon(a))
	return b;
    // Default to a.
    return a;
}

ITEM *
MOB::aiLikeMoreArmour(ITEM *a, ITEM *b) const
{
    if (b && b->isBroken()) return a;

    // Trivial case
    if (!a) return b;
    if (!b) return a;

    if (a->isBroken() && !b->isBroken()) return b;
    
    if (b->getDamageReduction() > a->getDamageReduction())
	return b;
    // Default to a.
    return a;
}

ITEM *
MOB::aiLikeMoreWand(ITEM *a, ITEM *b) const
{
    if (b && b->isBroken()) return a;

    // Trivial case
    if (!a) return b;
    if (!b) return a;
    
    if (a->isBroken() && !b->isBroken()) return b;
    
    if (avatar_weight_wand(b) > avatar_weight_wand(a))
	return b;
    // Default to a.
    return a;
}

bool
MOB::aiDoMouse()
{
    int		lastdir, dir, newdir;
    int		dx, dy;
    bool	found = false;
    int		i;

    // Least three bits track our last direction.
    lastdir = myAIState & 7;

    // We want to try and track a wall.  We want this wall to
    // be on our right hand side.

    // Take a look at right hand square.
    rand_angletodir((lastdir + 2) & 7, dx, dy);
    
    if (!canMoveDir(dx, dy, true))
    {
	// Got a wall to the right.  Try first available direction.
	// Because we want to hug using diagonals, we try the forward
	// and right first.
	dir = (lastdir+1) & 7;
    }
    else
    {
	// No wall on right.  Try and go straight forward by default.
	dir = lastdir;
    }

    // If everyway is blocked, bump straight forward.
    newdir = lastdir;
    for (i = 0; i < 8; i++)
    {
	rand_angletodir(dir, dx, dy);
	if (canMoveDir(dx, dy, true))
	{
	    newdir = dir;
	    break;
	}
	// Keeping wall on right means turning left first!
	dir--;
	dir &= 7;
    }

    if (newdir == -1)
	newdir = lastdir;
    else
	found = true;

    // Store our last direction.
    myAIState &= ~7;
    myAIState |= newdir;

    if (found)
    {
	return actionBump(dx, dy);
    }

    // Mouse is cornered!  Return to normal AI.
    return false;
}

bool
MOB::aiStraightLine()
{
    int		lastdir;
    int		dx, dy;
    MOB		*target;
    POS		t;

    // Least three bits track our last direction.
    lastdir = myAIState & 7;

    // Bump & go.

    // Take a look at right hand square.
    rand_angletodir(lastdir, dx, dy);

    t = pos().delta(dx, dy);
    
    // If target is avatar, run into him!
    target = t.mob();
    if (target && !isFriends(target))
    {
	return actionBump(dx, dy);
    }
    
    if (!canMove(t, true))
    {
	// Can't go there!  Pick new direction.
	// Do not want same direction again.
	lastdir += rand_choice(7) + 1;
	lastdir &= 7;
    }
    myAIState = lastdir;
    rand_angletodir(lastdir, dx, dy);
    t = pos().delta(dx, dy);
    
    // If target is avatar, run into him!
    target = t.mob();
    if (target && !isFriends(target))
    {
	return actionBump(dx, dy);
    }

    // Bump failed.
    if (!canMove(t, true))
	return false;

    // Move forward!
    return actionBump(dx, dy);
}

bool
MOB::aiFleeFromAvatar()
{
    if (aiAcquireAvatar())
    {
	if (aiFleeFrom(myTarget))
	    return true;
    }

    return false;
}

bool
MOB::aiFleeFrom(POS goal, bool sameroom)
{
    int		dx, dy;
    int		angle, resultangle = 0, i;
    int		choice = 0;

    pos().dirTo(goal, dx, dy);
    dx = -dx;
    dy = -dy;
    
    angle = rand_dirtoangle(dx, dy);

    // 3 ones in same direction get equal chance.
    for (i = angle-1; i <= angle+1; i++)
    {
	rand_angletodir(i, dx, dy);
	if (sameroom)
	{
	    POS	goal;
	    goal = pos().delta(dx, dy);
	    if (goal.roomId() != pos().roomId())
		continue;
	}
	if (canMoveDir(dx, dy))
	{
	    choice++;
	    if (!rand_choice(choice))
		resultangle = i;
	}
    }

    if (!choice)
    {
	// Try a bit more desperately...
	for (i = angle-2; i <= angle+2; i += 4)
	{
	    rand_angletodir(i, dx, dy);
	    if (sameroom)
	    {
		POS	goal;
		goal = pos().delta(dx, dy);
		if (goal.roomId() != pos().roomId())
		    continue;
	    }
	    if (canMoveDir(dx, dy))
	    {
		choice++;
		if (!rand_choice(choice))
		    resultangle = i;
	    }
	}
    }

    if (choice)
    {
	// Move in the direction
	rand_angletodir(resultangle, dx, dy);
	return actionBump(dx, dy);
    }

    // Failed
    return false;
}

bool
MOB::aiFleeFromSafe(POS goal, bool avoidrange)
{
    if (aiFleeFromSafe(goal, avoidrange, true))
	return true;
    return aiFleeFromSafe(goal, avoidrange, false);
}

bool
MOB::aiFleeFromSafe(POS goal, bool avoidrange, bool avoidmob)
{
    int		dx, dy;
    int		angle, resultangle = 0, i;
    int		choice = 0;

    pos().dirTo(goal, dx, dy);
    dx = -dx;
    dy = -dy;
    
    angle = rand_dirtoangle(dx, dy);

    // 3 ones in same direction get equal chance.
    for (i = angle-1; i <= angle+1; i++)
    {
	rand_angletodir(i, dx, dy);

	// Ignore if it gets as beside!
	POS	g;
	g = pos().delta(dx, dy);
	if (g.dist(goal) <= 1)
	    continue;
	if (avoidrange && goal.mob())
	{
	    if (goal.mob()->canTargetAtRange(g))
		continue;
	}
	if (canMoveDir(dx, dy, avoidmob))
	{
	    choice++;
	    if (!rand_choice(choice))
		resultangle = i;
	}
    }

    if (!choice)
    {
	// Try a bit more desperately...
	for (i = angle-2; i <= angle+2; i += 4)
	{
	    rand_angletodir(i, dx, dy);
	    // Ignore if it gets as beside!
	    POS	g;
	    g = pos().delta(dx, dy);
	    if (g.dist(goal) <= 1)
		continue;
	    if (avoidrange && goal.mob())
	    {
		if (goal.mob()->canTargetAtRange(g))
		    continue;
	    }
	    if (canMoveDir(dx, dy, avoidmob))
	    {
		choice++;
		if (!rand_choice(choice))
		    resultangle = i;
	    }
	}
    }

    if (choice)
    {
	// Move in the direction
	rand_angletodir(resultangle, dx, dy);
	return actionBump(dx, dy);
    }

    // Failed
    return false;
}

bool
MOB::aiAcquireAvatar()
{
    MOB		*a;
    a = getAvatar();

    if (a && !a->alive())
	return false;

    return aiAcquireTarget(a);
}

bool
MOB::aiAcquireTarget(MOB *a)
{
    if (a && a->isSwallowed())
    {
	myTarget = POS();
	return false;
    }
    // If we can see avatar, charge at them!  Otherwise, move
    // to our target if set.  Otherwise random walk.
    if (a && 
	a->pos().isFOV() &&
	pos().isFOV())
    {
	int		dist;
	// Determine distance
	dist = pos().dist(a->pos());
	if (dist <= defn().sightrange)
	{
	    myTarget = a->pos();
	}
    }
    else
    {
	// Random chance of forgetting avatar location.
	// Also, if current location is avatar location, we forget it.
	if (pos() == myTarget)
	    myTarget = POS();
	if (!rand_choice(10))
	    myTarget = POS();
    }

    return myTarget.valid();
}

bool
MOB::aiRandomWalk(bool orthoonly, bool sameroom)
{
    int		dx, dy;
    int		angle, resultangle = 0;
    int		choice = 0;

    for (angle = 0; angle < 8; angle++)
    {
	if (orthoonly && (angle & 1))
	    continue;

	rand_angletodir(angle, dx, dy);
	POS	goal;
	goal = pos().delta(dx, dy);
	if (sameroom)
	{
	    if (goal.roomId() != pos().roomId())
		continue;
	    if (goal.tile() == TILE_DOOR)
	    {
		// Obviously going through a door is changing the room!
		continue;
	    }
	}

	// Don't wander onto ice as it may melt!  (together with
	// charging thus avoids orcs getting stranded in the lakes, I hope!)
	if (goal.defn().forbidrandomwander)
	    continue;

	if (aiAvoidDirection(dx, dy))
	    continue;
	if (canMoveDir(dx, dy))
	{
	    choice++;
	    if (!rand_choice(choice))
		resultangle = angle;
	}
    }

    if (choice)
    {
	// Move in the direction
	rand_angletodir(resultangle, dx, dy);
	return actionWalk(dx, dy);
    }

    // Failed
    return false;
}

bool
MOB::aiCharge(MOB *foe, AI_NAMES aitype, bool orthoonly)
{
    if (aiAcquireTarget(foe))
    {
	if (aitype == AI_FLANK)
	{
	    if (aiFlankTo(myTarget))
		return true;
	}
	else
	{
	    if (aiMoveTo(myTarget, orthoonly))
		return true;
	}
    }

    return false;
}

bool
MOB::aiRangeAttack(MOB *target)
{
    // Wait for recharge.
    if (myRangeTimeout)
	return false;

    // See if we have a ranged attack at all!
    if (!defn().range_valid && !lookupWand())
	return false;

    int		 dx, dy;
    MOB		*a = getAvatar(), *v;

    // Override the target from the avatar...
    if (target)
	a = target;

    if (!a)
	return false;

    if (!a->alive())
	return false;

    pos().dirTo(a->pos(), dx, dy);

    // If we are in melee range, don't use our ranged attack!
    if (pos().dist(a->pos()) <= 1)
    {
	return false;
    }

    // Try ranged attack.
    v = pos().traceBullet(getRangedRange(), dx, dy);
    if (v == a)
    {
	// Potential range attack.
	return actionFire(dx, dy);
    }

    return false;
}

bool
MOB::canTargetAtRange(POS goal) const
{
    int		dx, dy;

    pos().dirTo(goal, dx, dy);

    // If we are in melee range, don't use our ranged attack!
    if (pos().dist(goal) <= 1)
    {
	return false;
    }

    // Try ranged attack.
    int		range = getRangedRange();

    POS		next = pos();

    while (range > 0)
    {
	range--;
	next = next.delta(dx, dy);
	if (next == goal)
	    return true;
	if (next.mob())
	{
	    return false;
	}

	// Stop at a wall.
	if (!next.defn().ispassable)
	    return false;
    }
    return false;
}

bool
MOB::aiMoveTo(POS t, bool orthoonly)
{
    int		dx, dy, dist;
    int		angle, resultangle = 0, i;
    int		choice = 0;

    pos().dirTo(t, dx, dy);

    if (orthoonly && dx && dy)
    {
	// Ensure source is orthogonal.
	if (rand_choice(2))
	    dx = 0;
	else
	    dy = 0;
    }

    dist = pos().dist(t);

    if (dist == 1)
    {
	// Attack!
	if (canMoveDir(dx, dy, false))
	    return actionBump(dx, dy);
	return false;
    }

    // Move in general direction, preferring a straight line.
    angle = rand_dirtoangle(dx, dy);

    for (i = 0; i <= 2; i++)
    {
	if (orthoonly && (i & 1))
	    continue;

	rand_angletodir(angle-i, dx, dy);
	if (canMoveDir(dx, dy))
	{
	    choice++;
	    if (!rand_choice(choice))
		resultangle = angle-i;
	}
	
	rand_angletodir(angle+i, dx, dy);
	if (canMoveDir(dx, dy))
	{
	    choice++;
	    if (!rand_choice(choice))
		resultangle = angle+i;
	}

	if (choice)
	{
	    rand_angletodir(resultangle, dx, dy);

	    // If wew can leap, maybe leap?
	    if (defn().canleap)
	    {
		if (dist > 2 &&
		    canMoveDir(2*dx, 2*dy))
		{
		    formatAndReport("%S <leap>.");
		    dx *= 2;
		    dy *= 2;
		}
	    }
	    
	    return actionWalk(dx, dy);
	}
    }

    return false;
}

bool
MOB::aiKillPathTo(MOB *target)
{
    if (!target)
	return false;

    myTarget = target->pos();
    
    return aiPathFindTo(target->pos());
}

bool
MOB::aiPathFindTo(POS goal)
{
    if (aiPathFindTo(goal, true))
	return true;
    return aiPathFindTo(goal, false);
}

bool
MOB::aiPathFindTo(POS goal, bool avoidmob)
{
    int		dx, dy, ndx, ndy;
    int		curdist, dist;
    int		distmap;

    bool	found;

    // see if already there.
    if (pos() == goal)
	return false;

    // Really, really, inefficient.
    distmap = pos().map()->buildDistMap(goal);

    curdist = pos().getDistance(distmap);
    
    // We don't care if this square is unreachable, as if it is the
    // square with the mob, almost by definition it is unreachable
    
    found = false;
    int			nmatch = 0;
    FORALL_8DIR(dx, dy)
    {
	dist = pos().delta(dx, dy).getDistance(distmap);

	// If dist is 0, we do not ignore the mob as we want
	// to reach it!
	if (avoidmob && dist)
	    if (pos().delta(dx, dy).mob())
		continue;
	// If there is a corpse there, keep away.
	if (aiAvoidDirection(dx, dy))
	    continue;
	if (dist >= 0 && dist <= curdist)
	{
	    found = true;

	    if (dist < curdist)
	    {
		// Net new smallest
		ndx = dx;
		ndy = dy;
		curdist = dist;
		nmatch = 1;
	    }
	    else
	    {
		// Replace if chance.
		nmatch++;
		if (!rand_choice(nmatch))
		{
		    ndx = dx;
		    ndy = dy;
		}
	    }
	}
    }

    // If we didn't find a direction, abort
    if (!found)
    {
	if (isAvatar())
	    formatAndReport("%S cannot find a path there.");
	return false;
    }

    // Otherwise, act.
    return actionWalk(ndx, ndy);
}

bool
MOB::aiPathFindToAvoid(POS goal, MOB *avoid)
{
    if (aiPathFindToAvoid(goal, avoid, true))
	return true;
    return aiPathFindToAvoid(goal, avoid, false);
}

double
aiWeightedDist(double dist, double avoid)
{
    // Max effect is 10
    avoid = 10 - avoid;
    if (avoid < 0)
	return dist;

    // Go non-linear!
    avoid *= avoid;
    avoid /= 10;

    return dist + avoid;
}

bool
MOB::aiPathFindToAvoid(POS goal, MOB *avoid, bool avoidmob)
{
    int		dx, dy, ndx, ndy;
    double	curdist, dist;
    int		distmap, avoidmap;

    bool	found;

    if (!avoid)
	return aiPathFindTo(goal, avoidmob);

    // see if already there.
    if (pos() == goal)
	return false;

    // Really, really, inefficient.
    distmap = pos().map()->buildDistMap(goal);
    avoidmap = pos().map()->buildDistMap(avoid->pos());

    // We bias towards path finding so that one will flow around the
    // target rather than stop hard.
    curdist = aiWeightedDist(pos().getDistance(distmap),
			     pos().getDistance(avoidmap));
    
    // We don't care if this square is unreachable, as if it is the
    // square with the mob, almost by definition it is unreachable
    
    found = false;
    int			nmatch = 0;
    FORALL_8DIR(dx, dy)
    {
	dist = pos().delta(dx, dy).getDistance(distmap);

	if (dist < 0)
	    continue;

	if (avoidmob)
	    if (pos().delta(dx, dy).mob())
		continue;

	// Refuse to step beside the avoid mob.
	if (avoid->pos().dist(pos().delta(dx, dy)) <= 1)
	    continue;

	// If the avoid mob has a ranged attack, don't be stupid.
	if (avoid->lookupWand())
	{
	    if (avoid->canTargetAtRange(pos().delta(dx, dy)))
		continue;
	}
	
	dist = aiWeightedDist(dist, pos().delta(dx, dy).getDistance(avoidmap));
	if (dist <= curdist)
	{
	    found = true;

	    if (dist < curdist)
	    {
		// Net new smallest
		ndx = dx;
		ndy = dy;
		curdist = dist;
		nmatch = 1;
	    }
	    else
	    {
		// Replace if chance.
		nmatch++;
		if (!rand_choice(nmatch))
		{
		    ndx = dx;
		    ndy = dy;
		}
	    }
	}
    }

    // If we didn't find a direction, abort
    if (!found)
    {
	if (isAvatar())
	    formatAndReport("%S cannot find a path there.");
	return false;
    }

    // Otherwise, act.
    return actionWalk(ndx, ndy);
}

bool
MOB::aiFlankTo(POS goal)
{
    int		dx, dy, dist;
    int		angle, resultangle = 0, i, j;
    int		choice = 0;

    MOB		*avatarvis = goal.mob();
    bool	aisrange = false;
    
    if (avatarvis)
	aisrange = avatarvis->lookupWand() ? true : false;

    pos().dirTo(goal, dx, dy);

    dist = pos().dist(goal);

    if (dist == 1)
    {
	// Attack!
	if (canMove(goal, false))
	    return actionBump(dx, dy);
	return false;
    }

    // Move in general direction, preferring a straight line.
    angle = rand_dirtoangle(dx, dy);

    for (j = 0; j <= 2; j++)
    {
	// To flank, we prefer the non-straight approach.
	switch (j)
	{
	    case 0:
		i = 1;
		break;
	    case 1:
		i = 0;
		break;
	    case 2:
		i = 2;
		break;
	}
	rand_angletodir(angle-i, dx, dy);
	if (canMoveDir(dx, dy) &&
	    (!aisrange || !avatarvis->canTargetAtRange(pos().delta(dx, dy))))
	{
	    choice++;
	    if (!rand_choice(choice))
		resultangle = angle-i;
	}
	
	rand_angletodir(angle+i, dx, dy);
	if (canMoveDir(dx, dy) &&
	    (!aisrange || !avatarvis->canTargetAtRange(pos().delta(dx, dy))))
	{
	    choice++;
	    if (!rand_choice(choice))
		resultangle = angle+i;
	}

	if (choice)
	{
	    rand_angletodir(resultangle, dx, dy);
	    return actionWalk(dx, dy);
	}
    }

    return false;
}

