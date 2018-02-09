/** @file geomath.h
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef GLOOM_GEOMATH_H
#define GLOOM_GEOMATH_H

#include <de/Vector>

namespace gloom { namespace geo {

/**
 * 2D line.
 */
template <typename T>
struct Line
{
    T start;
    T end;

    Line() {}

    Line(const T &span)
        : end(span)
    {}

    Line(const T &a, const T &b)
        : start(a)
        , end(b)
    {}

    T span() const
    {
        return end - start;
    }

    T dir() const
    {
        return span().normalize();
    }

    T normal() const
    {
        const auto dirVec = dir();
        return de::Vector3<typename T::ValueType>(dirVec.x, 0.0, dirVec.y)
            .cross(de::Vector3<typename T::ValueType>(0, 1, 0))
            .xz();
    }

    bool intersect(const Line &other, double &t) const
    {
        const auto &p = other.start;
        const auto &r = other.span();

        const auto &q = start;
        const auto  s = span();

        const double r_s = r.cross(s);
        if (de::abs(r_s) < de::EPSILON)
        {
            return false;
        }
        t = (q - p).cross(r) / r_s;

        // It has to hit somewhere on `other`.
        const double u = (q - p).cross(s) / r_s;
        return u >= 0 && u <= 1;
    }
};

}} // namespace gloom::geo

#endif // GLOOM_GEOMATH_H
