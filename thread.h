/*
 * Licensed under CLIFE license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	CLIFE Development
 *
 * NAME:        thread.h ( CLIFE, C++ )
 *
 * COMMENTS:
 *	Attempt at a threading class.
 */

#ifndef __thread_h__
#define __thread_h__

// Avoids system dependent libraries for standard threading!
#define STD_THREAD

#ifdef STD_THREAD
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#endif

typedef void *(*THREADmainFunc)(void *);

class THREAD
{
public:
    // We use this generator to return the system specific implementation.
    static THREAD	*alloc();
    static int		 numProcessors();

    // Yields the executing thread
    static void		 yield();

    // (Re)starts this, invoking the given function as the main 
    // thread program.
    virtual void	 start(THREADmainFunc func, void *data) = 0;

    // Blocks until this is done
    virtual void	 join() = 0;

    // Is this thread still processing?
    bool	 	 isActive() const { return myActive; }

    // Terminates this thread.
    virtual void	 kill() = 0;
    
protected:
			 THREAD() 
			 { 
			     myActive = false; 
			     myCB = nullptr;
			     myCBData = nullptr;
			 }
    virtual 		~THREAD() {}

    void		 setActive(bool val) { myActive = val; }

    // Blocks until the thread is ready to process.
    virtual void	 waittillimready() = 0;
    virtual void	 iamdonenow() = 0;

    // Canonical main func for threads.
    static void		*wrapper(void *data);

    // No public copying.
	    THREAD(const THREAD &) { }
    THREAD &operator=(const THREAD &) { return *this; }

    bool		 myActive; 
    THREADmainFunc	 myCB;
    void		*myCBData;
};

typedef int s32;

#ifdef STD_THREAD

#else

#ifdef LINUX

#include <pthread.h>

// Requires GCC 4.1 ++
// Consider this punishment for 4.3's behaviour with enums.
inline s32
testandset(s32 *addr, s32 val)
{
    return __sync_lock_test_and_set(addr, val);
}

inline s32
testandadd(s32 *addr, s32 val)
{
    return __sync_fetch_and_add(addr, val);
}

#else

#define _THREAD_SAFE
#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <intrin.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#pragma intrinsic (_InterlockedExchange)
#pragma intrinsic (_InterlockedExchangeAdd)

inline s32
testandset(s32 *addr, s32 val)
{
    return (s32)_InterlockedExchange((long *)addr, (long)val);
}

inline s32
testandadd(s32 *addr, s32 val)
{
    return (s32)_InterlockedExchangeAdd((long *)addr, (long)val);
}

#endif
#endif

class ATOMIC_INT32
{
public:
    explicit ATOMIC_INT32(s32 value = 0) : myValue(value) {}

    // Swap this and val
    inline s32	exchange(s32 val)
    {
#ifdef STD_THREAD
	myValue.exchange(val);
#else
	return testandset(&myValue, val);
#endif
    }

    // Add val to this, return the old value.
    inline s32	exchangeAdd(s32 val)
    {
#ifdef STD_THREAD
	return myValue.fetch_add(val);
#else
	return testandadd(&myValue, val);
#endif
    }

    // Add val to this, return new value.
    inline s32	add(s32 val)
    {
#ifdef STD_THREAD
	return myValue.fetch_add(val) + val;
#else
	return testandadd(&myValue, val) + val;
#endif
    }

    // Set self to maximum of self and val
    inline s32	maximum(s32 val)
    {
	s32	peek = exchange(val);
	while (peek > val)
	{
	    val = peek;
	    peek = exchange(val);
	}
    }

    void	set(s32 val) { myValue = val; }

    operator	s32() const { return myValue; }
private:
#ifdef STD_THREAD
    std::atomic<std::int32_t>	myValue;
#else
    s32		myValue;
#endif

    ATOMIC_INT32(const ATOMIC_INT32 &) {}
    ATOMIC_INT32 &operator=(const ATOMIC_INT32 &) {}
};


class LOCK
{
public:
	LOCK();
	~LOCK();

    // Non blocking attempt at the lock, returns true if now locked.
    bool	tryToLock();

    void	lock();
    void	unlock();
    bool	isLocked();

protected:
    LOCK(const LOCK &) {}
    LOCK &operator=(const LOCK &) {}

#ifdef STD_THREAD
    std::recursive_mutex		 	myLock;
#else
#ifdef LINUX
    pthread_mutex_t	 myLock;
    pthread_mutexattr_t	 myLockAttr;
#else
    CRITICAL_SECTION	*myLock;
#endif
#endif

    friend class CONDITION;
};

#ifdef STD_THREAD
class CONDLOCK
{
public:
	CONDLOCK();
	~CONDLOCK();

    // Non blocking attempt at the lock, returns true if now locked.
    bool	tryToLock();

    void	lock();
    void	unlock();
    bool	isLocked();

protected:
    CONDLOCK(const CONDLOCK &) {}
    CONDLOCK &operator=(const CONDLOCK &) {}

    std::mutex		 	myLock;

    friend class CONDITION;
    friend class AUTOCONDLOCK;
};
#else
using CONDLOCK = LOCK;
#endif

class AUTOLOCK
{
public:
    AUTOLOCK(LOCK &lock) : myLock(lock) { myLock.lock(); }
    ~AUTOLOCK() { myLock.unlock(); }

protected:
    LOCK	&myLock;

    friend class CONDITION;
};

#ifdef STD_THREAD
class AUTOCONDLOCK
{
public:
    AUTOCONDLOCK(CONDLOCK &lock) 
    : myLock(lock) 
    , myLockHolder(lock.myLock, std::defer_lock)
    { myLockHolder.lock(); }
    ~AUTOCONDLOCK() { myLockHolder.unlock(); }

protected:
    std::unique_lock<std::mutex>	myLockHolder;
    CONDLOCK	&myLock;

    friend class CONDITION;
};
#else
using AUTO_CONDLOCK = AUTO_LOCK;
#endif

class CONDITION
{
public:
    CONDITION();
    ~CONDITION();

    // Lock is currently set.  We will unlock it, block, then
    // when condition is triggered relock it and return.
    void	wait(AUTOCONDLOCK &lock);

    // Trigger, allowing one thread through.
    // Caller should currently have the wait lock.
    void	trigger();
private:

    CONDITION(const CONDITION &) {}
    CONDITION &operator=(const CONDITION &) {}

#ifdef STD_THREAD
    std::condition_variable	myCond;
#else
#ifdef LINUX
    pthread_cond_t	myCond;
#else
    ATOMIC_INT32	myNumWaiting;
    HANDLE		myEvent;
#endif
#endif
};

#endif
