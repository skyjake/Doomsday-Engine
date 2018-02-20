/** @file geomath.cpp
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

#include "gloom/geo/geomath.h"

using namespace de;

namespace gloom { namespace geo {

bool Plane::isPointAbove(const Vec3d &pos) const
{
    return (pos - point).dot(normal) >= 0;
}

double Plane::project(const Vec2d &pos) const
{
    const double d = -normal.x * point.x - normal.y * point.y - normal.z * point.z;
    return (-normal.x * pos.x - normal.z * pos.y - d) / normal.y;
}

}} // namespace gloom::geo

