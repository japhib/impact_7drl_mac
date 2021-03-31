#pragma once

#include "assert.h"
#include "ptrlist.h"
#include "vec2.h"

template <typename T>
class ROOMDATA
{
public:
    ROOMDATA(int w, int h, T def);
    ~ROOMDATA();
    ROOMDATA(const ROOMDATA<T> &src);
    ROOMDATA &operator=(const ROOMDATA<T> &src);

    int		width() const { return myWidth; }
    int		height() const { return myHeight; }

    class op_equal
    {
    public:
	op_equal(T value) { myV = value; }
	bool operator()(T comp) const { return comp == myV; }
    private:
	T	myV;
    };

    class op_notequal
    {
    public:
	op_notequal(T value) { myV = value; }
	bool operator()(T comp) const { return comp != myV; }
    private:
	T	myV;
    };

    template <typename OP>
    int		countMatch(OP op) const
    {
	int		x, y;
	int		result = 0;
	FORALL_XY(x, y)
	{
	    if (op((*this)(x, y)))
		result++;
	}
	return result;
    }
    int		countEqual(T value) const
    { op_equal op(value); return countMatch(op); }
    int		countNotEqual(T value) const
    { op_notequal op(value); return countMatch(op); }

    template <typename OP>
    bool		findNthMatch(OP op, int nth, int &x, int &y) const
    {
	FORALL_XY(x, y)
	{
	    if (op((*this)(x,y)))
	    {
		if (!nth)
		    return true;
		nth--;
	    }
	}
	return false;
    }
    bool		findNthEqual(T value, int nth, int &x, int &y) const
    { op_equal op(value); return findNthMatch(op, nth, x, y); }
    bool		findNthNotEqual(T value, int nth, int &x, int &y) const
    { op_notequal op(value); return findNthMatch(op, nth, x, y); }

	    
    void	findMin(int &mx, int &my) const
    {
	T		mval = (*this)(0,0);
	int		x, y;
	mx = my = 0;
	FORALL_XY(x, y)
	{
	    if ((*this)(x,y) < mval)
	    {
		mval = (*this)(x, y);
		mx = x;
		my = y;
	    }
	}
    }
    template <typename OP>
    bool	randomMatch(OP op, int &foundx, int &foundy) const
    {
	int		nth = rand_choice(countMatch(op));

	return findNthMatch(op, nth, foundx, foundy);
    }
    bool	randomEqual(T value, int &foundx, int &foundy) const
    { op_equal op(value); return randomMatch(op, foundx, foundy); }
    bool	randomNotEqual(T value, int &foundx, int &foundy) const
    { op_notequal op(value); return randomMatch(op, foundx, foundy); }
    template <typename OP>
    void	buildListMatch(OP op, PTRLIST<IVEC2> &ptlist) const
    {
	int		x, y;
	FORALL_XY(x, y)
	{
	    if (op((*this)(x,y)))
	    {
		IVEC2		pt;
		pt.x() = x;
		pt.y() = y;
		ptlist.append(pt);
	    }
	}
    }
    void	buildListEqual(T value, PTRLIST<IVEC2> &ptlist) const
    { op_equal op(value); buildListMatch(op, ptlist); }

    T		operator()(int x, int y) const
    { if (x < 0 || x >= myWidth ||
	  y < 0 || y >= myHeight)
	return myDefault;
      return myData[x + y * myWidth]; 
    }
    T		operator()(IVEC2 p) const
    { return (*this)(p.x(), p.y()); }
    T		&operator()(int x, int y)
    {
	J_ASSERT(inbounds(x, y));
	J_ASSERT(x >= 0 && x < myWidth);
	J_ASSERT(y >= 0 && y < myHeight);
	return myData[x+y * myWidth];
    }
    T		&operator()(IVEC2 p)
    { return (*this)(p.x(), p.y()); }
    bool	inbounds(int x, int y) const
    {
	if (x >= 0 && x < myWidth &&
	    y >= 0 && y < myHeight)
	{
	    return true;
	}
	return false;
    }

    bool	inbounds(IVEC2 p) const
    { return inbounds(p.x(), p.y()); }

    template <typename OP>
    void	clearIfMatch(OP op, T newval,
			int cx, int cy, int radx, int rady)
    {
	for (int y = cy - rady; y <= cy + rady; y++)
	{
	    for (int x = cx - radx; x <= cx + radx; x++)
	    {
		if (!inbounds(x, y))
		    continue;
		if (op((*this)(x, y)))
		    (*this)(x, y) = newval;
	    }
	}
    }
    void	clearIfEqual(T compare, T newval,
			int cx, int cy, int radx, int rady)
    { op_equal op(compare); clearIfMatch(op, newval, cx, cy, radx, rady); }

    void	clear(T value);
protected:
    T		*myData;
    T		 myDefault;
    int		 myWidth, myHeight;
};


template <typename T>
ROOMDATA<T>::ROOMDATA(int w, int h, T def)
{
    myWidth = w;
    myHeight = h;
    myDefault = def;

    myData = new T[myWidth * myHeight];
    clear(def);
}

template <typename T>
ROOMDATA<T>::ROOMDATA(const ROOMDATA<T> &src)
{
    myData = 0;
    *this = src;
}

template <typename T>
ROOMDATA<T> &
ROOMDATA<T>::operator=(const ROOMDATA<T> &src)
{
    if (&src == this)
	return *this;
    delete [] myData;
    myWidth = src.myWidth;
    myHeight = src.myHeight;
    myDefault = src.myDefault;

    myData = new T[myWidth * myHeight];

    memcpy(myData, src.myData, sizeof(T) * myWidth * myHeight);

    return *this;
}

template <typename T>
ROOMDATA<T>::~ROOMDATA()
{
    delete [] myData;
}

template <typename T>
void
ROOMDATA<T>::clear(T value)
{
    for (int y = 0; y < myHeight; y++)
	for (int x = 0; x < myWidth; x++)
	    (*this)(x, y) = value;
}
