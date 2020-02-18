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
#include "../api_map.h"
#include "../players.h"

// This macro can be used to calculate a mobj-specific 'random' number.
#define MOBJ_TO_ID(mo)          ( (long)(mo)->thinker.id * 48 + (PTR2INT(mo)/1000) )

// Forward declaration.
struct mobj_s;
typedef struct mobj_s mobj_t;
typedef struct mobjinfo_s mobjinfo_t;

// Game plugins define their own mobj_s/t.
/// @todo Plugin mobjs should be derived from a class in libdoomsday, and
/// the DD_BASE_MOBJ_ELEMENTS macros should be removed. -jk
#ifndef LIBDOOMSDAY_CUSTOM_MOBJ

// We'll use the base mobj template directly as our mobj.
typedef struct mobj_s {
    DD_BASE_MOBJ_ELEMENTS()
} mobj_t;

#endif // LIBDOOMSDAY_CUSTOM_MOBJ

/**
 * Returns the size of the game-defined mobj_t struct.
 */
DE_EXTERN_C LIBDOOMSDAY_PUBLIC size_t Mobj_Sizeof(void);

#ifdef __cplusplus

#include <de/Vector>

namespace world
{
    class BspLeaf;
    class Map;
}

LIBDOOMSDAY_PUBLIC mobj_t *P_MobjCreate(thinkfunc_t function, const de::Vec3d &origin, angle_t angle,
                                        coord_t radius, coord_t height, int ddflags);

/**
 * Called when a mobj is actually removed (when it's thinking turn comes around).
 * The mobj is moved to the unused list to be reused later.
 */
LIBDOOMSDAY_PUBLIC void P_MobjRecycle(mobj_t* mo);

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

/**
 * Returns the map BSP leaf at the origin of the map-object. Note that the mobj must be
 * linked in the map (i.e., @ref Mobj_SetOrigin() has been called).
 *
 * @param mob  Map-object.
 *
 * @see Mobj_IsLinked(), Mobj_SetOrigin()
 */
LIBDOOMSDAY_PUBLIC world::BspLeaf &Mobj_BspLeafAtOrigin(const mobj_t &mob);

/**
 * Returns @c true if the map-object has been linked into the map. The only time this is
 * not true is if @ref Mobj_SetOrigin() has not yet been called.
 *
 * @param mob  Map-object.
 *
 * @todo Automatically link all new mobjs into the map (making this redundant).
 */
LIBDOOMSDAY_PUBLIC bool Mobj_IsLinked(const mobj_t &mob);

/**
 * Returns @c true if the map-object is physically inside (and @em presently linked to)
 * some Sector of the owning Map.
 */
LIBDOOMSDAY_PUBLIC bool Mobj_IsSectorLinked(const mobj_t &mob);

/**
 * Returns the map in which the map-object exists. Note that a map-object may exist in a
 * map while not being @em linked into data structures such as the blockmap and sectors.
 * To determine whether the map-object is linked, call @ref Mobj_IsLinked().
 *
 * @see Thinker_Map()
 */
LIBDOOMSDAY_PUBLIC world::Map &Mobj_Map(const mobj_t &mob);

#endif // __cplusplus

/**
 * Returns the sector attributed to the BSP leaf in which the mobj's origin
 * currently falls. If the mobj is not yet linked then @c 0 is returned.
 *
 * Note: The mobj is necessarily within the bounds of the sector!
 *
 * @param mobj  Mobj instance.
 */
DE_EXTERN_C LIBDOOMSDAY_PUBLIC world_Sector *Mobj_Sector(const mobj_t *mobj);

#endif // LIBDOOMSDAY_MOBJ_H
