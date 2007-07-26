/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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

/*
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

#define GET_VERTEX_IDX(vtx)     ((vtx) - vertexes)
#define GET_LINE_IDX(li)        ((li) - lines)
#define GET_SIDE_IDX(si)        ((si) - sides)
#define GET_SECTOR_IDX(sec)     ((sec) - sectors)
#define GET_SUBSECTOR_IDX(sub)  ((sub) - subsectors)
#define GET_POLYOBJ_IDX(po)     ((po) - polyobjs)
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
#define PO_PTR(i)               (&polyobjs[i])

// Map line flags.
#define ML_BLOCKING         0x0001
#define ML_TWOSIDED         0x0004
#define ML_DONTPEGTOP       0x0008
#define ML_DONTPEGBOTTOM    0x0010
//#define ML_MAPPED           0x0100 // set if already drawn in automap

// Node flags.
#define NF_SUBSECTOR    0x80000000

// Sidedef flags
#define SDF_BLENDTOPTOMID       0x01
#define SDF_BLENDMIDTOTOP       0x02
#define SDF_BLENDMIDTOBOTTOM    0x04
#define SDF_BLENDBOTTOMTOMID    0x08
//#define SDF_MIDTEXUPPER         0x10

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

extern int      numthings;

extern fixed_t  mapgravity;        // Gravity for the current map.
extern int      mapambient;        // Ambient light level for the current map.

extern uint    *missingFronts;
extern uint     numMissingFronts;

typedef struct gamemap_s {
    char        levelid[9];
    uint        numvertexes;
    vertex_t   *vertexes;

    uint        numsegs;
    seg_t      *segs;

    uint        numsectors;
    sector_t   *sectors;

    uint        numsubsectors;
    subsector_t *subsectors;

    uint        numnodes;
    node_t     *nodes;

    uint        numlines;
    line_t     *lines;

    uint        numsides;
    side_t     *sides;

    uint        po_NumPolyobjs;
    polyobj_t  *polyobjs;

    int         numthings;

    long       *blockmaplump;           // offsets in blockmap are from here
    long       *blockmap;

    uint        bmapwidth, bmapheight;  // in mapblocks
    fixed_t     bmaporgx, bmaporgy;     // origin of block map
    struct linkmobj_s *blockrings;             // for thing rings

    byte       *rejectmatrix;
} gamemap_t;

void            P_PolyobjChanged(polyobj_t *po);
void            P_FloorChanged(sector_t *sector);
void            P_CeilingChanged(sector_t *sector);
void            P_PlaneChanged(sector_t *sector, uint plane);
void            P_InitData(void);
int             P_CheckTexture(char *name, boolean planeTex, int dataType,
                               unsigned int element, int property);

boolean         P_CheckLevel(char *levelID, boolean silent);
#endif
