/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Daniel Swanson <danij@dengine.net>
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
#include "de_dam.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

boolean     MPE_PrintMapErrors(boolean silent);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean editMapInited = false;
static editmap_t editMap, *map = &editMap;

// The following is used in error fixing/detection/reporting:
// missing sidedefs
static uint numMissingFronts;
static uint *missingFronts;

static gamemap_t *lastBuiltMap = NULL;

// CODE --------------------------------------------------------------------

vertex_t *createVertex(void)
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
    memset(map->name, 0, sizeof(map->name));

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
    {
        uint                i;
        for(i = 0; i < map->numsectors; ++i)
        {
            uint                j;
            sector_t           *s = map->sectors[i];

            if(s->planes)
            {
                for(j = 0; j < s->planecount; ++j)
                {
                    M_Free(s->planes[j]);
                }
                M_Free(s->planes);
            }
        }
        M_Free(map->sectors);
    }
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
        M_Free(map->nodes);
    map->nodes = NULL;
    map->rootNode = NULL;
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
boolean MPE_Begin(const char *name)
{
    if(editMapInited)
        return true; // Already been here.

    destroyMap();

    if(name && name[0])
    {
        strncpy(map->name, name, 8);
    }

    editMapInited = true;
    return true;
}

static void findMissingFrontSidedefs(gamemap_t *map)
{
    uint        i;

    numMissingFronts = 0;
    missingFronts = M_Calloc(map->numlines * sizeof(uint));

    for(i = 0; i < map->numlines; ++i)
    {
        line_t     *li = &map->lines[i];

        if(!li->L_frontside)
        {   // A missing front sidedef
            missingFronts[i] = 1;
            numMissingFronts++;
        }
    }
}

static void markSelfReferencingLinedefs(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numlines; ++i)
    {
        line_t     *li = &map->lines[i];

        // A self-referencing line?
        if(li->L_frontside && li->L_backside &&
           li->L_frontside->sector == li->L_backside->sector)
            li->flags |= LINEF_SELFREF;
    }
}

static void linkSSecsToSectors(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numsubsectors; ++i)
    {
        subsector_t *ssec = &map->subsectors[i];
        seg_t      **segp;
        boolean     found;

        segp = ssec->segs;
        found = false;
        while(*segp)
        {
            seg_t      *seg = *segp;

            if(!found && seg->sidedef)
            {
                ssec->sector = seg->sidedef->sector;
                found = true;
            }

            seg->subsector = ssec;
            *segp++;
        }

        assert(ssec->sector);
    }
}

static void hardenSectorSSecList(gamemap_t *map, uint secIDX)
{
    uint        i, n, count;
    sector_t   *sec = &map->sectors[secIDX];

    count = 0;
    for(i = 0; i < map->numsubsectors; ++i)
    {
        subsector_t *ssec = &map->subsectors[i];
        if(ssec->sector == sec)
            count++;
    }

    sec->subsectors =
        Z_Malloc((count + 1) * sizeof(subsector_t*), PU_LEVELSTATIC, NULL);

    n = 0;
    for(i = 0; i < map->numsubsectors; ++i)
    {
        subsector_t *ssec = &map->subsectors[i];
        if(ssec->sector == sec)
            sec->subsectors[n++] = ssec;
    }
    sec->subsectors[n] = NULL; // Terminate.
    sec->subscount = count;
}

/**
 * Build subsector tables for all sectors.
 */
static void buildSectorSSecLists(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numsectors; ++i)
    {
        hardenSectorSSecList(map, i);
    }
}

static void buildSectorLineLists(gamemap_t *map)
{
    typedef struct linelink_s {
        line_t      *line;
        struct linelink_s *next;
    } linelink_t;

    uint        i, j;
    line_t     *li;
    sector_t   *sec;

    zblockset_t *lineLinksBlockSet;
    linelink_t  **sectorLineLinks;
    uint        totallinks;

    // build line tables for each sector.
    lineLinksBlockSet = Z_BlockCreate(sizeof(linelink_t), 512, PU_STATIC);
    sectorLineLinks = M_Calloc(sizeof(linelink_t*) * map->numsectors);
    totallinks = 0;
    for(i = 0, li = map->lines; i < map->numlines; ++i, li++)
    {
        uint        secIDX;
        linelink_t *link;

        if(li->L_frontside)
        {
            link = Z_BlockNewElement(lineLinksBlockSet);

            secIDX = li->L_frontsector - map->sectors;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            li->L_frontsector->linecount++;
            totallinks++;
        }

        if(li->L_backside && li->L_backsector != li->L_frontsector)
        {
            link = Z_BlockNewElement(lineLinksBlockSet);

            secIDX = li->L_backsector - map->sectors;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            li->L_backsector->linecount++;
            totallinks++;
        }
    }

    // Harden the sector line links into arrays.
    {
    line_t    **linebuffer;
    line_t    **linebptr;

    linebuffer = Z_Malloc((totallinks + map->numsectors) * sizeof(line_t*),
                          PU_LEVELSTATIC, 0);
    linebptr = linebuffer;

    for(i = 0, sec = map->sectors; i < map->numsectors; ++i, sec++)
    {
        if(sectorLineLinks[i])
        {
            linelink_t *link = sectorLineLinks[i];
            sec->Lines = linebptr;
            j = 0;
            while(link)
            {
                sec->Lines[j++] = link->line;
                link = link->next;
            }
            sec->Lines[j] = NULL; // terminate.
            sec->linecount = j;
            linebptr += j + 1;
        }
        else
        {
            sec->Lines = NULL;
            sec->linecount = 0;
        }
    }
    }

    // Free temporary storage.
    Z_BlockDestroy(lineLinksBlockSet);
    M_Free(sectorLineLinks);
}

/**
 * \pre Sector planes must be initialized before this is called.
 * \todo Bad design: the subgroup is the same for all planes, only the
 * linked group ptrs need to be per-plane.
 */
static void findSectorSSecGroups(sector_t *sec)
{
    uint        i;

    if(!sec)
        return;

    sec->subsgroupcount = 1;
    sec->subsgroups =
        Z_Malloc(sizeof(ssecgroup_t) * sec->subsgroupcount, PU_LEVEL, 0);

    sec->subsgroups[0].linked =
        Z_Malloc(sizeof(sector_t*) * sec->planecount, PU_LEVEL, 0);
    for(i = 0; i < sec->planecount; ++i)
        sec->subsgroups[0].linked[i] = NULL;
}


/**
 * \pre Lines in sector must be setup before this is called!
 */
static void updateSectorBounds(sector_t *sec)
{
    uint        i;
    float      *bbox;
    vertex_t   *vtx;

    if(!sec)
        return;

    bbox = sec->bbox;

    if(!(sec->linecount > 0))
    {
        memset(sec->bbox, 0, sizeof(sec->bbox));
        return;
    }

    vtx = sec->Lines[0]->L_v1;
    bbox[BOXLEFT] = bbox[BOXRIGHT]  = vtx->V_pos[VX];
    bbox[BOXTOP]  = bbox[BOXBOTTOM] = vtx->V_pos[VY];

    for(i = 1; i < sec->linecount; ++i)
    {
        vtx = sec->Lines[i]->L_v1;

        if(vtx->V_pos[VX] < bbox[BOXLEFT])
            bbox[BOXLEFT]   = vtx->V_pos[VX];
        if(vtx->V_pos[VX] > bbox[BOXRIGHT])
            bbox[BOXRIGHT]  = vtx->V_pos[VX];
        if(vtx->V_pos[VY] < bbox[BOXBOTTOM])
            bbox[BOXBOTTOM] = vtx->V_pos[VY];
        if(vtx->V_pos[VY] > bbox[BOXTOP])
            bbox[BOXTOP]   = vtx->V_pos[VY];
    }
}

/**
 * \pre Sector bounds must be setup before this is called!
 */
void P_GetSectorBounds(sector_t *sec, float *min, float *max)
{
    min[VX] = sec->bbox[BOXLEFT];
    min[VY] = sec->bbox[BOXBOTTOM];

    max[VX] = sec->bbox[BOXRIGHT];
    max[VY] = sec->bbox[BOXTOP];
}

static void finishSectors(gamemap_t *map)
{
    uint        i;
    vec2_t      bmapOrigin;
    uint        bmapSize[2];

    P_GetBlockmapBounds(map->blockMap, bmapOrigin, NULL);
    P_GetBlockmapDimensions(map->blockMap, bmapSize);

    for(i = 0; i < map->numsectors; ++i)
    {
        uint        k;
        float       min[2], max[2];
        sector_t   *sec = &map->sectors[i];

        findSectorSSecGroups(sec);

        if(!(sec->linecount > 0))
        {   // Its a "benign" sector.
            // Send the game a status report (we don't need to do anything).
            if(gx.HandleMapObjectStatusReport)
                gx.HandleMapObjectStatusReport(DMUSC_SECTOR_ISBENIGN,
                                               sec - map->sectors,
                                               DMU_SECTOR, NULL);
        }

        updateSectorBounds(sec);

        P_GetSectorBounds(sec, min, max);

        // Set the degenmobj_t to the middle of the bounding box
        sec->soundorg.pos[VX] = (min[VX] + max[VX]) / 2;
        sec->soundorg.pos[VY] = (min[VY] + max[VY]) / 2;

        // Set the z height of the sector sound origin
        sec->soundorg.pos[VZ] = (sec->SP_ceilheight - sec->SP_floorheight) / 2;

        // Set the position of the sound origin for all plane sound origins.
        // Set target heights for all planes.
        for(k = 0; k < sec->planecount; ++k)
        {
            sec->planes[k]->soundorg.pos[VX] = sec->soundorg.pos[VX];
            sec->planes[k]->soundorg.pos[VY] = sec->soundorg.pos[VY];
            sec->planes[k]->soundorg.pos[VZ] = sec->planes[k]->height;

            sec->planes[k]->target = sec->planes[k]->height;
        }
    }
}

/**
 * Completes the linedef loading by resolving the front/back
 * sector ptrs which we couldn't do earlier as the sidedefs
 * hadn't been loaded at the time.
 */
static void finishLineDefs(gamemap_t* map)
{
    uint        i;
    line_t     *ld;
    vertex_t   *v[2];
    seg_t      *startSeg, *endSeg;

    VERBOSE2(Con_Message("Finalizing Linedefs...\n"));

    for(i = 0; i < map->numlines; ++i)
    {
        ld = &map->lines[i];

        if(!ld->sides[0]->segcount)
            continue;

        startSeg = ld->sides[0]->segs[0];
        endSeg = ld->sides[0]->segs[ld->sides[0]->segcount - 1];
        ld->v[0] = v[0] = startSeg->SG_v1;
        ld->v[1] = v[1] = endSeg->SG_v2;
        ld->dx = v[1]->V_pos[VX] - v[0]->V_pos[VX];
        ld->dy = v[1]->V_pos[VY] - v[0]->V_pos[VY];

        // Calculate the accurate length of each line.
        ld->length = P_AccurateDistance(ld->dx, ld->dy);
        ld->angle = bamsAtan2((int) (ld->v[1]->V_pos[VY] - ld->v[0]->V_pos[VY]),
                      (int) (ld->v[1]->V_pos[VX] - ld->v[0]->V_pos[VX])) << FRACBITS;

        if(!ld->dx)
            ld->slopetype = ST_VERTICAL;
        else if(!ld->dy)
            ld->slopetype = ST_HORIZONTAL;
        else
        {
            if(ld->dy / ld->dx > 0)
                ld->slopetype = ST_POSITIVE;
            else
                ld->slopetype = ST_NEGATIVE;
        }

        if(v[0]->V_pos[VX] < v[1]->V_pos[VX])
        {
            ld->bbox[BOXLEFT]   = v[0]->V_pos[VX];
            ld->bbox[BOXRIGHT]  = v[1]->V_pos[VX];
        }
        else
        {
            ld->bbox[BOXLEFT]   = v[1]->V_pos[VX];
            ld->bbox[BOXRIGHT]  = v[0]->V_pos[VX];
        }

        if(v[0]->V_pos[VY] < v[1]->V_pos[VY])
        {
            ld->bbox[BOXBOTTOM] = v[0]->V_pos[VY];
            ld->bbox[BOXTOP]    = v[1]->V_pos[VY];
        }
        else
        {
            ld->bbox[BOXBOTTOM] = v[1]->V_pos[VY];
            ld->bbox[BOXTOP]    = v[0]->V_pos[VY];
        }
    }
}

/**
 * Builds sector line lists and subsector sector numbers.
 * Finds block bounding boxes for sectors.
 */
static void finalizeMapData(gamemap_t *map)
{
    uint        startTime = Sys_GetRealTime();

    Con_Message(" Sector look up...\n");
    linkSSecsToSectors(map);

    Con_Message(" Build subsector tables...\n");
    buildSectorSSecLists(map);

    Con_Message(" Build line tables...\n");
    buildSectorLineLists(map);

    finishLineDefs(map);

    if(1)//mustCreateBlockMap)
    {
        DAM_BuildBlockMap(map);
    }

    finishSectors(map);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("finalizeMapData: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}

static void updateMapBounds(gamemap_t *map)
{
    uint        i;

    memset(map->bbox, 0, sizeof(map->bbox));
    for(i = 0; i < map->numsectors; ++i)
    {
        sector_t   *sec = &map->sectors[i];

        if(i == 0)
        {
            // The first sector is used as is.
            memcpy(map->bbox, sec->bbox, sizeof(map->bbox));
        }
        else
        {
            // Expand the bounding box.
            M_JoinBoxes(map->bbox, sec->bbox);
        }
    }
}

static void markUnclosedSectors(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numsectors; ++i)
    {
        boolean     unclosed = false;
        sector_t   *sec = &map->sectors[i];

        if(sec->linecount < 3)
        {
            unclosed = true;
        }
        else
        {
            // \todo Add algorithm to check for unclosed sectors here.
            // Perhaps have a look at glBSP.
        }

        if(unclosed)
            sec->flags |= SECF_UNCLOSED;
    }
}

static void updateSSecMidPoint(subsector_t *sub)
{
    seg_t     **ptr;
    fvertex_t  *vtx;

    // Find the center point. First calculate the bounding box.
    ptr = sub->segs;
    vtx = &((*ptr)->SG_v1->v);
    sub->bbox[0].pos[VX] = sub->bbox[1].pos[VX] = sub->midpoint.pos[VX] = vtx->pos[VX];
    sub->bbox[0].pos[VY] = sub->bbox[1].pos[VY] = sub->midpoint.pos[VY] = vtx->pos[VY];

    *ptr++;
    while(*ptr)
    {
        vtx = &((*ptr)->SG_v1->v);
        if(vtx->pos[VX] < sub->bbox[0].pos[VX])
            sub->bbox[0].pos[VX] = vtx->pos[VX];
        if(vtx->pos[VY] < sub->bbox[0].pos[VY])
            sub->bbox[0].pos[VY] = vtx->pos[VY];
        if(vtx->pos[VX] > sub->bbox[1].pos[VX])
            sub->bbox[1].pos[VX] = vtx->pos[VX];
        if(vtx->pos[VY] > sub->bbox[1].pos[VY])
            sub->bbox[1].pos[VY] = vtx->pos[VY];

        sub->midpoint.pos[VX] += vtx->pos[VX];
        sub->midpoint.pos[VY] += vtx->pos[VY];
        *ptr++;
    }

    sub->midpoint.pos[VX] /= sub->segcount; // num vertices.
    sub->midpoint.pos[VY] /= sub->segcount;
}

static void prepareSubSectors(gamemap_t *map)
{
    uint            i;

    for(i = 0; i < map->numsubsectors; ++i)
    {
        subsector_t *ssec = &map->subsectors[i];

        updateSSecMidPoint(ssec);
    }
}

/**
 * Called to complete the map building process.
 */
boolean MPE_End(void)
{
    gamemap_t              *gamemap;

    if(!editMapInited)
        return false;

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
        return false;
    }

    // Call the game's setup routines.
    if(gx.SetupForMapData)
    {
        // gx.SetupForMapData(DAM_THING, map->numthings);
        gx.SetupForMapData(DAM_VERTEX, map->numvertexes);
        gx.SetupForMapData(DAM_LINE, map->numlines);
        gx.SetupForMapData(DAM_SIDE, map->numsides);
        gx.SetupForMapData(DAM_SECTOR, map->numsectors);
    }

    // Build the vertex line owner rings.
    R_BuildVertexOwners(gamemap);
    findMissingFrontSidedefs(gamemap);
    markSelfReferencingLinedefs(gamemap);
    finalizeMapData(gamemap);

    markUnclosedSectors(gamemap);
    updateMapBounds(gamemap);
    S_DetermineSubSecsAffectingSectorReverb(gamemap);
    prepareSubSectors(gamemap);
    // Announce any issues detected with the map.
    MPE_PrintMapErrors(false);

    /**
     * Are we caching this map?
     */
    if(map->name && map->name[0])
    {   // Yes, write the cached map data file.
        filename_t              cachedMapDir;
        filename_t              cachedMapDataFile;
        int                     markerLumpNum;

        markerLumpNum = W_GetNumForName(map->name);
        DAM_GetCachedMapDir(cachedMapDir, markerLumpNum);

        // Ensure the destination path exists.
        M_CheckPath(cachedMapDir);

        sprintf(cachedMapDataFile, "%s%s", cachedMapDir,
                                   W_LumpName(markerLumpNum));
        M_TranslatePath(cachedMapDataFile, cachedMapDataFile);
        strcat(cachedMapDataFile, ".dcm");

        DAM_MapWrite(gamemap, cachedMapDataFile);
    }

    lastBuiltMap = gamemap;

    // Destroy the editable map (we are finished with it).
    destroyMap();
    editMapInited = false;

    // Success!
    return true;
}

gamemap_t *MPE_GetLastBuiltMap(void)
{
    return lastBuiltMap;
}

/**
 * If we encountered any problems during setup - announce them to the user.
 *
 * \todo latter on this will be expanded to check for various
 * doom.exe renderer hacks and other stuff.
 *
 * @param silent        @c true = don't announce non-critical errors.
 *
 * @return              @c true = we can continue setting up the level.
 */
boolean MPE_PrintMapErrors(boolean silent)
{
    uint        i, printCount;
    boolean     canContinue = !numMissingFronts;

    // If we are missing any front sidedefs announce them to the user.
    // Critical
    if(numMissingFronts)
    {
        Con_Message(" ![100] Error: Found %u linedef(s) missing front sidedefs:\n",
                    numMissingFronts);

        printCount = 0;
        for(i = 0; i < numlines; ++i)
        {
            if(missingFronts[i])
            {
                Con_Printf("%s%u,", printCount? " ": "   ", i);

                if((++printCount) > 9)
                {   // print 10 per line then wrap.
                    printCount = 0;
                    Con_Printf("\n ");
                }
            }
        }
        Con_Printf("\n");
    }

    // Announce any bad texture names we came across when loading the map.
    if(!silent)
        P_PrintMissingTextureList();

    // Dont need this stuff anymore
    if(missingFronts != NULL)
        M_Free(missingFronts);

    P_FreeBadTexList();

    if(!canContinue)
    {
        Con_Message("\nP_CheckLevel: Critical errors encountered "
                    "(marked with '!').\n  You will need to fix these errors in "
                    "order to play this map.\n");
        return false;
    }

    return true;
}

uint MPE_VertexCreate(float x, float y)
{
    vertex_t           *v;

    if(!editMapInited)
        return 0;

    v = createVertex();
    v->V_pos[VX] = x;
    v->V_pos[VY] = y;
    v->buildData.pos[VX] = (double) v->V_pos[VX];
    v->buildData.pos[VY] = (double) v->V_pos[VY];

    return v->buildData.index;
}

uint MPE_SidedefCreate(uint sector, short flags,
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
                       float bottomBlue)
{
    int                 matIdx;
    side_t             *s;

    if(!editMapInited)
        return 0;

    if(sector > map->numsectors)
        return 0;

    s = createSide();
    s->flags = flags;
    s->sector = (sector == 0? NULL: map->sectors[sector-1]);

    matIdx = R_CheckMaterialNumForName(topMaterial, topMaterialType);
    s->SW_topmaterial =
        (matIdx == -1? NULL : R_GetMaterial(matIdx, topMaterialType));
    s->SW_topoffset[VX] = topOffsetX;
    s->SW_topoffset[VY] = topOffsetY;
    s->SW_toprgba[CR] = MINMAX_OF(0, topRed, 1);
    s->SW_toprgba[CG] = MINMAX_OF(0, topGreen, 1);
    s->SW_toprgba[CB] = MINMAX_OF(0, topBlue, 1);
    s->SW_toprgba[CA] = 1;

    matIdx = R_CheckMaterialNumForName(middleMaterial, middleMaterialType);
    s->SW_middlematerial =
        (matIdx == -1? NULL : R_GetMaterial(matIdx, middleMaterialType));
    s->SW_middleoffset[VX] = middleOffsetX;
    s->SW_middleoffset[VY] = middleOffsetY;
    s->SW_middlergba[CR] = MINMAX_OF(0, middleRed, 1);
    s->SW_middlergba[CG] = MINMAX_OF(0, middleGreen, 1);
    s->SW_middlergba[CB] = MINMAX_OF(0, middleBlue, 1);
    s->SW_middlergba[CA] = MINMAX_OF(0, middleAlpha, 1);

    matIdx = R_CheckMaterialNumForName(bottomMaterial, bottomMaterialType);
    s->SW_bottommaterial =
        (matIdx == -1? NULL : R_GetMaterial(matIdx, bottomMaterialType));
    s->SW_bottomoffset[VX] = bottomOffsetX;
    s->SW_bottomoffset[VY] = bottomOffsetY;
    s->SW_bottomrgba[CR] = MINMAX_OF(0, bottomRed, 1);
    s->SW_bottomrgba[CG] = MINMAX_OF(0, bottomGreen, 1);
    s->SW_bottomrgba[CB] = MINMAX_OF(0, bottomBlue, 1);
    s->SW_bottomrgba[CA] = 1;

    return s->buildData.index;
}

uint MPE_LinedefCreate(uint v1, uint v2, uint frontSide, uint backSide,
                       short mapflags, int flags)
{
    line_t             *l;
    side_t             *front = NULL, *back = NULL;

    if(!editMapInited)
        return 0;

    if(frontSide > map->numsides)
        return 0;
    if(backSide > map->numsides)
        return 0;
    if(v1 > map->numvertexes)
        return 0;
    if(v2 > map->numvertexes)
        return 0;
    if(v1 == v2)
        return 0;

    // First, ensure that the side indices are unique.
    if(frontSide > 0)
        front = map->sides[frontSide - 1];
    if(backSide > 0)
        back = map->sides[backSide - 1];

    if(front || back)
    {
        uint                i;
        for(i = 0; i < map->numlines; ++i)
        {
            line_t             *other = map->lines[i];

            if(other->L_frontside)
            {
                if((front && front == other->L_frontside) ||
                   (back && back == other->L_frontside))
                {
                    return 0;
                }
            }

            if(other->L_backside)
            {
                if((front && front == other->L_backside) ||
                   (back && back == other->L_backside))
                {
                    return 0;
                }
            }
        }
    }

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

    l->L_frontside = front;
    l->L_backside = back;

    return l->buildData.index;
}

uint MPE_SectorCreate(float lightlevel, float red, float green, float blue,
                      float floorHeight, const char *floorMaterial,
                      materialtype_t floorMaterialType, float floorOffsetX,
                      float floorOffsetY, float floorRed, float floorGreen,
                      float floorBlue, float ceilHeight,
                      const char *ceilMaterial,
                      materialtype_t ceilMaterialType, float ceilOffsetX,
                      float ceilOffsetY, float ceilRed, float ceilGreen,
                      float ceilBlue)
{
    int                 i, matIdx;
    sector_t           *s;

    if(!editMapInited)
        return 0;

    s = createSector();

    s->rgb[CR] = MINMAX_OF(0, red, 1);
    s->rgb[CG] = MINMAX_OF(0, green, 1);
    s->rgb[CB] = MINMAX_OF(0, blue, 1);
    s->lightlevel = MINMAX_OF(0, lightlevel, 1);
    s->buildData.warnedFacing = -1;
    s->planecount = 2;
    s->planes = M_Malloc(sizeof(plane_t*) * (s->planecount+1));
    for(i = 0; i < 2; ++i)
    {
        plane_t            *pln = M_Calloc(sizeof(plane_t));

        if(i == 0)
        {
            matIdx = R_CheckMaterialNumForName(floorMaterial, floorMaterialType);
            pln->height = floorHeight;
            pln->PS_material =
                (matIdx == -1? NULL : R_GetMaterial(matIdx, floorMaterialType));
            pln->PS_rgba[CR] = MINMAX_OF(0, ceilRed, 1);
            pln->PS_rgba[CG] = MINMAX_OF(0, ceilGreen, 1);
            pln->PS_rgba[CB] = MINMAX_OF(0, ceilBlue, 1);
            pln->PS_rgba[CA] = 1;
            pln->PS_offset[VX] = floorOffsetX;
            pln->PS_offset[VY] = floorOffsetY;
            pln->PS_normal[VX] = 0;
            pln->PS_normal[VY] = 0;
            pln->PS_normal[VZ] = 1;
            pln->sector = s;
        }
        else
        {
            matIdx = R_CheckMaterialNumForName(ceilMaterial, ceilMaterialType);
            pln->height = ceilHeight;
            pln->PS_material =
                (matIdx == -1? NULL : R_GetMaterial(matIdx, ceilMaterialType));
            pln->PS_rgba[CR] = MINMAX_OF(0, floorRed, 1);
            pln->PS_rgba[CG] = MINMAX_OF(0, floorGreen, 1);
            pln->PS_rgba[CB] = MINMAX_OF(0, floorBlue, 1);
            pln->PS_rgba[CA] = 1;
            pln->PS_offset[VX] = ceilOffsetX;
            pln->PS_offset[VY] = ceilOffsetY;
            pln->PS_normal[VX] = 0;
            pln->PS_normal[VY] = 0;
            pln->PS_normal[VZ] = -1;
            pln->sector = s;
        }

        s->planes[i] = pln;
    }
    s->planes[i] = NULL; // Terminate.

    return s->buildData.index;
}

uint MPE_PolyobjCreate(uint *lines, uint lineCount, boolean crush,
                       int tag, int sequenceType, float startX,
                       float startY)
{
    uint                i;
    polyobj_t          *po;

    if(!editMapInited)
        return 0;

    po = createPolyobj();
    po->buildData.lines = M_Calloc(sizeof(line_t*) * (lineCount+1));
    for(i = 0; i < lineCount; ++i)
    {
        line_t             *line = map->lines[lines[i]-1];

        // This line is part of a polyobj.
        line->flags |= LINEF_POLYOBJ;

        po->buildData.lines[i] = line;
    }
    po->buildData.lines[i] = NULL;
    po->buildData.lineCount = lineCount;
    po->tag = tag;
    po->seqType = sequenceType;
    po->crush = crush;
    po->startSpot.pos[VX] = startX;
    po->startSpot.pos[VY] = startY;

    return po->buildData.index;
}

subsector_t *BSP_NewSubsector(void)
{
    subsector_t        *s;

    if(!editMapInited)
        return NULL;

    s = createSubsector();
    return s;
}

node_t *BSP_NewNode(void)
{
    node_t             *n;

    if(!editMapInited)
        return NULL;

    n = createNode();
    return n;
}
