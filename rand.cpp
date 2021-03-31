/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        random.cpp ( Live Once Library, C++ )
 *
 * COMMENTS:
 *	This file is pre-7DRL
 */

#include "mygba.h"
#include "rand.h"

#include "mt19937ar.c"

int
genrandom()
{
    // We don't want a sign bit.
    return genrand_int31();
}

RANDSEQ::RANDSEQ()
{
    mySeedLow = genrand_int32();
    mySeedHigh = genrand_int32();
}

RANDSEQ::RANDSEQ(int seed)
{
    mySeedLow = seed;
    mySeedHigh = rand_wanginthash(seed);
}

int
RANDSEQ::nextrandom()
{
    int		result = rand_wanginthash(mySeedLow);
    
    // Slowly shuffle in the high seed, relying on avalanching
    // hopefully working.
    mySeedLow = result;
    u32 nbit = mySeedHigh & 3;
    u32 obit = mySeedLow & 3;
    mySeedLow >>= 2;
    mySeedHigh >>= 2;
    mySeedLow |= nbit << 30;
    mySeedHigh |= obit << 30;

    return ABS(result);
}

int
RANDSEQ::range(int min, int max)
{
    int		v;

    if (min > max)
	return range(max, min);
    
    v = choice(max - min + 1);
    return v + min;
}

int
RANDSEQ::choice(int num)
{
    int		v;

    // Always use a random number even if we don't want it now.
    v = nextrandom();

    // Choice of 0 or 1 is always 0.
    if (num < 2) return 0;

    v %= num;

    return v;
}

bool
RANDSEQ::chance(int percentage)
{
    int		percent;

    // Do not short circuit, all code paths must use a rand.
    percent = choice(100);
    // We want strict less than so percentage 0 will never pass,
    // and percentage 99 will pass only one in 100.
    return percent < percentage;
}

bool
RANDSEQ::chance_range(float value, float min, float max)
{
    int		percent;

    // Important all code paths use up a random number!
    if (value <= min)
	percent = 0;
    else if (value >= max)
	percent = 100;
    else
    {
	value = (value - min) / (max - min);
	percent = (int)(value * 100);
    }
    return chance(percent);
}

const char *
RANDSEQ::string(const char **stringlist)
{
    int		n;
    
    // Find length of the string list.
    for (n = 0; stringlist[n]; n++);

    return stringlist[choice(n)];
}

long
rand_getseed()
{
    long		seed;
    seed = genrandom();
    rand_setseed(seed);
    return seed;
}

void
rand_setseed(long seed)
{
    init_genrand((unsigned long) seed);
}

unsigned int
rand_wanginthash(unsigned int key)
{
    key += ~(key << 16);
    key ^=  (key >> 5);
    key +=  (key << 3);
    key ^=  (key >> 13);
    key += ~(key << 9);
    key ^=  (key >> 17);
    return key;
}


int
rand_range(int min, int max)
{
    int		v;

    if (min > max)
	return rand_range(max, min);
    
    v = rand_choice(max - min + 1);
    return v + min;
}

int
rand_choice(int num)
{
    int		v;

    // Choice of 0 or 1 is always 0.
    if (num < 2) return 0;

    v = genrandom();
    v %= num;

    return v;
}

const char *
rand_string(const char **stringlist)
{
    int		n;
    
    // Find length of the string list.
    for (n = 0; stringlist[n]; n++);

    return stringlist[rand_choice(n)];
}

int
rand_roll(int num, int reroll)
{
    int		max = 0, val;

    // -1, 0, and 1 all evaluate to themselves always.
    if (num >= -1 && num <= 1)
	return num;

    if (num < 0)
    {
	// Negative numbers just invoke this and reverse the results.
	// Note we can't just negate the result, as we want higher rerolls
	// to move it closer to -1, not closer to num!
	val = rand_roll(-num, reroll);
	val -= num + 1;
	return val;
    }

    if (reroll < 0)
    {
	// Negative rerolls means we want to reroll but pick the
	// smallest result.  This is the same as inverting our normal
	// roll distribution, so thus...
	val = rand_roll(num, -reroll);
	val = num + 1 - val;
	return val;
    }
    
    // I wasn't even drunk when I made reroll of 0 mean roll
    // once, and thus necissating this change.
    reroll++;
    while (reroll--)
    {
	val = rand_choice(num) + 1;
	if (val > max)
	    max = val;
    }

    return max;
}

int
rand_dice(int numdie, int sides, int bonus)
{
    int		i, total = bonus;

    for (i = 0; i < numdie; i++)
    {
	total += rand_choice(sides) + 1;
    }
    return total;
}

bool
rand_chance(int percentage)
{
    int		percent;

    percent = rand_choice(100);
    // We want strict less than so percentage 0 will never pass,
    // and percentage 99 will pass only one in 100.
    return percent < percentage;
}

int
rand_sign()
{
    return rand_choice(2) * 2 - 1;
}

void
rand_direction(int &dx, int &dy)
{
    if (rand_choice(2))
    {
	dx = rand_sign();
	dy = 0;
    }
    else
    {
	dx = 0;
	dy = rand_sign();
    }
}

void
rand_shuffle(u8 *set, int n)
{
    int		i, j;
    u8		tmp;

    for (i = n-1; i > 0; i--)
    {
	// Want to swap with anything earlier, including self!
	j = rand_choice(i+1);
	
	tmp = set[i];
	set[i] = set[j];
	set[j] = tmp;
    }
}

void
rand_shuffle(int *set, int n)
{
    int		i, j;
    int		tmp;

    for (i = n-1; i > 0; i--)
    {
	// Want to swap with anything earlier, including self!
	j = rand_choice(i+1);
	
	tmp = set[i];
	set[i] = set[j];
	set[j] = tmp;
    }
}

void
rand_getdirection(int dir, int &dx, int &dy)
{
    dir &= 3;
    switch (dir)
    {
	case 0:
	    dx = 0;
	    dy = 1;
	    break;

	case 1:
	    dx = 1;
	    dy = 0;
	    break;

	case 2:
	    dx = 0;
	    dy = -1;
	    break;

	case 3:
	    dx = -1;
	    dy = 0;
	    break;
    }
}

int
rand_dir4_toangle(int dx, int dy)
{
    if (dy > 0)
	return 0;
    if (dx > 0)
	return 1;
    if (dy < 0)
	return 2;
    return 3;
}

void
rand_angletodir(int angle, int &dx, int &dy)
{
    angle &= 7;

    int		dxtable[8] = { 1, 1, 0,-1,-1,-1, 0, 1 };
    int		dytable[8] = { 0, 1, 1, 1, 0,-1,-1,-1 };

    dx = dxtable[angle];
    dy = dytable[angle];
}

int
rand_dirtoangle(int dx, int dy)
{
    int		x, y, a;

    for (a = 0; a < 8; a++)
    {
	rand_angletodir(a, x, y);
	if (x == dx && y == dy)
	    return a;
    }

    // This is 0,0, so we just return any angle!
    return rand_range(0, 7);
}

const char *
rand_dirtoname(int dx, int dy)
{
    if (!dx && !dy)
	return "Self";
    return rand_angletoname(rand_dirtoangle(dx, dy));
}

const char *
rand_angletoname(int angle)
{
    const char *anglenames[] =
    {
	"East",
	"Southeast",
	"South",
	"Southwest",
	"West",
	"Northwest",
	"North",
	"Northeast"
    };
    angle &= 7;

    return anglenames[angle];
}

int
DICE::roll() const
{
    return rand_dice(myNumDie, mySides, myBonus);
}

DPDF
DICE::buildDPDF() const
{
    // Start with 100% chance of 0.
    DPDF		total(0);
    int			i;

    // Add in our bonus.
    total += myBonus;

    // Add in each die roll.
    for (i = 0; i < myNumDie; i++)
    {
	// Fuck yeah, this is the way to work with probability
	// functions.  No messy math here!  Just brute force and ignorance!
	DPDF		die(1, mySides);

	total += die;
    }
    
    return total;
}

double
rand_double()
{
    return genrand_res53();
}

int
rand_int()
{
    return genrand_int32();
}

// Written in a beautiful provincial park in a nice cool June day.
// If this were traditional manuscript, it would smell of woodsmoke,
// but the curse of digital is the destruction of all sidebands
// of history.  Which I guess makes comments like this all the
// more important.
//
// Hence copied into the 7DRL baseline from the POWDER one on a
// chilly afternoon in rural Japan.  The smell now is of an oil
// heater labouring to keep the room warm.
BUF
rand_name()
{
    // Very simple markov generator.
    // We repeat letters to make them more likely.
    const char *vowels = "aaaeeeiiiooouuyy'";
    const char *frictive = "rsfhvnmz";
    const char *plosive = "tpdgkbc";
    const char *weird = "qwjx";
    // State transitions..
    // v -> f, p, w, v'
    // v' -> f, p, w
    // f -> p', v
    // p -> v, f'
    // w, p', f' -> v

    BUF		buf;

    int		syllables = 0;
    char	state;
    bool	prime = false;

    // Initial state choice
    if (rand_chance(30))
	state = 'v';
    else if (rand_chance(40))
	state = 'f';
    else if (rand_chance(70))
	state = 'p';
    else
	state = 'w';

    while (1)
    {
	// Apply current state
	switch (state)
	{
	    case 'v':
		buf.append(vowels[rand_choice(MYstrlen(vowels))]);
		if (!prime)
		    syllables++;
		break;
	    case 'f':
		buf.append(frictive[rand_choice(MYstrlen(frictive))]);
		break;
	    case 'p':
		buf.append(plosive[rand_choice(MYstrlen(plosive))]);
		break;
	    case 'w':
		buf.append(weird[rand_choice(MYstrlen(weird))]);
		break;
	}

	// Chance to stop..
	if (syllables && buf.strlen() >= 3)
	{
	    if (rand_chance(20+buf.strlen()*4))
		break;
	}

	// Transition...
	switch (state)
	{
	    case 'v':
		if (!prime && rand_chance(10))
		{
		    state = 'v';
		    prime = true;
		    break;
		}
		else if (rand_chance(40))
		    state = 'f';
		else if (rand_chance(70))
		    state = 'p';
		else
		    state = 'w';
		prime = false;
		break;
	    case 'f':
		if (!prime && rand_chance(50))
		{
		    prime = true;
		    state = 'p';
		    break;
		}
		state = 'v';
		prime = false;
		break;
	    case 'p':
		if (!prime && rand_chance(10))
		{
		    prime = true;
		    state = 'f';
		    break;
		}
		state = 'v';
		prime = false;
		break;
	    case 'w':
		state = 'v';
		prime = false;
		break;
	}
    }
    buf.uniquify();
    buf.evildata()[0] = toupper(buf.buffer()[0]);

    return buf;
}
