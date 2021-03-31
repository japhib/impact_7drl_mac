/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        mob.h ( Live Once Library, C++ )
 *
 * COMMENTS:
 */

#ifndef __mob__
#define __mob__

#include "glbdef.h"
#include "gamedef.h"

#include "grammar.h"
#include "dpdf.h"
#include "ptrlist.h"
#include "map.h"

#include <iostream>
using namespace std;

class MAP;
class ITEM;

class MOB
{
public:
    static MOB		*create(MOB_NAMES def);

    // This can return 0 if no valid mobs at that depth!
    static MOB		*createNPC(int depth);
    static MOB		*createHero(HERO_NAMES player);

    // Construct a sensible set of monster defines from scratch.
    static void		 buildMonsterList();
    static MOB_NAMES	 getBossName();

    static void	initSystem();
    static void saveGlobal(ostream &os);
    static void loadGlobal(istream &is);

    // Creates a copy.  Ideally we make this copy on write so
    // is cheap, but mobs probably move every frame anyways.
    MOB			*copy() const;
    
    ~MOB();

    const POS		&pos() const { return myPos; }
    // TFW you notice you've been doing pos().map() everywhere and
    // this has been around...
    const MAP		*map() const { return myPos.map(); }

    bool		 isMeditating() const
			 { return myMeditatePos.valid(); }
    bool		 isMeditateDecoupled() const
			 { return myDecoupledFromBody; }

    void		 startWaiting() 
    { 
	stopRunning();		// duh!
	myWaiting = true; 
    }

    void		 stopWaiting(const char *msg) 
    { 
	if (isWaiting()) 
	{ 
	    myWaiting = false; 
	    if (msg) formatAndReport(msg); 
	} 
    }

    bool		 isWaiting() const
			 { return myWaiting; }

    void		 meditateMove(POS dest, bool decouple = true);

    POS			 meditatePos() const
			 { if (isMeditating()) return myMeditatePos;
			   return pos();
			 }
    void		 setPos(POS pos);
    void		 setMap(MAP *map);
    void		 clearAllPos();

    bool		 alive() const { return myHP > 0; }

    int			 getHP() const { return myHP; }
    int			 getMaxHP() const;
    int			 getMP() const { return myMP; }
    int			 getMaxMP() const;
    bool		 isFullHealth() const { return myHP == getMaxHP(); }
    int			 getGold() const;
    void		 gainGold(int deltagold);
    int			 getCoin(MATERIAL_NAMES material) const;
    void		 gainCoin(MATERIAL_NAMES material, int deltagold);

    int			 getFood() const;
    void		 gainFood(int food);
    void		 eatFood(int food);
    int			 getMaxFood() const;

    void		 gainExp(int exp, bool silent);
    // Gain one level.  Does not affect exp, designed to separate level
    // up keep.
    void		 gainLevel(bool silent);

    int			 getExp() const { return myExp; }
    int			 getLevel() const { return myLevel; }
    int			 getExtraLives() const { return myExtraLives; }
    void		 grantExtraLife();
    int			 getSpellTimeout() const { return mySpellTimeout; }

    BUF		 	 buildAttackDescr(ATTACK_NAMES attack) const;
    BUF		 	 buildSpecialDescr(MOB_NAMES mob) const;
    BUF		 	 buildEffectDescr(EFFECT_NAMES effect) const;
    BUF		 	 buildSpellDescr(SPELL_NAMES spell) const;

    void		 gainHP(int hp);
    void		 gainMP(int mp);
    void		 gainPartyHP(int hp);
    void		 gainPartyMP(int mp, ELEMENT_NAMES element);
    void		 corrupt(int corruption);

    bool		 isVulnerable(ELEMENT_NAMES element) const;

    int			 numDeaths() const { return myNumDeaths; }

    void		 setHome(POS pos) { myHome = pos; }

    bool		 isSwallowed() const { return myIsSwallowed; }
    void		 setSwallowed(bool isswallowed) { myIsSwallowed = isswallowed; }

    // REturns true if we die!
    bool		 applyEffect(MOB *src, EFFECT_NAMES effect,
				    ATTACKSTYLE_NAMES attackstyle);

    // Returns true if we die!
    bool		 applyDamage(MOB *src, int hp, ELEMENT_NAMES element,
				    ATTACKSTYLE_NAMES attackstyle, bool silent = false);
    void		 kill(MOB *src);

    // posts an event tied to this mobs location.
    void		 postEvent(EVENTTYPE_NAMES type, u8 sym, ATTR_NAMES attr, const char *text = 0) const;
    void		 postEvent(EVENTTYPE_NAMES type, u8 sym, ATTR_NAMES attr, BUF buffer) const { postEvent(type, sym, attr, buffer.buffer()); }


    MOB_NAMES		 getDefinition() const { return myDefinition; }

    const MOB_DEF	&defn() const { return defn(getDefinition()); }
    static const MOB_DEF &defn(int mob) { return *GAMEDEF::mobdef(mob); }
    MOB_DEF		&edefn() const { return edefn(getDefinition()); }
    static MOB_DEF 	&edefn(int mob) { return *GAMEDEF::mobdef(mob); }

    bool		 isAvatar() const { return this == getAvatar(); }
    bool		 inParty() const;
    MOB			*getAvatar() const { if (pos().map()) return pos().map()->avatar(); return 0; }
    bool		 hasWon() const { return myHasWon; }

    bool		 isBoss() const { return false; }
    VERB_PERSON		 getPerson() const;

    // Symbol and attributes for rendering.
    void		 getLook(u8 &symbol, ATTR_NAMES &attr) const;

    // Retrieve the raw name.
    BUF			 getName() const;
    BUF			 getRawName() const;

    void		 setName(const char *name) { myName.strcpy(name); }
    void		 setName(const BUF &buf) { myName = buf; }

    BUF			 getLongDescription() const;

    // Returns true if we have the item in question.
    bool		 hasItem(ITEM_NAMES itemname) const;
    // Gives this mob a new version of the item, useful for flags.
    // Returns the resulting item.
    ITEM		*giftItem(ITEM_NAMES itemname);
    ITEM 		*lookupItem(ITEM_NAMES itemname) const;
    ITEM 		*lookupItem(ITEM_NAMES itemname, MATERIAL_NAMES materialname) const;
    ITEM 		*lookupWeapon() const;
    ITEM		*lookupArmour() const;
    int			 getDamageReduction(ELEMENT_NAMES element) const;
    ITEM 		*lookupWand() const;
    ITEM 		*lookupRing() const;
    RING_NAMES		 lookupRingName() const;
    ITEM		*getRandomItem(bool allowequipped) const;

    // Steal all the items that aren't equipped or flags, used to
    // emulate shared inventory.
    void		 stealNonEquipped(MOB *src);

    // Setups up $MOBNAME $A_MOBNAME
    void		 setupTextVariables() const;

    // If item, uses items level boost, otherwise our level boost for
    // natural attacks.
    int			 evaluateAttackDamage(ATTACK_NAMES attack, ITEM *weap) const;
    bool		 evaluateWillHit(ATTACK_NAMES attack, ITEM *weap, MOB *victim) const;

    // Poor MOB:: gets all these utilities as I'm too lazy to put
    // them where they belong!
    static int			 evaluateDistribution(int amount, DISTRIBUTION_NAMES distribution);

    ATTACK_NAMES	 getMeleeAttack() const;
    const ATTACK_DEF	&attackdefn() const { return *GAMEDEF::attackdef(getMeleeAttack()); }	
    bool	 	 evaluateMeleeWillHit(MOB *victim) const;
    int			 evaluateMeleeDamage() const;
    int			 getMeleeCanonicalDamage() const;
    const char		*getMeleeVerb() const;
    const char		*getMeleeWeaponName() const;
    ELEMENT_NAMES	 getMeleeElement() const;

    // Note that ITEM uses getRange, and MOB uses getRanged.  This is
    // a very clever form of hungarian so you always know if you are dealing
    // with ITEM or MOB.  Heaven forbid you write clever duck-typed template
    // code anywhere!
    ATTACK_NAMES	 getRangedAttack() const;
    int			 getRangedCanonicalDamage() const;
    const ATTACK_DEF	&rangeattackdefn() const { return *GAMEDEF::attackdef(getRangedAttack()); }	
    DPDF		 getRangedDPDF() const;
    int			 getRangedRange() const;
    const char		*getRangedWeaponName() const;
    const char		*getRangedVerb() const;
    ELEMENT_NAMES	 getRangedElement() const;

    void		 getRangedLook(u8 &symbol, ATTR_NAMES &attr) const;

    bool		 canMoveDir(int dx, int dy, bool checkmob = true) const;
    bool		 canMove(POS pos, bool checkmob = true) const;

    // true if we don't feel like going there.
    bool		 aiAvoidDirection(int dx, int dy) const;

    void		 move(POS newpos, bool ignoreangle = false);
    void		 silentMove(POS newpos) { myPos = newpos; }

    bool		 isFriends(const MOB *other) const;
    // Number of hostiles surrounding this.
    int			 numberMeleeHostiles() const;
    // True if we can hit that square with our current ranged weapon.
    bool		 canTargetAtRange(POS goal) const;

    void		 setSearchPower(int power) { mySearchPower = power; }

    int			 getCowardice() const { return myCowardice; }
    void		 setCowardice(int cower) { myCowardice = cower; }

    int			 getKills() const { return myKills; }
    void		 recordKill() { myKills++; }

    void		 addItem(ITEM *item);
    void		 removeItem(ITEM *item, bool quiet = false);
    void		 updateEquippedItems();
    // Decreases the items count by one, returning a singular copy
    // of the item the caller must delete.  (But is useful for getting
    // a single verb)
    ITEM		*splitStack(ITEM *item);

    void		 loseTempItems();
    void		 loseAllItems();

    void		 clearBoredom() { myBoredom = 0; }

    // Message reporting, only spams if the player can see it.
    void		 formatAndReport(const char *msg);
    void		 formatAndReport(BUF buf) { formatAndReport(buf.buffer()); }
    void		 formatAndReport(const char *msg, MOB *object);
    void		 formatAndReport(const char *msg, const char *verb, MOB *object);
    void		 formatAndReport(BUF buf, MOB *object) { formatAndReport(buf.buffer(), object); }
    void		 formatAndReport(const char *msg, ITEM *object);
    void		 formatAndReport(BUF buf, ITEM *object) { formatAndReport(buf.buffer(), object); }
    void		 formatAndReport(const char *msg, const char *object);
    void		 formatAndReport(BUF buf, const char *object) { formatAndReport(buf.buffer(), object); }
    void		 formatAndReport(const char *msg, BUF object) { formatAndReport(msg, object.buffer()); }
    void		 formatAndReport(BUF buf, BUF object) { formatAndReport(buf.buffer(), object.buffer()); }


    //
    // High level AI functions.
    //

    AI_NAMES		 getAI() const;

    void		 doEmote(const char *txt);
    void		 doEmote(BUF buf) { doEmote(buf.buffer()); }
    void		 doShout(const char *txt);
    void		 doShout(BUF buf) { doShout(buf.buffer()); }

    // Determines if there are any mandatory actions to be done.
    // Return true if a forced action occured, in which case the
    // player gets no further input.
    bool		 aiForcedAction();

    void		 aiTrySpeaking();

    // Runs the normal AI routines.  Called if aiForcedAction failed
    // and not the avatar.
    bool		 aiDoAI();
    // Allows you to force a specific ai type.
    bool		 aiDoAIType(AI_NAMES aitype);

    // Runs the twitch AI.  Stuff that we must do the next turn.
    bool		 aiTwitch(MOB *avatar);

    // Runs the tatics AI.  How to fight this battle.
    bool		 aiTactics(MOB *avatar);

    // Before we wade into battle
    bool		 aiBattlePrep();

    // Deny the avatar!
    bool		 aiDestroySomethingGood(MOB *denyee);

    // Updates our lock on the avatar - tracking if we know
    // where he is.  Position of last known location is in myTX.
    bool		 aiAcquireAvatar();
    bool		 aiAcquireTarget(MOB *foe);

    // Determine which item we like more.
    ITEM		*aiLikeMoreWeapon(ITEM *a, ITEM *b) const;
    ITEM		*aiLikeMoreWand(ITEM *a, ITEM *b) const;
    ITEM		*aiLikeMoreArmour(ITEM *a, ITEM *b) const;

    // Hugs the right hand walls.
    bool		 aiDoMouse();
    // Only attacks in numbers
    bool		 aiDoRat();
    // Swarms all friendly critters!
    bool		 aiDoOrc();

    // Runs in straight lines until it hits something.
    bool		 aiStraightLine();

    // Charges the listed mob if in FOV
    bool		 aiCharge(MOB *foe, AI_NAMES aitype, bool orthoonly = false);

    // Can find mob anywhere on the map - charges and kills.
    bool		 aiKillPathTo(MOB *target);

    // Attempts a ranged attack against the avatar
    bool		 aiRangeAttack(MOB *target = 0);

    // Runs away from (x, y).  Return true if action taken.
    bool		 aiFleeFrom(POS goal, bool sameroom = false);
    bool		 aiFleeFromAvatar();
    // Makes the requirement the new square is not adjacent.
    bool		 aiFleeFromSafe(POS goal, bool avoidrange, bool avoidmob);
    bool		 aiFleeFromSafe(POS goal, bool avoidrange);

    // Runs straight towards (x, y).  Return true if action taken.
    bool		 aiMoveTo(POS goal, bool orthoonly = false);

    // Does a path find to get to x/y.  Returns false if blocked
    // or already at x & y.
    bool		 aiPathFindTo(POS goal);
    bool		 aiPathFindTo(POS goal, bool avoidmob);
    // Does a path find, trying to avoid the given mob if not 0
    bool		 aiPathFindToAvoid(POS goal, MOB *avoid);
    bool		 aiPathFindToAvoid(POS goal, MOB *avoid, bool avoidmob);

    // Tries to go to (x, y) by flanking them.
    bool		 aiFlankTo(POS goal);
    
    bool		 aiRandomWalk(bool orthoonly = false, bool sameroom = false);

    // Action methods.  These are how the AI and user manipulates
    // mobs.
    // Return true if the action consumed a turn, else false.
    bool		 actionBump(int dx, int dy);
    bool		 actionRotate(int angle);
    bool		 actionChat(int dx, int dy);
    bool		 actionMelee(int dx, int dy);
    bool		 actionWalk(int dx, int dy);
    bool		 actionFire(int dx, int dy);
    bool		 actionCast(SPELL_NAMES spell, int dx, int dy);
    bool		 actionThrow(ITEM *item, int dx, int dy);
    bool		 actionThrowTop(int dx, int dy);
    bool		 actionPortalFire(int dx, int dy, int portal);
    bool		 actionPickup();
    bool		 actionPickup(ITEM *item);
    bool		 actionTransmute();
    bool		 actionTransmute(ITEM *item);
    bool		 actionSetLeader(int partyidx);
    bool		 actionDrop(ITEM *item);
    bool		 actionDropButOne(ITEM *item);
    bool		 actionDropSurplus();
    bool		 actionDropTop();
    bool		 actionDropAll();
    bool		 actionBagShake();
    bool		 actionBagSwapTop();
    bool		 actionEat(ITEM *item);
    bool		 actionEatTop();
    bool		 actionQuaff(ITEM *item);
    bool		 actionQuaffTop();
    bool		 actionRead(ITEM *item);
    bool		 actionBreak(ITEM *item);
    bool		 actionBreakTop();
    bool		 actionWear(ITEM *item);
    bool		 actionWearTop();
    bool		 actionMeditate();
    bool		 actionSearch(bool silent = false);

    bool		 actionYell(YELL_NAMES yell);

    bool		 actionSuicide();

    bool		 actionClimb();

    // Escapes if at a boundary.
    bool		 actionFleeBattleField();

    bool		 castDestroyWalls();

    void		 save(ostream &os) const;
    static MOB		*load(istream &is);

    const ITEMLIST	&inventory() const { return myInventory; }
    ITEM		*getItemFromNo(int itemno) const
			{ if (itemno < 0 || itemno >= myInventory.entries()) return 0; return myInventory(itemno); }
    ITEM		*getItemFromId(int itemid) const;

    ITEM		*getTopBackpackItem(int depth = 0) const;
    ITEM		*getWieldedOrTopBackpackItem() const;

    void		 getVisibleEnemies(PTRLIST<MOB *> &list) const;
    bool		 hasVisibleEnemies() const;
    bool		 hasSurplusItems() const;
    int			 numSurplusRange() const;
    int			 hasDestroyables() const;

    bool		 aiWantsItem(ITEM *item) const;
    bool		 aiWantsAnyMoreItems() const;

    bool		 buildPortalAtLocation(POS vpos, int portal) const;

    // Searches the given square.
    void		 searchOffset(int dx, int dy, bool silent);

    template <typename OP>
    bool		 doRangedAttack(int range, int area, int dx, int dy,
				u8 sym, ATTR_NAMES attr,
				const char *verb, bool targetself,
				bool piercing, OP op);
    void		 triggerManaUse(SPELL_NAMES spell, int manaused);

    void		 clearCollision();
    MOBLIST		&collisionSources() { return myCollisionSources; }
    const MOBLIST	&collisionSources() const { return myCollisionSources; }
    MOB			*collisionTarget() const { return myCollisionTarget; }
    // Handles sources backpointer.
    void		 setCollisionTarget(MOB *target);

    bool	  	 isDelayMob() const { return myDelayMob; }
    int			 delayMobIdx() const { return myDelayMobIdx; }
    void		 setDelayMob(bool val, int idx) 
			 { myDelayMob = val;
				myDelayMobIdx = idx; }
    
    int			 getUID() const { return myUID; }
    void		 setUID(int uid) { myUID = uid; }

    const HERO_DEF	&hero() const { return hero(getHero()); }
    static const HERO_DEF &hero(int h) { return *GAMEDEF::herodef(h); }
    HERO_NAMES		 getHero() const { return myHero; }
    void		 setHero(HERO_NAMES pc) { myHero = pc; }

    int			 getNextStackUID() const { return myNextStackUID; }
    void		 setNextStackUID(int uid) { myNextStackUID = uid; }

    bool		 shouldPush(MOB *victim) const;

    // To allow us to find ourselves in auxillary structures.
    // Not saved or copied.
    int			 getScratchIndex() const { return myScratchIndex; }
    void		 setScratchIndex(int index) { myScratchIndex = index; }

    void		 reportSquare(POS p);

    void		 skipNextTurn() { mySkipNextTurn = true; }

    void		 stopRunning();

    // 0 .. 1 for taintedness.
    static float	 transmuteTaint();
    static void		 incTransmuteTaint(int amount = 1);
protected:
    MOB();

    MOB_NAMES		 myDefinition;

    BUF			 myName;

    POS			 myPos;
    POS			 myMeditatePos;
    bool		 myDecoupledFromBody;

    int			 myNumDeaths;

    ITEMLIST		 myInventory;

    // Current target
    POS			 myTarget;
    int			 myFleeCount;
    int			 myBoredom;
    int			 myYellHystersis;

    bool		 myIsSwallowed;

    bool		 myHeardYell[NUM_YELLS];
    bool		 myHeardYellSameRoom[NUM_YELLS];
    bool		 mySawMurder;
    bool		 mySawMeanMurder;
    bool		 mySawVictory;
    bool		 myAvatarHasRanged;
    bool		 myAngryWithAvatar;

    // State machine
    int			 myAIState;

    /* Scratch */
    int			 myScratchIndex;
    /* End scratch */

    // My home spot.
    POS			 myHome;

    // Hitpoints
    int			 myHP;
    int			 myMP;
    int			 myFood;

    int			 mySearchPower;
    int			 myKills;

    // Useless ID
    int			 myUID;
    int			 myNextStackUID;	// used for stacking in map

    MOB			*myCollisionTarget;
    MOBLIST		 myCollisionSources;

    bool		 mySkipNextTurn;
    bool		 myDelayMob;
    bool		 myWaiting;
    int			 myDelayMobIdx;

    // HP threshold where I retreat.
    int			 myCowardice;

    int			 myRangeTimeout;
    int			 mySpellTimeout;
    bool		 myHasWon;
    bool		 myHasTriedSuicide;

    HERO_NAMES		 myHero;

    int			 myLastDir, myLastDirCount;
    int			 mySeed;

    int			 myExp, myLevel;
    int			 myExtraLives;
};

#endif

