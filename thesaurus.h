/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	Everything Is Fodder Development
 *
 * NAME:        thesaurus.h ( Everything Is Fodder, C++ )
 *
 * COMMENTS:
 */

#ifndef __thesaurus__
#define __thesaurus__

#include "glbdef.h"

#include <iostream>
using namespace std;

void thesaurus_init();

BUF thesaurus_expand(int seed, MOB_NAMES mob);
BUF thesaurus_expand(RANDSEQ &seq, MOB_NAMES mob);
BUF thesaurus_expand(int seed, MATERIAL_NAMES material);
BUF thesaurus_expand(RANDSEQ &seq, MATERIAL_NAMES material);
BUF thesaurus_expand(int seed, ITEM_NAMES item, MATERIAL_NAMES material, MATERIAL_NAMES gilt);
BUF thesaurus_expand(int seed, ROOMTYPE_NAMES room);
BUF thesaurus_expand(RANDSEQ &seq, ROOMTYPE_NAMES room);

#endif
