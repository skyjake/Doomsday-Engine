/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBDENG2_MATH_H
#define LIBDENG2_MATH_H

#include <de/libdeng2.h>
#include <cmath>

#ifdef min
#   undef min
#endif

#ifdef max
#   undef max
#endif

namespace de {

//#undef PI
ddouble const PI = 3.1415926535897932384626433832795028841971693993751;
ddouble const EPSILON = 1.0e-7;
dfloat const FLOAT_EPSILON = 1.0e-5f;

/// Absolute value.
template <typename Type>
inline Type abs(Type const &a) {
    if(a < 0.0) {
        return -a;
    }
    return a;
}

// Special case, this is never negative.
inline duint abs(duint const &a) {
    return a;
}

/// Minimum of two values.
template <typename Type>
inline Type const &min(Type const &a, Type const &b) {
    return (a < b? a : b);
}

/// Minimum of three values.
template <typename Type>
inline Type const &min(Type const &a, Type const &b, Type const &c) {
    return min(a, min(b, c));
}

/// Maximum of two values.
template <typename Type>
inline Type const &max(Type const &a, Type const &b) {
    return (a > b? a : b);
}

/// Maximum of three values.
template <typename Type>
inline Type const &max(Type const &a, Type const &b, Type const &c) {
    return max(a, max(b, c));
}

/// Clamp value within range.
template <typename Type>
inline Type clamp(Type const &low, Type const &value, Type const &high) {
    return min(max(value, low), high);
}

/// Wrap value within range [low, high).
template <typename Type>
Type wrap(Type value, Type const &low, Type const &high) {
    Type const range = high - low;
    while(value < low) value += range;
    while(value >= high) value -= range;
    return value;
}

template <typename Type>
inline Type squared(Type const &value) { return value * value; }

template <typename Type>
inline Type round(dfloat const &value) {
    return Type(std::floor(value + 0.5f));
}

inline dint   roundi(dfloat const &value) { return round<int>(value);   }
inline dfloat roundf(dfloat const &value) { return round<float>(value); }

template <typename Type>
inline Type round(ddouble const &value) {
    return Type(std::floor(value + 0.5));
}

inline dint32 floor(dfloat const &value) {
    return dint32(std::floor(value));
}

inline dint64 floor(ddouble const &value) {
    return dint64(std::floor(value));
}

inline dint32 ceil(dfloat const &value) {
    return dint32(std::ceil(value));
}

inline dint64 ceil(ddouble const &value) {
    return dint64(std::ceil(value));
}

template <typename Type>
inline Type fract(Type const &value) {
    return value - std::floor(value);
}

/// Compare two single-precision floating-point values for equality,
/// with the precision of FLOAT_EPSILON.
inline bool fequal(dfloat a, dfloat b) {
    return abs(a - b) < FLOAT_EPSILON;
}

/// Compare two double-precision floating-point values for equality,
/// with the precision of EPSILON.
inline bool fequal(ddouble a, ddouble b) {
    return abs(a - b) < EPSILON;
}

/// Compare two single-precision floating-point values for equality,
/// with a user specified precision.
inline bool fequal(dfloat a, dfloat b, dfloat precision) {
    return abs(a - b) < abs(precision);
}

/// Compare two double-precision floating-point values for equality.
/// with a user specified precision.
inline bool fequal(ddouble a, ddouble b, ddouble precision) {
    return abs(a - b) < abs(precision);
}

template <typename Type>
inline Type degreeToRadian(Type const &degree) {
    return degree / Type(180) * PI;
}

template <typename Type>
inline Type radianToDegree(Type const &radian) {
    return radian / PI * Type(180);
}

/// General comparison function.
template <typename Type>
inline dint cmp(Type const &a, Type const &b) {
    if(a < b) return -1;
    if(a > b) return 1;
    return 0;
}

template <typename IntType>
IntType ceilPow2(IntType const &num) {
    IntType cumul;
    for(cumul = 1; num > cumul; cumul <<= 1) {}
    return cumul;
}

/**
 * Linear interpolation between two values.
 *
 * @param start  Value to interpolate from.
 * @param end    Value to interpolate to.
 * @param pos    Normalized interpolation point [0..1].
 */
template <typename Type>
inline Type lerp(Type const &start, Type const &end, float pos) {
    return end * pos + (start * (1.f - pos));
}

} // namespace de

#endif /* LIBDENG2_MATH_H */
