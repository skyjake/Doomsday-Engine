/** @file id1map_util.h  Miscellaneous map converter utility routines.
 *
 * @ingroup wadmapconverter
 *
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef WADMAPCONVERTER_ID1MAP_UTIL
#define WADMAPCONVERTER_ID1MAP_UTIL

#include "doomsday.h"
#include "dd_types.h"
#include "id1map.h"
#include "maplumpinfo.h"

/**
 * Retrieve the textual name for the identified map format @a id.
 * @param id  Unique identifier of the map format.
 * @return Textual name for this format. Always returns a valid ddstring_t that
 *         should NOT be free'd.
 */
Str const *MapFormatNameForId(MapFormatId id);

/**
 * Determine type of a named map data lump.
 * @param name  Name of the data lump.
 * @return MapLumpType associated with the named map data lump.
 */
MapLumpType MapLumpTypeForName(char const *name);

#endif // WADMAPCONVERTER_ID1MAP_UTIL_H
