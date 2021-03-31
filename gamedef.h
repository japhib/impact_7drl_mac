/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7dayband Develoment
 *
 * NAME:        map.h ( 7dayband, C++ )
 *
 * COMMENTS:
 *	Defines all the game state that the user has
 *	input that differentiates this from the shell game.
 */

#ifndef __gamedef_h__
#define __gamedef_h__

class GAMEDEF
{
public:
    static void		reset(bool loaddefaults);
    static void		load(const char *fname);
    static void		save(const char *fname);

    // We note here for the record that a strict interpretation
    // of C++ only allows enums to have values up to one less than
    // the highest power of two that holds them.
    // 
    // This is bullshit.
    //
    // I was incredibly happy when the gcc team was forced to admit
    // that despite the standard so saying, any code compiled with
    // non-short enums had better support the possibility of out of
    // range values.
    
#define 	ENUM_ACCESSORS(name, Name, NAME) \
    static NAME##_DEF	*name##def(int defn); \
    static NAME##_NAMES	 createNew##Name##Def(); \
    static int 		 getNum##Name();

    ENUM_ACCESSORS(mob, Mob, MOB)
    ENUM_ACCESSORS(attr, Attr, ATTR)
    ENUM_ACCESSORS(tile, Tile, TILE)
    ENUM_ACCESSORS(item, Item, ITEM)
    ENUM_ACCESSORS(itemclass, Itemclass, ITEMCLASS)
    ENUM_ACCESSORS(armourslot, Armourslot, ARMOURSLOT)
    ENUM_ACCESSORS(attack, Attack, ATTACK)
    ENUM_ACCESSORS(potion, Potion, POTION)
    ENUM_ACCESSORS(ring, Ring, RING)
    ENUM_ACCESSORS(element, Element, ELEMENT)
    ENUM_ACCESSORS(material, Material, MATERIAL)
    ENUM_ACCESSORS(gamerules, GameRules, GAMERULES)
    ENUM_ACCESSORS(level, Level, LEVEL)
    ENUM_ACCESSORS(effect, Effect, EFFECT)
    ENUM_ACCESSORS(spell, Spell, SPELL)
    ENUM_ACCESSORS(key, Key, KEY)
    ENUM_ACCESSORS(terrain, Terrain, TERRAIN)
    ENUM_ACCESSORS(hero, Hero, HERO)

#undef ENUM_ACCESSORS

    // Useful shortcuts.
    static GAMERULES_DEF	&rules() { return *gamerulesdef(GAMERULES_OFFICIAL); }
};



#endif
