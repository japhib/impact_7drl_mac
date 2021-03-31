/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	Jacob's Matrix Development
 *
 * NAME:        engine.h ( Jacob's Matrix, C++ )
 *
 * COMMENTS:
 *	Our game engine.  Grabs commands from its command
 *	queue and processes them as fast as possible.  Provides
 *	a mutexed method to get a recent map from other threads.
 *	Note that this will run on its own thread!
 */

#ifndef __engine__
#define __engine__

#include "command.h"
#include "thread.h"
#include "display.h"
#include "buf.h"
#include "text.h"

class THREAD;
class MAP;
class DISPLAY;
class MOB;

class ENGINE
{
public:
    ENGINE(DISPLAY *display);
    ~ENGINE();

    // These methods are thread safe.
    CMD_QUEUE		&queue() { return myQueue; }
    // Returns a map with the reference copy incremented, you must
    // decRef when done!
    MAP			*copyMap();

    // Public only for call back conveninece.
    void		 mainLoop();

    // Waits for the map to finish building, invoke after
    // a RESTART command.
    // Returns true if loaded from disk
    bool		 awaitRebuild(bool doredraws=true);

    // Waits for the map to finish saving after ACTION_SAVE
    void		 awaitSave(bool doredraws=true);

    // Waits for the engine queue to go empty.
    void		 awaitQueueEmpty();

    // Returns the next requested popup notifier.
    // Returns empty string if none.
    BUF			 getNextPopup();

    // Call from any thread, triggers a delayed popup
    void		 popupText(const char *text);
    void		 popupText(BUF buf) { popupText(buf.buffer()); }

    void		 fadeFromWhite() { myDisplay->fadeFromWhite(); }


    enum	QuestionType
    {
	QUERY_INVALID,
	QUERY_TEXT,
	QUERY_CHOICE,
    };
    class	Question
    {
    public:
	Question()
	{
	    type = QUERY_INVALID;
	    defaultchoice = 0;
	    payload = -1;
	    mob = 0;
	    item = 0;
	}

	void		setChoices(const char **c)
	{
	    choices.clear();
	    for (int i = 0; c[i]; i++)
	    {
		BUF	tmp;
		tmp.strcpy(c[i]);
		choices.append(tmp);
	    }
	}
	QuestionType	type;
	BUF		query;
	PTRLIST<BUF>	choices;
	int		defaultchoice;
	int		payload;
	MOB		*mob;
	ITEM		*item;
    };

    class	Response
    {
    public:
	Response()
	{
	    type = QUERY_INVALID;
	}
	QuestionType	type;
	BUF		answer;
	int		choice;
    };

    bool		 questionsEnabled() const { return myEnabledQuestions; }
    void		 enableQuestions(bool allowed) { myEnabledQuestions = allowed; }

    bool		 timeForQuestions() const
    { return myQuestionTimeOut == 0; }

    // From the engine thread, asks a question to the user.
    // Blocks until you get an anwer.
    Response		 askQuestion(Question query, bool isclient);

    BUF			 askQuestionText(BUF text, bool isclient);
    BUF			 askQuestionTextCanned(const char *dict, const char *key, bool isclient);
    bool		 askQuestionNum(BUF text, int &result, bool isclient);
    bool		 askQuestionNumCanned(const char *dict, const char *key, int &result, bool isclient)
    { return askQuestionNum(text_lookup(dict, key), result, isclient); }
    int			 askQuestionList(BUF text, const PTRLIST<BUF> &choices, int defchoice, bool isclient);
    int			 askQuestionListCanned(const char *dict, const char *key, const PTRLIST<BUF> &choices, int defchoice, bool isclient)
    { return askQuestionList(text_lookup(dict, key), choices, defchoice, isclient); }
    // This takes a null terminated list of strings.
    int			 askQuestionList(BUF text, const char **choices, int defchoice, bool isclient);
    int			 askQuestionListCanned(const char *dict, const char *key, const char **choices, int defchoice, bool isclient)
    { return askQuestionList(text_lookup(dict, key), choices, defchoice, isclient); }

    // From the main thread, sees if there is a question pending.
    bool		 getQuestion(Question &query);
    // Immediately answer any pending questions.
    void		 answerAnyQuestions();
    // From the main thread, posts an answer to a question.
    void		 answerQuestion(Response response);

protected:
    /// Called when we are happy with myMap and want to publish it.
    void		 updateMap(bool forcepost);

private:
    MAP			*myMap;
    MAP			*myOldMap;
    LOCK		 myMapLock;
    CMD_QUEUE		 myQueue;
    DISPLAY		*myDisplay;

    QUEUE<int>		 myRebuildQueue;
    QUEUE<int>		 mySaveQueue;
    QUEUE<int>		 myEngineQueueEmpty;

    QUEUE<BUF>		 myPopupQueue;

    bool		 myEnabledQuestions;
    QUEUE<Question>	 myQuestionQueue;
    QUEUE<Response>	 myResponseQueue;

    THREAD		*myThread;

    int			 myQuestionTimeOut;
};

extern ENGINE *glbEngine;

ENGINE::Response engine_askQuestion(const ENGINE::Question &query);

#endif
