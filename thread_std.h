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

#ifndef __threadstd_h__
#define __threadstd_h__

#include "mygba.h"
#include "thread.h"
#include <thread>

class THREAD_STD : public THREAD
{
public:
    // (Re)starts this, invoking the given function as the main 
    // thread program.
    virtual void	 start(THREADmainFunc func, void *data);

    // Blocks until this is done
    virtual void	 join();

    // Terminates this thread.
    virtual void	 kill();
    
			 THREAD_STD();
    virtual 		~THREAD_STD();
protected:
    // Blocks until the thread is ready to process.
    virtual void	 waittillimready();
    virtual void	 iamdonenow();
    virtual void	 jobsready();

    std::unique_ptr<std::thread>	mySelf;
};

#endif

