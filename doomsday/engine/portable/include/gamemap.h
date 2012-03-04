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
const Uri* GameMap_Uri(GameMap* map);

/// @return  The old 'unique' identifier of the map.
const char* GameMap_OldUniqueId(GameMap* map);

void GameMap_Bounds(GameMap* map, float* min, float* max);

/**
 * Retrieve the map-global ambient light level.
 *
 * @param map  GameMap instance.
 * @return  Ambient light level.
 */
int GameMap_AmbientLightLevel(GameMap* map);

/**
 * Lookup a Vertex by its unique index.
 *
 * @param map  GameMap instance.
 * @param idx  Unique index of the vertex.
 * @return  Pointer to Vertex with this index else @c NULL if @a idx is not valid.
 */
vertex_t* GameMap_Vertex(GameMap* map, uint idx);

/**
 * Lookup a LineDef by its unique index.
 *
 * @param map  GameMap instance.
 * @param idx  Unique index of the linedef.
 * @return  Pointer to LineDef with this index else @c NULL if @a idx is not valid.
 */
linedef_t* GameMap_LineDef(GameMap* map, uint idx);

/**
 * Lookup a SideDef by its unique index.
 *
 * @param map  GameMap instance.
 * @param idx  Unique index of the sidedef.
 * @return  Pointer to SideDef with this index else @c NULL if @a idx is not valid.
 */
sidedef_t* GameMap_SideDef(GameMap* map, uint idx);

/**
 * Lookup a Sector by its unique index.
 *
 * @param map  GameMap instance.
 * @param idx  Unique index of the sector.
 * @return  Pointer to Sector with this index else @c NULL if @a idx is not valid.
 */
sector_t* GameMap_Sector(GameMap* map, uint idx);

/**
 * Lookup a Subsector by its unique index.
 *
 * @param map  GameMap instance.
 * @param idx  Unique index of the subsector.
 * @return  Pointer to Subsector with this index else @c NULL if @a idx is not valid.
 */
subsector_t* GameMap_Subsector(GameMap* map, uint idx);

/**
 * Lookup a HEdge by its unique index.
 *
 * @param map  GameMap instance.
 * @param idx  Unique index of the hedge.
 * @return  Pointer to HEdge with this index else @c NULL if @a idx is not valid.
 */
HEdge* GameMap_HEdge(GameMap* map, uint idx);

/**
 * Lookup a Node by its unique index.
 *
 * @param map  GameMap instance.
 * @param idx  Unique index of the node.
 * @return  Pointer to Node with this index else @c NULL if @a idx is not valid.
 */
node_t* GameMap_Node(GameMap* map, uint idx);

/**
 * Lookup the unique index for @a vertex.
 *
 * @param map  GameMap instance.
 * @param vtx  Vertex to lookup.
 * @return  Unique index for the Vertex else @c -1 if not present.
 */
int GameMap_VertexIndex(GameMap* map, vertex_t* vtx);

/**
 * Lookup the unique index for @a lineDef.
 *
 * @param map  GameMap instance.
 * @param line  LineDef to lookup.
 * @return  Unique index for the LineDef else @c -1 if not present.
 */
int GameMap_LineDefIndex(GameMap* map, linedef_t* line);

/**
 * Lookup the unique index for @a sideDef.
 *
 * @param map  GameMap instance.
 * @param side  SideDef to lookup.
 * @return  Unique index for the SideDef else @c -1 if not present.
 */
int GameMap_SideDefIndex(GameMap* map, sidedef_t* side);

/**
 * Lookup the unique index for @a sector.
 *
 * @param map  GameMap instance.
 * @param sector  Sector to lookup.
 * @return  Unique index for the Sector else @c -1 if not present.
 */
int GameMap_SectorIndex(GameMap* map, sector_t* sector);

/**
 * Lookup the unique index for @a subsector.
 *
 * @param map  GameMap instance.
 * @param subsector  Subsector to lookup.
 * @return  Unique index for the Subsector else @c -1 if not present.
 */
int GameMap_SubsectorIndex(GameMap* map, subsector_t* subsector);

/**
 * Lookup the unique index for @a hedge.
 *
 * @param map  GameMap instance.
 * @param hedge  HEdge to lookup.
 * @return  Unique index for the HEdge else @c -1 if not present.
 */
int GameMap_HEdgeIndex(GameMap* map, HEdge* hedge);

/**
 * Lookup the unique index for @a node.
 *
 * @param map  GameMap instance.
 * @param node  Node to lookup.
 * @return  Unique index for the Node else @c -1 if not present.
 */
int GameMap_NodeIndex(GameMap* map, node_t* node);

/**
 * Retrieve the number of Vertex instances owned by this.
 *
 * @param map  GameMap instance.
 * @return  Number Vertexes.
 */
uint GameMap_VertexCount(GameMap* map);

/**
 * Retrieve the number of LineDef instances owned by this.
 *
 * @param map  GameMap instance.
 * @return  Number LineDef.
 */
uint GameMap_LineDefCount(GameMap* map);

/**
 * Retrieve the number of SideDef instances owned by this.
 *
 * @param map  GameMap instance.
 * @return  Number SideDef.
 */
uint GameMap_SideDefCount(GameMap* map);

/**
 * Retrieve the number of Sector instances owned by this.
 *
 * @param map  GameMap instance.
 * @return  Number Sector.
 */
uint GameMap_SectorCount(GameMap* map);

/**
 * Retrieve the number of Subsector instances owned by this.
 *
 * @param map  GameMap instance.
 * @return  Number Subsector.
 */
uint GameMap_SubsectorCount(GameMap* map);

/**
 * Retrieve the number of HEdge instances owned by this.
 *
 * @param map  GameMap instance.
 * @return  Number HEdge.
 */
uint GameMap_HEdgeCount(GameMap* map);

/**
 * Retrieve the number of Node instances owned by this.
 *
 * @param map  GameMap instance.
 * @return  Number Node.
 */
uint GameMap_NodeCount(GameMap* map);

/**
 * Retrieve the number of Polyobj instances owned by this.
 *
 * @param map  GameMap instance.
 * @return  Number Polyobj.
 */
uint GameMap_PolyobjCount(GameMap* map);

/**
 * Lookup a Polyobj in the map by unique ID.
 *
 * @param map  GameMap instance.
 * @param id  Unique identifier of the Polyobj to be found.
 * @return  Found Polyobj instance else @c NULL.
 */
polyobj_t* GameMap_PolyobjByID(GameMap* map, uint id);

/**
 * Lookup a Polyobj in the map by tag.
 *
 * @param map  GameMap instance.
 * @param tag  Tag associated with the Polyobj to be found.
 * @return  Found Polyobj instance else @c NULL.
 */
polyobj_t* GameMap_PolyobjByTag(GameMap* map, int tag);

/**
 * Lookup a Polyobj in the map by origin.
 *
 * @param map  GameMap instance.
 * @param tag  Tag associated with the Polyobj to be found.
 * @return  Found Polyobj instance else @c NULL.
 */
polyobj_t* GameMap_PolyobjByOrigin(GameMap* map, void* ddMobjBase);

/**
 * Initialize all Polyobjs in the map. To be called after map load.
 *
 * @param map  GameMap instance.
 */
void GameMap_InitPolyobjs(GameMap* map);

/**
 * Initialize the node piles and link rings. To be called after map load.
 *
 * @param map  GameMap instance.
 */
void GameMap_InitNodePiles(GameMap* map);

/**
 * Construct an initial (empty) Mobj Blockmap for this map.
 *
 * @param min  Minimal coordinates for the map.
 * @param max  Maximal coordinates for the map.
 */
void GameMap_InitMobjBlockmap(GameMap* map, const_pvec2_t min, const_pvec2_t max);

void GameMap_LinkMobjInBlockmap(GameMap* map, struct mobj_s* mo);
boolean GameMap_UnlinkMobjInBlockmap(GameMap* map, struct mobj_s* mo);

int GameMap_IterateCellMobjs(GameMap* map, const uint coords[2],
    int (*callback) (struct mobj_s*, void*), void* paramaters);
int GameMap_IterateCellBlockMobjs(GameMap* map, const GridmapBlock* blockCoords,
    int (*callback) (struct mobj_s*, void*), void* paramaters);

/**
 * Construct an initial (empty) LineDef Blockmap for this map.
 *
 * @param min  Minimal coordinates for the map.
 * @param max  Maximal coordinates for the map.
 */
void GameMap_InitLineDefBlockmap(GameMap* map, const_pvec2_t min, const_pvec2_t max);

void GameMap_LinkLineDefInBlockmap(GameMap* map, linedef_t* lineDef);

int GameMap_IterateCellLineDefs(GameMap* map, const uint coords[2],
    int (*callback) (linedef_t*, void*), void* paramaters);
int GameMap_IterateCellBlockLineDefs(GameMap* map, const GridmapBlock* blockCoords,
    int (*callback) (linedef_t*, void*), void* paramaters);

/**
 * Construct an initial (empty) Subsector Blockmap for this map.
 *
 * @param min  Minimal coordinates for the map.
 * @param max  Maximal coordinates for the map.
 */
void GameMap_InitSubsectorBlockmap(GameMap* map, const_pvec2_t min, const_pvec2_t max);

void GameMap_LinkSubsectorInBlockmap(GameMap* map, subsector_t* subsector);

int GameMap_IterateCellSubsectors(GameMap* map, const uint coords[2],
    sector_t* sector, const AABoxf* box, int localValidCount,
    int (*callback) (subsector_t*, void*), void* paramaters);
int GameMap_IterateCellBlockSubsectors(GameMap* map, const GridmapBlock* blockCoords,
    sector_t* sector, const AABoxf* box, int localValidCount,
    int (*callback) (subsector_t*, void*), void* paramaters);

/**
 * Construct an initial (empty) Polyobj Blockmap for this map.
 *
 * @param min  Minimal coordinates for the map.
 * @param max  Maximal coordinates for the map.
 */
void GameMap_InitPolyobjBlockmap(GameMap* map, const_pvec2_t min, const_pvec2_t max);

void GameMap_LinkPolyobjInBlockmap(GameMap* map, polyobj_t* po);
void  GameMap_UnlinkPolyobjInBlockmap(GameMap* map, polyobj_t* po);

int GameMap_IterateCellPolyobjs(GameMap* map, const uint coords[2],
    int (*callback) (polyobj_t*, void*), void* paramaters);
int GameMap_IterateCellBlockPolyobjs(GameMap* map, const GridmapBlock* blockCoords,
    int (*callback) (polyobj_t*, void*), void* paramaters);

int GameMap_IterateCellPolyobjLineDefs(GameMap* map, const uint coords[2],
    int (*callback) (linedef_t*, void*), void* paramaters);
int GameMap_IterateCellBlockPolyobjLineDefs(GameMap* map, const GridmapBlock* blockCoords,
    int (*callback) (linedef_t*, void*), void* paramaters);

#endif /// LIBDENG_GAMEMAP_H
