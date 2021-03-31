// This is just a crappy .h file because I'm too lazy to rename
// where the enummaker's output goes to.

#ifndef __mygba_h__
#define __mygba_h__

#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "assert.h"


#if 0
// J_ASSERT is of course j'assert, for "I assert!"
// I'm not just typing that because I'm on a train heading back from Montreal.
#ifdef LINUX
#define J_ASSERT(test) assert(test)
#else
#define J_ASSERT(test) \
do {			\
    if (!(test))	\
    {			\
	__debugbreak();	\
	assert(false);	\
    }			\
} while (false)
#endif
#else
#define J_ASSERT(test) (void)0;
#endif

typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

inline int MYstrlen(const char *str) { return (int)strlen(str); }

// Bit scan functions
#ifdef LINUX
u32 __inline__ MYbsr(u32 mask)
{
    return 31 - __builtin_clz(mask);
}
u32 __inline__ MYbsl(u32 mask)
{
    return __builtin_ctz(mask);
}
#else
#include <intrin.h>
u32 inline MYbsl(u32 mask)
{
    unsigned long result;
    _BitScanForward(&result, mask);
    return result;
}
u32 inline MYbsr(u32 mask)
{
    unsigned long result;
    _BitScanReverse(&result, mask);
    return result;
}
#endif

// ... grr ... 
#ifdef LINUX
#define MYstrdup(foo) ::strdup(foo)
#define MYunlink(foo) ::unlink(foo)
#else
#define MYstrdup(foo) ::strdup(foo)
#define MYunlink(foo) ::_unlink(foo)
#endif

// MSVC is a broken compiler.
inline bool MYisspace(int c) { if (c < 0 || c >= 128) return false; return isspace(c) ? true : false; }
inline bool MYisdigit(int c) { if (c < 0 || c >= 128) return false; return isdigit(c) ? true : false; }
inline bool MYisalnum(int c) { if (c < 0 || c >= 128) return false; return isalnum(c) ? true : false; }
inline bool MYisalpha(int c) { if (c < 0 || c >= 128) return false; return isalpha(c) ? true : false; }
inline bool MYislower(int c) { if (c < 0 || c >= 128) return false; return islower(c) ? true : false; }
inline bool MYisupper(int c) { if (c < 0 || c >= 128) return false; return isupper(c) ? true : false; }

#endif
