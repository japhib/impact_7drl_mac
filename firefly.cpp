/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	Smart Kobold Development
 *
 * NAME:        firefly.cpp ( Jacob's Matrix, C++ )
 *
 * COMMENTS:
 *		Runs a fire simulation on another thread.
 *		Allows you to get pure info dumps of the state.
 */

#include <libtcod.hpp>
#undef CLAMP
#include <math.h>
#include <assert.h>

#include "firefly.h"

#include "rand.h"

#define BOX_DEPTH 5
#define DEFSPEED 30
#define MAXSPEED 60

#define STATE_ALIVE 0
#define STATE_DYING 1
#define STATE_DEAD 2


FIRETEX::FIRETEX(int w, int h)
{
    myW = w;
    myH = h;
    myR = myG = myB = 196;
    myRGB = new u8[w * h * 3];
    memset(myRGB, 0, w * h * 3);
    myRefCnt.set(0);
}

FIRETEX::~FIRETEX()
{
    delete [] myRGB;
}

void
FIRETEX::decRef()
{
    if (myRefCnt.add(-1) <= 0)
	delete this;
}

void
FIRETEX::incRef()
{
    myRefCnt.add(1);
}

void
FIRETEX::redraw(int sx, int sy) const
{
    int		x, y;
    FORALL_XY(x, y)
    {
	TCODConsole::root->setCharBackground(x+sx, y+sy, TCODColor(getR(x,y), getG(x,y), getB(x,y)));
	// If we are bright, invert!
	if ( (getR(x,y) | getG(x,y) | getB(x,y)) > 128)
	{
	    TCODConsole::root->setCharForeground(x+sx, y+sy, TCODColor(0,0,0));
	}
    }
}

void
FIRETEX::firecurve(FireCurve firetype, float heat,
		    u8 *rgb)
{
    float	r, g, b;

    switch (firetype)
    {
	case FIRE_HEALTH:
	    r = heat * 64 * 4 + 128;
	    g = (heat - 1/3.0f) * 255 * 2;
	    b = (heat - 2/3.0f) * 255 * 1;
	    if (heat < 0.0)
	    {
		r = g = b = 0;
	    }
	    break;
	case FIRE_BLACKBODY:
	    r = heat * 255 * 3;
	    g = (heat - 1/3.0f) * 255 * 2;
	    b = (heat - 2/3.0f) * 255 * 1;
	    break;
	case FIRE_ICE:
	    b = heat * 128 * 3 + 128;
	    g = (heat - 1/3.0f) * 255 * 2;
	    r = (heat - 2/3.0f) * 255 * 1;
	    break;
	case FIRE_MANA:
	    b = heat * 255 * 3;
	    g = (heat - 1/3.0f) * 255 * 2;
	    r = (heat - 2/3.0f) * 255 * 1;
	    if (heat < 0.0)
	    {
		r = g = b = 0;
	    }
	    break;
	case FIRE_GOLD:
	    r = heat * 64 * 4 + 128;
	    g = heat * 64 * 4 + 128;
	    b = (heat - 1/3.0f) * 255 * 1;
	    break;
	case FIRE_POISON:
	    r = (heat - 1/3.0f) * 255 * 1;
	    g = heat * 64 * 4 + 128;
	    b = (heat - 1/3.0f) * 255 * 1;
	    if (heat < 0.0)
	    {
		r = g = b = 0;
	    }
	    break;
	case FIRE_MONO:
	    r = g = b = heat * 255;
	    break;
	case FIRE_FOOD:
	    b = 127 + 128 * heat;
	    g = 191 + 64 * heat;
	    r = 255;
	    if (heat < 0.0)
	    {
		r = g = b = 0;
	    }
	    break;
	case FIRE_COLOR:
	    b = myB-32 + 64 * heat;
	    g = myG-32 + 64 * heat;
	    r = myR-32 + 64 * heat;
	    if (heat < 0.0)
	    {
		r = g = b = 0;
	    }
	    break;
    }

    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;

    rgb[0] = (u8) r;
    rgb[1] = (u8) g;
    rgb[2] = (u8) b;
}

void
FIRETEX::buildFromConstant(bool horizontal, float cutoff, FireCurve firetype)
{
    int		x, y;
    int		ycut = (int)(height() - height() * cutoff);
    float	heat;

    if (horizontal)
    {
	int		xcut = (int)(width() - width() * cutoff);
	for (x = 0; x < width(); x++)
	{
	    heat = x / (float)width();
	    heat *= 1.5;

	    u8		rgb[3];

	    if (x < xcut)
		rgb[0] = rgb[1] = rgb[2] = 0;
	    else
		firecurve(firetype, heat, rgb);

	    for (y = 0; y < height(); y++)
	    {
		setRGB(x, y, rgb[0], rgb[1], rgb[2]);
	    }
	}
    }
    else
    {
	for (y = 0; y < height(); y++)
	{
	    heat = y / (float)height();
	    heat *= 1.5;

	    u8		rgb[3];

	    if (y < ycut)
		rgb[0] = rgb[1] = rgb[2] = 0;
	    else
		firecurve(firetype, heat, rgb);

	    for (x = 0; x < width(); x++)
	    {
		setRGB(x, y, rgb[0], rgb[1], rgb[2]);
	    }
	}
    }
}

void
FIRETEX::buildFromParticles(const PARTICLELIST &particles, FireCurve firetype, FireCurve bloodtype, int bloodcount)
{
    int		x, y;
    float	heat;

    FORALL_XY(x, y)
    {
	// Clear to black.
	setRGB(x, y, 0, 0, 0);
    }

    for (int i = 0; i < particles.entries(); i++)
    {
	x = (int)particles(i).myP.x();
	y = (int)particles(i).myP.y();
	heat = particles(i).myP.z();

	if (x >= 0 && x < width() &&
	    y >= 0 && y < height())
	{
	    u8		rgb[3];

	    // Modulate according to the particle id.
	    float		bright = (rand_wanginthash(particles(i).myId) & 255) / 255.F;

	    if (i < bloodcount)
		firecurve(bloodtype, bright, rgb);
	    else
		firecurve(firetype, bright, rgb);
	    setRGB(x, y, rgb[0], rgb[1], rgb[2]);
	}
    }
}

static void *
fire_threadstarter(void *vdata)
{
    FIREFLY	*fire = (FIREFLY *) vdata;

    fire->mainLoop();

    return 0;
}


FIREFLY::FIREFLY(int count, int tw, int th, FireCurve firetype, bool horizontal)
{
    myTexW = tw;
    myTexH = th;
    myFireType = firetype;
    myHorizontal = horizontal;

    myR = myG = myB = 196;

    myTex = new FIRETEX(myTexW, myTexH);
    myTex->setColor(myR, myG, myB);
    myTex->incRef();

    myRatioLiving = 1.0;
    myUseBarGraph = false;
    myBloodCoins = 0;

    for (int n = 0; n < 3; n++)
    {
	myNoise[n] = new TCODNoise(3, 0.5, 2.0);

	// Force the noise to init in case it does JIT as we must be on
	// main thread to use its RNG>
	float	f[3] = { 0, 0 };
	myNoise[n]->getTurbulence(f, 4, TCOD_NOISE_WAVELET);
    }

    myThread = THREAD::alloc();
    myThread->start(fire_threadstarter, this);

    setParticleCount(count);
}

FIREFLY::~FIREFLY()
{
    myTex->decRef();

    for (int n = 0; n < 3; n++)
    {
	delete myNoise[n];
    }
}

void
FIREFLY::updateTex(FIRETEX *tex)
{
    AUTOLOCK	a(myTexLock);

    tex->incRef();
    myTex->decRef();
    myTex = tex;
}

FIRETEX *
FIREFLY::getTex()
{
    AUTOLOCK	a(myTexLock);

    myTex->incRef();

    return myTex;
}

void
FIREFLY::setRatioLiving(float size)
{
    // LEt us hope atomic :>
    myRatioLiving = size;
}

// Zany short circuit evaluation for the win!
#define FORALL_PARTICLES(ppart)		\
    for (int lcl_index = 0; lcl_index < myParticles.entries() && (ppart = myParticles.rawptr(lcl_index)); lcl_index++)

void
FIREFLY::addNewParticle()
{
    PARTICLE	part;

    static int 	uid = 0;

    // We all are mortal!
    part.myState = STATE_DYING;
    part.myP.x() = (float)(rand_double() * (width() -1));
    part.myP.y() = 0;
    part.myId = uid++;

    myParticles.append(part);
}

void
FIREFLY::removeParticle()
{
    float		maxy = -1;
    int			maxidx = -1, nchoice = 0; 

    for (int idx = 0; idx < myParticles.entries(); idx++)
    {
	if (myParticles(idx).myP.y() > maxy + 0.5)
	{
	    nchoice = 1;
	    maxidx = idx;
	    maxy = myParticles(idx).myP.y();
	}
	else if (myParticles(idx).myP.y() > maxy - 0.5)
	{
	    nchoice++;
	    if (!rand_choice(nchoice))
	    {
		maxidx = idx;
	    }
	}
    }

    if (maxidx >= 0)
	myParticles.removeAt(maxidx);
}

void
FIREFLY::mainLoop()
{
    int		lastms, ms;
    float	tinc, t;
    float	ratioliving;
    PARTICLE	*ppt;
    u8		*partcount;
    PARTICLELIST sortedlist;
    
    partcount = new u8[width()*height()];

    lastms = TCOD_sys_elapsed_milli();

    t = 0.0;

    while (1)
    {
	ms = TCOD_sys_elapsed_milli();

	// Clamp this at 50.
	if (ms - lastms < 20)
	{
	    if (ms - lastms < 15)
		TCOD_sys_sleep_milli(5);
	    else
		TCOD_sys_sleep_milli(1);
	    continue;
	}

	// Rebuild particle list if requested.
	if (myPendingCountPending)
	{
	    if (myPendingCount < myParticles.entries())
	    {
		removeParticle();
	    }
	    else if (myPendingCount > myParticles.entries())
	    {
		addNewParticle();
	    }

	    if (myPendingCount == myParticles.entries())
		myPendingCountPending = 0;
	}

	tinc = (ms - lastms) / 1000.0F;
	t += tinc;

	// Noise functions get crappy to far from 0.
	// This adds a "beat" of one minute.  Yeah!  That is why!
	if (t > 60) t -= 60;

	lastms = ms;

	// Read only once as unlocked..
	ratioliving = myRatioLiving;

	if (useBarGraph())
	{
	    if (myOldRatio != ratioliving)
	    {
		// Rebuild our texture...
		FIRETEX		*tex;
		tex = new FIRETEX(myTexW, myTexH);
		tex->setColor(myR, myG, myB);
		tex->buildFromConstant(horizontal(), ratioliving, myFireType);

		// Publish result.
		updateTex(tex);
		myOldRatio = ratioliving;
	    }
	}
	else
	{
	    // Advect the particles.
	    FORALL_PARTICLES(ppt)
	    {
		if (ppt->myState != STATE_DEAD)
		    ppt->myP += ppt->myV * tinc;
	    }

	    // Gravity on dying particles
	    FORALL_PARTICLES(ppt)
	    {
		if (ppt->myState == STATE_DYING)
		{
		    ppt->myV.y() += 20 * tinc;
		}
	    }

	    if (true)
	    {
		sortedlist.clear();

		// This is the last day, I'm not doing anything pretty here!
		FORALL_PARTICLES(ppt)
		{
		    if (ppt->myState != STATE_DEAD)
			continue;

		    PARTICLE	part = *ppt;
		    // Jitter in the y direction to jitter our sort
		    // behaviour!
		    part.myP.y() = float(int(part.myP.y()));
		    part.myP.y() += (float)(rand_double()*0.1F);
		    part.myState = lcl_index;
		    sortedlist.append(part);
		}
		sortedlist.stableSort();

		memset(partcount, 0, width()*height());
		int			 id;

		int			partperbucket;

		partperbucket = 1;

		for (int i = sortedlist.entries(); i-->0; )
		{
		    id = sortedlist(i).myState;

		    int	x = (int)(sortedlist(i).myP.x());
		    int	y = (int)(sortedlist(i).myP.y());
		    x = BOUND(x, 0, width()-1);
		    y = BOUND(y, 0, height()-1);

		    while (y > 0 && (partcount[x+y*width()] >= partperbucket))
		    {
			// Bubble up
			myParticles(id).myP.y() -= 1;
			y--;
		    }

		    if (y < 0)
		    {
			// We filled up!
			myParticles(id).myP.y() = 0;
			continue;
		    }

		    // Bubble down!
		    while (y < height()-1)
		    {
			if (partcount[x+(y+1)*width()] < partperbucket)
			{
			    // Fall straight.
			    y++;
			    myParticles(id).myP.y() += 1;
			    continue;
			}
			int		dx = 0;
			int		nfound = 0;
			if ((x < width()-1) 
			    && (partcount[x+1+(y+1)*width()] < partperbucket))
			{
			    // Down right
			    dx = 1;
			    nfound = 1;
			}
			if ((x > 0)
			    && (partcount[x-1+(y+1)*width()] < partperbucket))
			{
			    // Down left
			    if (!nfound || !rand_choice(2))
			    {
				dx = -1;
				nfound++;
			    }
			}

			if (nfound)
			{
			    y++;
			    x += dx;
			    myParticles(id).myP.x() += dx;
			    myParticles(id).myP.y() += 1;
			}
			else
			{
			    // Hit supporting structure
			    break;
			}
		    }

		    // Write out our new particle
		    partcount[x + y * width()]++;
		    if (partcount[x+y*width()] > partperbucket)
			partcount[x + y *width()] = partperbucket;
		}

		// Kill any dying particles that have hit a dead particle
		// Bounce any living particles out.
		FORALL_PARTICLES(ppt)
		{
		    int	x = (int)(ppt->myP.x());
		    int	y = (int)(ppt->myP.y());


		    if (ppt->myState == STATE_DYING)
		    {
			if (y > height()-1)
			{
			    ppt->myState = STATE_DEAD;
			    ppt->myP.y() = (float)(height()-1);
			}

			x = BOUND(x, 0, width()-1);
			y = BOUND(y, 0, height()-1);
			if (partcount[x + y * width()])
			    ppt->myState = STATE_DEAD;
		    }
		}
	    }

	    // Rebuild our texture...
	    FIRETEX		*tex;
	    tex = new FIRETEX(myTexW, myTexH);
	    tex->setColor(myR, myG, myB);
	    tex->buildFromParticles(myParticles, myFireType, FIRE_HEALTH, myBloodCoins);

	    // Publish result.
	    updateTex(tex);
	}
    }

    // HAHA!  As if this is ever reached.
    delete [] partcount;
}
