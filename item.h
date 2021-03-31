/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        item.h ( Live Once Library, C++ )
 *
 * COMMENTS:
 */

#ifndef __item__
#define __item__

#include "glbdef.h"

#include "dpdf.h"
#include "grammar.h"
#include "map.h"
#include "buf.h"

#include <iostream>
using namespace std;

// Generates a unique id
int glb_allocUID();
// Reports that the given UID was loaded from disk, ensures we don't
// repeat it.
void glb_reportUID(int uid);

#define INVALID_UID -1

class ITEM
{
public:
		~ITEM();

    static ITEM *create(ITEM_NAMES item, int depth = 0);

    // This can return 0 if no valid items exist at this depth!
    static ITEM *createRandom(int depth);
    // This respects our unlocks so *will* give zeros!
    static ITEM *createRandomBalanced(int depth);
    static ITEM_NAMES itemFromHash(unsigned hash, int depth);

    static void	initSystem();
    static void saveGlobal(ostream &os);
    static void loadGlobal(istream &is);

    // Setups up $ITEMNAME $AN_ITEMNAME
    void		 setupTextVariables() const;

    // Makes an identical copy
    ITEM	*copy() const;
    // Makes a new item with all the same properties, but new UID, etc.
    ITEM	*createCopy() const;

    ITEM_NAMES	 getDefinition() const { return myDefinition; }
    int		 getMagicClass() const;
    void	 markMagicClassKnown();
    bool	 isMagicClassKnown() const;
		
    VERB_PERSON	 getPerson() const;
    BUF		 getName() const;
    BUF		 getSingleName() const;
    BUF		 getSingleArticleName() const;
    BUF		 getRawName() const;
    BUF		 getArticleName() const;

    // Returns "" if no detailed description.  Returns a multi-line
    // buffer prefixed with +-
    BUF		 getDetailedDescription() const;

    // Detailed Description + the text lookup.
    BUF		 getLongDescription() const;

    bool	 isBroken() const { return myBroken; }
    void	 setBroken(bool isbroken) { myBroken = isbroken; }

    int		 breakChance(MOB_NAMES victim) const;

    bool	 isEquipped() const { return myEquipped; }
    void	 setEquipped(bool isequipped) { myEquipped = isequipped; }

    const ITEM_DEF	&defn() const { return defn(getDefinition()); }
    static const ITEM_DEF &defn(ITEM_NAMES item) { return *GAMEDEF::itemdef(item); }

    ATTACK_NAMES	getMeleeAttack() const;
    const ATTACK_DEF	&attackdefn() const { return *GAMEDEF::attackdef(getMeleeAttack()); }
    static const ATTACK_DEF &attackdefn(ITEM_NAMES item) { return *GAMEDEF::attackdef((ATTACK_NAMES) defn(item).melee_attack); }
    const ATTACK_DEF	&rangedefn() const { return *GAMEDEF::attackdef(getRangeAttack()); }
    static const ATTACK_DEF &rangedefn(ITEM_NAMES item) { return *GAMEDEF::attackdef((ATTACK_NAMES) defn(item).range_attack); }
    ITEM_DEF	&edefn() const { return edefn(getDefinition()); }
    static ITEM_DEF &edefn(ITEM_NAMES item) { return *GAMEDEF::itemdef(item); }

    void	 getLook(u8 &symbol, ATTR_NAMES &attr) const;

    const POS   &pos() const { return myPos; }

    // Warning: This can delete this
    void	 move(POS pos);

    // Silent move, does not update maps!  Do not use.
    void	 silentMove(POS pos) { myPos = pos; }

    // Unlinks our position, dangerous so only use if you are sure you
    // will be removing from the map!
    void	 clearAllPos();

    void	 setMap(MAP *map) { myPos.setMap(map); }

    bool	 canStackWith(const ITEM *stack) const;
    void	 combineItem(const ITEM *item);
    int		 getStackCount() const { return myCount; }
    void	 decStackCount() { myCount--; }
    void	 setStackCount(int count) { myCount = count; }

    // -1 for things without a count down.
    int		 getTimer() const { return myTimer; }
    void	 addTimer(int add) { myTimer += add; }
    void	 setTimer(int timer) { myTimer = timer; }

    // Returns true if should self destruct.
    bool	 runHeartbeat();

    int		 getDepth() const { return defn().depth; }

    int		 getLevel() const { return myLevel; }
    int		 getExp() const { return myExp; }

    void	 gainExp(int exp, bool silent);
    // Gain one level.  Does not affect exp, designed to separate level
    // up keep.
    void	 gainLevel(bool silent);


    // Determines if it is at all considerable as a weapon
    bool	 isWeapon() const;
    bool	 isArmour() const;
    bool	 isRanged() const;
    bool	 isPotion() const;
    bool	 isSpellbook() const;
    bool	 isRing() const;
    bool	 isFood() const;
    bool	 isFlag() const;

    ITEMCLASS_NAMES	 itemClass() const { return (ITEMCLASS_NAMES) defn().itemclass; }
    WEAPONCLASS_NAMES	 weaponClass() const { return (WEAPONCLASS_NAMES) defn().weaponclass; }

    int		 getRangeRange() const;
    int		 getRangeArea() const;
    void	 getRangeStats(int &range, int &area) const;
    ATTACK_NAMES getRangeAttack() const;

    int		 getDamageReduction() const { return defn().damagereduction; }

    void	 save(ostream &os) const;
    static ITEM	*load(istream &is);

    int		 getUID() const { return myUID; }
    int		 getNextStackUID() const { return myNextStackUID; }
    void	 setNextStackUID(int uid) { myNextStackUID = uid; }

    int		 getInterestedUID() const { return myInterestedMobUID; }
    void	 setInterestedUID(int uid) { myInterestedMobUID = uid; }

    void	 setMobType(MOB_NAMES mob) { myMobType = mob; }
    MOB_NAMES	 mobType() const { return myMobType; }
    void	 setHero(HERO_NAMES hero) { myHero = hero; }
    HERO_NAMES	 hero() const { return myHero; }

    void	 setElement(ELEMENT_NAMES element) { myElement = element; }
    ELEMENT_NAMES element() const { return myElement; }

    EFFECT_NAMES  	 corpseEffect() const;
    EFFECT_NAMES	 potionEffect() const;
    static EFFECT_NAMES	 potionEffect(ITEM_NAMES item);

    MATERIAL_NAMES	material() const { return myMaterial; }
    void		setMaterial(MATERIAL_NAMES material) { myMaterial = material; }
    MATERIAL_NAMES	giltMaterial() const { return myGiltMaterial; }
    bool		isGilt() const { return myGiltMaterial != MATERIAL_NONE; }

protected:
		 ITEM();

    ITEM_NAMES	 myDefinition;
    ELEMENT_NAMES	myElement;

    MOB_NAMES	 myMobType;
    HERO_NAMES   myHero;

    POS		 myPos;
    int		 myCount;
    int		 myTimer;
    int		 myUID;
    int		 myNextStackUID;
    int		 myInterestedMobUID;
    bool	 myBroken;
    bool	 myEquipped;
    int		 mySeed;

    int		 myExp, myLevel;

    MATERIAL_NAMES	myMaterial, myGiltMaterial;
};

#endif

