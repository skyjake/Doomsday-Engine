/** @file mobj.h  Base for world map objects.
 *
 * @authors Copyright © 2014-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_MOBJ_H
#define LIBDOOMSDAY_MOBJ_H

#include "dd_share.h" /// @todo dd_share.h is not part of libdoomsday.
#include "../players.h"

// This macro can be used to calculate a mobj-specific 'random' number.
#define MOBJ_TO_ID(mo)          ( (long)(mo)->thinker.id * 48 + (PTR2INT(mo)/1000) )

// Game plugins define their own mobj_s/t.
/// @todo Plugin mobjs should be derived from a class in libdoomsday, and
/// the DD_BASE_MOBJ_ELEMENTS macros should be removed. -jk
#ifndef LIBDOOMSDAY_CUSTOM_MOBJ

// We'll use the base mobj template directly as our mobj.
typedef struct mobj_s {
    DD_BASE_MOBJ_ELEMENTS()
} mobj_t;

#endif // LIBDOOMSDAY_CUSTOM_MOBJ

#ifdef __cplusplus

#include <de/Vector>

/**
 * Returns a copy of the map-object's origin in map space.
 */
LIBDOOMSDAY_PUBLIC de::Vec3d Mobj_Origin(const mobj_t &mob);

/**
 * Returns the map-object's visual center (i.e., origin plus z-height offset).
 */
LIBDOOMSDAY_PUBLIC de::Vec3d Mobj_Center(mobj_t &mob);

/**
 * Returns the physical radius of the mobj.
 *
 * @param mob  Map-object.
 *
 * @see Mobj_VisualRadius()
 */
LIBDOOMSDAY_PUBLIC coord_t Mobj_Radius(const mobj_t &mob);

/**
 * Returns an axis-aligned bounding box for the mobj in map space, centered
 * on the origin with dimensions equal to @code radius * 2 @endcode.
 *
 * @param mob  Map-object.
 *
 * @see Mobj_Radius()
 */
LIBDOOMSDAY_PUBLIC AABoxd Mobj_Bounds(const mobj_t &mob);

#endif // __cplusplus

#endif // LIBDOOMSDAY_MOBJ_H
