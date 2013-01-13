/**
 * @file p_mapdata.h
 * Playsim Data Structures, Macros and Constants. @ingroup play
 *
 * These are internal to Doomsday. The games have no direct access to this data.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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
#  error Attempted to include internal Doomsday p_mapdata.h from a game
#endif

#include "dd_share.h"
#include "api_mapedit.h"
#include "map/dam_main.h"
#include "m_nodepile.h"
#include <de/binangle.h>
#include <de/vector1.h>

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

typedef struct surfacelistnode_s {
    void*           data;
    struct surfacelistnode_s* next;
} surfacelistnode_t;

typedef struct surfacelist_s {
    uint            num;
    surfacelistnode_t* head;
} surfacelist_t;

// Map entity definitions.
struct mapentitydef_s;

typedef struct mapentitypropertydef_s {
    /// Entity-unique identifier associated with this property.
    int id;

    /// Entity-unique name for this property.
    char* name;

    /// Value type identifier for this property.
    valuetype_t type;

    /// Entity definition which owns this property.
    struct mapentitydef_s* entity;
} MapEntityPropertyDef;

typedef struct mapentitydef_s {
    /// Unique identifier associated with this entity.
    int id;

    /// Set of known properties for this entity.
    uint numProps;
    MapEntityPropertyDef* props;

#ifdef __cplusplus
    mapentitydef_s(int _id) : id(_id), numProps(0), props(0) {}
#endif
} MapEntityDef;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Lookup a defined property by identifier.
 *
 * @param def           MapEntityDef instance.
 * @param propertyId    Entity-unique identifier for the property to lookup.
 * @param retDef        If not @c NULL, the found property definition is written here (else @c 0 if not found).
 *
 * @return Logical index of the found property (zero-based) else @c -1 if not found.
 */
int MapEntityDef_Property2(MapEntityDef* def, int propertyId, MapEntityPropertyDef** retDef);
int MapEntityDef_Property(MapEntityDef* def, int propertyId/*,MapEntityPropertyDef** retDef = NULL*/);

/**
 * Lookup a defined property by name.
 *
 * @param def           MapEntityDef instance.
 * @param propertyName  Entity-unique name for the property to lookup.
 * @param retDef        If not @c NULL, the found property definition is written here (else @c 0 if not found).
 *
 * @return Logical index of the found property (zero-based) else @c -1 if not found.
 */
int MapEntityDef_PropertyByName2(MapEntityDef* def, const char* propertyName, MapEntityPropertyDef** retDef);
int MapEntityDef_PropertyByName(MapEntityDef* def, const char* propertyName/*,MapEntityPropertyDef** retDef = NULL*/);

/**
 * Lookup a MapEntityDef by unique identfier @a id.
 *
 * @note Performance is O(log n).
 *
 * @return Found MapEntityDef else @c NULL.
 */
MapEntityDef* P_MapEntityDef(int id);

/**
 * Lookup a MapEntityDef by unique name.
 *
 * @note Performance is O(log n).
 *
 * @return Found MapEntityDef else @c NULL.
 */
MapEntityDef* P_MapEntityDefByName(char const* name);

/**
 * Lookup the unique name associated with MapEntityDef @a def.
 *
 * @note Performance is O(n).
 *
 * @return Unique name associated with @a def if found, else a zero-length string.
 */
AutoStr* P_NameForMapEntityDef(MapEntityDef* def);

#ifdef __cplusplus
} // extern "C"
#endif

#include "../EntityDatabase"

#ifdef __cplusplus
extern "C" {
#endif

boolean P_SetMapEntityProperty(EntityDatabase* db, MapEntityPropertyDef* propertyDef, uint elementIndex, valuetype_t valueType, void* valueAdr);

/*
byte P_GetGMOByte(int entityId, uint elementIndex, int propertyId);
short P_GetGMOShort(int entityId, uint elementIndex, int propertyId);
int P_GetGMOInt(int entityId, uint elementIndex, int propertyId);
fixed_t P_GetGMOFixed(int entityId, uint elementIndex, int propertyId);
angle_t P_GetGMOAngle(int entityId, uint elementIndex, int propertyId);
float P_GetGMOFloat(int entityId, uint elementIndex, int propertyId);
*/

extern Uri* mapUri;

/**
 * To be called to initialize the game map object defs.
 */
void P_InitMapEntityDefs(void);

/**
 * To be called to free all memory allocated for the map obj defs.
 */
void P_ShutdownMapEntityDefs(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_PLAY_MAPDATA_H
