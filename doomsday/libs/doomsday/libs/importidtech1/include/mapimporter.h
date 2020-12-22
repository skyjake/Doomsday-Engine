/** @file mapimporter.h  Resource importer for id Tech 1 format maps.
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

#ifndef IMPORTIDTECH1_MAPIMPORTER_H
#define IMPORTIDTECH1_MAPIMPORTER_H

#include "dd_types.h"                   // lumpnum_t
#include <doomsday/filesys/file.h>
#include <doomsday/filesys/lumpindex.h>
#include <doomsday/uri.h>
#include <de/libcore.h>
#include <de/error.h>
#include <de/string.h>
#include <de/stringpool.h>

namespace idtech1 {

class MaterialDict;

/// Material group identifiers.
enum MaterialGroup {
    PlaneMaterials,
    WallMaterials
};

typedef de::StringPool::Id MaterialId;

/**
 * Resource importer for id Tech 1 format maps.
 *
 * @ingroup idtech1converter
 */
class MapImporter
{
public:
    /// Base class for load-related errors. @ingroup errors
    DE_ERROR(LoadError);

public:
    /**
     * Attempt to construct a new Id1Map from the @a recognized data specified.
     */
    MapImporter(const res::Id1MapRecognizer &recognized);

    /**
     * Transfer the map to Doomsday (i.e., rebuild in native map format via the
     * public MapEdit API).
     */
    void transfer();

    /**
     * Convert a textual material @a name to an internal material dictionary id.
     */
    MaterialId toMaterialId(de::String name, MaterialGroup group);

    /**
     * Convert a Doom64 style unique material @a number to an internal dictionary id.
     */
    MaterialId toMaterialId(int number, MaterialGroup group);

private:
    DE_PRIVATE(d)
};

} // namespace idtech1

#endif // IMPORTIDTECH1_MAPIMPORTER_H
