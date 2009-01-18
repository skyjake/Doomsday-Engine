/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
#include "m_binarytree.h"
#include "p_materialmanager.h"

// Editable map.
typedef struct editmap_s {
    char            name[9];
    uint            numVertexes;
    vertex_t**      vertexes;
    uint            numLineDefs;
    linedef_t**     lineDefs;
    uint            numSideDefs;
    sidedef_t**     sideDefs;
    uint            numSectors;
    sector_t**      sectors;
    uint            numPolyObjs;
    polyobj_t**     polyObjs;

    // The following is for game-specific map object data.
    gameobjdata_t   gameObjData;
} editmap_t;

boolean         MPE_Begin(const char *name);
boolean         MPE_End(void);

uint            MPE_VertexCreate(float x, float y);
boolean         MPE_VertexCreatev(size_t num, float *values, uint *indices);
uint            MPE_SidedefCreate(uint sector, short flags,
                                  materialnum_t topMaterial,
                                  float topOffsetX, float topOffsetY, float topRed,
                                  float topGreen, float topBlue,
                                  materialnum_t middleMaterial,
                                  float middleOffsetX, float middleOffsetY,
                                  float middleRed, float middleGreen,
                                  float middleBlue, float middleAlpha,
                                  materialnum_t bottomMaterial,
                                  float bottomOffsetX, float bottomOffsetY,
                                  float bottomRed, float bottomGreen,
                                  float bottomBlue);
uint            MPE_LinedefCreate(uint v1, uint v2, uint frontSide, uint backSide,
                                  int flags);
uint            MPE_SectorCreate(float lightlevel, float red, float green, float blue);
uint            MPE_PlaneCreate(uint sector, float height,
                                materialnum_t material,
                                float matOffsetX, float matOffsetY,
                                float r, float g, float b, float a,
                                float normalX, float normalY, float normalZ);
uint            MPE_PolyobjCreate(uint *lines, uint linecount,
                                  int tag, int sequenceType, float startX, float startY);

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

void            MPE_PruneRedundantMapData(editmap_t *map, int flags);

boolean         MPE_RegisterUnclosedSectorNear(sector_t *sec, double x, double y);
void            MPE_PrintUnclosedSectorList(void);
void            MPE_FreeUnclosedSectorList(void);

gamemap_t      *MPE_GetLastBuiltMap(void);
vertex_t       *createVertex(void);
#endif
