/**\file edit_map.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_bsp.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "de_edit.h"
#include "de_dam.h"
#include "de_filesys.h"

#include "s_environ.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct usecrecord_s {
    sector_t           *sec;
    double              nearPos[2];
} usecrecord_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void MPE_PrintMapErrors(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

editmap_t editMap;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean editMapInited = false;
static editmap_t* map = &editMap;

static GameMap *lastBuiltMap = NULL;

static uint numUnclosedSectors;
static usecrecord_t *unclosedSectors;

static vertex_t *rootVtx; // Used when sorting vertex line owners.

// CODE --------------------------------------------------------------------

vertex_t* createVertex(void)
{
    vertex_t*               vtx;

    vtx = M_Calloc(sizeof(*vtx));
    vtx->header.type = DMU_VERTEX;

    map->vertexes =
        M_Realloc(map->vertexes, sizeof(vtx) * (++map->numVertexes + 1));
    map->vertexes[map->numVertexes-1] = vtx;
    map->vertexes[map->numVertexes] = NULL;

    vtx->buildData.index = map->numVertexes; // 1-based index, 0 = NIL.
    return vtx;
}

static linedef_t* createLine(void)
{
    linedef_t*              line;

    line = M_Calloc(sizeof(*line));
    line->header.type = DMU_LINEDEF;

    map->lineDefs =
        M_Realloc(map->lineDefs, sizeof(line) * (++map->numLineDefs + 1));
    map->lineDefs[map->numLineDefs-1] = line;
    map->lineDefs[map->numLineDefs] = NULL;

    line->buildData.index = map->numLineDefs; // 1-based index, 0 = NIL.
    return line;
}

static sidedef_t* createSide(void)
{
    sidedef_t*              side;

    side = M_Calloc(sizeof(*side));
    side->header.type = DMU_SIDEDEF;

    map->sideDefs =
        M_Realloc(map->sideDefs, sizeof(side) * (++map->numSideDefs + 1));
    map->sideDefs[map->numSideDefs-1] = side;
    map->sideDefs[map->numSideDefs] = NULL;

    side->buildData.index = map->numSideDefs; // 1-based index, 0 = NIL.
    side->SW_bottomsurface.owner = (void*) side;
    side->SW_middlesurface.owner = (void*) side;
    side->SW_topsurface.owner = (void*) side;
    return side;
}

static sector_t* createSector(void)
{
    sector_t*           sec;

    sec = M_Calloc(sizeof(*sec));
    sec->header.type = DMU_SECTOR;

    map->sectors = M_Realloc(map->sectors, sizeof(sec) * (++map->numSectors + 1));
    map->sectors[map->numSectors-1] = sec;
    map->sectors[map->numSectors] = NULL;

    sec->buildData.index = map->numSectors; // 1-based index, 0 = NIL.
    return sec;
}

static polyobj_t* createPolyobj(void)
{
    polyobj_t*          po;

    po = M_Calloc(sizeof(*po));

    map->polyObjs = M_Realloc(map->polyObjs, sizeof(po) * (++map->numPolyObjs + 1));
    map->polyObjs[map->numPolyObjs-1] = po;
    map->polyObjs[map->numPolyObjs] = NULL;

    po->buildData.index = map->numPolyObjs; // 1-based index, 0 = NIL.
    return po;
}

static void destroyEditablePolyObjs(editmap_t* map)
{
    if(map->polyObjs)
    {
        uint i;
        for(i = 0; i < map->numPolyObjs; ++i)
        {
            polyobj_t* po = map->polyObjs[i];
            M_Free(po->lines);
            M_Free(po);
        }
        M_Free(map->polyObjs);
    }
    map->polyObjs = NULL;
    map->numPolyObjs = 0;
}

static void destroyEditableLineDefs(editmap_t* map)
{
    if(map->lineDefs)
    {
        uint i;
        for(i = 0; i < map->numLineDefs; ++i)
        {
            linedef_t* line = map->lineDefs[i];
            M_Free(line);
        }

        M_Free(map->lineDefs);
    }
    map->lineDefs = NULL;
    map->numLineDefs = 0;
}

static void destroyEditableSideDefs(editmap_t *map)
{
    if(map->sideDefs)
    {
        uint                i;
        for(i = 0; i < map->numSideDefs; ++i)
        {
            sidedef_t             *side = map->sideDefs[i];
            M_Free(side);
        }

        M_Free(map->sideDefs);
    }
    map->sideDefs = NULL;
    map->numSideDefs = 0;
}

static void destroyEditableSectors(editmap_t *map)
{
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

            M_Free(s);
        }

        M_Free(map->sectors);
    }
    map->sectors = NULL;
    map->numSectors = 0;
}

static void destroyEditableVertexes(editmap_t *map)
{
    if(map->vertexes)
    {
        uint                i;
        for(i = 0; i < map->numVertexes; ++i)
        {
            vertex_t           *vtx = map->vertexes[i];
            edgetip_t          *tip, *n;

            tip = vtx->buildData.tipSet;
            while(tip)
            {
                n = tip->ET_next;
                BSP_DestroyVertexEdgeTip(tip);
                tip = n;
            }

            M_Free(vtx);
        }

        M_Free(map->vertexes);
    }
    map->vertexes = NULL;
    map->numVertexes = 0;
}

static void destroyMap(void)
{
    destroyEditableVertexes(map);

    // These should already be gone:
    destroyEditableLineDefs(map);
    destroyEditableSideDefs(map);
    destroyEditableSectors(map);
    destroyEditablePolyObjs(map);
}

static int C_DECL vertexCompare(const void* p1, const void* p2)
{
    const vertex_t* a = *((const void**) p1);
    const vertex_t* b = *((const void**) p2);

    if(a == b) return 0;

    if((int) a->buildData.pos[VX] != (int) b->buildData.pos[VX])
        return (int) a->buildData.pos[VX] - (int) b->buildData.pos[VX];

    return (int) a->buildData.pos[VY] - (int) b->buildData.pos[VY];
}

void MPE_DetectDuplicateVertices(editmap_t *map)
{
    size_t              i;
    vertex_t          **hits;

    hits = M_Malloc(map->numVertexes * sizeof(vertex_t*));

    // Sort array of ptrs.
    for(i = 0; i < map->numVertexes; ++i)
        hits[i] = map->vertexes[i];
    qsort(hits, map->numVertexes, sizeof(vertex_t*), vertexCompare);

    // Now mark them off.
    for(i = 0; i < map->numVertexes - 1; ++i)
    {
        // A duplicate?
        if(vertexCompare(hits + i, hits + i + 1) == 0)
        {   // Yes.
            vertex_t           *a = hits[i];
            vertex_t           *b = hits[i + 1];

            b->buildData.equiv =
                (a->buildData.equiv ? a->buildData.equiv : a);
        }
    }

    M_Free(hits);
}

#if 0
static void findEquivalentVertexes(editmap_t *src)
{
    uint            i, newNum;

    // Scan all linedefs.
    for(i = 0, newNum = 0; i < src->numLineDefs; ++i)
    {
        linedef_t         *l = src->lineDefs[i];

        // Handle duplicated vertices.
        while(l->v[0]->buildData.equiv)
        {
            l->v[0]->buildData.refCount--;
            l->v[0] = l->v[0]->buildData.equiv;
            l->v[0]->buildData.refCount++;
        }

        while(l->v[1]->buildData.equiv)
        {
            l->v[1]->buildData.refCount--;
            l->v[1] = l->v[1]->buildData.equiv;
            l->v[1]->buildData.refCount++;
        }

        l->buildData.index = newNum + 1;
        src->lineDefs[newNum++] = src->lineDefs[i];
    }
}

static void pruneLinedefs(editmap_t* map)
{
    uint                i, newNum, unused = 0;

    for(i = 0, newNum = 0; i < map->numLineDefs; ++i)
    {
        linedef_t*          l = map->lineDefs[i];

        if(!l->L_frontside && !l->L_backside)
        {
            unused++;

            M_Free(l);
            continue;
        }

        l->buildData.index = newNum + 1;
        map->lineDefs[newNum++] = l;
    }

    if(newNum < map->numLineDefs)
    {
        if(unused > 0)
            Con_Message("  Pruned %d unused linedefs\n", unused);

        map->numLineDefs = newNum;
    }
}

static void pruneVertices(editmap_t* map)
{
    uint                i, newNum, unused = 0;

    // Scan all vertices.
    for(i = 0, newNum = 0; i < map->numVertexes; ++i)
    {
        vertex_t           *v = map->vertexes[i];

        if(v->buildData.refCount < 0)
            Con_Error("Vertex %d ref_count is %d", i, v->buildData.refCount);

        if(v->buildData.refCount == 0)
        {
            if(v->buildData.equiv == NULL)
                unused++;

            M_Free(v);
            continue;
        }

        v->buildData.index = newNum + 1;
        map->vertexes[newNum++] = v;
    }

    if(newNum < map->numVertexes)
    {
        int         dupNum = map->numVertexes - newNum - unused;

        if(unused > 0)
            Con_Message("  Pruned %d unused vertices.\n", unused);

        if(dupNum > 0)
            Con_Message("  Pruned %d duplicate vertices\n", dupNum);

        map->numVertexes = newNum;
    }
}

static void pruneUnusedSidedefs(editmap_t* map)
{
    uint                i, newNum, unused = 0;

    for(i = 0, newNum = 0; i < map->numSideDefs; ++i)
    {
        sidedef_t*          s = map->sideDefs[i];

        if(s->buildData.refCount == 0)
        {
            unused++;

            M_Free(s);
            continue;
        }

        s->buildData.index = newNum + 1;
        map->sideDefs[newNum++] = s;
    }

    if(newNum < map->numSideDefs)
    {
        int                 dupNum = map->numSideDefs - newNum - unused;

        if(unused > 0)
            Con_Message("  Pruned %d unused sidedefs\n", unused);

        if(dupNum > 0)
            Con_Message("  Pruned %d duplicate sidedefs\n", dupNum);

        map->numSideDefs = newNum;
    }
}

static void pruneUnusedSectors(editmap_t* map)
{
    uint                i, newNum;

    for(i = 0; i < map->numSideDefs; ++i)
    {
        sidedef_t*          s = map->sideDefs[i];

        if(s->sector)
            s->sector->buildData.refCount++;
    }

    // Scan all sectors.
    for(i = 0, newNum = 0; i < map->numSectors; ++i)
    {
        sector_t*           s = map->sectors[i];

        if(s->buildData.refCount == 0)
        {
            M_Free(s);
            continue;
        }

        s->buildData.index = newNum + 1;
        map->sectors[newNum++] = s;
    }

    if(newNum < map->numSectors)
    {
        Con_Message("  Pruned %d unused sectors\n", map->numSectors - newNum);
        map->numSectors = newNum;
    }
}
#endif

/**
 * \note Order here is critical!
 */
void MPE_PruneRedundantMapData(editmap_t *map, int flags)
{
#if 0
    /**
     * \fixme Pruning cannot be done as game map data object properties
     * are currently indexed by their original indices as determined by the
     * position in the map data. The same problem occurs within ACS scripts
     * and XG line/sector references.
     */
    if(!editMapInited)
        return;

    findEquivalentVertexes(map);

    if(flags & PRUNE_LINEDEFS)
        pruneLinedefs(map);

    if(flags & PRUNE_VERTEXES)
        pruneVertices(map);

    if(flags & PRUNE_SIDEDEFS)
        pruneUnusedSidedefs(map);

    if(flags & PRUNE_SECTORS)
        pruneUnusedSectors(map);
#endif
}

/**
 * Register the specified sector in the list of unclosed sectors.
 *
 * @param sec           Ptr to the sector to be registered.
 * @param x             Approximate X coordinate to the sector's origin.
 * @param y             Approximate Y coordinate to the sector's origin.
 *
 * @return              @c true, if sector was registered.
 */
boolean MPE_RegisterUnclosedSectorNear(sector_t *sec, double x, double y)
{
    uint                i;
    usecrecord_t       *usec;

    if(!sec)
        return false; // Wha?

    // Has this sector already been registered as unclosed?
    for(i = 0; i < numUnclosedSectors; ++i)
    {
        if(unclosedSectors[i].sec == sec)
            return true;
    }

    // A new one.
    unclosedSectors = M_Realloc(unclosedSectors,
                                ++numUnclosedSectors * sizeof(usecrecord_t));
    usec = &unclosedSectors[numUnclosedSectors-1];
    usec->sec = sec;
    usec->nearPos[VX] = x;
    usec->nearPos[VY] = y;

    // Flag the sector as unclosed.
    sec->flags |= SECF_UNCLOSED;

    return true;
}

/**
 * Print the list of unclosed sectors.
 */
void MPE_PrintUnclosedSectorList(void)
{
    uint                i;

    if(!editMapInited)
        return;

    for(i = 0; i < numUnclosedSectors; ++i)
    {
        usecrecord_t       *usec = &unclosedSectors[i];

        Con_Message("Sector #%d is unclosed near (%1.1f,%1.1f)\n",
                    usec->sec->buildData.index - 1, usec->nearPos[VX],
                    usec->nearPos[VY]);
    }
}

/**
 * Free the list of unclosed sectors.
 */
void MPE_FreeUnclosedSectorList(void)
{
    if(unclosedSectors)
        M_Free(unclosedSectors);
    unclosedSectors = NULL;
    numUnclosedSectors = 0;
}

/**
 * Called to begin the map building process.
 */
boolean MPE_Begin(const char* mapUri)
{
    if(editMapInited)
        return true; // Already been here.

    MPE_FreeUnclosedSectorList();

    // Init the gameObj lists, and value db.
    map->gameObjData.db.numTables = 0;
    map->gameObjData.db.tables = NULL;
    map->gameObjData.objLists = NULL;

    destroyMap();

    editMapInited = true;
    return true;
}

static void hardenSectorSSecList(GameMap* map, uint secIDX)
{
    assert(map && secIDX < map->numSectors);
    {
    sector_t* sec = &map->sectors[secIDX];
    uint i, n, count;

    count = 0;
    for(i = 0; i < map->numSubsectors; ++i)
    {
        subsector_t *ssec = &map->subsectors[i];
        if(ssec->sector == sec)
            ++count;
    }

    if(0 == count)
        return;

    sec->subsectors = Z_Malloc((count + 1) * sizeof(subsector_t*), PU_MAPSTATIC, NULL);

    n = 0;
    for(i = 0; i < map->numSubsectors; ++i)
    {
        subsector_t* ssec = &map->subsectors[i];
        if(ssec->sector == sec)
        {
            ssec->inSectorID = n;
            sec->subsectors[n++] = ssec;
        }
    }
    sec->subsectors[n] = NULL; // Terminate.
    sec->subsectorCount = count;
    }
}

/**
 * Build subsector tables for all sectors.
 */
static void buildSectorSSecLists(GameMap *map)
{
    VERBOSE( Con_Message(" Build subsector tables...\n") )
    { uint i;
    for(i = 0; i < map->numSectors; ++i)
    {
        hardenSectorSSecList(map, i);
    }}
}

static void buildSectorLineLists(GameMap* map)
{
    typedef struct linelink_s {
        linedef_t* line;
        struct linelink_s* next;
    } linelink_t;

    uint i, j;
    linedef_t* li;
    sector_t* sec;

    zblockset_t* lineLinksBlockSet;
    linelink_t** sectorLineLinks;
    uint totallinks;

    VERBOSE( Con_Message(" Build line tables...\n") )

    // build line tables for each sector.
    lineLinksBlockSet = ZBlockSet_New(sizeof(linelink_t), 512, PU_APPSTATIC);
    sectorLineLinks = M_Calloc(sizeof(linelink_t*) * map->numSectors);
    totallinks = 0;
    for(i = 0, li = map->lineDefs; i < map->numLineDefs; ++i, li++)
    {
        uint secIDX;
        linelink_t* link;

        if(li->L_frontside)
        {
            link = ZBlockSet_Allocate(lineLinksBlockSet);

            secIDX = li->L_frontsector - map->sectors;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            li->L_frontsector->lineDefCount++;
            totallinks++;
        }

        if(li->L_backside && li->L_backsector != li->L_frontsector)
        {
            link = ZBlockSet_Allocate(lineLinksBlockSet);

            secIDX = li->L_backsector - map->sectors;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            li->L_backsector->lineDefCount++;
            totallinks++;
        }
    }

    // Harden the sector line links into arrays.
    {
    linedef_t** linebuffer;
    linedef_t** linebptr;

    linebuffer = Z_Malloc((totallinks + map->numSectors) * sizeof(linedef_t*),
                          PU_MAPSTATIC, 0);
    linebptr = linebuffer;

    for(i = 0, sec = map->sectors; i < map->numSectors; ++i, sec++)
    {
        if(sectorLineLinks[i])
        {
            linelink_t* link = sectorLineLinks[i];
            uint numLineDefs;

            /**
             * The behaviour of some algorithms used in original DOOM are
             * dependant upon the order of these lists (e.g., EV_DoFloor
             * and EV_BuildStairs). Lets be helpful and use the same order.
             *
             * Sort: LineDef index ascending (zero based).
             */
            numLineDefs = 0;
            while(link)
            {
                numLineDefs++;
                link = link->next;
            }

            sec->lineDefs = linebptr;
            j = numLineDefs - 1;
            link = sectorLineLinks[i];
            while(link)
            {
                sec->lineDefs[j--] = link->line;
                link = link->next;
            }

            sec->lineDefs[numLineDefs] = NULL; // terminate.
            sec->lineDefCount = numLineDefs;
            linebptr += numLineDefs + 1;
        }
        else
        {
            sec->lineDefs = NULL;
            sec->lineDefCount = 0;
        }
    }
    }

    // Free temporary storage.
    ZBlockSet_Delete(lineLinksBlockSet);
    M_Free(sectorLineLinks);
}

/**
 * \pre Lines in sector must be setup before this is called!
 */
static void updateSectorBounds(sector_t* sec)
{
    uint                i;
    float*              bbox;
    vertex_t*           vtx;

    if(!sec)
        return;

    bbox = sec->bBox;

    if(!(sec->lineDefCount > 0))
    {
        memset(sec->bBox, 0, sizeof(sec->bBox));
        return;
    }

    vtx = sec->lineDefs[0]->L_v1;
    bbox[BOXLEFT] = bbox[BOXRIGHT]  = vtx->V_pos[VX];
    bbox[BOXTOP]  = bbox[BOXBOTTOM] = vtx->V_pos[VY];

    for(i = 1; i < sec->lineDefCount; ++i)
    {
        vtx = sec->lineDefs[i]->L_v1;

        if(vtx->V_pos[VX] < bbox[BOXLEFT])
            bbox[BOXLEFT]   = vtx->V_pos[VX];
        if(vtx->V_pos[VX] > bbox[BOXRIGHT])
            bbox[BOXRIGHT]  = vtx->V_pos[VX];
        if(vtx->V_pos[VY] < bbox[BOXBOTTOM])
            bbox[BOXBOTTOM] = vtx->V_pos[VY];
        if(vtx->V_pos[VY] > bbox[BOXTOP])
            bbox[BOXTOP]   = vtx->V_pos[VY];
    }

    // This is very rough estimate of sector area.
    sec->approxArea = ((bbox[BOXRIGHT] - bbox[BOXLEFT]) / 128) *
        ((bbox[BOXTOP] - bbox[BOXBOTTOM]) / 128);
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

static void finishSectors(GameMap* map)
{
    uint i;
    for(i = 0; i < map->numSectors; ++i)
    {
        uint k;
        float min[2], max[2];
        sector_t* sec = &map->sectors[i];

        updateSectorBounds(sec);
        P_GetSectorBounds(sec, min, max);

        // Set the degenmobj_t to the middle of the bounding box.
        sec->soundOrg.pos[VX] = (min[VX] + max[VX]) / 2;
        sec->soundOrg.pos[VY] = (min[VY] + max[VY]) / 2;

        // Set the z height of the sector sound origin.
        sec->soundOrg.pos[VZ] =
            (sec->SP_ceilheight - sec->SP_floorheight) / 2;

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
static void finishLineDefs(GameMap* map)
{
    uint                i;
    linedef_t          *ld;
    vertex_t           *v[2];
    HEdge              *startSeg, *endSeg;

    VERBOSE2( Con_Message("Finalizing Linedefs...\n") )

    for(i = 0; i < map->numLineDefs; ++i)
    {
        ld = &map->lineDefs[i];

        if(!ld->sideDefs[0]->hedgeCount)
            continue;

        startSeg = ld->sideDefs[0]->hedges[0];
        endSeg = ld->sideDefs[0]->hedges[ld->sideDefs[0]->hedgeCount - 1];
        ld->v[0] = v[0] = startSeg->HE_v1;
        ld->v[1] = v[1] = endSeg->HE_v2;
        ld->dX = v[1]->V_pos[VX] - v[0]->V_pos[VX];
        ld->dY = v[1]->V_pos[VY] - v[0]->V_pos[VY];

        // Calculate the accurate length of each line.
        ld->length = P_AccurateDistance(ld->dX, ld->dY);
        ld->angle = bamsAtan2((int) (ld->v[1]->V_pos[VY] - ld->v[0]->V_pos[VY]),
                      (int) (ld->v[1]->V_pos[VX] - ld->v[0]->V_pos[VX]));

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
            ld->aaBox.minX = v[0]->V_pos[VX];
            ld->aaBox.maxX = v[1]->V_pos[VX];
        }
        else
        {
            ld->aaBox.minX = v[1]->V_pos[VX];
            ld->aaBox.maxX = v[0]->V_pos[VX];
        }

        if(v[0]->V_pos[VY] < v[1]->V_pos[VY])
        {
            ld->aaBox.minY = v[0]->V_pos[VY];
            ld->aaBox.maxY = v[1]->V_pos[VY];
        }
        else
        {
            ld->aaBox.minY = v[1]->V_pos[VY];
            ld->aaBox.maxY = v[0]->V_pos[VY];
        }
    }
}

static void updateMapBounds(GameMap* map)
{
    assert(map);
    {
    boolean isFirst = true;
    memset(map->bBox, 0, sizeof(map->bBox));
    { uint i;
    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sec = &map->sectors[i];
        if(0 == sec->lineDefCount)
            continue;
        if(isFirst)
        {   // The first sector is used as is.
            memcpy(map->bBox, sec->bBox, sizeof(map->bBox));
        }
        else
        {   // Expand the bounding box.
            M_JoinBoxes(map->bBox, sec->bBox);
        }
        isFirst = false;
    }}
    }
}

static void updateSSecMidPoint(subsector_t *sub)
{
    HEdge             **ptr;
    fvertex_t          *vtx;

    // Find the center point. First calculate the bounding box.
    ptr = sub->hedges;
    vtx = &((*ptr)->HE_v1->v);
    sub->aaBox.minX = sub->aaBox.maxX = sub->midPoint.pos[VX] = vtx->pos[VX];
    sub->aaBox.minY = sub->aaBox.maxY = sub->midPoint.pos[VY] = vtx->pos[VY];

    ptr++;
    while(*ptr)
    {
        vtx = &((*ptr)->HE_v1->v);
        if(vtx->pos[VX] < sub->aaBox.minX)
            sub->aaBox.minX = vtx->pos[VX];
        if(vtx->pos[VY] < sub->aaBox.minY)
            sub->aaBox.minY = vtx->pos[VY];
        if(vtx->pos[VX] > sub->aaBox.maxX)
            sub->aaBox.maxX = vtx->pos[VX];
        if(vtx->pos[VY] > sub->aaBox.maxY)
            sub->aaBox.maxY = vtx->pos[VY];

        sub->midPoint.pos[VX] += vtx->pos[VX];
        sub->midPoint.pos[VY] += vtx->pos[VY];
        ptr++;
    }

    sub->midPoint.pos[VX] /= sub->hedgeCount; // num vertices.
    sub->midPoint.pos[VY] /= sub->hedgeCount;

    // Calculate the worldwide grid offset.
    sub->worldGridOffset[VX] = fmod(sub->aaBox.minX, 64);
    sub->worldGridOffset[VY] = fmod(sub->aaBox.maxY, 64);
}

static void prepareSubsectors(GameMap* map)
{
    uint i;

    for(i = 0; i < map->numSubsectors; ++i)
    {
        subsector_t* ssec = &map->subsectors[i];

        updateSSecMidPoint(ssec);
    }
}

/**
 * Compares the angles of two lines that share a common vertex.
 *
 * pre: rootVtx must point to the vertex common between a and b
 *      which are (lineowner_t*) ptrs.
 */
static int C_DECL lineAngleSorter(const void *a, const void *b)
{
    uint                i;
    fixed_t             dx, dy;
    binangle_t          angles[2];
    lineowner_t        *own[2];
    linedef_t          *line;

    own[0] = (lineowner_t *)a;
    own[1] = (lineowner_t *)b;
    for(i = 0; i < 2; ++i)
    {
        if(own[i]->LO_prev) // We have a cached result.
        {
            angles[i] = own[i]->angle;
        }
        else
        {
            vertex_t    *otherVtx;

            line = own[i]->lineDef;
            otherVtx = line->L_v(line->L_v1 == rootVtx? 1:0);

            dx = otherVtx->V_pos[VX] - rootVtx->V_pos[VX];
            dy = otherVtx->V_pos[VY] - rootVtx->V_pos[VY];

            own[i]->angle = angles[i] = bamsAtan2(-100 *dx, 100 * dy);

            // Mark as having a cached angle.
            own[i]->LO_prev = (lineowner_t*) 1;
        }
    }

    return (angles[1] - angles[0]);
}

/**
 * Merge left and right line owner lists into a new list.
 *
 * @return              Ptr to the newly merged list.
 */
static lineowner_t *mergeLineOwners(lineowner_t *left, lineowner_t *right,
                                    int (C_DECL *compare) (const void *a,
                                                           const void *b))
{
    lineowner_t         tmp, *np;

    np = &tmp;
    tmp.LO_next = np;
    while(left != NULL && right != NULL)
    {
        if(compare(left, right) <= 0)
        {
            np->LO_next = left;
            np = left;

            left = left->LO_next;
        }
        else
        {
            np->LO_next = right;
            np = right;

            right = right->LO_next;
        }
    }

    // At least one of these lists is now empty.
    if(left)
        np->LO_next = left;
    if(right)
        np->LO_next = right;

    // Is the list empty?
    if(tmp.LO_next == &tmp)
        return NULL;

    return tmp.LO_next;
}

static lineowner_t *splitLineOwners(lineowner_t *list)
{
    lineowner_t        *lista, *listb, *listc;

    if(!list)
        return NULL;

    lista = listb = listc = list;
    do
    {
        listc = listb;
        listb = listb->LO_next;
        lista = lista->LO_next;
        if(lista != NULL)
            lista = lista->LO_next;
    } while(lista);

    listc->LO_next = NULL;
    return listb;
}

/**
 * This routine uses a recursive mergesort algorithm; O(NlogN)
 */
static lineowner_t *sortLineOwners(lineowner_t *list,
                                   int (C_DECL *compare) (const void *a,
                                                          const void *b))
{
    lineowner_t        *p;

    if(list && list->LO_next)
    {
        p = splitLineOwners(list);

        // Sort both halves and merge them back.
        list = mergeLineOwners(sortLineOwners(list, compare),
                               sortLineOwners(p, compare), compare);
    }
    return list;
}

static void setVertexLineOwner(vertex_t *vtx, linedef_t *lineptr,
                               lineowner_t **storage)
{
    lineowner_t        *p, *newOwner;

    if(!lineptr)
        return;

    // Has this line already been registered with this vertex?
    if(vtx->numLineOwners != 0)
    {
        p = vtx->lineOwners;
        while(p)
        {
            if(p->lineDef == lineptr)
                return;             // Yes, we can exit.

            p = p->LO_next;
        }
    }

    //Add a new owner.
    vtx->numLineOwners++;

    newOwner = (*storage)++;
    newOwner->lineDef = lineptr;
    newOwner->LO_prev = NULL;

    // Link it in.
    // NOTE: We don't bother linking everything at this stage since we'll
    // be sorting the lists anyway. After which we'll finish the job by
    // setting the prev and circular links.
    // So, for now this is only linked singlely, forward.
    newOwner->LO_next = vtx->lineOwners;
    vtx->lineOwners = newOwner;

    // Link the line to its respective owner node.
    if(vtx == lineptr->L_v1)
        lineptr->L_vo1 = newOwner;
    else
        lineptr->L_vo2 = newOwner;
}

/**
 * Generates the line owner rings for each vertex. Each ring includes all
 * the lines which the vertex belongs to sorted by angle, (the rings are
 * arranged in clockwise order, east = 0).
 */
static void buildVertexOwnerRings(editmap_t* map)
{
    uint                i;
    lineowner_t*        lineOwners, *allocator;

    // We know how many vertex line owners we need (numLineDefs * 2).
    lineOwners =
        Z_Malloc(sizeof(lineowner_t) * map->numLineDefs * 2, PU_MAPSTATIC, 0);
    allocator = lineOwners;

    for(i = 0; i < map->numLineDefs; ++i)
    {
        uint                p;
        linedef_t*          line = map->lineDefs[i];

        for(p = 0; p < 2; ++p)
        {
            vertex_t*           vtx = line->L_v(p);

            setVertexLineOwner(vtx, line, &allocator);
        }
    }
}

static void hardenVertexOwnerRings(GameMap* dest, editmap_t* src)
{
    uint                i;

    // Sort line owners and then finish the rings.
    for(i = 0; i < src->numVertexes; ++i)
    {
        vertex_t*           v = src->vertexes[i];

        // Line owners:
        if(v->numLineOwners != 0)
        {
            lineowner_t*        p, *last;
            binangle_t          firstAngle;

            // Redirect the linedef links to the hardened map.
            p = v->lineOwners;
            while(p)
            {
                p->lineDef =
                    &dest->lineDefs[p->lineDef->buildData.index - 1];
                p = p->LO_next;
            }

            // Sort them; ordered clockwise by angle.
            rootVtx = v;
            v->lineOwners =
                sortLineOwners(v->lineOwners, lineAngleSorter);

            // Finish the linking job and convert to relative angles.
            // They are only singly linked atm, we need them to be doubly
            // and circularly linked.
            firstAngle = v->lineOwners->angle;
            last = v->lineOwners;
            p = last->LO_next;
            while(p)
            {
                p->LO_prev = last;

                // Convert to a relative angle between last and this.
                last->angle = last->angle - p->angle;

                last = p;
                p = p->LO_next;
            }
            last->LO_next = v->lineOwners;
            v->lineOwners->LO_prev = last;

            // Set the angle of the last owner.
            last->angle = last->angle - firstAngle;
/*
#if _DEBUG
{
// For checking the line owner link rings are formed correctly.
lineowner_t *base;
uint        idx;

if(verbose >= 2)
    Con_Message("Vertex #%i: line owners #%i\n", i, v->numLineOwners);

p = base = v->lineOwners;
idx = 0;
do
{
    if(verbose >= 2)
        Con_Message("  %i: p= #%05i this= #%05i n= #%05i, dANG= %-3.f\n",
                    idx, p->LO_prev->line - map->lineDefs,
                    p->line - map->lineDefs,
                    p->LO_next->line - map->lineDefs, BANG2DEG(p->angle));

    if(p->LO_prev->LO_next != p || p->LO_next->LO_prev != p)
       Con_Error("Invalid line owner link ring!");

    p = p->LO_next;
    idx++;
} while(p != base);
}
#endif
*/
        }
    }
}

static void hardenLinedefs(GameMap *dest, editmap_t *src)
{
    uint                i;

    dest->numLineDefs = src->numLineDefs;
    dest->lineDefs = Z_Calloc(dest->numLineDefs * sizeof(linedef_t), PU_MAPSTATIC, 0);

    for(i = 0; i < dest->numLineDefs; ++i)
    {
        linedef_t          *destL = &dest->lineDefs[i];
        linedef_t          *srcL = src->lineDefs[i];

        memcpy(destL, srcL, sizeof(*destL));

        //// \todo We shouldn't still have lines with missing fronts but...
        destL->L_frontside = (srcL->L_frontside?
            &dest->sideDefs[srcL->L_frontside->buildData.index - 1] : NULL);
        destL->L_backside = (srcL->L_backside?
            &dest->sideDefs[srcL->L_backside->buildData.index - 1] : NULL);
        if(destL->L_frontside)
            destL->L_frontside->line = destL;
        if(destL->L_backside)
            destL->L_backside->line = destL;
    }
}

static void hardenSidedefs(GameMap* dest, editmap_t* src)
{
    uint                i;

    dest->numSideDefs = src->numSideDefs;
    dest->sideDefs = Z_Malloc(dest->numSideDefs * sizeof(sidedef_t), PU_MAPSTATIC, 0);

    for(i = 0; i < dest->numSideDefs; ++i)
    {
        sidedef_t         *destS = &dest->sideDefs[i];
        sidedef_t         *srcS = src->sideDefs[i];

        memcpy(destS, srcS, sizeof(*destS));
        destS->sector = &dest->sectors[srcS->sector->buildData.index - 1];
        destS->SW_bottomsurface.owner = destS;
        destS->SW_middlesurface.owner = destS;
        destS->SW_topsurface.owner = destS;
        destS->SW_bottomsurface.visOffset[0] = destS->SW_bottomsurface.offset[0];
        destS->SW_bottomsurface.visOffset[1] = destS->SW_bottomsurface.offset[1];
        destS->SW_middlesurface.visOffset[0] = destS->SW_middlesurface.offset[0];
        destS->SW_middlesurface.visOffset[1] = destS->SW_middlesurface.offset[1];
        destS->SW_topsurface.visOffset[0] = destS->SW_topsurface.offset[0];
        destS->SW_topsurface.visOffset[1] = destS->SW_topsurface.offset[1];
    }
}

static void hardenSectors(GameMap* dest, editmap_t* src)
{
    uint                i;

    dest->numSectors = src->numSectors;
    dest->sectors = Z_Malloc(dest->numSectors * sizeof(sector_t), PU_MAPSTATIC, 0);

    for(i = 0; i < dest->numSectors; ++i)
    {
        sector_t           *destS = &dest->sectors[i];
        sector_t           *srcS = src->sectors[i];

        memcpy(destS, srcS, sizeof(*destS));
        destS->planeCount = 0;
        destS->planes = NULL;
    }
}

static void hardenPlanes(GameMap* dest, editmap_t* src)
{
    uint                i, j;

    for(i = 0; i < dest->numSectors; ++i)
    {
        sector_t           *destS = &dest->sectors[i];
        sector_t           *srcS = src->sectors[i];

        for(j = 0; j < srcS->planeCount; ++j)
        {
            plane_t*            destP = R_NewPlaneForSector(destS);
            plane_t*            srcP = srcS->planes[j];

            destP->height = destP->oldHeight[0] = destP->oldHeight[1] =
                destP->visHeight = srcP->height;
            destP->visHeightDelta = 0;
            memcpy(&destP->surface, &srcP->surface, sizeof(destP->surface));
            destP->type = srcP->type;
            destP->sector = destS;
            destP->surface.owner = destP;
        }
    }
}

static void hardenPolyobjs(GameMap* dest, editmap_t* src)
{
    uint i;

    if(src->numPolyObjs == 0)
    {
        dest->numPolyObjs = 0;
        dest->polyObjs = NULL;
        return;
    }

    dest->numPolyObjs = src->numPolyObjs;
    dest->polyObjs = Z_Malloc((dest->numPolyObjs+1) * sizeof(polyobj_t*), PU_MAP, 0);

    for(i = 0; i < dest->numPolyObjs; ++i)
    {
        polyobj_t* destP, *srcP = src->polyObjs[i];
        HEdge* hedges;
        uint j;

        destP = Z_Calloc(POLYOBJ_SIZE, PU_MAP, 0);
        destP->idx = i;
        destP->crush = srcP->crush;
        destP->tag = srcP->tag;
        destP->seqType = srcP->seqType;
        destP->pos[VX] = srcP->pos[VX];
        destP->pos[VY] = srcP->pos[VY];

        destP->lineCount = srcP->lineCount;

        destP->originalPts = Z_Malloc(destP->lineCount * sizeof(fvertex_t), PU_MAP, 0);
        destP->prevPts     = Z_Malloc(destP->lineCount * sizeof(fvertex_t), PU_MAP, 0);

        // Create a hedge for each line of this polyobj.
        hedges = Z_Calloc(sizeof(HEdge) * destP->lineCount, PU_MAP, 0);

        destP->lines = Z_Malloc(sizeof(*destP->lines) * (destP->lineCount+1), PU_MAP, 0);
        for(j = 0; j < destP->lineCount; ++j)
        {
            linedef_t* line = &dest->lineDefs[srcP->lines[j]->buildData.index - 1];
            HEdge* hedge = &hedges[j];
            float dx, dy;

            // This line belongs to a polyobj.
            line->inFlags |= LF_POLYOBJ;

            hedge->header.type = DMU_HEDGE;
            hedge->lineDef = line;
            dx = line->L_v2pos[VX] - line->L_v1pos[VX];
            dy = line->L_v2pos[VY] - line->L_v1pos[VY];
            hedge->length = P_AccurateDistance(dx, dy);
            hedge->twin = NULL;
            hedge->subsector = NULL;
            hedge->HE_frontsector = line->L_frontsector;
            hedge->HE_backsector = NULL;
            hedge->flags |= HEDGEF_POLYOBJ;

            line->L_frontside->hedges = Z_Malloc(sizeof(*line->L_frontside->hedges), PU_MAP, 0);
            line->L_frontside->hedges[0] = hedge;

            destP->lines[j] = line;
        }
        destP->lines[j] = NULL; // Terminate.

        // Add this polyobj to the global list.
        dest->polyObjs[i] = destP;
    }
    dest->polyObjs[i] = NULL; // Terminate.
}

/**
 * \note Algorithm:
 * Cast a line horizontally or vertically and see what we hit (OUCH, we
 * have to iterate over all linedefs!).
 */
static void testForWindowEffect(editmap_t* map, linedef_t* l)
{
// Smallest distance between two points before being considered equal.
#define DIST_EPSILON        (1.0 / 128.0)

    uint                i;
    double              mX, mY, dX, dY;
    boolean             castHoriz;
    double              backDist = DDMAXFLOAT;
    sector_t*           backOpen = NULL;
    double              frontDist = DDMAXFLOAT;
    sector_t*           frontOpen = NULL;
    linedef_t*          frontLine = NULL, *backLine = NULL;

    mX = (l->v[0]->buildData.pos[VX] + l->v[1]->buildData.pos[VX]) / 2.0;
    mY = (l->v[0]->buildData.pos[VY] + l->v[1]->buildData.pos[VY]) / 2.0;

    dX = l->v[1]->buildData.pos[VX] - l->v[0]->buildData.pos[VX];
    dY = l->v[1]->buildData.pos[VY] - l->v[0]->buildData.pos[VY];

    castHoriz = (fabs(dX) < fabs(dY)? true : false);

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t*          n = map->lineDefs[i];
        double              dist;
        boolean             isFront;
        sidedef_t*          hitSide;
        double              dX2, dY2;

        if(n == l || LINE_SELFREF(n) /*|| n->buildData.overlap ||
           (n->buildData.mlFlags & MLF_ZEROLENGTH)*/)
            continue;
        if(n->buildData.mlFlags & MLF_POLYOBJ)
            continue;

        dX2 = n->v[1]->buildData.pos[VX] - n->v[0]->buildData.pos[VX];
        dY2 = n->v[1]->buildData.pos[VY] - n->v[0]->buildData.pos[VY];

        if(castHoriz)
        {   // Horizontal.
            if(fabs(dY2) < DIST_EPSILON)
                continue;

            if((MAX_OF(n->v[0]->buildData.pos[VY], n->v[1]->buildData.pos[VY]) < mY - DIST_EPSILON) ||
               (MIN_OF(n->v[0]->buildData.pos[VY], n->v[1]->buildData.pos[VY]) > mY + DIST_EPSILON))
                continue;

            dist = (n->v[0]->buildData.pos[VX] +
                (mY - n->v[0]->buildData.pos[VY]) * dX2 / dY2) - mX;

            isFront = (((dY > 0) != (dist > 0)) ? true : false);

            dist = fabs(dist);
            if(dist < DIST_EPSILON)
                continue; // Too close (overlapping lines ?)

            hitSide = n->sideDefs[(dY > 0) ^ (dY2 > 0) ^ !isFront];
        }
        else
        {   // Vertical.
            if(fabs(dX2) < DIST_EPSILON)
                continue;

            if((MAX_OF(n->v[0]->buildData.pos[VX], n->v[1]->buildData.pos[VX]) < mX - DIST_EPSILON) ||
               (MIN_OF(n->v[0]->buildData.pos[VX], n->v[1]->buildData.pos[VX]) > mX + DIST_EPSILON))
                continue;

            dist = (n->v[0]->buildData.pos[VY] +
                (mX - n->v[0]->buildData.pos[VX]) * dY2 / dX2) - mY;

            isFront = (((dX > 0) == (dist > 0)) ? true : false);

            dist = fabs(dist);

            hitSide = n->sideDefs[(dX > 0) ^ (dX2 > 0) ^ !isFront];
        }

        if(dist < DIST_EPSILON) // Too close (overlapping lines ?)
            continue;

        if(isFront)
        {
            if(dist < frontDist)
            {
                frontDist = dist;
                if(hitSide)
                    frontOpen = hitSide->sector;
                else
                    frontOpen = NULL;

                frontLine = n;
            }
        }
        else
        {
            if(dist < backDist)
            {
                backDist = dist;
                if(hitSide)
                    backOpen = hitSide->sector;
                else
                    backOpen = NULL;

                backLine = n;
            }
        }
    }

/*#if _DEBUG
Con_Message("back line: %d  back dist: %1.1f  back_open: %s\n",
            (backLine? backLine->buildData.index : -1), backDist,
            (backOpen? "OPEN" : "CLOSED"));
Con_Message("front line: %d  front dist: %1.1f  front_open: %s\n",
            (frontLine? frontLine->buildData.index : -1), frontDist,
            (frontOpen? "OPEN" : "CLOSED"));
#endif*/

    if(backOpen && frontOpen && l->sideDefs[FRONT]->sector == backOpen)
    {
        VERBOSE( Con_Message("Linedef #%d seems to be a One-Sided Window "
                             "(back faces sector #%d).\n", l->buildData.index - 1,
                             backOpen->buildData.index - 1) );

        l->buildData.windowEffect = frontOpen;
    }

#undef DIST_EPSILON
}

static void countVertexLineOwners(vertex_t* vtx, uint* oneSided,
                                  uint* twoSided)
{
    lineowner_t*        p;

    p = vtx->lineOwners;
    while(p)
    {
        if(!p->lineDef->L_frontside || !p->lineDef->L_backside)
            (*oneSided)++;
        else
            (*twoSided)++;

        p = p->LO_next;
    }
}

/**
 * \note Algorithm:
 * Scan the linedef list looking for possible candidates, checking for an
 * odd number of one-sided linedefs connected to a single vertex.
 * This idea courtesy of Graham Jackson.
 */
void MPE_DetectWindowEffects(editmap_t* map)
{
    uint                i, oneSiders, twoSiders;

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t*          l = map->lineDefs[i];

        if((l->L_frontside && l->L_backside) || !l->L_frontside /*||
           (l->buildData.mlFlags & MLF_ZEROLENGTH) ||
           l->buildData.overlap*/)
            continue;
        if(l->buildData.mlFlags & MLF_POLYOBJ)
            continue;

        oneSiders = twoSiders = 0;
        countVertexLineOwners(l->v[0], &oneSiders, &twoSiders);

        if((oneSiders % 2) == 1 && (oneSiders + twoSiders) > 1)
        {
/*#if _DEBUG
Con_Message("FUNNY LINE %d : start vertex %d has odd number of one-siders\n",
            i, l->buildData.v[0]->index);
#endif*/
            testForWindowEffect(map, l);
            continue;
        }

        oneSiders = twoSiders = 0;
        countVertexLineOwners(l->v[1], &oneSiders, &twoSiders);

        if((oneSiders % 2) == 1 && (oneSiders + twoSiders) > 1)
        {
/*#if _DEBUG
Con_Message("FUNNY LINE %d : end vertex %d has odd number of one-siders\n",
            i, l->buildData.v[1]->index));
#endif*/
            testForWindowEffect(map, l);
        }
    }
}

#if 0 /* Currently unused. */
/**
 * @return              The "lowest" vertex (normally the left-most, but if
 *                      the line is vertical, then the bottom-most).
 *                      @c => 0 for start, 1 for end.
 */
static __inline int lineVertexLowest(const linedef_t* l)
{
    return (((int) l->v[0]->buildData.pos[VX] < (int) l->v[1]->buildData.pos[VX] ||
             ((int) l->v[0]->buildData.pos[VX] == (int) l->v[1]->buildData.pos[VX] &&
              (int) l->v[0]->buildData.pos[VY] < (int) l->v[1]->buildData.pos[VY]))? 0 : 1);
}

static int C_DECL lineStartCompare(const void* p1, const void* p2)
{
    const linedef_t*    a = (const linedef_t*) p1;
    const linedef_t*    b = (const linedef_t*) p2;
    vertex_t*           c, *d;

    // Determine left-most vertex of each line.
    c = (lineVertexLowest(a)? a->v[1] : a->v[0]);
    d = (lineVertexLowest(b)? b->v[1] : b->v[0]);

    if((int) c->buildData.pos[VX] != (int) d->buildData.pos[VX])
        return (int) c->buildData.pos[VX] - (int) d->buildData.pos[VX];

    return (int) c->buildData.pos[VY] - (int) d->buildData.pos[VY];
}

static int C_DECL lineEndCompare(const void* p1, const void* p2)
{
    const linedef_t*    a = (const linedef_t*) p1;
    const linedef_t*    b = (const linedef_t*) p2;
    vertex_t*           c, *d;

    // Determine right-most vertex of each line.
    c = (lineVertexLowest(a)? a->v[0] : a->v[1]);
    d = (lineVertexLowest(b)? b->v[0] : b->v[1]);

    if((int) c->buildData.pos[VX] != (int) d->buildData.pos[VX])
        return (int) c->buildData.pos[VX] - (int) d->buildData.pos[VX];

    return (int) c->buildData.pos[VY] - (int) d->buildData.pos[VY];
}

size_t numOverlaps;

int testOverlaps(linedef_t* b, void* data)
{
    linedef_t*          a = (linedef_t*) data;

    if(a != b)
    {
        if(lineStartCompare(a, b) == 0)
            if(lineEndCompare(a, b) == 0)
            {   // Found an overlap!
                b->buildData.overlap =
                    (a->buildData.overlap ? a->buildData.overlap : a);
                numOverlaps++;
            }
    }

    return false; // Continue iteration.
}

typedef struct {
    GameMap* map;
    uint coords[2]; // Blockmap cell coordinates.
} findoverlaps_params_t;

int findOverlapsForLinedef(linedef_t* l, void* data)
{
    findoverlaps_params_t* p = (findoverlaps_params_t*) data;
    GameMap_IterateCellLineDefs(p->map, p->coords, testOverlaps, l);
    return false; // Continue iteration.
}

/**
 * \note Does not detect partially overlapping lines!
 */
void MPE_DetectOverlappingLines(GameMap* map)
{
    uint x, y, bmapDimensions[2];
    findoverlaps_params_t params;

    Blockmap_Size(map->lineDefBlockmap, bmapDimensions);

    numOverlaps = 0;

    for(y = 0; y < bmapSize[VY]; ++y)
        for(x = 0; x < bmapSize[VX]; ++x)
        {
            params.map = map;
            params.coords[VX] = x;
            params.coords[VY] = y;

            GameMap_IterateCellLineDefs(map, params.coords, findOverlapsForLinedef, &params);
        }

    if(numOverlaps == 0) return;

    VERBOSE( Con_Message("Detected %lu overlapped linedefs\n", (unsigned long) numOverlaps) )
}
#endif

/**
 * Find the extremal coordinates for the given set of vertexes.
 *
 * @param vertexes  Address of the list of vertexes to be scanned.
 * @param numVertexes  Number of vertex elements in @a vertexes.
 * @param min  Minimal coordinates will be written here.
 * @param max  Maximal coordinates will be written here.
 */
static void findBounds(vertex_t const** vertexes, uint numVertexes, vec2_t min, vec2_t max)
{
    vec2_t bounds[2], point;
    const vertex_t* vtx;
    uint i;

    if(!min && !max) return;

    if(!vertexes || !numVertexes)
    {
        V2_Set(min, DDMAXFLOAT, DDMAXFLOAT);
        V2_Set(max, DDMINFLOAT, DDMINFLOAT);
        return;
    }

    for(i = 0; i < numVertexes; ++i)
    {
        vtx = vertexes[i];
        V2_Set(point, vtx->V_pos[VX], vtx->V_pos[VY]);
        if(!i)
            V2_InitBox(bounds, point);
        else
            V2_AddToBox(bounds, point);
    }

    if(min)
    {
        V2_Set(min, bounds[0][VX], bounds[0][VY]);
    }
    if(max)
    {
        V2_Set(max, bounds[1][VX], bounds[1][VY]);
    }
}

/**
 * Called to complete the map building process.
 */
boolean MPE_End(void)
{
    GameMap* gamemap;
    boolean builtOK;
    vec2_t min, max;
    uint i;

    if(!editMapInited)
        return false;

    gamemap = Z_Calloc(sizeof(*gamemap), PU_MAPSTATIC, 0);

    // Pass on the game map obj database. The game will want to query it
    // once we have finished constructing the map.
    memcpy(&gamemap->gameObjData, &map->gameObjData, sizeof gamemap->gameObjData);

    /**
     * Perform cleanup on the loaded map data, removing duplicate vertexes,
     * pruning unused sectors etc, etc...
     */
    MPE_DetectDuplicateVertices(map);
    MPE_PruneRedundantMapData(map, PRUNE_ALL);

    buildVertexOwnerRings(map);

    MPE_DetectWindowEffects(map);

    /**
     * Harden most of the map data so that we can construct some of the more
     * intricate data structures early on (and thus make use of them during
     * the BSP generation).
     *
     * \todo I'm sure this can be reworked further so that we destroy as we
     * go and reduce the current working memory surcharge.
     */
    hardenSectors(gamemap, map);
    hardenSidedefs(gamemap, map);
    hardenLinedefs(gamemap, map);
    hardenPolyobjs(gamemap, map);

    hardenVertexOwnerRings(gamemap, map);

    // Don't destroy the sectors (planes are linked to them).
    destroyEditableSideDefs(map);
    destroyEditableLineDefs(map);
    destroyEditablePolyObjs(map);

    /**
     * Build blockmaps.
     */
    findBounds((vertex_t const**)map->vertexes, map->numVertexes, min, max);

    GameMap_InitLineDefBlockmap(gamemap, min, max);
    for(i = 0; i < gamemap->numLineDefs; ++i)
    {
        GameMap_LinkLineDefInBlockmap(gamemap, gamemap->lineDefs + i);
    }

    // Mobj and Polyobj blockmaps are maintained dynamically.
    GameMap_InitMobjBlockmap(gamemap, min, max);
    GameMap_InitPolyobjBlockmap(gamemap, min, max);

    /**
     * Build a BSP for this map.
     */
    builtOK = BSP_Build(gamemap, &map->vertexes, &map->numVertexes);

    // Finish the polyobjs (after the vertexes are hardened).
    for(i = 0; i < gamemap->numPolyObjs; ++i)
    {
        polyobj_t* po = gamemap->polyObjs[i];
        linedef_t** lineIter;
        uint n = 0;

        for(lineIter = po->lines; *lineIter; lineIter++, n++)
        {
            linedef_t* line = *lineIter;
            HEdge* hedge = line->L_frontside->hedges[0];

            hedge->HE_v1 = line->L_v1;
            hedge->HE_v2 = line->L_v2;

            // The original Pts are based off the anchor Pt, and are unique
            // to each hedge, not each linedef.
            po->originalPts[n].pos[VX] = line->L_v1pos[VX] - po->pos[VX];
            po->originalPts[n].pos[VY] = line->L_v1pos[VY] - po->pos[VY];
        }
    }

    // Polygonize.
    R_PolygonizeMap(gamemap);

    buildSectorSSecLists(gamemap);

    // Announce any issues detected with the map.
    MPE_PrintMapErrors();

    // Map must be polygonized and sector->subsectors must be built before
    // this is called!
    hardenPlanes(gamemap, map);

    // Destroy the rest of editable map, we are finished with it.
    // \note Only the vertexes should be left anyway.
    destroyMap();

    if(!builtOK)
    {
        // Failed. Need to clean up.
        P_DestroyGameMapObjDB(&gamemap->gameObjData);
        Z_Free(gamemap);
        return false;
    }

    buildSectorLineLists(gamemap);
    finishLineDefs(gamemap);
    finishSectors(gamemap);
    updateMapBounds(gamemap);
    S_DetermineSubsecsAffectingSectorReverb(gamemap);
    prepareSubsectors(gamemap);

    P_FreeBadTexList();
    MPE_FreeUnclosedSectorList();

    editMapInited = false;

    // Call the game's setup routines.
    if(gx.SetupForMapData)
    {
        gx.SetupForMapData(DMU_VERTEX, gamemap->numVertexes);
        gx.SetupForMapData(DMU_LINEDEF, gamemap->numLineDefs);
        gx.SetupForMapData(DMU_SIDEDEF, gamemap->numSideDefs);
        gx.SetupForMapData(DMU_SECTOR, gamemap->numSectors);
    }

    /**
     * Are we caching this map?
     */
    if(gamemap->uri && !Str_IsEmpty(Uri_Path(gamemap->uri)))
    {
        // Yes, write the cached map data file.
        lumpnum_t markerLumpNum = F_CheckLumpNumForName2(Str_Text(Uri_Path(gamemap->uri)), true);
        ddstring_t* cachedMapDir = DAM_ComposeCacheDir(F_LumpSourceFile(markerLumpNum));
        ddstring_t cachedMapPath;

        Str_Init(&cachedMapPath);
        F_FileName(&cachedMapPath, F_LumpName(markerLumpNum));
        Str_Append(&cachedMapPath, ".dcm");
        Str_Prepend(&cachedMapPath, Str_Text(cachedMapDir));
        F_ExpandBasePath(&cachedMapPath, &cachedMapPath);

        // Ensure the destination directory exists.
        F_MakePath(Str_Text(cachedMapDir));

        // Archive this map!
        DAM_MapWrite(gamemap, Str_Text(&cachedMapPath));

        Str_Delete(cachedMapDir);
        Str_Free(&cachedMapPath);
    }

    lastBuiltMap = gamemap;

    // Success!
    return true;
}

GameMap* MPE_GetLastBuiltMap(void)
{
    return lastBuiltMap;
}

/**
 * If we encountered any problems during setup - announce them to the user.
 */
void MPE_PrintMapErrors(void)
{
    // Announce unclosed sectors.
    MPE_PrintUnclosedSectorList();

    // Announce any bad texture names we came across when loading the map.
    P_PrintMissingTextureList();
}

/**
 * Create a new vertex in currently loaded editable map.
 *
 * @param x             X coordinate of the new vertex.
 * @param y             Y coordinate of the new vertex.
 *
 * @return              Index number of the newly created vertex else 0 if
 *                      the vertex could not be created for some reason.
 */
uint MPE_VertexCreate(float x, float y)
{
    vertex_t*           v;

    if(!editMapInited)
        return 0;

    v = createVertex();
    v->V_pos[VX] = x;
    v->V_pos[VY] = y;
    v->buildData.pos[VX] = (double) v->V_pos[VX];
    v->buildData.pos[VY] = (double) v->V_pos[VY];

    return v->buildData.index;
}

/**
 * Create many new vertexs in the currently loaded editable map.
 *
 * @param num           Number of vertexes to be created.
 * @param values        Ptr to an array containing the coordinates for the
 *                      vertexs to create [v0 X, vo Y, v1 X, v1 Y...]
 * @param indices       If not @c NULL, the indices of the newly created
 *                      vertexes will be written back here.
 */
boolean MPE_VertexCreatev(size_t num, float* values, uint* indices)
{
    uint                n;

    if(!editMapInited || !num || !values)
        return false;

    // Create many vertexes.
    for(n = 0; n < num; ++n)
    {
        vertex_t*           v;

        v = createVertex();
        v->V_pos[VX] = values[n * 2];
        v->V_pos[VY] = values[n * 2 + 1];
        v->buildData.pos[VX] = (double) v->V_pos[VX];
        v->buildData.pos[VY] = (double) v->V_pos[VY];

        if(indices)
            indices[n] = v->buildData.index;
    }

    return true;
}

uint MPE_SidedefCreate(uint sector, short flags,
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
                       float bottomBlue)
{
    sidedef_t*          s;

    if(!editMapInited)
        return 0;

    if(sector > map->numSectors)
        return 0;

    s = createSide();
    s->flags = flags;
    s->sector = (sector == 0? NULL: map->sectors[sector-1]);

    Surface_SetMaterial(&s->SW_topsurface, Materials_ToMaterial(topMaterial));
    Surface_SetMaterialOrigin(&s->SW_topsurface, topOffsetX, topOffsetY);
    Surface_SetColorAndAlpha(&s->SW_topsurface, topRed, topGreen, topBlue, 1);

    Surface_SetMaterial(&s->SW_middlesurface, Materials_ToMaterial(middleMaterial));
    Surface_SetMaterialOrigin(&s->SW_middlesurface, middleOffsetX, middleOffsetY);
    Surface_SetColorAndAlpha(&s->SW_middlesurface, middleRed, middleGreen, middleBlue, middleAlpha);

    Surface_SetMaterial(&s->SW_bottomsurface, Materials_ToMaterial(bottomMaterial));
    Surface_SetMaterialOrigin(&s->SW_bottomsurface, bottomOffsetX, bottomOffsetY);
    Surface_SetColorAndAlpha(&s->SW_bottomsurface, bottomRed, bottomGreen, bottomBlue, 1);

    return s->buildData.index;
}

/**
 * Create a new linedef in the editable map.
 *
 * @param v1            Idx of the start vertex.
 * @param v2            Idx of the end vertex.
 * @param frontSide     Idx of the front sidedef.
 * @param backSide      Idx of the back sidedef.
 * @param flags         DDLF_* flags.
 *
 * @return              Idx of the newly created linedef else @c 0 if there
 *                      was an error.
 */
uint MPE_LinedefCreate(uint v1, uint v2, uint frontSide, uint backSide,
                       int flags)
{
    linedef_t*          l;
    sidedef_t*          front = NULL, *back = NULL;
    vertex_t*           vtx1, *vtx2;
    float               length, dx, dy;

    if(!editMapInited)
        return 0;

    if(frontSide > map->numSideDefs)
        return 0;
    if(backSide > map->numSideDefs)
        return 0;
    if(v1 == 0 || v1 > map->numVertexes)
        return 0;
    if(v2 == 0 || v2 > map->numVertexes)
        return 0;
    if(v1 == v2)
        return 0;

    // First, ensure that the side indices are unique.
    if(frontSide && map->sideDefs[frontSide - 1]->buildData.refCount)
        return 0; // Already in use.
    if(backSide && map->sideDefs[backSide - 1]->buildData.refCount)
        return 0; // Already in use.

    // Next, check the length is not zero.
    vtx1 = map->vertexes[v1 - 1];
    vtx2 = map->vertexes[v2 - 1];
    dx = vtx2->V_pos[VX] - vtx1->V_pos[VX];
    dy = vtx2->V_pos[VY] - vtx1->V_pos[VY];
    length = P_AccurateDistance(dx, dy);
    if(!(length > 0))
        return 0;

    if(frontSide > 0)
        front = map->sideDefs[frontSide - 1];
    if(backSide > 0)
        back = map->sideDefs[backSide - 1];

    l = createLine();
    l->L_v1 = vtx1;
    l->L_v2 = vtx2;

    l->L_v1->buildData.refCount++;
    l->L_v2->buildData.refCount++;

    l->dX = dx;
    l->dY = dy;
    l->length = length;

    l->angle =
        bamsAtan2((int) (l->L_v2pos[VY] - l->L_v1pos[VY]),
                  (int) (l->L_v2pos[VX] - l->L_v1pos[VX]));

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
        l->aaBox.minX = l->L_v1pos[VX];
        l->aaBox.maxX = l->L_v2pos[VX];
    }
    else
    {
        l->aaBox.minX = l->L_v2pos[VX];
        l->aaBox.maxX = l->L_v1pos[VX];
    }

    if(l->L_v1pos[VY] < l->L_v2pos[VY])
    {
        l->aaBox.minY = l->L_v1pos[VY];
        l->aaBox.maxY = l->L_v2pos[VY];
    }
    else
    {
        l->aaBox.minY = l->L_v2pos[VY];
        l->aaBox.maxY = l->L_v1pos[VY];
    }

    l->L_frontside = front;
    l->L_backside = back;

    // Remember the number of unique references.
    if(l->L_frontside)
    {
        l->L_frontside->line = l;
        l->L_frontside->buildData.refCount++;
    }

    if(l->L_backside)
    {
        l->L_backside->line = l;
        l->L_backside->buildData.refCount++;
    }

    l->inFlags = 0;

    // Determine the default linedef flags.
    l->flags = flags;
    if(!front || !back)
        l->flags |= DDLF_BLOCKING;

    return l->buildData.index;
}

uint MPE_PlaneCreate(uint sector, float height, materialid_t material,
                     float matOffsetX, float matOffsetY,
                     float r, float g, float b, float a,
                     float normalX, float normalY, float normalZ)
{
    uint                i;
    sector_t*           s;
    plane_t**           newList, *pln;

    if(!editMapInited)
        return 0;

    if(sector == 0 || sector > map->numSectors)
        return 0;

    s = map->sectors[sector - 1];

    pln = M_Calloc(sizeof(plane_t));
    pln->surface.owner = (void*) pln;
    pln->height = height;
    Surface_SetMaterial(&pln->surface, Materials_ToMaterial(material));
    Surface_SetColorAndAlpha(&pln->surface, r, g, b, a);
    Surface_SetMaterialOrigin(&pln->surface, matOffsetX, matOffsetY);
    V3_Set(pln->PS_normal, normalX, normalY, normalZ);
    V3_Normalize(pln->PS_normal);
    V3_BuildTangents(pln->PS_tangent, pln->PS_bitangent, pln->PS_normal);

    pln->type = (pln->PS_normal[VZ] < 0? PLN_CEILING : PLN_FLOOR);

    pln->sector = s;
    newList = M_Malloc(sizeof(plane_t*) * (++s->planeCount + 1));
    for(i = 0; i < s->planeCount - 1; ++i)
    {
        newList[i] = s->planes[i];
    }
    newList[i++] = pln;
    newList[i] = NULL; // Terminate.

    if(s->planes)
        M_Free(s->planes);
    s->planes = newList;

    return s->planeCount; // 1-based index.
}

uint MPE_SectorCreate(float lightlevel, float red, float green, float blue)
{
    sector_t           *s;

    if(!editMapInited)
        return 0;

    s = createSector();

    s->rgb[CR] = MINMAX_OF(0, red, 1);
    s->rgb[CG] = MINMAX_OF(0, green, 1);
    s->rgb[CB] = MINMAX_OF(0, blue, 1);
    s->lightLevel = MINMAX_OF(0, lightlevel, 1);
    s->planeCount = 0;
    s->planes = NULL;

    return s->buildData.index;
}

uint MPE_PolyobjCreate(uint* lines, uint lineCount, int tag, int sequenceType,
    float anchorX, float anchorY)
{
    polyobj_t* po;
    uint i;

    if(!editMapInited || !lineCount || !lines) return 0;

    // First check that all the line indices are valid and that they arn't
    // already part of another polyobj.
    for(i = 0; i < lineCount; ++i)
    {
        linedef_t* line;

        if(lines[i] == 0 || lines[i] > map->numLineDefs) return 0;

        line = map->lineDefs[lines[i] - 1];
        if(line->inFlags & LF_POLYOBJ) return 0;
    }

    po = createPolyobj();
    po->tag = tag;
    po->seqType = sequenceType;
    po->pos[VX] = anchorX;
    po->pos[VY] = anchorY;

    po->lineCount = lineCount;
    po->lines = M_Calloc(sizeof(linedef_t*) * (po->lineCount+1));
    for(i = 0; i < lineCount; ++i)
    {
        linedef_t* line = map->lineDefs[lines[i] - 1];

        // This line belongs to a polyobj.
        line->inFlags |= LF_POLYOBJ;
        po->lines[i] = line;
    }
    po->lines[i] = NULL;

    return po->buildData.index;
}

boolean MPE_GameObjProperty(const char *objName, uint idx,
                            const char *propName, valuetype_t type,
                            void *data)
{
    uint                i;
    size_t              len;
    gamemapobjdef_t    *def;

    if(!objName || !propName || !data)
        return false; // Hmm...

    // Is this a known object?
    if((def = P_GetGameMapObjDef(0, objName, false)) == NULL)
        return false; // Nope.

    // Is this a known property?
    len = strlen(propName);
    for(i = 0; i < def->numProps; ++i)
    {
        if(!strnicmp(propName, def->props[i].name, len))
        {   // Found a match!
            // Create a record of this so that the game can query it later.
            P_AddGameMapObjValue(&map->gameObjData, def, i, idx, type, data);
            return true; // We're done.
        }
    }

    // An unknown property.
    VERBOSE(Con_Message("MPE_GameObjProperty: %s has no property \"%s\".\n",
                        def->name, propName));

    return false;
}
