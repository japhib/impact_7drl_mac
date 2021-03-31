/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	Jacob's Matrix Development
 *
 * NAME:        main.cpp ( Jacob's Matrix, C++ )
 *
 * COMMENTS:
 */

#include <libtcod.hpp>
#undef CLAMP
#include <stdio.h>
#include <time.h>

//#define USE_AUDIO

#define POPUP_DELAY 1000

#ifdef WIN32
#include <windows.h>
#include <process.h>
#define usleep(x) Sleep((x)*1000)
// #pragma comment(lib, "SDL.lib")
// #pragma comment(lib, "SDLmain.lib")

// I really hate windows.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#else
#include <unistd.h>
#endif

#include <fstream>
using namespace std;

int		 glbLevelStartMS = -1;

#include "rand.h"
#include "gfxengine.h"
#include "config.h"
#include "builder.h"
#include "dircontrol.h"
#include "thesaurus.h"

//#include <SDL.h>
#ifdef USE_AUDIO
#include <SDL_mixer.h>

Mix_Music		*glbTunes = 0;
#endif
bool			 glbMusicActive = false;

// Cannot be bool!
// libtcod does not accept bools properly.

volatile int		glbItemCache = -1;

SPELL_NAMES		glbLastSpell = SPELL_NONE;
int			glbLastItemIdx = 0;
int			glbLastItemAction = 0;
const char		*glbWorldName = "impact";
bool			glbReloadedLevel = true;

BUF			 glbPlayerName;
BUF			 glbStatLineFormat;
bool			 glbStatLineSwap;

void endOfMusic()
{
    // Keep playing.
    // glbMusicActive = false;
}

void
setMusicVolume(int volume)
{
#ifdef USE_AUDIO
    volume = BOUND(volume, 0, 10);

    glbConfig->myMusicVolume = volume;

    Mix_VolumeMusic((MIX_MAX_VOLUME * volume) / 10);
#endif
}

void
stopMusic(bool atoncedamnit = false)
{
#ifdef USE_AUDIO
    if (glbMusicActive)
    {
	if (atoncedamnit)
	    Mix_HaltMusic();
	else
	    Mix_FadeOutMusic(2000);
	glbMusicActive = false;
    }
#endif
}


void
startMusic()
{
#ifdef USE_AUDIO
    if (glbMusicActive)
	stopMusic();

    if (!glbTunes)
	glbTunes = Mix_LoadMUS(glbConfig->musicFile());
    if (!glbTunes)
    {
	printf("Failed to load %s, error %s\n", 
		glbConfig->musicFile(), 
		Mix_GetError());
    }
    else
    {
	glbMusicActive = true;
	glbLevelStartMS = TCOD_sys_elapsed_milli();

	if (glbConfig->musicEnable())
	{
	    Mix_PlayMusic(glbTunes, -1);		// Infinite
	    Mix_HookMusicFinished(endOfMusic);

	    setMusicVolume(glbConfig->myMusicVolume);
	}
    }
#endif
}


#ifdef main
#undef main
#endif

#include "mob.h"
#include "map.h"
#include "text.h"
#include "display.h"
#include "engine.h"
#include "panel.h"
#include "msg.h"
#include "firefly.h"
#include "item.h"
#include "chooser.h"

PANEL		*glbMessagePanel = 0;
DISPLAY		*glbDisplay = 0;
MAP		*glbMap = 0;
ENGINE		*glbEngine = 0;
PANEL		*glbPopUp = 0;
PANEL		*glbJacob = 0;
PANEL		*glbStatLine = 0;
PANEL		*glbHelp = 0;
PANEL		*glbVictoryInfo = 0;
bool		 glbPopUpActive = false;
bool		 glbRoleActive = false;
FIREFLY		*glbCoinStack = 0;
int		 glbLevel = 0;
int		 glbMaxLevel = 0;
PTRLIST<int>	 glbLevelTimes;
bool		 glbVeryFirstRun = true;

CHOOSER		*glbLevelBuildStatus = 0;

CHOOSER		*glbChooser = 0;
bool		 glbChooserActive = false;

int		 glbInvertX = -1, glbInvertY = -1;
int		 glbMeditateX = -1, glbMeditateY = -1;
int		 glbWaypointX = -1, glbWaypointY = -1;
IVEC2		 glbUnitWaypoint[10];
bool		 glbHasDied = false;

#define DEATHTIME 15000

ATTR_NAMES
colourFromRatio(int cur, int max)
{
    if (cur >= max)
	return ATTR_LTGREY;
    if (cur < 5 || (cur < max / 6))
	return ATTR_RED;
    if (cur < max / 2)
	return ATTR_LTYELLOW;
    return ATTR_LTGREEN;
}


BUF
mstotime(int ms)
{
    int		sec, min;
    BUF		buf;

    sec = ms / 1000;
    ms -= sec * 1000;
    min = sec / 60;
    sec -= min * 60;

    if (min)
	buf.sprintf("%dm%d.%03ds", min, sec, ms);
    else if (sec)
	buf.sprintf("%d.%03ds", sec, ms);
    else
	buf.sprintf("0.%03ds", ms);

    return buf;
}

class MOBPTR_SORT
{
public:
    bool operator()(MOB *a, MOB *b)
    {
	return a->getUID() < b->getUID();
    }
};

class ITEMPTR_SORT
{
public:
    bool operator()(ITEM *a, ITEM *b)
    {
	return a->getUID() < b->getUID();
    }
};

class ITEMPTR_QUALITYSORT
{
public:
    bool operator()(ITEM *a, ITEM *b)
    {
	if (a->isBroken() && !b->isBroken())
	    return true;
	if (!a->isBroken() && b->isBroken())
	    return false;
	if (a->isWeapon())
	{
	    if (b->isArmour())
		return true;
	    if (b->isWeapon())
		return (a->attackdefn().damage < b->attackdefn().damage);
	    return true;
	}
	if (a->isArmour())
	{
	    if (b->isWeapon())
		return false;
	    if (b->isArmour())
		return (a->getDamageReduction() < b->getDamageReduction());
	    return true;
	}
	return a->getUID() < b->getUID();
    }
};


KEY_NAMES
cookKey(u8 rawkey)
{
    KEY_NAMES	key;
    FOREACH_KEY(key)
    {
	if (GAMEDEF::keydef(key)->val == rawkey)
	    return key;
    }

    return NUM_KEYS;
}

void
swapKeys(int keya, int keyb)
{
    KEY_NAMES	a, b;
    a = cookKey(keya);
    b = cookKey(keyb);
    if (a == NUM_KEYS || b == NUM_KEYS)
    {
	J_ASSERT(!"Invalid key given!");
	return;
    }
    GAMEDEF::keydef(a)->val = keyb;
    GAMEDEF::keydef(b)->val = keya;
}

bool
cookDirKey(int &key, int &dx, int &dy, bool allowshift)
{
    // Do not try to cook vi keys as we want to handl them ourselves!
    if (!strchr("yuhjklbn", key))
    {
	// Comment this out to avoid giving free keys for numerics & arrows.
	if (gfx_cookDir(key, dx, dy, GFX_COOKDIR_ALL, allowshift, 5))
	    return true;
    }
    KEY_NAMES	cookkey = cookKey(key);
    switch (cookkey)
    {
	case KEY_MOVE_NW:
	    dx = -1;	dy = -1;
	    key = 0;
	    return true;
	case KEY_MOVE_N:
	    dx =  0;	dy = -1;
	    key = 0;
	    return true;
	case KEY_MOVE_NE:
	    dx = +1;	dy = -1;
	    key = 0;
	    return true;
	case KEY_MOVE_W:
	    dx = -1;	dy =  0;
	    key = 0;
	    return true;
	case KEY_MOVE_STAY:
	    dx =  0;	dy =  0;
	    key = 0;
	    return true;
	case KEY_MOVE_E:
	    dx = +1;	dy =  0;
	    key = 0;
	    return true;
	case KEY_MOVE_SW:
	    dx = -1;	dy = +1;
	    key = 0;
	    return true;
	case KEY_MOVE_S:
	    dx =  0;	dy = +1;
	    key = 0;
	    return true;
	case KEY_MOVE_SE:
	    dx = +1;	dy = +1;
	    key = 0;
	    return true;
	default:
	    // Ignore other ones.
	    break;
    }
    // Don't eat.
    return false;
}

void
updateHelp(PANEL *panel)
{
    // By doing this here we can be a bit fancier.
    panel->clear();

    panel->appendText("[?] Keyboard Help\n");
    panel->appendText("\n");
}

// Should call this in all of our loops.
void
redrawWorldNoFlush()
{
    static MAP	*centermap = 0;
    static POS	 mapcenter;
    bool	 isblind = false;
    bool	 isvictory = false;

    gfx_updatepulsetime();

    // Wipe the world
    for (int y = 0; y < SCR_HEIGHT; y++)
	for (int x = 0; x < SCR_WIDTH; x++)
	    gfx_printchar(x, y, ' ', ATTR_NORMAL);

    if (glbMap)
	glbMap->decRef();
    glbMap = glbEngine ? glbEngine->copyMap() : 0;

    glbStatLine->clear();
    glbHelp->clear();

    updateHelp(glbHelp);

    glbMeditateX = -1;
    glbMeditateY = -1;
    int			avataralive = 0;
    if (glbMap && glbMap->avatar())
    {
	MOB		*avatar = glbMap->avatar();
	int		 timems;

	isvictory = false;

	avataralive = avatar->alive();

	if (avatar->alive() && avatar->isMeditating() && avatar->isMeditateDecoupled())
	{
	    glbMeditateX = glbDisplay->x() + glbDisplay->width()/2;
	    glbMeditateY = glbDisplay->y() + glbDisplay->height()/2;
	}

	timems = TCOD_sys_elapsed_milli();

	if (centermap)
	    centermap->decRef();
	centermap = glbMap;
	centermap->incRef();
	mapcenter = avatar->meditatePos();

	isblind = avatar->hasItem(ITEM_BLIND);

	if (glbCoinStack)
	{
	    int		ngold = avatar->getHP() / 2;

	    glbCoinStack->setParticleCount(BOUND(ngold, 0, 22 * SCR_HEIGHT));
	    glbCoinStack->setBloodCoins(0);
	    glbCoinStack->setRatioLiving((float) (BOUND(ngold, 0, 20*(SCR_HEIGHT-2)) / float(20*(SCR_HEIGHT-2))));
	}

	BUF		buf;

	{
	    MOBLIST	atlist;

	    glbStatLine->setTextAttr(ATTR_WHITE);
	    glbStatLine->clear();
	}
    }
    
    glbDisplay->display(mapcenter, isblind);

    glbMessagePanel->redraw();

    {
	glbJacob->clear();

	if (glbMap)
	{
	}
	

	if (glbMap && glbMap->avatar())
	{
	    MOB	*avatar = glbMap->avatar();
	    BUF		buf;
	    const char	*slotname = "ASDF";

	    glbJacob->setTextAttr(ATTR_NORMAL);
	    for (int i = 0; i < 20; i++)
		glbJacob->appendText(" ");
	    if (avatar->pos().depth() > 0)
		buf.sprintf("Depth: %d\n", avatar->pos().depth());
	    else
		buf.sprintf("\n");
	    glbJacob->appendText(buf);

	    glbJacob->setTextAttr(ATTR_NORMAL);
	    for (int i = 0; i < MAX_PARTY; i++)
	    {
		MOB *mob = glbMap->party().member(i);
		if (mob)
		{
		    bool active = glbMap->party().active() == mob;
		    if (active)
			glbJacob->setTextAttr(ATTR_YELLOW);
		    else
			glbJacob->setTextAttr(ATTR_WHITE);
		    buf.sprintf(" %c%c%c ", 
				active ? '*' : ' ', 
				slotname[i],
				active ? '*' : ':');
		    glbJacob->appendText(buf);

		    BUF name = gram_capitalize(mob->hero().name);
		    buf.sprintf("%s", name.buffer());
		    glbJacob->appendText(buf);

		    int elen = strlen(mob->hero().name);
		    while (elen++ < 27)
			glbJacob->appendText(" ");

		    glbJacob->setTextAttr(ATTR_NORMAL);
		    buf.sprintf("HP: ");
		    glbJacob->appendText(buf);
		    
		    // Set the colour from the ratio.
		    glbJacob->setTextAttr(colourFromRatio(mob->getHP(), mob->getMaxHP()));
		    buf.sprintf("%3d/%3d", mob->getHP(), mob->getMaxHP());
		    glbJacob->appendText(buf);
		    glbJacob->setTextAttr(ATTR_NORMAL);
		    if (mob->getExtraLives())
		    {
			glbJacob->setTextAttr(ATTR_CYAN);
			glbJacob->appendText("  ");
			for (int i = 0; i < mob->getExtraLives(); i++)
			{
			    glbJacob->appendText("*");
			}
		    }
		    glbJacob->appendText("\n");
		    glbJacob->setTextAttr(ATTR_NORMAL);
		    buf.sprintf("     Level: %d", mob->getLevel());
		    glbJacob->appendText(buf);

		    {
			ELEMENT_NAMES 	elem;
			bool		anyvuln = false;
			FOREACH_ELEMENT(elem)
			{
			    if (elem == ELEMENT_NONE)
				continue;
			    if (mob->isVulnerable(elem))
			    {
				if (!anyvuln)
				{
				    glbJacob->setTextAttr(ATTR_NORMAL);
				    glbJacob->appendText(" Vuln: ");
				    anyvuln = true;
				}
				glbJacob->setTextAttr((ATTR_NAMES) GAMEDEF::elementdef(elem)->attr);
				buf.sprintf("%c", GAMEDEF::elementdef(elem)->symbol);
				glbJacob->appendText(buf);
			    }
			}
		    }

		    if (mob->lookupItem(ITEM_POISON))
		    {
			glbJacob->setTextAttr(ATTR_DKGREEN);
			glbJacob->appendText(" Poison");
		    }
		    if (mob->lookupItem(ITEM_REGEN))
		    {
			glbJacob->setTextAttr(ATTR_YELLOW);
			glbJacob->appendText(" Regen");
		    }
		    if (mob->lookupItem(ITEM_QUICKBOOST))
		    {
			glbJacob->setTextAttr(ATTR_NORMAL);
			glbJacob->appendText(" Sprint");
		    }
		    if (mob->lookupItem(ITEM_INVULNERABLE))
		    {
			glbJacob->setTextAttr(ATTR_WHITE);
			glbJacob->appendText(" Invuln");
		    }
		    if (mob->lookupItem(ITEM_SLOW))
		    {
			glbJacob->setTextAttr(ATTR_DKGREY);
			glbJacob->appendText(" Slow");
		    }
		    if (mob->lookupItem(ITEM_HASTED))
		    {
			glbJacob->setTextAttr(ATTR_LTGREY);
			glbJacob->appendText(" Fast");
		    }
		    if (mob->lookupItem(ITEM_PLAGUE))
		    {
			glbJacob->setTextAttr(ATTR_WHITE);
			glbJacob->appendText(" Sick");
		    }
		    if (mob->getFood() >= mob->getMaxFood())
		    {
			glbJacob->setTextAttr(ATTR_NORMAL);
			glbJacob->appendText(" Full");
		    }


		    buf.sprintf("\n");
		    glbJacob->appendText(buf);

		    elen = 0;
		    if (mob->getSpellTimeout())
		    {
			glbJacob->setTextAttr(ATTR_LIGHTBLACK);
			buf.sprintf("     [e] ");
			glbJacob->appendText(buf);
			glbJacob->setTextAttr(ATTR_NORMAL);
			buf.sprintf("%s: In %d", GAMEDEF::spelldef(mob->hero().skill)->name, mob->getSpellTimeout());
			elen = buf.strlen();
			glbJacob->appendText(buf);
		    }
		    else
		    {
			glbJacob->setTextAttr((ATTR_NAMES) GAMEDEF::elementdef(mob->hero().element)->attr);
			buf.sprintf("     [e] ");
			glbJacob->appendText(buf);
			glbJacob->setTextAttr(ATTR_NORMAL);
			buf.sprintf("%s: Ready", GAMEDEF::spelldef(mob->hero().skill)->name);
			elen = buf.strlen();
			glbJacob->appendText(buf);
		    }

		    while (elen++ < 23)
			glbJacob->appendText(" ");

		    if (mob->getMP() < mob->getMaxMP())
		    {
			glbJacob->setTextAttr(ATTR_LIGHTBLACK);
			buf.sprintf("[q] ");
			glbJacob->appendText(buf);

			glbJacob->setTextAttr(ATTR_NORMAL);
			buf.sprintf("%s: %d / %d\n", GAMEDEF::spelldef(mob->hero().burst)->name, mob->getMP(), mob->getMaxMP());
			glbJacob->appendText(buf);
		    }
		    else
		    {
			glbJacob->setTextAttr((ATTR_NAMES) GAMEDEF::elementdef(mob->hero().element)->attr);
			buf.sprintf("[q] ");
			glbJacob->appendText(buf);

			glbJacob->setTextAttr(ATTR_NORMAL);
			buf.sprintf("%s: Ready\n", GAMEDEF::spelldef(mob->hero().burst)->name);
			glbJacob->appendText(buf);
		    }

		    glbJacob->setTextAttr(ATTR_NORMAL);
		}
		else
		{
		    buf.sprintf("  %c: <empty>\n\n\n", slotname[i]);
		    glbJacob->appendText(buf);
		}
		glbJacob->appendText("\n");
	    }
	}
    }

    glbJacob->redraw();

    glbHelp->redraw();

    glbStatLine->clear();
    if (glbMap && glbMap->avatar())
    {
	MOB	*avatar = glbMap->avatar();
	BUF	 stats;

	glbStatLine->appendText(stats);
    }

    glbStatLine->redraw();

    if (isvictory)
	glbVictoryInfo->redraw();

    {
	// we have to clear the RHS.
	static FIRETEX		 *cleartex = 0;
	if (!cleartex)
	{
	    cleartex = new FIRETEX(30, (SCR_HEIGHT-2));
	    cleartex->buildFromConstant(false, 0, FIRE_HEALTH);
	}
	cleartex->redraw(SCR_WIDTH-30, 2);
    }
    if (glbCoinStack)
    {
	FIRETEX		*tex = glbCoinStack->getTex();
	tex->redraw(0, 0);
	tex->decRef();
    }

#if 0
    if (glbMeditateX >= 0 && glbMeditateY >= 0)
    {
	gfx_printattrback(glbMeditateX, glbMeditateY, ATTR_AVATARMEDITATE);
    }
#endif

    if (glbInvertX >= 0 && glbInvertY >= 0)
    {
	gfx_printattr(glbInvertX, glbInvertY, ATTR_HILITE);
    }

    if (glbWaypointX >= 0 && glbWaypointY >= 0)
    {
	gfx_printattrback(glbWaypointX, glbWaypointY, ATTR_LTWAYPOINT);
    }

    if (glbItemCache >= 0)
    {
	glbLevelBuildStatus->clear();

	glbLevelBuildStatus->appendChoice("   Building Distance Caches   ", glbItemCache);
	glbLevelBuildStatus->redraw();
    }

    if (glbChooserActive)
    {
	glbChooser->redraw();
    }

    if (glbPopUpActive)
	glbPopUp->redraw();
}

void
redrawWorld()
{
    redrawWorldNoFlush();
    TCODConsole::flush();
}

// Pops up the given text onto the screen.
// The key used to dismiss the text is the return code.
int
popupText(const char *buf, int delayms = 0)
{
    int		key = 0;
    int		startms = TCOD_sys_elapsed_milli();

    glbPopUp->clear();
    glbPopUp->resize(50, SCR_HEIGHT-2);
    glbPopUp->move(SCR_WIDTH/2 - 25, 1);

    // A header matches the footer.
    glbPopUp->appendText("\n");

    // Must make active first because append text may trigger a 
    // redraw if it is long!
    glbPopUpActive = true;
    glbPopUp->appendText(buf);

    // In case we drew due to too long text
    glbPopUp->erase();

    // Determine the size of the popup and resize.
    glbPopUp->shrinkToFit();

    while (!TCODConsole::isWindowClosed())
    {
	redrawWorld();
	key = gfx_getKey(false);
	if (delayms)
	{
	    int		currentms = (int)TCOD_sys_elapsed_milli() - startms;
	    float	ratio = currentms / (float)delayms;
	    ratio = BOUND(ratio, 0, 1);

	    glbPopUp->fillRect(0, glbPopUp->height()-1, (int)(glbPopUp->width()*ratio+0.5), 1, ATTR_WAITBAR);
	}
	if (key)
	{
	    // Don't let them abort too soon.
	    if (((int)TCOD_sys_elapsed_milli() - startms) >= delayms)
		break;
	}
    }

    glbPopUpActive = false;
    glbPopUp->erase();

    glbPopUp->resize(50, 15);
    glbPopUp->move(15, 5);

    redrawWorld();

    return key;
}

int
popupText(BUF buf, int delayms = 0)
{
    return popupText(buf.buffer(), delayms);
}

// Pops up the given text onto the screen.
// Allows the user to enter a response.
// Returns what they typed.
BUF
popupQuestion(const char *buf)
{
#define RESPONSE_LEN 50
    char	response[RESPONSE_LEN];

    glbPopUp->clear();
    glbPopUp->resize(50, SCR_HEIGHT-2);
    glbPopUp->move(15, 1);

    // A header matches the footer.
    glbPopUp->appendText("\n");

    // Must make active first because append text may trigger a 
    // redraw if it is long!
    glbPopUpActive = true;
    glbPopUp->appendText(buf);
    glbPopUp->appendText("\n");

    // In case we drew due to too long text
    glbPopUp->erase();

    // Determine the size of the popup and resize.
    glbPopUp->shrinkToFit();
    glbPopUp->resize( MAX(RESPONSE_LEN + 2, glbPopUp->getW()),
		      glbPopUp->getH() + 3 );

    redrawWorld();
    glbPopUp->getString("\n> ", response, RESPONSE_LEN);

    BUF		result;
    result.strcpy(response);

    glbPopUpActive = false;
    glbPopUp->erase();

    glbPopUp->resize(50, 30);
    glbPopUp->move(15, 10);

    redrawWorld();

    return result;
}

BUF
popupQuestion(BUF buf)
{
    return popupQuestion(buf.buffer());
}

template <typename T>
T
runMenuSelection(const PTRLIST<T> &list, T fallback, 
		 const PTRLIST<int> &hotkeys)
{
    // Run the chooser...
    if (!list.entries())
	return fallback;

    int		key;
    while (!TCODConsole::isWindowClosed())
    {
	redrawWorld();
	key = gfx_getKey(false);

	glbChooser->processKey(key);

	if (key)
	{
	    int		itemno = glbChooser->getChoice();
	    // User selects this?
	    if (key == '\n' || key == ' ')
	    {
		fallback = list(itemno);
		break;
	    }
	    else if (key == '\x1b')
	    {
		// Esc on options is to go back to play.
		// Play...
		break;
	    }
	    else
	    {
		// Ignore other options.
		for (int i = 0; i < hotkeys.entries(); i++)
		{
		    if (key == hotkeys(i))
		    {
			fallback = list(i);
			glbChooserActive = false;
			return fallback;
		    }
		}
	    }
	}
    }
    glbChooserActive = false;
    return fallback;
}

//
// Prints the welcome text, with proper varialbes
//
void
welcomeText(const char *key)
{
    popupText(text_lookup("welcome", key));
}

bool
awaitAlphaKey(int &key)
{
    key = 0;
    while (!TCODConsole::isWindowClosed())
    {
	key = gfx_getKey(false);

	if (key >= 'a' && key <= 'z')
	    return true;
	else if (key)
	    return false;
    }

    return false;
}

//
// Freezes the game until a key is pressed.
// If key is not direction, false returned and it is eaten.
// else true returned with dx/dy set.  Can be 0,0.
bool
awaitDirection(int &dx, int &dy)
{
    int			key;

    while (!TCODConsole::isWindowClosed())
    {
	redrawWorld();

	key = gfx_getKey(false);

	if (cookDirKey(key, dx, dy, false))
	{
	    return true;
	}
	else if (key)
	{
	    // Invalid direction.  We are very mean and just pick
	    // a random direction.  Hopefully I remember to remove this
	    // from future versions :>
	    // I came very close to forgetting!
	    return false;
	}
	if (key)
	    return false;
    }

    return false;
}


void
displayMob(MOB *mob)
{
    bool		done = false;

    while (!TCODConsole::isWindowClosed() && !done)
    {
	// Display description with option to edit...
	glbChooser->clear();
	glbChooser->setTextAttr(ATTR_NORMAL);

	glbChooser->setPrequelAttr(ATTR_WHITE);

	BUF		longtext = mob->getLongDescription();
	longtext = longtext.wordwrap(WORDWRAP_LEN);
	longtext.stripWS();
	longtext.append('\n');
	longtext.append('\n');
	glbChooser->setPrequel(longtext);

	glbChooser->appendChoice("Done");

	glbChooser->setChoice(0);
	glbChooserActive = true;

	// Run the chooser...
	while (!TCODConsole::isWindowClosed())
	{
	    redrawWorld();
	    int key = gfx_getKey(false);

	    glbChooser->processKey(key);

	    if (key)
	    {
		if (key == '\n' || key == ' ')
		{
		    if (glbChooser->getChoice() == 0)
		    {
			// done!
			done = true;
			break;
		    }
		}
		else if (key == '\x1b')
		{
		    // Esc on options is to go back to play.
		    // Play...
		    done = true;
		    break;
		}
	    }
	}
    }
    glbChooserActive = false;
}

void
displayItem(ITEM *item)
{
    bool		done = false;

    while (!TCODConsole::isWindowClosed() && !done)
    {
	// Display description with option to edit...
	glbChooser->clear();
	glbChooser->setTextAttr(ATTR_NORMAL);

	glbChooser->setPrequelAttr(ATTR_WHITE);

	BUF		longtext = item->getLongDescription();
	longtext = longtext.wordwrap(WORDWRAP_LEN);
	longtext.stripWS();
	longtext.append('\n');
	longtext.append('\n');
	glbChooser->setPrequel(longtext);

	int		lastchoice = 0;

	glbChooser->appendChoice("Done");



	glbChooser->setChoice(lastchoice);
	glbChooserActive = true;

	// Run the chooser...
	while (!TCODConsole::isWindowClosed())
	{
	    redrawWorld();
	    int key = gfx_getKey(false);

	    glbChooser->processKey(key);

	    if (key)
	    {
		if (key == '\n' || key == ' ')
		{
		    if (glbChooser->getChoice() == lastchoice)
		    {
			// done!
			done = true;
			break;
		    }
		}
		else if (key == '\x1b')
		{
		    // Esc on options is to go back to play.
		    // Play...
		    done = true;
		    break;
		}
	    }
	}
    }
    glbChooserActive = false;
}

void
examineTopItem()
{
    if (!glbMap || !glbMap->avatar())
	return;

    if (glbMap->avatar()->hasItem(ITEM_BLIND))
    {
	msg_report("You can't see anything when blind.");
	msg_newturn();
	return;
    }

    MOB *avatar = glbMap->avatar();
    ITEM *item = avatar->getTopBackpackItem();
    if (!item)
    {
	// Intentionally conflated with the other examine.
	msg_report("You see nothing to look at.");
    }
    else
    {
	msg_format("You look closely at %S", item);
	displayItem(item);
    }
    msg_newturn();
}

void
doExamine()
{
    int		x = glbDisplay->x() + glbDisplay->width()/2;
    int		y = glbDisplay->y() + glbDisplay->height()/2;
    int		key;
    int		dx, dy;
    POS		p;
    BUF		buf;
    bool	blind;
    bool	lookclosely = false;

    glbInvertX = x;
    glbInvertY = y;
    blind = false;
    if (glbMap && glbMap->avatar())
	blind = glbMap->avatar()->hasItem(ITEM_BLIND);
    while (!TCODConsole::isWindowClosed())
    {
	redrawWorld();

	key = gfx_getKey(false);

	// We eat space below, so check here!
	if (key == ' ')
	{
	    lookclosely = true;
	    break;
	}

	if (cookDirKey(key, dx, dy, true))
	{
	    // Space we want to select.
	    if (!dx && !dy)
	    {
		break;
	    }
	    x += dx;
	    y += dy;
	    x = BOUND(x, glbDisplay->x(), glbDisplay->x()+glbDisplay->width()-1);
	    y = BOUND(y, glbDisplay->y(), glbDisplay->y()+glbDisplay->height()-1);

	    glbInvertX = x;
	    glbInvertY = y;
	    p = glbDisplay->lookup(x, y);

	    if (p.tile() != TILE_INVALID && p.isFOV())
	    {
		p.describeSquare(blind);
	    }

	    msg_newturn();
	}

	// Any other key stops the look.
	if (key)
	{
	    if (key == '\n' || key == ' ' || key == 'x')
	    {
		// Request to look closely
		lookclosely = true;
	    }
	    break;
	}
    }

    if (lookclosely)
    {
	// Check if we left on a mob.
	// Must re look up as displayWorld may have updated the map.
	p = glbDisplay->lookup(x, y);
	if (p.mob())
	{
	    displayMob(p.mob());
	}
	else if (p.item())
	{
	    displayItem(p.item());
	}
    }

    glbInvertX = -1;
    glbInvertY = -1;
}

bool
doThrow(int itemno)
{
    int			dx, dy;

    msg_report("Throw in which direction?  ");
    if (awaitDirection(dx, dy) && (dx || dy))
    {
	msg_report(rand_dirtoname(dx, dy));
	msg_newturn();
    }
    else
    {
	msg_report("Cancelled.  ");
	msg_newturn();
	return false;
    }

    // Do the throw
    glbEngine->queue().append(COMMAND(ACTION_THROW, dx, dy, itemno));

    return true;
}

void
buildAction(ITEM *item, ACTION_NAMES *actions)
{
    glbChooser->clear();

    glbChooser->setTextAttr(ATTR_NORMAL);
    int			choice = 0;
    ACTION_NAMES	*origaction = actions;
    if (!item)
    {
	glbChooser->appendChoice("Nothing to do with nothing.");
	*actions++ = ACTION_NONE;
    }
    else
    {
	glbChooser->appendChoice("[ ] Do nothing");
	if (glbLastItemAction == ACTION_NONE)
	    choice = (int)(actions - origaction);
	*actions++ = ACTION_NONE;

	glbChooser->appendChoice("[X] Examine");
	if (glbLastItemAction == ACTION_EXAMINE)
	    choice = (int)(actions - origaction);
	*actions++ = ACTION_EXAMINE;

	if (item->isPotion())
	{
	    glbChooser->appendChoice("[Q] Quaff");
	    if (glbLastItemAction == ACTION_QUAFF)
		choice = (int)(actions - origaction);
	    *actions++ = ACTION_QUAFF;
	}
	if (item->defn().throwable)
	{
	    glbChooser->appendChoice("[T] Throw");
	    if (glbLastItemAction == ACTION_THROW)
		choice = (int)(actions - origaction);
	    *actions++ = ACTION_THROW;
	}
	if (item->isFood())
	{
	    glbChooser->appendChoice("[E] Eat");
	    if (glbLastItemAction == ACTION_EAT)
		choice = (int)(actions - origaction);
	    *actions++ = ACTION_EAT;
	}
	if (item->isWeapon())
	{
	    glbChooser->appendChoice("[W] Wield");
	    if (glbLastItemAction == ACTION_WEAR)
		choice = (int)(actions - origaction);
	    *actions++ = ACTION_WEAR;
	}
	else if (item->isRing() || item->isArmour())
	{
	    glbChooser->appendChoice("[W] Wear");
	    if (glbLastItemAction == ACTION_WEAR)
		choice = (int)(actions - origaction);
	    *actions++ = ACTION_WEAR;
	}
	if (item->isWeapon() || item->isRing())
	{
	    glbChooser->appendChoice("[F] Fuse into Equipped");
	    if (glbLastItemAction == ACTION_TRANSMUTE)
		choice = (int)(actions - origaction);
	    *actions++ = ACTION_TRANSMUTE;
	}
	if (item->isSpellbook())
	{
	    glbChooser->appendChoice("[R] Read");
	    if (glbLastItemAction == ACTION_READ)
		choice = (int)(actions - origaction);
	    *actions++ = ACTION_READ;
	}
	if (!item->defn().isflag)
	{
	    glbChooser->appendChoice("[D] Drop");
	    if (glbLastItemAction == ACTION_DROP)
		choice = (int)(actions - origaction);
	    *actions++ = ACTION_DROP;
	}
    }
    glbChooser->setChoice(choice);

    glbChooserActive = true;
}

bool
useItem(MOB *mob, ITEM *item, int itemno)
{
    ACTION_NAMES		actions[NUM_ACTIONS+1];
    bool			done = false;
    int				key;

    buildAction(item, actions);
    glbChooserActive = true;

    // Run the chooser...
    while (!TCODConsole::isWindowClosed())
    {
	redrawWorld();
	key = gfx_getKey(false);

	glbChooser->processKey(key);

	if (key)
	{
	    if (key == 'd' || key == 'D')
	    {
		glbEngine->queue().append(COMMAND(ACTION_DROP, itemno));
		done = true;
		break;
	    }
	    else if (key == 'q' || key == 'Q')
	    {
		glbEngine->queue().append(COMMAND(ACTION_QUAFF, itemno));
		done = true;
		break;
	    }
	    else if (key == 'e' || key == 'E')
	    {
		glbEngine->queue().append(COMMAND(ACTION_EAT, itemno));
		done = true;
		break;
	    }
	    else if (key == 'r' || key == 'R')
	    {
		glbEngine->queue().append(COMMAND(ACTION_READ, itemno));
		done = true;
		break;
	    }
	    else if (key == 'w' || key == 'W')
	    {
		glbEngine->queue().append(COMMAND(ACTION_WEAR, itemno));
		done = true;
		break;
	    }
	    else if (key == 'f' || key == 'F')
	    {
		glbEngine->queue().append(COMMAND(ACTION_TRANSMUTE, itemno));
		done = true;
		break;
	    }
	    else if (key == 'x' || key == 'X')
	    {
		displayItem(item);
		done = false;
		break;
	    }
	    else if (key == 't' || key == 'T')
	    {
		glbChooserActive = false;
		doThrow(itemno);
		done = true;
		break;
	    }
	    // User selects this?
	    else if (key == '\n' || key == ' ')
	    {
		ACTION_NAMES	action;

		action = actions[glbChooser->getChoice()];
		glbLastItemAction = action;
		switch (action)
		{
		    case ACTION_DROP:
			glbEngine->queue().append(COMMAND(ACTION_DROP, itemno));
			done = true;
			break;
		    case ACTION_QUAFF:
			glbEngine->queue().append(COMMAND(ACTION_QUAFF, itemno));
			done = true;
			break;
		    case ACTION_BREAK:
			glbEngine->queue().append(COMMAND(ACTION_BREAK, itemno));
			done = true;
			break;
		    case ACTION_WEAR:
			glbEngine->queue().append(COMMAND(ACTION_WEAR, itemno));
			done = true;
			break;
		    case ACTION_READ:
			glbEngine->queue().append(COMMAND(ACTION_READ, itemno));
			done = true;
			break;
		    case ACTION_TRANSMUTE:
			glbEngine->queue().append(COMMAND(ACTION_TRANSMUTE, itemno));
			done = true;
			break;
		    case ACTION_EAT:
			glbEngine->queue().append(COMMAND(ACTION_EAT, itemno));
			done = true;
			break;
		    case ACTION_SEARCH:
			glbEngine->queue().append(COMMAND(ACTION_SEARCH, itemno));
			done = true;
			break;
		    case ACTION_MEDITATE:
			glbEngine->queue().append(COMMAND(ACTION_MEDITATE, itemno));
			done = true;
			break;
		    case ACTION_EXAMINE:
			displayItem(item);
			done = false;
			break;
		    case ACTION_THROW:
			glbChooserActive = false;
			doThrow(itemno);
			done = true;
			break;
		}
		break;
	    }
	    else if (key == '\x1b')
	    {
		// Esc on options is to go back to play.
		// Play...
		break;
	    }
	    else
	    {
		// Ignore other options.
	    }
	}
    }
    glbChooserActive = false;

    return done;
}

void
buildInventory(MOB *mob, PTRLIST<int> &items, bool dointrinsics)
{
    glbChooser->clear();
    items.clear();

    glbChooser->setTextAttr(ATTR_NORMAL);
    if (!mob)
    {
	glbChooser->appendChoice("The dead own nothing.");
    }
    else
    {
	for (int i = 0; i < mob->inventory().entries(); i++)
	{
	    ITEM 		*item = mob->inventory()(i);

	    if (dointrinsics && !item->isFlag())
		continue;
	    if (!dointrinsics && item->isFlag())
		continue;

	    items.append(i);
	}
	if (!items.entries())
	{
	    glbChooser->appendChoice("Nothing");
	}

	// TFW you are about to hook into std::stalbe_sort and find
	// PTRLIST already has the hook.
	items.stableSort([&](int a, int b) -> int
	{
	    if (a == b) return false;

	    ITEM		*ia = mob->inventory()(a);
	    ITEM		*ib = mob->inventory()(b);

	    if (!ia && !ib) return false;
	    if (!ia) return false;
	    if (!ib) return true;

	    // Equipped first.
	    if (ia->isEquipped() != ib->isEquipped())
		return ia->isEquipped();

	    // Sort by class
	    if (ia->itemClass() != ib->itemClass())
		return ia->itemClass() < ib->itemClass();

	    // For weapons, sort by weapon class.
	    if (ia->isWeapon() &&
		ia->weaponClass() != ib->weaponClass())
	    {
		return ia->weaponClass() < ib->weaponClass();
	    }

	    // Magic items sort by their innate class... Hmm...
	    // this is a security leak as the magic class is fixed,
	    // so expert players can sort-id!
	    // I think this is the sort of meta-puzzle that is like
	    // price-id, and I price-ided scrolls of identify in Nethack
	    // so consider this an intentional decision!
	    // (AKA: It's not a bug if documented.)
	    // Especially as it can be closed by doing
	    // the definition sort first!
	    if ((ia->isRing() || ia->isPotion()) &&
		(ia->getMagicClass() != ib->getMagicClass()))
	    {
		return ia->getMagicClass() < ib->getMagicClass();
	    }

	    // Sort by raw defintion
	    if (ia->getDefinition() != ib->getDefinition())
		return ia->getDefinition() < ib->getDefinition();

	    // Sort by material.
	    if (ia->material() != ib->material())
		return ia->material() > ib->material();

	    // Sort by element
	    if (ia->element() != ib->element())
		return ia->element() < ib->element();

	    // Sort by corpse mob
	    if (ia->mobType() != ib->mobType())
		return ia->mobType() < ib->mobType();
	    if (ia->hero() != ib->hero())
		return ia->hero() < ib->hero();

	    // Sort by experience
	    if (ia->getExp() != ib->getExp())
		return ia->getExp() > ib->getExp();

	    // Sort by broken
	    if (ia->isBroken() != ib->isBroken())
		return ia->isBroken();

	    // If we don't find another differentiator, sort by
	    // position in inventory.
	    return a < b;
	});

	for (int i = 0; i < items.entries(); i++)
	{
	    ITEM 		*item = mob->inventory()(items(i));
	    BUF		 name = item->getName();
	    name = gram_capitalize(name);
	    if (item->getLevel() > 1)
	    {
		BUF		equip;
		equip.sprintf("%s [%d]", name.buffer(), item->getLevel());
		name.strcpy(equip);
	    }
	    if (item->isEquipped())
	    {
		BUF		equip;
		equip.sprintf("%s (equipped)", name.buffer());
		name.strcpy(equip);
	    }

	    glbChooser->appendChoice(name);
	}
    }
    glbChooser->setChoice(glbLastItemIdx);

    glbChooserActive = true;
}

void
inventoryMenu(bool dointrinsics)
{
    MOB		*avatar = (glbMap ? glbMap->avatar() : 0);
    int		key;
    bool	setlast = false;
    PTRLIST<int> itemlist;
    buildInventory(avatar, itemlist, dointrinsics);

    // Run the chooser...
    while (!TCODConsole::isWindowClosed())
    {
	redrawWorld();
	key = gfx_getKey(false);

	glbChooser->processKey(key);

	if (key)
	{
	    int		choiceno = glbChooser->getChoice();
	    int		itemno = -1;
	    if (choiceno >= 0 && choiceno < itemlist.entries())
		itemno = itemlist(choiceno);
	    bool	done = false;
	    // User selects this?
	    if (key == '\n' || key == ' ')
	    {
		done = true;

		glbLastItemIdx = glbChooser->getChoice();
		setlast = true;
		if (itemno >= 0 && itemno < avatar->inventory().entries())
		{
		    if (dointrinsics)
		    {
			// Only thing to do is examine, so skip extra
			// menu
			if (itemno >= 0 && itemno < avatar->inventory().entries())
			{
			    // This corrupts our chooser so we have
			    // do abort.
			    displayItem(avatar->inventory()(itemno));
			    buildInventory(avatar, itemlist, dointrinsics);
			    glbChooser->setChoice(choiceno);
			    done = false;
			}
		    }
		    else
		    {
			done = useItem(avatar, avatar->inventory()(itemno), itemno);
		    }
		}
		// Finish the inventory menu.
		if (done)
		{
		    break;
		}
		else
		{
		    buildInventory(avatar, itemlist, dointrinsics);
		    setlast = false;
		}
	    }
	    else if (key == '\x1b' || key == 'i' || key == 'I')
	    {
		// Esc on options is to go back to play.
		// Play...
		break;
	    }
	    else if (itemno >= 0 && itemno < avatar->inventory().entries())
	    {
		if (key == 'd' || key == 'D')
		{
		    glbEngine->queue().append(COMMAND(ACTION_DROP, itemno));
		    done = true;
		}
		else if (key == 'q' || key == 'Q')
		{
		    glbEngine->queue().append(COMMAND(ACTION_QUAFF, itemno));
		    done = true;
		}
		else if (key == 'e' || key == 'E')
		{
		    glbEngine->queue().append(COMMAND(ACTION_EAT, itemno));
		    done = true;
		}
		else if (key == 'r' || key == 'R')
		{
		    glbEngine->queue().append(COMMAND(ACTION_READ, itemno));
		    done = true;
		    break;
		}
		else if (key == 'w' || key == 'W')
		{
		    glbEngine->queue().append(COMMAND(ACTION_WEAR, itemno));
		    done = true;
		}
		else if (key == 'x' || key == 'X')
		{
		    if (itemno >= 0 && itemno < avatar->inventory().entries())
		    {
			// This corrupts our chooser so we have
			// do abort.
			displayItem(avatar->inventory()(itemno));
			buildInventory(avatar, itemlist, dointrinsics);
			glbChooser->setChoice(choiceno);
			done = false;
		    }
		}
		else if (key == 't' || key == 'T')
		{
		    glbLastItemIdx = glbChooser->getChoice();
		    setlast = true;
		    glbChooserActive = false;
		    doThrow(itemno);
		    done = true;
		    break;
		}
		else if (key == 'f' || key == 'F')
		{
		    glbLastItemIdx = glbChooser->getChoice();
		    glbEngine->queue().append(COMMAND(ACTION_TRANSMUTE, itemno));
		    done = true;
		    break;
		}
	    }
	    else
	    {
		// Ignore other options.
	    }

	    if (done)
		break;
	}
    }
    if (!setlast)
	glbLastItemIdx = glbChooser->getChoice();
    glbChooserActive = false;
}

void
castSpell(MOB *mob, SPELL_NAMES spell)
{
    int			dx = 0;
    int			dy = 0;

    if (GAMEDEF::spelldef(spell)->needsdir)
    {
	msg_report("Cast in which direction?  ");
	if (awaitDirection(dx, dy))
	{
	    msg_report(rand_dirtoname(dx, dy));
	    msg_newturn();
	}
	else
	{
	    msg_report("Cancelled.  ");
	    msg_newturn();
	    return;
	}
    }
    glbLastSpell = spell;
    glbEngine->queue().append(COMMAND(ACTION_CAST, dx, dy, spell));
}

void
castCoinSpell(int selection)
{
    int			dx = 0;
    int			dy = 0;

    if (selection & (1 << MATERIAL_STONE) && 
	(selection != ((1 << MATERIAL_STONE) | (1 << MATERIAL_WOOD))))
    {
	msg_report("Cast in which direction?  ");
	if (awaitDirection(dx, dy))
	{
	    msg_report(rand_dirtoname(dx, dy));
	    msg_newturn();
	}
	else
	{
	    msg_report("Cancelled.  ");
	    msg_newturn();
	    return;
	}
    }
    glbEngine->queue().append(COMMAND(ACTION_CAST, dx, dy, selection));
}

void
buildZapList(MOB *mob, PTRLIST<SPELL_NAMES> &list)
{
    glbChooser->clear();
    list.clear();
    int				chosen = 0;

    glbChooser->setTextAttr(ATTR_NORMAL);
    if (!mob)
    {
	glbChooser->appendChoice("The dead can cast nothing.");
	list.append(SPELL_NONE);
    }
    else
    {
	SPELL_NAMES		spell;

	for (spell = SPELL_NONE; spell < GAMEDEF::getNumSpell(); spell = (SPELL_NAMES) (spell + 1))
	{
	    ITEM_NAMES		itemname;
	    if (spell == SPELL_NONE)
		continue;
	    itemname = (ITEM_NAMES) GAMEDEF::spelldef(spell)->item;
	    // None now implies cantrips you always have.
	    if (itemname == ITEM_NONE ||
		mob->hasItem(itemname))
	    {
		BUF		buf;

		buf.sprintf("Cast %s", GAMEDEF::spelldef(spell)->name);
		glbChooser->appendChoice(buf);
		list.append(spell);
		if (spell == glbLastSpell)
		    chosen = list.entries()-1;
	    }
	}
    }
    if (!glbChooser->entries())
    {
	glbChooser->appendChoice("You know no magic spells.");
	list.append(SPELL_NONE);
    }

    glbChooser->setChoice(chosen);

    glbChooserActive = true;
}

void
buildZapCoinList(MOB *mob, PTRLIST<int> &list, int selection)
{
    glbChooser->clear();
    list.clear();
    int				chosen = 0;

    glbChooser->setTextAttr(ATTR_NORMAL);
    if (!mob)
    {
	glbChooser->appendChoice("The dead can cast nothing.");
	list.append(0);
    }
    else
    {
	MATERIAL_NAMES		material;

	FOREACH_MATERIAL(material)
	{
	    if (material == MATERIAL_NONE)
		continue;

	    ITEM		*coins;
	    int			 bitmask = 1 << material;
	    coins = mob->lookupItem(ITEM_COIN, material);
	    if (coins && coins->getStackCount())
	    {
		BUF		name, choice;
		name = gram_capitalize(coins->getName());
		choice.sprintf("[%s] %s",
			(selection & bitmask) ? "X" : " ",
			name.buffer());
		glbChooser->appendChoice(choice);
		list.append(bitmask);
	    }
	}
    }
    if (!glbChooser->entries())
    {
	glbChooser->appendChoice("You have no material to cast with.");
	list.append(0);
    }
    else
    {
	glbChooser->appendChoice("Cast With Selected Materials.");
	list.append(0);
    }

    glbChooser->setChoice(chosen);

    glbChooserActive = true;
}

int
zapMenu(bool actuallycast)
{
    MOB		*avatar = (glbMap ? glbMap->avatar() : 0);

    int			selection = 0;
    bool		done = false;
    bool		docast = false;
    int			chosen = 0;
    while (!done)
    {
	PTRLIST<int>		list;

	buildZapCoinList(avatar, list, selection);
	glbChooser->setChoice(chosen);
	int		key;
	while (!TCODConsole::isWindowClosed())
	{
	    redrawWorld();
	    key = gfx_getKey(false);

	    glbChooser->processKey(key);

	    if (key)
	    {
		int		itemno = glbChooser->getChoice();
		chosen = itemno;
		// User selects this?
		if (key == '\n' || key == ' ')
		{
		    if (list(itemno))
		    {
			selection ^= list(itemno);
		    }
		    else
		    {
			docast = true;
			done = true;
		    }
		    break;
		}
		else if (key == '\x1b')
		{
		    // Esc on options is to go back to play.
		    // Play...
		    done = true;
		    break;
		}
	    }
	}
	glbChooserActive = false;
    }

    if (docast && actuallycast)
    {
	castCoinSpell(selection);
    }
    if (!docast)
	selection = 0;
    return selection;

#if 0
    PTRLIST<SPELL_NAMES>	spelllist;
    PTRLIST<int>		hotkeys;
    buildZapList(avatar, spelllist);

    SPELL_NAMES	spell = runMenuSelection(spelllist, SPELL_NONE, hotkeys);

    if (spell != SPELL_NONE)
    {
	castSpell(avatar, spell);
    }
    glbChooserActive = false;
#endif
}

void
buildOptionsMenu(OPTION_NAMES d)
{
    OPTION_NAMES	option;
    glbChooser->clear();

    glbChooser->setTextAttr(ATTR_NORMAL);
    FOREACH_OPTION(option)
    {
	if (option == OPTION_PLAY)
	{
	    BUF		buf;
	    buf.sprintf("Play", glbWorldName);
	    glbChooser->appendChoice(buf);
	}
	else
	    glbChooser->appendChoice(glb_optiondefs[option].name);
    }
    glbChooser->setChoice(d);

    glbChooserActive = true;
}

bool
terminalOutput(const char *text)
{
    gfx_faketerminal_setheader(1);
    gfx_faketerminal_write("+++\n");
    gfx_faketerminal_write("OK\n");
    gfx_faketerminal_write(text);
    gfx_faketerminal_write("\nEOF\n");

    while (!TCODConsole::isWindowClosed())
    {
	int key = gfx_getKey(true);
	if (key)
	    break;
    }

    gfx_faketerminal_write("\nATO\n");
    gfx_delay(200);
    redrawWorld();
    return true;
}

bool terminalOutput(BUF buf) { return terminalOutput(buf.buffer()); }


bool
optionsMenu()
{
    int			key;
    bool		done = false;

    buildOptionsMenu(OPTION_PLAY);

    // Run the chooser...
    while (!TCODConsole::isWindowClosed())
    {
	redrawWorld();
	key = gfx_getKey(false);

	glbChooser->processKey(key);

	if (key)
	{
	    // User selects this?
	    if (key == '\n' || key == ' ')
	    {
		if (glbChooser->getChoice() == OPTION_INSTRUCTIONS)
		{
		    // Instructions.
		    popupText(text_lookup("game", "help"));
		}
		if (glbChooser->getChoice() == OPTION_WELCOME)
		{
		    // Instructions.
		    popupText(text_lookup("welcome", "You1"));
		}
		if (glbChooser->getChoice() == OPTION_ABOUT)
		{
		    // Instructions.
		    popupText(text_lookup("game", "about"));
		}
		else if (glbChooser->getChoice() == OPTION_PLAY)
		{
		    // Play...
		    break;
		}
#if 0
		else if (glbChooser->getChoice() == OPTION_VOLUME)
		{
		    int			i;
		    BUF			buf;
		    // Volume
		    glbChooser->clear();

		    for (i = 0; i <= 10; i++)
		    {
			buf.sprintf("%3d%% Volume", (10 - i) * 10);

			glbChooser->appendChoice(buf);
		    }
		    glbChooser->setChoice(10 - glbConfig->myMusicVolume);

		    while (!TCODConsole::isWindowClosed())
		    {
			redrawWorld();
			key = gfx_getKey(false);

			glbChooser->processKey(key);

			setMusicVolume(10 - glbChooser->getChoice());
			if (key)
			{
			    break;
			}
		    }
		    buildOptionsMenu(OPTION_VOLUME);
		}
#endif
		else if (glbChooser->getChoice() == OPTION_FULLSCREEN)
		{
		    glbConfig->myFullScreen = !glbConfig->myFullScreen;
		    // This is intentionally unrolled to work around a
		    // bool/int problem in libtcod
		    if (glbConfig->myFullScreen)
			TCODConsole::setFullscreen(true);
		    else
			TCODConsole::setFullscreen(false);
		}
		else if (glbChooser->getChoice() == OPTION_QUIT)
		{
		    // Quit
		    done = true;
		    break;
		}
	    }
	    else if (key == '\x1b')
	    {
		// Esc on options is to go back to play.
		// Play...
		break;
	    }
	    else
	    {
		// Ignore other options.
	    }
	}
    }
    glbChooserActive = false;

    return done;
}

bool
reloadLevel(bool alreadybuilt, bool alreadywaited = false, bool reload=false)
{
    if (TCODConsole::isWindowClosed())
	return true;

    // Build a new game world!
    glbLevel = 0;
    msg_newturn();
    glbMessagePanel->clear();
    glbMessagePanel->appendText("> ");

    for (int i = 0; i < 10; i++)
    {
	glbUnitWaypoint[i] = IVEC2(-1, -1);
    }

    glbVeryFirstRun = false;

    if (!alreadybuilt)
	glbEngine->queue().append(COMMAND(ACTION_RESTART, glbLevel));

    startMusic();

    // Wait for the map to finish.
    if (!alreadywaited)
	reload = glbEngine->awaitRebuild();

    text_registervariable("FOE", GAMEDEF::mobdef(MOB::getBossName())->name);
    text_registervariable("PRIZE", GAMEDEF::itemdef(ITEM_MACGUFFIN)->name);
    if (reload)
    {
	welcomeText("Back");
    }
    else
    {
	welcomeText("You1");
    }

    // Redrawing the world updates our glbMap so everyone will see it.
    redrawWorld();

    return false;
}

// Let the avatar watch the end...
void
deathCoolDown()
{
    int		coolstartms = TCOD_sys_elapsed_milli();
    int		lastenginems = coolstartms;
    int		curtime;
    bool	done = false;
    double	ratio;

    glbDisplay->fadeToBlack(true);
    while (!done)
    {
	curtime = TCOD_sys_elapsed_milli();

	// Queue wait events, one per 100 ms.
	while (lastenginems + 200 < curtime)
	{
	    glbEngine->queue().append(COMMAND(ACTION_WAIT));
	    lastenginems += 200;
	}

	// Set our fade.
	if (!glbDisplay->fadeToBlack(true))
	{
	    done = true;
	    ratio = 0;
	}

	// Keep updating
	redrawWorld();
    }

    gfx_clearKeyBuf();
}

void
shutdownEverything(bool save)
{
#if 1
    glbConfig->save("../impact.cfg");

    // Save the game.  A no-op if dead.
    if (save)
    {
	glbEngine->queue().append(COMMAND(ACTION_SAVE));
	glbEngine->awaitSave();
    }
    else
    {
	glbEngine->queue().append(COMMAND(ACTION_SHUTDOWN));
	glbEngine->awaitSave();
    }

#if 0
    BUF		path;
    path.sprintf("../save/%s.txt", glbWorldName);
    GAMEDEF::save(path.buffer());
#endif
#else
    glbEngine->queue().append(COMMAND(ACTION_SHUTDOWN));
    glbEngine->awaitSave();
#endif

    stopMusic(true);
#ifdef USE_AUDIO
    if (glbTunes)
	Mix_FreeMusic(glbTunes);
#endif

    gfx_shutdown();

#ifdef USE_AUDIO
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
#endif

    //SDL_Quit();
}

void
queryOfferPrice(COMMAND &cmd)
{
    MOB		*avatar = (glbMap ? glbMap->avatar() : 0);
    if (!avatar || !avatar->alive())
    {
	cmd.action() = ACTION_NONE;
	return;
    }

    MOB		*at = avatar->map()->findMob(cmd.dy());
    ITEM	*item = avatar->getItemFromId(cmd.dz());
    if (!item || !at)
    {
	cmd.action() = ACTION_NONE;
	return;
    }

    BUF		query;
    BUF		itemname = item->getSingleName();

    query.sprintf("You notice %s has %d gold.\nHow much will you demand for %s?\n",
	    at->getName().buffer(),
	    at->getGold(),
	    itemname.buffer() );

    BUF		amount = popupQuestion(query);

    cmd.dw() = atoi(amount.buffer());
    if (cmd.dw() <= 0)
	cmd.action() = ACTION_NONE;
}

void
queryPurchasePrice(COMMAND &cmd)
{
    MOB		*avatar = (glbMap ? glbMap->avatar() : 0);
    if (!avatar || !avatar->alive())
    {
	cmd.action() = ACTION_NONE;
	return;
    }

    MOB		*at = avatar->map()->findMob(cmd.dy());
    ITEM	*item = at->getItemFromId(cmd.dz());

    if (!item || !at)
    {
	cmd.action() = ACTION_NONE;
	return;
    }
    BUF		query;
    BUF		itemname = item->getSingleName();

    query.sprintf("You have %d gold.\nHow much will you offer for %s?\n",
	    avatar->getGold(),
	    itemname.buffer() );

    BUF		amount = popupQuestion(query);

    cmd.dw() = atoi(amount.buffer());
    if (cmd.dw() <= 0)
	cmd.action() = ACTION_NONE;
}

void
buildShopMenu(MOB_NAMES keeper, PTRLIST<COMMAND> &list, PTRLIST<int> &hotkeys)
{
    glbChooser->clear();
    list.clear();
    BUF			buf;
    MOB		*avatar = (glbMap ? glbMap->avatar() : 0);
    bool	valid = false;

    if (!avatar || !avatar->alive())
	return;

    glbChooser->setPrequelAttr((ATTR_NAMES) glb_mobdefs[keeper].attr);

    if (valid)
    {
	glbChooser->appendChoice("No thank you.");
	list.append(COMMAND(ACTION_NONE));
	glbChooser->setChoice(list.entries()-1);
	glbChooserActive = true;
    }
    else
    {
	glbChooser->clear();
	list.clear();
    }
}


void
runShop(int shopkeeper_uid)
{
    PTRLIST<COMMAND>	shoppinglist;
    PTRLIST<int>	hotkeys;

    if (!glbMap)
	return;
    MOB		*shopkeeper_mob = glbMap->findMob(shopkeeper_uid);
    if (!shopkeeper_mob)
	return;

    // buildShopMenu(shopkeeper, shoppinglist, hotkeys);
    
    if (shoppinglist.entries() == 0)
	return;

    // Run the chooser...
    COMMAND	cmd = runMenuSelection(shoppinglist, COMMAND(ACTION_NONE), hotkeys);

    if (cmd.action() != ACTION_NONE)
    {
	glbEngine->queue().append(cmd);
    }
}

void
stopWaiting()
{
    if (glbMap && glbMap->avatar() && 
	glbMap->avatar()->alive())
    {
	if (glbMap->avatar()->isWaiting())
	{
	    glbEngine->queue().append(COMMAND(ACTION_STOPWAITING));
	}
    }
}

#ifdef WIN32
int WINAPI
WinMain(HINSTANCE hINstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCMdSHow)
#else
int 
main(int argc, char **argv)
#endif
{
    bool		done = false;

    // Clamp frame rate.
    /// Has to be high to fix keyrepeat problem :<
    TCOD_sys_set_fps(200);

    glbConfig = new CONFIG();
    glbConfig->load("../impact.cfg");

    // Dear Microsoft,
    // The following code in optimized is both a waste and seems designed
    // to ensure that if we call a bool where the callee pretends it is
    // an int we'll end up with garbage data.
    //
    // 0040BF4C  movzx       eax,byte ptr [eax+10h] 

    // TCODConsole::initRoot(SCR_WIDTH, SCR_HEIGHT, "The Smith's Hand", myFullScreen);
    // 0040BF50  xor         ebp,ebp 
    // 0040BF52  cmp         eax,ebp 
    // 0040BF54  setne       cl   
    // 0040BF57  mov         dword ptr [esp+28h],eax 
    // 0040BF5B  push        ecx  

    TCODConsole::setCustomFont("terminal.png", TCOD_FONT_LAYOUT_ASCII_INCOL | TCOD_FONT_TYPE_GREYSCALE);

    // My work around is to constantify the fullscreen and hope that
    // the compiler doesn't catch on.
    if (glbConfig->myFullScreen)
	TCODConsole::initRoot(SCR_WIDTH, SCR_HEIGHT, "Rogue Impact", true);
    else
	TCODConsole::initRoot(SCR_WIDTH, SCR_HEIGHT, "Rogue Impact", false);

    // TCOD doesn't do audio.
#ifdef USE_AUDIO
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (Mix_OpenAudio(22050, AUDIO_S16, 2, 4096))
    {
	printf("Failed to open audio!\n");
	exit(1);
    }
#endif

    // SDL_InitSubSystem(SDL_INIT_JOYSTICK);

    setMusicVolume(glbConfig->musicVolume());

    rand_setseed((long) time(0));
    for (int i = 0; i < 10000; i++)
	rand_choice(2);

    text_init();

    gfx_init();
    MAP::init();

    GAMEDEF::reset(true);

    thesaurus_init();

    glbDisplay = new DISPLAY(0, 0, 72, 40, 0);

    glbMessagePanel = new PANEL(65, 20);
    msg_registerpanel(glbMessagePanel);
    glbMessagePanel->move(SCR_WIDTH-68, SCR_HEIGHT-20);

    glbPopUp = new PANEL(50, 80);
    glbPopUp->move(SCR_WIDTH/2 - 25, 5);
    glbPopUp->setBorder(true, ' ', ATTR_BORDER);
    // And so the mispelling is propagated...
    glbPopUp->setRigthMargin(1);
    glbPopUp->setIndent(1);

    glbJacob = new PANEL(65, SCR_HEIGHT-22);
    glbJacob->move(SCR_WIDTH-68, 0);
    glbJacob->setTransparent(true);
    glbJacob->setAllowScroll(false);

    glbHelp = new PANEL(65, 2);
    glbHelp->move(SCR_WIDTH-68, SCR_HEIGHT-22);
    glbHelp->setTransparent(true);
    glbHelp->setAllowScroll(false);

    glbStatLine = new PANEL(65, 2);
    glbStatLine->move(SCR_WIDTH-68, SCR_HEIGHT-22);
    glbStatLine->setTransparent(true);

    glbVictoryInfo = new PANEL(26, 5);
    glbVictoryInfo->move(SCR_WIDTH/2 - 13, 2);
    glbVictoryInfo->setIndent(2);
    glbVictoryInfo->setBorder(true, ' ', ATTR_VICTORYBORDER);
    glbVictoryInfo->clear();
    glbVictoryInfo->newLine();
    glbVictoryInfo->appendText("The battle has ended!");
    glbVictoryInfo->newLine();
    glbVictoryInfo->newLine();
    glbVictoryInfo->appendText("Press [V] to finish.");

    glbLevel = 1;

    glbChooser = new CHOOSER();
    glbChooser->move(SCR_WIDTH/2, SCR_HEIGHT/2, CHOOSER::JUSTCENTER, CHOOSER::JUSTCENTER);
    glbChooser->setBorder(true, ' ', ATTR_BORDER);
    glbChooser->setIndent(1);
    glbChooser->setRigthMargin(1);
    glbChooser->setAttr(ATTR_NORMAL, ATTR_HILITE, ATTR_LIGHTBLACK, ATTR_DKHILITE);

    glbLevelBuildStatus = new CHOOSER();
    glbLevelBuildStatus->move(SCR_WIDTH/2, SCR_HEIGHT/2, CHOOSER::JUSTCENTER, CHOOSER::JUSTCENTER);
    glbLevelBuildStatus->setBorder(true, ' ', ATTR_BORDER);
    glbLevelBuildStatus->setIndent(1);
    glbLevelBuildStatus->setRigthMargin(1);
    glbLevelBuildStatus->setAttr(ATTR_NORMAL, ATTR_HILITE, ATTR_HILITE,
				ATTR_HILITE);

    glbEngine = new ENGINE(glbDisplay);

    if (1)
    {
	glbCoinStack = 0;
    }
    else
    {
	glbCoinStack = new FIREFLY(0, 20, SCR_HEIGHT, FIRE_HEALTH, false);
	glbCoinStack->setBarGraph(false);

	{
	    ATTR_DEF	*def = GAMEDEF::attrdef(GAMEDEF::itemdef(ITEM_COIN)->attr);
	    glbCoinStack->setColor( def->fg_r, def->fg_g, def->fg_b );
	}
    }

    // Run our first level immediately so it can be built or 
    // loaded from disk while people wait.
    // The problem is we expect to be able to create/restore
    // worlds from the options menu, so we had better have
    // finished it prior to that!
    glbEngine->queue().append(COMMAND(ACTION_RESTART, 0));

    glbReloadedLevel = glbEngine->awaitRebuild();
    glbHasDied = false;

    done = reloadLevel(true, true, glbReloadedLevel);
    if (done)
    {
	shutdownEverything(true);
	return 0;
    }

    bool		clearkeybuf = false;
    unsigned int			framestartMS = TCOD_sys_elapsed_milli();
    do
    {
	int		key;
	int		dx, dy;

	// our own frame governer.
	while (TCOD_sys_elapsed_milli() - framestartMS < 33)
	{
	    THREAD::yield();
	    TCOD_sys_sleep_milli(5);
	}
	framestartMS = TCOD_sys_elapsed_milli();

	redrawWorld();

	while (1)
	{
	    BUF		pop;

	    pop = glbEngine->getNextPopup();
	    if (!pop.isstring())
		break;

	    popupText(pop, POPUP_DELAY);
	}

	while (1)
	{
	    ENGINE::Question		query;

	    if (glbEngine->getQuestion(query))
	    {
		glbEngine->answerQuestion(engine_askQuestion(query));
	    }
	    else
		break;
	}


	if (glbMap && glbMap->avatar() &&
	    !glbMap->avatar()->alive())
	{
	    // You have died...
	    deathCoolDown();

	    gfx_clearKeyBuf();
	    clearkeybuf = true;

	    text_registervariable("FOE", GAMEDEF::mobdef(MOB::getBossName())->name);
	    text_registervariable("PRIZE", GAMEDEF::itemdef(ITEM_MACGUFFIN)->name);
	    popupText(text_lookup("game", "lose"));

	    stopMusic();

	    // Don't prep a reload.
	    done = reloadLevel(false);
	    glbHasDied = false;

	    glbDisplay->fadeToBlack(false);
	}
	else if (glbMap && glbMap->avatar() &&
		glbMap->avatar()->hasWon())
	{
	    // Traditional win.
	    gfx_clearKeyBuf();
	    clearkeybuf = true;

	    text_registervariable("FOE", GAMEDEF::mobdef(MOB::getBossName())->name);
	    text_registervariable("PRIZE", GAMEDEF::itemdef(ITEM_MACGUFFIN)->name);
	    popupText(text_lookup("game", "victory"));
	    glbHasDied = false;

	    stopMusic();
	    done = reloadLevel(false);
	}
	else if (glbMap && !glbMap->avatar())
	{
	    stopMusic();
	    done = reloadLevel(false);
	    glbHasDied = false;
	}

	if (glbMap)
	{
	    if (!glbHasDied)
	    {
		// See if the game is over!
	    }
	}

	// Wait for queue to empty so we don't repeat.
	// glbEngine->awaitQueueEmpty();
	if (clearkeybuf)
	{
	    gfx_clearKeyBuf();
	    clearkeybuf = false;
	}

	key = gfx_getKey(false);

	if (!key)
	    continue;

	if (cookDirKey(key, dx, dy, false))
	{
	    // If no motion, it is a normal wait.
	    if (!dx && !dy)
	    {
		glbEngine->queue().appendIfEmpty(COMMAND(ACTION_WAIT));
	    }
	    else
	    {
		// Direction.
		glbEngine->queue().appendIfEmpty(COMMAND(ACTION_BUMP, dx, dy));
	    }
	    // empty key buffer.
	    // DAMN YOU TCOD!
	    gfx_clearKeyBuf();
	    clearkeybuf = true;
	}

	KEY_NAMES cookkey = cookKey(key);

	// Unbindable:
	switch (key)
	{
	    case 'a':
	    case 's':
	    case 'd':
	    case 'f':
	    {
		const char *slotnames = "asdf";
		int	select = strchr(slotnames, key) - slotnames;
		glbEngine->queue().append(COMMAND(ACTION_SWITCHLEADER, select));
		break;
	    }

	    case '\x1b':
		// If meditating, abort.
		if (glbMap && glbMap->avatar() && 
		    glbMap->avatar()->alive() && glbMap->avatar()->isMeditating())
		{
		    glbEngine->queue().append(COMMAND(ACTION_GHOSTRECENTER));
		    break;
		}
		// If waiting, abort.
		if (glbMap && glbMap->avatar() && 
		    glbMap->avatar()->alive() && glbMap->avatar()->isWaiting())
		{
		    glbEngine->queue().append(COMMAND(ACTION_STOPWAITING));
		    break;
		}
		// Trigger options
		done = optionsMenu();
		break;

	    case GFX_KEYF1:
	    case '?':
		stopWaiting();
		popupText(text_lookup("game", "help"));
		break;
	}

	switch (cookkey)
	{
	    case KEY_WELCOME:
	    {
		welcomeText("You1");
		break;
	    }
	    case KEY_ABOUT:
		popupText(text_lookup("game", "about"));
		break;

	    case KEY_FULLSCREEN:
		glbConfig->myFullScreen = !glbConfig->myFullScreen;
		// This is intentionally unrolled to work around a
		// bool/int problem in libtcod
		if (glbConfig->myFullScreen)
		    TCODConsole::setFullscreen(true);
		else
		    TCODConsole::setFullscreen(false);
		break;
	    case KEY_OPTIONS:
		stopWaiting();
		done = optionsMenu();
		break;
	    case KEY_QUIT:
		done = true;
		break;
	    case KEY_RELOAD:
	    {
		// It is rather dangerous to leave reload, let people
		// just die.  Or if I care, add with a prompt on the
		// options menu.
		BUF		restart_query;
		restart_query.reference("Are you sure you wish to restart from the beginning?");
		const char *answers[] = { "Yes, a fresh start!", "No, I wish to keep playing.", 0 };
		int choice = glbEngine->askQuestionList(restart_query, answers, 1, true);
		if (choice == 0)
		{
		    done = reloadLevel(false);
		}
		break;
	    }

	    case KEY_INVENTORY:
		inventoryMenu(false);
		break;

	    case KEY_INTRINSICS:
		inventoryMenu(true);
		break;

	    case KEY_CLIMB:
	    case KEY_CLIMBUP:
	    case KEY_CLIMBDOWN:
		glbEngine->queue().append(COMMAND(ACTION_CLIMB));
		break;

	    case KEY_EXAMINE:
		doExamine();
		break;

	    case KEY_PICKUP:
	    case KEY_PICKUP2:
		glbEngine->queue().append(COMMAND(ACTION_PICKUP));
		break;

	    case KEY_EBURST:
		// Your 'Q' attack
		if (glbMap && glbMap->avatar())
		{
		    MOB *avatar = glbMap->avatar();
		    castSpell(avatar, (SPELL_NAMES) avatar->hero().burst);
		}
		break;

	    case KEY_ESKILL:
		// Your 'E' attack
		if (glbMap && glbMap->avatar())
		{
		    MOB *avatar = glbMap->avatar();
		    castSpell(avatar, (SPELL_NAMES) avatar->hero().skill);
		}
		break;

	    case KEY_FIRE:
		msg_report("Shoot in what direction?  ");
		if (awaitDirection(dx, dy) && (dx || dy))
		{
		    msg_report(rand_dirtoname(dx, dy));
		    msg_newturn();
		    glbEngine->queue().append(COMMAND(ACTION_FIRE, dx, dy));
		}
		else
		{
		    msg_report("Cancelled.  ");
		    msg_newturn();
		}
		break;

	    case KEY_SEARCH:
		glbEngine->queue().append(COMMAND(ACTION_SEARCH));
		break;
	    case NUM_KEYS:
		break;
	}
    } while (!done && !TCODConsole::isWindowClosed());

    shutdownEverything(true);

    return 0;
}
