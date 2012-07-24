/**
 * @file p_mapdata.h
 * Playsim Data Structures, Macros and Constants. @ingroup play
 *
 * These are internal to Doomsday. The games have no direct access to this data.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_PLAY_MAPDATA_H
#define LIBDENG_PLAY_MAPDATA_H

#if defined(__JDOOM__) || defined(__JHERETIC__) || defined(__JHEXEN__)
#  error "Attempted to include internal Doomsday p_mapdata.h from a game"
#endif

#include "dd_share.h"
#include "dam_main.h"
#include "rend_bias.h"
#include "m_nodepile.h"
#include "m_vector.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GET_VERTEX_IDX(vtx)     GameMap_VertexIndex(theMap, vtx)
#define GET_LINE_IDX(li)        GameMap_LineDefIndex(theMap, li)
#define GET_SIDE_IDX(si)        GameMap_SideDefIndex(theMap, si)
#define GET_SECTOR_IDX(sec)     GameMap_SectorIndex(theMap, sec)
#define GET_HEDGE_IDX(he)       GameMap_HEdgeIndex(theMap, he)
#define GET_BSPLEAF_IDX(bl)     GameMap_BspLeafIndex(theMap, bl)
#define GET_BSPNODE_IDX(nd)     GameMap_BspNodeIndex(theMap, nd)

#define VERTEX_PTR(idx)         GameMap_Vertex(theMap, idx)
#define LINE_PTR(idx)           GameMap_LineDef(theMap, idx)
#define SIDE_PTR(idx)           GameMap_SideDef(theMap, idx)
#define SECTOR_PTR(idx)         GameMap_Sector(theMap, idx)
#define HEDGE_PTR(idx)          GameMap_HEdge(theMap, idx)
#define BSPLEAF_PTR(idx)        GameMap_BspLeaf(theMap, idx)
#define BSPNODE_PTR(idx)        GameMap_BspNode(theMap, idx)

#define NUM_VERTEXES            GameMap_VertexCount(theMap)
#define NUM_LINEDEFS            GameMap_LineDefCount(theMap)
#define NUM_SIDEDEFS            GameMap_SideDefCount(theMap)
#define NUM_SECTORS             GameMap_SectorCount(theMap)
#define NUM_HEDGES              GameMap_HEdgeCount(theMap)
#define NUM_BSPLEAFS            GameMap_BspLeafCount(theMap)
#define NUM_BSPNODES            GameMap_BspNodeCount(theMap)

#define NUM_POLYOBJS            GameMap_PolyobjCount(theMap)

// Runtime map data objects, such as vertices, sectors, and BspLeafs all
// have this header as their first member. This makes it possible to treat
// an unknown map data pointer as a runtime_mapdata_header_t* and determine
// its type. Note that this information is internal to the engine.
typedef struct runtime_mapdata_header_s {
    int             type; // One of the DMU type constants.
} runtime_mapdata_header_t;

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

typedef struct planelist_s {
    uint            num, maxNum;
    struct plane_s** array;
} planelist_t;

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

#include "polyobj.h"
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

extern Uri* mapUri;

/**
 * The map data arrays are accessible globally inside the engine.
 */
extern Vertex* vertexes;
extern SideDef* sideDefs;
extern LineDef* lineDefs;
extern Sector* sectors;
extern Polyobj** polyObjs; ///< List of all polyobjs on the current map.

extern BspNode** bspNodes;
extern BspLeaf** bspLeafs;
extern HEdge** hedges;

#include "gamemap.h"

// The current map.
extern GameMap* theMap;

/**
 * Change the global "current" map.
 */
void P_SetCurrentMap(GameMap* map);

/**
 * Generate a 'unique' identifier for the map.  This identifier
 * contains information about the map tag (E3M3), the WAD that
 * contains the map (DOOM.IWAD), and the game mode (doom-ultimate).
 *
 * The entire ID string will be in lowercase letters.
 */
const char* P_GenerateUniqueMapId(const char* mapId);

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

void P_RegisterMissingMaterial(const char* materialUri);

/**
 * Announce any missing materials we came across when loading the map.
 */
void P_PrintMissingMaterialList(void);

/**
 * Clear the missing material list.
 */
void P_ClearMissingMaterialList(void);

/**
 * To be called to initialize the game map object defs.
 */
void P_InitGameMapObjDefs(void);

/**
 * To be called to free all memory allocated for the map obj defs.
 */
void P_ShutdownGameMapObjDefs(void);

void P_InitGameMapObjDefs(void);
void P_ShutdownGameMapObjDefs(void);

/**
 * Called by the game to register the map object types it wishes us to make
 * public via the MPE interface.
 */
boolean P_RegisterMapObj(int identifier, const char* name);

/**
 * Called by the game to add a new property to a previously registered
 * map object type definition.
 */
boolean P_RegisterMapObjProperty(int identifier, int propIdentifier,
    const char* propName, valuetype_t type);

/**
 * Look up a mapobj definition.
 *
 * @param identifer     If objName is @c NULL, compare using this unique identifier.
 * @param objName       If not @c NULL, compare using this unique name.
 */
gamemapobjdef_t* P_GetGameMapObjDef(int identifier, const char *objName, boolean canCreate);

/**
 * Destroy the given game map obj database.
 */
void P_DestroyGameMapObjDB(gameobjdata_t* moData);

void P_AddGameMapObjValue(gameobjdata_t* moData, gamemapobjdef_t* gmoDef, uint propIdx,
    uint elmIdx, valuetype_t type, void* data);

gamemapobj_t* P_GetGameMapObj(gameobjdata_t* moData, gamemapobjdef_t* def, uint elmIdx, boolean canCreate);

uint P_CountGameMapObjs(int identifier);
byte P_GetGMOByte(int identifier, uint elmIdx, int propIdentifier);
short P_GetGMOShort(int identifier, uint elmIdx, int propIdentifier);
int P_GetGMOInt(int identifier, uint elmIdx, int propIdentifier);
fixed_t P_GetGMOFixed(int identifier, uint elmIdx, int propIdentifier);
angle_t P_GetGMOAngle(int identifier, uint elmIdx, int propIdentifier);
float P_GetGMOFloat(int identifier, uint elmIdx, int propIdentifier);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_PLAY_MAPDATA_H
