/**
 * @file gamemap.h
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

#ifndef LIBDENG_GAMEMAP_H
#define LIBDENG_GAMEMAP_H

typedef struct gamemap_s {
    Uri* uri;
    char uniqueId[256];

    float bBox[4];

    uint numVertexes;
    vertex_t* vertexes;

    uint numHEdges;
    HEdge* hedges;

    uint numSectors;
    sector_t* sectors;

    uint numSubsectors;
    subsector_t* subsectors;

    uint numNodes;
    node_t* nodes;

    uint numLineDefs;
    linedef_t* lineDefs;

    uint numSideDefs;
    sidedef_t* sideDefs;

    uint numPolyObjs;
    polyobj_t** polyObjs;

    gameobjdata_t gameObjData;

    watchedplanelist_t watchedPlaneList;
    surfacelist_t movingSurfaceList;
    surfacelist_t decoratedSurfaceList;
    surfacelist_t glowingSurfaceList;

    Blockmap* mobjBlockmap;
    Blockmap* polyobjBlockmap;
    Blockmap* lineDefBlockmap;
    Blockmap* subsectorBlockmap;

    nodepile_t mobjNodes, lineNodes; // All kinds of wacky links.
    nodeindex_t* lineLinks; // Indices to roots.

    float globalGravity; // Gravity for the current map.
    int ambientLightLevel; // Ambient lightlevel for the current map.
} GameMap;

/**
 * This ID is the name of the lump tag that marks the beginning of map
 * data, e.g. "MAP03" or "E2M8".
 */
const Uri* P_MapUri(GameMap* map);

/// @return  The 'unique' identifier of the map.
const char* P_GetUniqueMapId(GameMap* map);

void P_GetMapBounds(GameMap* map, float* min, float* max);
int P_GetMapAmbientLightLevel(GameMap* map);

#endif /// LIBDENG_GAMEMAP_H
