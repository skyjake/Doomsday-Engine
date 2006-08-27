/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 *
 * Based on Hexen by Raven Software.
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
#include "p_arch.h"

// Ticcmd
#define TICCMD_SIZE     gx.ticcmd_size
#define TICCMD_IDX(i)   ((i)*TICCMD_SIZE)

// Tick Command template. Of course it contains much more info,
// but these first bytes are always the same.
typedef struct {
    char            forwardMove;   //*2048 for real move
    char            sideMove;      //*2048 for real move
} ticcmd_t;

#define GET_VERTEX_IDX(vtx)     ((vtx) - vertexes)
#define GET_LINE_IDX(li)        ((li) - lines)
#define GET_SIDE_IDX(si)        ((si) - sides)
#define GET_SECTOR_IDX(sec)     ((sec) - sectors)
#define GET_SUBSECTOR_IDX(sub)  ((sub) - subsectors)
#define GET_POLYOBJ_IDX(po)     ((po) - polyobjs)
#define GET_SEG_IDX(seg)        ((seg) - segs)
#define GET_NODE_IDX(nd)        ((nd) - nodes)

// Return the index of plane within a sector's planes array.
#define GET_PLANE_IDX(pln)      ((pln) - (pln)->sector->planes)

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
#define ML_MAPPED           0x0100 // set if already drawn in automap

#define MAX_POLY_SIDES      64     // A subsector has at most this many edges.

// Node flags.
#define NF_SUBSECTOR    0x80000000

// Sidedef flags
#define SDF_BLENDTOPTOMID       0x01
#define SDF_BLENDMIDTOTOP       0x02
#define SDF_BLENDMIDTOBOTTOM    0x04
#define SDF_BLENDBOTTOMTOMID    0x08
#define SDF_MIDTEXUPPER         0x10

// Runtime map data objects, such as vertices, sectors, and subsectors all
// have this header as their first member. This makes it possible to treat
// an unknown map data pointer as a runtime_mapdata_header_t* and determine
// its type. Note that this information is internal to the engine.
typedef struct runtime_mapdata_header_s {
    int             type;       // One of the DMU type constants.
} runtime_mapdata_header_t;

typedef struct fvertex_s {
    float           x;
    float           y;
} fvertex_t;

enum {
    PLN_FLOOR,
    PLN_CEILING,
    NUM_PLANES
};

#include "p_maptypes.h"

/*
typedef struct vertex_s {
    runtime_mapdata_header_t header;
    fixed_t         x;
    fixed_t         y;
} vertex_t;

typedef struct seg_s {
    runtime_mapdata_header_t header;
    vertex_t       *v1, *v2;
    float           length;    // Accurate length of the segment (v1 -> v2).
    fixed_t         offset;
    struct side_s  *sidedef;
    struct line_s  *linedef;
    struct sector_s *frontsector;
    struct sector_s *backsector;    // NULL for one sided lines
    byte            flags;
    angle_t         angle;
} seg_t;

typedef struct subsector_s {
    runtime_mapdata_header_t header;
    struct sector_s *sector;
    int             linecount;
    int             firstline;
    struct polyobj_s *poly;    // NULL if there is no polyobj
    // Sorted edge vertices for rendering floors and ceilings.
    char            numverts;
    fvertex_t      *verts;     // A list of edge vertices.
    fvertex_t       bbox[2];   // Min and max points.
    fvertex_t       midpoint;  // Center of vertices.
    byte            flags;
} subsector_t;

typedef struct sector_s {
    runtime_mapdata_header_t header;
    fixed_t         floorheight, ceilingheight;
    short           floorpic, ceilingpic;
    short           lightlevel;
    byte            rgb[3];
    byte            floorrgb[3], ceilingrgb[3];
    int             validcount;    // if == validcount, already checked
    struct mobj_s  *thinglist;     // list of mobjs in sector
    int             linecount;
    struct line_s **Lines;         // [linecount] size
    float           flooroffx, flooroffy;   // floor texture offset
    float           ceiloffx, ceiloffy; // ceiling texture offset
    // Plane glows
    byte            floorglowrgb[3], ceilingglowrgb[3];
    float           floorglow, ceilingglow;
    int             skyfix;        // Offset to ceiling height
    // rendering w/sky.
    float           reverb[NUM_REVERB_DATA];
    int             blockbox[4];   // mapblock bounding box for
    // height changes
    plane_t         planes[2];     // PLN_*
    degenmobj_t     soundorg;      // origin for any sounds
    degenmobj_t     floorsoundorg, ceilingsoundorg;
    // played by the sector
} sector_t;

typedef struct side_s {
    runtime_mapdata_header_t header;
    fixed_t         textureoffset; // add this to the calculated texture col
    fixed_t         rowoffset;     // add this to the calculated texture top
    short           toptexture, bottomtexture, midtexture;
    byte            toprgb[3], bottomrgb[3], midrgba[4];
    blendmode_t     blendmode;
    short           flags;
    sector_t        *sector;
} side_t;

typedef struct line_s {
    runtime_mapdata_header_t header;
    vertex_t       *v1;
    vertex_t       *v2;
    short           flags;
    sector_t       *frontsector;
    sector_t       *backsector;
    fixed_t         dx;
    fixed_t         dy;
    slopetype_t     slopetype;
    int             validcount;
    int             sidenum[2];
    fixed_t         bbox[4];
} line_t;

typedef struct polyobj_s {
    runtime_mapdata_header_t header;
    int             numsegs;
    seg_t         **segs;
    int             validcount;
    degenmobj_t     startSpot;
    angle_t         angle;
    ddvertex_t     *originalPts;   // used as the base for the rotations
    ddvertex_t     *prevPts;       // use to restore the old point values
    int             tag;           // reference tag assigned in HereticEd
    fixed_t         bbox[4];
    vertex_t        dest;
    int             speed;         // Destination XY and speed.
    angle_t         destAngle;     // Destination angle.
    angle_t         angleSpeed;    // Rotation speed.
    boolean         crush;         // should the polyobj attempt to crush mobjs?
    int             seqType;
    fixed_t         size;          // polyobj size (area of POLY_AREAUNIT == size of FRACUNIT)
    void           *specialdata;   // pointer a thinker, if the poly is moving
} polyobj_t;

typedef struct {
    runtime_mapdata_header_t header;
    fixed_t         x, y, dx, dy;  // partition line
    fixed_t         bbox[2][4];    // bounding box for each child
    int             children[2];   // if NF_SUBSECTOR its a subsector
} node_t;
*/

/*
 * The map data arrays are accessible globally inside the engine.
 */
extern int      numvertexes;
extern vertex_t *vertexes;

extern int      numsegs;
extern seg_t   *segs;

extern int      numsectors;
extern sector_t *sectors;

extern int      numsubsectors;
extern subsector_t *subsectors;

extern int      numnodes;
extern node_t  *nodes;

extern int      numlines;
extern line_t  *lines;

extern int      numsides;
extern side_t  *sides;

extern int      numthings;

extern fixed_t  mapgravity;        // Gravity for the current map.

extern int      numUniqueLines;

extern int     *missingFronts;
extern int      numMissingFronts;

void            P_PolyobjChanged(polyobj_t *po);
void            P_FloorChanged(sector_t *sector);
void            P_CeilingChanged(sector_t *sector);
void            P_PlaneChanged(sector_t *sector, int plane);
void            P_InitData(void);
const char*     value_Str(int val);
int             P_CheckTexture(char *name, boolean planeTex, int dataType,
                               unsigned int element, int property);

boolean         P_CheckLevel(char *levelID, boolean silent);
#endif
