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
 * edit_map.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_play.h"
#include "de_bsp.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "de_edit.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean editMapInited = false;
static editmap_t editMap, *map = &editMap;

// CODE --------------------------------------------------------------------

static vertex_t *createVertex(void)
{
    vertex_t           *vtx;

    vtx = M_Calloc(sizeof(*vtx));
    vtx->header.type = DMU_VERTEX;

    map->vertexes =
        M_Realloc(map->vertexes, sizeof(vtx) * (++map->numvertexes + 1));
    map->vertexes[map->numvertexes-1] = vtx;
    map->vertexes[map->numvertexes] = NULL;

    vtx->buildData.index = map->numvertexes; // 1-based index, 0 = NIL.
    return vtx;
}

static line_t *createLine(void)
{
    line_t             *line;

    line = M_Calloc(sizeof(*line));
    line->header.type = DMU_LINE;

    map->lines =
        M_Realloc(map->lines, sizeof(line) * (++map->numlines + 1));
    map->lines[map->numlines-1] = line;
    map->lines[map->numlines] = NULL;

    line->buildData.index = map->numlines; // 1-based index, 0 = NIL.
    return line;
}

static side_t *createSide(void)
{
    side_t             *side;

    side = M_Calloc(sizeof(*side));
    side->header.type = DMU_SIDE;

    map->sides =
        M_Realloc(map->sides, sizeof(side) * (++map->numsides + 1));
    map->sides[map->numsides-1] = side;
    map->sides[map->numsides] = NULL;

    side->buildData.index = map->numsides; // 1-based index, 0 = NIL.
    return side;
}

static sector_t *createSector(void)
{
    sector_t           *sec;

    sec = M_Calloc(sizeof(*sec));
    sec->header.type = DMU_SECTOR;

    map->sectors = M_Realloc(map->sectors, sizeof(sec) * (++map->numsectors + 1));
    map->sectors[map->numsectors-1] = sec;
    map->sectors[map->numsectors] = NULL;

    sec->buildData.index = map->numsectors; // 1-based index, 0 = NIL.
    return sec;
}

static subsector_t *createSubsector(void)
{
    subsector_t        *ssec;

    ssec = M_Calloc(sizeof(*ssec));
    ssec->header.type = DMU_SUBSECTOR;

    map->subsectors =
        M_Realloc(map->subsectors, sizeof(ssec) * (++map->numsubsectors + 1));
    map->subsectors[map->numsubsectors-1] = ssec;
    map->subsectors[map->numsubsectors] = NULL;

    ssec->buildData.index = map->numsubsectors; // 1-based index, 0 = NIL.
    return ssec;
}

static node_t *createNode(void)
{
    node_t             *node;

    node = M_Calloc(sizeof(*node));
    node->header.type = DMU_NODE;

    map->nodes = M_Realloc(map->nodes, sizeof(node) * (++map->numnodes + 1));
    map->nodes[map->numnodes-1] = node;
    map->nodes[map->numnodes] = NULL;

    node->buildData.index = map->numnodes; // 1-based index, 0 = NIL.
    return node;
}

static polyobj_t *createPolyobj(void)
{
    polyobj_t          *po;

    po = M_Calloc(sizeof(*po));
    po->header.type = DMU_POLYOBJ;

    map->polyobjs = M_Realloc(map->polyobjs, sizeof(po) * (++map->numpolyobjs + 1));
    map->polyobjs[map->numpolyobjs-1] = po;
    map->polyobjs[map->numpolyobjs] = NULL;

    po->buildData.index = map->numpolyobjs; // 1-based index, 0 = NIL.
    return po;
}

static void destroyMap(void)
{
    if(map->vertexes)
        M_Free(map->vertexes);
    map->vertexes = NULL;
    map->numvertexes = 0;

    if(map->lines)
        M_Free(map->lines);
    map->lines = NULL;
    map->numlines = 0;

    if(map->sides)
        M_Free(map->sides);
    map->sides = NULL;
    map->numsides = 0;

    if(map->sectors)
        M_Free(map->sectors);
    map->sectors = NULL;
    map->numsectors = 0;

    if(map->segs)
        M_Free(map->segs);
    map->segs = NULL;
    map->numsegs = 0;

    if(map->subsectors)
        M_Free(map->subsectors);
    map->subsectors = NULL;
    map->numsubsectors = 0;

    if(map->nodes)
        M_Free(nodes);
    map->nodes = NULL;
    map->numnodes = 0;

    if(map->polyobjs)
    {
        uint                i;
        for(i = 0; i < map->numpolyobjs; ++i)
        {
            polyobj_t          *po = map->polyobjs[i];
            M_Free(po->buildData.lines);
            M_Free(po);
        }
        M_Free(map->polyobjs);
    }
    map->polyobjs = NULL;
    map->numpolyobjs = 0;
}

static int C_DECL vertexCompare(const void *p1, const void *p2)
{
    uint        vert1 = ((const uint *) p1)[0];
    uint        vert2 = ((const uint *) p2)[0];
    vertex_t   *a, *b;

    if(vert1 == vert2)
        return 0;

    a = editMap.vertexes[vert1];
    b = editMap.vertexes[vert2];

    if((int) a->buildData.pos[VX] != (int) b->buildData.pos[VX])
        return (int) a->buildData.pos[VX] - (int) b->buildData.pos[VX];

    return (int) a->buildData.pos[VY] - (int) b->buildData.pos[VY];
}

void MPE_DetectDuplicateVertices(editmap_t *map)
{
    uint        i;
    uint       *hits = M_Malloc(map->numvertexes * sizeof(uint));

    // Sort array of indices.
    for(i = 0; i < map->numvertexes; ++i)
        hits[i] = i;

    qsort(hits, map->numvertexes, sizeof(uint), vertexCompare);

    // Now mark them off.
    for(i = 0; i < map->numvertexes - 1; ++i)
    {
        // A duplicate?
        if(vertexCompare(hits + i, hits + i + 1) == 0)
        {   // Yes.
            vertex_t *a = map->vertexes[hits[i]];
            vertex_t *b = map->vertexes[hits[i + 1]];

            b->buildData.equiv = (a->buildData.equiv ? a->buildData.equiv : a);
        }
    }

    M_Free(hits);
}

/**
 * Called to begin the map building process.
 */
boolean P_InitMapBuild(void)
{
    if(editMapInited)
        return true; // Already been here.

    destroyMap();

    editMapInited = true;
    return true;
}

gamemap_t *P_HardenMap(void)
{
    gamemap_t                  *gamemap;

    if(!editMapInited)
        return NULL;

    /**
     * Perform cleanup on the loaded map data, removing duplicate vertexes,
     * pruning unused sectors etc, etc...
     */
    MPE_DetectDuplicateVertices(map);
    //MPE_PruneRedundantMapData(map, PRUNE_ALL);

    gamemap = Z_Calloc(sizeof(*gamemap), PU_LEVELSTATIC, 0);

    /**
     * Build a BSP for this map.
     */
    if(!BSP_Build(gamemap, map))
    {   // Argh, failed.
        // Need to clean up.
        Z_Free(gamemap);
        return NULL;
    }

    // Success!
    return gamemap;
}

void P_ShutdownMapBuild(void)
{
    destroyMap();
}

vertex_t *P_NewVertex(float x, float y)
{
    vertex_t           *v;

    if(!editMapInited)
        return NULL;

    v = createVertex();
    v->V_pos[VX] = x;
    v->V_pos[VY] = y;
    v->buildData.pos[VX] = (double) v->V_pos[VX];
    v->buildData.pos[VY] = (double) v->V_pos[VY];

    return v;
}

side_t *P_NewSide(uint sector, short flags,
                  material_t *topMaterial, float topOffsetX,
                  float topOffsetY, float topRed, float topGreen,
                  float topBlue, material_t *middleMaterial,
                  float middleOffsetX, float middleOffsetY,
                  float middleRed, float middleGreen, float middleBlue,
                  float middleAlpha, material_t *bottomMaterial,
                  float bottomOffsetX, float bottomOffsetY, float bottomRed,
                  float bottomGreen, float bottomBlue)
{
    side_t             *s;

    if(!editMapInited)
        return NULL;

    if(sector > map->numsectors)
        return NULL;

    s = createSide();
    s->flags = flags;
    s->sector = (sector == 0? NULL: map->sectors[sector-1]);

    s->SW_topmaterial = topMaterial;
    s->SW_topoffset[VX] = topOffsetX;
    s->SW_topoffset[VY] = topOffsetY;
    s->SW_toprgba[CR] = MINMAX_OF(0, topRed, 1);
    s->SW_toprgba[CG] = MINMAX_OF(0, topGreen, 1);
    s->SW_toprgba[CB] = MINMAX_OF(0, topBlue, 1);
    s->SW_toprgba[CA] = 1;

    s->SW_middlematerial = middleMaterial;
    s->SW_middleoffset[VX] = middleOffsetX;
    s->SW_middleoffset[VY] = middleOffsetY;
    s->SW_middlergba[CR] = MINMAX_OF(0, middleRed, 1);
    s->SW_middlergba[CG] = MINMAX_OF(0, middleGreen, 1);
    s->SW_middlergba[CB] = MINMAX_OF(0, middleBlue, 1);
    s->SW_middlergba[CA] = MINMAX_OF(0, middleAlpha, 1);

    s->SW_bottommaterial = bottomMaterial;
    s->SW_bottomoffset[VX] = bottomOffsetX;
    s->SW_bottomoffset[VY] = bottomOffsetY;
    s->SW_bottomrgba[CR] = MINMAX_OF(0, bottomRed, 1);
    s->SW_bottomrgba[CG] = MINMAX_OF(0, bottomGreen, 1);
    s->SW_bottomrgba[CB] = MINMAX_OF(0, bottomBlue, 1);
    s->SW_bottomrgba[CA] = 1;

    return s;
}

line_t *P_NewLine(uint v1, uint v2, uint frontSide, uint backSide,
                  short mapflags, int flags)
{
    line_t             *l;

    if(!editMapInited)
        return NULL;

    if(frontSide > map->numsides)
        return NULL;
    if(backSide > map->numsides)
        return NULL;
    if(v1 > map->numvertexes)
        return NULL;
    if(v2 > map->numvertexes)
        return NULL;
    if(v1 == v2)
        return NULL;

    l = createLine();
    l->mapflags = mapflags;
    l->flags = flags;
    l->L_v1 = (v1 == 0? NULL : map->vertexes[v1-1]);
    l->L_v2 = (v2 == 0? NULL : map->vertexes[v2-1]);

    l->dx = l->L_v2pos[VX] - l->L_v1pos[VX];
    l->dy = l->L_v2pos[VY] - l->L_v1pos[VY];
    l->length = P_AccurateDistance(l->dx, l->dy);

    l->angle =
        bamsAtan2((int) (l->L_v2pos[VY] - l->L_v1pos[VY]),
                  (int) (l->L_v2pos[VX] - l->L_v1pos[VX])) << FRACBITS;

    if(l->dx == 0)
        l->slopetype = ST_VERTICAL;
    else if(l->dy == 0)
        l->slopetype = ST_HORIZONTAL;
    else
    {
        if(l->dy / l->dx > 0)
            l->slopetype = ST_POSITIVE;
        else
            l->slopetype = ST_NEGATIVE;
    }

    if(l->L_v1pos[VX] < l->L_v2pos[VX])
    {
        l->bbox[BOXLEFT]   = l->L_v1pos[VX];
        l->bbox[BOXRIGHT]  = l->L_v2pos[VX];
    }
    else
    {
        l->bbox[BOXLEFT]   = l->L_v2pos[VX];
        l->bbox[BOXRIGHT]  = l->L_v1pos[VX];
    }

    if(l->L_v1pos[VY] < l->L_v2pos[VY])
    {
        l->bbox[BOXBOTTOM] = l->L_v1pos[VY];
        l->bbox[BOXTOP]    = l->L_v2pos[VY];
    }
    else
    {
        l->bbox[BOXBOTTOM] = l->L_v1pos[VY];
        l->bbox[BOXTOP]    = l->L_v2pos[VY];
    }

    l->L_frontside = (frontSide == 0? NULL : map->sides[frontSide-1]);
    l->L_backside = (backSide == 0? NULL : map->sides[backSide-1]);

    return l;
}

sector_t *P_NewSector(plane_t **planes, uint planecount,
                      float lightlevel, float red, float green, float blue)
{
    sector_t           *s;

    if(!editMapInited)
        return NULL;

    s = createSector();

    s->rgb[CR] = MINMAX_OF(0, red, 1);
    s->rgb[CG] = MINMAX_OF(0, green, 1);
    s->rgb[CB] = MINMAX_OF(0, blue, 1);
    s->lightlevel = MINMAX_OF(0, lightlevel, 1);
    s->buildData.warnedFacing = -1;
    s->planes = planes;
    s->planecount = planecount;

    return s;
}

polyobj_t *P_NewPolyobj(uint *lines, uint lineCount, boolean crush, int tag,
                        int sequenceType, float startX, float startY)
{
    uint                i;
    polyobj_t          *po;

    if(!editMapInited)
        return NULL;

    po = createPolyobj();
    po->buildData.lines = M_Calloc(sizeof(line_t*) * (lineCount+1));
    for(i = 0; i < lineCount; ++i)
        po->buildData.lines[i] = map->lines[lines[i]-1];
    po->buildData.lines[i] = NULL;
    po->buildData.lineCount = lineCount;
    po->tag = tag;
    po->seqType = sequenceType;
    po->crush = crush;
    po->startSpot.pos[VX] = startX;
    po->startSpot.pos[VY] = startY;

    return po;
}

subsector_t *P_NewSubsector(void)
{
    subsector_t        *s;

    if(!editMapInited)
        return NULL;

    s = createSubsector();
    return s;
}

node_t *P_NewNode(void)
{
    node_t             *n;

    if(!editMapInited)
        return NULL;

    n = createNode();
    return n;
}
