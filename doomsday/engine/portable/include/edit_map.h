/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright Â© 2007 Daniel Swanson <danij@dengine.net>
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
#include "r_materials.h"

typedef struct editmap_s {
    uint            numvertexes;
    vertex_t      **vertexes;
    uint            numlines;
    line_t        **lines;
    uint            numsides;
    side_t        **sides;
    uint            numsectors;
    sector_t      **sectors;
    uint            numsegs;
    seg_t         **segs;
    uint            numsubsectors;
    subsector_t   **subsectors;
    uint            numnodes;
    node_t        **nodes;
    uint            numpolyobjs;
    polyobj_t     **polyobjs;

    node_t         *rootNode;
} editmap_t;

boolean         P_InitMapBuild(void);
void            P_ShutdownMapBuild(void);
gamemap_t      *P_HardenMap(void);

vertex_t       *P_NewVertex(float x, float y);
side_t         *P_NewSide(uint sector, short flags,
                          material_t *topMaterial, float topOffsetX,
                          float topOffsetY, float topRed, float topGreen,
                          float topBlue, material_t *middleMaterial,
                          float middleOffsetX, float middleOffsetY,
                          float middleRed, float middleGreen, float middleBlue,
                          float middleAlpha, material_t *bottomMaterial,
                          float bottomOffsetX, float bottomOffsetY, float bottomRed,
                          float bottomGreen, float bottomBlue);
line_t         *P_NewLine(uint v1, uint v2, uint frontSide, uint backSide,
                          short mapflags, int flags);
sector_t       *P_NewSector(plane_t **planes, uint planecount,
                            float lightlevel, float red, float green, float blue);
polyobj_t      *P_NewPolyobj(uint *lines, uint linecount, boolean crush,
                             int tag, int sequenceType, float startX, float startY);

// Non-public.
subsector_t    *P_NewSubsector(void);
node_t         *P_NewNode(void);
#endif
