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

#include "glbdef.h"
#include "gamedef.h"
#include "ptrlist.h"
#include "thread.h"
#include "mob.h"
#include "item.h"
#include <stdio.h>
#include <fstream>

#define		DECLARE_ENUM(name, NAME) \
PTRLIST<NAME##_DEF *>	xtra_##name##defs;

DECLARE_ENUM(mob, MOB)
DECLARE_ENUM(attr, ATTR)
DECLARE_ENUM(tile, TILE)
DECLARE_ENUM(item, ITEM)
DECLARE_ENUM(itemclass, ITEMCLASS)
DECLARE_ENUM(armourslot, ARMOURSLOT)
DECLARE_ENUM(attack, ATTACK)
DECLARE_ENUM(potion, POTION)
DECLARE_ENUM(ring, RING)
DECLARE_ENUM(element, ELEMENT)
DECLARE_ENUM(material, MATERIAL)
DECLARE_ENUM(gamerules, GAMERULES)
DECLARE_ENUM(level, LEVEL)
DECLARE_ENUM(effect, EFFECT)
DECLARE_ENUM(spell, SPELL)
DECLARE_ENUM(key, KEY)
DECLARE_ENUM(terrain, TERRAIN)
DECLARE_ENUM(hero, HERO)

#undef DECLARE_ENUM

// If we never edit the game def, we can leave it unlocked.
//#define USE_LOCKS

LOCK			glbGamedefLock;

#ifdef USE_LOCKS
#define GAMEDEFLOCK { AUTOLOCK	alock(glbGamedefLock); }
#else
#define GAMEDEFLOCK ;
#endif

void
GAMEDEF::reset(bool loaddefaults)
{
    GAMEDEFLOCK;

#define		RESET_ENUM(name, Name, NAME) \
    for (int i = 0; i < xtra_##name##defs.entries(); i++) \
	delete xtra_##name##defs(i); \
    xtra_##name##defs.clear(); \
 \
    if (loaddefaults) \
    { \
	NAME##_NAMES	name; \
	FOREACH_##NAME(name) \
	{ \
	    NAME##_DEF	*def = new NAME##_DEF; \
	    *def = glb_##name##defs[name]; \
	    xtra_##name##defs.append(def); \
	} \
    }

    RESET_ENUM(mob, Mob, MOB)
    RESET_ENUM(attr, Attr, ATTR)
    RESET_ENUM(tile, Tile, TILE)
    RESET_ENUM(item, Item, ITEM)
    RESET_ENUM(itemclass, Itemclass, ITEMCLASS)
    RESET_ENUM(armourslot, Armourslot, ARMOURSLOT)
    RESET_ENUM(attack, Attack, ATTACK)
    RESET_ENUM(potion, Potion, POTION)
    RESET_ENUM(ring, Ring, RING)
    RESET_ENUM(element, Element, ELEMENT)
    RESET_ENUM(material, Material, MATERIAL)
    RESET_ENUM(gamerules, GameRules, GAMERULES)
    RESET_ENUM(level, Level, LEVEL)
    RESET_ENUM(effect, Effect, EFFECT)
    RESET_ENUM(spell, Spell, SPELL)
    RESET_ENUM(key, Key, KEY)
    RESET_ENUM(terrain, Terrain, TERRAIN)
    RESET_ENUM(hero, Hero, HERO)

#undef RESET_ENUM
}

void
GAMEDEF::load(const char *fname)
{
    GAMEDEFLOCK;

    ifstream		is(fname);
    BUF			buf;

    // Reset.
    if (is)
	reset(false);
    else
	reset(true);

    while (is)
    {
	buf = BUF::readtoken(is);

#define LOAD_ENUM(name, Name, NAME) \
	if (!buf.strcmp(#NAME)) \
	{ \
	    int 	idx = createNew##Name##Def(); \
	    buf = BUF::readtoken(is); \
 \
	    if (!buf.strcmp("{")) \
	    { \
		name##def(idx)->loadData(is); \
	    } \
	}

	LOAD_ENUM(mob, Mob, MOB)
	LOAD_ENUM(attr, Attr, ATTR)
	LOAD_ENUM(tile, Tile, TILE)
	LOAD_ENUM(item, Item, ITEM)
	LOAD_ENUM(itemclass, Itemclass, ITEMCLASS)
	LOAD_ENUM(armourslot, Armourslot, ARMOURSLOT)
	LOAD_ENUM(attack, Attack, ATTACK)
	LOAD_ENUM(potion, Potion, POTION)
	LOAD_ENUM(ring, Ring, RING)
	LOAD_ENUM(element, Element, ELEMENT)
	LOAD_ENUM(material, Material, MATERIAL)
	LOAD_ENUM(gamerules, GameRules, GAMERULES)
	LOAD_ENUM(level, Level, LEVEL)
	LOAD_ENUM(effect, Effect, EFFECT)
	LOAD_ENUM(spell, Spell, SPELL)
	LOAD_ENUM(key, Key, KEY)
	LOAD_ENUM(terrain, Terrain, TERRAIN)
	LOAD_ENUM(hero, Hero, HERO)

#undef LOAD_ENUM
    }
}


void
GAMEDEF::save(const char *fname)
{
    GAMEDEFLOCK;

    ofstream		os(fname);
    BUF			buf;

#define SAVE_ENUM(name, Name, NAME) \
    for (int i = 0; i < getNum##Name(); i++) \
    { \
	os << #NAME << "\n{\n"; \
	name##def(i)->saveData(os); \
	os << "}\n\n"; \
    }

    SAVE_ENUM(mob, Mob, MOB)
    SAVE_ENUM(attr, Attr, ATTR)
    SAVE_ENUM(tile, Tile, TILE)
    SAVE_ENUM(item, Item, ITEM)
    SAVE_ENUM(itemclass, Itemclass, ITEMCLASS)
    SAVE_ENUM(armourslot, Armourslot, ARMOURSLOT)
    SAVE_ENUM(attack, Attack, ATTACK)
    SAVE_ENUM(potion, Potion, POTION)
    SAVE_ENUM(ring, Ring, RING)
    SAVE_ENUM(element, Element, ELEMENT)
    SAVE_ENUM(material, Material, MATERIAL)
    SAVE_ENUM(gamerules, GameRules, GAMERULES)
    SAVE_ENUM(level, Level, LEVEL)
    SAVE_ENUM(effect, Effect, EFFECT)
    SAVE_ENUM(spell, Spell, SPELL)
    SAVE_ENUM(key, Key, KEY)
    SAVE_ENUM(terrain, Terrain, TERRAIN)
    SAVE_ENUM(hero, Hero, HERO)

#undef SAVE_ENUM
}

#define ENUM_ACCESSORS(name, Name, NAME) \
    \
NAME##_NAMES \
GAMEDEF::createNew##Name##Def() \
{ \
    GAMEDEFLOCK; \
 \
    NAME##_DEF	*def; \
    def = new NAME##_DEF; \
    def->reset(); \
    xtra_##name##defs.append(def); \
    return (NAME##_NAMES) (xtra_##name##defs.entries()-1); \
} \
 \
NAME##_DEF * \
GAMEDEF::name##def(int defn) \
{ \
    GAMEDEFLOCK; \
 \
    return xtra_##name##defs(defn); \
} \
 \
int \
GAMEDEF::getNum##Name() \
{ \
    GAMEDEFLOCK; \
 \
    return xtra_##name##defs.entries(); \
}

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

