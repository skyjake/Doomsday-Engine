/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * edit_map.h: Runtime map building.
 */

#ifndef __DOOMSDAY_MAP_EDITOR_H__
#define __DOOMSDAY_MAP_EDITOR_H__

#include "map/gamemap.h"
#include "resource/materials.h"

#ifdef __cplusplus
extern "C" {
#endif

// Editable map.
typedef struct editmap_s {
    uint numVertexes;
    Vertex** vertexes;
    uint numLineDefs;
    LineDef** lineDefs;
    uint numSideDefs;
    SideDef** sideDefs;
    uint numSectors;
    Sector** sectors;
    uint numPolyObjs;
    Polyobj** polyObjs;

    // Game-specific map entity property values.
    EntityDatabase* entityDatabase;
} editmap_t;

// Non-public (temporary)
// Flags for MPE_PruneRedundantMapData().
#define PRUNE_LINEDEFS      0x1
#define PRUNE_VERTEXES      0x2
#define PRUNE_SIDEDEFS      0x4
#define PRUNE_SECTORS       0x8
#define PRUNE_ALL           (PRUNE_LINEDEFS|PRUNE_VERTEXES|PRUNE_SIDEDEFS|PRUNE_SECTORS)

void            MPE_PruneRedundantMapData(editmap_t* map, int flags);

GameMap*        MPE_GetLastBuiltMap(void);
boolean         MPE_GetLastBuiltMapResult(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
