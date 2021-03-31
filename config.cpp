/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	Jacob's Matrix Development
 *
 * NAME:        config.cpp ( Jacob's Matrix, C++ )
 *
 * COMMENTS:
 */

#include <libtcod.hpp>
#undef CLAMP

#include "config.h"
#include "buf.h"

#include <fstream>
using namespace std;

CONFIG *glbConfig = 0;

CONFIG::CONFIG()
{
}

CONFIG::~CONFIG()
{
}

void
CONFIG::load(const char *fname)
{
    TCODParser		parser;
    TCODParserStruct	*flame, *music, *screen, *game, *bindings;

    flame = parser.newStructure("bullets");
    flame->addProperty("display", TCOD_TYPE_BOOL, true);

    game =  parser.newStructure("game");
    game->addProperty("easymode", TCOD_TYPE_BOOL, false);
    game->addProperty("debug", TCOD_TYPE_BOOL, false);
    game->addProperty("colorblind", TCOD_TYPE_BOOL, false);

    music = parser.newStructure("music");
    music->addProperty("enable", TCOD_TYPE_BOOL, true);
    music->addProperty("file", TCOD_TYPE_STRING, true);
    music->addProperty("volume", TCOD_TYPE_INT, true);

    screen = parser.newStructure("screen");
    screen->addProperty("full", TCOD_TYPE_BOOL, true);

    bindings = parser.newStructure("bindings");
    for (int i = 0; i < 12; i++)
    {
	BUF		fname;
	fname.sprintf("F%d", i+1);
	bindings->addProperty(fname.buffer(), TCOD_TYPE_INT, false);
    }

    parser.run(fname, 0);

    myBulletsDisplay = parser.getBoolProperty("bullets.display");
    myEasyMode = parser.getBoolProperty("game.easymode");
    myDebug = parser.getBoolProperty("game.debug");
    myColorBlind = parser.getBoolProperty("game.colorblind");

    myMusicEnable = parser.getBoolProperty("music.enable");
    myMusicFile = parser.getStringProperty("music.file");
    myMusicVolume = parser.getIntProperty("music.volume");

    myFullScreen = parser.getBoolProperty("screen.full");

    for (int i = 0; i < 12; i++)
    {
	BUF		fname;
	fname.sprintf("bindings.F%d", i+1);
	myBindings[i] = parser.getIntProperty(fname.buffer());
    }
}

void
CONFIG::save(const char *fname)
{
    // So TCOD has a parser that does thet hard part,
    // of trying to read in a file.

    // But it has no way to write out a changed version?

    // Argh!
    ofstream		os(fname);

    os << "bullets" << endl;
    os << "{" << endl;
    os << "    display = " << (myBulletsDisplay ? "true" : "false") << endl;
    os << "}" << endl;
    os << endl;

    os << "game" << endl;
    os << "{" << endl;
    os << "    easymode = " << (myEasyMode ? "true" : "false") << endl;
    os << "    debug = " << (myDebug ? "true" : "false") << endl;
    os << "    colorblind = " << (myColorBlind ? "true" : "false") << endl;
    os << "}" << endl;
    os << endl;

    os << "music" << endl;
    os << "{" << endl;
    os << "    enable = " << (myMusicEnable ? "true" : "false") << endl;
    // Heaven help you if you have " in your file as we don't escape here.
    os << "    file = \"" << myMusicFile << "\"" << endl;
    os << "    volume = " << myMusicVolume << endl;
    os << "}" << endl;
    os << endl;

    os << "screen" << endl;
    os << "{" << endl;
    os << "    full = " << (myFullScreen ? "true" : "false") << endl;
    os << "}" << endl;
    os << endl;

    os << "bindings" << endl;
    os << "{" << endl;
    for (int i = 0; i < 12; i++)
    {
	BUF		fname;
	fname.sprintf("F%d", i+1);
	os << "    " << fname.buffer() << " = " << myBindings[i] << endl;
    }
    os << "}" << endl;
    os << endl;
}
