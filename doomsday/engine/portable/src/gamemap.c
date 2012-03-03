/**
 * @file gamemap.c
 * Gamemap. @ingroup map
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_system.h"

#include "gamemap.h"

const Uri* GameMap_Uri(GameMap* map)
{
    assert(map);
    return map->uri;
}

const char* GameMap_OldUniqueId(GameMap* map)
{
    assert(map);
    return map->uniqueId;
}

void GameMap_Bounds(GameMap* map, float* min, float* max)
{
    assert(map);

    min[VX] = map->bBox[BOXLEFT];
    min[VY] = map->bBox[BOXBOTTOM];

    max[VX] = map->bBox[BOXRIGHT];
    max[VY] = map->bBox[BOXTOP];
}

int GameMap_AmbientLightLevel(GameMap* map)
{
    assert(map);
    return map->ambientLightLevel;
}

vertex_t* GameMap_Vertex(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numVertexes) return NULL;
    return &map->vertexes[idx];
}

int GameMap_VertexIndex(GameMap* map, vertex_t* vtx)
{
    assert(map);
    if(!vtx || !(vtx >= map->vertexes && vtx <= &map->vertexes[map->numVertexes])) return -1;
    return vtx - map->vertexes;
}

int GameMap_LineDefIndex(GameMap* map, linedef_t* line)
{
    assert(map);
    if(!line || !(line >= map->lineDefs && line <= &map->lineDefs[map->numLineDefs])) return -1;
    return line - map->lineDefs;
}

linedef_t* GameMap_LineDef(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numLineDefs) return NULL;
    return &map->lineDefs[idx];
}

int GameMap_SideDefIndex(GameMap* map, sidedef_t* side)
{
    assert(map);
    if(!side || !(side >= map->sideDefs && side <= &map->sideDefs[map->numSideDefs])) return -1;
    return side - map->sideDefs;
}

sidedef_t* GameMap_SideDef(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numSideDefs) return NULL;
    return &map->sideDefs[idx];
}

int GameMap_SectorIndex(GameMap* map, sector_t* sec)
{
    assert(map);
    if(!sec || !(sec >= map->sectors && sec <= &map->sectors[map->numSectors])) return -1;
    return sec - map->sectors;
}

sector_t* GameMap_Sector(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numSectors) return NULL;
    return &map->sectors[idx];
}

int GameMap_SubsectorIndex(GameMap* map, subsector_t* ssec)
{
    assert(map);
    if(!ssec || !(ssec >= map->subsectors && ssec <= &map->subsectors[map->numSubsectors])) return -1;
    return ssec - map->subsectors;
}

subsector_t* GameMap_Subsector(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numSubsectors) return NULL;
    return &map->subsectors[idx];
}

int GameMap_HEdgeIndex(GameMap* map, HEdge* hedge)
{
    assert(map);
    if(!hedge || !(hedge >= map->hedges && hedge <= &map->hedges[map->numHEdges])) return -1;
    return hedge - map->hedges;
}

HEdge* GameMap_HEdge(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numHEdges) return NULL;
    return &map->hedges[idx];
}

int GameMap_NodeIndex(GameMap* map, node_t* node)
{
    assert(map);
    if(!node || !(node >= map->nodes && node <= &map->nodes[map->numNodes])) return -1;
    return node - map->nodes;
}

node_t* GameMap_Node(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numNodes) return NULL;
    return &map->nodes[idx];
}

void GameMap_InitNodePiles(GameMap* map)
{
    uint i, starttime = 0;

    assert(map);

    VERBOSE( Con_Message("GameMap::InitNodePiles: Initializing...\n") )
    VERBOSE2( starttime = Sys_GetRealTime() )

    // Initialize node piles and line rings.
    NP_Init(&map->mobjNodes, 256);  // Allocate a small pile.
    NP_Init(&map->lineNodes, map->numLineDefs + 1000);

    // Allocate the rings.
    map->lineLinks = Z_Malloc(sizeof(*map->lineLinks) * map->numLineDefs, PU_MAPSTATIC, 0);

    for(i = 0; i < map->numLineDefs; ++i)
    {
        map->lineLinks[i] = NP_New(&map->lineNodes, NP_ROOT_NODE);
    }

    // How much time did we spend?
    VERBOSE2( Con_Message("  Done in %.2f seconds.\n", (Sys_GetRealTime() - starttime) / 1000.0f) )
}
