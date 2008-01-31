/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2008 Daniel Swanson <danij@dengine.net>
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
#include "r_materials.h"

typedef struct editmap_s {
    char            name[9];
    uint            numVertexes;
    vertex_t      **vertexes;
    uint            numLines;
    line_t        **lines;
    uint            numSides;
    side_t        **sides;
    uint            numSectors;
    sector_t      **sectors;
    uint            numSegs;
    seg_t         **segs;

    uint            numPolyobjs;
    polyobj_t     **polyobjs;

    // BSP data.
    uint            numSubsectors;
    binarytree_t   *rootNode;
} editmap_t;

boolean         MPE_Begin(const char *name);
boolean         MPE_End(void);

uint            MPE_VertexCreate(float x, float y);
uint            MPE_SidedefCreate(uint sector, short flags,
                                  const char *topMaterial,
                                  materialtype_t topMaterialType,
                                  float topOffsetX, float topOffsetY, float topRed,
                                  float topGreen, float topBlue,
                                  const char *middleMaterial,
                                  materialtype_t middleMaterialType,
                                  float middleOffsetX, float middleOffsetY,
                                  float middleRed, float middleGreen,
                                  float middleBlue, float middleAlpha,
                                  const char *bottomMaterial,
                                  materialtype_t bottomMaterialType,
                                  float bottomOffsetX, float bottomOffsetY,
                                  float bottomRed, float bottomGreen,
                                  float bottomBlue);
uint            MPE_LinedefCreate(uint v1, uint v2, uint frontSide, uint backSide,
                                  short mapflags, int flags);
uint            MPE_SectorCreate(float lightlevel, float red, float green, float blue,
                                 float floorHeight, const char *floorMaterial,
                                 materialtype_t floorMaterialType, float floorOffsetX,
                                 float floorOffsetY, float floorRed, float floorGreen,
                                 float floorBlue, float ceilHeight,
                                 const char *ceilMaterial,
                                 materialtype_t ceilMaterialType, float ceilOffsetX,
                                 float ceilOffsetY, float ceilRed, float ceilGreen,
                                 float ceilBlue);
uint            MPE_PolyobjCreate(uint *lines, uint linecount,
                                  int tag, int sequenceType, float startX, float startY);

// Non-public (temporary)
gamemap_t      *MPE_GetLastBuiltMap(void);
vertex_t       *createVertex(void);

// Non-public (to be moved into the BSP code).
subsector_t    *BSP_NewSubsector(void);
#endif
