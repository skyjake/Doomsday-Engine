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
        M_Realloc(map->vertexes, sizeof(vtx) * (++map->numVertexes + 1));
    map->vertexes[map->numVertexes-1] = vtx;
    map->vertexes[map->numVertexes] = NULL;

    vtx->buildData.index = map->numVertexes; // 1-based index, 0 = NIL.
    return vtx;
}

static line_t *createLine(void)
{
    line_t             *line;

    line = M_Calloc(sizeof(*line));
    line->header.type = DMU_LINE;

    map->lines =
        M_Realloc(map->lines, sizeof(line) * (++map->numLines + 1));
    map->lines[map->numLines-1] = line;
    map->lines[map->numLines] = NULL;

    line->buildData.index = map->numLines; // 1-based index, 0 = NIL.
    return line;
}

static side_t *createSide(void)
{
    side_t             *side;

    side = M_Calloc(sizeof(*side));
    side->header.type = DMU_SIDE;

    map->sides =
        M_Realloc(map->sides, sizeof(side) * (++map->numSides + 1));
    map->sides[map->numSides-1] = side;
    map->sides[map->numSides] = NULL;

    side->buildData.index = map->numSides; // 1-based index, 0 = NIL.
    return side;
}

static sector_t *createSector(void)
{
    sector_t           *sec;

    sec = M_Calloc(sizeof(*sec));
    sec->header.type = DMU_SECTOR;

    map->sectors = M_Realloc(map->sectors, sizeof(sec) * (++map->numSectors + 1));
    map->sectors[map->numSectors-1] = sec;
    map->sectors[map->numSectors] = NULL;

    sec->buildData.index = map->numSectors; // 1-based index, 0 = NIL.
    return sec;
}

static polyobj_t *createPolyobj(void)
{
    polyobj_t          *po;

    po = M_Calloc(sizeof(*po));
    po->header.type = DMU_POLYOBJ;

    map->polyobjs = M_Realloc(map->polyobjs, sizeof(po) * (++map->numPolyobjs + 1));
    map->polyobjs[map->numPolyobjs-1] = po;
    map->polyobjs[map->numPolyobjs] = NULL;

    po->buildData.index = map->numPolyobjs; // 1-based index, 0 = NIL.
    return po;
}

static boolean C_DECL freeBSPNodeData(binarytree_t *tree, void *data)
{
    void               *bspData = BinaryTree_GetData(tree);

    if(bspData)
    {
        if(BinaryTree_IsLeaf(tree))
        {
            subsector_t        *ssec = bspData;
            hedge_t            *hEdge, *np;

            hEdge = ssec->buildData.hEdges;
            while(hEdge)
            {
                np = hEdge->next;
                HEdge_Destroy(hEdge);
                hEdge = np;
            }
        }

        M_Free(bspData);
    }

    BinaryTree_SetData(tree, NULL);

    return true; // Continue iteration.
}

static void destroyMap(void)
{
    memset(map->name, 0, sizeof(map->name));

    if(map->vertexes)
        M_Free(map->vertexes);
    map->vertexes = NULL;
    map->numVertexes = 0;

    if(map->lines)
        M_Free(map->lines);
    map->lines = NULL;
    map->numLines = 0;

    if(map->sides)
        M_Free(map->sides);
    map->sides = NULL;
    map->numSides = 0;

    if(map->sectors)
    {
        uint                i;
        for(i = 0; i < map->numSectors; ++i)
        {
            uint                j;
            sector_t           *s = map->sectors[i];

            if(s->planes)
            {
                for(j = 0; j < s->planeCount; ++j)
                {
                    M_Free(s->planes[j]);
                }
                M_Free(s->planes);
            }
        }
        M_Free(map->sectors);
    }
    map->sectors = NULL;
    map->numSectors = 0;

    if(map->rootNode)
    {
        BinaryTree_PostOrder(map->rootNode, freeBSPNodeData, NULL);
        BinaryTree_Destroy(map->rootNode);
        map->rootNode = NULL;
    }

    if(map->polyobjs)
    {
        uint                i;
        for(i = 0; i < map->numPolyobjs; ++i)
        {
            polyobj_t          *po = map->polyobjs[i];
            M_Free(po->buildData.lines);
            M_Free(po);
        }
        M_Free(map->polyobjs);
    }
    map->polyobjs = NULL;
    map->numPolyobjs = 0;
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
    uint       *hits = M_Malloc(map->numVertexes * sizeof(uint));

    // Sort array of indices.
    for(i = 0; i < map->numVertexes; ++i)
        hits[i] = i;

    qsort(hits, map->numVertexes, sizeof(uint), vertexCompare);

    // Now mark them off.
    for(i = 0; i < map->numVertexes - 1; ++i)
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
    missingFronts = M_Calloc(map->numLines * sizeof(uint));

    for(i = 0; i < map->numLines; ++i)
    {
        line_t     *li = &map->lines[i];

        if(!li->L_frontside)
        {   // A missing front sidedef
            missingFronts[i] = 1;
            numMissingFronts++;
        }
    }
}

static void linkSSecsToSectors(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numSubsectors; ++i)
    {
        subsector_t *ssec = &map->subsectors[i];
        seg_t      **segp;
        boolean     found;

        segp = ssec->segs;
        found = false;
        while(*segp)
        {
            seg_t      *seg = *segp;

            if(!found && seg->sideDef)
            {
                ssec->sector = seg->sideDef->sector;
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
    for(i = 0; i < map->numSubsectors; ++i)
    {
        subsector_t *ssec = &map->subsectors[i];
        if(ssec->sector == sec)
            count++;
    }

    sec->subsectors =
        Z_Malloc((count + 1) * sizeof(subsector_t*), PU_LEVELSTATIC, NULL);

    n = 0;
    for(i = 0; i < map->numSubsectors; ++i)
    {
        subsector_t *ssec = &map->subsectors[i];
        if(ssec->sector == sec)
            sec->subsectors[n++] = ssec;
    }
    sec->subsectors[n] = NULL; // Terminate.
    sec->subsCount = count;
}

/**
 * Build subsector tables for all sectors.
 */
static void buildSectorSSecLists(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numSectors; ++i)
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
    sectorLineLinks = M_Calloc(sizeof(linelink_t*) * map->numSectors);
    totallinks = 0;
    for(i = 0, li = map->lines; i < map->numLines; ++i, li++)
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
            li->L_frontsector->lineCount++;
            totallinks++;
        }

        if(li->L_backside && li->L_backsector != li->L_frontsector)
        {
            link = Z_BlockNewElement(lineLinksBlockSet);

            secIDX = li->L_backsector - map->sectors;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            li->L_backsector->lineCount++;
            totallinks++;
        }
    }

    // Harden the sector line links into arrays.
    {
    line_t    **linebuffer;
    line_t    **linebptr;

    linebuffer = Z_Malloc((totallinks + map->numSectors) * sizeof(line_t*),
                          PU_LEVELSTATIC, 0);
    linebptr = linebuffer;

    for(i = 0, sec = map->sectors; i < map->numSectors; ++i, sec++)
    {
        if(sectorLineLinks[i])
        {
            linelink_t *link = sectorLineLinks[i];
            sec->lines = linebptr;
            j = 0;
            while(link)
            {
                sec->lines[j++] = link->line;
                link = link->next;
            }
            sec->lines[j] = NULL; // terminate.
            sec->lineCount = j;
            linebptr += j + 1;
        }
        else
        {
            sec->lines = NULL;
            sec->lineCount = 0;
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

    sec->subsGroupCount = 1;
    sec->subsGroups =
        Z_Malloc(sizeof(ssecgroup_t) * sec->subsGroupCount, PU_LEVEL, 0);

    sec->subsGroups[0].linked =
        Z_Malloc(sizeof(sector_t*) * sec->planeCount, PU_LEVEL, 0);
    for(i = 0; i < sec->planeCount; ++i)
        sec->subsGroups[0].linked[i] = NULL;
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

    bbox = sec->bBox;

    if(!(sec->lineCount > 0))
    {
        memset(sec->bBox, 0, sizeof(sec->bBox));
        return;
    }

    vtx = sec->lines[0]->L_v1;
    bbox[BOXLEFT] = bbox[BOXRIGHT]  = vtx->V_pos[VX];
    bbox[BOXTOP]  = bbox[BOXBOTTOM] = vtx->V_pos[VY];

    for(i = 1; i < sec->lineCount; ++i)
    {
        vtx = sec->lines[i]->L_v1;

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
    min[VX] = sec->bBox[BOXLEFT];
    min[VY] = sec->bBox[BOXBOTTOM];

    max[VX] = sec->bBox[BOXRIGHT];
    max[VY] = sec->bBox[BOXTOP];
}

static void finishSectors(gamemap_t *map)
{
    uint        i;
    vec2_t      bmapOrigin;
    uint        bmapSize[2];

    P_GetBlockmapBounds(map->blockMap, bmapOrigin, NULL);
    P_GetBlockmapDimensions(map->blockMap, bmapSize);

    for(i = 0; i < map->numSectors; ++i)
    {
        uint        k;
        float       min[2], max[2];
        sector_t   *sec = &map->sectors[i];

        findSectorSSecGroups(sec);

        if(!(sec->lineCount > 0))
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
        sec->soundOrg.pos[VX] = (min[VX] + max[VX]) / 2;
        sec->soundOrg.pos[VY] = (min[VY] + max[VY]) / 2;

        // Set the z height of the sector sound origin
        sec->soundOrg.pos[VZ] = (sec->SP_ceilheight - sec->SP_floorheight) / 2;

        // Set the position of the sound origin for all plane sound origins.
        // Set target heights for all planes.
        for(k = 0; k < sec->planeCount; ++k)
        {
            sec->planes[k]->soundOrg.pos[VX] = sec->soundOrg.pos[VX];
            sec->planes[k]->soundOrg.pos[VY] = sec->soundOrg.pos[VY];
            sec->planes[k]->soundOrg.pos[VZ] = sec->planes[k]->height;

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

    for(i = 0; i < map->numLines; ++i)
    {
        ld = &map->lines[i];

        if(!ld->sides[0]->segCount)
            continue;

        startSeg = ld->sides[0]->segs[0];
        endSeg = ld->sides[0]->segs[ld->sides[0]->segCount - 1];
        ld->v[0] = v[0] = startSeg->SG_v1;
        ld->v[1] = v[1] = endSeg->SG_v2;
        ld->dX = v[1]->V_pos[VX] - v[0]->V_pos[VX];
        ld->dY = v[1]->V_pos[VY] - v[0]->V_pos[VY];

        // Calculate the accurate length of each line.
        ld->length = P_AccurateDistance(ld->dX, ld->dY);
        ld->angle = bamsAtan2((int) (ld->v[1]->V_pos[VY] - ld->v[0]->V_pos[VY]),
                      (int) (ld->v[1]->V_pos[VX] - ld->v[0]->V_pos[VX])) << FRACBITS;

        if(!ld->dX)
            ld->slopeType = ST_VERTICAL;
        else if(!ld->dY)
            ld->slopeType = ST_HORIZONTAL;
        else
        {
            if(ld->dY / ld->dX > 0)
                ld->slopeType = ST_POSITIVE;
            else
                ld->slopeType = ST_NEGATIVE;
        }

        if(v[0]->V_pos[VX] < v[1]->V_pos[VX])
        {
            ld->bBox[BOXLEFT]   = v[0]->V_pos[VX];
            ld->bBox[BOXRIGHT]  = v[1]->V_pos[VX];
        }
        else
        {
            ld->bBox[BOXLEFT]   = v[1]->V_pos[VX];
            ld->bBox[BOXRIGHT]  = v[0]->V_pos[VX];
        }

        if(v[0]->V_pos[VY] < v[1]->V_pos[VY])
        {
            ld->bBox[BOXBOTTOM] = v[0]->V_pos[VY];
            ld->bBox[BOXTOP]    = v[1]->V_pos[VY];
        }
        else
        {
            ld->bBox[BOXBOTTOM] = v[1]->V_pos[VY];
            ld->bBox[BOXTOP]    = v[0]->V_pos[VY];
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

    memset(map->bBox, 0, sizeof(map->bBox));
    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t   *sec = &map->sectors[i];

        if(i == 0)
        {
            // The first sector is used as is.
            memcpy(map->bBox, sec->bBox, sizeof(map->bBox));
        }
        else
        {
            // Expand the bounding box.
            M_JoinBoxes(map->bBox, sec->bBox);
        }
    }
}

static void markUnclosedSectors(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numSectors; ++i)
    {
        boolean     unclosed = false;
        sector_t   *sec = &map->sectors[i];

        if(sec->lineCount < 3)
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
    sub->bBox[0].pos[VX] = sub->bBox[1].pos[VX] = sub->midPoint.pos[VX] = vtx->pos[VX];
    sub->bBox[0].pos[VY] = sub->bBox[1].pos[VY] = sub->midPoint.pos[VY] = vtx->pos[VY];

    *ptr++;
    while(*ptr)
    {
        vtx = &((*ptr)->SG_v1->v);
        if(vtx->pos[VX] < sub->bBox[0].pos[VX])
            sub->bBox[0].pos[VX] = vtx->pos[VX];
        if(vtx->pos[VY] < sub->bBox[0].pos[VY])
            sub->bBox[0].pos[VY] = vtx->pos[VY];
        if(vtx->pos[VX] > sub->bBox[1].pos[VX])
            sub->bBox[1].pos[VX] = vtx->pos[VX];
        if(vtx->pos[VY] > sub->bBox[1].pos[VY])
            sub->bBox[1].pos[VY] = vtx->pos[VY];

        sub->midPoint.pos[VX] += vtx->pos[VX];
        sub->midPoint.pos[VY] += vtx->pos[VY];
        *ptr++;
    }

    sub->midPoint.pos[VX] /= sub->segCount; // num vertices.
    sub->midPoint.pos[VY] /= sub->segCount;
}

static void prepareSubSectors(gamemap_t *map)
{
    uint            i;

    for(i = 0; i < map->numSubsectors; ++i)
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
        // gx.SetupForMapData(DAM_THING, map->numThings);
        gx.SetupForMapData(DAM_VERTEX, map->numVertexes);
        gx.SetupForMapData(DAM_LINE, map->numLines);
        gx.SetupForMapData(DAM_SIDE, map->numSides);
        gx.SetupForMapData(DAM_SECTOR, map->numSectors);
    }

    // Build the vertex line owner rings.
    R_BuildVertexOwners(gamemap);
    findMissingFrontSidedefs(gamemap);
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

    if(sector > map->numSectors)
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

/**
 * Create a new linedef in the editable map.
 *
 * @param v1            Idx of the start vertex.
 * @param v2            Idx of the end vertex.
 * @param frontSide     Idx of the front sidedef.
 * @param backSide      Idx of the back sidedef.
 * @param flags         Currently unused.
 *
 * @return              Idx of the newly created linedef else @c 0 if there
 *                      was an error.
 */
uint MPE_LinedefCreate(uint v1, uint v2, uint frontSide, uint backSide,
                       int flags)
{
    line_t             *l;
    side_t             *front = NULL, *back = NULL;

    if(!editMapInited)
        return 0;

    if(frontSide > map->numSides)
        return 0;
    if(backSide > map->numSides)
        return 0;
    if(v1 > map->numVertexes)
        return 0;
    if(v2 > map->numVertexes)
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
        for(i = 0; i < map->numLines; ++i)
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
    l->L_v1 = (v1 == 0? NULL : map->vertexes[v1-1]);
    l->L_v2 = (v2 == 0? NULL : map->vertexes[v2-1]);

    l->dX = l->L_v2pos[VX] - l->L_v1pos[VX];
    l->dY = l->L_v2pos[VY] - l->L_v1pos[VY];
    l->length = P_AccurateDistance(l->dX, l->dY);

    l->angle =
        bamsAtan2((int) (l->L_v2pos[VY] - l->L_v1pos[VY]),
                  (int) (l->L_v2pos[VX] - l->L_v1pos[VX])) << FRACBITS;

    if(l->dX == 0)
        l->slopeType = ST_VERTICAL;
    else if(l->dY == 0)
        l->slopeType = ST_HORIZONTAL;
    else
    {
        if(l->dY / l->dX > 0)
            l->slopeType = ST_POSITIVE;
        else
            l->slopeType = ST_NEGATIVE;
    }

    if(l->L_v1pos[VX] < l->L_v2pos[VX])
    {
        l->bBox[BOXLEFT]   = l->L_v1pos[VX];
        l->bBox[BOXRIGHT]  = l->L_v2pos[VX];
    }
    else
    {
        l->bBox[BOXLEFT]   = l->L_v2pos[VX];
        l->bBox[BOXRIGHT]  = l->L_v1pos[VX];
    }

    if(l->L_v1pos[VY] < l->L_v2pos[VY])
    {
        l->bBox[BOXBOTTOM] = l->L_v1pos[VY];
        l->bBox[BOXTOP]    = l->L_v2pos[VY];
    }
    else
    {
        l->bBox[BOXBOTTOM] = l->L_v1pos[VY];
        l->bBox[BOXTOP]    = l->L_v2pos[VY];
    }

    l->L_frontside = front;
    l->L_backside = back;

    // Determine the default linedef flags.
    l->flags = 0;
    if(front && back)
        l->flags |= DDLF_TWOSIDED;
    else
        l->flags |= DDLF_BLOCKING;

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
    s->lightLevel = MINMAX_OF(0, lightlevel, 1);
    s->buildData.warnedFacing = -1;
    s->planeCount = 2;
    s->planes = M_Malloc(sizeof(plane_t*) * (s->planeCount+1));
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

uint MPE_PolyobjCreate(uint *lines, uint lineCount, int tag,
                       int sequenceType, float anchorX, float anchorY)
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
        line->inFlags |= LF_POLYOBJ;

        po->buildData.lines[i] = line;
    }
    po->buildData.lines[i] = NULL;
    po->buildData.lineCount = lineCount;
    po->tag = tag;
    po->seqType = sequenceType;
    po->startSpot.pos[VX] = anchorX;
    po->startSpot.pos[VY] = anchorY;

    return po->buildData.index;
}
