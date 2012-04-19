/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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

#include "p_mapdata.h"
#include "binarytree.h"
#include "materials.h"

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

    // The following is for game-specific map object data.
    gameobjdata_t gameObjData;
} editmap_t;

//extern editmap_t editMap;

boolean         MPE_Begin(const char* mapUri);
boolean         MPE_End(void);

/**
 * Create a new vertex in currently loaded editable map.
 *
 * @param x  X map space coordinate of the new vertex.
 * @param y  Y map space coordinate of the new vertex.
 *
 * @return  Index number of the newly created vertex else @c =0 if the vertex
 *          could not be created for some reason.
 */
uint MPE_VertexCreate(coord_t x, coord_t y);

/**
 * Create many new vertexs in the currently loaded editable map.
 *
 * @param num  Number of vertexes to be created.
 * @param values  Array containing the coordinates for all vertexes to be
 *                created [v0:X, vo:Y, v1:X, v1:Y, ..]
 * @param indices  If not @c =NULL, the indices of the newly created vertexes
 *                 will be written back here.
 * @return  @c =true iff all vertexes were created successfully.
 */
boolean MPE_VertexCreatev(size_t num, coord_t* values, uint* indices);

uint            MPE_SidedefCreate(uint sector, short flags,
                                  materialid_t topMaterial,
                                  float topOffsetX, float topOffsetY, float topRed,
                                  float topGreen, float topBlue,
                                  materialid_t middleMaterial,
                                  float middleOffsetX, float middleOffsetY,
                                  float middleRed, float middleGreen,
                                  float middleBlue, float middleAlpha,
                                  materialid_t bottomMaterial,
                                  float bottomOffsetX, float bottomOffsetY,
                                  float bottomRed, float bottomGreen,
                                  float bottomBlue);
uint            MPE_LinedefCreate(uint v1, uint v2, uint frontSide, uint backSide,
                                  int flags);
uint            MPE_SectorCreate(float lightlevel, float red, float green, float blue);
uint            MPE_PlaneCreate(uint sector, coord_t height,
                                materialid_t material,
                                float matOffsetX, float matOffsetY,
                                float r, float g, float b, float a,
                                float normalX, float normalY, float normalZ);
uint            MPE_PolyobjCreate(uint *lines, uint linecount,
                                  int tag, int sequenceType, coord_t originX, coord_t originY);

boolean         MPE_GameObjProperty(const char *objName, uint idx,
                                    const char *propName, valuetype_t type,
                                    void *data);

// Non-public (temporary)
// Flags for MPE_PruneRedundantMapData().
#define PRUNE_LINEDEFS      0x1
#define PRUNE_VERTEXES      0x2
#define PRUNE_SIDEDEFS      0x4
#define PRUNE_SECTORS       0x8
#define PRUNE_ALL           (PRUNE_LINEDEFS|PRUNE_VERTEXES|PRUNE_SIDEDEFS|PRUNE_SECTORS)

void            MPE_PruneRedundantMapData(editmap_t* map, int flags);

GameMap*        MPE_GetLastBuiltMap(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
