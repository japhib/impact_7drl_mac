/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        mob.cpp ( Live Once Library, C++ )
 *
 * COMMENTS:
 */

#include "mob.h"

#include "map.h"
#include "msg.h"
#include "text.h"
#include "item.h"
#include "event.h"
#include "display.h"
#include "engine.h"
#include "thesaurus.h"
#include "level.h"

#include <stdio.h>

#include <iostream>
using namespace std;

static int glbTransmuteTaint = 0;

//
// Fireball operators
//
class ATTACK_OP
{
public:
    ATTACK_OP(MOB *src, ITEM *item, ATTACK_NAMES attack)
    {
	mySrc = src;
	myItem = item;
	myAttack = attack;
    }

    void operator()(POS p)
    {
	if (p.mob() && p.mob() != mySrc)
	{
	    if (mySrc->evaluateWillHit(myAttack, myItem, p.mob()))
	    {
		mySrc->formatAndReport("%S %v %O.", GAMEDEF::attackdef(myAttack)->verb, p.mob());
		if (!p.mob()->applyDamage(mySrc, 
			    mySrc->evaluateAttackDamage(myAttack, myItem),
			    (ELEMENT_NAMES) GAMEDEF::attackdef(myAttack)->element,
			    ATTACKSTYLE_RANGE))
		{
		    // Mob still alive.
		    if (p.mob() && GAMEDEF::attackdef(myAttack)->effect != EFFECT_NONE)
		    {
			p.mob()->applyEffect(mySrc,
			    (EFFECT_NAMES) GAMEDEF::attackdef(myAttack)->effect,
			    ATTACKSTYLE_RANGE);
		    }
		}
	    }
	    else
	    {
		mySrc->formatAndReport("%S <miss> %O.", p.mob());
	    }
	}
    }

private:
    ATTACK_NAMES	 myAttack;
    MOB			*mySrc;
    ITEM		*myItem;
};

class POTION_OP
{
public:
    POTION_OP(MOB *src, ITEM_NAMES potion, bool *interest) 
    { mySrc = src; myPotion = potion; myInterest = interest; }

    void operator()(POS p)
    {
	MOB		*mob = p.mob();

	if (mob)
	{
	    if (ITEM::potionEffect(myPotion) != EFFECT_NONE)
	    {
		if (myInterest)
		    *myInterest = true;
	    }
	    mob->applyEffect(mySrc, ITEM::potionEffect(myPotion),
		ATTACKSTYLE_RANGE);
	}
    }

private:
    bool		*myInterest;
    ITEM_NAMES	 	myPotion;
    MOB			*mySrc;
};


//
// MOB Implementation
//

MOB::MOB()
{
    myFleeCount = 0;
    myBoredom = 0;
    myYellHystersis = 0;
    myAIState = 0;
    myHP = 0;
    // Enter the dungeon half fed!
    myFood = getMaxFood() / 2;
    myIsSwallowed = false;
    myNumDeaths = 0;
    myUID = INVALID_UID;
    myNextStackUID = INVALID_UID;

    mySkipNextTurn = false;
    myDelayMob = false;
    myDelayMobIdx = -1;
    myCollisionTarget = 0;
    mySearchPower = 0;
    myAngryWithAvatar = false;

    YELL_NAMES	yell;
    FOREACH_YELL(yell)
    {
	myHeardYell[yell] = false;
	myHeardYellSameRoom[yell] = false;
    }
    mySawMurder = false;
    mySawMeanMurder = false;
    mySawVictory = false;
    myAvatarHasRanged = false;
    myWaiting = false;
    myCowardice = 0;
    myKills = 0;
    myHasWon = false;
    myHasTriedSuicide = false;
    myHero = HERO_NONE;
    myRangeTimeout = 0;
    mySpellTimeout = 0;

    myLastDir = 0;
    myLastDirCount = 0;

    mySeed = 0;

    myLevel = 1;
    myExp = 0;
    myExtraLives = 0;

    myDecoupledFromBody = true;
}

MOB::~MOB()
{
    int		i;

    if (myPos.map() && myPos.map()->avatar() == this)
	myPos.map()->setAvatar(0);

    myPos.removeMob(this);

    for (i = 0; i < myInventory.entries(); i++)
	delete myInventory(i);
}

#define MAX_TAINT		200

void
MOB::initSystem()
{
    glbTransmuteTaint = 0;
}

void
MOB::saveGlobal(ostream &os)
{
    int		val;

    val = glbTransmuteTaint;
    os.write((const char *)&val, sizeof(int));
}

void
MOB::loadGlobal(istream &is)
{
    int		val;

    is.read((char *) &val, sizeof(int));
    glbTransmuteTaint = val;
}

float
MOB::transmuteTaint()
{
    return ((float)glbTransmuteTaint) / MAX_TAINT;
}

void
MOB::incTransmuteTaint(int amount)
{
    glbTransmuteTaint += amount;
}

static u8
mob_findsymbol(const char *name)
{
    // Find letter before space.
    if (!name || !*name)
	return '!';
    const char *idx = &name[strlen(name)-1];

    // Eat trailing spaces (which there shouldn't be any!)
    while (idx > name)
    {
	if (*idx == ' ')
	    idx--;
	else
	    break;
    }

    while (idx > name)
    {
	if (*idx == ' ')
	    return *(idx+1);
	idx--;
    }
    return *name;
}

static EFFECT_NAMES
mob_randeffect()
{
    static EFFECT_NAMES effectlist[7] =
    {
	EFFECT_POTION_SPEED,
	EFFECT_POTION_HEAL,
	EFFECT_POTION_GREATERHEAL,
	EFFECT_POTION_ACIDVULN,
	EFFECT_POTION_ACIDRESIST,
	EFFECT_POTION_POISON,
	EFFECT_POTION_CURE
    };
    return effectlist[rand_choice(7)];
}

static ELEMENT_NAMES
mob_randelementresist()
{
    // Intentionally skip physical as that is too unbalanced both ways.
    ELEMENT_NAMES	elem = (ELEMENT_NAMES) rand_range(ELEMENT_PHYSICAL+1, NUM_ELEMENTS-1);

    // Light needs to be pretty rare as it is very odd.
    if (elem == ELEMENT_LIGHT && rand_chance(30))
	return mob_randelementresist();

    return elem;
}

static ELEMENT_NAMES
mob_randelementattack()
{
    // We already gate to bias to physical in the caller!
    // So only roll for non-physical
    ELEMENT_NAMES	elem = (ELEMENT_NAMES) rand_range(ELEMENT_PHYSICAL+1, NUM_ELEMENTS-1);

    // Light needs to be pretty rare as it is very odd.
    if (elem == ELEMENT_LIGHT && rand_chance(50))
	return mob_randelementattack();

    return elem;
}

static ATTACK_NAMES
mob_randattack(int depth)
{
    ATTACK_NAMES result = GAMEDEF::createNewAttackDef();
    ATTACK_DEF *def = GAMEDEF::attackdef(result);

    def->damage = rand_range(depth, depth*2+depth/2);
    if (depth < 1)
	def->damage = 1;

    def->distribution = (DISTRIBUTION_NAMES) rand_choice(NUM_DISTRIBUTIONS);
    if (rand_chance(30+depth*2))
	def->element = mob_randelementattack();

    if (def->element == ELEMENT_POISON)
    {
	// Apply a poison effect!
	// We transfer damage into turns of poison. We average 1 poison
	// per turn.
	int		duration = def->damage;
	def->damage = (def->damage + 1) / 2;
	EFFECT_NAMES effname = GAMEDEF::createNewEffectDef();
	EFFECT_DEF *eff = GAMEDEF::effectdef(effname);
	eff->type = EFFECTCLASS_POISON;
	eff->element = ELEMENT_POISON;
	eff->duration = duration;
	def->effect = effname;
    }

    def->verb = GAMEDEF::elementdef(def->element)->damageverb;

    return result;
}

static ATTR_NAMES
mob_randattr()
{
    static const ATTR_NAMES attrnames[20] =
    {
	ATTR_GOLD,
	ATTR_YELLOW,
	ATTR_PINK,
	ATTR_PURPLE,
	ATTR_NORMAL,
	ATTR_LIGHTBLACK,
	ATTR_WHITE,
	ATTR_ORANGE,
	ATTR_LIGHTBROWN,
	ATTR_BROWN,
	ATTR_RED,
	ATTR_DKRED,
	ATTR_GREEN,
	ATTR_DKGREEN,
	ATTR_BLUE,
	ATTR_LIGHTBLUE,
	ATTR_TEAL,
	ATTR_CYAN,
	ATTR_FIRE,
	ATTR_DKCYAN
    };

    return attrnames[rand_choice(20)];
}

static ATTR_NAMES
mob_randattrfromname(const char *name)
{
    if (strstr(name, "white") || strstr(name, "arctic") || strstr(name, "snowy"))
	return ATTR_WHITE;
    if (strstr(name, "brown"))
	return ATTR_BROWN;
    if (strstr(name, "red"))
	return ATTR_RED;
    if (strstr(name, "black"))
	return ATTR_LIGHTBLACK;
    if (strstr(name, "gold"))
	return ATTR_GOLD;
    if (strstr(name, "blue"))
	return ATTR_BLUE;
    if (strstr(name, "purple"))
	return ATTR_PURPLE;
    if (strstr(name, "green") || strstr(name, "malachite") || strstr(name, "emerald"))
	return ATTR_GREEN;
    if (strstr(name, "teal"))	//YES, I know!
	return ATTR_TEAL;
    if (strstr(name, "yellow"))
	return ATTR_YELLOW;
    if (strstr(name, "grey"))
	return ATTR_GREY;
    if (strstr(name, "silver"))
	return ATTR_LTGREY;
    if (strstr(name, "olive"))
	return ATTR_DKGREEN;

    return mob_randattr();
}

// These properties make the mob harder
enum MOB_BONUS
{
    BONUS_RANGE,
    BONUS_RANGECOWARD,
    BONUS_CORROSIVE,
    BONUS_TANK,
    BONUS_STRONG,
    BONUS_RESISTANCE,
    BONUS_FLANK,
    BONUS_FAST,
    BONUS_VAMPIRE,
    BONUS_REGENERATE,
    BONUS_SWALLOW,
    BONUS_BREEDER,
    BONUS_THIEF,
    BONUS_LEAP,
    BONUS_DIG,
    NUM_BONUS
};

// These properties make the mob easier/more rewarding
enum MOB_MALUS
{
    MALUS_CORPSEEFFECT,
    MALUS_SLOW,
    MALUS_SHORTSIGHT,
    MALUS_FEEBLE,
    MALUS_PUSHOVER,
    MALUS_PACK,
    MALUS_VULNERABLE,
    MALUS_ORTHOAI,
    NUM_MALUS
};

void
mob_adddescr(MOB_DEF *def, const char *descr)
{
    BUF		buf;

    // Don't double add!
    if (strstr(def->descr, descr))
	return;

    buf.sprintf("%s%s  ", def->descr, descr);
    def->descr = glb_harden(buf);
}

void
mob_applybonus(MOB_DEF *def, MOB_BONUS bonus)
{
    switch (bonus)
    {
	case BONUS_RANGECOWARD:
	    def->ai = AI_RANGECOWARD;
	    // FALL THROUGH to build range attack
	case BONUS_RANGE:
	    def->range_valid = true;
	    def->range_attack = mob_randattack(def->depth - 1 - (bonus == BONUS_RANGECOWARD));
	    def->range_range = rand_range(3, 8);
	    def->range_recharge = rand_range(0, 5);
	    def->range_symbol = rand_chance(50) ? '/' : '*';
	    def->range_attr = mob_randattr();
	    break;
	case BONUS_CORROSIVE:
	    def->corrosiveblood = true;
	    break;
	case BONUS_TANK:
	    def->max_hp += def->max_hp/2;
	    mob_adddescr(def, "Their strong constitution lets them keep fighting when most have collapsed.");
	    break;
	case BONUS_STRONG:
	    def->melee_attack = mob_randattack(def->depth+1);
	    mob_adddescr(def, "The strength of their attacks are not to be underestimated.");
	    break;
	case BONUS_RESISTANCE:
	    def->resistance = mob_randelementresist();
	    break;
	case BONUS_FLANK:
	    def->ai = AI_FLANK;
	    break;
	case BONUS_FAST:
	    def->isfast = true;
	    break;
	case BONUS_VAMPIRE:
	    def->isvampire = true;
	    break;
	case BONUS_REGENERATE:
	    def->isregenerate = true;
	    break;
	case BONUS_SWALLOW:
	    def->swallows = true;
	    break;
	case BONUS_BREEDER:
	    def->breeder = true;
	    break;
	case BONUS_THIEF:
	    def->isthief = true;
	    break;
	case BONUS_LEAP:
	    def->canleap = true;
	    break;
	case BONUS_DIG:
	    def->candig = true;
	    break;
	case NUM_BONUS:
	    break;
    }
}

void
mob_applymalus(MOB_DEF *def, MOB_MALUS malus)
{
    switch (malus)
    {
	case MALUS_CORPSEEFFECT:
	    def->corpsechance = 100;
	    if (def->corpse_effect == EFFECT_NONE)
		mob_adddescr(def, "Their corpses are highly sought after by the medicinal trade.");
	    def->corpse_effect = mob_randeffect();
	    break;
	case MALUS_SLOW:
	    def->isslow = true;
	    break;
	case MALUS_SHORTSIGHT:
	    def->sightrange = 2;
	    break;
	case MALUS_FEEBLE:
	    mob_adddescr(def, "Their attack is not particularly powerful for their kind.");
	    def->melee_attack = mob_randattack(def->depth-1);
	    break;
	case MALUS_PUSHOVER:
	    mob_adddescr(def, "For creatures of its type, they are known to be easy to kill.");
	    def->max_hp /= 2;
	    if (def->max_hp <= 0)
		def->max_hp = 1;
	    break;
	case MALUS_PACK:
	    def->ai = AI_RAT;
	    break;
	case MALUS_VULNERABLE:
	    def->vulnerability = mob_randelementresist();
	    break;
	case MALUS_ORTHOAI:
	    def->ai = AI_ORTHO;
	    break;

	case NUM_MALUS:
	    break;
    }
}

MOB_NAMES glbBossName = MOB_BAEZLBUB;

void
MOB::buildMonsterList()
{
    // We design a bunch of monsters, a few types per level.
    // 2 common, one rare.
    text_shufflenames();
    int		nameidx = 0;
    for (int depth = 1; depth < 10; depth++)
    {
	for (int mid = 0; mid < 3; mid++)
	{
	    MOB_NAMES	mobname = GAMEDEF::createNewMobDef();
	    MOB_DEF *def = GAMEDEF::mobdef(mobname);

	    def->name = text_getname(nameidx++);
	    def->symbol = mob_findsymbol(def->name);
	    def->attr = mob_randattrfromname(def->name);
	    def->corpsechance = 33;

	    def->depth = depth;
	    def->sightrange = 10+depth;
	    if (!mid)
		def->rarity = 20;

	    int effectivedepth = (depth+1)/2;

	    // Standard abilities:
	    def->melee_attack = mob_randattack(depth);
	    def->max_hp = rand_range( 3*depth, 4*depth );

	    int		malus = rand_range(0, 2);
	    malus = rand_range(0, malus);
	    int		bonus = rand_range(0, MIN(effectivedepth - 1 + malus, 5));
	    bonus = rand_range(0, bonus);

	    // Apply bonus, then malus, as we just overwrite for things
	    // like damage.
	    for (int b = 0; b < bonus; b++)
		mob_applybonus(def, (MOB_BONUS) rand_choice(NUM_BONUS));
	    for (int m = 0; m < malus; m++)
		mob_applymalus(def, (MOB_MALUS) rand_choice(NUM_MALUS));
	}
    }

    // Create the boss!
    int bossdepth = GAMEDEF::rules().bosslevel;
    glbBossName = GAMEDEF::createNewMobDef();
    MOB_DEF *def = GAMEDEF::mobdef(glbBossName);

    def->name = text_getname(nameidx++);
    // Make it upper case so we don't have to memorize!
    def->symbol = toupper(mob_findsymbol(def->name));
    def->attr = mob_randattr();

    def->depth = bossdepth;
    def->rarity = 0;

    // Standard abilities:
    int effectivedepth = (bossdepth+1)/2;

    // Standard abilities:
    def->melee_attack = mob_randattack(bossdepth);
    def->max_hp = rand_range( 7*bossdepth, 10*bossdepth );

    mob_applybonus(def, (MOB_BONUS) rand_choice(NUM_BONUS));
    mob_applybonus(def, BONUS_RANGE);
    mob_applybonus(def, BONUS_STRONG);
    mob_applybonus(def, BONUS_RESISTANCE);
    mob_applymalus(def, MALUS_VULNERABLE);	// Some way..
    def->ai = AI_PATHTO;
}

MOB *
MOB::create(MOB_NAMES def)
{
    MOB		*mob;

    mob = new MOB();

    mob->myDefinition = def;

    mob->myHP = defn(def).max_hp;
    mob->myMP = defn(def).max_mp;

    mob->mySeed = rand_int();

    // We don't currently support this!
    J_ASSERT(!mob->defn().swallows);

    return mob;
}

MOB *
MOB::copy() const
{
    MOB		*mob;

    mob = new MOB();
    
    *mob = *this;

    // Copy inventory one item at a time.
    // We are careful to maintain the same list order here so restoring
    // won't shuffle things unexpectadly
    mob->myInventory.clear();
    for (int i = 0; i < myInventory.entries(); i++)
    {
	mob->myInventory.append(myInventory(i)->copy());
    }
    
    return mob;
}

void
MOB::setPos(POS pos)
{
    pos.moveMob(this, myPos);
    myPos = pos;
}


void
MOB::setMap(MAP *map)
{
    myPos.setMap(map);
    myTarget.setMap(map);
    myHome.setMap(map);
    myMeditatePos.setMap(map);
    for (int i = 0; i < myInventory.entries(); i++)
    {
	if (myInventory(i))
	    myInventory(i)->setMap(map);
    }
}

void
MOB::clearAllPos()
{
    myPos = POS();
    myTarget = POS();
    myHome = POS();
    myMeditatePos = POS();
}

MOB *
MOB::createNPC(int difficulty)
{
    int		i;
    MOB_NAMES	mob = MOB_NONE;
    int		choice = 0;
    int		matchingdepth = 0;
    MOB		*m;

    // Given the letter choice, choose a mob that matches it appropriately.
    choice = 0;
    for (i = 0; i < GAMEDEF::getNumMob(); i++)
    {
	// depth 0 isn't generated.
	if (!defn(i).depth)
	    continue;
	// Use the baseletter to bias the creation.
	if (defn(i).depth <= difficulty)
	{
	    if (rand_choice(choice + defn(i).rarity) < defn(i).rarity)
		mob = (MOB_NAMES) i;
	    choice += defn(i).rarity;
	}
    }

    if (mob == MOB_NONE)
    {
	// Welp, we create a new one!
	return 0;
    }

    // Testing..
    m = MOB::create(mob);

    if (m)
    {
	// Set the base level.  This will be near the official level.
	int moblevel = rand_range(difficulty-2, difficulty+2);
	moblevel = MIN(moblevel, LEVEL::MAX_LEVEL);
	m->gainExp(LEVEL::expFromLevel(moblevel), /*silent=*/true);
    }

    if (0)
    {
	ITEM *item = ITEM::createRandom(difficulty);
	if (item)
	    m->addItem(item);
    }

    return m;
}

MOB *
MOB::createHero(HERO_NAMES pc)
{
    MOB_NAMES	mob = MOB_AVATAR;

    MOB *m = MOB::create(mob);

    m->myHero = pc;

    // Rest hp as it will change after assigning hero.
    m->myHP = m->getMaxHP();
    m->myMP = 0;
    //Testing
    m->myMP = m->getMaxMP();

    // Always start with a starter weapon
    ITEM_NAMES item = ITEM_NONE;
    switch ((WEAPONCLASS_NAMES) m->hero().weaponclass)
    {
	case WEAPONCLASS_NONE:
	case NUM_WEAPONCLASSS:
	    break;
	
	case WEAPONCLASS_EDGE:
	    item = ITEM_SLASH_WEAPON;
	    break;
	case WEAPONCLASS_PIERCE:
	    item = ITEM_PIERCE_WEAPON;
	    break;
	case WEAPONCLASS_BLUNT:
	    item = ITEM_BLUNT_WEAPON;
	    break;
	case WEAPONCLASS_RANGE:
	    item = ITEM_RANGE_WEAPON;
	    break;
    }

    if (item != ITEM_NONE)
    {
	ITEM *i = ITEM::create(item, 1);
	if (i)
	{
	    m->addItem(i);
	    i->setEquipped(true);
	}

#if 0
	i = ITEM::create(ITEM_PORTAL_STONE, 1);
	m->addItem(i);
	i = ITEM::create(ITEM_WISH_STONE, 1);
	m->addItem(i);
	i = ITEM::create(ITEM_MACGUFFIN, 1);
	m->addItem(i);
#endif
    }

    return m;
}

void
MOB::getLook(u8 &symbol, ATTR_NAMES &attr) const
{
    symbol = defn().symbol;
    attr = (ATTR_NAMES) defn().attr;

    if (getHero() != HERO_NONE)
	attr = (ATTR_NAMES) hero().attr;

    if (!alive())
    {
	// Dead creatures override their attribute to blood
	attr = ATTR_RED;
    }
}

BUF
MOB::getName() const
{
    BUF		result;

    if (getHero() != HERO_NONE)
    {
	result.reference(hero().name);
    }
    else if (isAvatar())
	result.reference("you");
    else if (myName.isstring())
	result = myName;
    else
    {
	result = thesaurus_expand(mySeed, getDefinition());
    }

    return result;
}

BUF
MOB::getRawName() const
{
    BUF		result;
    // This still falls through to the hero as that is the actual
    // template name.
    if (getHero() != HERO_NONE)
    {
	result.reference(hero().name);
    }
    else
	result.reference(defn().name);
    return result;
}

void
MOB::setupTextVariables() const
{
    BUF		tmp;

    tmp.sprintf("%s%s", gram_getarticle(getName()), getName().buffer());
    text_registervariable("A_MOBNAME", tmp);
    text_registervariable("MOBNAME", getName());
    text_registervariable("MOBNAMES", gram_makepossessive(getName()));
}

MOB_NAMES
MOB::getBossName()
{
    return glbBossName;
}

BUF
MOB::buildSpellDescr(SPELL_NAMES spell) const
{
    BUF		stats, descr;

    SPELL_DEF		*def = GAMEDEF::spelldef(spell);

    stats.sprintf("Spell: %s (%s)\n", def->name, def->verb);
    descr.strcat(stats);
    if (def->timeout)
    {
	stats.sprintf("Timeout: %d  ", def->timeout);
	descr.strcat(stats);
    }
    if (def->mana || def->reqfullmana)
    {
	stats.sprintf("Mana: %d  ", def->reqfullmana ? getMaxMP() : def->mana);
	descr.strcat(stats);
    }
    if (def->radius > 1)
    {
	stats.sprintf("Radius: %d  ", def->radius);
	descr.strcat(stats);
    }

    if (def->piercing)
    {
	descr.strcat("Piercing ");
    }

    if (def->blast)
    {
	stats.sprintf("Close Blast\n");
	descr.strcat(stats);
    }
    else if (def->range > 1)
    {
	stats.sprintf("Range: %d\n", def->range);
	descr.strcat(stats);
    }
    else
    {
	descr.strcat("\n");
    }

    if (!def->needsdir)
	descr.strcat("Targets Self\n");

    stats = buildEffectDescr((EFFECT_NAMES) def->effect);
    if (stats.isstring())
    {
	descr.strcat("Effect: ");
	descr.strcat(stats);
	descr.strcat("\n");
    }

    return descr;
}

BUF
MOB::buildAttackDescr(ATTACK_NAMES attack) const
{
    BUF		stats, descr;

    ATTACK_DEF		*def = GAMEDEF::attackdef(attack);
    stats.sprintf("Attack: %s (%s)\n", def->noun, def->verb);
    descr.strcat(stats);
    stats.sprintf("   Damage: %d (%s)\n", LEVEL::boostDamage(def->damage, getLevel()), glb_distributiondefs[def->distribution].name);
    descr.strcat(stats);
    if (def->chancebonus)
    {
	stats.sprintf("   Bonus to Hit: %d\n", def->chancebonus);
	descr.strcat(stats);
    }
    if (def->element != ELEMENT_PHYSICAL)
    {
	stats.sprintf("   Element: %s\n", GAMEDEF::elementdef(def->element)->name);
	descr.strcat(stats);
    }
    stats = buildEffectDescr((EFFECT_NAMES) def->effect);
    if (stats.isstring())
    {
	descr.strcat("   Effect: ");
	descr.strcat(stats);
	descr.append('\n');
    }

    return descr;
}

BUF
MOB::buildSpecialDescr(MOB_NAMES mob) const
{
    BUF		descr, stats;

    descr.strcat(defn(mob).descr);

    if (defn(mob).resistance != ELEMENT_NONE)
    {
	stats.sprintf("They are unaffected by %s.  ", GAMEDEF::elementdef(defn(mob).resistance)->name);
	descr.strcat(stats);
    }
    if (defn(mob).vulnerability != ELEMENT_NONE)
    {
	stats.sprintf("They are particularly sensitive to %s.  ", GAMEDEF::elementdef(defn(mob).vulnerability)->name);
	descr.strcat(stats);
    }

    if (defn(mob).isslow)
    {
	descr.strcat("They move with a slow plodding pace.  ");
    }
    if (defn(mob).isfast)
    {
	descr.strcat("They move with shocking speed.  ");
    }
    if (defn(mob).isvampire)
    {
	descr.strcat("They drink the blood of their foes, gaining energy therefrom.  ");
    }
    if (defn(mob).isregenerate)
    {
	descr.strcat("They heal wounds with astonishing speed.  ");
    }
    if (defn(mob).candig)
    {
	descr.strcat("They can dig through solid rock.  ");
    }
    if (defn(mob).swallows)
    {
	descr.strcat("They can swallow their foes whole.  ");
    }
    if (defn(mob).breeder)
    {
	descr.strcat("The rate of their reproduction leaves rabbits embarrassed.  ");
    }
    if (defn(mob).isthief)
    {
	descr.strcat("They steal interesting objects.  ");
    }
    if (defn(mob).canleap)
    {
	descr.strcat("They can leap long distances.  ");
    }
    if (defn(mob).corrosiveblood)
    {
	descr.strcat("Their corrosive blood will damage weapons that strike them.  ");
    }
    if (defn(mob).sightrange < 5)
    {
	descr.strcat("They are known to be short-sighted.  ");
    }

    return descr;
}

BUF
MOB::getLongDescription() const
{
    BUF		descr, detail;
    BUF		stats, buf;

    descr.strcpy(gram_capitalize(getName()));
    stats.sprintf(" - Health: %d / %d", getHP(), getMaxHP());
    descr.strcat(stats);
    if (getMaxMP())
    {
	stats.sprintf("  Mana: %d / %d", getMP(), getMaxMP());
	descr.strcat(stats);
    }
    descr.strcat("\n");

    {
	BUF		text = text_lookup("mob", getRawName());
	descr.strcat("\n");
	text.stripTrailingWS();
	descr.strcat(text);
	descr.strcat("\n");
    }

    if (getHero() == HERO_NONE)
    {
	const char *rarity = "a unique creature";
	if (defn().rarity > 0)
	{
	    if (defn().rarity >= 100)
		rarity = "a common creature";
	    else
		rarity = "a rare creature";
	}
	stats.sprintf("\nThe %s is %s.  %s  ",
		getName().buffer(), rarity, glb_aidefs[getAI()].descr);

	descr.strcat(stats);

	stats = buildSpecialDescr(getDefinition());
	descr.strcat(stats);
	descr.strcat("\n");
    }
#if 0
    if (isAvatar())
    {
	stats.sprintf("Food: %d / %d\n", getFood(), getMaxFood());
	descr.strcat(stats);
    }
#endif

    int		reduction = getDamageReduction(ELEMENT_PHYSICAL);
    if (reduction)
    {
	stats.sprintf("Damage Reduction: %d", reduction);
	descr.strcat(stats);
    }
    if (defn().dodgebonus)
    {
	if (reduction)
	    descr.strcat("; ");
	stats.sprintf("Dodge Bonus: %d", defn().dodgebonus);
	descr.strcat(stats);
    }
    if (defn().dodgebonus || reduction)
	descr.append('\n');

    if (getMeleeAttack() != ATTACK_NONE)
    {
	descr.append('\n');
	ATTACK_DEF		*def = GAMEDEF::attackdef(getMeleeAttack());
	stats.sprintf("They %s their foes for %s %d damage.",
			def->verb, 
			glb_distributiondefs[def->distribution].descr,
			getMeleeCanonicalDamage());
	descr.strcat(stats);
    }

    if ((defn().range_valid && defn().range_range) || lookupWand())
    {
	stats.sprintf("\n\nThey can also attack at a up to %d distance, every %d turns.  ", getRangedRange(), defn().range_recharge);
	descr.strcat(stats);

	ATTACK_DEF		*def = GAMEDEF::attackdef(getRangedAttack());
	stats.sprintf("They %s their foes for %s %d damage.",
			def->verb, 
			glb_distributiondefs[def->distribution].descr,
			getRangedCanonicalDamage());
	descr.strcat(stats);
    }

    descr.append('\n');

    // For heros provide their skill and burst
    if (getHero() != HERO_NONE)
    {
	BUF	spelldescr;

	spelldescr.sprintf("They are skilled with %s weapons.\n",
		glb_weaponclassdefs[hero().weaponclass].name);
	descr.strcat("\n");
	descr.strcat(spelldescr);

	descr.strcat("\n");
	spelldescr = buildSpellDescr((SPELL_NAMES) hero().skill);
	stats.sprintf("[E] Skill %s\n", spelldescr.buffer());
	descr.strcat(stats);

	spelldescr = buildSpellDescr((SPELL_NAMES) hero().burst);
	stats.sprintf("[Q] Burst %s\n", spelldescr.buffer());
	descr.strcat(stats);
    }

    // If not the avatar, describe any items.
    if (myInventory.entries())
    {
	descr.strcat("Intrinsics: ");
	int		any = 0;
	for (int i = 0; i < myInventory.entries(); i++)
	{
	    if (!myInventory(i)->isFlag())
		continue;
	    BUF		 name = myInventory(i)->getName();
	    if (any)
		descr.strcat(", ");
	    any++;
	    descr.strcat(name);
	}
	descr.strcat("\n");
    }

    buf.sprintf("\nLevel: %d   Exp: %d / %d\n",
	    getLevel(), getExp(), LEVEL::expFromLevel(getLevel()+1));
    descr.strcat(buf);
    if (getExtraLives() > 0)
    {
	buf.sprintf("They have %d extra lives.\n", getExtraLives());
	descr.strcat(buf);
    }

    return descr;
}

ITEM *
MOB::getRandomItem(bool allowequipped) const
{
    ITEM	*result;
    int		choice = 0;

    result = 0;

    if (myInventory.entries())
    {
	// I hope I was drunk when I did multi-sample here?
	// Admittedly, this is the only code which has generated hate
	// mail, so it must be doing something right!
	int		tries = 10;
	while (tries--)
	{
	    choice = rand_choice(myInventory.entries());

	    // Ignore flags
	    if (myInventory(choice)->isFlag())
		continue;

	    // Ignore equipped if asked
	    if (myInventory(choice)->isEquipped() && !allowequipped)
		continue;

	    return myInventory(choice);
	}
    }
    return result;
}

void
MOB::stealNonEquipped(MOB *src)
{
    // NO-ops
    if (!src || src == this)
	return;

    for (int i = src->myInventory.entries(); i --> 0;)
    {
	ITEM *item = src->myInventory(i);

	if (item->isEquipped())
	    continue;

	if (item->isFlag())
	    continue;

	src->removeItem(item, true);
	addItem(item);
    }
}

bool
MOB::hasItem(ITEM_NAMES itemname) const
{
    if (lookupItem(itemname))
	return true;

    return false;
}

ITEM *
MOB::giftItem(ITEM_NAMES itemname)
{
    ITEM		*item;

    item = ITEM::create(itemname);

    addItem(item);

    // Allow for stacking!
    item = lookupItem(itemname);

    return item;
}

ITEM *
MOB::lookupWeapon() const
{
    for (int i = 0; i < myInventory.entries(); i++)
    {
	if (myInventory(i)->isWeapon() && myInventory(i)->isEquipped())
	    return myInventory(i);
    }
    return 0;
}

int
MOB::getDamageReduction(ELEMENT_NAMES element) const
{
    int		reduction = defn().damagereduction;
    ITEM	*armour = lookupArmour();
    if (armour)
    {
	reduction += LEVEL::boostResist(armour->getDamageReduction(), armour->getLevel());
    }

    if (element != ELEMENT_PHYSICAL)
	reduction = 0;

    ITEM *ring = lookupRing();
    if (ring)
    {
	RING_NAMES	 ringname = (RING_NAMES) ring->getMagicClass();
	if (ringname != RING_NONE)
	{
	    if (GAMEDEF::ringdef(ringname)->resist == element)
	    {
		// TODO: Scale by item level.
		reduction += LEVEL::boostResist(GAMEDEF::ringdef(ringname)->resist_amt, ring->getLevel());
		// Identify.
		ring->markMagicClassKnown();
	    }
	}
    }

    return reduction;
}

ITEM *
MOB::lookupArmour() const
{
    for (int i = 0; i < myInventory.entries(); i++)
    {
	if (myInventory(i)->isArmour() && myInventory(i)->isEquipped())
	    return myInventory(i);
    }
    return 0;
}

ITEM *
MOB::lookupWand() const
{
    for (int i = 0; i < myInventory.entries(); i++)
    {
	if (myInventory(i)->isRanged() && myInventory(i)->isEquipped())
	    return myInventory(i);
	if (myInventory(i)->isWeapon() && myInventory(i)->defn().weaponclass == WEAPONCLASS_RANGE && myInventory(i)->isEquipped())
	    return myInventory(i);
    }
    return 0;
}

ITEM *
MOB::lookupItem(ITEM_NAMES itemname) const
{
    int		i;

    for (i = 0; i < myInventory.entries(); i++)
    {
	if (myInventory(i)->getDefinition() == itemname)
	    return myInventory(i);
    }
    return 0;
}

ITEM *
MOB::lookupItem(ITEM_NAMES itemname, MATERIAL_NAMES materialname) const
{
    int		i;

    for (i = 0; i < myInventory.entries(); i++)
    {
	if (myInventory(i)->getDefinition() == itemname)
	{
	    if (myInventory(i)->material() == materialname)
		return myInventory(i);
	}
    }
    return 0;
}

ITEM *
MOB::lookupRing() const
{
    for (int i = 0; i < myInventory.entries(); i++)
    {
	if (myInventory(i)->isRing() && myInventory(i)->isEquipped())
	    return myInventory(i);
    }
    return 0;
}

RING_NAMES
MOB::lookupRingName() const
{
    ITEM		*ring = lookupRing();
    if (!ring)
	return RING_NONE;

    return (RING_NAMES) ring->getMagicClass();
}

void
MOB::stopRunning()
{
    if (hasItem(ITEM_QUICKBOOST))
	removeItem(lookupItem(ITEM_QUICKBOOST));
    myLastDirCount = 0;
}

bool
MOB::canMoveDir(int dx, int dy, bool checkmob) const
{
    POS		goal;

    goal = myPos.delta(dx, dy);

    return canMove(goal, checkmob);
}

bool
MOB::canMove(POS pos, bool checkmob) const
{
    if (!pos.valid())
	return false;

    if (!pos.isPassable())
    {
	if (pos.defn().isphaseable && defn().passwall)
	{
	    // Can move through it...
	}
	else if (pos.defn().isdiggable && defn().candig)
	{
	    // Could dig through it.
	}
	else
	{
	    // Failed...
	    return false;
	}
    }

    if (checkmob && pos.mob())
	return false;

    return true;
}

void
MOB::move(POS newpos, bool ignoreangle)
{
    // If we have swallowed something, move it too.
    if (!isSwallowed())
    {
	PTRLIST<MOB *>	moblist;
	int		i;

	pos().getAllMobs(moblist);
	for (i = 0; i < moblist.entries(); i++)
	{
	    if (moblist(i)->isSwallowed())
	    {
		moblist(i)->move(newpos, true);
	    }
	}
    }

    if (ignoreangle)
	newpos.setAngle(pos().angle());

    // Check if we are entering a new room...
    if (isAvatar() && (newpos.roomId() != pos().roomId()))
    {
	if (newpos.map())
	{
	    newpos.map()->expandActiveRooms(newpos.roomId(), 2);
	}
    }

    setPos(newpos);

    reportSquare(pos());
}

int
MOB::getFood() const
{
    return myFood;
}

void
MOB::gainFood(int food)
{
    myFood += food;
    // We allow ourselves to be oversatiated!
}

void
MOB::eatFood(int food)
{
    myFood -= food;
    if (myFood < 0)
    {
	myFood = 0;
    }
}

int
MOB::getMaxFood() const
{
    return 200;
}

int
MOB::getMaxHP() const
{
    int	max_hp = defn().max_hp;
    if (getDefinition() == MOB_AVATAR)
	max_hp =  hero().max_hp;
    return LEVEL::boostHP(max_hp, getLevel());;
}

void
MOB::gainPartyHP(int hp)
{
    if (!isAvatar())
	gainHP(hp);

    auto && party = pos().map()->party();
    for (int idx = 0; idx < MAX_PARTY; idx++)
    {
	if (party.member(idx))
	    party.member(idx)->gainHP(hp);
    }
}

void
MOB::gainHP(int hp)
{
    int		maxhp;

    maxhp = getMaxHP();

    if (hasItem(ITEM_PLAGUE))
    {
	// Do not heal as well when sick.
	if (hp > 0)
	    hp = (hp + 1) / 2;
    }

    myHP += hp;
    if (myHP > maxhp)
	myHP = maxhp;
    if (myHP < 0)
	myHP = 0;
}

int
MOB::getMaxMP() const
{
    if (getDefinition() == MOB_AVATAR)
	return hero().max_mp;
    return defn().max_mp;
}

void
MOB::gainMP(int mp)
{
    int		maxmp;

    maxmp = getMaxMP();

    myMP += mp;
    if (myMP > maxmp)
	myMP = maxmp;
    if (myMP < 0)
	myMP = 0;
}

void
MOB::gainPartyMP(int hp, ELEMENT_NAMES element)
{
    if (!isAvatar())
	gainMP(hp);

    auto && party = pos().map()->party();
    for (int idx = 0; idx < MAX_PARTY; idx++)
    {
	if (party.member(idx))
	{
	    // Bonus gain for matching element.
	    if (party.member(idx)->hero().element == element)
		party.member(idx)->gainMP(hp/3);
	    party.member(idx)->gainMP(hp);
	}
    }
}

void
MOB::postEvent(EVENTTYPE_NAMES type, u8 sym, ATTR_NAMES attr, const char *text) const
{
    if (pos().map())
	pos().map()->getDisplay()->queue().append(EVENT((MOB *)this, sym, attr, type, text));
}

void
MOB::doEmote(const char *text)
{
    postEvent( (EVENTTYPE_NAMES) (EVENTTYPE_SHOUT | EVENTTYPE_LONG),
		    ' ', ATTR_EMOTE,
		    text);
}

void
MOB::doShout(const char *text)
{
    postEvent( (EVENTTYPE_NAMES) (EVENTTYPE_SHOUT | EVENTTYPE_LONG),
		    ' ', ATTR_SHOUT,
		    text);
}

BUF
MOB::buildEffectDescr(EFFECT_NAMES effect) const
{
    BUF		descr;
    EFFECT_DEF	*def = GAMEDEF::effectdef(effect);

    if (effect == EFFECT_NONE)
	return descr;
    switch ((EFFECTCLASS_NAMES) def->type)
    {
	case NUM_EFFECTCLASSS:
	case EFFECTCLASS_NONE:
	    break;
	case EFFECTCLASS_HEAL:
	    descr.sprintf("Heals %s%d (%s).  ",
			    def->affectparty ? "party " : "",
			    LEVEL::boostHeal(def->amount, getLevel()),
			    glb_distributiondefs[def->distribution].name);
	    break;
	case EFFECTCLASS_GAINMANA:
	    descr.sprintf("Regains %d (%s) mana.  ",
			    LEVEL::boostHeal(def->amount, getLevel()),
			    glb_distributiondefs[def->distribution].name);
	    break;
	case EFFECTCLASS_LOSEMANA:
	    descr.sprintf("Drains %d (%s) mana.  ",
			    LEVEL::boostDamage(def->amount, getLevel()),
			    glb_distributiondefs[def->distribution].name);
	    break;
	case EFFECTCLASS_GAINFOOD:
	    descr.sprintf("Creates %d (%s) food.  ",
			    LEVEL::boostHeal(def->amount, getLevel()),
			    glb_distributiondefs[def->distribution].name);
	    break;
	case EFFECTCLASS_LOSEFOOD:
	    descr.sprintf("Consumes %d (%s) food.  ",
			    LEVEL::boostDamage(def->amount, getLevel()),
			    glb_distributiondefs[def->distribution].name);
	    break;
	case EFFECTCLASS_POISON:
	    descr.sprintf("Poisons %sfor %d turns.  ",
			    def->affectparty ? "party " : "",
			    LEVEL::boostDuration(def->duration, getLevel()));
	    break;
	case EFFECTCLASS_CURE:
	    descr.sprintf("Cures poison%s.  ",
			    def->affectparty ? " on party" : "");
	    break;
	case EFFECTCLASS_DAMAGE:
	    descr.sprintf("Causes %d %s damage %s(%s).  ",
			LEVEL::boostDamage(def->amount, getLevel()),
			GAMEDEF::elementdef(def->element)->name,
			def->affectparty ? "to party " : "",
			glb_distributiondefs[def->distribution].name);
	    break;
	case EFFECTCLASS_RESIST:
	    descr.sprintf("Grant %sresist %s for %d turns.  ",
			def->affectparty ? "party " : "",
			GAMEDEF::elementdef(def->element)->name,
			LEVEL::boostDuration(def->duration, getLevel()));
	    break;
	case EFFECTCLASS_VULNERABLE:
	    descr.sprintf("Grant %svulnerable to %s for %d turns.  ",
			def->affectparty ? "party " : "",
			GAMEDEF::elementdef(def->element)->name,
			LEVEL::boostDuration(def->duration, getLevel()));
	    break;
	case EFFECTCLASS_GAINITEM:
	    descr.sprintf("Grants %s%s for %d turns.  ",
			    def->affectparty ? "party " : "",
			    GAMEDEF::itemdef(def->itemflag)->name,
			    LEVEL::boostDuration(def->duration, getLevel()));
	    break;
    }

    if (def->next != EFFECT_NONE)
    {
	BUF furtherdescr = buildEffectDescr((EFFECT_NAMES) def->next);

	BUF total;
	total.sprintf("%s%s", descr.buffer(), furtherdescr.buffer());
	descr = total;
    }

    return descr;
}

bool
MOB::applyEffect(MOB *src, EFFECT_NAMES effect, ATTACKSTYLE_NAMES attackstyle)
{
    if (effect == EFFECT_NONE)
	return false;

    EFFECT_DEF	*def = GAMEDEF::effectdef(effect);

    switch ((EFFECTCLASS_NAMES) def->type)
    {
	case NUM_EFFECTCLASSS:
	case EFFECTCLASS_NONE:
	    break;

	case EFFECTCLASS_HEAL:
	{
	    BUF		healstring;
	    int		heal = def->amount;
	    
	    if (src)
		heal = LEVEL::boostHeal(heal, src->getLevel());
	    
	    heal = evaluateDistribution(heal, (DISTRIBUTION_NAMES) def->distribution);

	    // heal = MIN(heal, getMaxHP()-getHP());

	    healstring.sprintf("%%S <heal> %d damage.", heal);
	    formatAndReport(healstring);
	    if (def->affectparty)
		gainPartyHP(heal);
	    else
		gainHP(heal);
	    break;
	}

	case EFFECTCLASS_GAINMANA:
	{
	    BUF		healstring;
	    int		heal = def->amount;
	    if (src)
		heal = LEVEL::boostHeal(heal, src->getLevel());
	    heal = evaluateDistribution(heal, (DISTRIBUTION_NAMES) def->distribution);

	    healstring.sprintf("%%S <regain> %d mana.", heal);
	    formatAndReport(healstring);
	    if (def->affectparty)
		gainPartyMP(heal, (ELEMENT_NAMES)def->element);
	    else
		gainMP(heal);
	    break;
	}

	case EFFECTCLASS_LOSEMANA:
	{
	    BUF		healstring;
	    int		heal = def->amount;
	    if (src)
		heal = LEVEL::boostDamage(heal, src->getLevel());
	    heal = evaluateDistribution(heal, (DISTRIBUTION_NAMES) def->distribution);
	    healstring.sprintf("%%S <be> drained of %d mana.", heal);
	    formatAndReport(healstring);
	    if (def->affectparty)
		gainPartyMP(-heal, (ELEMENT_NAMES)def->element);
	    else
		gainMP(-heal);
	    break;
	}

	case EFFECTCLASS_GAINFOOD:
	{
	    BUF		healstring;
	    int		heal = def->amount;
	    if (src)
		heal = LEVEL::boostHeal(heal, src->getLevel());
	    heal = evaluateDistribution(heal, (DISTRIBUTION_NAMES) def->distribution);

	    heal = MIN(heal, getMaxFood()-getFood());

	    healstring.sprintf("%%S <gain> %d food.", heal);
	    formatAndReport(healstring);
	    gainFood(heal);
	    break;
	}

	case EFFECTCLASS_LOSEFOOD:
	{
	    BUF		healstring;
	    int		heal = def->amount;
	    if (src)
		heal = LEVEL::boostDamage(heal, src->getLevel());
	    heal = evaluateDistribution(heal, (DISTRIBUTION_NAMES) def->distribution);

	    heal = MIN(heal, getFood());

	    healstring.sprintf("%%S <be> drained of %d food.", heal);
	    formatAndReport(healstring);
	    eatFood(heal);
	    break;
	}

	case EFFECTCLASS_CURE:
	{
	    if (!def->affectparty || !isAvatar())
	    {
		ITEM		*item = lookupItem(ITEM_POISON);

		if (!item)
		{
		    formatAndReport("%S <feel> healthy.");
		}
		else
		{
		    removeItem(item);
		    delete item;
		}
	    }
	    else
	    {
		for (int idx = 0; idx < MAX_PARTY; idx++)
		{
		    MOB *curee = pos().map()->party().member(idx);
		    if (!curee)
			continue;
		    ITEM		*item = curee->lookupItem(ITEM_POISON);

		    if (!item)
		    {
			curee->formatAndReport("%S <feel> healthy.");
		    }
		    else
		    {
			curee->removeItem(item);
			delete item;
		    }
		}
	    }
	    break;
	}

	case EFFECTCLASS_DAMAGE:
	{
	    int damage = def->amount;
	    if (src)
		damage = LEVEL::boostDamage(damage, src->getLevel());
	    damage = evaluateDistribution(damage, (DISTRIBUTION_NAMES) def->distribution);
	    if (src)
	    {
		src->formatAndReport("%S %v %O.", GAMEDEF::elementdef(def->element)->damageverb, this);
	    }
	    else
	    {
		formatAndReport("%S <be> %V.", GAMEDEF::elementdef(def->element)->damageverb, this);
	    }
	    if (applyDamage(src, damage, (ELEMENT_NAMES) def->element, attackstyle))
		return true;
	    break;
	}

	case EFFECTCLASS_RESIST:
	case EFFECTCLASS_VULNERABLE:
	case EFFECTCLASS_GAINITEM:
	case EFFECTCLASS_POISON:
	{
	    ITEM_NAMES		 itemname;
	    // Probably should have all been GAINITEM to begin with!
	    if (def->type == EFFECTCLASS_RESIST)
		itemname = ITEM_RESIST;
	    else if (def->type == EFFECTCLASS_VULNERABLE)
		itemname = ITEM_VULNERABLE;
	    else if (def->type == EFFECTCLASS_GAINITEM)
		itemname = (ITEM_NAMES) def->itemflag;
	    else if (def->type == EFFECTCLASS_POISON)
		itemname = ITEM_POISON;
	    else
	    {
		J_ASSERT(!"Unhandled effect class");
		break;
	    }

	    if (!def->affectparty || !isAvatar())
	    {
		ITEM		*item = ITEM::create(itemname);

		int		duration = def->duration;
		if (src)
		    duration = LEVEL::boostDuration(duration, src->getLevel());
		item->setTimer(duration);
		item->setElement((ELEMENT_NAMES) def->element);

		addItem(item);
	    }
	    else
	    {
		for (int idx = 0; idx < MAX_PARTY; idx++)
		{
		    MOB *curee = pos().map()->party().member(idx);
		    if (!curee)
			continue;
		    ITEM		*item = ITEM::create(itemname);

		    int		duration = def->duration;
		    if (src)
			duration = LEVEL::boostDuration(duration, src->getLevel());
		    item->setTimer(duration);
		    item->setElement((ELEMENT_NAMES) def->element);

		    curee->addItem(item);
		}
	    }
	    break;
	}
    }

    // Chain the effect (still alive as we'd return if we died)
    if (def->next != EFFECT_NONE)
    {
	return applyEffect(src, (EFFECT_NAMES) def->next, attackstyle);
    }

    return false;
}

bool
MOB::isVulnerable(ELEMENT_NAMES element) const
{
    if (defn().vulnerability == element)
	return true;

    // Check any items...
    for (int i = 0; i < myInventory.entries(); i++)
    {
	ITEM	*item = myInventory(i);
	if (item->getDefinition() == ITEM_VULNERABLE)
	{
	    if (item->element() == element)
		return true;
	}
    }
    return false;
}

bool
MOB::applyDamage(MOB *src, int hits, ELEMENT_NAMES element, ATTACKSTYLE_NAMES attackstyle, bool silent)
{
    // Being hit isn't boring.
    if (!silent)
    {
	clearBoredom();
	stopWaiting("%S <be> hit, so <end> %r rest.");
    }

    if (hasItem(ITEM_INVULNERABLE))
	return false;

    if (element == ELEMENT_LIGHT && hasItem(ITEM_BLIND))
    {
	return false;
    }

    if (src && src->isAvatar() && src != this && !myAngryWithAvatar)
    {
	if (getAI() == AI_PEACEFUL)
	{
	    formatAndReport("%S <grow> angry with %o!", src);
	    myAngryWithAvatar = true;
	}
    }

    // Adjust damage down for armour.
    float		 multiplier = 1;
    int			 reduction = 0;

    reduction = getDamageReduction(element);

    // No armour if the attack came from within.
    // But magical elemental resistance from rings should handle this,
    // and get damage reduction already ignores non-physical elements
    // for armour...
    //if (attackstyle == ATTACKSTYLE_INTERNAL)
    //	  reduction = 0;
    
    // Check for resist/vulnerability
    if (element != ELEMENT_NONE)
    {
	if (defn().vulnerability == element)
	    multiplier += 1;
	if (defn().resistance == element)
	    multiplier -= 1;

	// Check any items...
	for (int i = 0; i < myInventory.entries(); i++)
	{
	    ITEM	*item = myInventory(i);
	    if (item->getDefinition() == ITEM_RESIST)
	    {
		if (item->element() == element)
		    multiplier -= 1;
	    }
	    if (item->getDefinition() == ITEM_VULNERABLE)
	    {
		if (item->element() == element)
		    multiplier += 1;
	    }
	}
	if (multiplier < 0)
	    multiplier = 0;
    }

    if (multiplier == 0)
    {
	formatAndReport("%S <ignore> the attack.");
	return false;
    }

    if (multiplier != 1.0f)
    {
	float		fhits = hits * multiplier;

	hits = int(fhits);
	fhits -= hits;

	// Add the round off error.
	if (rand_double() < fhits)
	    hits++;
    }

    if (hasItem(ITEM_PLAGUE))
    {
	// Take extra damage.
	hits += hits / 3;
    }

    // Add in reduction
    reduction = rand_range(0, reduction);
    hits -= reduction;
    if (hits < 0)
	hits = 0;

    if (src && src->isAvatar() && src != this)
    {
	// Gain elemental power for a hit.
	// Do not gain for overkill.
	int mpgain = (MIN(hits, getHP()) + 2) / 3;
	gainPartyMP(mpgain, element);
    }

    if (hits >= getHP())
    {
	// If we die, but have an extra life...
	if (getExtraLives())
	{
	    formatAndReport("%S <fall> to the ground dead.");
	    formatAndReport("%S <glow> brightly and <return> to life!");
	    if (inParty())
	    {
		text_registervariable("MOB", getName());
		glbEngine->popupText(text_lookup("death", "Another Chance"));
	    }
	    // All flags are reset with our new life, both bonus an malus.
	    loseTempItems();

	    myExtraLives--;
	    gainHP(getMaxHP() - getHP());
	    return true;
	}
	kill(src);
	return true;
    }

    // If the attack element is light, they are now blind
    if (element == ELEMENT_LIGHT)
    {
	giftItem(ITEM_BLIND);
    }

    // They lived.  They get the chance to yell.
    if (src && src->isAvatar() && isFriends(src) && !isAvatar() && !inParty())
    {
	// This is a free action.
	actionYell(YELL_MURDERER);
	giftItem(ITEM_ENRAGED);
    }

    // Flash that they are hit.
    if (hits && !silent)
	pos().postEvent(EVENTTYPE_FOREBACK, ' ', ATTR_HEALTH);

    myHP -= hits;
    return false;
}

void
MOB::kill(MOB *src)
{
    // Ensure we are dead to alive()
    myHP = 0;
    myNumDeaths++;
    // Record for inverse monster memory.
    edefn().totalkillsever++;
    // Rather mean.
    myMP = 0;

    // Death!
    if (src)
	src->formatAndReport("%S <kill> %O!", this);
    else
	formatAndReport("%S <be> killed!");

    // If there is a source, and they are swallowed,
    // they are released.
    if (src && src->isSwallowed())
    {
	src->setSwallowed(false);
    }
    
    // If we are the avatar, special stuff happens.
    if (isAvatar())
    {
	// never update on this thread!
	// Swap out someone else to be our leader!
	// Only if everyone is gone do we fail

	// msg_update();
	// TODO: Pause something here?
    }
    else
    {
	// Acquire any loot.
	int		gold = 0;
	if (defn().loot >= 0)
	    gold = rand_choice(defn().loot+1);
	if (gold)
	{
	    gainGold(gold);
	}

	// Drop any stuff we have.
	int i;
	for (i = 0; i < myInventory.entries(); i++)
	{
	    myInventory(i)->setEquipped(false);
	    if (myInventory(i)->isFlag())
		delete myInventory(i);
	    else
		myInventory(i)->move(pos());
	}
	myInventory.clear();
    }

    if (src)
    {
	// Assign exp based on target's level.
	// this isn't meant to be a viable way to gain levels, as books
	// are that!
	src->gainExp( defn().depth + getLevel(), /*silent=*/ false );
    }

    if (src && src->isAvatar())
    {
	// Award our score.

    }

    // Flash the screen
    pos().postEvent(EVENTTYPE_FOREBACK, ' ', ATTR_INVULNERABLE);

    // Note that avatar doesn't actually die...
    if (!isAvatar() && !inParty())
    {
	// Anyone who witnessed this can yell.
	if (src && src->isAvatar() && isFriends(src))
	{
	    MOBLIST		allmobs;

	    pos().map()->getAllMobs(allmobs);

	    for (int i = 0; i < allmobs.entries(); i++)
	    {
		if (allmobs(i)->pos().isFOV() && allmobs(i) != this &&
		    !allmobs(i)->isAvatar() &&
		    allmobs(i)->alive() &&
		    allmobs(i)->isFriends(src))
		{
		    allmobs(i)->actionYell(YELL_MURDERER);
		    allmobs(i)->giftItem(ITEM_ENRAGED);
		    break;
		}
	    }
	}

	// Don't actually delete, but die.
	MAP		*map = pos().map();

	{
	    if (rand_chance(defn().corpsechance))
	    {
		ITEM	*corpse = ITEM::create(ITEM_CORPSE);
		corpse->setMobType(getDefinition());
		corpse->setHero(getHero());

		corpse->move(pos());
	    }
	}

	myPos.removeMob(this);
	map->addDeadMob(this);
	clearAllPos();
	loseTempItems();
    }
    else
    {
	// Either the avatar, or someone in the party dies.
	// If it is the avatar it is the main player so we have more
	// work to do.
	bool	avatardeath = isAvatar();

	// Make sure we drop our blind attribute..
	loseTempItems();

	// End any meditation
	myMeditatePos = POS();

	// No matter what, the source sees it (we may be meditating)
	if (src && avatardeath)
	    src->mySawVictory = true;

	if (pos().isFOV() && avatardeath)
	{
	    MOBLIST		allmobs;

	    pos().map()->getAllMobs(allmobs);

	    for (int i = 0; i < allmobs.entries(); i++)
	    {
		if (allmobs(i)->pos().isFOV())
		    allmobs(i)->mySawVictory = true;
	    }
	}

	// Now activate the next party member if we have more than one.
	MAP *map = pos().map();
	POS  thispos = pos();
	if (!avatardeath)
	{
	    // Drop stuff on main map, not on astral plane!
	    thispos = getAvatar()->pos();
	}

	{
	    if (rand_chance(defn().corpsechance))
	    {
		ITEM	*corpse = ITEM::create(ITEM_CORPSE);
		corpse->setMobType(getDefinition());
		corpse->setHero(getHero());

		corpse->move(thispos);
	    }
	}

	if (!avatardeath)
	{
	    // Off screen death of party member.  Make
	    // sure their stuff lands on the floor.

	    // Delete from astral
	    myPos.removeMob(this);
	    map->addDeadMob(this);
	    clearAllPos();

	    // Drop equipped items that aren't considered shared
	    // inventory.  Drop to avatar location
	    int i;
	    for (i = 0; i < myInventory.entries(); i++)
	    {
		myInventory(i)->setEquipped(false);
		if (myInventory(i)->isFlag())
		    delete myInventory(i);
		else
		    myInventory(i)->move(thispos);
	    }
	    myInventory.clear();

	    glbEngine->popupText(text_lookup("death", this->getRawName()));

	    // Remove ourself from the party
	    map->party().removeMob(this);
	}
	else
	{
	    J_ASSERT(map->party().active() == this);
	    MOB *leader = map->party().removeActive();

	    if (leader)
	    {
		myPos.removeMob(this);
		map->addDeadMob(this);
		clearAllPos();

		// Drop equipped items that aren't considered shared
		// inventory.
		int i;
		for (i = 0; i < myInventory.entries(); i++)
		{
		    myInventory(i)->setEquipped(false);
		    if (myInventory(i)->isFlag())
			delete myInventory(i);
		    else
			myInventory(i)->move(thispos);
		}
		myInventory.clear();

		glbEngine->popupText(text_lookup("death", this->getRawName()));

		leader->move(thispos);
		map->setAvatar(leader);
	    }
	    else
	    {
		// Total Party Kill!
		// Find another avatar...
		// hide from the search.
		myDefinition = MOB_NONE;
		MOB *leader = map->findMobByType(MOB_AVATAR);
		J_ASSERT(leader != this);
		myDefinition = MOB_AVATAR;

		if (leader)
		{
		    // Someone in the village to switch to!
		    glbEngine->popupText(text_lookup("death", this->getRawName()));
		    glbEngine->popupText(text_lookup("death", "New Party"));

		    myPos.removeMob(this);
		    map->addDeadMob(this);
		    clearAllPos();

		    // If we take over an enraged person, they calm down.
		    ITEM *enrage = leader->lookupItem(ITEM_ENRAGED);
		    if (enrage)
		    {
			leader->removeItem(enrage);
			delete enrage;
		    }

		    // Drop equipped items that aren't considered shared
		    // inventory.
		    // Since we haven't rebuilt the party, all items
		    // are now dumped here.
		    int i;
		    for (i = 0; i < myInventory.entries(); i++)
		    {
			myInventory(i)->setEquipped(false);
			if (myInventory(i)->isFlag())
			    delete myInventory(i);
			else
			    myInventory(i)->move(thispos);
		    }
		    myInventory.clear();

		    map->party().replace(0, leader);
		    map->party().makeActive(0);
		    map->setAvatar(leader);
		}
		else
		{
		    // Total party kill and no one in town.
		    // We have to make the dead avatar active in the party
		    // so that findAvatar() will locate a dead avatar
		    // and we trigger the end conditions.
		    map->party().replace(0, this);
		    map->party().makeActive(0);
		}
	    }
	}
    }
}

bool
MOB::inParty() const
{
    MAP *map = pos().map();
    if (!map)
	return false;

    for (int i = 0; i < MAX_PARTY; i++)
    {
	if (map->party().member(i) == this)
	    return true;
    }
    return false;
}

VERB_PERSON
MOB::getPerson() const
{
    // Rogue Impact is in third person.
    if (isAvatar())
	return VERB_THEYS;

    return VERB_IT;
}

bool
MOB::isFriends(const MOB *other) const
{
    if (other == this)
	return true;

    if (hasItem(ITEM_ENRAGED) || other->hasItem(ITEM_ENRAGED))
	return false;
    
    if (isAvatar())
    {
	if (other->defn().isfriendly)
	    return true;
	else
	    return false;
    }

    // Friendly hate non friendly, vice versa.
    return defn().isfriendly == other->defn().isfriendly;
}

AI_NAMES
MOB::getAI() const
{
    return (AI_NAMES) defn().ai;
}

void
MOB::reportSquare(POS t)
{
    if (!isAvatar())
	return;

    bool	isblind = hasItem(ITEM_BLIND);

    if (t.mob() && t.mob() != this)
    {
	if (isblind)
	    formatAndReport("%S <feel> %O.", t.mob());
	else
	    formatAndReport("%S <see> %O.", t.mob());
    }
    if (t.item())
    {
	ITEMLIST	itemlist;

	t.allItems(itemlist);
	if (itemlist.entries())
	{
	    BUF		msg;
	    if (isblind)
		msg.strcpy("%S <feel> ");
	    else
		msg.strcpy("%S <see> ");
	    for (int i = 0; i < itemlist.entries(); i++)
	    {
		if (i)
		{
		    if (i == itemlist.entries()-1)
			msg.strcat(" and ");
		    else
			msg.strcat(", ");
		}

		msg.strcat(itemlist(i)->getArticleName());
	    }

	    msg.strcat(".");
	    formatAndReport(msg);
	}
    }

    if (t.defn().describe)
    {
	if (isblind)
	    formatAndReport("%S <feel> %O.", t.defn().legend);
	else
	    formatAndReport("%S <see> %O.", t.defn().legend);
    }

    // If this is the meditation spot, prompt for new world.
    if (t.tile() == TILE_MEDITATIONSPOT)
    {
	ITEM *heart = lookupItem(ITEM_MACGUFFIN);
	if (heart)
	{
	    const char *options[] =
	    {
		"Seal the evil and protect this world",
		"Expose the deeper evil",
		"Think on it",
		0
	    };
	    int choice = glbEngine->askQuestionListCanned("victory", "heartchoice", options, 2, false);
	    if (choice == 0)
	    {
		formatAndReport("%S <seal> the evil from further influencing this realm.");
		heart = splitStack(heart);
		delete heart;
		myHasWon = true;
	    }
	    else if (choice == 1)
	    {
		formatAndReport("%S <uncover> deeper evil so they can destroy it too.");
		heart = splitStack(heart);
		delete heart;
		t.map()->ascendWorld();
	    }
	    else
	    {
		formatAndReport("%S <think> on what choice to make.");
	    }
	}
	else
	{
	    glbEngine->popupText(text_lookup("victory", "noheart"));
	}
    }
}

bool
MOB::shouldPush(MOB *victim) const
{
    // Global rank:
    return true;
}

void
MOB::meditateMove(POS t, bool decouple)
{
    myMeditatePos = t;
    myDecoupledFromBody = decouple;
    // Don't spam because we have no cursor.
    // reportSquare(t);
}

bool
MOB::actionBump(int dx, int dy)
{
    MOB		*mob;
    POS		 t;

    // Stand in place.
    if (!dx && !dy)
    {
	stopRunning();
	return true;
    }

    if (isMeditating())
    {
	stopRunning();
	// We are free of our body!
	t = myMeditatePos.delta(dx, dy);
	if (t.defn().ispassable)
	{
	    meditateMove(t);
	    return true;
	}
	// Wall slide...
	if (dx && dy && isAvatar())
	{
	    // Try to wall slide, cause we are pretty real time here
	    // and it is frustrating to navigate curvy passages otherwise.
	    t = myMeditatePos.delta(dx, 0);
	    if (!rand_choice(2) && t.defn().ispassable)
	    { meditateMove(t); return true; }

	    t = myMeditatePos.delta(0, dy);
	    if (t.defn().ispassable)
	    { meditateMove(t); return true; }

	    t = myMeditatePos.delta(dx, 0);
	    if (t.defn().ispassable)
	    { meditateMove(t); return true; }
	}
	else if ((dx || dy) && isAvatar())
	{
	    // If we have
	    // ..
	    // @#
	    // ..
	    // Moving right we want to slide to a diagonal.
	    int		sdx, sdy;

	    // This bit of code is too clever for its own good!
	    sdx = !dx;
	    sdy = !dy;

	    t = myMeditatePos.delta(dx+sdx, dy+sdy);
	    if (!rand_choice(2) && t.defn().ispassable)
	    { meditateMove(t); return true; }

	    t = myMeditatePos.delta(dx-sdx, dy-sdy);
	    if (t.defn().ispassable)
	    { meditateMove(t); return true; }

	    t = myMeditatePos.delta(dx+sdx, dy+sdy);
	    if (t.defn().ispassable)
	    { meditateMove(t); return true; }
	}
	return false;
    }

    t = pos().delta(dx, dy);

    // If we are swallowed, we must attack.
    if (isSwallowed())
	return actionMelee(dx, dy);
    
    mob = t.mob();
    if (mob)
    {
	// Either kill or chat!
	if (mob->isFriends(this))
	{
	    if (isAvatar())
		return actionChat(dx, dy);
	}
	else
	{
	    return actionMelee(dx, dy);
	}
    }

    // No mob, see if we can move that way.
    // We let actionWalk deal with unable to move notification.
    return actionWalk(dx, dy);
}

bool
MOB::actionMeditate()
{
    stopRunning();
    if (isMeditating())
    {
	formatAndReport("%S <stop> meditating.");
	myMeditatePos = POS();
	// we don't want the user to be able to use up a fast
	// turn by doing this!
	PHASE_NAMES		phase;

	phase = map()->getPhase();
	if (phase == PHASE_FAST || phase == PHASE_QUICK)
	{
	    mySkipNextTurn = true;
	}
    }
    else
    {
	if (pos().tile() != TILE_MEDITATIONSPOT)
	{
	    formatAndReport("This location is not tranquil enough to support meditation.");
	    return false;
	}
	formatAndReport("%S <close> %r eyes and meditate.");
    }
    return true;
}

void
MOB::searchOffset(int dx, int dy, bool silent)
{
    POS		square = pos().delta(dx, dy);

    if (square.isTrap())
    {
	TRAP_NAMES		trap = (TRAP_NAMES) rand_choice(NUM_TRAPS);

	formatAndReport("%S find %O and disarm it.", glb_trapdefs[trap].name);
	square.clearTrap();

	square.postEvent((EVENTTYPE_NAMES)(EVENTTYPE_ALL | EVENTTYPE_LONG), 
			glb_trapdefs[trap].sym,
			(ATTR_NAMES) glb_trapdefs[trap].attr);
    }
    else if (square.tile() == TILE_SECRETDOOR)
    {
	formatAndReport("%S <reveal> a secret door.");
	square.setTile(TILE_DOOR);
	square.postEvent(EVENTTYPE_FOREBACK, '+', ATTR_SEARCH);
    }
    else
    {
	if (!silent)
	    square.postEvent(EVENTTYPE_FOREBACK, ' ', ATTR_SEARCH);
    }
}

bool
MOB::actionSearch(bool silent)
{
    stopRunning();
    mySearchPower = 1;
    if (!silent)
	formatAndReport("%S <search>.");

    int		ir, r;

    r = mySearchPower;
    // Note the < r to avoid double counting corners.
    for (ir = -r; ir < r; ir++)
    {
	searchOffset(ir, -r, silent);
	searchOffset(r, ir, silent);
	searchOffset(-ir, r, silent);
	searchOffset(-r, -ir, silent);
    }

    return true;
}

bool
MOB::actionDropButOne(ITEM *item)
{
    stopRunning();
    if (!item)
	return false;

    ITEM		*butone;

    butone = splitStack(item);
    J_ASSERT(butone != item);

    if (butone == item)
    {
	addItem(butone);
	return false;
    }

    // Swap the two meanings
    butone->setStackCount(item->getStackCount());
    item->setStackCount(1);

    formatAndReport("%S <drop> %O.", butone);

    // And drop
    butone->move(pos());

    return true;
}

bool
MOB::actionDrop(ITEM *item)
{
    stopRunning();
    if (!item)
	return false;
    // Drop any stuff we have.
    int i;
    bool	fail = true;
    for (i = 0; i < myInventory.entries(); i++)
    {
	// Ignore special case items..
	// We don't want to drop "blindness" :>
	if (myInventory(i)->isFlag())
	    continue;

	if (myInventory(i) == item)
	{
	    formatAndReport("%S <drop> %O.", myInventory(i));
	    fail = false;
	    myInventory(i)->setEquipped(false);
	    myInventory(i)->move(pos());
	    myInventory.set(i, 0);
	}
    }
    myInventory.collapse();

    updateEquippedItems();

    if (fail)
	formatAndReport("%S <drop> nothing.");

    return true;
}

bool
MOB::actionQuaffTop()
{
    stopRunning();
    ITEM		*item = getTopBackpackItem();
    if (item)
	actionQuaff(item);
    else
	formatAndReport("%S <fiddle> with %r backpack.");
    return true;
}

bool
MOB::actionQuaff(ITEM *item)
{
    stopRunning();
    if (!item)
	return false;
    if (!item->isPotion())
    {
	formatAndReport("%S cannot drink %O.", item);
	return false;
    }

    item = splitStack(item);

    doEmote("*quaff*");
    formatAndReport("%S <quaff> %O.", item);

    bool		interesting = false;

    interesting = item->potionEffect() != EFFECT_NONE;
    applyEffect(this, item->potionEffect(),
		    ATTACKSTYLE_INTERNAL);

    if (interesting)
    {
	item->markMagicClassKnown();
    }
    else
    {
	formatAndReport("Nothing happens.");
    }


    delete item;

    return true;
}

bool
MOB::actionRead(ITEM *item)
{
    stopRunning();
    if (!item)
	return false;
    if (!item->isSpellbook())
    {
	formatAndReport("%S <see> no writing on %O.", item);
	return false;
    }

    item = splitStack(item);

    doEmote("*studies*");
    formatAndReport("%S <read> %O, gaining experience.", item);

    gainExp(item->defn().depth * 100, /*silent=*/false);

    // These are self named now.
    // item->markMagicClassKnown();

    delete item;

    return true;
}

bool
MOB::actionThrowTop(int dx, int dy)
{
    ITEM *item = getTopBackpackItem();

    if (!item)
    {
	formatAndReport("%S <have> nothing to throw.");
	return true;
    }

    return actionThrow(item, dx, dy);
}

bool
MOB::actionThrow(ITEM *item, int dx, int dy)
{
    stopRunning();

    // If swallowed, rather useless.
    if (isSwallowed())
    {
	formatAndReport("%S <do> not have enough room inside here.");
	return true;
    }

    if (!item)
	return false;
    item = splitStack(item);

    formatAndReport("%S <throw> %O.", item);
    if (item->isPotion())
    {
	bool		interesting = false;

	POTION_OP		op(this, item->getDefinition(), &interesting);

	u8			sym;
	ATTR_NAMES		attr;
	item->getLook(sym, attr);

	if (!doRangedAttack(5, 2, dx ,dy, '~', attr, "hit", true, false, op))
	{
	    addItem(item);
	    return false;
	}

	if (interesting)
	{
	    item->markMagicClassKnown();
	}

	delete item;

	return true;
    }
    else if (item->getDefinition() == ITEM_PIERCE_WEAPON)
    {
	// Aerodynamic, shot and counts as melee!
	ATTACK_OP	op(this, item, (ATTACK_NAMES) item->getMeleeAttack());

	u8		symbol;
	ATTR_NAMES	attr;
	item->getLook(symbol, attr);

	POS	epos = pos().traceBulletPos(5, dx, dy, true, true);
	doRangedAttack(5, 1, dx, dy,
			    symbol, attr,
			    GAMEDEF::attackdef(item->getMeleeAttack())->verb,
			    true, false, op);
	item->move(epos);

	return true;
    }
    else if (item->getDefinition() == ITEM_PORTAL_STONE)
    {
	bool		ok = false;
	if (pos().depth() == 0)
	{
	    formatAndReport("%S <throw> %O, but it fails to activate.  Perhaps it must be farther from the village.", item);
	}
	else
	{
	    if (actionPortalFire(dx, dy, 0))
	    {
		// Made a portal!
		delete item;
		return true;
	    }
	}
	// Failed to make a portal, so go back to throwing the rock.
    }
    else if (item->getDefinition() == ITEM_WISH_STONE)
    {
	bool		ok = false;
	if (pos().depth() != 0)
	{
	    formatAndReport("%S <throw> %O, but it fails to activate.  Perhaps it would work in a safer location.", item);
	}
	else
	{
	    POS	epos = pos().traceBulletPos(3, dx, dy, true, true);

	    // See if a free spot was reached.
	    if (epos.mob())
	    {
		formatAndReport("%S <throw> %O, but it fails to activate.  Someone seems to have been in the way.", item);
		item->move(epos);

		return true;
	    }

	    // Create a hero at the location!
	    HERO_NAMES	h = (HERO_NAMES) rand_range(HERO_NONE+1, NUM_HEROS-1);

	    // h = HERO_TIMMY;
	    // h = HERO_ALKAMI;

	    MAP *map = pos().map();
	    MOB *oldhero = map->findMobByHero(h);

	    formatAndReport("%O dissolves into mist.", item);
	    delete item;
	    if (oldhero)
	    {
		// Invigorate the specified hero.
		formatAndReport("The spirit of %O coalesces into existence.", oldhero);
		formatAndReport("The spirit flies to their body, imbuing them with extra fortitude.");
		oldhero->grantExtraLife();
	    }
	    else
	    {
		// No such hero, build one!
		oldhero = createHero(h);
		oldhero->move(epos);
		formatAndReport("The spirit of %O coalesces into existence.", oldhero);
	    }
	    return true;
	}
	// Return to normal tossing.
    }
    // Unwieldly!
    POS	epos = pos().traceBulletPos(2, dx, dy, true, true);
    item->move(epos);

    return true;
}

bool
MOB::actionEatTop()
{
    ITEM		*item = getTopBackpackItem();
    if (item)
	actionEat(item);
    else
	formatAndReport("%S <fiddle> with %r backpack.");
    return true;
}

bool
MOB::actionEat(ITEM *item)
{
    stopRunning();
    if (!item)
	return false;

    if (!item->isFood())
    {
	formatAndReport("%S cannot eat %O.", item);
	return false;
    }

    if (getFood() >= getMaxFood())
    {
	formatAndReport("%S <be> too full to eat any more.");
	return false;
    }

    item = splitStack(item);

    formatAndReport("%S <eat> %O.", item);

    if (getHP() < getMaxHP())
    {
	formatAndReport("%S <heal>.");
	gainHP(rand_range(1, 5));
    }

    // This is basically the timeout for healing with food.
    // But everyone in the party eats
    gainFood(80);

    if (item->hero() != HERO_NONE)
    {
	formatAndReport("%S <learn> that there are public health reasons to avoid cannibalism.");
    }

    if (item->corpseEffect() != EFFECT_NONE)
    {
	applyEffect(this, item->corpseEffect(), ATTACKSTYLE_INTERNAL);
    }

    delete item;

    return true;
}

bool
MOB::actionBreakTop()
{
    ITEM		*item = getWieldedOrTopBackpackItem();
    if (item)
	actionBreak(item);
    else
	formatAndReport("%S <fiddle> with %r backpack.");
    return true;
}

bool
MOB::actionBreak(ITEM *item)
{
    stopRunning();
    if (!item)
	return false;

    if ((!item->isRanged() && !item->isWeapon()) || item->isBroken())
    {
	item = splitStack(item);

	formatAndReport("%S <destroy> %O.", item);
	delete item;
	return true;
    }

    formatAndReport("%S <break> %O.", item);

    doEmote("*snap*");

    item->setBroken(true);

    return true;
}

bool
MOB::actionWearTop()
{
    stopRunning();
    ITEM *item = getTopBackpackItem();

    if (item)
    {
	actionWear(item);
    }
    else
    {
	formatAndReport("%S <fiddle> with %r backpack.");
    }
    return true;
}

bool
MOB::actionWear(ITEM *item)
{
    stopRunning();
    if (!item)
	return false;

    if (item->isRanged())
    {
	formatAndReport("%S already <know> the best bow to use.");
	return false;
    }

    if (item->isWeapon())
    {
	ITEM		*olditem = lookupWeapon();

	// Don't let us equip stuff we shouldn't use, but if we get it
	// somehow at least let people unequip.
	if (isAvatar() && hero().weaponclass != item->defn().weaponclass
	    && olditem != item)
	{
	    formatAndReport("%S <be> not skilled with that weapon.");
	    return false;
	}
	if (olditem)
	{
	    formatAndReport("%S <unwield> %O.", olditem);
	    olditem->setEquipped(false);
	    // Move to top of our list.
	    myInventory.removePtr(olditem);
	    myInventory.append(olditem);
	}

	// If they are the same, the user wanted to toggle!
	if (olditem != item)
	{
	    item = splitStack(item);
	    formatAndReport("%S <wield> %O.", item);
	    item->setEquipped(true);
	    addItem(item);
	}
	return true;
    }

    if (item->isArmour())
    {
	ITEM		*olditem = lookupArmour();
	if (olditem)
	{
	    formatAndReport("%S <take> off %O.", olditem);
	    olditem->setEquipped(false);
	    myInventory.removePtr(olditem);
	    myInventory.append(olditem);
	}

	// If they are the same, the user wanted to toggle!
	if (olditem != item)
	{
	    item = splitStack(item);
	    formatAndReport("%S <put> on %O.", item);
	    item->setEquipped(true);
	    addItem(item);
	}
	return true;
    }

    if (!item->isRing())
    {
	formatAndReport("%S cannot wear %O.", item);
	return false;
    }

    ITEM		*oldring = lookupRing();

    if (oldring)
    {
	formatAndReport("%S <take> off %O.", oldring);
	oldring->setEquipped(false);
	myInventory.removePtr(oldring);
	myInventory.append(oldring);
    }

    // If they are the same, the user wanted to toggle!
    if (oldring != item)
    {
	item = splitStack(item);
	formatAndReport("%S <put> on %O.", item);
	item->setEquipped(true);
	addItem(item);
    }

    return true;
}

const char *
getYellMessage(YELL_NAMES yell)
{
    const char *yellmsg = "I KNOW NOT WHAT I'M DOING!";

    switch (yell)
    {
	case YELL_KEEPOUT:
	{
	    const char *msg[] =
	    {		// KEEPOUT
		"Keep out!",
		"Away!",
		"Leave!",
		"No Farther!",
		0
	    };
	    yellmsg = rand_string(msg);
	    break;
	}
	case YELL_MURDERER:
	{
	    const char *msg[] =
	    {		// MURDERER
		"Killer!",
		"Murderer!",
		0
	    };
	    yellmsg = rand_string(msg);
	    break;
	}
	case YELL_INVADER:
	{
	    const char *msg[] =
	    {		// INVADER
		"Invader!",
		"ALARM!",
		"ALARM!",
		"Code Yellow!",	// For yummy gold.
		0
	    };
	    yellmsg = rand_string(msg);
	    break;
	}
	case YELL_RANGED:
	{
	    const char *msg[] =
	    {		// RANGED
		"Far Threat!",
		"Code Bow!",	// Kobolds not that imaginative after all
		"Incoming!",
		"Archers!",	
		0
	    };
	    yellmsg = rand_string(msg);
	    break;
	}
	case YELL_KILL:
	{
	    const char *msg[] =
	    {		// KILL
		"Kill it now!",
		"Code Gold!",
		"Kill!",
		"Eviscerate It!",
		"Kill!",
		"Kill!",
		0
	    };
	    yellmsg = rand_string(msg);
	    break;
	}
	case YELL_KILLCHASE:
	{
	    const char *msg[] =
	    {		// KILL
		"No Mercy!",
		"Code Blood!",
		"Hunt It Down!",
		0
	    };
	    yellmsg = rand_string(msg);
	    break;
	}
	case YELL_LOCATION:
	{
	    const char *msg[] =
	    {		// LOCATION
		"It's here!",
		"Foe Sighted!",
		"Over here!",
		"I see it!",
		0
	    };
	    yellmsg = rand_string(msg);
	    break;
	}
	case YELL_HEARDLOCATION:
	{
	    const char *msg[] =
	    {		// LOCATION_HEARD 
		"It's there!",
		"Foe Located!",
		"Over there!",
		0
	    };
	    yellmsg = rand_string(msg);
	    break;
	}
	case YELL_TAUNT:
	{
	    const char *msg[] =
	    {		// LOCATION_HEARD 
		"Coward!",
		"You Flea!",
		"Mendicant!",
		"Wimp!",
		"Fool!",
		"Urchin!",
		0
	    };
	    yellmsg = rand_string(msg);
	    break;
	}
	case YELL_VICTORY:
	{
	    const char *msg[] =
	    {		// LOCATION_HEARD 
		"Victory!",
		"The @ is dead!",
		"Huzzah!",
		"Hooray!",
		"Gold!",
		0
	    };
	    yellmsg = rand_string(msg);
	    break;
	}
    }
    return yellmsg;
}

bool
MOB::actionYell(YELL_NAMES yell)
{
    stopRunning();
    const char	*yellflavour[] =
    {
	"%S <yell>: \"%o\"",
	"%S <shout>: \"%o\"",
	"%S <curse>: \"%o\"",
	0
    };
    const char *yellmsg = getYellMessage(yell);

#if 1
    if (pos().isFOV())
    {
	// Send the shout out.
	msg_format(rand_string(yellflavour), this, yellmsg);
	doShout(yellmsg);
    }
#else
    MOBLIST	hearmobs;

    // Should use spatial search!
    for (int i = 0; i < hearmobs.entries(); i++)
    {
	if (hearmobs(i)->isAvatar())
	{
	    // Don't use line of sight, but the same logic we use.
	    // Thus can't use format and report!
	    msg_format(rand_string(yellflavour), this, yellmsg);
	    doShout(yellmsg);
	}
	if (hearmobs(i) == this)
	    continue;
	hearmobs(i)->myHeardYell[yell] = true;
	if (hearmobs(i)->pos().roomId() == pos().roomId())
	{
	    // Extra flag to avoid spurious re-shouts
	    hearmobs(i)->myHeardYellSameRoom[yell] = true;
	}
    }
#endif

    return true;
}

bool
MOB::actionRotate(int angle)
{
    myPos = myPos.rotate(angle);
    if (isMeditating())
	myMeditatePos = myMeditatePos.rotate(angle);
    // This is always a free action.
    return false;
}

bool
MOB::actionSuicide()
{
    const char *attempt_notes[] =
    {
	"You hold your breath until you fall unconscious.  Blearily, you wake up.",
	"You bash your head against the ground.  Sanity barely intervenes.",
	"It is claimed it is physically impossible to swallow your tongue.  You give it a good try, but fail.",
	"Your fingernails seem not quite enough to open your cartoid artery.",
	"Only after casting the sphere of death spell do you recall it isn't a ranged spell.  Thankfully you got some of the words wrong, and it doesn't hit you fully.",
	"You close your eyes and empty your mind.  You commend all but a spark to the void.",
	"You curse the gods for your situation.  They decide to give you a quick reminder of why they deserve respect.",
	0
    };

    const char *death_notes[] =
    {
	"Through intense concentration, you manage to stop the beating of your heart!  You die.",
	"You hold your breath until you fall unconscious.  You don't wake up.",
	"You bash your head against the ground.  Until you can't.",
	"It is claimed it is physically impossible to swallow your tongue.  You manage to prove them wrong.  You choke to death.",
	"It takes a while, but your fingernails can suffice to open your cartoid artery.  You die.",
	"Only after casting the sphere of death spell do you recall it isn't a ranged spell.",
	"You close your eyes and empty your mind.  You succeed.  You are dead.",
	"You curse the gods for your situation.  They hear, and are not impressed.  You are killed by divine wrath.",
	"You wrap your hands to opposite sides of your head.  A quick jerk, a snap, and your neck is broken.",
	0
    };

    if (getHP() < getMaxHP() / 2 || myHasTriedSuicide)
    {
	formatAndReport(rand_string(death_notes));

	gainHP(-getHP());
	return true;
    }
    myHasTriedSuicide = true;
    formatAndReport(rand_string(attempt_notes));
    gainHP(-getHP()+1);
    return true;
}

bool
MOB::actionClimb()
{
    stopRunning();
    TILE_NAMES		tile;

    tile = pos().tile();

    if (tile == TILE_DOWNSTAIRS)
    {
	// Climbing is interesting.
	myFleeCount = 0;
	clearBoredom();

	if (isAvatar())
	{
	    formatAndReport("%S <climb> down...");
	    pos().map()->moveAvatarToDepth(pos().depth()+1);
	    // Don't use time so you have a chance
	    // to move on new level.
	    return false;
	}
	else
	    return true;
    }
    else if (tile == TILE_UPSTAIRS)
    {
	if (isAvatar())
	{
	    if (pos().depth() > 1)
	    {
		// Not done yet
		formatAndReport("%S <climb> up...");
		pos().map()->moveAvatarToDepth(pos().depth()-1);
		// Don't use time so you have a chance
		// to move on new level.
		return false;
	    }
	    else
	    {
		// Allow returning to town.
#if 0
		if (!hasItem(ITEM_MACGUFFIN))
		{
		    formatAndReport("%S cannot climb without the %o!", GAMEDEF::itemdef(ITEM_MACGUFFIN)->name);
		    return false;
		}
		else
		{
		    formatAndReport("%S <escape>!");
		    // myHasWon = true;
		    return false;
		}
#endif
		formatAndReport("%S <climb> up...");
		pos().map()->moveAvatarToDepth(pos().depth()-1);
		return false;
	    }
	}
	else
	    return true;
    }

    formatAndReport("%S <see> nothing to climb here.");

    return true;
}

bool
MOB::actionChat(int dx, int dy)
{
    stopRunning();
    MOB		*victim;

    // This should never occur, but to avoid
    // embarassaments...
    if (!isAvatar())
	return false;
    
    victim = pos().delta(dx, dy).mob();
    if (!victim)
    {
	// Talk to self
	formatAndReport("%S <talk> to %O!", this);
	return false;
    }

    formatAndReport("%S <chat> with %O.", victim);

    if (victim->getHero() != HERO_NONE)
    {
	const char	*slotname = "ASDF";
	// Offer to join our party.
	const char *join_options[MAX_PARTY+2] =
	{  
	    "Request they take slot A",
	    "Request they take slot S",
	    "Request they take slot D",
	    "Request they take slot F",
	    "Maybe another time.",
	    0
	};

	J_ASSERT(MAX_PARTY == 4);

	BUF	tempbuf[MAX_PARTY];
	MAP 	*map = pos().map();

	for (int i = 0; i < MAX_PARTY; i++)
	{
	    MOB *member = map->party().member(i);
	    if (!member)
	    {
		tempbuf[i].sprintf("Spot %c is free.", slotname[i]);
		join_options[i] = tempbuf[i].buffer();
	    }
	    else
	    {
		tempbuf[i].sprintf("Replace %s in spot %c.",
			    member->getName().buffer(),
			    slotname[i]);
		join_options[i] = tempbuf[i].buffer();
	    }
	}
	join_options[MAX_PARTY] = "Maybe another time.";
	join_options[MAX_PARTY+1] = 0;

	int choice = glbEngine->askQuestionListCanned("chat", victim->getRawName().buffer(), join_options, 4, false);
	if (choice >= 0 && choice < MAX_PARTY)
	{
	    // Try to swap them.
	    // Note this may cause us to stop being the primary
	    // avatar!
	    MAP *map = pos().map();
	    MOB *old = map->party().replace(choice, victim);

	    POS victimpos = victim->pos();
	    POS curpos = pos();
	    victim->move(map->astralLoc());
	    if (old)
		old->move(victimpos);

	    // If the victim is the new avatar, update.
	    if (map->party().active() == victim)
	    {
		victim->move(curpos);
		map->setAvatar(victim);
	    }
	}
    }
    
    return true;
}

bool
MOB::actionMelee(int dx, int dy)
{
    stopRunning();
    MOB		*victim;

    // If we are swallowed, attack the person on our square.
    if (isSwallowed())
	dx = dy = 0;

    POS		 t = pos().delta(dx, dy);

    victim = t.mob();
    if (!victim)
    {
	// Swing in air!
	formatAndReport("%S <swing> at empty air!");
	return false;
    }

    if (!victim->alive())
    {
	// The end GAme avatar, just silently ignore
	J_ASSERT(!isAvatar());
	return false;
    }

    // Shooting is interesting.
    clearBoredom();
 
    int		damage;
    bool	victimdead;

    damage = 0;
    if (evaluateMeleeWillHit(victim))
	damage = evaluateMeleeDamage();

    // attempt to kill victim
    if (damage)
    {
	formatAndReport("%S %v %O.", getMeleeVerb(), victim);
    }
    else
    {
	formatAndReport("%S <miss> %O.", victim);
	return true;
    }

    MOB_NAMES	victimtype = victim->getDefinition();

    victimdead = victim->applyDamage(this, damage,
				    getMeleeElement(),
				    ATTACKSTYLE_MELEE);
    
    // Vampires regenerate!
    if (defn().isvampire)
    {
	myHP += damage;
    }

    // Grant our item...
    if (!victimdead)
    {
	if (attackdefn().effect != EFFECT_NONE)
	{
	    if (victim->applyEffect(this, (EFFECT_NAMES) attackdefn().effect,
				    ATTACKSTYLE_MELEE))
		return true;
	}

	// Swallow the victim
	// Seems silly to do this on a miss.
	// You also can't swallow people when you yourself are swallowed.
	if (!isSwallowed() && defn().swallows && damage)
	{
	    victim->setSwallowed(true);
	    victim->move(pos(), true);
	}

	// Steal something
	if (defn().isthief)
	{
	    ITEM		*item;
	    
	    // Get a random item from the victim
	    // Don't steal equipped items as that is too deadly for
	    // most item-levelling systems.
	    item = victim->getRandomItem(false);
	    if (item) 
	    {
		formatAndReport("%S <steal> %O.", item);
		doEmote("*yoink*");
		// Theft successful.
		myFleeCount += 30;
		victim->removeItem(item, true);
		item->setEquipped(false);
		addItem(item);
	    }
	}
    }

    // Chance to break our weapon.
    {
	ITEM	*w;

	w = lookupWeapon();
	if (w)
	{
	    if (rand_chance(w->breakChance(victimtype)))
	    {
		formatAndReport("%r %o breaks.", w);
		doEmote("*snap*");
		w->setBroken(true);
	    }
	}
    }
    return true;
}

ATTACK_NAMES
MOB::getMeleeAttack() const
{
    ITEM	*w;

    w = lookupWeapon();
    if (!w)
	return (ATTACK_NAMES) defn().melee_attack;

    return (ATTACK_NAMES) w->getMeleeAttack();
}

int
MOB::getMeleeCanonicalDamage() const
{
    ITEM	*weap;

    weap = lookupWeapon();

    int		base = attackdefn().damage;
    if (weap)
	base = LEVEL::boostDamage(base, weap->getLevel());
    else
	base = LEVEL::boostDamage(base, getLevel());

    return base;
}

const char *
MOB::getMeleeVerb() const
{
    return attackdefn().verb;
}

ELEMENT_NAMES
MOB::getMeleeElement() const
{
    ATTACK_NAMES	attack = getMeleeAttack();

    return (ELEMENT_NAMES) GAMEDEF::attackdef(attack)->element;
}

const char *
MOB::getMeleeWeaponName() const
{
    ITEM	*w;

    w = lookupWeapon();
    if (!w)
	return attackdefn().noun;

    return w->defn().name;
}

int
MOB::evaluateMeleeDamage() const
{
    ATTACK_NAMES	attack = getMeleeAttack();

    return evaluateAttackDamage(attack, lookupWeapon());
}

bool
MOB::evaluateMeleeWillHit(MOB *victim) const
{
    ATTACK_NAMES	attack = getMeleeAttack();

    return evaluateWillHit(attack, lookupWeapon(), victim);
}

bool
MOB::evaluateWillHit(ATTACK_NAMES attack, ITEM *weap, MOB *victim) const
{
    // We can always hit inanimate objects?
    if (!victim)
	return true;

    ATTACK_DEF	*def = GAMEDEF::attackdef(attack);

    int		tohit = 10;

    tohit -= def->chancebonus;
    tohit += victim->defn().dodgebonus;

    // Our natural skill comes into play...
    // We have the difference so a factor of 20 is overwhelming.
    int		levelbias = 0;
    levelbias -= getLevel();
    if (weap)
	levelbias -= weap->getLevel();
    levelbias += victim->getLevel();
    levelbias /= 2;  // Round to no effect.
    tohit += levelbias;

    // I keep turning back to the d20 system.  The simple fact is
    // that 5% is right at the human threshold for noticing, so is
    // by far the most convenient to express our boni with.
    int		dieroll = rand_range(1, 20);

    if (dieroll != 1 &&
	(dieroll == 20 || (dieroll >= tohit)))
	return true;
    return false;
}

int
MOB::getRangedCanonicalDamage() const
{
    ITEM	*weap;

    weap = lookupWand();

    int		base = rangeattackdefn().damage;
    if (weap)
	base = LEVEL::boostDamage(base, weap->getLevel());
    else
	base = LEVEL::boostDamage(base, getLevel());

    return base;
}


int
MOB::evaluateAttackDamage(ATTACK_NAMES attack, ITEM *weap) const
{
    ATTACK_DEF	*def = GAMEDEF::attackdef(attack);

    int		base = def->damage;
    if (weap)
	base = LEVEL::boostDamage(base, weap->getLevel());
    else
	base = LEVEL::boostDamage(base, getLevel());

    base = evaluateDistribution(base, (DISTRIBUTION_NAMES)def->distribution);

    return base;
}

int
MOB::evaluateDistribution(int amount, DISTRIBUTION_NAMES distribution)
{
    int		base = amount;

    switch (distribution)
    {
	case NUM_DISTRIBUTIONS:
	    break;
	case DISTRIBUTION_CONSTANT:
	    // damage is just damage
	    break;
	case DISTRIBUTION_UNIFORM:
	    // Uniform in double range gives same average.
	    base = rand_range(0, base * 2-1);
	    base++;
	    break;
	case DISTRIBUTION_BIMODAL:
	{
	    // Half the time we do a low-end distribution,
	    // half the time a high end.
	    if (rand_choice(2))
	    {
		// We don't double here, but add 50 percent
		// so our average matches.
		base += base / 2;
	    }
	    else
	    {
		base = base/2;
		if (!base)
		    base = 1;
	    }
		
	    //FALL THROUGH
	}
	case DISTRIBUTION_GAUSSIAN:
	{
	    // Sum of four uniforms.
	    int		total = 0;
	    total += rand_range(0, base * 2-1);
	    total += rand_range(0, base * 2-1);
	    total += rand_range(0, base * 2-1);
	    total += rand_range(0, base * 2-1);

	    total += 2;
	    total /= 4;

	    base = total + 1;
	    break;
	}
    }

    return base;
}

ATTACK_NAMES
MOB::getRangedAttack() const
{
    ITEM	*w;

    w = lookupWand();
    if (!w)
	return (ATTACK_NAMES) defn().range_attack;

    return (ATTACK_NAMES) w->getRangeAttack();
}

int
MOB::getRangedRange() const
{
    ITEM	*w;

    w = lookupWand();
    if (!w)
	return defn().range_range;

    return w->getRangeRange();
}

const char *
MOB::getRangedWeaponName() const
{
    ITEM	*w;

    w = lookupWand();
    if (!w)
	return rangeattackdefn().noun;

    return w->defn().name;
}

const char *
MOB::getRangedVerb() const
{
    return rangeattackdefn().verb;
}

ELEMENT_NAMES
MOB::getRangedElement() const
{
    return (ELEMENT_NAMES) rangeattackdefn().element;
}

void
MOB::getRangedLook(u8 &symbol, ATTR_NAMES &attr) const
{
    ITEM	*w;

    w = lookupWand();
    if (!w)
    {
	symbol = defn().range_symbol;
	attr = (ATTR_NAMES) defn().range_attr;
    }
    else
    {
	symbol = w->defn().range_symbol;
	attr = (ATTR_NAMES) w->defn().range_attr;
    }
}


template <typename OP>
bool
MOB::doRangedAttack(int range, int area, int dx, int dy, 
		u8 symbol, ATTR_NAMES attr,
		const char *verb, bool targetself, bool piercing, OP op)
{
    int		rangeleft;
    MOB		*victim;

    // Check for friendly kill.
    victim = pos().traceBullet(range, dx, dy, &rangeleft);

    // Friendly fire is cool!

    // Shooting is interesting.
    clearBoredom();
 
    pos().displayBullet(range,
			dx, dy,
			symbol, attr,
			true);

    if (!victim)
    {
	// Shoot at air!
	// But, really, the fireball should explode!
	POS		vpos;
	vpos = pos().traceBulletPos(range, dx, dy, true);

	// Apply damage to everyone in range.
	vpos.fireball(this, area, symbol, attr, op); 

	return true;
    }

    //  Attemp to kill victim
    // I'm not sure why this print out is here, this is a duplicate
    // as the effect will also trigger this?
#if 0
    if (targetself || (victim != this))
	formatAndReport("%S %v %O.", verb, victim);
#endif

    // NOTE: Bad idea to call a function on victim->pos() as
    // that likely will be deleted :>
    POS		vpos = victim->pos();

    // Apply damage to everyone in range.
    vpos.fireball(this, area, symbol, attr, op); 
    
    // The following code will keep the flame going past the target
    while (piercing && rangeleft && /* area == 1 && */ (dx || dy))
    {
	// Ensure our dx/dy is copacetic.
	vpos.setAngle(pos().angle());
	int	newrangeleft = rangeleft;
	victim = vpos.traceBullet(rangeleft, dx, dy, &newrangeleft);

	vpos.displayBullet(rangeleft,
			    dx, dy,
			    symbol, attr,
			    true);

	if (victim)
	{
	    formatAndReport("%S %v %O.", verb, victim);
	    vpos = victim->pos();
	    vpos.fireball(this, area, symbol, attr, op); 
	    rangeleft = newrangeleft;
	}
	else
	{
	    // Shoot at air!
	    // But, really, the fireball should explode!
	    vpos = vpos.traceBulletPos(rangeleft, dx, dy, true);

	    // Apply damage to everyone in range.
	    vpos.fireball(this, area, symbol, attr, op); 

	    rangeleft = 0;
	}
    }

    return true;
}

bool
MOB::actionFire(int dx, int dy)
{
    stopRunning();
    // Check for no ranged weapon.
    ITEM		*wand = lookupWand();

    if (!defn().range_valid && !wand)
    {
	formatAndReport("%S <lack> a ranged attack!");
	return false;
    }

    ITEM_NAMES		ammo = ITEM_NONE;

    if (isAvatar() && lookupWand())
	ammo = (ITEM_NAMES) lookupWand()->defn().ammo;

    if (ammo != ITEM_NONE && !hasItem(ammo))
    {
	BUF		msg, ammoname;

	ammoname = gram_makeplural(ITEM::defn(ammo).name);
	msg.sprintf("%%S <be> out of %s!", ammoname.buffer());
	formatAndReport(msg);
	return false;
    }

    // No suicide.
    if (!dx && !dy)
    {
	formatAndReport("%S <decide> not to aim at %O.", this);
	return false;
    }
    
    // If swallowed, rather useless.
    if (isSwallowed())
    {
	formatAndReport("%S <do> not have enough room inside here.");
	return false;
    }

    u8		symbol;
    ATTR_NAMES	attr;
    getRangedLook(symbol, attr);

    // Use up an arrow!
    if (ammo != ITEM_NONE)
    {
	ITEM	*arrow;
	arrow = lookupItem(ammo);
	if (arrow)
	{
	    arrow->decStackCount();
	    if (!arrow->getStackCount())
	    {
		BUF		ammoname;

		ammoname = gram_makeplural(ITEM::defn(ammo).name);

		removeItem(arrow, true);
		formatAndReport("%S <shoot> the last of %r %o.", ammoname);
		delete arrow;
	    }
	}
    }

    ATTACK_OP	op(this, wand, getRangedAttack());

    // This is not enforced, but is a hint for the AI.
    myRangeTimeout += defn().range_recharge;

    return doRangedAttack(getRangedRange(), 1, dx, dy, 
		    symbol, attr,
		    getRangedVerb(),
		    true, false, op);
}

void
MOB::triggerManaUse(SPELL_NAMES spell, int manacost)
{
}

int
MOB::getGold() const
{
    return getCoin(MATERIAL_GOLD);
}

int
MOB::getCoin(MATERIAL_NAMES material) const
{
    ITEM		*gold;
    int			 ngold;

    gold = lookupItem(ITEM_COIN, material);
    
    ngold = 0;
    if (gold)
	ngold = gold->getStackCount();

    return ngold;
}

void
MOB::gainGold(int delta) 
{
    gainCoin(MATERIAL_GOLD, delta);
}

void
MOB::gainExp(int exp, bool silent)
{
    myExp += exp;

    int level = LEVEL::levelFromExp(myExp);
    while (level > myLevel)
	gainLevel(silent);
}

void
MOB::gainLevel(bool silent)
{
    myLevel++;
    if (!silent)
	formatAndReport("%S <be> more experienced.");

    // Full heal.
    gainHP(getMaxHP() - getHP());
}

void
MOB::grantExtraLife()
{
    myExtraLives++;
    formatAndReport("%S <glow> from within, gaining the ability to thwart death!");
    gainHP(getMaxHP() - getHP());
}

void
MOB::gainCoin(MATERIAL_NAMES material, int delta)
{
    ITEM		*gold;

    if (!delta)
	return;

    gold = lookupItem(ITEM_COIN, material);
    if (!gold && delta>0)
    {
	gold = ITEM::create(ITEM_COIN);
	gold->setMaterial(material);
	gold->setStackCount(0);
	addItem(gold);
    }
    if (!gold && delta<=0)
    {
	// Can't go negative?
	J_ASSERT(!"Negative gold?");
	return;
    }
    
    gold->setStackCount(gold->getStackCount() + delta);

    if (gold->getStackCount() <= 0)
    {
	// All gone, get rid of it so we don't have a No Coins entry.
	// (as cute as that is)
	removeItem(gold);
	delete gold;
    }
}

class SPELL_OP
{
public:
    SPELL_OP(MOB *src, SPELL_NAMES spell, bool hitsself) 
    { mySrc = src; mySpell = spell; myHitsSelf = hitsself; }

    void operator()(POS p)
    {
	MOB		*mob = p.mob();

	if (!myHitsSelf && mySrc == mob)
	    return;
	if (mob)
	{
	    mob->applyEffect(mySrc, 
		(EFFECT_NAMES) GAMEDEF::spelldef(mySpell)->effect,
		ATTACKSTYLE_RANGE);
	}
    }

private:
    SPELL_NAMES	 	mySpell;
    bool		myHitsSelf;
    MOB			*mySrc;
};

class COINSPELL_OP
{
public:
    COINSPELL_OP(MOB *src, EFFECT_NAMES effect, bool hitsself) 
    { mySrc = src; myEffect = effect; myHitsSelf = hitsself; }

    void operator()(POS p)
    {
	MOB		*mob = p.mob();

	if (mob == mySrc && !myHitsSelf)
	    return;
	if (mob)
	{
	    mob->applyEffect(mySrc, 
		myEffect,
		ATTACKSTYLE_RANGE);
	}
    }

private:
    EFFECT_NAMES	myEffect;
    MOB			*mySrc;
    bool		myHitsSelf;
};

bool
MOB::actionCast(SPELL_NAMES spell, int dx, int dy)
{
    stopRunning();
    // Pay the piper...
    int			manacost = 0, hpcost = 0;

    manacost = GAMEDEF::spelldef(spell)->mana;
    if (GAMEDEF::spelldef(spell)->reqfullmana)
    {
	manacost = getMaxMP();
    }

    // CHeck timeout
    int timeout = GAMEDEF::spelldef(spell)->timeout;
    if (timeout)
    {
	if (mySpellTimeout)
	{
	    formatAndReport("%S <be> not ready to cast %O.",
				GAMEDEF::spelldef(spell)->name);
	    return false;
	}
    }
    
    if (manacost > getMP())
    {
	// Let people suicide.
	hpcost = manacost - getMP();
	manacost = getMP();
    }

    if (hpcost)
    {
	// No blood magic!
#if 0
	formatAndReport("Paying with %r own life, %S <cast> %O.",
			    GAMEDEF::spelldef(spell)->name);
#else
	formatAndReport("%S <lack> the mana to cast %O.",
			    GAMEDEF::spelldef(spell)->name);
	return false;
#endif
    }
    else
    {
	// Less dramatic.
	formatAndReport("%S <cast> %O.",
			    GAMEDEF::spelldef(spell)->name);
    }

    if (hpcost)
    {
	if (applyDamage(this, hpcost, ELEMENT_NONE, ATTACKSTYLE_INTERNAL))
	    return true;
    }

    // Delay mana cost until after the spell so you can't
    // recharge your q with the q itself.
    triggerManaUse(spell, manacost);

    // Generic spells:

    // Fire ball self or in a direction...
    bool targetself = GAMEDEF::spelldef(spell)->friendlyfire;
    SPELL_OP		op(this, spell, targetself);

    int radius = GAMEDEF::spelldef(spell)->radius;
    int range = GAMEDEF::spelldef(spell)->range;

    if (GAMEDEF::spelldef(spell)->blast)
    {
	POS		vpos = pos().delta(dx * radius, dy * radius);

	vpos.fireball(this, radius,
		    GAMEDEF::spelldef(spell)->symbol,
		    (ATTR_NAMES) GAMEDEF::spelldef(spell)->attr,
		    op);
    }
    else
    {
	// Ray + fireball
	doRangedAttack(range, 
		    radius,
		    dx, dy,
		    GAMEDEF::spelldef(spell)->symbol,
		    (ATTR_NAMES) GAMEDEF::spelldef(spell)->attr,
		    GAMEDEF::spelldef(spell)->verb,
		    targetself, GAMEDEF::spelldef(spell)->piercing,
		    op);
    }

    gainMP(-manacost);
    mySpellTimeout += timeout;
    
    return true;
}

bool
MOB::castDestroyWalls()
{
    formatAndReport("The walls around %S shake.");

    bool		interesting;
    interesting = pos().forceConnectNeighbours();

    if (interesting)
    {
	formatAndReport("New passages open up.");
    }
    else
    {
	formatAndReport("Nothing happens.");
    }

    return true;
}

bool
MOB::actionFleeBattleField()
{
    int		dx, dy;
    FORALL_4DIR(dx, dy)
    {
	POS	t = pos().delta(dx, dy);

	if (!t.valid() && !isAvatar())
	{
	    // Fall off the world and die!
	    formatAndReport("%S <flee> the battleground.");
	    actionDropAll();

	    MAP *map = pos().map();
	    pos().removeMob(this);
	    map->addDeadMob(this);
	    clearAllPos();
	    loseTempItems();
	    // Successfully walk!
	    return true;
	}
    }
    return false;
}

bool
MOB::actionWalk(int dx, int dy)
{
    MOB		*victim;
    POS		 t;

    // If we are swallowed, we can't move.
    if (isSwallowed())
    {
	stopRunning();
	return false;
    }
    
    t = pos().delta(dx, dy);
    
    victim = t.mob();
    if (victim)
    {
	// If we are the avatar, screw being nice!
	if (!isFriends(victim))
	    return actionMelee(dx, dy);

	// formatAndReport("%S <bump> into %O.", victim);
	// Bump into the mob.

	// Nah, tell the map we want a delayed move.
	// Do not delay move the avatar, however!
	if (victim->isAvatar() || isAvatar())
	    return false;

	return pos().map()->requestDelayedMove(this, dx, dy);
    }

    // Check to see if we can move that way.
    if (canMove(t))
    {
	TILE_NAMES	tile;
	
	// Determine if it is a special move..
	tile = t.tile();
	if (t.defn().isdiggable &&
	    !t.defn().ispassable &&	
	    defn().candig)
	{
	    stopRunning();
	    return t.digSquare();
	}

	// See if we are still running...
	int		dir = rand_dirtoangle(dx, dy);
	if (myLastDir != dir)
	{
	    stopRunning();
	    myLastDir = dir;
	}
	myLastDirCount++;
	if (myLastDirCount > 3 && isAvatar())
	{
	    if (!hasItem(ITEM_QUICKBOOST))
		addItem(ITEM::create(ITEM_QUICKBOOST));
	}

	POS		opos = pos();
	
	// Move!
	move(t);

	// If we are a breeder, chance to reproduce!
	if (defn().breeder)
	{
	    static  int		lastbreedtime = -10;

	    // Only breed every 7 phases at most so one can
	    // always kill off the breeders.
	    // Only breed while the player is in the dungeon
	    bool		 canbreed = false;

	    if (getAvatar() && getAvatar()->alive() &&
		getAvatar()->pos().roomId() == pos().roomId())
	    {
		canbreed = true;
	    }

	    // Hard cap
	    int		mobcount = pos().map()->getMobCount(getDefinition());
	    if (mobcount > 10)
		canbreed = false;

	    // Every new breeder increases the chance of breeding
	    // by 1 roll.  Thus we need at least the base * mobcount
	    // to just keep the breeding chance linear.
	    if (canbreed && 
		(lastbreedtime < map()->getTime() - 12) &&
		!rand_choice(3 + 3*mobcount))
	    {
		MOB		*child;
		
		formatAndReport("%S <reproduce>!");
		child = MOB::create(getDefinition());
		child->move(opos);
		lastbreedtime = map()->getTime();
	    }
	}
	
	// If we are the avatar and something is here, pick it up.
	// Disable as packrat just fills your backpack and we
	// want people addicted to transmuting
#if 0
	if (isAvatar() && t.item() && 
	    (t.item()->defn().itemclass != ITEMCLASS_FURNITURE))
	{
	    stopRunning();
	    actionPickup();
	}
#endif

	// Avatars set off traps because they are clumsy
	bool		hadtrap = false;
	if (isAvatar() && t.isTrap())
	{
	    hadtrap = true;
	    t.clearTrap();
	    TRAP_NAMES		trap = (TRAP_NAMES) rand_choice(NUM_TRAPS);

	    t.postEvent((EVENTTYPE_NAMES)(EVENTTYPE_ALL | EVENTTYPE_LONG), 
			    glb_trapdefs[trap].sym,
			    (ATTR_NAMES) glb_trapdefs[trap].attr);
	    DPDF	dpdf(0);

	    formatAndReport("%S <set> off %O!", glb_trapdefs[trap].name);
	    stopRunning();

	    if (glb_trapdefs[trap].item != ITEM_NONE)
	    {
		ITEM		*item = ITEM::create((ITEM_NAMES)glb_trapdefs[trap].item);
		item->setStackCount(1);
		addItem(item);
	    }

	    applyDamage(0, dpdf.evaluate(), 
			    (ELEMENT_NAMES) glb_trapdefs[trap].element,
			    ATTACKSTYLE_MELEE);
	}

	// Apply searching

#if 0
	// Attempt to climb!
	if (alive() && isAvatar() && t.tile() == TILE_DOWNSTAIRS)
	{
	    msg_report("Press > to climb down.");
	}
	// Attempt to climb!
	if (alive() && isAvatar() && t.tile() == TILE_UPSTAIRS)
	{
	    msg_report("Press < to climb up.");
	}
#endif

	return true;
    }
    else
    {
	if (dx && dy && isAvatar())
	{
	    // Try to wall slide, cause we are pretty real time here
	    // and it is frustrating to navigate curvy passages otherwise.
	    if (!rand_choice(2) && canMoveDir(dx, 0, false))
		if (actionWalk(dx, 0))
		    return true;
	    if (canMoveDir(0, dy, false))
		if (actionWalk(0, dy))
		    return true;
	    if (canMoveDir(dx, 0, false))
		if (actionWalk(dx, 0))
		    return true;
	}
	else if ((dx || dy) && isAvatar())
	{
	    // If we have
	    // ..
	    // @#
	    // ..
	    // Moving right we want to slide to a diagonal.
	    int		sdx, sdy;

	    // This bit of code is too clever for its own good!
	    sdx = !dx;
	    sdy = !dy;

	    if (!rand_choice(2) && canMoveDir(dx+sdx, dy+sdy, false))
		if (actionWalk(dx+sdx, dy+sdy))
		    return true;
	    if (canMoveDir(dx-sdx, dy-sdy, false))
		if (actionWalk(dx-sdx, dy-sdy))
		    return true;
	    if (canMoveDir(dx+sdx, dy+sdy, false))
		if (actionWalk(dx+sdx, dy+sdy))
		    return true;
	    
	}

	stopRunning();
	formatAndReport("%S <be> blocked by %O.", t.defn().legend);
	if (isAvatar())
	    msg_newturn();
	// Bump into a wall.
	return false;
    }
}

bool
MOB::actionTransmute()
{
    stopRunning();
    ITEM		*item;

    item = pos().item();
    if (!item)
    {
	formatAndReport("%S <fail> to fuse the empty air.");
	return false;
    }

    if (item->pos() != pos())
    {
	formatAndReport("%S <be> too far away to fuse %O.", item);
	return false;
    }

    // Transmute the item.
    formatAndReport("%S <fuse> %O.", item);

    // Fail to transmute things with no base materials.
    if (item->material() == MATERIAL_NONE || item->getDefinition() == ITEM_COIN)
    {
	msg_format("%S <shake> violently, but remains intact.", item);
	return true;
    }

    // Create the coins
    msg_format("%S <dissolve> into its component parts.", item);

    item->setInterestedUID(INVALID_UID);
    item->move(POS());

    ITEM		*coin;
    coin = ITEM::create(ITEM_COIN);
    coin->setMaterial(item->material());
    coin->setStackCount(item->defn().mass * item->getStackCount());
    coin->silentMove(pos());
    actionPickup(coin);

    if (item->giltMaterial() != MATERIAL_NONE)
    {
	coin = ITEM::create(ITEM_COIN);
	coin->setMaterial(item->giltMaterial());
	coin->setStackCount(1 * item->getStackCount());
	coin->silentMove(pos());
	actionPickup(coin);
    }

    incTransmuteTaint();

    delete item;
    return true;
}

bool
MOB::actionTransmute(ITEM *item)
{
    stopRunning();

    // Make sure we have an equipped item to accept the power.
    ITEM *donee = nullptr;

    if (item->isWeapon())
    {
	donee = lookupWeapon();
    }
    if (item->isRing())
    {
	donee = lookupRing();
    }

    if (!donee)
    {
	formatAndReport("%S <have> nothing equipped to absorb %O.", item);
	return true;
    }

    if (donee == item)
    {
	formatAndReport("%S <realize> that destroying %O to empower itself won't work.", item);
	return true;
    }

    // Transmute the item.
    formatAndReport("%S <fuse> %O into your equipped item.", item);

    int		power = item->getLevel();
    power += GAMEDEF::materialdef(item->material())->depth;
    // Make it easier to charge rings as they are all one material.
    if (item->isRing())
	power += 2;
    power *= 30;
    power += item->getExp() / 4;

    item = splitStack(item);
    delete item;

    formatAndReport("%O gains power.", donee);

    donee->gainExp(power, /*silent=*/false);

    return true;
}

bool
MOB::actionSetLeader(int partyidx)
{
    MAP *map = pos().map();
    J_ASSERT(map);

    MOB *leader = map->party().member(partyidx);
    if (!leader)
    {
	formatAndReport("No one is in that slot to take the spot.");
	msg_newturn();
	return false;
    }

    if (leader == this)
    {
	formatAndReport("%S <be> already the leader.");
	msg_newturn();
	return false;
    }

    // Swap!
    formatAndReport("%S <swap> places with %O.", leader);

    leader->move(pos());
    move(map->astralLoc());

    map->party().makeActive(partyidx);
    map->setAvatar(leader);

    return true;
}

bool
MOB::actionPickup()
{
    stopRunning();
    ITEM		*item;

    item = pos().item();

    return actionPickup(item);
}

bool
MOB::actionPickup(ITEM *item)
{
    stopRunning();
    if (!item)
    {
	formatAndReport("%S <grope> the ground foolishly.");
	return true;
    }

    if (item->pos() != pos())
    {
	formatAndReport("%S <be> too far away to pick up %O.", item);
	return false;
    }

    // Pick up the item!
    formatAndReport("%S <pick> up %O.", item);

    // No longer coveted!
    item->setInterestedUID(INVALID_UID);
    item->move(POS());

    if (item->defn().discardextra && hasItem(item->getDefinition()))
    {
	// Replace broken items...
	ITEM		*olditem = lookupItem(item->getDefinition());
	
	if (olditem->isBroken() && !item->isBroken())
	{
	    formatAndReport("%S <replaces> %O.", olditem);
	    olditem->setBroken(false);
	}

	formatAndReport("%S already <have> one of these, so %S <discard> it.");

	delete item;
    }
    else
	addItem(item);

    return true;
}

void
MOB::updateEquippedItems()
{
    ITEM		*item;

    // Non avatars auto equip.
    if (!isAvatar() && getDefinition() != MOB_AVATAR)
    {
	// Mark everything unequipped
	for (int i = 0; i < myInventory.entries(); i++)
	{
	    item = myInventory(i);

	    if (!item->isRing())
		item->setEquipped(false);
	}

	// Equip our weapon and armour
	ITEM		*weap = 0, *arm = 0, *wand = 0;

	for (int i = 0; i < myInventory.entries(); i++)
	{
	    if (myInventory(i)->isWeapon() && !myInventory(i)->isBroken())
		weap = aiLikeMoreWeapon(myInventory(i), weap);
	}
	if (weap)
	    weap->setEquipped(true);
	for (int i = 0; i < myInventory.entries(); i++)
	{
	    if (myInventory(i)->isRanged() && !myInventory(i)->isBroken())
		wand = aiLikeMoreWand(myInventory(i), wand);
	}
	if (wand)
	    wand->setEquipped(true);
	for (int i = 0; i < myInventory.entries(); i++)
	{
	    if (myInventory(i)->isArmour() && !myInventory(i)->isBroken())
		arm = aiLikeMoreArmour(myInventory(i), arm);
	}
	if (arm)
	    arm->setEquipped(true);
    }

    // Count total items
    int		total = 0;
    for (int i = 0; i < myInventory.entries(); i++)
    {
	if (myInventory(i)->isFlag())
	    continue;
	total++;
    }
    const int max_items = 20;
    if (total > max_items)
    {
	// Yay!  Chacne to use count down operator.
	// We want to run in reverse here.
	formatAndReport("%r backpack is out of room!");
	for (int i = myInventory.entries(); i --> 0; )
	{
	    item = myInventory(i);
	    if (item->isFlag())
		continue;

	    formatAndReport("%S <drop> %O.", item);
	    item->move(pos());
	    item->setEquipped(false);
	    myInventory.removeAt(i);

	    total--;
	    if (total <= max_items)
		break;
	}
    }
}

void
MOB::addItem(ITEM *item)
{
    int			 i;

    if (!item)
	return;
    
    // Alert the world to our acquirement.
    if (isAvatar() || pos().isFOV())
	if (item->defn().gaintxt)
	    formatAndReport(item->defn().gaintxt, item);

    // First, check to see if we can merge...
    for (i = 0; i < myInventory.entries(); i++)
    {
	if (item->canStackWith(myInventory(i)))
	{
	    myInventory(i)->combineItem(item);
	    delete item;
	    // Pop that inventory to the top.
	    ITEM *merge = myInventory(i);
	    myInventory.removePtr(merge);
	    myInventory.append(merge);
	    return;
	}
    }

    // Brand new item.
    myInventory.append(item);

    // If we didn't stack, we may need to update our best items.
    updateEquippedItems();
}

ITEM *
MOB::splitStack(ITEM *item)
{
    if (item->getStackCount() > 1)
    {
	ITEM	*result;

	item->decStackCount();
	result = item->createCopy();
	result->setStackCount(1);

	return result;
    }
    else
    {
	removeItem(item);
	return item;
    }
}

void
MOB::removeItem(ITEM *item, bool quiet)
{
    // Alert the world to our acquirement.
    if ((isAvatar() || pos().isFOV()) && !quiet)
	if (item->defn().losetxt)
	    formatAndReport(item->defn().losetxt, item);

    if (item)
	item->setEquipped(false);
    myInventory.removePtr(item);

    updateEquippedItems();
}

void
MOB::loseAllItems()
{
    int		i;
    ITEM	*item;

    for (i = myInventory.entries(); i --> 0;)
    {
	item = myInventory(i);
	removeItem(item, true);
	delete item;
    }
}

void
MOB::loseTempItems()
{
    int		i;
    ITEM	*item;

    for (i = myInventory.entries(); i --> 0;)
    {
	item = myInventory(i);
	// All flags and timers are temp.
	if (item->getTimer() >= 0 || item->isFlag())
	{
	    removeItem(item, true);
	    delete item;
	}
    }
}

void
MOB::save(ostream &os) const
{
    int		val;
    u8		c;

    val = getDefinition();
    os.write((const char *) &val, sizeof(int));

    myName.save(os);

    myPos.save(os);
    myMeditatePos.save(os);
    myTarget.save(os);
    myHome.save(os);

    c = myWaiting;
    os.write((const char *) &c, sizeof(c));

    os.write((const char *) &myHP, sizeof(int));
    os.write((const char *) &myMP, sizeof(int));
    os.write((const char *) &myFood, sizeof(int));
    os.write((const char *) &myAIState, sizeof(int));
    os.write((const char *) &myFleeCount, sizeof(int));
    os.write((const char *) &myBoredom, sizeof(int));
    os.write((const char *) &myYellHystersis, sizeof(int));
    os.write((const char *) &myNumDeaths, sizeof(int));
    os.write((const char *) &myUID, sizeof(int));
    os.write((const char *) &myNextStackUID, sizeof(int));
    os.write((const char *) &mySeed, sizeof(int));
    os.write((const char *) &myLevel, sizeof(int));
    os.write((const char *) &myExp, sizeof(int));
    os.write((const char *) &myExtraLives, sizeof(int));
    os.write((const char *) &mySearchPower, sizeof(int));
    os.write((const char *) &myCowardice, sizeof(int));
    os.write((const char *) &myKills, sizeof(int));
    os.write((const char *) &myRangeTimeout, sizeof(int));
    os.write((const char *) &mySpellTimeout, sizeof(int));

    os.write((const char *) &myLastDir, sizeof(int));
    os.write((const char *) &myLastDirCount, sizeof(int));

    val = myHasTriedSuicide;
    os.write((const char *) &val, sizeof(int));

    val = myHero;
    os.write((const char *) &val, sizeof(int));

    val = isSwallowed();
    os.write((const char *) &val, sizeof(int));

    val = myAngryWithAvatar;
    os.write((const char *) &val, sizeof(int));

    int			 numitem;
    int			 i;

    c = mySawMurder;
    os.write((const char *) &c, 1);
    c = mySawMeanMurder;
    os.write((const char *) &c, 1);
    c = mySawVictory;
    os.write((const char *) &c, 1);
    c = myAvatarHasRanged;
    os.write((const char *) &c, 1);

    YELL_NAMES		yell;
    FOREACH_YELL(yell)
    {
	c = myHeardYell[yell];
	os.write((const char *) &c, 1);
	c = myHeardYellSameRoom[yell];
	os.write((const char *) &c, 1);
    }

    numitem = myInventory.entries();
    os.write((const char *) &numitem, sizeof(int));

    for (i = 0; i < myInventory.entries(); i++)
    {
	myInventory(i)->save(os);
    }
}

MOB *
MOB::load(istream &is)
{
    int		 val, num, i;
    u8		 c;
    MOB		*mob;

    mob = new MOB();

    is.read((char *)&val, sizeof(int));
    mob->myDefinition = (MOB_NAMES) val;

    if (mob->myDefinition >= GAMEDEF::getNumMob())
    {
	mob->myDefinition = MOB_NONE;
    }

    mob->myName.load(is);

    mob->myPos.load(is);
    mob->myMeditatePos.load(is);
    mob->myTarget.load(is);
    mob->myHome.load(is);

    is.read((char *) &c, sizeof(c));
    mob->myWaiting = c ? true : false;
    
    is.read((char *)&mob->myHP, sizeof(int));
    is.read((char *)&mob->myMP, sizeof(int));
    is.read((char *)&mob->myFood, sizeof(int));
    is.read((char *)&mob->myAIState, sizeof(int));
    is.read((char *)&mob->myFleeCount, sizeof(int));
    is.read((char *)&mob->myBoredom, sizeof(int));
    is.read((char *)&mob->myYellHystersis, sizeof(int));
    is.read((char *)&mob->myNumDeaths, sizeof(int));
    is.read((char *)&mob->myUID, sizeof(int));
    is.read((char *)&mob->myNextStackUID, sizeof(int));
    is.read((char *)&mob->mySeed, sizeof(int));
    is.read((char *)&mob->myLevel, sizeof(int));
    is.read((char *)&mob->myExp, sizeof(int));
    is.read((char *)&mob->myExtraLives, sizeof(int));
    is.read((char *)&mob->mySearchPower, sizeof(int));
    is.read((char *)&mob->myCowardice, sizeof(int));
    is.read((char *)&mob->myKills, sizeof(int));
    is.read((char *)&mob->myRangeTimeout, sizeof(int));
    is.read((char *)&mob->mySpellTimeout, sizeof(int));

    is.read((char *) &mob->myLastDir, sizeof(int));
    is.read((char *) &mob->myLastDirCount, sizeof(int));

    is.read((char *)&val, sizeof(int));
    mob->myHasTriedSuicide = val ? true : false;

    is.read((char *)&val, sizeof(int));
    mob->myHero = (HERO_NAMES) val;

    is.read((char *)&val, sizeof(int));
    mob->setSwallowed(val ? true : false);

    is.read((char *)&val, sizeof(int));
    mob->myAngryWithAvatar = (val ? true : false);

    is.read((char *)&c, 1);
    mob->mySawMurder = (c ? true : false);
    is.read((char *)&c, 1);
    mob->mySawMeanMurder = (c ? true : false);
    is.read((char *)&c, 1);
    mob->mySawVictory = (c ? true : false);
    is.read((char *)&c, 1);
    mob->myAvatarHasRanged = (c ? true : false);
    
    YELL_NAMES		yell;
    FOREACH_YELL(yell)
    {
	is.read((char *)&c, 1);
	mob->myHeardYell[yell] = (c ? true : false);
	is.read((char *)&c, 1);
	mob->myHeardYellSameRoom[yell] = (c ? true : false);
    }

    is.read((char *)&num, sizeof(int));

    for (i = 0; i < num; i++)
    {
	mob->myInventory.append(ITEM::load(is));
    }

    return mob;
}

ITEM *
MOB::getItemFromId(int itemid) const
{
    for (int i = 0; i < myInventory.entries(); i++) 
    { 
	if (myInventory(i)->getUID() == itemid) 
	    return myInventory(i);
    } 
    return 0; 
}

bool
MOB::hasVisibleEnemies() const
{
    PTRLIST<MOB *> list;

    getVisibleEnemies(list);
    if (list.entries())
	return true;
    return false;
}

ITEM *
MOB::getWieldedOrTopBackpackItem() const
{
    ITEM *item = lookupWeapon();
    if (item)
	return item;
    return getTopBackpackItem();
}

ITEM *
MOB::getTopBackpackItem(int depth) const
{
    for (int n = myInventory.entries(); n --> 0; )
    {
	ITEM *drop = myInventory(n);
	if (drop->isFlag())
	    continue;
	if (drop->isEquipped())
	    continue;

	if (!depth)
	    return drop;
	depth--;
    }
    return 0;
}

bool
MOB::actionDropTop()
{
    stopRunning();

    ITEM		*drop = getTopBackpackItem();

    if (drop)
	return actionDrop(drop);

    formatAndReport("%S <have> nothing to drop.");
    return true;
}

bool
MOB::actionDropAll()
{
    stopRunning();

    ITEM		*drop = 0;
    int		         dropped = 0;

    // Cap the loop to backpack size in case something goes horrificaly wrong.
    while (dropped < 20)
    {
	ITEM		*drop = getTopBackpackItem();

	if (!drop)
	    break;

	// found a droppable.
	actionDrop(drop);
	dropped++;
    }

    if (!dropped && isAvatar())
	formatAndReport("%S <have> nothing to drop.");

    return true;
}

bool
MOB::actionDropSurplus()
{
    stopRunning();
    ITEM		*weap, *wand;

    weap = lookupWeapon();
    wand = lookupWand();
    for (int i = 0; i < myInventory.entries(); i++)
    {
	if (myInventory(i) == weap)
	    continue;
	if (myInventory(i) == wand)
	    continue;
	if (myInventory(i)->isPotion())
	    continue;

	if (myInventory(i)->isFlag())
	    continue;

	return actionDrop(myInventory(i));
    }
    // Check if our wand/weap is redundant
    if (weap && weap->getStackCount() > 1)
    {
	return actionDropButOne(weap);
    }
    if (wand && wand->getStackCount() > 1)
    {
	return actionDropButOne(wand);
    }
    return false;
}

bool
MOB::actionBagShake()
{
    myInventory.shuffle();
    formatAndReport("%S <shake> %r backpack.");
    return true;
}

bool
MOB::actionBagSwapTop()
{
    // Find top non-held and non-flag items.
    ITEM		*top = getTopBackpackItem();
    ITEM		*nexttop = getTopBackpackItem(1);

    if (nexttop)
    {
	formatAndReport("%S <bury> %O farther into %r backpack.", top);
	myInventory.removePtr(nexttop);
	myInventory.append(nexttop);
    }
    else
    {
	formatAndReport("%S <fiddle> with %r backpack.");
    }

    return true;
}

bool
MOB::hasSurplusItems() const
{
    bool	hasrange = false;
    bool	hasmelee = false;

    for (int i = 0; i < myInventory.entries(); i++)
    {
	// We can always drink these.
	if (myInventory(i)->isPotion())
	    continue;
	if (myInventory(i)->isFlag())
	    continue;
	// Gold isn't surplus!
	if (myInventory(i)->getDefinition() == ITEM_COIN)
	    continue;
	// Stuff we wear is important!
	if (myInventory(i)->isEquipped())
	    continue;
	return true;
    }

    return false;
}

int
MOB::numSurplusRange() const
{
    int		numrange = 0;

    for (int i = 0; i < myInventory.entries(); i++)
    {
	if (myInventory(i)->isRanged())
	{
	    numrange += myInventory(i)->getStackCount();
	}
    }

    // Reserve one range for our own use.
    return numrange - 1;
}

void
MOB::getVisibleEnemies(PTRLIST<MOB *> &list) const
{
    // If we are blind, we can't see anyone.
    if (hasItem(ITEM_BLIND))
	return;

    // We only care about the non-avatar case now.
    if (isAvatar())
	return;

    // Check if we are in FOV.  If so, the avatar is visible.
    if (getAvatar())
    {
	// Ignore dead avatars.
	if (!getAvatar()->alive())
	{
	    return;
	}
	// Ignore none hostile
	if (isFriends(getAvatar()))
	    return;

	// We need the double check because if you are meditating
	// and put them in the fov, that doesn't mean that 
	// the avatar is now in fov.
	if (pos().isFOV() && getAvatar()->pos().isFOV())
	{
	    list.append(getAvatar());
	}
    }
}

void
MOB::formatAndReport(const char *msg)
{
    if (isAvatar() || pos().isFOV())
    {
	msg_format(msg, this);
    }
}

void
MOB::formatAndReport(const char *msg, MOB *object)
{
    if (isAvatar() || pos().isFOV())
    {
	msg_format(msg, this, object);
    }
}

void
MOB::formatAndReport(const char *msg, const char *verb, MOB *object)
{
    if (isAvatar() || pos().isFOV())
    {
	msg_format(msg, this, verb, object);
    }
}

void
MOB::formatAndReport(const char *msg, ITEM *object)
{
    if (isAvatar() || pos().isFOV())
    {
	msg_format(msg, this, object);
    }
}

void
MOB::formatAndReport(const char *msg, const char *object)
{
    if (isAvatar() || pos().isFOV())
    {
	msg_format(msg, this, object);
    }
}

int
MOB::numberMeleeHostiles() const
{
    int		dx, dy;
    MOB		*mob;
    int		hostiles = 0;

    FORALL_8DIR(dx, dy)
    {
	mob = pos().delta(dx, dy).mob();

	if (mob && !mob->isFriends(this))
	    hostiles++;
    }
    return hostiles;
}

void
MOB::clearCollision()
{
    myCollisionSources.clear();
    myCollisionTarget = 0;
}

void
MOB::setCollisionTarget(MOB *target)
{
    if (myCollisionTarget == target)
	return;

    J_ASSERT(!target || !myCollisionTarget);

    myCollisionTarget = target;
    if (target)
	target->collisionSources().append(this);
}

bool
MOB::actionPortalFire(int dx, int dy, int portal)
{
    stopRunning();
    // No suicide.
    if (!dx && !dy)
    {
	formatAndReport("%S <decide> not to aim at %O.", this);
	return false;
    }
    
    // If swallowed, rather useless.
    if (isSwallowed())
    {
	formatAndReport("%S do not have enough room inside here.");
	return false;
    }

    clearBoredom();

    // Always display now.
 
    u8		symbol = '*';
    ATTR_NAMES	attr;
    int		portalrange = 60;

    attr = ATTR_BLUE;
    if (portal)
	attr = ATTR_ORANGE;
    
    pos().displayBullet(portalrange,
			dx, dy,
			symbol, attr,
			false);

    POS		vpos;
    vpos = pos().traceBulletPos(portalrange, dx, dy, false, false);

    TILE_NAMES	wall_tile = (TILE_NAMES) vpos.roomDefn().wall_tile;
    
    if (vpos.tile() != wall_tile && vpos.tile() != TILE_PROTOPORTAL)
    {
	msg_format("The portal dissipates at %O.", 0, vpos.defn().legend);
	return false;
    }

    if (!vpos.roomDefn().allowportal)
    {
	formatAndReport("The walls of this area seem unable to hold portals.");
	return false;
    }


    // We want sloppy targeting for portals.
    // This means that given a portal location of # fired at from the south,
    // we want:
    //
    // 1#1
    // 2*2
    //
    // as potential portals.
    //
    // One fired from the south-west
    //  2
    // 1#2
    // *1

    POS		alt[5];

    if (dx && dy)
    {
	alt[0] = vpos;
	alt[1] = vpos.delta(-dx, 0);
	alt[2] = vpos.delta(0, -dy);
	alt[3] = vpos.delta(0, dy);
	alt[4] = vpos.delta(dx, 0);
    }
    else if (dx)
    {
	alt[0] = vpos;
	alt[1] = vpos.delta(0, 1);
	alt[2] = vpos.delta(0, -1);
	alt[3] = vpos.delta(-dx, 1);
	alt[4] = vpos.delta(-dx, -1);
    }
    else
    {
	alt[0] = vpos;
	alt[1] = vpos.delta(1, 0);
	alt[2] = vpos.delta(-1, 0);
	alt[3] = vpos.delta(1, -dy);
	alt[4] = vpos.delta(-1, -dy);
    }

    for (int i = 0; i < 5; i++)
    {
	if (buildPortalAtLocation(alt[i], portal))
	    return true;
    }
    msg_report("The wall proved too unstable to hold a portal.");

    return false;
}

bool
MOB::buildPortalAtLocation(POS vpos, int portal) const
{
    int		origangle;

    origangle = vpos.angle();

    TILE_NAMES	wall_tile = (TILE_NAMES) vpos.roomDefn().wall_tile;
    TILE_NAMES	tunnelwall_tile = (TILE_NAMES) vpos.roomDefn().tunnelwall_tile;
    TILE_NAMES	floor_tile = (TILE_NAMES) vpos.roomDefn().floor_tile;

    // We want all our portals in the same space.
    vpos.setAngle(0);

    // Check if it is a valid portal pos?
    if (!vpos.prepSquareForDestruction())
    {
	return false;
    }

    // Verify the portal is well formed, ie, three neighbours are now
    // walls and the other is a floor.
    int		dir, floordir = -1;
    TILE_NAMES	tile;

    for (dir = 0; dir < 4; dir++)
    {
	tile = vpos.delta4Direction(dir).tile();
	if (tile == floor_tile)
	{
	    if (floordir < 0)
		floordir = dir;
	    else
	    {
		// Uh oh.
		return false;

	    }
	}
	// Note invalid shouldn't occur here as we will have transformed
	// that into a wall in prep for destrction.
	else if (tile == wall_tile || tile == TILE_PROTOPORTAL ||
		 tile == tunnelwall_tile || tile == TILE_INVALID)
	{
	    // All good.
	}
	else
	{
	    // Uh oh.
	    return false;
	}
    }

    // Failed to find a proper portal.
    if (floordir < 0)
	return false;

    // In floordir+2 we will be placing the mirror of the opposite
    // portal.  It is important that square is not accessible.  We
    // merely make sure it isn't a floor
    // Still an issue of having ants dig it out.  Ideally we'd
    // have the virtual portal flagged with MAPFLAG_PORTAL but
    // we don't do that currently in buildPortal and doing so
    // would mean we'd have to clean it up properly.

    POS		virtualportal;

    virtualportal = vpos.delta4Direction((floordir+2)&3);
    virtualportal = virtualportal.delta4Direction((floordir+2)&3);

    // We now point to the square behind the proposed virtual portal
    // this should be wall or invalid
    tile = virtualportal.tile();
    if (tile != wall_tile && tile != tunnelwall_tile && tile != TILE_PROTOPORTAL && tile != TILE_INVALID)
	return false;
    // Try neighbours.
    tile = virtualportal.delta4Direction((floordir+1)&3).tile();
    if (tile != wall_tile && tile != tunnelwall_tile && tile != TILE_PROTOPORTAL && tile != TILE_INVALID)
	return false;
    tile = virtualportal.delta4Direction((floordir-1)&3).tile();
    if (tile != wall_tile && tile != tunnelwall_tile && tile != TILE_PROTOPORTAL && tile != TILE_INVALID)
	return false;

    // We now know we aren't an isolated island.  Yet.

    vpos.map()->buildUserPortal(vpos, portal, (floordir+2) & 3, origangle);

    return true;
}

