/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	Jacob's Matrix Development
 *
 * NAME:        engine.cpp ( Jacob's Matrix, C++ )
 *
 * COMMENTS:
 *	Our game engine.  Grabs commands from its command
 *	queue and processes them as fast as possible.  Provides
 *	a mutexed method to get a recent map from other threads.
 *	Note that this will run on its own thread!
 */

#include <libtcod.hpp>
#undef CLAMP

#include "engine.h"

#include "msg.h"
#include "rand.h"
#include "map.h"
#include "mob.h"
#include "item.h"
#include "text.h"
#include "chooser.h"
#include "gfxengine.h"
#include <time.h>

// #define DO_TIMING

// Avoid asking to quickly for things like AI and what not.
#define QUESTION_TIMEOUT	30

// Prototypes stolen from Main
extern int popupText(const char *buf, int delayms = 0);
extern int popupText(BUF buf, int delayms = 0);
extern BUF popupQuestion(const char *buf);
extern BUF popupQuestion(BUF buf);
extern void redrawWorld();

extern CHOOSER		*glbChooser;
extern bool		 glbChooserActive;

ENGINE::Response
engine_askQuestion(const ENGINE::Question &query)
{
    ENGINE::Response	response;

    switch (query.type)
    {
	case ENGINE::QUERY_TEXT:
	{
	    BUF		text = query.query;
	    text.stripWS();
	    response.type = query.type;
	    response.answer = popupQuestion(text);
	    break;
	}
	case ENGINE::QUERY_CHOICE:
	{
	    glbChooser->clear();

	    glbChooser->setTextAttr(ATTR_NORMAL);
	    BUF		wrapped;

	    wrapped = query.query.wordwrap(WORDWRAP_LEN);
	    wrapped.stripWS();
	    wrapped.append('\n');
	    wrapped.append('\n');

	    glbChooser->setPrequel(wrapped);
	    glbChooser->setPrequelAttr(ATTR_WHITE);
	    for (int i = 0; i < query.choices.entries(); i++)
		glbChooser->appendChoice(query.choices(i));
	    glbChooser->setChoice(query.defaultchoice);

	    response.choice = -1;

	    // Run the chooser...
	    glbChooserActive = true;
	    while (!TCODConsole::isWindowClosed())
	    {
		redrawWorld();
		int key = gfx_getKey(false);

		glbChooser->processKey(key);

		if (key)
		{
		    // User selects this?
		    if (key == '\n' || key == ' ')
		    {
			response.choice = glbChooser->getChoice();
			break;
		    }
		    else if (key == '\x1b')
		    {
			// Choose nothing.
			break;
		    }
		}
	    }
	    glbChooserActive = false;
	    if (response.choice >= 0)
		response.type = ENGINE::QUERY_CHOICE;

	    break;
	}
	case ENGINE::QUERY_INVALID:
	    // Leave default
	    break;
    }

    return response;
}

static void *
threadstarter(void *vdata)
{
    ENGINE	*engine = (ENGINE *) vdata;

    engine->mainLoop();

    return 0;
}

ENGINE::ENGINE(DISPLAY *display)
{
    myMap = myOldMap = 0;
    myDisplay = display;
    enableQuestions(true);
    myQuestionTimeOut = QUESTION_TIMEOUT;

    myThread = THREAD::alloc();
    myThread->start(threadstarter, this);
}

ENGINE::~ENGINE()
{
    if (myMap)
	myMap->decRef();
    if (myOldMap)
	myOldMap->decRef();
}

MAP *
ENGINE::copyMap()
{
    AUTOLOCK	a(myMapLock);

    if (myOldMap)
	myOldMap->incRef();

    return myOldMap;
}

void
ENGINE::updateMap(bool forcepost)
{
    bool		postedmap = true;

    {
	AUTOLOCK	a(myMapLock);

	if (myOldMap)
	{
	    if (myOldMap->refCount() == 1 && !forcepost)
	    {
		// No one has copied it out, no point updating to the
		// most recent until they have!
		postedmap = false;
	    }
	    else
	    {
		// Post ourselves!
		myOldMap->decRef();

	       myOldMap = myMap;
	    }
	}
	else
	{
	    myOldMap = myMap;
	}
    }
    // If we posted ourselves as oldmap, we have to copy into a new
    // version.  This is safe to do outside the lock as we've ideally
    // got read only.
    if (myOldMap && postedmap)
    {
#ifdef DO_TIMING
    int 		movestart = TCOD_sys_elapsed_milli();
#endif
	myMap = new MAP(*myOldMap);
	myMap->incRef();
#ifdef DO_TIMING
    int 		moveend = TCOD_sys_elapsed_milli();
    cerr << "Map update " << moveend - movestart << endl;
#endif
    }
}

#define VERIFY_ALIVE() \
    if (avatar && !avatar->alive()) \
    {			\
	msg_report("Dead people can't move.  ");	\
	break;			\
    }

void redrawWorld();

void
ENGINE::awaitQueueEmpty()
{
    int		value = 0;
    queue().append(COMMAND(ACTION_MARK_QUEUE_EMPTY));

    while (!myEngineQueueEmpty.remove(value))
    {
	redrawWorld();
    }
    myEngineQueueEmpty.clear();
    return;
}

bool
ENGINE::awaitRebuild(bool redraw)
{
    int		value = 0;

    while (!myRebuildQueue.remove(value))
    {
	answerAnyQuestions();
	if (redraw)
	    redrawWorld();
    }

    myRebuildQueue.clear();

    return value ? true : false;
}

void
ENGINE::awaitSave(bool redraw)
{
    int		value;

    while (!mySaveQueue.remove(value))
    {
	answerAnyQuestions();
	if (redraw)
	    redrawWorld();
    }

    mySaveQueue.clear();
}

BUF
ENGINE::getNextPopup()
{
    BUF		result;

    myPopupQueue.remove(result);

    return result;
}

void
ENGINE::popupText(const char *text)
{
    BUF		buf;
    
    buf.strcpy(text);

    myPopupQueue.append(buf);
}

BUF
ENGINE::askQuestionText(BUF text, bool isclient)
{
    Question		query;
    Response		response;
    query.query = text;
    query.type = QUERY_TEXT;
    response = askQuestion(query, isclient);

    if (response.type == QUERY_TEXT)
    {
	return response.answer;
    }
    BUF		empty;
    return empty;
}

bool
ENGINE::askQuestionNum(BUF text, int &result, bool isclient)
{
    Question		query;
    Response		response;
    query.query = text;
    query.type = QUERY_TEXT;
    response = askQuestion(query, isclient);

    if (response.type == QUERY_TEXT)
    {
	if (response.answer.isstring())
	{
	    if (response.answer.buffer()[0] == '+' ||
	        response.answer.buffer()[0] == '-' ||
	        MYisdigit(response.answer.buffer()[0]) )
	    {
		result = atoi(response.answer.buffer());
		return true;
	    }
	}
    }
    return false;
}

BUF
ENGINE::askQuestionTextCanned(const char *dict, const char *key, bool isclient)
{
    return askQuestionText(text_lookup(dict, key), isclient);
}

int
ENGINE::askQuestionList(BUF text, const PTRLIST<BUF> &choices, int defchoice, bool isclient)
{
    Question	query;
    query.type = QUERY_CHOICE;
    query.query = text;
    query.choices = choices;
    query.defaultchoice = defchoice;

    Response	response;
    if (isclient)
	response = engine_askQuestion(query);
    else
	response = askQuestion(query, isclient);

    if (response.type != QUERY_CHOICE)
	return -1;
    return response.choice;
}

int
ENGINE::askQuestionList(BUF text, const char **choices, int defchoice, bool isclient)
{
    PTRLIST<BUF>	list;

    for (int i = 0; choices[i]; i++)
    {
	BUF	tmp;
	tmp.reference(choices[i]);
	list.append(tmp);
    }

    return askQuestionList(text, list, defchoice, isclient);
}

ENGINE::Response
ENGINE::askQuestion(Question query, bool isclient)
{
    if (isclient)
	return engine_askQuestion(query);

    if (!questionsEnabled())
    {
	Response	dummy;
	return dummy;
    }

    myQuestionTimeOut = QUESTION_TIMEOUT;
    myQuestionQueue.append(query);

    return myResponseQueue.waitAndRemove();
}

bool
ENGINE::getQuestion(Question &query)
{
    return myQuestionQueue.remove(query);
}

void
ENGINE::answerQuestion(Response response)
{
    myResponseQueue.append(response);
}

void
ENGINE::answerAnyQuestions()
{
    Question	query;
    Response	response;

    while (getQuestion(query))
	answerQuestion(response);
}

void
ENGINE::mainLoop()
{
    rand_setseed((long) time(0));
    for (int i = 0; i < 10000; i++)
	rand_choice(2);

    COMMAND		cmd;
    MOB			*avatar;
    bool		timeused = false;
    bool		doheartbeat = true;

    while (1)
    {
	avatar = 0;
	if (myMap)
	    avatar = myMap->avatar();

	bool		possiblystop = false;

	// If there is a command waiting, we should stop waiting on
	// the avatar!
	if (avatar && !queue().isEmpty())
	{
	    possiblystop = avatar->isWaiting();
	    avatar->stopWaiting(0);
	}

	timeused = false;
	if (doheartbeat && avatar && avatar->aiForcedAction())
	{
	    // Make sure we have lots of new turns here!
	    if (avatar && avatar->isWaiting())
		msg_newturn();
	    cmd = COMMAND(ACTION_NONE);
	    timeused = true;
	}
	else
	{
	    if (doheartbeat)
		msg_newturn();

	    // Allow the other thread a chance to redraw.
	    // Possible we might want a delay here?

	    // Restore the wait state that we turned off for the force
	    // action pass...
	    if (possiblystop)
		avatar->startWaiting();

	    updateMap(queue().isEmpty());
	    avatar = 0;
	    if (myMap)
		avatar = myMap->avatar();

	    if (possiblystop && avatar)
		avatar->stopWaiting(0);

	    cmd = queue().waitAndRemove();

	    doheartbeat = false;
	}

	if (avatar && possiblystop && 
	    (cmd.action() == ACTION_MARK_QUEUE_EMPTY ||
	     cmd.action() == ACTION_GHOSTMOVEABS ||
	     cmd.action() == ACTION_NONE))
	{
	    // Resume.
	    avatar->startWaiting();
	    timeused = true;
	}

	switch (cmd.action())
	{
	    case ACTION_MARK_QUEUE_EMPTY:
		// Regardless if the queue is now empty, we mark it.
		myEngineQueueEmpty.append(1);
		break;
	    case ACTION_WAIT:
		// The dead are allowed to wait!
		if (avatar)
		    avatar->stopRunning();
		timeused = true;
		break;

	    case ACTION_RESTART:
	    {
		enableQuestions(false);
		int		loaded = 1;
		if (myMap)
		    myMap->decRef();

		myMap = MAP::load();

		if (!myMap)
		{
		    loaded = 0;
		    ITEM::initSystem();
		    MOB::initSystem();
		    avatar = MOB::createHero(HERO_AVATAR);
		    {
			// Start with a wish stone.
			ITEM *wish = ITEM::create(ITEM_WISH_STONE, 1);
			avatar->addItem(wish);
		    }

		    myMap = new MAP(cmd.dx(), avatar, myDisplay);
		}

#if 0
		// Force lots of loads
		while (1)
		{
		    MAP		*temp;
		    avatar = MOB::create(MOB_AVATAR);
		    temp = new MAP(cmd.dx(), avatar, myDisplay);
		    temp->incRef();
		    temp->decRef();
		}
#endif

		myMap->incRef();
		myMap->setDisplay(myDisplay);
		myMap->rebuildFOV();
		myMap->cacheItemPos();

		// Flag we've rebuilt.
		myRebuildQueue.append(loaded);
		enableQuestions(true);
		break;
	    }

	    case ACTION_SAVE:
	    {
		// Only save good games
		if (myMap && avatar && avatar->alive())
		{
		    myMap->save();
		}
		mySaveQueue.append(0);
		break;
	    }

	    case ACTION_SHUTDOWN:
	    {
		// Only save good games
		mySaveQueue.append(0);
		break;
	    }

	    case ACTION_REBOOTAVATAR:
	    {
		if (avatar)
		    avatar->gainHP(avatar->getMaxHP());
		break;
	    }

	    case ACTION_CLIMB:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionClimb();
		if (!timeused)
		    msg_newturn();
		break;
	    }

	    case ACTION_BUMP:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionBump(cmd.dx(), cmd.dy());
		if (!timeused)
		    msg_newturn();

		break;
	    }
	
	    case ACTION_DROP:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionDrop(avatar->getItemFromNo(cmd.dx()));
		if (!timeused)
		    msg_newturn();
		break;
	    }
	
	    case ACTION_WEARTOP:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionWearTop();
		if (!timeused)
		    msg_newturn();
		break;
	    }
	
	    case ACTION_EATTOP:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionEatTop();
		if (!timeused)
		    msg_newturn();
		break;
	    }
	
	    case ACTION_DROPTOP:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionDropTop();
		if (!timeused)
		    msg_newturn();
		break;
	    }
	
	    case ACTION_DROPALL:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionDropAll();
		if (!timeused)
		    msg_newturn();
		break;
	    }

	    case ACTION_BAGSHAKE:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionBagShake();
		if (!timeused)
		    msg_newturn();
		break;
	    }
	
	    case ACTION_BAGSWAPTOP:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionBagSwapTop();
		if (!timeused)
		    msg_newturn();
		break;
	    }
	
	    case ACTION_BREAKTOP:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionBreakTop();
		if (!timeused)
		    msg_newturn();
		break;
	    }

	    case ACTION_BREAK:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionBreak(avatar->getItemFromNo(cmd.dx()));
		if (!timeused)
		    msg_newturn();
		break;
	    }
	
	    case ACTION_WEAR:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionWear(avatar->getItemFromNo(cmd.dx()));
		if (!timeused)
		    msg_newturn();
		break;
	    }
	
	    case ACTION_EAT:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionEat(avatar->getItemFromNo(cmd.dx()));
		if (!timeused)
		    msg_newturn();
		break;
	    }
	
	    case ACTION_WAITUNTIL:
	    {
		VERIFY_ALIVE()
		if (avatar)
		{
		    timeused = true;
		    // avatar->formatAndReport("%S <wait> for the battle to unfold...");
		    avatar->startWaiting();
		}
		break;
	    }

	    case ACTION_SWITCHLEADER:
	    {
		VERIFY_ALIVE()
		if (avatar)
		{
		    timeused = avatar->actionSetLeader(cmd.dx());
		}
		if (!timeused)
		    msg_newturn();
		break;
	    }

	
	    case ACTION_STOPWAITING:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    avatar->stopWaiting(0);
		break;
	    }

	    case ACTION_MEDITATE:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionMeditate();
		if (!timeused)
		    msg_newturn();
		break;
	    }
	
	    case ACTION_PICKUP:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionPickup();
		if (!timeused)
		    msg_newturn();
		break;
	    }
	
	    case ACTION_SEARCH:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionSearch();
		if (!timeused)
		    msg_newturn();
		break;
	    }

	    case ACTION_CREATEITEM:
	    {
		VERIFY_ALIVE()
		if (avatar)
		{
		    ITEM	*item = ITEM::create((ITEM_NAMES) cmd.dx());
		    item->setMaterial((MATERIAL_NAMES) cmd.dy());
		    item->setStackCount(10);
		    avatar->addItem(item);
		}
		break;
	    }
	
	    case ACTION_QUAFFTOP:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionQuaffTop();
		if (!timeused)
		    msg_newturn();
		break;
	    }
	
	    case ACTION_QUAFF:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionQuaff(avatar->getItemFromNo(cmd.dx()));
		if (!timeused)
		    msg_newturn();
		break;
	    }
	
	    case ACTION_READ:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionRead(avatar->getItemFromNo(cmd.dx()));
		if (!timeused)
		    msg_newturn();
		break;
	    }
	
	    case ACTION_TRANSMUTE:
	    {
		VERIFY_ALIVE()
		if (avatar)
		{
		    if (cmd.dx() >= 0)
			timeused = avatar->actionTransmute(avatar->getItemFromNo(cmd.dx()));
		    else
			timeused = avatar->actionTransmute();
		}
		if (!timeused)
		    msg_newturn();
		break;
	    }

	    case ACTION_THROW:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionThrow(avatar->getItemFromNo(cmd.dz()),
					cmd.dx(), cmd.dy());
		if (!timeused)
		    msg_newturn();
		break;
	    }

	    case ACTION_THROWTOP:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionThrowTop(cmd.dx(), cmd.dy());
		if (!timeused)
		    msg_newturn();
		break;
	    }

	    case ACTION_CAST:
	    {
		VERIFY_ALIVE()
		if (avatar)
		    timeused = avatar->actionCast((SPELL_NAMES)cmd.dz(),
					cmd.dx(), cmd.dy());
		if (!timeused)
		    msg_newturn();
		break;
	    }
	
	    case ACTION_ROTATE:
	    {
		if (avatar)
		    timeused = avatar->actionRotate(cmd.dx());
		break;
	    }

	    case ACTION_FIRE:
	    {
		VERIFY_ALIVE()
		if (avatar)
		{
		    timeused = avatar->actionFire(cmd.dx(), cmd.dy());
		}
		if (!timeused)
		    msg_newturn();
		break;
	    }

	    case ACTION_SUICIDE:
	    {
		if (avatar)
		{
		    if (avatar->alive())
		    {
			// We want the flame to die.
			timeused = avatar->actionSuicide();
		    }
		}
		break;
	    }

	    case ACTION_GHOSTMOVE:
	    {
		if (avatar)
		{
		    avatar->meditateMove(avatar->meditatePos().delta(cmd.dx(), cmd.dy()), true);
		    myMap->rebuildFOV();

		    timeused = false;
		}
		break;
	    }
	    case ACTION_GHOSTMOVEABS:
	    {
		if (avatar)
		{
		}
		break;
	    }
	    case ACTION_GHOSTRECENTER:
	    {
		if (avatar && myMap)
		{
		}
		break;
	    }
	}

	if (myMap && timeused)
	{
	    avatar = myMap->avatar();

	    myQuestionTimeOut--;
	    if (myQuestionTimeOut < 0)
		myQuestionTimeOut = 0;

	    if (cmd.action() != ACTION_SEARCH &&
		cmd.action() != ACTION_NONE)
	    {
		// Depower searching
		if (avatar)
		    avatar->setSearchPower(0);
	    }

	    // We need to build the FOV for the monsters as they
	    // rely on the updated FOV to track, etc.
	    // Rebuild the FOV map
	    // Don't do this if no avatar, or the avatar is dead,
	    // as we want the old fov.
	    // We do want to do this if we are wiating as that is how
	    // we can find out about monsters and stop waiting!
	    if (avatar && avatar->alive())
		myMap->rebuildFOV();

	    // Update the world.
	    myMap->doMoveNPC();
	    myMap->doHeartbeat();

	    avatar = myMap->avatar();

	    // Rebuild the FOV map
	    if (avatar && avatar->alive())
	    {
		// Track the body!
		if (!avatar->isMeditateDecoupled())
		{
		    avatar->meditateMove(avatar->pos(), false);
		}
	
		myMap->rebuildFOV();
	    }

	    doheartbeat = true;
	}

	// Allow the other thread a chance to redraw.
	// Possible we might want a delay here?
	updateMap(false);
    }
}
