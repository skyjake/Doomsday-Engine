/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_MATH_H
#define LIBDENG2_MATH_H

#include <de/deng.h>

namespace de
{
    const ddouble PI = 3.14159265358979;
    const ddouble EPSILON = .0000001;

    /// Absolute value.
    template <typename Type>
    inline Type abs(const Type& a) {
        if(a < 0.0)
        {
            return -a;
        }
        return a;
    }

    // Special case, this is never negative.
    inline duint abs(const duint& a) {
        return a;
    }

    /// Minimum of two values.
    template <typename Type>
    inline const Type& min(const Type& a, const Type& b) {
        return (a < b? a : b);
    }

    /// Maximum of two values.
    template <typename Type>
    inline const Type& max(const Type& a, const Type& b) {
        return (a > b? a : b);
    }
    
    /// Clamp value within range.
    template <typename Type>
    inline Type clamp(const Type& low, const Type& value, const Type& high) {
        return min(max(value, low), high);
    }
    
    /// Compare two floating-point values for equality, with the precision of EPSILON.
    inline ddouble fequal(ddouble a, ddouble b) {
        return abs(a - b) < EPSILON;
    }    
}

#endif /* LIBDENG2_MATH_H */
