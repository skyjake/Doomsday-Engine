/** @file maputil.h  World map utilities.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifdef __CLIENT__
#ifndef WORLD_MAPUTIL_H
#define WORLD_MAPUTIL_H

#include <doomsday/world/lineowner.h>
#include <de/libcore.h>
#include <de/legacy/binangle.h>

#include "world/line.h"

namespace world { class Sector; }

/**
 * Returns @c true if this side is considered "closed" (i.e., there is no opening through
 * which the relative back Sector can be seen). Tests consider all Planes which interface
 * with this and the "middle" Material used on the "this" side.
 *
 * @param side           LineSide instance.
 * @param ignoreOpacity  @c true= do not consider Material opacity.
 */
bool R_SideBackClosed(const LineSide &side, bool ignoreOpacity = true);

/**
 * A neighbor is a Line that shares a vertex with @a line and faces the given @a sector.
 */
Line *R_FindLineNeighbor(const Line &line, const world::LineOwner &own, de::ClockDirection direction,
    const world::Sector *sector, binangle_t *diff = nullptr);

Line *R_FindSolidLineNeighbor(const Line &line, const world::LineOwner &own, de::ClockDirection direction,
    const world::Sector *sector, binangle_t *diff = nullptr);

#endif  // WORLD_MAPUTIL_H
#endif  // __CLIENT__
