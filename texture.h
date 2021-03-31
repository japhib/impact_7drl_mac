/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	1812 Development
 *
 * NAME:        texture.cpp ( Jacob's Matrix, C++ )
 *
 * COMMENTS:
 *		Shared texture map.  Supports COW mechanics.
 */

#pragma once

#include "mygba.h"
#include <memory>

template <typename T>
class TEXTURE_T
{
public:
    TEXTURE_T<T>()
    {
	reset();
    }
    explicit TEXTURE_T<T>(int w, int h)
    {
	calloc(w, h);
    }

    void	reset()
    {
	myData.reset();
	myWidth = myHeight = 0;
    }

    void	calloc(int w, int h)
    {
	if (w * h == 0)
	{
	    myData.reset();
	}
	else
	{
	    myData = std::shared_ptr<T>(new T[w * h]);
	    memset((char *) myData.get(), 0, sizeof(T) * w * h);
	}
	myWidth = w;
	myHeight = h;
    }
    // Does *not* clear if it is same size!
    void	resize(int w, int h)
    {
	if (width() != w || height() != h)
	{
	    calloc(w, h);
	}
    }

    int		height() const { return myHeight; }
    int		width() const { return myWidth; }

    T		get(int x, int y) const
    {
	J_ASSERT(x >= 0 && x < width());
	J_ASSERT(y >= 0 && y < height());
	return myData.get()[x + y * width()];
    }
    T		wrapget(int x, int y) const
    {
	wrap(x, y);
	return get(x, y);
    }
    void		wrap(int &x, int &y) const
    { x = x % width();  if (x < 0) x += width();
      y = y % height(); if (y < 0) y += height(); }
    T		operator()(int x, int y) const { return get(x, y); }
    T		&operator()(int x, int y)
    {
	J_ASSERT(isUnique());
	J_ASSERT(x >= 0 && x < width());
	J_ASSERT(y >= 0 && y < height());
	return myData.get()[x + y * width()];
    }
    void	set(int x, int y, T val)
    {
	J_ASSERT(isUnique());
	J_ASSERT(x >= 0 && x < width());
	J_ASSERT(y >= 0 && y < height());
	myData.get()[x + y * width()] = val;
    }

    void	clear(T val)
    {
	J_ASSERT(isUnique());

	T *d = myData.get();
	for (int i = 0; i < width() * height(); i++)
	    d[i] = val;
    }

    bool	isUnique() const
    {
	return myData.use_count() == 1;
    }
    void	makeUnique()
    {
	J_ASSERT(myData);
	if (!isUnique())
	{
	    std::shared_ptr<T>	newdata = std::shared_ptr<T>(new T[width()*height()]);
	    memcpy(newdata.get(), myData.get(), sizeof(T) * width() * height());
	    myData = newdata;
	    newdata.reset();

	    J_ASSERT(isUnique());
	}
    }

    const T 	*data() const
    {
	return myData.get();
    }

    T 	*writeableData()
    {
	J_ASSERT(isUnique());
	return myData.get();
    }

protected:
    std::shared_ptr<T>	myData;

    int			myWidth, myHeight;
};

using TEXTURE8 = TEXTURE_T<u8>;
using TEXTUREI = TEXTURE_T<int>;

