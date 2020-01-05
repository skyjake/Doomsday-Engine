/** @file core/slope.h  Vector/slope conversions.
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

#pragma once

#include "vector.h"

namespace de {

/**
 * Combination of a yaw angle and Z slope at an XY distance of 1.0 units.
 * @ingroup math
 */
struct DE_PUBLIC Slope
{
    float angle; // radians
    float slope;

    Slope(float angle = 0.0f, float slope = 0.0f);
    Vec3f toUnitVec() const;

    static Slope fromVec(const Vec3f &vector);
};

} // namespace de
