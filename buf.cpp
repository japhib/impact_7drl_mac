/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRL Development
 *
 * NAME:        buf.h ( POWDER Library, C++ )
 *
 * COMMENTS:
 *	Implements a simple character buffer.  Underlying data
 *	is reference counted and can be passed by value.
 *	Uses copy on write semantic.
 */

#include "mygba.h"
#include "buf.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <malloc.h>
#include "thread.h"
#include "rand.h"

ATOMIC_INT32		glbBufferCount;
ATOMIC_INT32		glbLeakedStringCount;


int
buf_numbufs()
{
    return glbBufferCount;
}

const char *
glb_harden(const char *src)
{
    if (!src)
	return "";
    glbLeakedStringCount.add(1);
    return ::strdup(src);
}

class BUF_int
{
public:
    BUF_int();
    // Takes ownership of the given text, will delete[] it.
    BUF_int(char *text, int len);
    // Does not take ownership of the text
    BUF_int(const char *text);

    void	 incref();
    void	 decref();
    int		 refcnt() { return myRefCount; }

    // Read methods works for shared objects
    const char	*buffer() const { return myData; }
    int		 datalen() { return myDataLen; }
    int		 strlen() 
    { 
	if (myStrLen < 0)
	    myStrLen = MYstrlen(myData);
	return myStrLen;
    }
    void	 dirtystrlen() { myStrLen = -1; }

    // Write methods require you have a uniqued buffer.
    void	 resize(int newlen);
    char	*data() { return myData; }

private:
    ~BUF_int();

private:
    ATOMIC_INT32	 myRefCount;
    char	*myData;
    // Allocated bytes for data.  -1 means we don't own it.
    int		 myDataLen;
    // -1 for not calculated.
    int		 myStrLen;
};

BUF_int::BUF_int()
{
    myData = (char *) "";
    myDataLen = -1;
    myRefCount.set(0);
    myStrLen = 0;

    glbBufferCount.add(1);
}

BUF_int::BUF_int(char *text, int len)
{
    myData = text;
    myDataLen = len;
    myRefCount.set(0);
    myStrLen = -1;

    glbBufferCount.add(1);
}

BUF_int::BUF_int(const char *text)
{
    myData = (char *) text;
    myDataLen = -1;
    myRefCount.set(0);
    myStrLen = -1;

    glbBufferCount.add(1);
}

BUF_int::~BUF_int()
{
    J_ASSERT(myRefCount <= 0);
    if (myDataLen >= 0)
	delete [] myData;

    glbBufferCount.add(-1);
}

void
BUF_int::incref()
{
    myRefCount.add(1);
}

void
BUF_int::decref()
{
    myRefCount.add(-1);
    if (myRefCount <= 0)
	delete this;
}

void
BUF_int::resize(int newlen)
{
    J_ASSERT(myRefCount <= 1);
    
    char		*text;
    int			 slen;

    // This is an inplace resize so we can never reduce!
    slen = strlen() + 1;
    if (slen > newlen)
	newlen = slen;

    // If this is a downsize, ignore it.
    if (newlen < myDataLen)
	return;
    
    text = new char[newlen];
    ::strcpy(text, myData);

    delete [] myData;
    myData = text;
    myDataLen = newlen;
}

///
/// BUF methods
///

BUF::BUF()
{
    myBuffer = 0;
}

BUF::BUF(int len)
{
    myBuffer = 0;

    allocate(len);
}

BUF::~BUF()
{
    if (myBuffer)
	myBuffer->decref();
}

BUF::BUF(const BUF &buf)
{
    myBuffer = 0;
    *this = buf;
}

BUF &
BUF::operator=(const BUF &buf)
{
    if (buf.myBuffer)
	buf.myBuffer->incref();
    if (myBuffer)
	myBuffer->decref();
    myBuffer = buf.myBuffer;
    return *this;
}

const char *
BUF::buffer() const
{
    if (!myBuffer)
	return 0;

    return myBuffer->buffer();
}

void
BUF::steal(char *text)
{
    if (myBuffer)
	myBuffer->decref();

    if (!text)
	myBuffer = 0;
    else
    {
	myBuffer = new BUF_int(text, MYstrlen(text)+1);
	myBuffer->incref();
    }
}

void
BUF::reference(const char *text)
{
    if (myBuffer)
	myBuffer->decref();

    if (!text)
	myBuffer = 0;
    else
    {
	myBuffer = new BUF_int(text);
	myBuffer->incref();
    }
}

void
BUF::strcpy(const char *src)
{
    if (myBuffer)
	myBuffer->decref();

    myBuffer = 0;

    if (src)
    {
	char	*text = new char [MYstrlen(src)+1];
	::strcpy(text, src);
	steal(text);
    }
}

int
BUF::strlen() const
{
    if (!myBuffer)
	return 0;

    return myBuffer->strlen();
}

int
BUF::strcmp(const char *cmp) const
{
    if (!myBuffer)
    {
	if (!cmp)
	    return 0;
	return -1;
    }

    return ::strcmp(buffer(), cmp);
}

int		
BUF::strcasecmp(const char *cmp) const
{
    if (!buffer())
    {
	if (!cmp)
	    return 0;
	return -1;
    }

#ifdef LINUX
    return ::strcasecmp(buffer(), cmp);
#else
    return ::stricmp(buffer(), cmp);
#endif
}

void
BUF::strcat(const char *src)
{
    // Trivial strcat.
    if (!src)
	return;

    if (!myBuffer)
    {
	// Equivalent to strcpy
	strcpy(src);
	return;
    }

    // Find our total length
    int		srclen, mylen;

    uniquify();

    mylen = strlen();
    srclen = MYstrlen(src);
    mylen += srclen + 1;

    myBuffer->resize(mylen);
    // Now safe...
    // We point to the end of our buffer as we know where
    // it is and that saves us O(n^2)
    ::strcat(&myBuffer->data()[strlen()], src);
    myBuffer->dirtystrlen();
}

void
BUF::append(char c)
{
    char	s[2];

    s[0] = c;
    s[1] = '\0';
    strcat(s);
}

void
BUF::clear()
{
    uniquify();
    evildata()[0] = '\0';
    myBuffer->dirtystrlen();
}

bool
BUF::isstring() const
{
    if (!myBuffer)
	return false;

    if (buffer()[0])
	return true;

    return false;
}

char
BUF::lastchar(int nthlast) const
{
    if (!myBuffer)
	return 0;

    int		len;
    len = strlen();
    len -= nthlast + 1;
    if (len < 0)
	return 0;
    return buffer()[len];
}

bool
BUF::extensionMatch(const char *ext) const
{
    const char	*dot;

    dot = ::strrchr(buffer(), '.');
    if (!dot)
	return false;
    dot++;
#ifdef LINUX
    return !::strcasecmp(dot, ext);
#else
    return !::stricmp(dot, ext);
#endif
}

bool
BUF::startsWith(const char *cmp) const
{
    // Everyone starts with blank.
    if (!cmp)
	return true;
    if (!*cmp)
	return true;
    if (!myBuffer)
    {
	return false;
    }

    return (::strncmp(buffer(), cmp, MYstrlen(cmp)) == 0);
}

bool
BUF::endsWith(const char *cmp) const
{
    // Everyone starts with blank.
    if (!cmp)
	return true;
    if (!*cmp)
	return true;
    if (!myBuffer)
    {
	return false;
    }

    if (strlen() < MYstrlen(cmp))
	return false;

    return (::strcmp(&buffer()[strlen()-MYstrlen(cmp)], cmp) == 0);
}

void
BUF::allocate(int len)
{
    char *text = new char[len];

    // We always want to be null terminated!
    *text = 0;

    if (myBuffer)
	myBuffer->decref();

    myBuffer = new BUF_int(text, len);
    myBuffer->incref();
}

void
BUF::save(ostream &os) const
{
    int		 len = 0;
    
    if (!myBuffer)
    {
	// Zero sized, trivial
	os.write((const char *) &len, sizeof(int));
	return;
    }

    len = myBuffer->datalen();
    os.write((const char *) &len, sizeof(int));

    os.write(myBuffer->buffer(), len);
}

void
BUF::load(istream &is)
{
    int		 len = 0;

    // Create an empty string.
    if (myBuffer)
	myBuffer->decref();
    myBuffer = 0;

    is.read((char *) &len, sizeof(int));

    // Trivial zero length.
    if (!len)
	return;

    // Allocate and read.
    allocate(len);
    
    is.read(myBuffer->data(), len);
}

int
OURvsnprintf(char *str, size_t size, const char *format, va_list ap)
{
#ifdef WIN32
    // I'd like to use a positional aware variant, but _vsprintf_p
    // doesn't return number that would be written, so you can'd distinguish
    // buffer overflow from a formatting error...
    return _vsnprintf(str, size, format, ap);
#else
#if 1
    int		result;
    va_list	ap_copy;
    // Apparently va_list can't be reused on modern compilers.  Same
    // compilers require support for va_copy which older compilers
    // lack.  *sigh*
    va_copy(ap_copy, ap);
    result = vsnprintf(str, size, format, ap_copy);
    va_end(ap_copy);
    return result;
#else
    return vsnprintf(str, size, format, ap);
#endif
#endif
}

int
BUF::vsprintf(const char *fmt, va_list ap)
{
    int	result;
    int	cursize;

    // We wipe ourself out, so is safe to allocate.
    cursize = 100;
    while (1)
    {
	// Reallocate with the new (larger?) size.
	allocate(cursize);
	
	// Marker so we can tell if we overflowed vs having a bad
	// format tag
	myBuffer->data()[cursize-1] = '\0';
	result = OURvsnprintf(myBuffer->data(), cursize, fmt, ap);
	if (result < 0)
	{
	    // Check if the null is still there, signifying something
	    // went wrong with formatting.
	    if (myBuffer->data()[cursize-1] == '\0')
	    {
		// Treat this as the final buffer.
		result = strlen();
		break;
	    }
	}

	// We really must have a final null, thus this paranoia...
	if (result < 0 || result > cursize-1)
	{
	    cursize *= 2;
	}
	else
	{
	    // Success!
	    break;
	}
    }
    myBuffer->dirtystrlen();
    return result;
}

int
BUF::sprintf(const char *fmt, ...)
{
    va_list	marker;
    int		result;

    va_start(marker, fmt);
    result = vsprintf(fmt, marker);
    va_end(marker);

    return result;
}

char *
BUF::strdup() const
{
    if (!myBuffer)
	return ::strdup("");

    return ::strdup(buffer());
}

char *
BUF::evildata()
{
    J_ASSERT(myBuffer);
    J_ASSERT(myBuffer->refcnt() <= 1);
    myBuffer->dirtystrlen();
    return (char *) buffer();
}

void
BUF::uniquify()
{
    if (!myBuffer)
    {
	// We want a writeable buffer
	allocate(50);
    }
    else
    {
	// If ref count is 1 or 0, we are only reference, so good.
	// UNLESS the current dude is a read only buffer.
	if (myBuffer->refcnt() <= 1 && myBuffer->datalen() >= 0)
	{
	    // Already writeable.
	    return;
	}
	// Copy the current buffer.
	int		len;
	char		*text;
	if (myBuffer->datalen() >= 0)
	    len = myBuffer->datalen();
	else
	    len = myBuffer->strlen() + 1;
	text = new char[len];
	::strcpy(text, myBuffer->buffer());

	// Throw away this buffer and make a new one.
	myBuffer->decref();
	myBuffer = new BUF_int(text, len);
	myBuffer->incref();
    }
}

void
BUF::makeFileSafe()
{
    uniquify();

    // Strip out evil characters
    unsigned char	*c;
    for (c = (unsigned char *)evildata(); *c; c++)
    {
	if (*c == '/' || *c == '\\' || *c == ':')
	    *c = '_';
    }
}

BUF
BUF::protectWithQuotes() const
{
    BUF		result = *this;

    if (!isstring())
    {
	result.reference("\"\"");
	return result;
    }

    // Check if we have any dangerous characters that need protecting.
    if (strchr('\\') || strchr('"') || strchr(' '))
    {
	result.clear();

	const char *src = buffer();
	result.append('"');
	while (*src)
	{
	    if (*src == '"')
	    {
		result.strcat("\\\"");
	    }
	    else if (*src == '\t')
	    {
		result.strcat("\\t");
	    }
	    else if (*src == '\n')
	    {
		result.strcat("\\n");
	    }
	    else if (*src == '\\')
	    {
		result.strcat("\\\\");
	    }
	    else
		result.append(*src);
	    src++;
	}
	result.append('"');
    }

    return result;
}

BUF
BUF::stripQuotes() const
{
    BUF		result = *this;

    if (!isstring())
    {
	result.reference("");
	return result;
    }

    if (startsWith("\"") && endsWith("\""))
    {
	// Has quotes to strip!
	result.clear();

	int		len = strlen()-1;
	const char *src = buffer();

	for (int idx = 1; idx < len; idx++)
	{
	    if (src[idx] == '\\')
	    {
		// We should check if idx is penultimate, but we shall
		// foolishly assume we are already a valid range.
		// At worst we read the final "
		idx++;
		if (src[idx] == 'n')
		    result.append('\n');
		else if (src[idx] == 't')
		    result.append('\t');
		else if (src[idx] == '"')
		    result.append('"');
		else if (src[idx] == '\\')
		    result.append('\\');
		else
		    result.append(src[idx]);
	    }
	    else
	    {
		result.append(src[idx]);
	    }
	    idx++;
	}
    }

    return result;
}

void
BUF::stripExtension()
{
    uniquify();

    char		*dot;

    dot = strrchr(evildata(), '.');
    if (dot)
    {
	*dot = '\0';
	myBuffer->dirtystrlen();
    }
}

void
BUF::truncate(int len)
{
    J_ASSERT(strlen() >= len);

    uniquify();
    evildata()[len] = '\0';
    myBuffer->dirtystrlen();
}

BUF
BUF::extractLine()
{
    uniquify();

    BUF		result;
    char	*pos = evildata();

    while (*pos && *pos != '\n' && *pos != '\r')
    {
	result.append(*pos);
	pos++;
    }

    // Strip any \n or \r
    int		nc = 0, rc = 0;

    while (*pos)
    {
	if (*pos == '\n' && !nc)
	    nc++;
	else if (*pos == '\r' && !rc)
	    rc++;
	else
	    break;
	pos++;
    }

    // This dance is because pos points to evildata, so we can't
    // strdup from it.
    char	*temp = ::strdup(pos);
    this->strcpy(temp);
    free(temp);

    return result;
}

BUF
BUF::wordwrap(int width) const
{
    BUF		result;

    const char *src = buffer();

    int		offset = 0;
    while (*src)
    {
	if (*src == '\n')
	{
	    // Trivial reset!
	    offset = 0;
	    result.append(*src);
	    src++;
	    continue;
	}
	result.append(*src);
	offset++;
	src++;

	if (offset >= width)
	{
	    int		rollback = 0;
	    // If this is a space, eat all succeeding spaces.
	    if (ISSPACE(*src))
	    {
	    }
	    else
	    {
		const char *fallback, *start = src - offset;

		fallback = src;
		
		// Not a space, we want to roll back until the next space.
		while (src > start)
		{
		    src--;
		    offset--;
		    if (ISSPACE(*src))
			break;
		}
		if (src == start)
		{
		    // Oops, no breakpoint! Hyphenate arbitrarily.
		    // Restore ourselves to our fallback position.
		    offset += (int)(fallback - src);
		    src = fallback;
		}
		rollback = (int)(fallback - src);
	    }

	    // Write in a null and new line.
	    result.truncate(result.strlen() - rollback);
	    result.append('\n');
	    offset = 0;

	    // Go forward in text removing all spaces.
	    while (*src && ISSPACE(*src) && *src != '\n')
	    {
		src++;
	    }
	}
    }
    return result;
}

void
BUF::stripWS()
{
    stripLeadingWS();
    stripTrailingWS();
}
void
BUF::stripLeadingWS()
{
    if (!isstring())
	return;

    uniquify();

    char *start = evildata();
    while (*start && MYisspace(*start))
	start++;
    if (!*start)
    {
	// Empty.
	reference("");
	return;
    }
    {
	BUF		result;
	result.strcpy(start);
	*this = result;
    }
}

void
BUF::stripTrailingWS()
{
    if (!isstring())
	return;

    uniquify();

    char	*last = &evildata()[strlen()-1];
    char	*start = evildata();
    while (last >= start && MYisspace(*last))
    {
	*last = '\0';
	last--;
    }

    myBuffer->dirtystrlen();
}

BUF
BUF::substr(int firstidx, int len) const
{
    BUF		result;
    char	*data;

    if (len <= 0)
	return result;

    int		ourlen = strlen();
    if (firstidx >= ourlen)
	return result;

    if (len > ourlen-firstidx)
	len = ourlen-firstidx;

    data = new char[len+1];
    memcpy(data, &buffer()[firstidx], len);
    data[len] = 0;

    result.steal(data);
    return result;
}

void
BUF::evaluateVariables(BUF_VarLookupCB cb, void *data)
{
    if (!isstring())
	return;

    // Verify we have a $( or else this is for naught...
    const char *s;

    for (s = buffer(); *s; s++)
    {
	if (*s == '$' && s[1] == '(')
	    break;
    }
    if (!*s)
	return;

    int		paren = 1;
    const char *e;
    for (e = s + 2; *e; e++)
    {
	if (*e == '(')
	    paren++;
	else if (*e == ')')
	{
	    paren--;
	    if (!paren)
		break;
	}
    }

    if (!*e)
    {
	// Failed variable expand!
	return;
    }

    BUF		variable;
    variable = substr((int)(s - buffer() + 2), (int)((e) - (s + 2)));

    // ENsure there are no variables inside the variable!
    variable.evaluateVariables(cb, data);

    BUF		lookupvalue;
    lookupvalue = cb(variable.buffer(), data);

    // Now splice together
    BUF		prefix, postfix;

    prefix = substr(0, (int)(s - buffer()));
    postfix = substr((int)(e - buffer() + 1), (int)(buffer()+strlen() - (e + 1)));

    BUF		result;
    result.strcat(prefix);
    result.strcat(lookupvalue);
    result.strcat(postfix);

    // Now see if there are any more variables!
    result.evaluateVariables(cb, data);

    // And assign at the end.
    *this = result;
}

int
BUF::hash() const
{
    int		result = 0;
    if (!isstring())
	return result;

    for (const char *s = buffer(); *s; s++)
    {
	result *= (37 + 256);
	// Make sure we do unsigned.
	result += *((const u8 *)s);
    }

    return result;
}

BUF
BUF::readtoken(istream &is)
{
    BUF		result;
    char	c;
    
    // Eat white space
    while (is)
    {
	is.read(&c, 1);
	if (!MYisspace(c))
	    break;
    }

    if (!is)
	return result;

    if (c == '"')
    {
	// Quoted string!
	while (is)
	{
	    is.read(&c, 1);

	    if (c == '"')
		break;

	    if (c == '\\')
	    {
		is.read(&c, 1);
		if (c == 'n')
		    result.append('\n');
		else if (c == 't')
		    result.append('\t');
		else
		    result.append(c);
	    }
	    else
		result.append(c);
	}
    }
    else
    {
	// Raw string!
	result.append(c);

	while (is)
	{
	    is.read(&c, 1);
	    if (MYisspace(c))
		break;
	    result.append(c);
	}
    }

    return result;
}

char *
glb_strdup(const char *src)
{
    return strdup(src);
}
