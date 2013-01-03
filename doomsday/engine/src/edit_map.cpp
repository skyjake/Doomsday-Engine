/**
 * @file edit_map.cpp

 * Map Editor interface. @ingroup map
 *
 * @author Copyright &copy; 2007-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @author Copyright &copy; 1998-2000 Colin Reed <cph@moria.org.uk>
 * @author Copyright &copy; 1998-2000 Lee Killough <killough@rsn.hp.com>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#define DENG_NO_API_MACROS_MAP_EDIT

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_bsp.h"
#include "de_misc.h"
#include "de_edit.h"
#include "de_dam.h"
#include "de_filesys.h"

#include <de/StringPool>

#include "audio/s_environ.h"

using namespace de;

typedef struct {
    ddstring_t uri;
    uint count; ///< Number of times this has been found missing.
} missingmaterialrecord_t;

static void printMissingMaterials(void);
static void clearMaterialDict(void);

static void assignSurfaceMaterial(Surface* suf, const ddstring_t* materialUri);

/**
 * Material name references specified during map conversion are recorded in this
 * dictionary. A dictionary is used to avoid repeatedly resolving the same URIs
 * and to facilitate a log of missing materials encountered during the process.
 *
 * The pointer user value holds a pointer to the resolved Material (if found).
 * The integer user value tracks the number of times a reference occurs.
 */
static StringPool* materialDict;

editmap_t editMap;
static boolean editMapInited = false;

static editmap_t* map = &editMap;
static boolean lastBuiltMapResult;
static GameMap* lastBuiltMap;

static Vertex* rootVtx; // Used when sorting vertex line owners.

static Vertex* createVertex(void)
{
    Vertex* vtx = (Vertex*) M_Calloc(sizeof(*vtx));
    vtx->header.type = DMU_VERTEX;

    map->vertexes = (Vertex**) M_Realloc(map->vertexes, sizeof(vtx) * (++map->numVertexes + 1));
    map->vertexes[map->numVertexes-1] = vtx;
    map->vertexes[map->numVertexes] = NULL;

    vtx->buildData.index = map->numVertexes; // 1-based index, 0 = NIL.
    return vtx;
}

static LineDef* createLine(void)
{
    LineDef* line = (LineDef*) M_Calloc(sizeof(*line));
    line->header.type = DMU_LINEDEF;

    map->lineDefs = (LineDef**) M_Realloc(map->lineDefs, sizeof(line) * (++map->numLineDefs + 1));
    map->lineDefs[map->numLineDefs-1] = line;
    map->lineDefs[map->numLineDefs] = NULL;

    line->origIndex = map->numLineDefs; // 1-based index, 0 = NIL.
    return line;
}

static SideDef* createSide(void)
{
    SideDef* side = (SideDef*) M_Calloc(sizeof(*side));
    side->header.type = DMU_SIDEDEF;

    map->sideDefs = (SideDef**) M_Realloc(map->sideDefs, sizeof(side) * (++map->numSideDefs + 1));
    map->sideDefs[map->numSideDefs-1] = side;
    map->sideDefs[map->numSideDefs] = NULL;

    side->buildData.index = map->numSideDefs; // 1-based index, 0 = NIL.
    side->SW_bottomsurface.owner = (void*) side;
    side->SW_middlesurface.owner = (void*) side;
    side->SW_topsurface.owner = (void*) side;
    return side;
}

static Sector* createSector(void)
{
    Sector* sec = (Sector*) M_Calloc(sizeof(*sec));
    sec->header.type = DMU_SECTOR;

    map->sectors = (Sector**) M_Realloc(map->sectors, sizeof(sec) * (++map->numSectors + 1));
    map->sectors[map->numSectors-1] = sec;
    map->sectors[map->numSectors] = NULL;

    sec->buildData.index = map->numSectors; // 1-based index, 0 = NIL.
    return sec;
}

static Polyobj* createPolyobj(void)
{
    Polyobj* po = (Polyobj*) M_Calloc(sizeof(*po));

    map->polyObjs = (Polyobj**) M_Realloc(map->polyObjs, sizeof(po) * (++map->numPolyObjs + 1));
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
            Polyobj* po = map->polyObjs[i];
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
            LineDef* line = map->lineDefs[i];
            M_Free(line);
        }

        M_Free(map->lineDefs);
    }
    map->lineDefs = NULL;
    map->numLineDefs = 0;
}

static void destroyEditableSideDefs(editmap_t* map)
{
    if(map->sideDefs)
    {
        uint i;
        for(i = 0; i < map->numSideDefs; ++i)
        {
            SideDef* side = map->sideDefs[i];
            M_Free(side);
        }

        M_Free(map->sideDefs);
    }
    map->sideDefs = NULL;
    map->numSideDefs = 0;
}

static void destroyEditableSectors(editmap_t* map)
{
    if(map->sectors)
    {
        uint i;
        for(i = 0; i < map->numSectors; ++i)
        {
            uint j;
            Sector* s = map->sectors[i];

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

static void destroyEditableVertexes(editmap_t* map)
{
    if(map->vertexes)
    {
        uint i;
        for(i = 0; i < map->numVertexes; ++i)
        {
            Vertex* vtx = map->vertexes[i];
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

static int vertexCompare(void const* p1, void const* p2)
{
    Vertex const* a = (Vertex const*) *((const void**) p1);
    Vertex const* b = (Vertex const*) *((const void**) p2);

    if(a == b) return 0;

    if(int(a->origin[VX]) != int(b->origin[VX]))
        return int(a->origin[VX]) - int(b->origin[VX]);

    return int(a->origin[VY]) - int(b->origin[VY]);
}

void MPE_DetectDuplicateVertices(editmap_t* map)
{
    DENG_ASSERT(map);
    Vertex** hits = (Vertex**) M_Malloc(map->numVertexes * sizeof(Vertex*));

    // Sort array of ptrs.
    for(uint i = 0; i < map->numVertexes; ++i)
    {
        hits[i] = map->vertexes[i];
    }
    qsort(hits, map->numVertexes, sizeof(Vertex*), vertexCompare);

    // Now mark them off.
    for(uint i = 0; i < map->numVertexes - 1; ++i)
    {
        // A duplicate?
        if(vertexCompare(hits + i, hits + i + 1) == 0)
        {
            // Yes.
            Vertex* a = hits[i];
            Vertex* b = hits[i + 1];

            b->buildData.equiv = (a->buildData.equiv ? a->buildData.equiv : a);
        }
    }

    M_Free(hits);
}

#if 0
static void findEquivalentVertexes(editmap_t* src)
{
    uint i, newNum;

    // Scan all linedefs.
    for(i = 0, newNum = 0; i < src->numLineDefs; ++i)
    {
        LineDef* l = src->lineDefs[i];

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
    uint i, newNum, unused = 0;

    for(i = 0, newNum = 0; i < map->numLineDefs; ++i)
    {
        LineDef* l = map->lineDefs[i];

        if(!l->L_frontsidedef && !l->L_backsidedef)
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
    uint i, newNum, unused = 0;

    // Scan all vertices.
    for(i = 0, newNum = 0; i < map->numVertexes; ++i)
    {
        Vertex* v = map->vertexes[i];

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
        int dupNum = map->numVertexes - newNum - unused;

        if(unused > 0)
            Con_Message("  Pruned %d unused vertices.\n", unused);

        if(dupNum > 0)
            Con_Message("  Pruned %d duplicate vertices\n", dupNum);

        map->numVertexes = newNum;
    }
}

static void pruneUnusedSidedefs(editmap_t* map)
{
    uint i, newNum, unused = 0;

    for(i = 0, newNum = 0; i < map->numSideDefs; ++i)
    {
        SideDef* s = map->sideDefs[i];

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
        int dupNum = map->numSideDefs - newNum - unused;

        if(unused > 0)
            Con_Message("  Pruned %d unused sidedefs\n", unused);

        if(dupNum > 0)
            Con_Message("  Pruned %d duplicate sidedefs\n", dupNum);

        map->numSideDefs = newNum;
    }
}

static void pruneUnusedSectors(editmap_t* map)
{
    uint i, newNum;

    for(i = 0; i < map->numSideDefs; ++i)
    {
        SideDef* s = map->sideDefs[i];

        if(s->sector)
            s->sector->buildData.refCount++;
    }

    // Scan all sectors.
    for(i = 0, newNum = 0; i < map->numSectors; ++i)
    {
        Sector* s = map->sectors[i];

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
 * @warning Order here is critical!
 */
void MPE_PruneRedundantMapData(editmap_t* /*map*/, int /*flags*/)
{
#if 0
    /**
     * @todo Pruning cannot be done as game map data object properties
     * are currently indexed by their original indices as determined by the
     * position in the map data. The same problem occurs within ACS scripts
     * and XG line/sector references.
     */
    if(!editMapInited) return;

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

boolean MPE_Begin(const char* mapUri)
{
    /// @todo Do not ignore; assign to the editable map.
    DENG_UNUSED(mapUri);

    if(editMapInited) return true; // Already been here.

    // Initialize the game-specific map entity property database.
    map->entityDatabase = EntityDatabase_New();

    destroyMap();

    lastBuiltMap = 0;
    lastBuiltMapResult = false; // Assume failure.

    editMapInited = true;
    return true;
}

static void hardenSectorBspLeafList(GameMap* map, uint secIDX)
{
    Sector* sec = &map->sectors[secIDX];
    uint i, n, count;
    assert(secIDX < map->numSectors);

    count = 0;
    for(i = 0; i < map->numBspLeafs; ++i)
    {
        BspLeaf *bspLeaf = map->bspLeafs[i];
        if(bspLeaf->sector == sec)
            ++count;
    }

    if(0 == count) return;

    sec->bspLeafs = (BspLeaf**) Z_Malloc((count + 1) * sizeof(BspLeaf*), PU_MAPSTATIC, NULL);

    n = 0;
    for(i = 0; i < map->numBspLeafs; ++i)
    {
        BspLeaf* bspLeaf = map->bspLeafs[i];
        if(bspLeaf->sector == sec)
        {
            sec->bspLeafs[n++] = bspLeaf;
        }
    }
    sec->bspLeafs[n] = NULL; // Terminate.
    sec->bspLeafCount = count;
}

/**
 * Build BspLeaf tables for all sectors.
 */
static void buildSectorBspLeafLists(GameMap* map)
{
    uint i;
    VERBOSE( Con_Message(" Build BSP leaf tables...\n") )

    for(i = 0; i < map->numSectors; ++i)
    {
        hardenSectorBspLeafList(map, i);
    }
}

static void buildSectorLineLists(GameMap* map)
{
    typedef struct linelink_s {
        LineDef* line;
        struct linelink_s* next;
    } linelink_t;

    LOG_VERBOSE("Building Sector LineDef lists...");

    // build line tables for each sector.
    zblockset_t* lineLinksBlockSet = ZBlockSet_New(sizeof(linelink_t), 512, PU_APPSTATIC);
    linelink_t** sectorLineLinks = (linelink_t**) M_Calloc(sizeof(linelink_t*) * map->numSectors);
    uint totallinks = 0;
    LineDef* li = map->lineDefs;
    for(uint i = 0; i < map->numLineDefs; ++i, li++)
    {
        uint secIDX;
        linelink_t* link;

        if(li->L_frontsector)
        {
            link = (linelink_t*) ZBlockSet_Allocate(lineLinksBlockSet);

            secIDX = li->L_frontsector - map->sectors;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            li->L_frontsector->lineDefCount++;
            totallinks++;
        }

        if(li->L_backsector && !LINE_SELFREF(li))
        {
            link = (linelink_t*) ZBlockSet_Allocate(lineLinksBlockSet);

            secIDX = li->L_backsector - map->sectors;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            li->L_backsector->lineDefCount++;
            totallinks++;
        }
    }

    // Harden the sector line links into arrays.
    LineDef** linebuffer = (LineDef**) Z_Malloc((totallinks + map->numSectors) * sizeof(LineDef*), PU_MAPSTATIC, 0);
    LineDef** linebptr = linebuffer;

    Sector* sec = map->sectors;
    for(uint i = 0; i < map->numSectors; ++i, sec++)
    {
        if(sectorLineLinks[i])
        {
            linelink_t* link = sectorLineLinks[i];

            /**
             * The behaviour of some algorithms used in original DOOM are
             * dependant upon the order of these lists (e.g., EV_DoFloor
             * and EV_BuildStairs). Lets be helpful and use the same order.
             *
             * Sort: LineDef index ascending (zero based).
             */
            uint numLineDefs = 0;
            while(link)
            {
                numLineDefs++;
                link = link->next;
            }

            sec->lineDefs = linebptr;
            uint j = numLineDefs - 1;
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

    // Free temporary storage.
    ZBlockSet_Delete(lineLinksBlockSet);
    M_Free(sectorLineLinks);
}

static void finishSectors(GameMap* map)
{
    DENG_ASSERT(map);

    for(uint i = 0; i < map->numSectors; ++i)
    {
        Sector* sec = &map->sectors[i];

        Sector_UpdateAABox(sec);
        Sector_UpdateArea(sec);
        Sector_UpdateBaseOrigin(sec);

        // Set the position of the sound origin for all plane sound origins.
        // Set target heights for all planes.
        for(uint k = 0; k < sec->planeCount; ++k)
        {
            Plane* pln = sec->planes[k];

            Surface_UpdateBaseOrigin(&pln->surface);
            pln->target = pln->height;
        }
    }
}

/**
 * @param sector  Sector in which to link @a base.
 * @param base  Mobj base to link in @a sector. Caller should ensure that the
 *              same object is not linked multiple times into the chain.
 */
static void linkBaseToSectorChain(Sector* sector, ddmobj_base_t* base)
{
    if(!sector || !base) return;

    // The sector's base is always head of the chain, so link the other after it.
    base->thinker.prev = &sector->base.thinker;
    base->thinker.next = sector->base.thinker.next;
    if(base->thinker.next)
        base->thinker.next->prev = &base->thinker;
    sector->base.thinker.next = &base->thinker;
}

/**
 * Chain together the ddmobj_base_t objects owned by all Surfaces in all sectors.
 * These chains are used for efficiently traversing all of the base objects in a
 * sector, for example; stopping sounds emitted from all origins within a sector.
 */
static void chainSectorBases(GameMap* map)
{
    DENG_ASSERT(map);

    for(uint i = 0; i < map->numSectors; ++i)
    {
        Sector* sec = GameMap_Sector(map, i);
        ddmobj_base_t* base = &sec->base;

        // Clear the chain head.
        base->thinker.next = base->thinker.prev = 0;

        // Add all plane base mobjs.
        for(uint j = 0; j < sec->planeCount; ++j)
        {
            Plane* pln = sec->SP_plane(j);
            linkBaseToSectorChain(sec, &pln->PS_base);
        }

        // Add all sidedef base mobjs.
        for(uint j = 0; j < sec->lineDefCount; ++j)
        {
            LineDef* line = sec->lineDefs[j];
            if(line->L_frontsector == sec)
            {
                SideDef* side = line->L_frontsidedef;
                linkBaseToSectorChain(sec, &side->SW_middlesurface.base);
                linkBaseToSectorChain(sec, &side->SW_bottomsurface.base);
                linkBaseToSectorChain(sec, &side->SW_topsurface.base);
            }
            if(line->L_backsidedef && line->L_backsector == sec)
            {
                SideDef* side = line->L_backsidedef;
                linkBaseToSectorChain(sec, &side->SW_middlesurface.base);
                linkBaseToSectorChain(sec, &side->SW_bottomsurface.base);
                linkBaseToSectorChain(sec, &side->SW_topsurface.base);
            }
        }
    }
}

static void finishSideDefs(GameMap* map)
{
    DENG_ASSERT(map);

    // Calculate the tangent space surface vectors.
    for(uint i = 0; i < map->numSideDefs; ++i)
    {
        SideDef* side = &map->sideDefs[i];
        SideDef_UpdateSurfaceTangents(side);
        SideDef_UpdateBaseOrigins(side);
    }
}

static void finishLineDefs(GameMap* map)
{
    DENG_ASSERT(map);

    LOG_VERBOSE("Finalizing LineDefs...");

    for(uint i = 0; i < map->numLineDefs; ++i)
    {
        LineDef* ld = &map->lineDefs[i];
        if(!ld->L_frontside.hedgeLeft) continue;

        HEdge const* leftHEdge  = ld->L_frontside.hedgeLeft;
        HEdge const* rightHEdge = ld->L_frontside.hedgeRight;

        ld->v[0] = leftHEdge->HE_v1;
        ld->v[1] = rightHEdge->HE_v2;

        LineDef_UpdateSlope(ld);
        LineDef_UpdateAABox(ld);

        ld->length = V2d_Length(ld->direction);
        ld->angle = bamsAtan2((int) ld->direction[VY], (int) ld->direction[VX]);
    }
}

/**
 * @pre Axis-aligned bounding boxes of all Sectors must be initialized.
 */
static void updateMapBounds(GameMap* map)
{
    DENG_ASSERT(map);

    bool isFirst = true;
    for(uint i = 0; i < map->numSectors; ++i)
    {
        Sector* sec = &map->sectors[i];
        if(!sec->lineDefCount) continue;

        if(isFirst)
        {
            // The first sector is used as is.
            V2d_CopyBox(map->aaBox.arvec2, sec->aaBox.arvec2);
        }
        else
        {
            // Expand the bounding box.
            V2d_UniteBox(map->aaBox.arvec2, sec->aaBox.arvec2);
        }
        isFirst = false;
    }
}

static void prepareBspLeafs(GameMap* map)
{
    for(uint i = 0; i < map->numBspLeafs; ++i)
    {
        BspLeaf* bspLeaf = map->bspLeafs[i];

        BspLeaf_UpdateAABox(bspLeaf);
        BspLeaf_UpdateMidPoint(bspLeaf);
        BspLeaf_UpdateWorldGridOffset(bspLeaf);
    }
}

/**
 * Compares the angles of two lines that share a common vertex.
 *
 * pre: rootVtx must point to the vertex common between a and b
 *      which are (lineowner_t*) ptrs.
 */
static int lineAngleSorter(void const* a, void const* b)
{
    binangle_t angles[2];

    lineowner_t* own[2] = { (lineowner_t *)a, (lineowner_t *)b };
    for(uint i = 0; i < 2; ++i)
    {
        if(own[i]->LO_prev) // We have a cached result.
        {
            angles[i] = own[i]->angle;
        }
        else
        {
            LineDef* line = own[i]->lineDef;
            Vertex* otherVtx = line->L_v(line->L_v1 == rootVtx? 1:0);

            fixed_t dx = otherVtx->origin[VX] - rootVtx->origin[VX];
            fixed_t dy = otherVtx->origin[VY] - rootVtx->origin[VY];

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
 * @return  The newly merged list.
 */
static lineowner_t* mergeLineOwners(lineowner_t* left, lineowner_t* right,
    int (*compare) (void const* a, void const* b))
{
    lineowner_t tmp;
    lineowner_t* np = &tmp;

    tmp.LO_next = np;
    while(left && right)
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
    {
        np->LO_next = left;
    }
    if(right)
    {
        np->LO_next = right;
    }

    // Is the list empty?
    if(tmp.LO_next == &tmp)
        return NULL;

    return tmp.LO_next;
}

static lineowner_t* splitLineOwners(lineowner_t* list)
{
    if(!list) return NULL;

    lineowner_t* lista = list;
    lineowner_t* listb = list;
    lineowner_t* listc = list;

    do
    {
        listc = listb;
        listb = listb->LO_next;
        lista = lista->LO_next;
        if(lista)
        {
            lista = lista->LO_next;
        }
    } while(lista);

    listc->LO_next = NULL;
    return listb;
}

/**
 * This routine uses a recursive mergesort algorithm; O(NlogN)
 */
static lineowner_t* sortLineOwners(lineowner_t* list,
    int (*compare) (void const* a, void const* b))
{
    if(list && list->LO_next)
    {
        lineowner_t* p = splitLineOwners(list);

        // Sort both halves and merge them back.
        list = mergeLineOwners(sortLineOwners(list, compare),
                               sortLineOwners(p, compare), compare);
    }
    return list;
}

static void setVertexLineOwner(Vertex* vtx, LineDef* lineptr, lineowner_t** storage)
{
    if(!lineptr) return;

    // Has this line already been registered with this vertex?
    if(vtx->numLineOwners != 0)
    {
        lineowner_t* p = vtx->lineOwners;
        while(p)
        {
            if(p->lineDef == lineptr)
                return; // Yes, we can exit.

            p = p->LO_next;
        }
    }

    // Add a new owner.
    vtx->numLineOwners++;

    lineowner_t* newOwner = (*storage)++;
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
    DENG_ASSERT(map);

    // We know how many vertex line owners we need (numLineDefs * 2).
    lineowner_t* lineOwners = (lineowner_t*) Z_Malloc(sizeof(lineowner_t) * map->numLineDefs * 2, PU_MAPSTATIC, 0);
    lineowner_t* allocator = lineOwners;

    for(uint i = 0; i < map->numLineDefs; ++i)
    {
        LineDef* line = map->lineDefs[i];

        for(uint p = 0; p < 2; ++p)
        {
            Vertex* vtx = line->L_v(p);
            setVertexLineOwner(vtx, line, &allocator);
        }
    }
}

/// Sort line owners and then finish the rings.
static void hardenVertexOwnerRings(GameMap* dest, editmap_t* src)
{
    DENG_ASSERT(dest && src);

    for(uint i = 0; i < src->numVertexes; ++i)
    {
        Vertex* v = src->vertexes[i];

        if(!v->numLineOwners) continue;

        // Redirect the linedef links to the hardened map.
        lineowner_t* p = v->lineOwners;
        while(p)
        {
            p->lineDef = &dest->lineDefs[p->lineDef->origIndex - 1];
            p = p->LO_next;
        }

        // Sort them; ordered clockwise by angle.
        rootVtx = v;
        v->lineOwners = sortLineOwners(v->lineOwners, lineAngleSorter);

        // Finish the linking job and convert to relative angles.
        // They are only singly linked atm, we need them to be doubly
        // and circularly linked.
        binangle_t firstAngle = v->lineOwners->angle;
        lineowner_t* last = v->lineOwners;
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

/*#if _DEBUG
        // Check the line owner link rings are formed correctly.
        lineowner_t* base;
        uint idx;

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
#endif*/
    }
}

static void hardenLinedefs(GameMap* dest, editmap_t* src)
{
    dest->numLineDefs = src->numLineDefs;
    dest->lineDefs = (LineDef*) Z_Calloc(dest->numLineDefs * sizeof(LineDef), PU_MAPSTATIC, 0);

    for(uint i = 0; i < dest->numLineDefs; ++i)
    {
        LineDef* destL = &dest->lineDefs[i];
        LineDef* srcL = src->lineDefs[i];

        memcpy(destL, srcL, sizeof(*destL));

        /// @todo We shouldn't still have lines with missing fronts but...
        destL->L_frontsidedef = (srcL->L_frontsidedef?
            &dest->sideDefs[srcL->L_frontsidedef->buildData.index - 1] : NULL);
        destL->L_backsidedef = (srcL->L_backsidedef?
            &dest->sideDefs[srcL->L_backsidedef->buildData.index - 1] : NULL);

        if(destL->L_frontsidedef)
            destL->L_frontsidedef->line = destL;
        if(destL->L_backsidedef)
            destL->L_backsidedef->line = destL;

        destL->L_frontsector = (srcL->L_frontsector?
            &dest->sectors[srcL->L_frontsector->buildData.index - 1] : NULL);

        destL->L_backsector  = (srcL->L_backsector?
            &dest->sectors[srcL->L_backsector->buildData.index - 1] : NULL);
    }
}

static void hardenSidedefs(GameMap* dest, editmap_t* src)
{
    dest->numSideDefs = src->numSideDefs;
    dest->sideDefs = (SideDef*) Z_Malloc(dest->numSideDefs * sizeof(SideDef), PU_MAPSTATIC, 0);

    for(uint i = 0; i < dest->numSideDefs; ++i)
    {
        SideDef* destS = &dest->sideDefs[i];
        SideDef* srcS = src->sideDefs[i];

        memcpy(destS, srcS, sizeof(*destS));
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
    dest->numSectors = src->numSectors;
    dest->sectors = (Sector*) Z_Malloc(dest->numSectors * sizeof(Sector), PU_MAPSTATIC, 0);

    for(uint i = 0; i < dest->numSectors; ++i)
    {
        Sector* destS = &dest->sectors[i];
        Sector* srcS = src->sectors[i];

        memcpy(destS, srcS, sizeof(*destS));
        destS->planeCount = 0;
        destS->planes = NULL;
    }
}

static void hardenPlanes(GameMap* dest, editmap_t* src)
{
    for(uint i = 0; i < dest->numSectors; ++i)
    {
        Sector* destS = &dest->sectors[i];
        Sector* srcS = src->sectors[i];

        for(uint j = 0; j < srcS->planeCount; ++j)
        {
            Plane* destP = R_NewPlaneForSector(destS);
            Plane* srcP = srcS->planes[j];

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
    if(src->numPolyObjs == 0)
    {
        dest->numPolyObjs = 0;
        dest->polyObjs = NULL;
        return;
    }

    dest->numPolyObjs = src->numPolyObjs;
    dest->polyObjs = (Polyobj**) Z_Malloc((dest->numPolyObjs+1) * sizeof(Polyobj*), PU_MAP, 0);

    uint i;
    for(i = 0; i < dest->numPolyObjs; ++i)
    {
        Polyobj* destP, *srcP = src->polyObjs[i];
        HEdge* hedges;
        uint j;

        destP = (Polyobj*) Z_Calloc(POLYOBJ_SIZE, PU_MAP, 0);
        destP->idx = i;
        destP->crush = srcP->crush;
        destP->tag = srcP->tag;
        destP->seqType = srcP->seqType;
        destP->origin[VX] = srcP->origin[VX];
        destP->origin[VY] = srcP->origin[VY];

        destP->lineCount = srcP->lineCount;

        destP->originalPts = (povertex_t*) Z_Malloc(destP->lineCount * sizeof(povertex_t), PU_MAP, 0);
        destP->prevPts     = (povertex_t*) Z_Malloc(destP->lineCount * sizeof(povertex_t), PU_MAP, 0);

        // Create a hedge for each line of this polyobj.
        hedges = (HEdge*) Z_Calloc(sizeof(HEdge) * destP->lineCount, PU_MAP, 0);

        destP->lines = (LineDef**) Z_Malloc(sizeof(*destP->lines) * (destP->lineCount+1), PU_MAP, 0);
        for(j = 0; j < destP->lineCount; ++j)
        {
            LineDef* line = &dest->lineDefs[srcP->lines[j]->origIndex - 1];
            HEdge* hedge = &hedges[j];

            // This line belongs to a polyobj.
            line->inFlags |= LF_POLYOBJ;

            hedge->header.type = DMU_HEDGE;
            hedge->lineDef = line;
            hedge->length = V2d_Distance(line->L_v2origin, line->L_v1origin);
            hedge->twin = NULL;
            hedge->bspLeaf = NULL;
            hedge->sector = line->L_frontsector;

            line->L_frontside.hedgeLeft = line->L_frontside.hedgeRight = hedge;

            destP->lines[j] = line;
        }
        destP->lines[j] = NULL; // Terminate.

        // Add this polyobj to the global list.
        dest->polyObjs[i] = destP;
    }
    dest->polyObjs[i] = NULL; // Terminate.
}

#if 0 /* Currently unused. */
/**
 * @return  The "lowest" vertex (normally the left-most, but if the line is vertical,
 *          then the bottom-most). @c => 0 for start, 1 for end.
 */
static __inline int lineVertexLowest(const LineDef* l)
{
    return (((int) l->v[0]->buildData.pos[VX] < (int) l->v[1]->buildData.pos[VX] ||
             ((int) l->v[0]->buildData.pos[VX] == (int) l->v[1]->buildData.pos[VX] &&
              (int) l->v[0]->buildData.pos[VY] < (int) l->v[1]->buildData.pos[VY]))? 0 : 1);
}

static int C_DECL lineStartCompare(const void* p1, const void* p2)
{
    const LineDef* a = (const LineDef*) p1;
    const LineDef* b = (const LineDef*) p2;
    Vertex* c, *d;

    // Determine left-most vertex of each line.
    c = (lineVertexLowest(a)? a->v[1] : a->v[0]);
    d = (lineVertexLowest(b)? b->v[1] : b->v[0]);

    if((int) c->buildData.pos[VX] != (int) d->buildData.pos[VX])
        return (int) c->buildData.pos[VX] - (int) d->buildData.pos[VX];

    return (int) c->buildData.pos[VY] - (int) d->buildData.pos[VY];
}

static int C_DECL lineEndCompare(const void* p1, const void* p2)
{
    const LineDef* a = (const LineDef*) p1;
    const LineDef* b = (const LineDef*) p2;
    Vertex* c, *d;

    // Determine right-most vertex of each line.
    c = (lineVertexLowest(a)? a->v[0] : a->v[1]);
    d = (lineVertexLowest(b)? b->v[0] : b->v[1]);

    if((int) c->buildData.pos[VX] != (int) d->buildData.pos[VX])
        return (int) c->buildData.pos[VX] - (int) d->buildData.pos[VX];

    return (int) c->buildData.pos[VY] - (int) d->buildData.pos[VY];
}

size_t numOverlaps;

int testOverlaps(LineDef* b, void* data)
{
    LineDef* a = (LineDef*) data;

    if(a != b)
    {
        if(lineStartCompare(a, b) == 0)
            if(lineEndCompare(a, b) == 0)
            {
                // Found an overlap!
                b->buildData.overlap = (a->buildData.overlap ? a->buildData.overlap : a);
                numOverlaps++;
            }
    }

    return false; // Continue iteration.
}

typedef struct {
    GameMap* map;
    uint coords[2]; // Blockmap cell coordinates.
} findoverlaps_params_t;

int findOverlapsForLinedef(LineDef* l, void* data)
{
    findoverlaps_params_t* p = (findoverlaps_params_t*) data;
    GameMap_IterateCellLineDefs(p->map, p->coords, testOverlaps, l);
    return false; // Continue iteration.
}

/**
 * @note Does not detect partially overlapping lines!
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
static void findBounds(Vertex const** vertexes, uint numVertexes, vec2d_t min, vec2d_t max)
{
    vec2d_t bounds[2], point;
    const Vertex* vtx;
    uint i;

    if(!min && !max) return;

    if(!vertexes || !numVertexes)
    {
        V2d_Set(min, DDMAXFLOAT, DDMAXFLOAT);
        V2d_Set(max, DDMINFLOAT, DDMINFLOAT);
        return;
    }

    for(i = 0; i < numVertexes; ++i)
    {
        vtx = vertexes[i];
        V2d_Set(point, vtx->origin[VX], vtx->origin[VY]);
        if(!i)
            V2d_InitBox(bounds, point);
        else
            V2d_AddToBox(bounds, point);
    }

    if(min)
    {
        V2d_Set(min, bounds[0][VX], bounds[0][VY]);
    }
    if(max)
    {
        V2d_Set(max, bounds[1][VX], bounds[1][VY]);
    }
}

static boolean buildBsp(GameMap* gamemap)
{
    BspBuilder_c* bspBuilder = NULL;
    uint startTime;
    boolean builtOK;

    if(!map) return false;

    LegacyCore_PrintfLogFragmentAtLevel(DE2_LOG_INFO,
        "Building BSP using tunable split factor of %d...\n", bspFactor);

    // It begins...
    startTime = Timer_RealMilliseconds();

    bspBuilder = BspBuilder_New(gamemap, &map->numVertexes, &map->vertexes);
    BspBuilder_SetSplitCostFactor(bspBuilder, bspFactor);

    builtOK = BspBuilder_Build(bspBuilder);
    if(builtOK)
    {
        MPE_SaveBsp(bspBuilder, gamemap, &map->numVertexes, &map->vertexes);
    }

    BspBuilder_Delete(bspBuilder);

    // How much time did we spend?
    LegacyCore_PrintfLogFragmentAtLevel(DE2_LOG_INFO,
        "BSP built in %.2f seconds.\n", (Timer_RealMilliseconds() - startTime) / 1000.0f);
    return builtOK;
}

#if 0
/**
 * The REJECT resource is a LUT that provides the results of trivial
 * line-of-sight tests between sectors. This is done with a matrix of sector
 * pairs i.e. if a monster in sector 4 can see the player in sector 2; the
 * inverse should be true.
 *
 * Note however, some PWADS have carefully constructed REJECT data to create
 * special effects. For example it is possible to make a player completely
 * invissible in certain sectors.
 *
 * The format of the table is a simple matrix of boolean values, a (true)
 * value indicates that it is impossible for mobjs in sector A to see mobjs
 * in sector B (and vice-versa). A (false) value indicates that a
 * line-of-sight MIGHT be possible and a more accurate (thus more expensive)
 * calculation will have to be made.
 *
 * The table itself is constructed as follows:
 *
 *     X = sector num player is in
 *     Y = sector num monster is in
 *
 *         X
 *
 *       0 1 2 3 4 ->
 *     0 1 - 1 - -
 *  Y  1 - - 1 - -
 *     2 1 1 - - 1
 *     3 - - - 1 -
 *    \|/
 *
 * These results are read left-to-right, top-to-bottom and are packed into
 * bytes (each byte represents eight results). As are all lumps in WAD the
 * data is in little-endian order.
 *
 * Thus the size of a valid REJECT lump can be calculated as:
 *
 *     ceiling(numSectors^2)
 *
 * For now we only do very basic reject processing, limited to determining
 * all isolated sector groups (islands that are surrounded by void space).
 *
 * @note Algorithm:
 * Initially all sectors are in individual groups. Next, we scan the linedef
 * list. For each 2-sectored line, merge the two sector groups into one.
 */
static void buildReject(gamemap_t* map)
{
    int i, group;
    int* secGroups;
    int view, target;
    size_t rejectSize;
    byte* matrix;

    secGroups = M_Malloc(sizeof(int) * numSectors);
    for(i = 0; i < numSectors; ++i)
    {
        sector_t  *sec = LookupSector(i);
        secGroups[i] = group++;
        sec->rejNext = sec->rejPrev = sec;
    }

    for(i = 0; i < numLinedefs; ++i)
    {
        linedef_t* line = LookupLinedef(i);
        sector_t* sec1, *sec2, *p;

        if(!line->sideDefs[FRONT] || !line->sideDefs[BACK])
            continue;

        sec1 = line->sideDefs[FRONT]->sector;
        sec2 = line->sideDefs[BACK]->sector;

        if(!sec1 || !sec2 || sec1 == sec2)
            continue;

        // Already in the same group?
        if(secGroups[sec1->index] == secGroups[sec2->index])
            continue;

        // Swap sectors so that the smallest group is added to the biggest
        // group. This is based on the assumption that sector numbers in
        // wads will generally increase over the set of linedefs, and so
        // (by swapping) we'll tend to add small groups into larger
        // groups, thereby minimising the updates to 'rej_group' fields
        // that is required when merging.
        if(secGroups[sec1->index] > secGroups[sec2->index])
        {
            p = sec1;
            sec1 = sec2;
            sec2 = p;
        }

        // Update the group numbers in the second group
        secGroups[sec2->index] = secGroups[sec1->index];
        for(p = sec2->rejNext; p != sec2; p = p->rejNext)
            secGroups[p->index] = secGroups[sec1->index];

        // Merge 'em baby...
        sec1->rejNext->rejPrev = sec2;
        sec2->rejNext->rejPrev = sec1;

        p = sec1->rejNext;
        sec1->rejNext = sec2->rejNext;
        sec2->rejNext = p;
    }

    rejectSize = (numSectors * numSectors + 7) / 8;
    matrix = Z_Calloc(rejectSize, PU_MAPSTATIC, 0);

    for(view = 0; view < numSectors; ++view)
        for(target = 0; target < view; ++target)
        {
            int p1, p2;

            if(secGroups[view] == secGroups[target])
                continue;

            // For symmetry, do two bits at a time.
            p1 = view * numSectors + target;
            p2 = target * numSectors + view;

            matrix[p1 >> 3] |= (1 << (p1 & 7));
            matrix[p2 >> 3] |= (1 << (p2 & 7));
        }

    M_Free(secGroups);
}
#endif

boolean MPE_End(void)
{
    boolean builtOK;
    vec2d_t min, max;
    uint i;

    if(!editMapInited)
        return false;

    GameMap* gamemap = (GameMap*) Z_Calloc(sizeof(*gamemap), PU_MAPSTATIC, 0);

    // Pass on the game-specific map entity property database. The game will
    // want to query it once we have finished constructing the map.
    gamemap->entityDatabase = map->entityDatabase;

    /**
     * Perform cleanup on the loaded map data, removing duplicate vertexes,
     * pruning unused sectors etc, etc...
     */
    MPE_DetectDuplicateVertices(map);
    MPE_PruneRedundantMapData(map, PRUNE_ALL);

    buildVertexOwnerRings(map);

    /**
     * Harden most of the map data so that we can construct some of the more
     * intricate data structures early on (and thus make use of them during
     * the BSP generation).
     *
     * @todo I'm sure this can be reworked further so that we destroy as we
     *       go and reduce the current working memory surcharge.
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
    findBounds((Vertex const**)map->vertexes, map->numVertexes, min, max);

    GameMap_InitLineDefBlockmap(gamemap, min, max);
    for(i = 0; i < gamemap->numLineDefs; ++i)
    {
        GameMap_LinkLineDef(gamemap, gamemap->lineDefs + i);
    }

    // Mobj and Polyobj blockmaps are maintained dynamically.
    GameMap_InitMobjBlockmap(gamemap, min, max);
    GameMap_InitPolyobjBlockmap(gamemap, min, max);

    // Announce any missing materials we encountered during the conversion.
    printMissingMaterials();

    /**
     * Build a BSP for this map.
     */
    builtOK = buildBsp(gamemap);

    // Finish the polyobjs (after the vertexes are hardened).
    for(i = 0; i < gamemap->numPolyObjs; ++i)
    {
        Polyobj* po = gamemap->polyObjs[i];
        LineDef** lineIter;
        uint n = 0;

        for(lineIter = po->lines; *lineIter; lineIter++, n++)
        {
            LineDef* line = *lineIter;
            HEdge* hedge = line->L_frontside.hedgeLeft;

            hedge->HE_v1 = line->L_v1;
            hedge->HE_v2 = line->L_v2;

            // The original Pts are based off the anchor Pt, and are unique
            // to each hedge, not each linedef.
            po->originalPts[n].origin[VX] = line->L_v1origin[VX] - po->origin[VX];
            po->originalPts[n].origin[VY] = line->L_v1origin[VY] - po->origin[VY];
        }
    }

    buildSectorBspLeafLists(gamemap);

    // Map must be polygonized and sector->bspLeafs must be built before
    // this is called!
    hardenPlanes(gamemap, map);

    // Destroy the rest of editable map, we are finished with it.
    /// @note Only the vertexes should be left anyway.
    destroyMap();

    if(!builtOK)
    {
        // Failed. Need to clean up.
        clearMaterialDict();
        EntityDatabase_Delete(gamemap->entityDatabase);
        Z_Free(gamemap);
        lastBuiltMapResult = false; // Failed.

        return lastBuiltMapResult;
    }

    buildSectorLineLists(gamemap);
    finishSideDefs(gamemap);
    finishLineDefs(gamemap);
    finishSectors(gamemap);
    chainSectorBases(gamemap);

    updateMapBounds(gamemap);
    S_DetermineBspLeafsAffectingSectorReverb(gamemap);
    prepareBspLeafs(gamemap);

    clearMaterialDict();

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
        char const* markerLumpName = Str_Text(Uri_Path(gamemap->uri));
        lumpnum_t markerLumpNum = F_LumpNumForName(markerLumpName);
        AutoStr* cachedMapDir = DAM_ComposeCacheDir(Str_Text(F_ComposeLumpFilePath(markerLumpNum)));
        ddstring_t cachedMapPath;

        Str_InitStd(&cachedMapPath);
        F_FileName(&cachedMapPath, markerLumpName);
        Str_Append(&cachedMapPath, ".dcm");
        Str_Prepend(&cachedMapPath, Str_Text(cachedMapDir));
        F_ExpandBasePath(&cachedMapPath, &cachedMapPath);

        // Ensure the destination directory exists.
        F_MakePath(Str_Text(cachedMapDir));

        // Archive this map!
        DAM_MapWrite(gamemap, Str_Text(&cachedMapPath));

        Str_Free(&cachedMapPath);
    }

    lastBuiltMap = gamemap;
    lastBuiltMapResult = true; // Success.

    return lastBuiltMapResult;
}

GameMap* MPE_GetLastBuiltMap(void)
{
    return lastBuiltMap;
}

boolean MPE_GetLastBuiltMapResult(void)
{
    return lastBuiltMapResult;
}

uint MPE_VertexCreate(coord_t x, coord_t y)
{
    Vertex* v;

    if(!editMapInited) return 0;

    v = createVertex();
    v->origin[VX] = x;
    v->origin[VY] = y;

    return v->buildData.index;
}

boolean MPE_VertexCreatev(size_t num, coord_t* values, uint* indices)
{
    uint n;

    if(!editMapInited || !num || !values)
        return false;

    // Create many vertexes.
    for(n = 0; n < num; ++n)
    {
        Vertex* v;

        v = createVertex();
        v->origin[VX] = values[n * 2];
        v->origin[VY] = values[n * 2 + 1];

        if(indices)
            indices[n] = v->buildData.index;
    }

    return true;
}

/**
 * Either print or count-the-number-of unresolved references in the
 * material dictionary.
 *
 * @param internId    Unique id associated with the reference.
 * @param parameters  If a uint pointer operate in "count" mode (total written here).
 *                    Else operate in "print" mode.
 * @return Always @c 0 (for use as an iterator).
 */
static int printMissingMaterialWorker(StringPool::Id internId, void* parameters)
{
    uint* count = (uint*)parameters;

    // A valid id?
    if(materialDict->string(internId))
    {
        // Have we resolved this reference yet?
        if(!materialDict->userPointer(internId))
        {
            // An unresolved reference.
            if(count)
            {
                // Count mode.
                *count += 1;
            }
            else
            {
                // Print mode.
                int const refCount = materialDict->userValue(internId);
                String const& materialUri = materialDict->string(internId);
                QByteArray materialUriUtf8 = materialUri.toUtf8();
                Con_Message(" %4u x \"%s\"\n", refCount, materialUriUtf8.constData());
            }
        }
    }
    return 0; // Continue iteration.
}

/**
 * Announce any missing materials we came across when loading the map.
 */
static void printMissingMaterials(void)
{
    if(!materialDict) return;

    // Count missing materials.
    uint numMissing = 0;
    materialDict->iterate(printMissingMaterialWorker, &numMissing);
    if(!numMissing) return;

    Con_Message("  [110] Warning: Found %u unknown %s:\n", numMissing, numMissing == 1? "material":"materials");
    // List the missing materials.
    materialDict->iterate(printMissingMaterialWorker, 0);
}

static void clearMaterialDict(void)
{
    if(!materialDict) return;
    materialDict->clear();
    delete materialDict; materialDict = 0;
}

static void assignSurfaceMaterial(Surface *suf, ddstring_t const *materialUriStr)
{
    DENG_ASSERT(suf);

    material_t *material = 0;
    if(materialUriStr && !Str_IsEmpty(materialUriStr))
    {
        // Are we yet to instantiate the dictionary?
        if(!materialDict)
        {
            materialDict = new StringPool;
        }

        de::Uri materialUri(Str_Text(materialUriStr), RC_NULL);

        // Intern this reference.
        StringPool::Id internId = materialDict->intern(materialUri.compose());

        // Have we previously encountered this?.
        uint refCount = materialDict->userValue(internId);
        if(refCount)
        {
            // Yes, if resolved the user pointer holds the found material.
            material = (material_t *) materialDict->userPointer(internId);
        }
        else
        {
            // No, attempt to resolve this URI and update the dictionary.
            // First try the preferred scheme, then any.
            materialid_t materialId = App_Materials()->resolveUri(materialUri);
            if(materialId == NOMATERIALID)
            {
                materialUri.setScheme("");
                materialId = App_Materials()->resolveUri(materialUri);
            }
            material = App_Materials()->toMaterial(materialId);

            // Insert the possibly resolved material into the dictionary.
            materialDict->setUserPointer(internId, material);
        }

        // There is now one more reference.
        refCount++;
        materialDict->setUserValue(internId, refCount);
    }

    // Assign the resolved material if found.
    Surface_SetMaterial(suf, material);
}

uint MPE_SidedefCreate(short flags, const ddstring_t* topMaterial,
    float topOffsetX, float topOffsetY, float topRed, float topGreen, float topBlue,
    const ddstring_t* middleMaterial, float middleOffsetX, float middleOffsetY, float middleRed,
    float middleGreen, float middleBlue, float middleAlpha, const ddstring_t* bottomMaterial,
    float bottomOffsetX, float bottomOffsetY, float bottomRed, float bottomGreen,
    float bottomBlue)
{
    SideDef* s;

    if(!editMapInited) return 0;

    s = createSide();
    s->flags = flags;

    assignSurfaceMaterial(&s->SW_topsurface, topMaterial);
    Surface_SetMaterialOrigin(&s->SW_topsurface, topOffsetX, topOffsetY);
    Surface_SetColorAndAlpha(&s->SW_topsurface, topRed, topGreen, topBlue, 1);

    assignSurfaceMaterial(&s->SW_middlesurface, middleMaterial);
    Surface_SetMaterialOrigin(&s->SW_middlesurface, middleOffsetX, middleOffsetY);
    Surface_SetColorAndAlpha(&s->SW_middlesurface, middleRed, middleGreen, middleBlue, middleAlpha);

    assignSurfaceMaterial(&s->SW_bottomsurface, bottomMaterial);
    Surface_SetMaterialOrigin(&s->SW_bottomsurface, bottomOffsetX, bottomOffsetY);
    Surface_SetColorAndAlpha(&s->SW_bottomsurface, bottomRed, bottomGreen, bottomBlue, 1);

    return s->buildData.index;
}

uint MPE_LinedefCreate(uint v1, uint v2, uint frontSector, uint backSector,
    uint frontSide, uint backSide, int flags)
{
    LineDef* l;
    SideDef* front = NULL, *back = NULL;
    Vertex* vtx1, *vtx2;
    float length;

    if(!editMapInited) return 0;
    if(frontSector > map->numSectors) return 0;
    if(backSector > map->numSectors) return 0;
    if(frontSide > map->numSideDefs) return 0;
    if(backSide > map->numSideDefs) return 0;
    if(v1 == 0 || v1 > map->numVertexes) return 0;
    if(v2 == 0 || v2 > map->numVertexes) return 0;
    if(v1 == v2) return 0;

    // Ensure that the side indices are unique.
    if(frontSide && map->sideDefs[frontSide - 1]->buildData.refCount)
        return 0;
    if(backSide && map->sideDefs[backSide - 1]->buildData.refCount)
        return 0;

    // Next, check the length is not zero.
    vtx1 = map->vertexes[v1 - 1];
    vtx2 = map->vertexes[v2 - 1];
    length = V2d_Distance(vtx2->origin, vtx1->origin);
    if(!(length > 0)) return 0;

    if(frontSide > 0)
    {
        front = map->sideDefs[frontSide - 1];
    }
    if(backSide > 0)
    {
        back = map->sideDefs[backSide - 1];
    }

    l = createLine();
    l->L_v1 = vtx1;
    l->L_v2 = vtx2;

    l->L_v1->buildData.refCount++;
    l->L_v2->buildData.refCount++;

    l->L_frontsector = (frontSector == 0? NULL: map->sectors[frontSector-1]);
    l->L_backsector  = (backSector  == 0? NULL: map->sectors[backSector-1]);

    l->L_frontsidedef = front;
    l->L_backsidedef = back;

    l->length = length;

    LineDef_UpdateSlope(l);
    LineDef_UpdateAABox(l);

    l->angle = bamsAtan2((int) l->direction[VY], (int) l->direction[VX]);

    // Remember the number of unique references.
    if(l->L_frontsidedef)
    {
        l->L_frontsidedef->line = l;
        l->L_frontsidedef->buildData.refCount++;
    }

    if(l->L_backsidedef)
    {
        l->L_backsidedef->line = l;
        l->L_backsidedef->buildData.refCount++;
    }

    l->inFlags = 0;

    // Determine the default linedef flags.
    l->flags = flags;
    if(!front || !back)
        l->flags |= DDLF_BLOCKING;

    return l->origIndex;
}

uint MPE_PlaneCreate(uint sector, coord_t height, const ddstring_t* materialUri, float matOffsetX,
    float matOffsetY, float r, float g, float b, float a, float normalX, float normalY, float normalZ)
{
    if(!editMapInited) return 0;
    if(sector == 0 || sector > map->numSectors) return 0;

    Sector* s = map->sectors[sector - 1];

    Plane* pln = (Plane*) M_Calloc(sizeof(Plane));
    pln->surface.owner = (void*) pln;
    pln->height = height;

    assignSurfaceMaterial(&pln->surface, materialUri);
    Surface_SetColorAndAlpha(&pln->surface, r, g, b, a);
    Surface_SetMaterialOrigin(&pln->surface, matOffsetX, matOffsetY);

    V3f_Set(pln->PS_normal, normalX, normalY, normalZ);
    V3f_Normalize(pln->PS_normal);
    V3f_BuildTangents(pln->PS_tangent, pln->PS_bitangent, pln->PS_normal);

    pln->type = (pln->PS_normal[VZ] < 0? PLN_CEILING : PLN_FLOOR);

    pln->sector = s;
    Plane** newList = (Plane**) M_Malloc(sizeof(Plane*) * (++s->planeCount + 1));
    uint i;
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
    if(!editMapInited) return 0;

    Sector* s = createSector();

    s->rgb[CR] = MINMAX_OF(0, red, 1);
    s->rgb[CG] = MINMAX_OF(0, green, 1);
    s->rgb[CB] = MINMAX_OF(0, blue, 1);
    s->lightLevel = MINMAX_OF(0, lightlevel, 1);
    s->planeCount = 0;
    s->planes = NULL;

    return s->buildData.index;
}

uint MPE_PolyobjCreate(uint* lines, uint lineCount, int tag, int sequenceType,
    coord_t originX, coord_t originY)
{
    if(!editMapInited || !lineCount || !lines) return 0;

    // First check that all the line indices are valid and that they arn't
    // already part of another polyobj.
    uint i;
    for(i = 0; i < lineCount; ++i)
    {
        if(lines[i] == 0 || lines[i] > map->numLineDefs) return 0;

        LineDef* line = map->lineDefs[lines[i] - 1];
        if(line->inFlags & LF_POLYOBJ) return 0;
    }

    Polyobj* po = createPolyobj();
    po->tag = tag;
    po->seqType = sequenceType;
    po->origin[VX] = originX;
    po->origin[VY] = originY;

    po->lineCount = lineCount;
    po->lines = (LineDef**) M_Calloc(sizeof(LineDef*) * (po->lineCount+1));
    for(i = 0; i < lineCount; ++i)
    {
        LineDef* line = map->lineDefs[lines[i] - 1];

        // This line belongs to a polyobj.
        line->inFlags |= LF_POLYOBJ;
        po->lines[i] = line;
    }
    po->lines[i] = NULL;

    return po->buildData.index;
}

boolean MPE_GameObjProperty(const char* entityName, uint elementIndex,
                            const char* propertyName, valuetype_t type, void* valueAdr)
{
    MapEntityDef* entityDef;
    MapEntityPropertyDef* propertyDef;

    if(!editMapInited) return false;

    if(!entityName || !propertyName || !valueAdr)
        return false; // Hmm...

    // Is this a known entity?
    entityDef = P_MapEntityDefByName(entityName);
    if(!entityDef)
    {
        Con_Message("Warning: MPE_GameObjProperty: Unknown entity name:\"%s\", ignoring.\n", entityName);
        return false;
    }

    // Is this a known property?
    if(MapEntityDef_PropertyByName2(entityDef, propertyName, &propertyDef) < 0)
    {
        Con_Message("Warning: MPE_GameObjProperty: Entity \"%s\" has no \"%s\" property, ignoring.\n", entityName, propertyName);
        return false;
    }

    return P_SetMapEntityProperty(map->entityDatabase, propertyDef, elementIndex, type, valueAdr);
}

// p_data.cpp
DENG_EXTERN_C boolean P_RegisterMapObj(int identifier, const char* name);
DENG_EXTERN_C boolean P_RegisterMapObjProperty(int entityId, int propertyId, const char* propertyName, valuetype_t type);

DENG_DECLARE_API(MPE) =
{
    { DE_API_MAP_EDIT_latest },

    P_RegisterMapObj,
    P_RegisterMapObjProperty,
    MPE_Begin,
    MPE_End,
    MPE_VertexCreate,
    MPE_VertexCreatev,
    MPE_SidedefCreate,
    MPE_LinedefCreate,
    MPE_SectorCreate,
    MPE_PlaneCreate,
    MPE_PolyobjCreate,
    MPE_GameObjProperty
};
