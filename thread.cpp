/*
 * Licensed under CLIFE license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	CLIFE Development
 *
 * NAME:        thread.cpp ( CLIFE, C++ )
 *
 * COMMENTS:
 *	Attempt at a threading class.
 */

#include "thread.h"

#ifdef STD_THREAD
#include "thread_std.h"
#else
#ifdef LINUX
#include "thread_linux.h"
#include <sys/sysinfo.h>
#else
#include "thread_win.h"
#endif
#endif

THREAD *
THREAD::alloc()
{
#ifdef STD_THREAD
    return new THREAD_STD();
#else
#ifdef LINUX
    return new THREAD_LINUX();
#else
    return new THREAD_WIN();
#endif
#endif
}

int
THREAD::numProcessors()
{
    static int nproc = -1;

    if (nproc < 0)
    {
#ifdef STD_THREAD
	nproc = std::thread::hardware_concurrency();
#else
#ifdef LINUX
	nproc = get_nprocs_conf();
#else
	SYSTEM_INFO		sysinfo;
	GetSystemInfo(&sysinfo);
	nproc = sysinfo.dwNumberOfProcessors;
#endif
#endif

	// Zany users have managed to muck with thier registery to
	// get 0 procs returned...
	if (nproc < 1)
	    nproc = 1;
    }

    return nproc;
}

void *
THREAD::wrapper(void *data)
{
    THREAD *thread = (THREAD *) data;

    while (1)
    {
	// Await the thread to be ready.
	thread->waittillimready();

	thread->setActive(true);

	// Hurray!  Trigger callback.
	if (thread->myCB)
	    thread->myCB(thread->myCBData);

	thread->iamdonenow();

	thread->setActive(false);
    }

    return (void *)1;
}

LOCK::LOCK()
{
#ifdef STD_THREAD
#else
#ifdef LINUX
    pthread_mutexattr_init(&myLockAttr);
    pthread_mutexattr_settype(&myLockAttr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init(&myLock, &myLockAttr);
#else
    myLock = new CRITICAL_SECTION;
    ::InitializeCriticalSection(myLock);
#endif
#endif
}

LOCK::~LOCK()
{
#ifdef STD_THREAD
#else
#ifdef LINUX
    pthread_mutex_destroy(&myLock);
    pthread_mutexattr_destroy(&myLockAttr);
#else
    DeleteCriticalSection(myLock);
    delete myLock;
#endif
#endif
}

bool
LOCK::tryToLock()
{
#ifdef STD_THREAD
    return myLock.try_lock();
#else
#ifdef LINUX
    return !pthread_mutex_trylock(&myLock);
#else
    return ::TryEnterCriticalSection(myLock) ? true : false;
#endif
#endif
}

void
LOCK::lock()
{
#ifdef STD_THREAD
    myLock.lock();
#else
#ifdef LINUX
    pthread_mutex_lock(&myLock);
#else
    ::EnterCriticalSection(myLock);
#endif
#endif
}

void
LOCK::unlock()
{
#ifdef STD_THREAD
    myLock.unlock();
#else
#ifdef LINUX
    pthread_mutex_unlock(&myLock);
#else
    ::LeaveCriticalSection(myLock);
#endif
#endif
}

bool
LOCK::isLocked()
{
    // Note this may spuriously fail in std::thread, so we may
    // declare locked something unlocked.  (But you should be stable with
    // that because this sort of thing is variable in a MT environment regardless!
    if (tryToLock())
    {
	unlock();
	return false;
    }
    return true;
}

#ifdef STD_THREAD
CONDLOCK::CONDLOCK()
{
}

CONDLOCK::~CONDLOCK()
{
}

bool
CONDLOCK::tryToLock()
{
    return myLock.try_lock();
}

void
CONDLOCK::lock()
{
    myLock.lock();
}

void
CONDLOCK::unlock()
{
    myLock.unlock();
}

bool
CONDLOCK::isLocked()
{
    // Note this may spuriously fail in std::thread, so we may
    // declare locked something unlocked.  (But you should be stable with
    // that because this sort of thing is variable in a MT environment regardless!
    if (tryToLock())
    {
	unlock();
	return false;
    }
    return true;
}
#endif  	// def STD_THREAD


CONDITION::CONDITION()
{
#ifdef STD_THREAD
#else
#ifdef LINUX
    pthread_cond_init(&myCond, 0);
#else
    myEvent = CreateEvent(0, false, false, 0);
    myNumWaiting.set(0);
#endif
#endif
}

CONDITION::~CONDITION()
{
#ifdef STD_THREAD
#else
#ifdef LINUX
    pthread_cond_destroy(&myCond);
#else
    CloseHandle(myEvent);
#endif
#endif
}

void
CONDITION::wait(AUTOCONDLOCK &autolock)
{
#ifdef STD_THREAD
    myCond.wait(autolock.myLockHolder);
#else
#ifdef LINUX
    pthread_cond_wait(&myCond, &autolock.myLock.myLock);
#else
    myNumWaiting.add(1);
    autolock.myLock.unlock();
    WaitForSingleObject(myEvent, INFINITE);
    myNumWaiting.add(-1);
    autolock.myLock.lock();
#endif
#endif
}

void
CONDITION::trigger()
{
#ifdef STD_THREAD
    myCond.notify_one();
#else
#ifdef LINUX
    pthread_cond_signal(&myCond);
#else
    // This requires the caller has the lock active so no one
    // will start a wait during our test.
    if (myNumWaiting > 0)
	SetEvent(myEvent);
#endif
#endif
}
