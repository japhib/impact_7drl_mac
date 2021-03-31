/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        text.cpp ( Live Once Library, C++ )
 *
 * COMMENTS:
 */

#include "mygba.h"
#include "text.h"

#include <fstream>
using namespace std;

#include "ptrlist.h"
#include "grammar.h"
#include "rand.h"
#include "thread.h"

PTRLIST<char *> glbTextEntry;
PTRLIST<char *> glbTextKey;

PTRLIST<char *> glbNameList;

PTRLIST<char *>	glbVariableKeys;
PTRLIST<char *>	glbVariableValues;

LOCK		glbTextLock;

BUF
text_lookupvariableCB(const char *variable, void *data)
{
    AUTOLOCK	a(glbTextLock);
    BUF		buf;
    buf.strcpy(text_lookupvariable(variable));
    return buf;
}

void
text_registervariable(const char *variable, const char *value)
{
    AUTOLOCK	a(glbTextLock);
    // First see if we have the key..
    int		key;
    for (key = 0; key < glbVariableKeys.entries(); key++)
    {
	if (!strcmp(variable, glbVariableKeys(key)))
	{
	    break;
	}
    }
    if (key >= glbVariableKeys.entries())
    {
	glbVariableKeys.append(MYstrdup(variable));
	glbVariableValues.append(MYstrdup(""));
    }

    free(glbVariableValues(key));
    glbVariableValues(key) = MYstrdup(value);
}

const char *
text_lookupvariable(const char *variable)
{
    AUTOLOCK	a(glbTextLock);
    for (int key = 0; key < glbVariableKeys.entries(); key++)
    {
	if (!strcmp(variable, glbVariableKeys(key)))
	{
	    return glbVariableValues(key);
	}
    }

    return "";
}

char *
text_append(char *oldtxt, const char *append)
{
    int		len;
    char	*txt;
    
    len = (int)MYstrlen(oldtxt) + (int)MYstrlen(append) + 5;
    txt = (char *) malloc(len);
    strcpy(txt, oldtxt);
    strcat(txt, append);
    free(oldtxt);
    return txt;
}

void
text_striplf(char *line)
{
    while (*line)
    {
	if (*line == '\n' || *line == '\r')
	{
	    *line = '\0';
	    return;
	}
	line++;
    }
}

bool
text_hasnonws(const char *line)
{
    while (*line)
    {
	if (!ISSPACE(*line))
	    return true;
	line++;
    }
    return false;
}

int
text_firstnonws(const char *line)
{
    int		i = 0;
    while (line[i])
    {
	if (!ISSPACE(line[i]))
	    return i;
	i++;
    }
    return i;
}

int
text_lastnonws(const char *line)
{
    int		i = 0;
    while (line[i])
	i++;

    while (i > 0)
    {
	if (line[i] && !ISSPACE(line[i]))
	    return i;
	i--;
    }
    return i;
}

static void
text_names_init()
{
    ifstream	is("../names.txt");
    char	line[500];
    bool	hasline = false;
    
    while (hasline || is.getline(line, 500))
    {
	text_striplf(line);
	hasline = false;

	// Ignore comments.
	if (line[0] == '#')
	    continue;

	// See if an entry...
	if (!ISSPACE(line[0]))
	{
	    // This line is a name.
	    glbNameList.append(MYstrdup(line));
	}
    }
}

void
text_init()
{
    text_names_init();

    ifstream	is("../text.txt");
    char	line[500];
    bool	hasline = false;
    char	*text;

    while (hasline || is.getline(line, 500))
    {
	text_striplf(line);
	hasline = false;

	// Ignore comments.
	if (line[0] == '#')
	    continue;

	// See if an entry...
	if (!ISSPACE(line[0]))
	{
	    // This line is a key.
	    glbTextKey.append(MYstrdup(line));
	    // Rest is the message...
	    text = MYstrdup("");
	    while (is.getline(line, 500))
	    {
		text_striplf(line);
		int		firstnonws;

		for (firstnonws = 0; line[firstnonws] && ISSPACE(line[firstnonws]); firstnonws++);

		if (!line[firstnonws])
		{
		    // Completely blank line - insert a hard return!
		    text = text_append(text, "\n\n");
		}
		else if (!firstnonws)
		{
		    // New dictionary entry, break out!
		    hasline = true;
		    break;
		}
		else
		{
		    // Allow some ascii art...
		    if (firstnonws > 4)
		    {
			text = text_append(text, "\n");
			firstnonws = 4;
		    }

		    // Append remainder.
		    // Determine if last char was end of sentence.
		    if (*text && text[MYstrlen(text)-1] != '\n')
		    {
			if (gram_isendsentence(text[MYstrlen(text)-1]))
			    text = text_append(text, " ");
			text = text_append(text, " ");
		    }

		    text = text_append(text, &line[firstnonws]);
		}
	    }

	    // Append the resulting text.
	    glbTextEntry.append(text);
	}
    }
}



void
text_shutdown()
{
    int		i;

    for (i = 0; i < glbTextKey.entries(); i++)
    {
	free(glbTextKey(i));
	free(glbTextEntry(i));
    }
    glbTextKey.clear();
    glbTextEntry.clear();

    for (i = 0; i < glbNameList.entries(); i++)
	free(glbNameList(i));
    glbNameList.clear();
}

bool
text_hasentry(const char *key)
{
    for (int i = 0; i < glbTextKey.entries(); i++)
    {
	if (!strcmp(key, glbTextKey(i)))
	{
	    return true;
	}
    }
    return false;
}

bool
text_hasentry(const char *dict, const char *word)
{
    BUF		key;
    key.sprintf("%s::%s", dict, word);
    return text_hasentry(key);
}

BUF
text_lookup(const char *key)
{
    BUF		 buf;
    int		i;

    for (i = 0; i < glbTextKey.entries(); i++)
    {
	if (!strcmp(key, glbTextKey(i)))
	{
	    buf.reference(glbTextEntry(i));
	    buf.evaluateVariables(text_lookupvariableCB, 0);
	    return buf;
	}
    }

    buf.sprintf("Missing text entry: \"%s\".\n", key);

    return buf;
}

BUF
text_lookup(const char *dict, const char *word)
{
    BUF		buf;

    buf.sprintf("%s::%s", dict, word);
    return text_lookup(buf);
}

BUF
text_lookup(const char *dict, const char *word, const char *subword)
{
    BUF		buf;

    buf.sprintf("%s::%s::%s", dict, word, subword);
    return text_lookup(buf);
}

const char *
text_getname(int idx)
{
    if (idx < 0 || idx >= glbNameList.entries())
	return "invalid";
    return glbNameList(idx);
}

void
text_shufflenames()
{
    glbNameList.shuffle();
}
