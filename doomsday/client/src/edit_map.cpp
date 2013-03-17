/** @file edit_map.cpp Map Editor interface.
 *
 * @authors Copyright &copy; 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright &copy; 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright &copy; 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#include <cmath>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_misc.h"
#include "de_edit.h"
#include "de_dam.h"
#include "de_filesys.h"

#include <de/StringPool>
#include <BspBuilder>

#include "audio/s_environ.h"

using namespace de;

static void printMissingMaterials();
static void clearMaterialDict();

static void assignSurfaceMaterial(Surface *suf, ddstring_t const *materialUri);

/**
 * Material name references specified during map conversion are recorded in this
 * dictionary. A dictionary is used to avoid repeatedly resolving the same URIs
 * and to facilitate a log of missing materials encountered during the process.
 *
 * The pointer user value holds a pointer to the resolved Material (if found).
 * The integer user value tracks the number of times a reference occurs.
 */
static StringPool *materialDict;

EditMap editMap;
static boolean editMapInited;

static EditMap *e_map = &editMap;
static boolean lastBuiltMapResult;
static GameMap *lastBuiltMap;

static Vertex *rootVtx; // Used when sorting vertex line owners.

static int bspFactor = 7;

void MPE_Register()
{
    C_VAR_INT("bsp-factor", &bspFactor, CVF_NO_MAX, 0, 0);
}

static Vertex *createVertex(coord_t x, coord_t y)
{
    Vertex *vtx = new Vertex(x, y);

    e_map->vertexes.push_back(vtx);
    vtx->_buildData.index = e_map->vertexes.size(); // 1-based index, 0 = NIL.

    return vtx;
}

static LineDef *createLine()
{
    LineDef *line = new LineDef;

    e_map->lines.push_back(line);
    line->_origIndex = e_map->lines.size(); // 1-based index, 0 = NIL.

    return line;
}

static SideDef *createSideDef()
{
    SideDef *sideDef = new SideDef;

    e_map->sideDefs.push_back(sideDef);
    sideDef->_buildData.index = e_map->sideDefs.size(); // 1-based index, 0 = NIL.

    return sideDef;
}

static Sector *createSector(Vector3f const &ambientLightColor, float lightLevel)
{
    Sector *sec = new Sector;

    sec->_lightColor[CR] = de::clamp(0.f, ambientLightColor.x, 1.f);
    sec->_lightColor[CG] = de::clamp(0.f, ambientLightColor.y, 1.f);
    sec->_lightColor[CB] = de::clamp(0.f, ambientLightColor.z, 1.f);
    sec->_lightLevel = de::clamp(0.f, lightLevel, 1.f);
    
    e_map->sectors.push_back(sec);
    sec->_origIndex = e_map->sectors.size(); // 1-based index, 0 = NIL.

    return sec;
}

static Polyobj *createPolyobj()
{
    Polyobj *po = (Polyobj *) M_Calloc(sizeof(*po));

    e_map->polyObjs = (Polyobj **) M_Realloc(e_map->polyObjs, sizeof(po) * (++e_map->numPolyObjs + 1));
    e_map->polyObjs[e_map->numPolyObjs-1] = po;
    e_map->polyObjs[e_map->numPolyObjs] = NULL;

    po->buildData.index = e_map->numPolyObjs; // 1-based index, 0 = NIL.

    return po;
}

static void destroyEditablePolyObjs(EditMap *map)
{
    if(map->polyObjs)
    {
        for(uint i = 0; i < map->numPolyObjs; ++i)
        {
            Polyobj *po = map->polyObjs[i];
            M_Free(po->lines);
            M_Free(po);
        }
        M_Free(map->polyObjs);
    }
    map->polyObjs = 0;
    map->numPolyObjs = 0;
}

static void destroyEditableLineDefs(EditMap *map)
{
    DENG2_FOR_EACH(EditMap::Lines, s, map->lines)
    {
        delete *s;
    }
    map->lines.clear();
}

static void destroyEditableSideDefs(EditMap *map)
{
    DENG2_FOR_EACH(EditMap::SideDefs, s, map->sideDefs)
    {
        delete *s;
    }
    map->sideDefs.clear();
}

static void destroyEditableSectors(EditMap *map)
{
    /*
    if(map->sectors)
    {
        uint i;
        for(i = 0; i < map->numSectors; ++i)
        {
            uint j;
            sector_s* s = map->sectors[i];

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
    map->numSectors = 0;*/

    DENG2_FOR_EACH(EditMap::Sectors, sec, map->sectors)
    {
        delete *sec;
    }
    map->sectors.clear();
}

static void destroyMap()
{
    e_map->vertexes.clear();

    // These should already be gone:
    destroyEditableLineDefs(e_map);
    destroyEditableSideDefs(e_map);
    destroyEditableSectors(e_map);
    destroyEditablePolyObjs(e_map);
}

static int vertexCompare(void const *p1, void const *p2)
{
    Vertex const *a = (Vertex const *) *((void const **) p1);
    Vertex const *b = (Vertex const *) *((void const **) p2);

    if(a == b) return 0;

    if(int(a->origin()[VX]) != int(b->origin()[VX]))
        return int(a->origin()[VX]) - int(b->origin()[VX]);

    return int(a->origin()[VY]) - int(b->origin()[VY]);
}

void MPE_DetectDuplicateVertices(EditMap *map)
{
    DENG_ASSERT(map);
    Vertex **hits = (Vertex **) M_Malloc(map->vertexCount() * sizeof(Vertex *));

    // Sort array of ptrs.
    for(uint i = 0; i < map->vertexCount(); ++i)
    {
        hits[i] = map->vertexes[i];
    }
    qsort(hits, map->vertexCount(), sizeof(Vertex *), vertexCompare);

    // Now mark them off.
    for(uint i = 0; i < map->vertexCount() - 1; ++i)
    {
        // A duplicate?
        if(vertexCompare(hits + i, hits + i + 1) == 0)
        {
            // Yes.
            Vertex *a = hits[i];
            Vertex *b = hits[i + 1];

            b->_buildData.equiv = (a->_buildData.equiv ? a->_buildData.equiv : a);
        }
    }

    M_Free(hits);
}

#if 0
static void findEquivalentVertexes(EditMap *src)
{
    uint i, newNum;

    // Scan all linedefs.
    for(i = 0, newNum = 0; i < src->numLineDefs; ++i)
    {
        LineDef *l = src->lineDefs[i];

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

static void pruneLinedefs(EditMap *map)
{
    uint i, newNum, unused = 0;

    for(i = 0, newNum = 0; i < map->numLineDefs; ++i)
    {
        LineDef *l = map->lineDefs[i];

        if(!l->hasFrontSideDef() && !l->hasBackSideDef())
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
            Con_Message("  Pruned %d unused linedefs.", unused);

        map->numLineDefs = newNum;
    }
}

static void pruneVertices(EditMap *map)
{
    uint i, newNum, unused = 0;

    // Scan all vertices.
    for(i = 0, newNum = 0; i < map->vertexCount(); ++i)
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

    if(newNum < map->vertexCount())
    {
        int dupNum = map->vertexCount() - newNum - unused;

        if(unused > 0)
            Con_Message("  Pruned %d unused vertices.", unused);

        if(dupNum > 0)
            Con_Message("  Pruned %d duplicate vertices.", dupNum);

        map->vertexCount() = newNum;
    }
}

static void pruneUnusedSidedefs(EditMap *map)
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
            Con_Message("  Pruned %d unused sidedefs.", unused);

        if(dupNum > 0)
            Con_Message("  Pruned %d duplicate sidedefs.", dupNum);

        map->numSideDefs = newNum;
    }
}

static void pruneUnusedSectors(EditMap *map)
{
    uint i, newNum;

    for(i = 0; i < map->numSideDefs; ++i)
    {
        SideDef* s = map->sideDefs[i];

        if(s->sector)
            s->sector->buildData.refCount++;
    }

    // Scan all sectors.
    for(i = 0, newNum = 0; i < map->sectorCount(); ++i)
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

    if(newNum < map->sectorCount())
    {
        Con_Message("  Pruned %d unused sectors.", map->sectorCount() - newNum);
        map->sectorCount() = newNum;
    }
}
#endif

/**
 * @warning Order here is critical!
 */
void MPE_PruneRedundantMapData(EditMap* /*map*/, int /*flags*/)
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

#undef MPE_Begin
boolean MPE_Begin(char const *mapUri)
{
    /// @todo Do not ignore; assign to the editable map.
    DENG_UNUSED(mapUri);

    if(editMapInited) return true; // Already been here.

    // Initialize the game-specific map entity property database.
    e_map->entityDatabase = EntityDatabase_New();

    destroyMap();

    lastBuiltMap = 0;
    lastBuiltMapResult = false; // Assume failure.

    editMapInited = true;
    return true;
}

static void hardenSectorBspLeafList(GameMap *map, uint secIDX)
{
    DENG_ASSERT(map && secIDX < map->sectorCount());

    Sector *sec = &map->sectors[secIDX];
    sec->_bspLeafs.clear();

#ifdef DENG2_QT_4_7_OR_NEWER
    uint count = 0;
    for(uint i = 0; i < map->bspLeafCount(); ++i)
    {
        BspLeaf &bspLeaf = map->bspLeafs[i];
        if(bspLeaf.sectorPtr() == sec)
            ++count;
    }
    if(0 == count) return;

    sec->_bspLeafs.reserve(count);
#endif

    for(uint i = 0; i < map->bspLeafCount(); ++i)
    {
        BspLeaf &bspLeaf = map->bspLeafs[i];
        if(bspLeaf.sectorPtr() == sec)
        {
            // Ownership of the BSP leaf is not given to the sector.
            sec->_bspLeafs.append(&bspLeaf);
        }
    }
}

static void buildSectorBspLeafLists(GameMap *map)
{
    DENG_ASSERT(map);

    for(uint i = 0; i < map->sectorCount(); ++i)
    {
        hardenSectorBspLeafList(map, i);
    }
}

static void buildSectorLineLists(GameMap *map)
{
    DENG_ASSERT(map);

    LOG_VERBOSE("Building Sector line lists...");

    struct LineLink
    {
        LineDef *line;
        LineLink *next;
    };

    // Collate a list of lines for each sector.
    zblockset_t *lineLinksBlockSet = ZBlockSet_New(sizeof(LineLink), 512, PU_APPSTATIC);
    LineLink **sectorLineLinks = (LineLink **) Z_Calloc(sizeof(*sectorLineLinks) * map->sectorCount(), PU_APPSTATIC, 0);

    for(uint i = 0; i < map->lineCount(); ++i)
    {
        LineDef *line = &map->lines[i];

        if(line->hasFrontSector())
        {
            int const sectorIndex = GameMap_SectorIndex(map, &line->frontSector());

            LineLink *link = (LineLink *) ZBlockSet_Allocate(lineLinksBlockSet);
            link->line = line;
            link->next = sectorLineLinks[sectorIndex];
            sectorLineLinks[sectorIndex] = link;
        }

        if(line->hasBackSector() && !line->isSelfReferencing())
        {
            int const sectorIndex = GameMap_SectorIndex(map, &line->backSector());

            LineLink *link = (LineLink *) ZBlockSet_Allocate(lineLinksBlockSet);
            link->line = line;
            link->next = sectorLineLinks[sectorIndex];
            sectorLineLinks[sectorIndex] = link;
        }
    }

    // Build the actual sector line lists.
    for(uint i = 0; i < map->sectorCount(); ++i)
    {
        Sector *sec = &map->sectors[i];

        sec->_lines.clear();

        if(!sectorLineLinks[i]) continue;

#ifdef DENG2_QT_4_7_OR_NEWER
        // Count the total number of lines in this sector.
        uint numLines = 0;
        for(LineLink *link = sectorLineLinks[i]; link; link = link->next)
        {
            numLines++;
        }
        // Reserve this much storage.
        sec->_lines.reserve(numLines);
#endif

        /**
         * The behaviour of some algorithms used in original DOOM are
         * dependant upon the order of these lists (e.g., EV_DoFloor
         * and EV_BuildStairs). Lets be helpful and use the same order.
         *
         * Sort: Original line index, ascending.
         */
        for(LineLink *link = sectorLineLinks[i]; link; link = link->next)
        {
            // Ownership of the line is not given to the sector.
            sec->_lines.prepend(link->line);
        }
    }

    // Free temporary storage.
    ZBlockSet_Delete(lineLinksBlockSet);
    Z_Free(sectorLineLinks);
}

static void finishSectors(GameMap *map)
{
    DENG_ASSERT(map);

    for(uint i = 0; i < map->sectorCount(); ++i)
    {
        Sector &sec = map->sectors[i];

        sec.updateAABox();
        sec.updateRoughArea();
        sec.updateSoundEmitterOrigin();

        // Set the position of the sound origin for all plane sound origins.
        // Set target heights for all planes.
        for(uint k = 0; k < sec.planeCount(); ++k)
        {
            Plane &pln = sec.plane(k);

            pln.surface().updateSoundEmitterOrigin();
            pln._targetHeight = pln._height;
        }
    }
}

/**
 * @param sector  Sector in which to link @a base.
 * @param base  Mobj base to link in @a sector. Caller should ensure that the
 *              same object is not linked multiple times into the chain.
 */
static void linkToSectorEmitterChain(Sector *sector, ddmobj_base_t *otherEmitter)
{
    if(!sector || !otherEmitter) return;

    // The sector's base is always head of the chain, so link the other after it.
    otherEmitter->thinker.prev = &sector->_soundEmitter.thinker;
    otherEmitter->thinker.next = sector->_soundEmitter.thinker.next;
    if(otherEmitter->thinker.next)
        otherEmitter->thinker.next->prev = &otherEmitter->thinker;
    sector->_soundEmitter.thinker.next = &otherEmitter->thinker;
}

/**
 * Chain together the ddmobj_base_t objects owned by all Surfaces in all sectors.
 * These chains are used for efficiently traversing all of the sound emitters in
 * a sector (e.g., when stopping all sounds emitted in the sector).
 */
static void chainSectorSoundEmitters(GameMap *map)
{
    DENG_ASSERT(map);

    for(uint i = 0; i < map->sectorCount(); ++i)
    {
        Sector *sec = GameMap_Sector(map, i);
        ddmobj_base_t &emitter = sec->soundEmitter();

        // Clear the head of the sound emitter chain.
        emitter.thinker.next = emitter.thinker.prev = 0;

        // Add all plane base mobjs.
        foreach(Plane *plane, sec->planes())
        {
            linkToSectorEmitterChain(sec, &plane->surface().soundEmitter());
        }

        // Add all sidedef base mobjs.
        foreach(LineDef *line, sec->lines())
        {
            if(line->frontSectorPtr() == sec)
            {
                SideDef &side = line->frontSideDef();
                linkToSectorEmitterChain(sec, &side.middle().soundEmitter());
                linkToSectorEmitterChain(sec, &side.bottom().soundEmitter());
                linkToSectorEmitterChain(sec, &side.top().soundEmitter());
            }
            if(line->hasBackSideDef() && line->backSectorPtr() == sec)
            {
                SideDef &side = line->backSideDef();
                linkToSectorEmitterChain(sec, &side.middle().soundEmitter());
                linkToSectorEmitterChain(sec, &side.bottom().soundEmitter());
                linkToSectorEmitterChain(sec, &side.top().soundEmitter());
            }
        }
    }
}

static void finishSideDefs(GameMap *map)
{
    DENG_ASSERT(map);

    // Calculate the tangent space surface vectors.
    for(uint i = 0; i < map->sideDefCount(); ++i)
    {
        SideDef &side = map->sideDefs[i];
        side.updateSurfaceTangents();
        side.updateSoundEmitterOrigins();
    }
}

static void finishLines(GameMap *map)
{
    DENG_ASSERT(map);

    for(uint i = 0; i < map->lineCount(); ++i)
    {
        LineDef &line = map->lines[i];
        LineDef::Side &front = line.front();

        if(!front._leftHEdge) continue;

        line._v[0] = &front.leftHEdge().v1();
        line._v[1] = &front.rightHEdge().v2();

        line.updateSlopeType();
        line.updateAABox();

        line._length = V2d_Length(line._direction);
        line._angle = bamsAtan2(int( line._direction[VY] ),
                                int( line._direction[VX] ));
    }
}

/**
 * @pre Axis-aligned bounding boxes of all Sectors must be initialized.
 */
static void updateMapBounds(GameMap *map)
{
    DENG_ASSERT(map);

    bool isFirst = true;
    for(uint i = 0; i < map->sectorCount(); ++i)
    {
        Sector *sec = &map->sectors[i];

        // Sectors with no lines have invalid bounds; skip them.
        if(!sec->lineCount()) continue;

        if(isFirst)
        {
            // The first sector is used as is.
            V2d_CopyBox(map->aaBox.arvec2, sec->aaBox().arvec2);
            isFirst = false;
        }
        else
        {
            // Expand the bounding box.
            V2d_UniteBox(map->aaBox.arvec2, sec->aaBox().arvec2);
        }
    }
}

static void prepareBspLeafs(GameMap *map)
{
    DENG_ASSERT(map);
    for(uint i = 0; i < map->bspLeafCount(); ++i)
    {
        BspLeaf &bspLeaf = map->bspLeafs[i];

        bspLeaf.updateAABox();
        bspLeaf.updateCenter();
        bspLeaf.updateWorldGridOffset();
    }
}

/**
 * Compares the angles of two lines that share a common vertex.
 *
 * pre: rootVtx must point to the vertex common between a and b
 *      which are (lineowner_t*) ptrs.
 */
static int lineAngleSorter(void const *a, void const *b)
{
    binangle_t angles[2];

    LineOwner *own[2] = { (LineOwner *)a, (LineOwner *)b };
    for(uint i = 0; i < 2; ++i)
    {
        if(own[i]->_link[LineOwner::Previous]) // We have a cached result.
        {
            angles[i] = own[i]->angle();
        }
        else
        {
            LineDef *line = &own[i]->line();
            Vertex const &otherVtx = line->vertex(&line->v1() == rootVtx? 1:0);

            fixed_t dx = otherVtx.origin()[VX] - rootVtx->origin()[VX];
            fixed_t dy = otherVtx.origin()[VY] - rootVtx->origin()[VY];

            own[i]->_angle = angles[i] = bamsAtan2(-100 *dx, 100 * dy);

            // Mark as having a cached angle.
            own[i]->_link[LineOwner::Previous] = (LineOwner *) 1;
        }
    }

    return (angles[1] - angles[0]);
}

/**
 * Merge left and right line owner lists into a new list.
 *
 * @return  The newly merged list.
 */
static LineOwner *mergeLineOwners(LineOwner *left, LineOwner *right,
    int (*compare) (void const *a, void const *b))
{
    LineOwner tmp;
    LineOwner *np = &tmp;

    tmp._link[LineOwner::Next] = np;
    while(left && right)
    {
        if(compare(left, right) <= 0)
        {
            np->_link[LineOwner::Next] = left;
            np = left;

            left = &left->next();
        }
        else
        {
            np->_link[LineOwner::Next] = right;
            np = right;

            right = &right->next();
        }
    }

    // At least one of these lists is now empty.
    if(left)
    {
        np->_link[LineOwner::Next] = left;
    }
    if(right)
    {
        np->_link[LineOwner::Next] = right;
    }

    // Is the list empty?
    if(!tmp.hasNext())
        return NULL;

    return &tmp.next();
}

static LineOwner *splitLineOwners(LineOwner *list)
{
    if(!list) return NULL;

    LineOwner *lista = list;
    LineOwner *listb = list;
    LineOwner *listc = list;

    do
    {
        listc = listb;
        listb = &listb->next();
        lista = &lista->next();
        if(lista)
        {
            lista = &lista->next();
        }
    } while(lista);

    listc->_link[LineOwner::Next] = NULL;
    return listb;
}

/**
 * This routine uses a recursive mergesort algorithm; O(NlogN)
 */
static LineOwner *sortLineOwners(LineOwner *list,
    int (*compare) (void const *a, void const *b))
{
    if(list && list->_link[LineOwner::Next])
    {
        LineOwner *p = splitLineOwners(list);

        // Sort both halves and merge them back.
        list = mergeLineOwners(sortLineOwners(list, compare),
                               sortLineOwners(p, compare), compare);
    }
    return list;
}

static void setVertexLineOwner(Vertex *vtx, LineDef *lineptr, LineOwner **storage)
{
    if(!lineptr) return;

    // Has this line already been registered with this vertex?
    LineOwner const *own = vtx->firstLineOwner();
    while(own)
    {
        if(&own->line() == lineptr)
            return; // Yes, we can exit.

        own = &own->next();
    }

    // Add a new owner.
    vtx->_numLineOwners++;
    LineOwner *newOwner = (*storage)++;

    newOwner->_line = lineptr;
    newOwner->_link[LineOwner::Previous] = NULL;

    // Link it in.
    // NOTE: We don't bother linking everything at this stage since we'll
    // be sorting the lists anyway. After which we'll finish the job by
    // setting the prev and circular links.
    // So, for now this is only linked singlely, forward.
    newOwner->_link[LineOwner::Next] = vtx->_lineOwners;
    vtx->_lineOwners = newOwner;

    // Link the line to its respective owner node.
    if(vtx == &lineptr->v1())
        lineptr->_vo[FROM] = newOwner;
    else
        lineptr->_vo[TO] = newOwner;
}

/**
 * Generates the line owner rings for each vertex. Each ring includes all
 * the lines which the vertex belongs to sorted by angle, (the rings are
 * arranged in clockwise order, east = 0).
 */
static void buildVertexOwnerRings(EditMap *map)
{
    DENG_ASSERT(map);

    // We know how many vertex line owners we need (numLineDefs * 2).
    LineOwner *lineOwners = (LineOwner *) Z_Malloc(sizeof(LineOwner) * map->lines.size() * 2, PU_MAPSTATIC, 0);
    LineOwner *allocator = lineOwners;

    for(uint i = 0; i < map->lines.size(); ++i)
    {
        LineDef *line = map->lines[i];

        for(uint p = 0; p < 2; ++p)
        {
            setVertexLineOwner(&line->vertex(p), line, &allocator);
        }
    }
}

/// Sort line owners and then finish the rings.
static void hardenVertexOwnerRings(GameMap *dest, EditMap *src)
{
    DENG_ASSERT(dest && src);

    for(uint i = 0; i < src->vertexCount(); ++i)
    {
        Vertex *v = src->vertexes[i];

        if(!v->_numLineOwners) continue;

        // Redirect the linedef links to the hardened map.
        LineOwner *p = v->_lineOwners;
        while(p)
        {
            p->_line = &dest->lines[p->_line->_origIndex - 1];
            p = &p->next();
        }

        // Sort them; ordered clockwise by angle.
        rootVtx = v;
        v->_lineOwners = sortLineOwners(v->_lineOwners, lineAngleSorter);

        // Finish the linking job and convert to relative angles.
        // They are only singly linked atm, we need them to be doubly
        // and circularly linked.
        binangle_t firstAngle = v->_lineOwners->angle();
        LineOwner *last = v->_lineOwners;
        p = &last->next();
        while(p)
        {
            p->_link[LineOwner::Previous] = last;

            // Convert to a relative angle between last and this.
            last->_angle = last->angle() - p->angle();

            last = p;
            p = &p->next();
        }
        last->_link[LineOwner::Next] = v->_lineOwners;
        v->_lineOwners->_link[LineOwner::Previous] = last;

        // Set the angle of the last owner.
        last->_angle = last->angle() - firstAngle;

/*#if _DEBUG
        // Check the line owner link rings are formed correctly.
        lineowner_t *base;
        uint idx;

        if(verbose >= 2)
            Con_Message("Vertex #%i: line owners #%i", i, v->numLineOwners);

        p = base = v->firstLineOwner();
        idx = 0;
        do
        {
            if(verbose >= 2)
                Con_Message("  %i: p= #%05i this= #%05i n= #%05i, dANG= %-3.f",
                            idx, &p->prev().line() - map->lines,
                            &p->line() - map->lineDefs,
                            &p->next().line() - map->lines, BANG2DEG(p->angle()));

            if(&p->prev().next() != p || &p->next().prev() != p)
               Con_Error("Invalid line owner link ring!");

            p = &p->next();
            idx++;
        } while(p != base);
#endif*/
    }
}

static void hardenLinedefs(GameMap *dest, EditMap *src)
{
    DENG2_ASSERT(dest && src);

    dest->lines.clearAndResize(src->lines.size());

    for(uint i = 0; i < dest->lineCount(); ++i)
    {
        LineDef &destL = dest->lines[i];
        LineDef const &srcL = *src->lines[i];

        destL = srcL;

        /// @todo We shouldn't still have lines with missing fronts but...
        destL.front()._sideDef = (srcL.front()._sideDef?
            &dest->sideDefs[srcL.front()._sideDef->_buildData.index - 1] : NULL);
        destL.back()._sideDef = (srcL.back()._sideDef?
            &dest->sideDefs[srcL.back()._sideDef->_buildData.index - 1] : NULL);

        if(destL.hasFrontSideDef())
            destL.frontSideDef()._line = &destL;
        if(destL.hasBackSideDef())
            destL.backSideDef()._line = &destL;

        destL.front()._sector = (srcL.front()._sector?
            &dest->sectors[srcL.front()._sector->_origIndex - 1] : NULL);

        destL.back()._sector  = (srcL.back()._sector?
            &dest->sectors[srcL.back()._sector->_origIndex - 1] : NULL);
    }
}

static void hardenSidedefs(GameMap *dest, EditMap *src)
{
    DENG2_ASSERT(dest && src);

    dest->sideDefs.clearAndResize(src->sideDefs.size());

    for(uint i = 0; i < dest->sideDefCount(); ++i)
    {
        SideDef &destS = dest->sideDefs[i];
        SideDef const &srcS = *src->sideDefs[i];

        destS = srcS;
    }
}

static void hardenSectors(GameMap *dest, EditMap *src)
{
    dest->sectors.clearAndResize(src->sectorCount());

    for(uint i = 0; i < src->sectorCount(); ++i)
    {
        Sector &destS = dest->sectors[i];
        Sector const &srcS = *src->sectors[i];

        destS = srcS;
        destS._planes.clear(); // ownership of planes not transferred
    }
}

static void hardenPlanes(GameMap *dest, EditMap *src)
{
    for(uint i = 0; i < dest->sectorCount(); ++i)
    {
        Sector &destS = dest->sectors[i];
        Sector const &srcS = *src->sectors[i];

        for(uint j = 0; j < srcS.planeCount(); ++j)
        {
            Plane *destP = R_NewPlaneForSector(&destS);
            Plane const &srcP = srcS.plane(j);

            destP->_height =
                destP->_oldHeight[0] =
                    destP->_oldHeight[1] =
                        destP->_visHeight = srcP._height;

            destP->_visHeightDelta = 0;
            destP->_surface        = srcP._surface;
            destP->_type           = srcP._type;
            destP->_sector         = &destS;
        }
    }
}

static void hardenPolyobjs(GameMap *dest, EditMap *src)
{
    if(src->numPolyObjs == 0)
    {
        dest->numPolyObjs = 0;
        dest->polyObjs = NULL;
        return;
    }

    dest->numPolyObjs = src->numPolyObjs;
    dest->polyObjs = (Polyobj **) Z_Malloc((dest->numPolyObjs + 1) * sizeof(Polyobj *), PU_MAP, 0);

    for(uint i = 0; i < dest->numPolyObjs; ++i)
    {
        Polyobj *srcP  = src->polyObjs[i];
        Polyobj *destP = (Polyobj *) Z_Calloc(POLYOBJ_SIZE, PU_MAP, 0);

        destP->idx = i;
        destP->crush = srcP->crush;
        destP->tag = srcP->tag;
        destP->seqType = srcP->seqType;
        destP->origin[VX] = srcP->origin[VX];
        destP->origin[VY] = srcP->origin[VY];

        destP->lineCount = srcP->lineCount;

        destP->originalPts = (povertex_t *) Z_Malloc(destP->lineCount * sizeof(povertex_t), PU_MAP, 0);
        destP->prevPts     = (povertex_t *) Z_Malloc(destP->lineCount * sizeof(povertex_t), PU_MAP, 0);

        // Create a hedge for each line of this polyobj.
        // TODO: Polyobj has ownership, must free it.
        HEdge *hedges = new HEdge[destP->lineCount];

        destP->lines = (LineDef **) Z_Malloc(sizeof(*destP->lines) * (destP->lineCount + 1), PU_MAP, 0);
        for(uint j = 0; j < destP->lineCount; ++j)
        {
            LineDef *line = &dest->lines[srcP->lines[j]->_origIndex - 1];
            HEdge *hedge = &hedges[j];

            // This line belongs to a polyobj.
            line->_inFlags |= LF_POLYOBJ;

            //hedge->header.type = DMU_HEDGE;
            hedge->_line = line;
            hedge->_length = V2d_Distance(line->v2Origin(), line->v1Origin());
            hedge->_twin = NULL;
            hedge->_bspLeaf = 0;
            hedge->_sector = line->frontSectorPtr();

            line->front()._leftHEdge = line->front()._rightHEdge = hedge;

            destP->lines[j] = line;
        }
        destP->lines[destP->lineCount] = NULL; // Terminate.

        // Add this polyobj to the global list.
        dest->polyObjs[i] = destP;
    }
    dest->polyObjs[dest->numPolyObjs] = NULL; // Terminate.
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

    VERBOSE( Con_Message("Detected %lu overlapping linedefs.", (unsigned long) numOverlaps) )
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
        V2d_Set(point, vtx->origin()[VX], vtx->origin()[VY]);
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

struct HEdgeCollectorParms
{
    GameMap *map;
    BspBuilder *builder;
};

static int hedgeCollector(BspTreeNode &tree, void *parameters)
{
    if(tree.isLeaf())
    {
        HEdgeCollectorParms* p = static_cast<HEdgeCollectorParms*>(parameters);
        BspLeaf *leaf = tree.userData()->castTo<BspLeaf>();
        HEdge *base = leaf->firstHEdge();
        HEdge *hedge = base;
        do
        {
            // Take ownership of this HEdge.
            p->builder->take(hedge);

            // Add this HEdge to the LUT.
            hedge->_origIndex = p->map->hedges.count();
            p->map->hedges.append(hedge);

            if(hedge->hasLine())
            {
                Vertex const &vtx = hedge->line().vertex(hedge->lineSideId());

                hedge->_sector = hedge->line().sectorPtr(hedge->lineSideId());
                hedge->_lineOffset = V2d_Distance(hedge->v1Origin(), vtx.origin());
            }

            hedge->_angle = bamsAtan2(int( hedge->v2Origin()[VY] - hedge->v1Origin()[VY] ),
                                      int( hedge->v2Origin()[VX] - hedge->v1Origin()[VX] )) << FRACBITS;

            // Calculate the length of the segment.
            hedge->_length = V2d_Distance(hedge->v2Origin(), hedge->v1Origin());

            if(hedge->_length == 0)
                hedge->_length = 0.01f; // Hmm...

        } while((hedge = &hedge->next()) != base);
    }
    return false; // Continue traversal.
}

static void collateHEdges(BspBuilder &builder, GameMap &map)
{
    DENG2_ASSERT(map.hedges.isEmpty());

    if(!builder.numHEdges()) return; // Should never happen.
#ifdef DENG2_QT_4_7_OR_NEWER
    map.hedges.reserve(builder.numHEdges());
#endif

    HEdgeCollectorParms parms;
    parms.builder = &builder;
    parms.map     = &map;
    builder.root()->traverseInOrder(hedgeCollector, &parms);
}

static void collateVertexes(BspBuilder &builder, GameMap &map,
    uint *numEditableVertexes, Vertex const ***editableVertexes)
{
    uint bspVertexCount = builder.numVertexes();

    DENG2_ASSERT(map.vertexes.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    map.vertexes.reserve(*numEditableVertexes + bspVertexCount);
#endif

    uint n = 0;
    for(; n < *numEditableVertexes; ++n)
    {
        Vertex *vtx = const_cast<Vertex *>((*editableVertexes)[n]);
        map.vertexes.append(vtx);
    }

    for(uint i = 0; i < bspVertexCount; ++i, ++n)
    {
        Vertex &vtx  = builder.vertex(i);

        builder.take(&vtx);
        map.vertexes.append(&vtx);
    }
}

struct CollateBspElementsParms
{
    BspBuilder *builder;
    GameMap *map;
};

static void collateBspElements(BspTreeNode &tree, CollateBspElementsParms const &parms)
{
    // We are only interested in BspNodes at this level.
    if(tree.isLeaf()) return; // Continue iteration.

    // Take ownership of this BspNode.
    DENG2_ASSERT(tree.userData());
    BspNode *node = tree.userData()->castTo<BspNode>();
    parms.builder->take(node);

    // Add this BspNode to the LUT.
    node->_index = parms.map->bspNodes.count();
    parms.map->bspNodes.append(node);

    if(BspTreeNode *right = tree.right())
    {
        if(right->isLeaf())
        {
            // Take ownership of this BspLeaf.
            DENG2_ASSERT(right->userData());
            BspLeaf *leaf = right->userData()->castTo<BspLeaf>();
            parms.builder->take(leaf);

            // Add this BspLeaf to the LUT.
            leaf->_index = parms.map->bspLeafs.count();
            parms.map->bspLeafs.append(leaf);
        }
    }

    if(BspTreeNode *left = tree.left())
    {
        if(left->isLeaf())
        {
            // Take ownership of this BspLeaf.
            DENG2_ASSERT(left->userData());
            BspLeaf *leaf = left->userData()->castTo<BspLeaf>();
            parms.builder->take(leaf);

            // Add this BspLeaf to the LUT.
            leaf->_index = parms.map->bspLeafs.count();
            parms.map->bspLeafs.append(leaf);
        }
    }
}

static int collateBspElementsWorker(BspTreeNode &tree, void *parameters)
{
    collateBspElements(tree, *static_cast<CollateBspElementsParms *>(parameters));
    return false; // Continue iteration.
}

static void collateBSP(BspBuilder &builder, GameMap &map)
{
    DENG2_ASSERT(map.bspLeafs.isEmpty());
    DENG2_ASSERT(map.bspNodes.isEmpty());

#ifdef DENG2_QT_4_7_OR_NEWER
    map.bspNodes.reserve(builder.numNodes());
    map.bspLeafs.reserve(builder.numLeafs());
#endif

    BspTreeNode *rootNode = builder.root();
    map.bsp = rootNode->userData();

    if(rootNode->isLeaf())
    {
        // Take ownership of this leaf.
        DENG2_ASSERT(rootNode->userData());
        BspLeaf *leaf = rootNode->userData()->castTo<BspLeaf>();
        builder.take(leaf);

        // Add this BspLeaf to the LUT.
        leaf->_index = 0;
        map.bspLeafs.append(leaf);

        return;
    }

    CollateBspElementsParms parms;
    parms.builder = &builder;
    parms.map     = &map;
    rootNode->traversePostOrder(collateBspElementsWorker, &parms);
}

static void saveBsp(BspBuilder &builder, GameMap &map, uint numEditableVertexes,
                    Vertex const **editableVertexes)
{
    dint32 rHeight, lHeight;
    BspTreeNode *rootNode = builder.root();
    if(!rootNode->isLeaf())
    {
        rHeight = dint32(rootNode->right()->height());
        lHeight = dint32(rootNode->left()->height());
    }
    else
    {
        rHeight = lHeight = 0;
    }

    LOG_INFO("BSP built: (%d:%d) %d Nodes, %d Leafs, %d HEdges, %d Vertexes.")
            << rHeight << lHeight << builder.numNodes() << builder.numLeafs()
            << builder.numHEdges() << builder.numVertexes();

    collateHEdges(builder, map);
    collateVertexes(builder, map, &numEditableVertexes, &editableVertexes);
    collateBSP(builder, map);
}

static bool buildBsp(GameMap &gamemap)
{
    DENG2_ASSERT(e_map);

    // It begins...
    Time begunAt;

    LOG_INFO("Building BSP using tunable split factor of %d...") << bspFactor;

    // Instantiate and configure a new BSP builder.
    BspBuilder nodeBuilder(gamemap, e_map->vertexCount(), e_map->verticesAsArray(), bspFactor);

    // Build the BSP.
    bool builtOK = nodeBuilder.build();
    if(builtOK)
    {
        // Take ownership of the built map data elements.
        saveBsp(nodeBuilder, gamemap, e_map->vertexCount(), e_map->verticesAsArray());
    }

    // How much time did we spend?
    LOG_INFO(String("BSP built in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));

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
    for(i = 0; i < sectorCount(); ++i)
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

    //GameMap* gamemap = (GameMap*) Z_Calloc(sizeof(*gamemap), PU_MAPSTATIC, 0);
    GameMap* gamemap = new GameMap;

    // Pass on the game-specific map entity property database. The game will
    // want to query it once we have finished constructing the map.
    gamemap->entityDatabase = e_map->entityDatabase;

    /**
     * Perform cleanup on the loaded map data, removing duplicate vertexes,
     * pruning unused sectors etc, etc...
     */
    MPE_DetectDuplicateVertices(e_map);
    MPE_PruneRedundantMapData(e_map, PRUNE_ALL);

    buildVertexOwnerRings(e_map);

    /**
     * Harden most of the map data so that we can construct some of the more
     * intricate data structures early on (and thus make use of them during
     * the BSP generation).
     *
     * @todo I'm sure this can be reworked further so that we destroy as we
     *       go and reduce the current working memory surcharge.
     */
    hardenSectors(gamemap, e_map);
    hardenSidedefs(gamemap, e_map);
    hardenLinedefs(gamemap, e_map);
    hardenPolyobjs(gamemap, e_map);

    hardenVertexOwnerRings(gamemap, e_map);

    // Don't destroy the sectors (planes are linked to them).
    destroyEditableSideDefs(e_map);
    destroyEditableLineDefs(e_map);
    destroyEditablePolyObjs(e_map);

    /**
     * Build blockmaps.
     */
    findBounds(e_map->verticesAsArray(), e_map->vertexCount(), min, max);

    GameMap_InitLineDefBlockmap(gamemap, min, max);
    for(i = 0; i < gamemap->lineCount(); ++i)
    {
        GameMap_LinkLineDef(gamemap, &gamemap->lines[i]);
    }

    // Mobj and Polyobj blockmaps are maintained dynamically.
    GameMap_InitMobjBlockmap(gamemap, min, max);
    GameMap_InitPolyobjBlockmap(gamemap, min, max);

    // Announce any missing materials we encountered during the conversion.
    printMissingMaterials();

    /**
     * Build a BSP for this map.
     */
    builtOK = buildBsp(*gamemap);

    // Finish the polyobjs (after the vertexes are hardened).
    for(i = 0; i < gamemap->numPolyObjs; ++i)
    {
        Polyobj *po = gamemap->polyObjs[i];

        uint n = 0;
        for(LineDef **lineIter = po->lines; *lineIter; lineIter++, n++)
        {
            LineDef *line = *lineIter;
            HEdge &hedge = line->front().leftHEdge();

            hedge._v[0] = &line->v1();
            hedge._v[1] = &line->v2();

            // The original Pts are based off the anchor Pt, and are unique
            // to each hedge, not each linedef.
            po->originalPts[n].origin[VX] = line->v1Origin()[VX] - po->origin[VX];
            po->originalPts[n].origin[VY] = line->v1Origin()[VY] - po->origin[VY];
        }
    }

    buildSectorBspLeafLists(gamemap);

    // Map must be polygonized and sector->bspLeafs must be built before
    // this is called!
    hardenPlanes(gamemap, e_map);

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
    finishLines(gamemap);
    finishSectors(gamemap);
    chainSectorSoundEmitters(gamemap);

    updateMapBounds(gamemap);
    S_DetermineBspLeafsAffectingSectorReverb(gamemap);
    prepareBspLeafs(gamemap);

    clearMaterialDict();

    editMapInited = false;

    // Call the game's setup routines.
    if(gx.SetupForMapData)
    {
        gx.SetupForMapData(DMU_VERTEX, gamemap->vertexCount());
        gx.SetupForMapData(DMU_LINEDEF, gamemap->lineCount());
        gx.SetupForMapData(DMU_SIDEDEF, gamemap->sideDefCount());
        gx.SetupForMapData(DMU_SECTOR, gamemap->sectorCount());
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

#undef MPE_VertexCreate
uint MPE_VertexCreate(coord_t x, coord_t y)
{
    if(!editMapInited) return 0;

    Vertex *v = createVertex(x, y);

    return v->_buildData.index;
}

#undef MPE_VertexCreatev
boolean MPE_VertexCreatev(size_t num, coord_t *values, uint *indices)
{
    if(!editMapInited || !num || !values)
        return false;

    // Create many vertexes.
    for(uint n = 0; n < num; ++n)
    {
        Vertex *v = createVertex(values[n * 2], values[n * 2 + 1]);

        if(indices)
            indices[n] = v->_buildData.index;
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
                Con_Message(" %4u x \"%s\"", refCount, materialUriUtf8.constData());
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

    Con_Message("  [110] Warning: Found %u unknown %s:", numMissing, numMissing == 1? "material":"materials");
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

    Material *material = 0;
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
            material = (Material *) materialDict->userPointer(internId);
        }
        else
        {
            // No, attempt to resolve this URI and update the dictionary.
            // First try the preferred scheme, then any.
            try
            {
                material = &App_Materials().find(materialUri).material();
            }
            catch(Materials::NotFoundError const &)
            {
                // Try any scheme.
                try
                {
                    materialUri.setScheme("");
                    material = &App_Materials().find(materialUri).material();
                }
                catch(Materials::NotFoundError const &)
                {}
            }

            // Insert the possibly resolved material into the dictionary.
            materialDict->setUserPointer(internId, material);
        }

        // There is now one more reference.
        refCount++;
        materialDict->setUserValue(internId, refCount);
    }

    // Assign the resolved material if found.
    suf->setMaterial(material);
}

#undef MPE_SidedefCreate
uint MPE_SidedefCreate(short flags, ddstring_t const *topMaterial,
    float topOffsetX, float topOffsetY, float topRed, float topGreen, float topBlue,
    ddstring_t const *middleMaterial, float middleOffsetX, float middleOffsetY, float middleRed,
    float middleGreen, float middleBlue, float middleAlpha, ddstring_t const *bottomMaterial,
    float bottomOffsetX, float bottomOffsetY, float bottomRed, float bottomGreen,
    float bottomBlue)
{
    if(!editMapInited) return 0;

    SideDef *s = createSideDef();
    s->_flags = flags;

    assignSurfaceMaterial(&s->top(), topMaterial);
    s->top().setMaterialOrigin(topOffsetX, topOffsetY);
    s->top().setColorAndAlpha(topRed, topGreen, topBlue, 1);

    assignSurfaceMaterial(&s->middle(), middleMaterial);
    s->middle().setMaterialOrigin(middleOffsetX, middleOffsetY);
    s->middle().setColorAndAlpha(middleRed, middleGreen, middleBlue, middleAlpha);

    assignSurfaceMaterial(&s->bottom(), bottomMaterial);
    s->bottom().setMaterialOrigin(bottomOffsetX, bottomOffsetY);
    s->bottom().setColorAndAlpha(bottomRed, bottomGreen, bottomBlue, 1);

    return s->_buildData.index;
}

#undef MPE_LinedefCreate
uint MPE_LinedefCreate(uint v1, uint v2, uint frontSector, uint backSector,
    uint frontSide, uint backSide, int flags)
{
    if(!editMapInited) return 0;

    if(frontSector > e_map->sectorCount()) return 0;
    if(backSector > e_map->sectorCount()) return 0;
    if(frontSide > e_map->sideDefs.size()) return 0;
    if(backSide > e_map->sideDefs.size()) return 0;
    if(v1 == 0 || v1 > e_map->vertexCount()) return 0;
    if(v2 == 0 || v2 > e_map->vertexCount()) return 0;
    if(v1 == v2) return 0;

    // Ensure that the side indices are unique.
    if(frontSide && e_map->sideDefs[frontSide - 1]->_buildData.refCount)
        return 0;
    if(backSide && e_map->sideDefs[backSide - 1]->_buildData.refCount)
        return 0;

    // Next, check the length is not zero.
    Vertex *vtx1 = e_map->vertexes[v1 - 1];
    Vertex *vtx2 = e_map->vertexes[v2 - 1];
    coord_t length = V2d_Distance(vtx2->origin(), vtx1->origin());
    if(!(length > 0)) return 0;

    SideDef *front = frontSide? e_map->sideDefs[frontSide - 1] : 0;
    SideDef *back  = backSide?  e_map->sideDefs[backSide  - 1] : 0;

    LineDef *l = createLine();
    l->_v[FROM] = vtx1;
    l->_v[TO] = vtx2;

    l->_v[FROM]->_buildData.refCount++;
    l->_v[TO]->_buildData.refCount++;

    l->_sides[FRONT]._sector = (frontSector == 0? NULL: e_map->sectors[frontSector-1]);
    l->_sides[BACK]._sector  = (backSector  == 0? NULL: e_map->sectors[backSector-1]);

    l->_sides[FRONT]._sideDef = front;
    l->_sides[BACK]._sideDef = back;

    l->_length = length;

    l->updateSlopeType();
    l->updateAABox();

    l->_angle = bamsAtan2(int( l->_direction[VY] ),
                          int( l->_direction[VX] ));

    // Remember the number of unique references.
    if(l->hasFrontSideDef())
    {
        l->frontSideDef()._line = l;
        l->frontSideDef()._buildData.refCount++;
    }

    if(l->hasBackSideDef())
    {
        l->backSideDef()._line = l;
        l->backSideDef()._buildData.refCount++;
    }

    l->_inFlags = 0;

    // Determine the default linedef flags.
    l->_flags = flags;
    if(!front || !back)
        l->_flags |= DDLF_BLOCKING;

    return l->_origIndex;
}

#undef MPE_PlaneCreate
uint MPE_PlaneCreate(uint sector, coord_t height, ddstring_t const *materialUri,
    float matOffsetX, float matOffsetY, float r, float g, float b, float a,
    float normalX, float normalY, float normalZ)
{
    if(!editMapInited) return 0;
    if(sector == 0 || sector > e_map->sectorCount()) return 0;

    Sector *s = e_map->sectors[sector - 1];

    vec3f_t normal; V3f_Set(normal, normalX, normalY, normalZ);

    Plane *pln = new Plane(*s, normal, height);

    assignSurfaceMaterial(&pln->surface(), materialUri);
    pln->surface().setColorAndAlpha(r, g, b, a);
    pln->surface().setMaterialOrigin(matOffsetX, matOffsetY);

    s->_planes.append(pln);

    return s->planeCount(); // 1-based index.
}

#undef MPE_SectorCreate
uint MPE_SectorCreate(float lightlevel, float red, float green, float blue)
{
    if(!editMapInited) return 0;

    Sector *s = createSector(Vector3f(red, green, blue), lightlevel);
    return s->origIndex();
}

#undef MPE_PolyobjCreate
uint MPE_PolyobjCreate(uint *lines, uint lineCount, int tag, int sequenceType,
    coord_t originX, coord_t originY)
{
    if(!editMapInited || !lineCount || !lines) return 0;

    // First check that all the line indices are valid and that they arn't
    // already part of another polyobj.
    for(uint i = 0; i < lineCount; ++i)
    {
        if(lines[i] == 0 || lines[i] > e_map->lines.size()) return 0;

        LineDef *line = e_map->lines[lines[i] - 1];
        if(line->isFromPolyobj()) return 0;
    }

    Polyobj *po = createPolyobj();
    po->tag = tag;
    po->seqType = sequenceType;
    po->origin[VX] = originX;
    po->origin[VY] = originY;

    po->lineCount = lineCount;
    po->lines = (LineDef **) M_Calloc(sizeof(*po->lines) * (po->lineCount+1));
    for(uint i = 0; i < lineCount; ++i)
    {
        LineDef *line = e_map->lines[lines[i] - 1];

        // This line belongs to a polyobj.
        line->_inFlags |= LF_POLYOBJ;
        po->lines[i] = line;
    }
    po->lines[lineCount] = NULL;

    return po->buildData.index;
}

#undef MPE_GameObjProperty
boolean MPE_GameObjProperty(char const *entityName, uint elementIndex,
                            char const *propertyName, valuetype_t type, void *valueAdr)
{
    if(!editMapInited) return false;

    if(!entityName || !propertyName || !valueAdr)
        return false; // Hmm...

    // Is this a known entity?
    MapEntityDef *entityDef = P_MapEntityDefByName(entityName);
    if(!entityDef)
    {
        Con_Message("Warning: MPE_GameObjProperty: Unknown entity name:\"%s\", ignoring.",
                    entityName);
        return false;
    }

    // Is this a known property?
    MapEntityPropertyDef *propertyDef;
    if(MapEntityDef_PropertyByName2(entityDef, propertyName, &propertyDef) < 0)
    {
        Con_Message("Warning: MPE_GameObjProperty: Entity \"%s\" has no \"%s\" property, ignoring.",
                    entityName, propertyName);
        return false;
    }

    return P_SetMapEntityProperty(e_map->entityDatabase, propertyDef, elementIndex, type, valueAdr);
}

EditMap::EditMap()
{
    numPolyObjs = 0;
    polyObjs = 0;

    entityDatabase = 0;
}

EditMap::~EditMap()
{}

// p_data.cpp
DENG_EXTERN_C boolean P_RegisterMapObj(int identifier, char const *name);
DENG_EXTERN_C boolean P_RegisterMapObjProperty(int entityId, int propertyId, char const *propertyName, valuetype_t type);

DENG_DECLARE_API(MPE) =
{
    { DE_API_MAP_EDIT },

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
