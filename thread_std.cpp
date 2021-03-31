/*
 * Licensed under CLIFE license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	CLIFE Development
 *
 * NAME:        thread_std.h ( CLIFE, C++ )
 *
 * COMMENTS:
 *	Attempt at a threading class.
 */

#include "thread_std.h"

void
THREAD::yield()
{
    std::this_thread::yield();
}

THREAD_STD::THREAD_STD()
{
}

THREAD_STD::~THREAD_STD()
{
    join();
}

void
THREAD_STD::start(THREADmainFunc func, void *data)
{
    // If we are still running, block until the current task completes.
    if (mySelf)
	join();

    myCB = func;
    myCBData = data;
    mySelf = std::make_unique<std::thread>(THREAD::wrapper, (void *)this);
    jobsready();
}

void
THREAD_STD::jobsready()
{
    // Always ready, no pooling!
}

void
THREAD_STD::iamdonenow()
{
    // Always done, no pooling.
}

void
THREAD_STD::waittillimready()
{
    // Immediately ready.
}

void
THREAD_STD::join()
{
    if (mySelf)
    {
	mySelf->join();
	mySelf.reset();
    }
}

void
THREAD_STD::kill()
{
    if (mySelf)
    {
	// This isn't alllowed in std::thread for very good reasons
	// I agree with
	J_ASSERT(!"WTFBBQ?  Don't go killing things randomly!");
	join();
    }
}
