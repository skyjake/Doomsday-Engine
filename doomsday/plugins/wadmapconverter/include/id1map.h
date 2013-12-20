/** @file id1map.h  id Tech 1 map format reader.
 *
 * @authors Copyright &copy; 2007-2013 Daniel Swanson <danij@dengine.net>
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

#include "doomsday.h"
#include "dd_types.h"
#include "maplumpinfo.h"
#include <de/Error>
#include <de/String>
#include <de/StringPool>
#include <vector>
#include <list>
#include <map>

struct mline_t;
struct mpolyobj_t;
struct msector_t;
struct mside_t;
struct mthing_t;
struct surfacetint_t;

typedef std::map<MapLumpType, MapLumpInfo *> MapLumpInfos;

/**
 *
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

    typedef std::vector<mline_t> Lines;
    typedef std::vector<mside_t> Sides;
    typedef std::vector<msector_t> Sectors;
    typedef std::vector<mthing_t> Things;
    typedef std::vector<surfacetint_t> SurfaceTints;
    typedef std::list<mpolyobj_t> Polyobjs;

    typedef std::list<uint> LineList;

    /// Material group identifiers.
    enum MaterialGroup {
        PlaneMaterials,
        WallMaterials
    };

    typedef de::StringPool::Id MaterialId;

public:
    Id1Map(Format format);

    Format format() const;

    void load(MapLumpInfos &lumpInfos);

    int transfer();

    MaterialId toMaterialId(de::String name, MaterialGroup group);

    // Doom64 format maps reference materials with unique ids.
    MaterialId toMaterialId(int uniqueId, MaterialGroup group);

private:
    DENG2_PRIVATE(d)
};

#endif // WADMAPCONVERTER_ID1MAP_H
