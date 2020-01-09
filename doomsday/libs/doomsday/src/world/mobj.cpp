/** @file mobj.cpp  Base for world map objects.
 *
 * @authors Copyright (c) 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/world/mobj.h"

using namespace de;

Vec3d Mobj_Origin(const mobj_t &mob)
{
    return de::Vec3d(mob.origin);
}

Vec3d Mobj_Center(mobj_t &mob)
{
    return de::Vec3d(mob.origin[0], mob.origin[1], mob.origin[2] + mob.height / 2);
}

coord_t Mobj_Radius(const mobj_t &mobj)
{
    return mobj.radius;
}

AABoxd Mobj_Bounds(const mobj_t &mobj)
{
    const Vec2d origin = Mobj_Origin(mobj);
    const ddouble radius  = Mobj_Radius(mobj);
    return AABoxd(origin.x - radius, origin.y - radius,
                  origin.x + radius, origin.y + radius);
}
