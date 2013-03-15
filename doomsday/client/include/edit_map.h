/** @file edit_map.h: Runtime map editing.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDENG_MAP_EDITOR_H
#define LIBDENG_MAP_EDITOR_H

#ifndef __cplusplus
#  error "edit_map.h requires C++"
#endif

#include <vector>
#include "map/gamemap.h"
#include "Materials"

// Editable map.
/// @todo Obviously this shares functionality/data with GameMap; a common base class needed? -jk
class EditMap
{
public:
    typedef std::vector<Vertex *> Vertices;
    Vertices vertexes; // really needs to be std::vector? (not a MapElementList?)

    typedef std::vector<LineDef *> Lines;
    Lines lines;

    typedef std::vector<SideDef *> SideDefs;
    SideDefs sideDefs;

    typedef std::vector<Sector *> Sectors;
    Sectors sectors;

    uint numPolyObjs;
    Polyobj **polyObjs;

    // Game-specific map entity property values.
    EntityDatabase* entityDatabase;

public:
    EditMap();

    virtual ~EditMap();

    Vertex const **verticesAsArray() const { return const_cast<Vertex const **>(&(vertexes[0])); }

    uint vertexCount() const { return vertexes.size(); }
    uint sectorCount() const { return sectors.size(); }
};

// Non-public (temporary)
// Flags for MPE_PruneRedundantMapData().
#define PRUNE_LINEDEFS      0x1
#define PRUNE_VERTEXES      0x2
#define PRUNE_SIDEDEFS      0x4
#define PRUNE_SECTORS       0x8
#define PRUNE_ALL           (PRUNE_LINEDEFS|PRUNE_VERTEXES|PRUNE_SIDEDEFS|PRUNE_SECTORS)

void MPE_PruneRedundantMapData(EditMap *map, int flags);

GameMap *MPE_GetLastBuiltMap();
boolean MPE_GetLastBuiltMapResult();

#endif // LIBDENG_MAP_EDITOR_H
