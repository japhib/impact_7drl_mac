/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	Everything Is Fodder Development
 *
 * NAME:        thesaurus.cpp ( Everything Is Fodder, C++ )
 *
 * COMMENTS:
 */

#include "thesaurus.h"
#include "rand.h"
#include "gamedef.h"
#include "grammar.h"
#include "mob.h"
#include <assert.h>

void
thesaurus_init()
{
}

BUF
thesaurus_expand(int seed, MOB_NAMES mob)
{
    RANDSEQ		seq(seed);
    return thesaurus_expand(seq, mob);
}

BUF
thesaurus_expand(RANDSEQ &seq, MOB_NAMES mob)
{
    BUF		name, noun;

    noun.strcpy(GAMEDEF::mobdef(mob)->name);

    return noun;

    static const char *mobadj[] =
    {
	"speckled",
	"spotted",
	"slimy",
	"feral",
	"vicious",
	"diseased",
	"mangy",
	"twisted",
	"sly",
	"cunning",
	"cruel",
	"striped",
	"piebald",
	"smelly",
	"foul-breathed",
	"spiky",
	0
    };

    BUF		adj;
    adj.reference(seq.string(mobadj));

    if (seq.chance_range(MOB::transmuteTaint(), 0, 1))
    {
	adj.clear();
    }

    if (adj.isstring())
	name.sprintf("%s %s", seq.string(mobadj), noun.buffer());
    else
	name = noun;
    return name;
}

BUF
thesaurus_expand(int seed, MATERIAL_NAMES material)
{
    RANDSEQ		seq(seed);
    return thesaurus_expand(seq, material);
}

BUF
thesaurus_expand(RANDSEQ &seq, MATERIAL_NAMES material)
{
    BUF		mat, basename;

    mat.reference(GAMEDEF::materialdef(material)->name);
    basename = mat;

    switch (material)
    {
	case MATERIAL_WOOD:
	{
	    static const char *matname[] =
	    {
		"oak",
		"pine",
		"birch",
		"acacia",
		"beech",
		"maple",
		"ash",
		0
	    };
	    mat.reference(seq.string(matname));
	    break;
	}
	case MATERIAL_STONE:
	{
	    static const char *matname[] =
	    {
		"granite",
		"dolomite",
		"obsidian",
		"pumice",
		"sandstone",
		"flint",
		"gneiss",
		0
	    };
	    mat.reference(seq.string(matname));
	    break;
	}
	case MATERIAL_IRON:
	{
	    static const char *matname[] =
	    {
		"wrought-iron",
		"pig-iron",
		"cast-iron",
		"meteoric-iron",
		"iron",
		0
	    };
	    mat.reference(seq.string(matname));
	    break;
	}
	case MATERIAL_STEEL:
	{
	    static const char *matname[] =
	    {
		"steel",
		"steel",
		"steel",
		"Damascus-steel",
		"Wootz-steel",
		0
	    };
	    mat.reference(seq.string(matname));
	    break;
	}
	case MATERIAL_SILVER:
	{
	    static const char *matname[] =
	    {
		"silver",
		"pure silver",
		"solid silver",
		"alloyed silver",
		"tarnished silver",
		0
	    };
	    mat.reference(seq.string(matname));
	    break;
	}
	case MATERIAL_GOLD:
	{
	    static const char *matname[] =
	    {
		"gold",
		"pure gold",
		"solid gold",
		"golden",
		"red-gold",
		"white-gold",
		0
	    };
	    mat.reference(seq.string(matname));
	    break;
	}
	case MATERIAL_ALUMINUM:
	{
	    static const char *matname[] =
	    {
		"aluminum",
		"aluminium", // Did I just troll you?
		0
	    };
	    mat.reference(seq.string(matname));
	    break;
	}
	case MATERIAL_TITANIUM:
	{
	    static const char *matname[] =
	    {
		"titanium",
		"mithril",
		0
	    };
	    mat.reference(seq.string(matname));
	    break;
	}
    }

    if (seq.chance_range(MOB::transmuteTaint(), 0.25, 1))
    {
	mat = basename;
    }

    return mat;
}

BUF
thesaurus_expand(int seed, ITEM_NAMES item, MATERIAL_NAMES material, MATERIAL_NAMES gilt)
{
    RANDSEQ		seq(seed);

    BUF		noun, name, qualia, gilttext, basename, basicname;
    noun.reference(GAMEDEF::itemdef(item)->name);
    basename = noun;
    basicname.reference("source of");
    bool	reversenoun = false;
    switch (item)
    {
	case ITEM_SLASH_WEAPON:
	{
	    basicname.reference("edged");
	    static const char *weapname[] =
	    {
		"longsword",
		"short sword",
		"great sword",
		"bastard sword",
		"battle axe",
		0
	    };
	    noun.reference(seq.string(weapname));
	    static const char *qualianame[] =
	    {
		"sharp",
		"dull",
		"scratched",
		"balanced",
		"serrated",
		"polished",
		0
	    };
	    qualia.reference(seq.string(qualianame));
	    break;
	}

	case ITEM_PIERCE_WEAPON:
	{
	    basicname.reference("pointed");
	    static const char *weapname[] =
	    {
		"spear",
		"dagger",
		"poinard",
		"dirk",
		"lance",
		0
	    };
	    noun.reference(seq.string(weapname));
	    static const char *qualianame[] =
	    {
		"long",
		"flexible",
		"light",
		"top heavy",
		"razor-pointed",
		0
	    };
	    qualia.reference(seq.string(qualianame));
	    break;
	}
	case ITEM_BLUNT_WEAPON:
	{
	    basicname.reference("blunt");
	    static const char *weapname[] =
	    {
		"club",
		"mace",
		"morning star",
		"flail",
		"warhammer",
		0
	    };
	    noun.reference(seq.string(weapname));
	    static const char *qualianame[] =
	    {
		"heavy",
		"unstable",
		"light",
		"balanced",
		"weighted",
		0
	    };
	    qualia.reference(seq.string(qualianame));
	    break;
	}
	case ITEM_CHAIR:
	{
	    static const char *furnname[] =
	    {
		"stool",
		"throne",
		"bench",
		"ottoman",
		"chair",
		0
	    };
	    noun.reference(seq.string(furnname));
	    static const char *qualianame[] =
	    {
		"tall",
		"short",
		"wobbly",
		"stable",
		"stained",
		"broken",
		"high-backed",
		0
	    };
	    qualia.reference(seq.string(qualianame));
	    break;
	}
	case ITEM_TABLE:
	{
	    static const char *furnname[] =
	    {
		"folding desk",
		"writing desk",
		"work table",
		"short table",
		"dining table",
		0
	    };
	    noun.reference(seq.string(furnname));
	    static const char *qualianame[] =
	    {
		"tall",
		"short",
		"wobbly",
		"stable",
		"stained",
		"scratched",
		"worn",
		"oval",
		"round",
		"rectangular",
		"long",
		0
	    };
	    qualia.reference(seq.string(qualianame));
	    break;
	}
	case ITEM_SHELF:
	{
	    static const char *furnname[] =
	    {
		"bookshelf",
		"armoire",
		"cabinet",
		"wardrobe",
		"closet",
		"display case",
		0
	    };
	    noun.reference(seq.string(furnname));
	    static const char *qualianame[] =
	    {
		"tall",
		"short",
		"stout",
		"cluttered",
		"empty",
		"stackable",
		0
	    };
	    qualia.reference(seq.string(qualianame));
	    break;
	}
	case ITEM_BED:
	{
	    static const char *furnname[] =
	    {
		"cot",
		"bed",
		"trundle",
		"divan",
		"bunk",
		0
	    };
	    noun.reference(seq.string(furnname));
	    static const char *qualianame[] =
	    {
		"tall",
		"short",
		"wide",
		"moldy",
		"stained",
		"dirty",
		"smelly",
		"clean",
		0
	    };
	    qualia.reference(seq.string(qualianame));
	    break;
	}
	case ITEM_STATUE:
	{
	    static const char *furnname[] =
	    {
		"statue",
		"bust",
		"icon",
		"figure",
		"effigy",
		"sculpture",
		0
	    };
	    noun.reference(seq.string(furnname));
	    static const char *qualianame[] =
	    {
		"stern",
		"beautiful",
		"evil-eyed",
		"broken",
		"painted",
		"chipped",
		"enigmatic",
		"well-crafted",
		0
	    };
	    qualia.reference(seq.string(qualianame));
	    break;
	}
	default:
	{
	    J_ASSERT(!"Unhandled item for thesaurus!");
	    break;
	}
    }

    if (seq.chance_range(MOB::transmuteTaint(), 0.5f, 1))
    {
	// Revert to base name.
	noun = basename;
    }
    if (seq.chance_range(MOB::transmuteTaint(), 1.0f, 1.2f))
    {
	noun = basicname;
	reversenoun = true;
    }

    if (gilt != MATERIAL_NONE)
    {
	static const char *giltname[] =
	{
	    "%s-chased",
	    "%s-accented",
	    "%s-detailed",
	    "%s-plated",
	    "%s-filigreed",
	    0
	};

	BUF		giltmat = thesaurus_expand(seq, gilt);

	gilttext.sprintf(seq.string(giltname), giltmat.buffer());

	if (seq.chance_range(MOB::transmuteTaint(), 0.5f, 1))
	{
	    gilttext.sprintf("%s-gilt", giltmat.buffer());
	}
	if (seq.chance_range(MOB::transmuteTaint(), 1.0f, 1.2f))
	{
	    gilttext = giltmat;
	}
    }

    BUF		peerage;
    static const char *peername[] =
    {
	"ancient",
	"antique",
	"new",
	"brand-new",
	"modern",
	"antiquated",
	"victorian",
	"edwardian",
	"gothic",
	"baroque",
	"brutalist",
	"plain",
	"bejewelled",
	"etched",
	"engraved",
	0
    };
    peerage.reference(seq.string(peername));

    // Not everything has all properties.
    if (seq.chance(70))
	peerage.clear();
    if (seq.chance_range(MOB::transmuteTaint(), 0.25f, 1))
	peerage.clear();
    if (seq.chance(50))
	qualia.clear();
    if (seq.chance_range(MOB::transmuteTaint(), 0.1f, 1))
	qualia.clear();

    // description:
    // PEERAGE GILT QUALIA MATERIAL NOUN
    // ancient silver-chased shiny silver long sword
    // brand-new gold accented oval wooden table

    if (peerage.isstring())
    {
	name.strcat(peerage);
	name.strcat(" ");
    }
    if (gilttext.isstring())
    {
	name.strcat(gilttext);
	name.strcat(" ");
    }
    if (qualia.isstring())
    {
	name.strcat(qualia);
	name.strcat(" ");
    }
    if (reversenoun)
    {
	name.strcat(noun);
	name.strcat(" ");
	name.strcat(thesaurus_expand(seq, material));
    }
    else
    {
	name.strcat(thesaurus_expand(seq, material));
	name.strcat(" ");
	name.strcat(noun);
    }

    return name;
}

BUF
thesaurus_expand(int seed, ROOMTYPE_NAMES room)
{
    RANDSEQ	seq(seed);

    return thesaurus_expand(seq, room);
}

BUF
thesaurus_expand(RANDSEQ &seq, ROOMTYPE_NAMES room)
{
    BUF		result, basename, basicname;

    basename.reference(glb_roomtypedefs[room].name);
    basicname = basename;

    switch (room)
    {
	case ROOMTYPE_CORRIDOR:
	{
	    static const char *roomname[] =
	    {
		"corridor",
		"passage",
		"passageway",
		"tunnel",
		"hallway",
		0
	    };
	    basename.reference(seq.string(roomname));
	    break;
	}
	case ROOMTYPE_BARRACKS:
	{
	    static const char *roomname[] =
	    {
		"barracks",
		"sleeping quarters",
		"bedroom",
		"bedchamber",
		0
	    };
	    basename.reference(seq.string(roomname));
	    break;
	}
	case ROOMTYPE_GUARDHOUSE:
	{
	    static const char *roomname[] =
	    {
		"guardhouse",
		"guardroom",
		"guard post",
		"sentry post",
		"checkpoint",
		0
	    };
	    basename.reference(seq.string(roomname));
	    break;
	}
	case ROOMTYPE_THRONEROOM:
	{
	    static const char *roomname[] =
	    {
		"audience chamber",
		"throne room",
		"throne hall",
		0
	    };
	    basename.reference(seq.string(roomname));
	    break;
	}
	case ROOMTYPE_VAULT:
	{
	    static const char *roomname[] =
	    {
		"treasure room",
		"vault",
		"treasure vault",
		"treasure hall",
		0
	    };
	    basename.reference(seq.string(roomname));
	    break;
	}
	case ROOMTYPE_LIBRARY:
	{
	    static const char *roomname[] =
	    {
		"library",
		"archive",
		"office",
		0
	    };
	    basename.reference(seq.string(roomname));
	    break;
	}
	case ROOMTYPE_KITCHEN:
	{
	    static const char *roomname[] =
	    {
		"kitchen",
		"scullery",
		"cookery",
		"mess",
		"kitchenette",
		"canteen",
		0
	    };
	    basename.reference(seq.string(roomname));
	    break;
	}
	case ROOMTYPE_DININGROOM:
	{
	    static const char *roomname[] =
	    {
		"dining room",
		"feasting hall",
		"banquet hall",
		"dinette",
		"breakfast nook",
		0
	    };
	    basename.reference(seq.string(roomname));
	    break;
	}
	case ROOMTYPE_PANTRY:
	{
	    static const char *roomname[] =
	    {
		"pantry",
		"larder",
		"storage room",
		0
	    };
	    basename.reference(seq.string(roomname));
	    break;
	}
    }

    static const char *statenames[] = 
    {
	"clean",
	"dirty",
	"dusty",
	"musty",
	"spotless",
	"blood-stained",
	"stained",
	"heavily-trafficked",
	"unused",
	"cobweb-infested",
	"neat",
	"disorganized",
	"dark",
	"bright",
	"well-lit",
	"dimly-lit",
	"murky",
	"shadowy",
	"misty",
	"draught-filled",
	"cluttered",
	"airy",
	"confined",
	0
    };

    BUF	state;
    state.reference(seq.string(statenames));

    static const char *qualianames[] = 
    {
	"opulent",
	"paneled",
	"golden",
	"marble",
	"rough-hewn",
	"tapestried",
	"carpeted",
	"tiled",
	"grand",
	"brick-lined",
	"dirt-floored",
	"parqueted",
	"painted",
	"velour",
	0
    };
    BUF	qualia;
    qualia.reference(seq.string(qualianames));

    if (seq.chance(15))
	qualia.clear();
    if (seq.chance_range(MOB::transmuteTaint(), 0.1F, 1))
	qualia.clear();
    if (seq.chance(15))
	state.clear();
    if (seq.chance_range(MOB::transmuteTaint(), 0.3F, 1))
	state.clear();

    if (seq.chance_range(MOB::transmuteTaint(), 0.5F, 1))
	basename = basicname;

    if (state.isstring())
    {
	result.strcat(state);
	result.strcat(" ");
    }
    if (qualia.isstring())
    {
	result.strcat(qualia);
	result.strcat(" ");
    }
    result.strcat(basename);

    if (seq.chance_range(MOB::transmuteTaint(), 1.0, 1.2f))
    {
	result.reference("room");
    }

    BUF	article;
    article = gram_createcount(result, 1, true);
    return article;
}
