/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        item.cpp ( Live Once Library, C++ )
 *
 * COMMENTS:
 */

#include "item.h"
#include "gamedef.h"
#include "mob.h"

#include <assert.h>

#include <iostream>
using namespace std;

#include "grammar.h"
#include "text.h"
#include "engine.h"
#include "msg.h"
#include "thesaurus.h"
#include "level.h"

int glbUniqueId = 1;

static PTRLIST<bool> glbItemIded;
static PTRLIST<int> glbItemMagicClass;

struct COLOURPAIR
{
    ATTR_NAMES	attr;
    const char *name;
};

static const COLOURPAIR glbColors[] =
{
    { ATTR_WHITE, "white" },
    { ATTR_GOLD, "gold" },
    { ATTR_PINK, "pink" },
    { ATTR_PURPLE, "purple" },
    { ATTR_LIGHTBLACK, "midnight" },
    { ATTR_ORANGE, "orange" },
    { ATTR_LIGHTBROWN, "tan" },
    { ATTR_BROWN, "brown" },
    { ATTR_RED, "red" },
    { ATTR_DKRED, "blood" },
    { ATTR_GREEN, "green" },
    { ATTR_DKGREEN, "olive" },
    { ATTR_BLUE, "blue" },
    { ATTR_TEAL, "teal" },
    { ATTR_CYAN, "cyan" },
    { ATTR_GREY, "grey" },
    { ATTR_FIRE, "glowing" },
    { ATTR_NONE, 0 }
};

MATERIAL_NAMES
item_randmaterial(int depth, bool gilt)
{
    MATERIAL_NAMES		choice = MATERIAL_NONE;
    int				nvalid = 0;

    for (MATERIAL_NAMES material = MATERIAL_NONE; material < (MATERIAL_NAMES) GAMEDEF::getNumMaterial(); material = (MATERIAL_NAMES) (material+1))
    {
	if (material == MATERIAL_NONE)
	    continue;
	if (gilt && !GAMEDEF::materialdef(material)->gildable)
	    continue;
	if (GAMEDEF::materialdef(material)->depth <= depth)
	{
	    nvalid++;
	    if (!rand_choice(nvalid))
	    {
		choice = material;
	    }
	}
    }

    return choice;
}

int 
glb_allocUID()
{
    return glbUniqueId++;
}

void
glb_reportUID(int uid)
{
    if (uid >= glbUniqueId)
	glbUniqueId = uid+1;
}


//
// Item Definitions
//

ITEM::ITEM()
{
    myDefinition = (ITEM_NAMES) 0;
    myCount = 1;
    myTimer = -1;
    myUID = INVALID_UID;
    myNextStackUID = INVALID_UID;
    myInterestedMobUID = INVALID_UID;
    myBroken = false;
    myEquipped = false;
    myMobType = MOB_NONE;
    myHero = HERO_NONE;
    myElement = ELEMENT_NONE;
    myMaterial = myGiltMaterial = MATERIAL_NONE;
    myLevel = 1;
    myExp = 0;
}

ITEM::~ITEM()
{
    pos().removeItem(this);
}

void
ITEM::move(POS pos)
{
    myPos.removeItem(this);
    myPos = pos;
    myPos.addItem(this);
}

void
ITEM::clearAllPos()
{
    myPos = POS();
}

ITEM *
ITEM::copy() const
{
    ITEM *item;

    item = new ITEM();

    *item = *this;

    return item;
}

ITEM *
ITEM::createCopy() const
{
    ITEM *item;

    item = copy();

    item->myUID = glb_allocUID();
    item->myNextStackUID = INVALID_UID;

    return item;
}

ITEM *
ITEM::create(ITEM_NAMES item, int depth)
{
    ITEM	*i;

    J_ASSERT(item >= ITEM_NONE && item < GAMEDEF::getNumItem());

    i = new ITEM();

    i->myDefinition = item;
    i->myTimer = i->defn().timer;
    i->setStackCount(i->defn().startstack);

    i->myUID = glb_allocUID();
    i->mySeed = rand_int();

    if (i->defn().startsbroken)
	i->myBroken = true;

    if (i->defn().needsmaterial)
    {
	i->myMaterial = item_randmaterial(depth, false);
	if (i->defn().gildable)
	{
	    if (!rand_choice(3))
	    {
		i->myGiltMaterial = item_randmaterial(depth, true);
	    }
	}
    }

    if (i->isRing())
    {
	// we boost the level of rings so you can eat them better
	int itemlevel = rand_choice(depth/2);
	itemlevel = MIN(itemlevel, LEVEL::MAX_LEVEL);
	i->gainExp(LEVEL::expFromLevel(itemlevel), /*silent=*/true);
    }

    return i;
}

ITEM *
ITEM::createRandomBalanced(int depth)
{
    // Decide on type
    ITEMCLASS_NAMES	choices[] =
    {
	ITEMCLASS_WEAPON,
	ITEMCLASS_POTION,
	ITEMCLASS_SPELLBOOK,
	ITEMCLASS_RING
    };

    ITEMCLASS_NAMES	itemclass = choices[0];
    int choice = 0;
    for (int i = 0; i < sizeof(choices)/sizeof(ITEMCLASS_NAMES); i++)
    {
	if (rand_choice(choice + GAMEDEF::itemclassdef(choices[i])->rarity) < GAMEDEF::itemclassdef(choices[i])->rarity)
	{
	    itemclass = choices[i];
	}
	choice += GAMEDEF::itemclassdef(choices[i])->rarity;
    }

    ITEM	*item = 0;

    int		matchingdepth = 0;
    choice = 0;
    ITEM_NAMES	itemname = ITEM_NONE;

    for (ITEM_NAMES i = ITEM_NONE; i < (ITEM_NAMES) GAMEDEF::getNumItem(); i = (ITEM_NAMES) (i+1))
    {
	// Ignore mismatching.
	if (!defn(i).depth)
	    continue;

	if (defn(i).itemclass != itemclass)
	    continue;

	if (defn(i).depth <= depth)
	{
	    if (defn(i).depth == depth)
		matchingdepth++;

	    if (rand_choice(choice + defn(i).rarity) < defn(i).rarity)
		itemname = (ITEM_NAMES) i;
	    choice += defn(i).rarity;
	}
    }

    if (itemname == ITEM_NONE)
	return 0;

    return create(itemname, depth);
}

ITEM *
ITEM::createRandom(int depth)
{
    ITEM_NAMES		item;

    item = itemFromHash(rand_choice(65536*32767), depth);
    if (item == ITEM_NONE)
	return 0;
    return create(item, depth);
}

ITEM_NAMES
ITEM::itemFromHash(unsigned int hash, int depth)
{
    int		totalrarity, rarity;
    int		i;

    // Find total rarity of items
    totalrarity = 0;
    for (i = ITEM_NONE; i < GAMEDEF::getNumItem(); i++)
    {
	rarity = defn((ITEM_NAMES)i).rarity;
	if (defn((ITEM_NAMES)i).depth > depth)
	    rarity = 0;
	totalrarity += rarity;
    }

    hash %= totalrarity;
    for (i = ITEM_NONE; i < GAMEDEF::getNumItem(); i++)
    {
	rarity = defn((ITEM_NAMES)i).rarity;
	if (defn((ITEM_NAMES)i).depth > depth)
	    rarity = 0;

	if (hash > (unsigned) rarity)
	    hash -= rarity;
	else
	    break;
    }
    return (ITEM_NAMES) i;
}

int
ITEM::getMagicClass() const
{
    return glbItemMagicClass.safeIndexAnywhere(getDefinition());
}

bool
ITEM::isMagicClassKnown() const
{
    return glbItemIded.safeIndexAnywhere(getDefinition());
}

void
ITEM::markMagicClassKnown()
{
    if (!glbItemIded.indexAnywhere(getDefinition()))
    {
	BUF		origname;
	origname = getSingleArticleName();
	glbItemIded.indexAnywhere(getDefinition()) = true;
	BUF		msgtext, newname;
	newname = getSingleArticleName();
	msgtext.sprintf("You discover that %s is %s.", origname.buffer(), newname.buffer());
	msg_report(msgtext);
    }
}

VERB_PERSON
ITEM::getPerson() const
{
    return VERB_IT;
}

BUF
ITEM::getSingleName() const
{
    BUF		buf, basename;

    basename = getRawName();

    if (mobType() != MOB_NONE)
    {
	if (hero() != HERO_NONE)
	{
	    const char *heroname = GAMEDEF::herodef(hero())->name;
	    buf.sprintf("%s of %s", basename.buffer(), heroname);
	    basename = buf;
	}
	else
	{
	    const char *mobname = GAMEDEF::mobdef(mobType())->name;
	    buf.sprintf("%s %s", mobname, basename.buffer());
	    basename = buf;
	}
    }
    if (element() != ELEMENT_NONE)
    {
	const char *elementname = GAMEDEF::elementdef(element())->name;
	buf.sprintf("%s %s", basename.buffer(), elementname);
	basename = buf;
    }
    if (defn().thesaurus)
    {
	basename = thesaurus_expand(mySeed, getDefinition(), material(), giltMaterial());
    }
    else
    {
	if (material() != MATERIAL_NONE)
	{
	    const char *materialname = GAMEDEF::materialdef(material())->adjname;
	    buf.sprintf("%s %s", materialname, basename.buffer());
	    basename = buf;
	}
	if (giltMaterial() != MATERIAL_NONE)
	{
	    const char *materialname = GAMEDEF::materialdef(giltMaterial())->name;
	    buf.sprintf("%s-chased %s", materialname, basename.buffer());
	    basename = buf;
	}
    }

    if (getTimer() >= 0)
    {
	// A timed item...
	buf.sprintf("%s (%d)", basename.buffer(), getTimer());
    }
    else
    {
	// A normal item.
	buf = basename;
    }
    if (isBroken())
    {
	BUF		broken;
	broken.sprintf("broken %s", buf.buffer());
	buf.strcpy(broken);
    }

    return buf;
}

BUF
ITEM::getName() const
{
    BUF		buf = getSingleName();
    return gram_createcount(buf, myCount, false);
}

BUF
ITEM::getArticleName() const
{
    BUF		name = getName();
    BUF		result;

    result.sprintf("%s%s", gram_getarticle(name.buffer()), name.buffer());

    return result;
}

BUF
ITEM::getSingleArticleName() const
{
    BUF		name = getSingleName();
    BUF		result;

    result.sprintf("%s%s", gram_getarticle(name.buffer()), name.buffer());

    return result;
}

BUF
ITEM::getRawName() const
{
    BUF		buf;

    buf.reference(defn().name);


    if (getDefinition() == ITEM_CORPSE)
    {
	if (MOB::defn(mobType()).isfriendly)
	    buf.reference("body");
    }

    if (isPotion())
    {
	if (isMagicClassKnown())
	{
	    buf.reference(GAMEDEF::potiondef(getMagicClass())->name);
	}
    }
    if (isRing())
    {
	if (isMagicClassKnown())
	{
	    buf.reference(GAMEDEF::ringdef(getMagicClass())->name);
	}
    }

    return buf;
}

BUF
ITEM::getDetailedDescription() const
{
    BUF		result, buf;

    result.reference("");

    if (isWeapon())
    {
#if 0
	buf.sprintf(
		    "Average Damage: %d\n"
		    "Distribution: %s\n"
		    "To Hit Bonus: %d\n"
		    "Element: %s\n",
		    attackdefn().damage,
		    glb_distributiondefs[attackdefn().distribution].name,
		    attackdefn().chancebonus,
		    GAMEDEF::elementdef(attackdefn().element)->name
		    );
	result.strcat(buf);
#else
	buf.sprintf("You can %s with %s%s for %s %d damage.  ",
		attackdefn().verb,
		gram_getarticle(getName()),
		getName().buffer(),
		glb_distributiondefs[attackdefn().distribution].descr,
		LEVEL::boostDamage(attackdefn().damage, getLevel()));
	result.strcat(buf);
	if (attackdefn().chancebonus < 0)
	    result.strcat("It feels very unwiedly for melee.  ");
	else if (attackdefn().chancebonus > 0)
	    result.strcat("It feels excellently balanced for melee.  ");
	if (getDefinition() == ITEM_RANGE_WEAPON)
	{
	    buf.sprintf("You can %s at a range %d with this for %s %d damage. ",
		    rangedefn().verb,
		    getRangeRange(),
		    glb_distributiondefs[rangedefn().distribution].descr,
		    LEVEL::boostDamage(rangedefn().damage, getLevel()));
	    if (rangedefn().chancebonus < 0)
		result.strcat("It feels unlikely to aim true.  ");
	    else if (rangedefn().chancebonus > 0)
		result.strcat("It feels like it will strike true.  ");
	    result.strcat(buf);
	}

	buf.sprintf("\nItem Level: %d   Exp: %d / %d\n",
		getLevel(), getExp(), LEVEL::expFromLevel(getLevel()+1));
	result.strcat(buf);
#endif
    }
    if (isRanged())
    {
    }
    if (isArmour())
    {
	buf.sprintf(
		    "Damage Reduction: %d\n",
		    LEVEL::boostResist(getDamageReduction(), getLevel()));
	result.strcat(buf);
    }

    if (isSpellbook())
    {
	// Find corresponding spell
	SPELL_NAMES spell = (SPELL_NAMES) defn().depth;
	buf.sprintf("Spidery writing provides the secrets of training!  "
	    "Read these runes to gain in experience!");
	result.strcat(buf);
    }

    if (isRing() && isMagicClassKnown())
    {
	RING_NAMES	ring = (RING_NAMES) getMagicClass();

	if (GAMEDEF::ringdef(ring)->resist != ELEMENT_NONE)
	{
	    BUF		capelement;
	    capelement = gram_capitalize(GAMEDEF::elementdef(GAMEDEF::ringdef(ring)->resist)->name);
	    int		ramt = GAMEDEF::ringdef(ring)->resist_amt;
	    ramt = LEVEL::boostResist(ramt, getLevel());
	    if (ramt < 0)
		buf.sprintf("%s Vulnerable: %d\n", capelement.buffer(), -ramt);
	    else
		buf.sprintf("%s Resist: %d\n", capelement.buffer(), ramt);
	    result.strcat(buf);
	}
	if (GAMEDEF::ringdef(ring)->deflect != 0)
	{
	    int		amt = GAMEDEF::ringdef(ring)->deflect;
	    buf.sprintf("Deflection: %d\n", amt);
	    result.strcat(buf);
	}
	buf.sprintf("\nItem Level: %d   Exp: %d / %d\n",
		getLevel(), getExp(), LEVEL::expFromLevel(getLevel()+1));
	result.strcat(buf);
    }
    else if (isRing())
    {
	buf.sprintf("\nItem Level: %d   Exp: %d / %d\n",
		getLevel(), getExp(), LEVEL::expFromLevel(getLevel()+1));
	result.strcat(buf);
    }
    return result;
}

BUF
ITEM::getLongDescription() const
{
    BUF		descr, detail;

    descr = gram_capitalize(getName());
    detail = getDetailedDescription();
    descr.append('\n');
    if (detail.isstring())
    {
	descr.append('\n');
	descr.strcat(detail);
    }
    const char *elementname = GAMEDEF::elementdef(element())->name;
    text_registervariable("ELEMENT", elementname);

    detail = text_lookup("item", getRawName());
    if (detail.isstring() && !detail.startsWith("Missing text entry: "))
    {
	descr.append('\n');
	descr.strcat(detail);
    }
    else
    {
	descr.append('\n');
    }

    // Append the non-magic version
    if (isMagicClassKnown() && (isPotion() || isRing()))
    {
	descr.strcat("Base Type: ");
	detail = gram_capitalize(defn().name);
	detail.append('\n');
	descr.strcat(detail);

	detail = text_lookup("item", defn().name);
	if (detail.isstring() && !detail.startsWith("Missing text entry: "))
	{
	    descr.append('\n');
	    descr.strcat(detail);
	}
	else
	{
	    descr.append('\n');
	}
    }

    return descr;
}

int
ITEM::breakChance(MOB_NAMES victim) const
{
    int		chance = 0;

    // Can't get blood from a stone.
    if (isBroken())
	return 0;

    // Only can break weapons.
    if (defn().itemclass != ITEMCLASS_WEAPON)
	return 0;

    // No breaking in Genshin, this isn't Zelda!
    return 0;
#if 0
    // Now a complicated matrix by type...
    chance = 5;

    if (getDefinition() == ITEM_SLASH_WEAPON)
    {
	if (material() == MATERIAL_GOLD)
	    chance = 15;
	if (material() == MATERIAL_SILVER)
	    chance = 10;
	if (material() == MATERIAL_ALUMINUM)
	    chance = 10;
    }
    else if (getDefinition() == ITEM_PIERCE_WEAPON)
    {
	if (material() == MATERIAL_WOOD)
	    chance = 15;
	if (material() == MATERIAL_GOLD)
	    chance = 10;
    }
    else if (getDefinition() == ITEM_BLUNT_WEAPON)
    {
	if (material() == MATERIAL_WOOD)
	    chance = 10;
	if (material() == MATERIAL_SILVER)
	    chance = 10;
    }

    if (GAMEDEF::mobdef(victim)->corrosiveblood)
    {
	if (GAMEDEF::materialdef(material())->corrodable)
	{
	    chance += 15;
	}
    }

    if (material() == MATERIAL_TITANIUM)
	chance /= 2;

    return chance;
#endif
}

ATTACK_NAMES
ITEM::getRangeAttack() const
{
    if (defn().range_attack == ATTACK_NONE)
	return ATTACK_NONE;

    if (isBroken())
    {
	// Bad melee attack.
	return ATTACK_BROKEN;
    }

    if (getDefinition() == ITEM_RANGE_WEAPON)
    {
	switch (material())
	{
	    case MATERIAL_WOOD:
		return ATTACK_BOW;
	    case MATERIAL_IRON:
	    case MATERIAL_STEEL:
		return ATTACK_BOW_FLEX_METAL;
	    case MATERIAL_GOLD:
	    case MATERIAL_SILVER:
	    case MATERIAL_ALUMINUM:
	    case MATERIAL_TITANIUM:
		return ATTACK_BOW_DUCTILE_METAL;
	    case MATERIAL_STONE:
	    case MATERIAL_NONE:
	    case NUM_MATERIALS:
		return ATTACK_BOW_STONE;
	}
	return ATTACK_EDGE;
    }
    else if (getDefinition() == ITEM_BLUNT_WEAPON)
    {
	switch (material())
	{
	    case MATERIAL_IRON:
	    case MATERIAL_STEEL:
	    case MATERIAL_SILVER:
		return ATTACK_BLUNT_METAL;
	    case MATERIAL_GOLD:
		return ATTACK_BLUNT_HEAVYMETAL;
	    case MATERIAL_ALUMINUM:
		return ATTACK_BLUNT_LIGHTMETAL;
	    case MATERIAL_TITANIUM:
		return ATTACK_BLUNT_TI;
	    case MATERIAL_WOOD:
	    case MATERIAL_STONE:
	    case MATERIAL_NONE:
	    case NUM_MATERIALS:
		return ATTACK_BLUNT;
	}
	return ATTACK_BLUNT;
    }
    else if (getDefinition() == ITEM_PIERCE_WEAPON)
    {
	switch (material())
	{
	    case MATERIAL_IRON:
	    case MATERIAL_STEEL:
	    case MATERIAL_SILVER:
		return ATTACK_POINT_METAL;
	    case MATERIAL_GOLD:
		return ATTACK_POINT_HEAVYMETAL;
	    case MATERIAL_ALUMINUM:
	    case MATERIAL_TITANIUM:
		return ATTACK_POINT_LIGHTMETAL;
	    case MATERIAL_WOOD:
	    case MATERIAL_STONE:
	    case MATERIAL_NONE:
	    case NUM_MATERIALS:
		return ATTACK_POINT;
	}
	return ATTACK_POINT;
    }

    return (ATTACK_NAMES) defn().melee_attack;
}

ATTACK_NAMES
ITEM::getMeleeAttack() const
{
    if (defn().melee_attack == ATTACK_NONE)
	return ATTACK_NONE;

    if (isBroken())
    {
	// Bad melee attack.
	return ATTACK_BROKEN;
    }

    if (getDefinition() == ITEM_SLASH_WEAPON)
    {
	switch (material())
	{
	    case MATERIAL_WOOD:
		return ATTACK_EDGE_WOOD;
	    case MATERIAL_IRON:
	    case MATERIAL_STEEL:
	    case MATERIAL_GOLD:
	    case MATERIAL_SILVER:
		return ATTACK_EDGE_METAL;
	    case MATERIAL_ALUMINUM:
	    case MATERIAL_TITANIUM:
		return ATTACK_EDGE_LIGHTMETAL;
	    case MATERIAL_STONE:
	    case MATERIAL_NONE:
	    case NUM_MATERIALS:
		return ATTACK_EDGE;
	}
	return ATTACK_EDGE;
    }
    else if (getDefinition() == ITEM_BLUNT_WEAPON)
    {
	switch (material())
	{
	    case MATERIAL_IRON:
	    case MATERIAL_STEEL:
	    case MATERIAL_SILVER:
		return ATTACK_BLUNT_METAL;
	    case MATERIAL_GOLD:
		return ATTACK_BLUNT_HEAVYMETAL;
	    case MATERIAL_ALUMINUM:
		return ATTACK_BLUNT_LIGHTMETAL;
	    case MATERIAL_TITANIUM:
		return ATTACK_BLUNT_TI;
	    case MATERIAL_WOOD:
	    case MATERIAL_STONE:
	    case MATERIAL_NONE:
	    case NUM_MATERIALS:
		return ATTACK_BLUNT;
	}
	return ATTACK_BLUNT;
    }
    else if (getDefinition() == ITEM_PIERCE_WEAPON)
    {
	switch (material())
	{
	    case MATERIAL_IRON:
	    case MATERIAL_STEEL:
	    case MATERIAL_SILVER:
		return ATTACK_POINT_METAL;
	    case MATERIAL_GOLD:
		return ATTACK_POINT_HEAVYMETAL;
	    case MATERIAL_ALUMINUM:
	    case MATERIAL_TITANIUM:
		return ATTACK_POINT_LIGHTMETAL;
	    case MATERIAL_WOOD:
	    case MATERIAL_STONE:
	    case MATERIAL_NONE:
	    case NUM_MATERIALS:
		return ATTACK_POINT;
	}
	return ATTACK_POINT;
    }

    return (ATTACK_NAMES) defn().melee_attack;
}

void
ITEM::getLook(u8 &symbol, ATTR_NAMES &attr) const
{
    symbol = defn().symbol;
    attr = (ATTR_NAMES) defn().attr;

    if (material() != MATERIAL_NONE)
    {
	attr = (ATTR_NAMES) GAMEDEF::materialdef(material())->attr;
    }
}

bool
ITEM::canStackWith(const ITEM *stack) const
{
    if (getDefinition() != stack->getDefinition())
	return false;

    if (material() != stack->material())
	return false;
    if (giltMaterial() != stack->giltMaterial())
	return false;
    if (element() != stack->element())
	return false;
    if (getExp() != stack->getExp())
	return false;
    
    if (mobType() != stack->mobType())
	return false;
    if (hero() != stack->hero())
	return false;

    if (isBroken() != stack->isBroken())
	return false;

    // Disable stacking of held/worn items.
    if (isEquipped() || stack->isEquipped())
	return false;

    // Disable stacking of weapons/books/armour as we want
    // to be able to delete them easily
    if (defn().unstackable)
	return false;

    // No reason why not...
    return true;
}

void
ITEM::combineItem(const ITEM *item)
{
    // Untimed items stack.
    if (getTimer() < 0)
	myCount += item->getStackCount();
    // Timed items add up charges.
    if (getTimer() >= 0)
    {
	J_ASSERT(item->getTimer() >= 0);
	myTimer += item->getTimer();
    }

    J_ASSERT(myCount >= 0);
    if (myCount < 0)
	myCount = 0;
}

bool
ITEM::runHeartbeat()
{
    if (myTimer >= 0)
    {
	if (!myTimer)
	    return true;
	myTimer--;
    }
    return false;
}

void
ITEM::initSystem()
{
    glbItemIded.clear();
    glbItemMagicClass.clear();

    {
	// Mix the potions...
	int		potionclasses[NUM_POTIONS];
	POTION_NAMES	potion;

	int pclass = 1;
	for (ITEM_NAMES item = ITEM_NONE; item < GAMEDEF::getNumItem();
	     item = (ITEM_NAMES)(item+1))
	{
	    if (defn(item).itemclass == ITEMCLASS_POTION)
	    {
		if (pclass >= NUM_POTIONS)
		{
		    J_ASSERT(!"Too many mundane potions");
		    break;
		}
		potionclasses[pclass] = item;
		pclass++;
	    }
	}
	rand_shuffle(&potionclasses[1], pclass-1);
	FOREACH_POTION(potion)
	{
	    if (potion == POTION_NONE)
		continue;
	    if (potion >= pclass)
	    {
		J_ASSERT(!"Not enough mundane potions");
		break;
	    }
	    glbItemMagicClass.indexAnywhere(potionclasses[potion]) = potion;
	}
    }
    {
	// Mix the rings...
	int		ringclasses[NUM_RINGS];
	RING_NAMES	ring;

	int pclass = 1;
	for (ITEM_NAMES item = ITEM_NONE; item < GAMEDEF::getNumItem();
	     item = (ITEM_NAMES)(item+1))
	{
	    if (defn(item).itemclass == ITEMCLASS_RING)
	    {
		if (pclass >= NUM_RINGS)
		{
		    J_ASSERT(!"Too many mundane rings");
		    break;
		}
		ringclasses[pclass] = item;
		pclass++;
	    }
	}
	rand_shuffle(&ringclasses[1], pclass-1);
	FOREACH_RING(ring)
	{
	    if (ring == RING_NONE)
		continue;
	    if (ring >= pclass)
	    {
		J_ASSERT(!"Not enough mundane rings");
		break;
	    }
	    glbItemMagicClass.indexAnywhere(ringclasses[ring]) = ring;
	}
    }

}

void
ITEM::saveGlobal(ostream &os)
{
    int		val;

    val = GAMEDEF::getNumItem();
    os.write((const char *)&val, sizeof(int));

    for (ITEM_NAMES item = ITEM_NONE; item < GAMEDEF::getNumItem();
	 item = (ITEM_NAMES)(item+1))
    {
	val = glbItemIded.safeIndexAnywhere(item);
	os.write((const char *) &val, sizeof(int));
	val = glbItemMagicClass.safeIndexAnywhere(item);
	os.write((const char *) &val, sizeof(int));
    }
}

void
ITEM::save(ostream &os) const
{
    int		val;

    val = myDefinition;
    os.write((const char *) &val, sizeof(int));

    myPos.save(os);

    os.write((const char *) &myCount, sizeof(int));
    os.write((const char *) &myTimer, sizeof(int));
    os.write((const char *) &myUID, sizeof(int));
    os.write((const char *) &myNextStackUID, sizeof(int));
    os.write((const char *) &myInterestedMobUID, sizeof(int));
    os.write((const char *) &myBroken, sizeof(bool));
    os.write((const char *) &myEquipped, sizeof(bool));
    val = myMobType;
    os.write((const char *) &val, sizeof(int));
    val = myHero;
    os.write((const char *) &val, sizeof(int));
    val = myElement;
    os.write((const char *) &val, sizeof(int));
    val = myMaterial;
    os.write((const char *) &val, sizeof(int));
    val = myGiltMaterial;
    os.write((const char *) &val, sizeof(int));
    val = mySeed;
    os.write((const char *) &val, sizeof(int));
    val = myLevel;
    os.write((const char *) &val, sizeof(int));
    val = myExp;
    os.write((const char *) &val, sizeof(int));
}

ITEM *
ITEM::load(istream &is)
{
    int		val;
    ITEM	*i;

    i = new ITEM();

    is.read((char *)&val, sizeof(int));
    i->myDefinition = (ITEM_NAMES) val;

    i->myPos.load(is);

    is.read((char *)&i->myCount, sizeof(int));
    is.read((char *)&i->myTimer, sizeof(int));

    is.read((char *)&i->myUID, sizeof(int));
    glb_reportUID(i->myUID);
    is.read((char *)&i->myNextStackUID, sizeof(int));
    is.read((char *)&i->myInterestedMobUID, sizeof(int));
    is.read((char *)&i->myBroken, sizeof(bool));
    is.read((char *)&i->myEquipped, sizeof(bool));
    is.read((char *)&val, sizeof(int));
    i->myMobType = (MOB_NAMES) val;
    is.read((char *)&val, sizeof(int));
    i->myHero = (HERO_NAMES) val;
    is.read((char *)&val, sizeof(int));
    i->myElement = (ELEMENT_NAMES) val;
    is.read((char *)&val, sizeof(int));
    i->myMaterial = (MATERIAL_NAMES) val;
    is.read((char *)&val, sizeof(int));
    i->myGiltMaterial = (MATERIAL_NAMES) val;
    is.read((char *)&val, sizeof(int));
    i->mySeed = val;
    is.read((char *)&val, sizeof(int));
    i->myLevel = val;
    is.read((char *)&val, sizeof(int));
    i->myExp = val;

    return i;
}

void
ITEM::loadGlobal(istream &is)
{
    int		val, total;

    is.read((char *) &total, sizeof(int));

    for (int i = 0; i < total; i++)
    {
	is.read((char *) &val, sizeof(int));
	glbItemIded.indexAnywhere(i) = val ? true : false;
	is.read((char *) &val, sizeof(int));
	glbItemMagicClass.indexAnywhere(i) = val;
    }
}


void
ITEM::gainExp(int exp, bool silent)
{
    myExp += exp;

    int level = LEVEL::levelFromExp(myExp);
    while (level > myLevel)
	gainLevel(silent);
}

void
ITEM::gainLevel(bool silent)
{
    myLevel++;
    BUF		origname;
    BUF msgtext;

    origname = getSingleArticleName();
    msgtext.sprintf("Imbued with more power, %s appears stronger.", origname.buffer());
    if (!silent)
	msg_report(msgtext);
}


bool
ITEM::isPotion() const
{
    if (defn().itemclass == ITEMCLASS_POTION)
	return true;
    return false;
}

bool
ITEM::isSpellbook() const
{
    if (defn().itemclass == ITEMCLASS_SPELLBOOK)
	return true;
    return false;
}

bool
ITEM::isRing() const
{
    if (defn().itemclass == ITEMCLASS_RING)
	return true;
    return false;
}

bool
ITEM::isFood() const
{
    if (defn().itemclass == ITEMCLASS_FOOD)
	return true;
    return false;
}

bool
ITEM::isFlag() const
{
    if (defn().isflag)
	return true;
    return false;
}

bool
ITEM::isWeapon() const
{
    if (defn().itemclass == ITEMCLASS_WEAPON)
	return true;
    return false;
}

bool
ITEM::isArmour() const
{
    if (defn().itemclass == ITEMCLASS_ARMOUR)
	return true;
    return false;
}

bool
ITEM::isRanged() const
{
    if (defn().itemclass == ITEMCLASS_RANGEWEAPON)
	return true;
    return false;
}

void
ITEM::getRangeStats(int &range, int &area) const
{
    range = defn().range_range;
    area = defn().range_area;
}

int
ITEM::getRangeRange() const
{
    // Compute our power, accuracy, and consistency.
    int		range, d;

    getRangeStats(range, d);

    return range;
}

int
ITEM::getRangeArea() const
{
    int		area, d;

    getRangeStats(d, area);

    return area;
}

void
ITEM::setupTextVariables() const
{
    BUF		tmp;

    tmp.strcpy(gram_getarticle(getName()));
    tmp.strcat(getName());
    text_registervariable("A_ITEMNAME", tmp);
    text_registervariable("ITEMNAME", getName());
    text_registervariable("ITEMNAMES", gram_makepossessive(getName()));
    tmp.sprintf("%d", defn().depth);
    text_registervariable("ITEMDEPTH", tmp);
    text_registervariable("ITEMCLASS", GAMEDEF::itemclassdef(defn().itemclass)->name);
}

EFFECT_NAMES
ITEM::corpseEffect() const
{
    if (getDefinition() != ITEM_CORPSE)
	return EFFECT_NONE;

    if (mobType() == MOB_NONE)
	return EFFECT_NONE;

    return (EFFECT_NAMES) MOB::defn(mobType()).corpse_effect;
}

EFFECT_NAMES
ITEM::potionEffect() const
{
    return potionEffect(getDefinition());
}

EFFECT_NAMES
ITEM::potionEffect(ITEM_NAMES item)
{
    if (defn(item).itemclass != ITEMCLASS_POTION)
	return EFFECT_NONE;
    POTION_NAMES potion = (POTION_NAMES) glbItemMagicClass.safeIndexAnywhere(item);

    return (EFFECT_NAMES) GAMEDEF::potiondef(glbItemMagicClass.safeIndexAnywhere(item))->effect;
}
