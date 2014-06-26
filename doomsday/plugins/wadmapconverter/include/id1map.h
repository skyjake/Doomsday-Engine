/** @file id1map.h  id Tech 1 map format reader/interpreter.
 *
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef WADMAPCONVERTER_ID1MAP_H
#define WADMAPCONVERTER_ID1MAP_H

#include "id1map_util.h"   // MapLumpType
#include "dd_types.h"      // lumpnum_t
#include <doomsday/uri.h>
#include <de/libcore.h>
#include <de/Error>
#include <de/String>
#include <de/StringPool>
#include <QMap>

namespace wadimp {

/**
 * Map resource converter/interpreter for id Tech 1 map format(s).
 *
 * @ingroup wadmapconverter
 */
class Id1Map
{
public:
    /// Base class for load-related errors. @ingroup errors
    DENG2_ERROR(LoadError);

    /// Logical map format identifiers.
    enum Format {
        UnknownFormat = -1,

        DoomFormat,
        HexenFormat,
        Doom64Format,

        MapFormatCount
    };

    /// Material group identifiers.
    enum MaterialGroup {
        PlaneMaterials,
        WallMaterials
    };

    typedef de::StringPool::Id MaterialId;

public:
    /**
     * Construct a new Id1Map of the specified @a format.
     */
    Id1Map(Format format);

    /**
     * Returns the unique format identifier for the map.
     */
    Format format() const;

    /**
     * Attempt to load a new map data set from the identified @a lumps.
     *
     * @param lumps  Map of lumps for each data type to be processed.
     */
    void load(QMap<MapLumpType, lumpnum_t> const &lumps);

    /**
     * Transfer the map to Doomsday (i.e., rebuild in native map format via the
     * public MapEdit API).
     */
    void transfer(de::Uri const &uri);

    /**
     * Convert a textual material @a name to an internal material dictionary id.
     */
    MaterialId toMaterialId(de::String name, MaterialGroup group);

    /**
     * Convert a Doom64 style unique material @a number to an internal dictionary id.
     */
    MaterialId toMaterialId(de::dint number, MaterialGroup group);

public:
    /**
     * Returns the textual name for the identified map format @a id.
     */
    static de::String const &formatName(Format id);

    /**
     * Determine the size (in bytes) of an element of the specified map data
     * lump @a type for the current map format.
     *
     * @param mapFormat     Map format identifier.
     * @param type          Map lump data type.
     * @return Size of an element of the specified type.
     *
     * @todo Should not be public.
     */
    static de::dsize ElementSizeForMapLumpType(Id1Map::Format mapFormat, MapLumpType type);

private:
    DENG2_PRIVATE(d)
};

} // namespace wadimp

#endif // WADMAPCONVERTER_ID1MAP_H
