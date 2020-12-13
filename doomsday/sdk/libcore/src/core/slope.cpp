/** @file slope.cpp  Vector/slope conversions.
 *
 * @authors Copyright (c) 2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/core/slope.h"

#include <cmath>

namespace de {

Slope::Slope(float angle, float slope)
    : angle(angle)
    , slope(slope)
{}

Vector3f Slope::toUnitVec() const
{
    return Vector3f(std::cos(angle), std::sin(angle), slope).normalize();
}

Slope Slope::fromVec(const Vector3f &vector)
{
    const auto vec = vector.normalize();
    if (fequal(vec.x, 0.0f) && fequal(vec.y, 0.0f))
    {
        return {0.0f, vec.z > 0 ? std::numeric_limits<float>::max() :
                                  std::numeric_limits<float>::min()};
    }
    return {atan2(vec.y, vec.x), float(vec.z / vec.xy().length())};
}

} // namespace de
