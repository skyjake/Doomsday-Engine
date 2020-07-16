/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_MATH_H
#define LIBCORE_MATH_H

#include "de/ibytearray.h"
#include <cmath>

#ifdef min
#   undef min
#endif

#ifdef max
#   undef max
#endif

namespace de {

/**
 * @addtogroup math
 * @{
 */

//#undef PI
const ddouble PI            = 3.1415926535897932384626433832795028841971693993751;
const dfloat  PIf           = dfloat(PI);
const ddouble EPSILON       = 1.0e-7;
const dfloat  FLOAT_EPSILON = 1.0e-5f;

/// Absolute value.
template <typename Type>
inline Type abs(Type a) {
    if (a < Type(0)) {
        return -a;
    }
    return a;
}

// Special case, this is never negative.
inline duint abs(duint a) {
    return a;
}

template <typename Integer>
inline Integer mod(Integer a, Integer b) {
    const Integer r = a % b;
    return r < 0 ? (r + b) : r;
}

/// Minimum of two values.
template <typename Type>
inline Type min(Type a, Type b) {
    return (a < b? a : b);
}

/// Minimum of three values.
template <typename Type>
inline Type min(Type a, Type b, Type c) {
    return min(a, min(b, c));
}

/// Maximum of two values.
template <typename Type>
inline Type max(Type a, Type b) {
    return (a > b? a : b);
}

/// Maximum of three values.
template <typename Type>
inline Type max(Type a, Type b, Type c) {
    return max(a, max(b, c));
}

/// Clamp value within range.
template <typename Type>
inline Type clamp(Type low, Type value, Type high) {
    return min(max(value, low), high);
}

/// Clamp value within @a lowHighLimit and @a -lowHighLimit.
template <typename Type>
inline Type clamp(Type value, Type lowHighLimit) {
    return clamp(-lowHighLimit, value, lowHighLimit);
}

/// Wrap value within range [low, high).
template <typename Type>
Type wrap(Type value, Type low, Type high) {
    DE_ASSERT(high > low);
    const Type range = high - low;
    while (value < low) value += range;
    while (value >= high) value -= range;
    return value;
}

template <typename Type>
inline Type squared(Type value) { return value * value; }

template <typename Type>
inline Type cubed(Type value) { return value * value * value; }

template <typename Type>
inline Type round(dfloat value) {
    return Type(std::floor(value + 0.5f));
}

inline dint   roundi(dfloat value) { return round<int>(value);   }
inline dfloat roundf(dfloat value) { return round<float>(value); }

template <typename Type>
inline Type round(const ddouble &value) {
    return Type(std::round(value));
}

inline dint64 roundi(ddouble value) { return round<dint64>(value); }

inline dint32 floor(dfloat value) {
    return dint32(std::floor(value));
}

inline dint64 floor(ddouble value) {
    return dint64(std::floor(value));
}

inline dint32 ceil(dfloat value) {
    return dint32(std::ceil(value));
}

inline dint64 ceil(ddouble value) {
    return dint64(std::ceil(value));
}

template <typename Type>
inline Type fract(Type value) {
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

enum class Sign { Positive, Zero, Negative };

template <typename Type>
inline Sign sign(const Type &a) {
    if (a < Type(0)) return Sign::Negative;
    if (a > Type(0)) return Sign::Positive;
    return Sign::Zero;
}

template <typename Type>
inline Type asNumber(Sign s) {
    if (s == Sign::Negative) return -1;
    if (s == Sign::Positive) return +1;
    return 0;
}

template <typename Type>
inline Type operator *(Sign s, Type t) { return asNumber<Type>(s) * t; }

template <typename Type>
inline Type operator *(Type t, Sign s) { return t * asNumber<Type>(s); }

template <typename Type>
inline Type degreeToRadian(Type degree) {
    return degree * PI / Type(180);
}

template <typename Type>
inline Type radianToDegree(Type radian) {
    return radian * Type(180) / PI;
}

/// General comparison function.
template <typename Type>
inline dint cmp(Type a, Type b) {
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

template <typename IntType>
IntType ceilPow2(IntType num) {
    IntType cumul;
    for (cumul = 1; num > cumul; cumul <<= 1) {}
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
inline Type lerp(Type start, Type end, float pos) {
    return end * pos + (start * (1.f - pos));
}

/**
 * @return Random floating-point value in the range [0, 1).
 */
DE_PUBLIC float randf();

/**
 * @return Random unsigned integer in the range [0, 4294967295].
 */
DE_PUBLIC duint32 randui32();

/** @} */

/**
 * Calculates the CRC32 checksum of the contents of byte array @a bytes.
 */
DE_PUBLIC duint32 crc32(const IByteArray &bytes);

} // namespace de

#endif /* LIBCORE_MATH_H */
