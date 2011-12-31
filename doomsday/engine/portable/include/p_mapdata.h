/**\file p_mapdata.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * Playsim Data Structures, Macros and Constants.
 *
 * These are internal to Doomsday. The games have no direct access to
 * this data.
 */

#ifndef LIBDENG_PLAY_DATA_H
#define LIBDENG_PLAY_DATA_H

#if defined(__JDOOM__) || defined(__JHERETIC__) || defined(__JHEXEN__)
#  error "Attempted to include internal Doomsday p_mapdata.h from a game"
#endif

#include "dd_share.h"
#include "dam_main.h"
#include "rend_bias.h"
#include "m_nodepile.h"
#include "m_vector.h"

#define GET_VERTEX_IDX(vtx)     ( (int) ((vtx) - vertexes) )
#define GET_LINE_IDX(li)        ( (int) ((li) - lineDefs) )
#define GET_SIDE_IDX(si)        ( (int) ((si) - sideDefs) )
#define GET_SECTOR_IDX(sec)     ( (int) ((sec) - sectors) )
#define GET_SUBSECTOR_IDX(sub)  ( (int) ((sub) - ssectors) )
#define GET_SEG_IDX(seg)        ( (int) ((seg) - segs) )
#define GET_NODE_IDX(nd)        ( (int) ((nd) - nodes) )

// Return the index of plane within a sector's planes array.
#define GET_PLANE_IDX(pln)      ( (int) ((pln) - (pln)->sector->planes[0]) )

#define VERTEX_PTR(i)       (&vertexes[i])
#define SEG_PTR(i)          (&segs[i])
#define SECTOR_PTR(i)       (&sectors[i])
#define SUBSECTOR_PTR(i)    (&ssectors[i])
#define NODE_PTR(i)         (&nodes[i])
#define LINE_PTR(i)         (&lineDefs[i])
#define SIDE_PTR(i)         (&sideDefs[i])

// Node flags.
#define NF_SUBSECTOR        0x80000000

// Runtime map data objects, such as vertices, sectors, and subsectors all
// have this header as their first member. This makes it possible to treat
// an unknown map data pointer as a runtime_mapdata_header_t* and determine
// its type. Note that this information is internal to the engine.
typedef struct runtime_mapdata_header_s {
    int             type; // One of the DMU type constants.
} runtime_mapdata_header_t;

typedef struct fvertex_s {
    float           pos[2];
} fvertex_t;

typedef struct shadowcorner_s {
    float           corner;
    struct sector_s* proximity;
    float           pOffset;
    float           pHeight;
} shadowcorner_t;

typedef struct edgespan_s {
    float           length;
    float           shift;
} edgespan_t;

typedef struct watchedplanelist_s {
    uint            num, maxNum;
    struct plane_s** list;
} watchedplanelist_t;

typedef struct surfacelistnode_s {
    void*           data;
    struct surfacelistnode_s* next;
} surfacelistnode_t;

typedef struct surfacelist_s {
    uint            num;
    surfacelistnode_t* head;
} surfacelist_t;

/**
 * Stores the data pertaining to vertex lighting for a worldmap, surface.
 */
typedef struct biassurface_s {
    uint            updated;
    uint            size;
    vertexillum_t*  illum; // [size]
    biastracker_t   tracker;
    biasaffection_t affected[MAX_BIAS_AFFECTED];

    struct biassurface_s* next;
} biassurface_t;

#include "m_gridmap.h"

struct blockmap_s; // The Blockmap instance (opaque).
typedef struct blockmap_s Blockmap;

Blockmap* Blockmap_New(const pvec2_t min, const pvec2_t max, uint cellWidth,
    uint cellHeight, size_t sizeOfCell);

/**
 * @return  "Origin" map space point for the Blockmap (minimal [x,y]).
 */
const pvec2_t Blockmap_Origin(Blockmap* blockmap);

/**
 * Retrieve the extremal map space points covered by the Blockmap.
 */
const AABoxf* Blockmap_Bounds(Blockmap* blockmap);

/// @return  Width of the Blockmap in cells.
uint Blockmap_Width(Blockmap* blockmap);

/// @return  Height of the Blockmap in cells.
uint Blockmap_Height(Blockmap* blockmap);

/**
 * Retrieve the size of the Blockmap in cells.
 *
 * @param widthHeight  Size of the Blockmap [width,height] written here.
 */
void Blockmap_Size(Blockmap* blockmap, uint widthHeight[2]);

/// @return  Width of a Blockmap cell in map space units.
float Blockmap_CellWidth(Blockmap* blockmap);

/// @return  Height of a Blockmap cell in map space units.
float Blockmap_CellHeight(Blockmap* blockmap);

/// @return  Size [width,height] of a Blockmap cell in map space units.
const pvec2_t Blockmap_CellSize(Blockmap* blockmap);

/**
 * Given map space X coordinate @a x, return the corresponding cell coordinate.
 * If @a x is outside the Blockmap it will be clamped to the nearest edge on
 * the X axis.
 */
uint Blockmap_CellX(Blockmap* blockmap, float x);

/**
 * Given map space Y coordinate @a y, return the corresponding cell coordinate.
 * If @a y is outside the Blockmap it will be clamped to the nearest edge on
 * the Y axis.
 *
 * @param outY  Blockmap cell X coordinate written here.
 * @param y  Map space X coordinate to be translated.
 * @return  @c true iff clamping was necessary.
 */
uint Blockmap_CellY(Blockmap* blockmap, float y);

/**
 * Same as @a Blockmap::CellX with alternative semantics for when the caller
 * needs to know if the coordinate specified was inside/outside the Blockmap.
 */
boolean Blockmap_ClipCellX(Blockmap* bm, uint* outX, float x);

/**
 * Same as @a Blockmap::CellY with alternative semantics for when the caller
 * needs to know if the coordinate specified was inside/outside the Blockmap.
 *
 * @param outY  Blockmap cell Y coordinate written here.
 * @param y  Map space Y coordinate to be translated.
 * @return  @c true iff clamping was necessary.
 */
boolean Blockmap_ClipCellY(Blockmap* bm, uint* outY, float y);

/**
 * Given map space XY coordinates @a pos, output the Blockmap cell[x, y] it
 * resides in. If @a pos is outside the Blockmap it will be clamped to the
 * nearest edge on one or more axes as necessary.
 *
 * @return  @c true iff clamping was necessary.
 */
boolean Blockmap_CellCoords(Blockmap* blockmap, uint coords[2], float const pos[2]);

/**
 * Given map space box XY coordinates @a box, output the blockmap cells[x, y]
 * they reside in. If any point defined by @a box lies outside the blockmap
 * it will be clamped to the nearest edge on one or more axes as necessary.
 *
 * @return  @c true iff Clamping was necessary.
 */
boolean Blockmap_CellBlockCoords(Blockmap* blockmap, GridmapBlock* blockCoords, const AABoxf* box);

#include "p_polyob.h"
#include "p_maptypes.h"

// Game-specific, map object type definitions.
typedef struct {
    int             identifier;
    char*           name;
    valuetype_t     type;
} mapobjprop_t;

typedef struct {
    int             identifier;
    char*           name;
    uint            numProps;
    mapobjprop_t*   props;
} gamemapobjdef_t;

// Map objects.
typedef struct {
    uint            idx;
    valuetype_t     type;
    uint            valueIdx;
} customproperty_t;

typedef struct {
    uint            elmIdx;
    uint            numProps;
    customproperty_t* props;
} gamemapobj_t;

typedef struct {
    uint            num;
    gamemapobjdef_t* def;
    gamemapobj_t**  objs;
} gamemapobjlist_t;

// Map value databases.
typedef struct {
    valuetype_t     type;
    uint            numElms;
    void*           data;
} valuetable_t;

typedef struct {
    uint            numTables;
    valuetable_t**  tables;
} valuedb_t;

typedef struct {
    gamemapobjlist_t* objLists;
    valuedb_t       db;
} gameobjdata_t;

/**
 * The map data arrays are accessible globally inside the engine.
 */
extern Uri* mapUri;
extern uint numVertexes;
extern vertex_t* vertexes;

extern uint numSegs;
extern seg_t* segs;

extern uint numSectors;
extern sector_t* sectors;

extern uint numSSectors;
extern subsector_t* ssectors;

extern uint numNodes;
extern node_t* nodes;

extern uint numLineDefs;
extern linedef_t* lineDefs;

extern uint numSideDefs;
extern sidedef_t* sideDefs;

extern watchedplanelist_t* watchedPlaneList;
extern surfacelist_t* movingSurfaceList;
extern surfacelist_t* decoratedSurfaceList;
extern surfacelist_t* glowingSurfaceList;

extern float mapGravity;

typedef struct gamemap_s {
    Uri* uri;
    char            uniqueId[256];

    float           bBox[4];

    uint            numVertexes;
    vertex_t*       vertexes;

    uint            numSegs;
    seg_t*          segs;

    uint            numSectors;
    sector_t*       sectors;

    uint            numSSectors;
    subsector_t*    ssectors;

    uint            numNodes;
    node_t*         nodes;

    uint            numLineDefs;
    linedef_t*      lineDefs;

    uint            numSideDefs;
    sidedef_t*      sideDefs;

    uint            numPolyObjs;
    polyobj_t**     polyObjs;

    gameobjdata_t   gameObjData;

    watchedplanelist_t watchedPlaneList;
    surfacelist_t   movingSurfaceList;
    surfacelist_t   decoratedSurfaceList;
    surfacelist_t   glowingSurfaceList;

    Blockmap*       mobjBlockmap;
    Blockmap*       polyobjBlockmap;
    Blockmap*       lineDefBlockmap;
    Blockmap*       subsectorBlockmap;

    nodepile_t      mobjNodes, lineNodes; // All kinds of wacky links.
    nodeindex_t*    lineLinks; // Indices to roots.

    float           globalGravity; // Gravity for the current map.
    int             ambientLightLevel; // Ambient lightlevel for the current map.
} gamemap_t;

gamemap_t*      P_GetCurrentMap(void);
void            P_SetCurrentMap(gamemap_t* map);

/**
 * This ID is the name of the lump tag that marks the beginning of map
 * data, e.g. "MAP03" or "E2M8".
 */
const Uri* P_MapUri(gamemap_t* map);

/// @return  The 'unique' identifier of the map.
const char* P_GetUniqueMapId(gamemap_t* map);

void            P_GetMapBounds(gamemap_t* map, float* min, float* max);
int             P_GetMapAmbientLightLevel(gamemap_t* map);

const char*     P_GenerateUniqueMapId(const char* mapId);

/**
 * Is there a known map referenced by @a uri and if so, is it available for loading?
 *
 * @param  Uri identifying the map to be searched for.
 * @return  @c true= A known and loadable map.
 */
boolean P_MapExists(const char* uri);

/**
 * Retrieve the name of the source file containing the map referenced by @a uri
 * if known and available for loading.
 *
 * @param  Uri identifying the map to be searched for.
 * @return  Fully qualified (i.e., absolute) path to the source file.
 */
const char* P_MapSourceFile(const char* uri);

/**
 * Begin the process of loading a new map.
 *
 * @param uri  Uri identifying the map to be loaded.
 * @return @c true, if the map was loaded successfully.
 */
boolean P_LoadMap(const char* uri);

void            P_PolyobjChanged(polyobj_t* po);
void            P_RegisterUnknownTexture(const char* name, boolean planeTex);
void            P_PrintMissingTextureList(void);
void            P_FreeBadTexList(void);

/// To be called to initialize the game map object defs.
void P_InitGameMapObjDefs(void);

/// To be called to free all memory allocated for the map obj defs.
void P_ShutdownGameMapObjDefs(void);

void            P_InitGameMapObjDefs(void);
void            P_ShutdownGameMapObjDefs(void);

boolean         P_RegisterMapObj(int identifier, const char* name);
boolean         P_RegisterMapObjProperty(int identifier, int propIdentifier,
                                         const char* propName, valuetype_t type);
gamemapobjdef_t* P_GetGameMapObjDef(int identifier, const char *objName,
                                    boolean canCreate);

void            P_DestroyGameMapObjDB(gameobjdata_t* moData);
void            P_AddGameMapObjValue(gameobjdata_t* moData, gamemapobjdef_t* gmoDef,
                                uint propIdx, uint elmIdx, valuetype_t type,
                                void* data);
gamemapobj_t*   P_GetGameMapObj(gameobjdata_t* moData, gamemapobjdef_t* def,
                                uint elmIdx, boolean canCreate);

uint            P_CountGameMapObjs(int identifier);
byte            P_GetGMOByte(int identifier, uint elmIdx, int propIdentifier);
short           P_GetGMOShort(int identifier, uint elmIdx, int propIdentifier);
int             P_GetGMOInt(int identifier, uint elmIdx, int propIdentifier);
fixed_t         P_GetGMOFixed(int identifier, uint elmIdx, int propIdentifier);
angle_t         P_GetGMOAngle(int identifier, uint elmIdx, int propIdentifier);
float           P_GetGMOFloat(int identifier, uint elmIdx, int propIdentifier);
#endif /* LIBDENG_PLAY_DATA_H */
