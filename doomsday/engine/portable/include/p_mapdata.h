/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * p_mapdata.h: Playsim Data Structures, Macros and Constants
 *
 * These are internal to Doomsday. The games have no direct access to
 * this data.
 */

#ifndef __DOOMSDAY_PLAY_DATA_H__
#define __DOOMSDAY_PLAY_DATA_H__

#if defined(__JDOOM__) || defined(__JHERETIC__) || defined(__JHEXEN__)
#  error "Attempted to include internal Doomsday p_mapdata.h from a game"
#endif

#include "dd_share.h"
#include "dam_main.h"
#include "rend_bias.h"
#include "m_nodepile.h"
#include "m_vector.h"

#define GET_VERTEX_IDX(vtx)     ((vtx) - vertexes)
#define GET_LINE_IDX(li)        ((li) - lines)
#define GET_SIDE_IDX(si)        ((si) - sides)
#define GET_SECTOR_IDX(sec)     ((sec) - sectors)
#define GET_SUBSECTOR_IDX(sub)  ((sub) - subsectors)
#define GET_SEG_IDX(seg)        ((seg) - segs)
#define GET_NODE_IDX(nd)        ((nd) - nodes)

// Return the index of plane within a sector's planes array.
#define GET_PLANE_IDX(pln)      ((pln) - (pln)->sector->planes[0])

#define VERTEX_PTR(i)           (&vertexes[i])
#define SEG_PTR(i)              (&segs[i])
#define SECTOR_PTR(i)           (&sectors[i])
#define SUBSECTOR_PTR(i)        (&subsectors[i])
#define NODE_PTR(i)             (&nodes[i])
#define LINE_PTR(i)             (&lines[i])
#define SIDE_PTR(i)             (&sides[i])

// Node flags.
#define NF_SUBSECTOR        0x80000000

// Runtime map data objects, such as vertices, sectors, and subsectors all
// have this header as their first member. This makes it possible to treat
// an unknown map data pointer as a runtime_mapdata_header_t* and determine
// its type. Note that this information is internal to the engine.
typedef struct runtime_mapdata_header_s {
    int             type;       // One of the DMU type constants.
} runtime_mapdata_header_t;

typedef struct fvertex_s {
    float           pos[2];
} fvertex_t;

typedef struct shadowcorner_s {
    float           corner;
    struct sector_s *proximity;
    float           pOffset;
    float           pHeight;
} shadowcorner_t;

typedef struct edgespan_s {
    float           length;
    float           shift;
} edgespan_t;

typedef struct linkpolyobj_s {
    struct polyobj_s *polyobj;
    struct linkpolyobj_s *prev;
    struct linkpolyobj_s *next;
} linkpolyobj_t;

typedef struct watchedplanelist_s {
    uint            num, maxNum;
    struct plane_s ** list;
} watchedplanelist_t;

typedef void* blockmap_t;

#include "p_maptypes.h"

/*
 * The map data arrays are accessible globally inside the engine.
 */
extern char     levelid[9];
extern uint     numvertexes;
extern vertex_t *vertexes;

extern uint     numsegs;
extern seg_t   *segs;

extern uint     numsectors;
extern sector_t *sectors;

extern uint     numsubsectors;
extern subsector_t *subsectors;

extern uint     numnodes;
extern node_t  *nodes;

extern uint     numlines;
extern line_t  *lines;

extern uint     numsides;
extern side_t  *sides;

extern watchedplanelist_t *watchedPlaneList;

extern uint     numthings;

extern float    mapGravity;

typedef struct gamemap_s {
    char        levelID[9];
    char        uniqueID[256];

    float       bBox[4];

    uint        numVertexes;
    vertex_t   *vertexes;

    uint        numSegs;
    seg_t      *segs;

    uint        numSectors;
    sector_t   *sectors;

    uint        numSubsectors;
    subsector_t *subsectors;

    uint        numNodes;
    node_t     *nodes;

    uint        numLines;
    line_t     *lines;

    uint        numSides;
    side_t     *sides;

    uint        numPolyobjs;
    polyobj_t **polyobjs;

    linkpolyobj_t **polyBlockMap;

    uint        numThings;

    watchedplanelist_t watchedPlaneList;

    blockmap_t *blockMap;
    blockmap_t *ssecBlockMap;

    struct linkmobj_s *blockRings;      // for mobj rings
    nodepile_t  mobjNodes, lineNodes;   // all kinds of wacky links.
    nodeindex_t *lineLinks;             // indices to roots.

    byte       *rejectMatrix;

    float       globalGravity;          // Gravity for the current map.
    int         ambientLightLevel;      // Ambient lightlevel for the current map.
} gamemap_t;

void        P_InitData(void);

gamemap_t  *P_GetCurrentMap(void);
void        P_SetCurrentMap(gamemap_t *map);

const char *P_GetMapID(gamemap_t *map);
const char *P_GetUniqueMapID(gamemap_t *map);
void        P_GetMapBounds(gamemap_t *map, float *min, float *max);
int         P_GetMapAmbientLightLevel(gamemap_t *map);

const char *P_GenerateUniqueMapID(const char *mapID);

void        P_PolyobjChanged(polyobj_t *po);
void        P_FloorChanged(sector_t *sector);
void        P_CeilingChanged(sector_t *sector);
void        P_PlaneChanged(sector_t *sector, uint plane);
int         P_CheckTexture(char *name, boolean planeTex, int dataType,
                           unsigned int element, int property);
void        P_RegisterUnknownTexture(const char *name, boolean planeTex);
void        P_PrintMissingTextureList(void);
void        P_FreeBadTexList(void);
#endif
