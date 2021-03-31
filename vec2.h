/*
 * Licensed under Tridude Heart license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	Tridude Heart Development
 *
 * NAME:        sprite.cpp ( Tridude, C++ )
 *
 * COMMENTS:
 */

#pragma once

#include "rand.h"
#include <math.h>

template <typename T>
class VEC2_T
{
public:
    VEC2_T() { vec[0] = vec[1] = 0; }
    VEC2_T(T x, T y) { vec[0] = x; vec[1] = y; }
    VEC2_T(T x) { vec[0] = x; vec[1] = x; }
    VEC2_T(const VEC2_T<T> &v) { vec[0] = v.vec[0]; vec[1] = v.vec[1]; }

    VEC2_T<T>	&operator=(T f)
    { *this = VEC2_T<T>(f); return *this; }
    VEC2_T<T>	&operator=(const VEC2_T<T> &v)
    { vec[0] = v.vec[0]; vec[1] = v.vec[1]; return *this; }

    T	x() const { return vec[0]; }
    T	&x() { return vec[0]; }
    T	y() const { return vec[1]; }
    T	&y() { return vec[1]; }

    void	normalize()
    { float len = length(); if (len > 0.00001) { vec[0] /= len; vec[1] /= len; } }
    float	length() const
    { return sqrt(length2()); }
    float	length2() const
    { return vec[0]*vec[0] + vec[1]*vec[1]; }


    T	operator()(int i) const { return vec[i]; }
    T	&operator()(int i) { return vec[i]; }

    bool	 operator==(const VEC2_T<T> &v)
    { return vec[0] == v.vec[0] && vec[1] == v.vec[1]; }

    VEC2_T<T>	&operator+=(const VEC2_T<T> &v)
    { vec[0] += v.vec[0]; vec[1] += v.vec[1]; return *this; }
    VEC2_T<T>	&operator*=(const VEC2_T<T> &v)
    { vec[0] *= v.vec[0]; vec[1] *= v.vec[1]; return *this; }
    VEC2_T<T>	&operator-=(const VEC2_T<T> &v)
    { vec[0] -= v.vec[0]; vec[1] -= v.vec[1]; return *this; }
    VEC2_T<T>	&operator/=(const VEC2_T<T> &v)
    { vec[0] /= v.vec[0]; vec[1] /= v.vec[1]; return *this; }

    VEC2_T<T>	&operator*=(float v)
    { vec[0] *= v; vec[1] *= v; return *this; }
    VEC2_T<T>	&operator/=(float v)
    { vec[0] /= v; vec[1] /= v; return *this; }

    VEC2_T<T>	 operator-() const
    { return VEC2_T<T>(-vec[0], -vec[1]); }

    void	clamp(const VEC2_T<T> &vmin, const VEC2_T<T> &vmax)
    {
	vec[0] = BOUND(vec[0], vmin.vec[0], vmax.vec[0]);
	vec[1] = BOUND(vec[1], vmin.vec[1], vmax.vec[1]);
    }

    void	rotate(float angle)
    {
	float		c, s, nx, ny;
	c = cos(angle);
	s = sin(angle);

	nx = c * x() - s * y();
	ny = s * x() + c * y();
	vec[0] = nx;
	vec[1] = ny;
    }
    void	rotate90()
    {
	T		tx = x();
	vec[0] = -vec[1];
	vec[1] = tx;
    }

private:
    T		vec[2];
};

template <typename T>
inline VEC2_T<T> operator+(const VEC2_T<T> &v1, const VEC2_T<T> &v2)
{ return VEC2_T<T>(v1.x()+v2.x(), v1.y()+v2.y()); }
template <typename T>
inline VEC2_T<T> operator-(const VEC2_T<T> &v1, const VEC2_T<T> &v2)
{ return VEC2_T<T>(v1.x()-v2.x(), v1.y()-v2.y()); }
template <typename T>
inline VEC2_T<T> operator*(const VEC2_T<T> &v1, const VEC2_T<T> &v2)
{ return VEC2_T<T>(v1.x()*v2.x(), v1.y()*v2.y()); }
template <typename T>
inline VEC2_T<T> operator/(const VEC2_T<T> &v1, const VEC2_T<T> &v2)
{ return VEC2_T<T>(v1.x()/v2.x(), v1.y()/v2.y()); }

template <typename T>
inline VEC2_T<T> operator*(float v1, const VEC2_T<T> &v2)
{ return VEC2_T<T>(v1*v2.x(), v1*v2.y()); }
template <typename T>
inline VEC2_T<T> operator/(float v1, const VEC2_T<T> &v2)
{ return VEC2_T<T>(v1/v2.x(), v1/v2.y()); }
template <typename T>
inline VEC2_T<T> operator*(const VEC2_T<T> &v1, float v2)
{ return VEC2_T<T>(v1.x()*v2, v1.y()*v2); }
template <typename T>
inline VEC2_T<T> operator/(const VEC2_T<T> &v1, float v2)
{ return VEC2_T<T>(v1.x()/v2, v1.y()/v2); }

template <typename T>
inline float dist(VEC2_T<T> v1, VEC2_T<T> v2)
{ return (v1 - v2).length(); }
template <typename T>
inline float quadrature(VEC2_T<T> v1, VEC2_T<T> v2)
{ return (v1 - v2).length2(); }


typedef VEC2_T<float> VEC2;
typedef VEC2_T<int> IVEC2;
