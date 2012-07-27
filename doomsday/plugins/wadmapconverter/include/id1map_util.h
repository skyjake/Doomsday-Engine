/**
 * @file id1map_util.h @ingroup wadmapconverter
 *
 * Miscellaneous map converter utility routines.
 *
 * @authors Copyright &copy; 2007-2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef __WADMAPCONVERTER_ID1MAP_UTIL_H__
#define __WADMAPCONVERTER_ID1MAP_UTIL_H__

#include "doomsday.h"
#include "dd_types.h"
#include "maplumpinfo.h"

/**
 * Logical map format identifier (unique).
 */
typedef enum {
    MF_UNKNOWN              = -1,
    MF_DOOM                 = 0,
    MF_HEXEN,
    MF_DOOM64,
    NUM_MAPFORMATS
} mapformatid_t;

/**
 * Helper macro for determining whether a value can be interpreted as a logical
 * map format identifier (@see mapformatid_t).
 */
#define VALID_MAPFORMATID(v)        ((v) >= MF_DOOM && (v) < NUM_MAPFORMATS)

/**
 * Retrieve the textual name for the identified map format @a id.
 * @param id  Unique identifier of the map format.
 * @return Textual name for this format. Always returns a valid ddstring_t that
 *         should NOT be free'd.
 */
const ddstring_t* MapFormatNameForId(mapformatid_t id);

/**
 * Determine type of a named map data lump.
 * @param name  Name of the data lump.
 * @return MapLumpType associated with the named map data lump.
 */
MapLumpType MapLumpTypeForName(const char* name);

#endif /* __WADMAPCONVERTER_ID1MAP_UTIL_H__ */
