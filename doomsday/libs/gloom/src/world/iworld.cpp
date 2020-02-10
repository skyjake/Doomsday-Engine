/** @file iworld.cpp
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

#include "gloom/world/iworld.h"

using namespace de;

namespace gloom {

IWorld::IWorld()
{}

IWorld::~IWorld()
{}

void IWorld::glInit()
{}

void IWorld::glDeinit()
{}

void IWorld::update(TimeSpan)
{}

void IWorld::render(const ICamera &)
{}

IWorld::POI IWorld::initialViewPosition() const
{
    return Vec3f();
}

List<IWorld::POI> IWorld::pointsOfInterest() const
{
    return List<POI>();
}

double IWorld::groundSurfaceHeight(const Vec3d &) const
{
    return 0.0;
}

double IWorld::ceilingHeight(const Vec3d &) const
{
    return 1000.0;
}

} // namespace gloom
