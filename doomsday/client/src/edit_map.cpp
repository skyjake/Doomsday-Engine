/** @file edit_map.cpp Map Editor interface.
 *
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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
#ifdef __CLIENT__
#  include "render/rend_main.h"
#endif
#include <de/StringPool>
#include <BspBuilder>

#include "audio/s_environ.h"

using namespace de;

/**
 * Editable map data.
 *
 * @todo Now redundant; refactor away. -ds
 */
class EditMap
{
public:
    typedef QList<Vertex *> Vertexes;
    Vertexes vertexes;

    typedef QList<LineDef *> Lines;
    Lines lines;

    typedef QList<SideDef *> SideDefs;
    SideDefs sideDefs;

    typedef QList<Sector *> Sectors;
    Sectors sectors;

    uint numPolyObjs;
    Polyobj **polyObjs;

    /// Map entity data (things, line specials, etc...).
    EntityDatabase *entityDatabase;

public:
    EditMap::EditMap()
    {
        numPolyObjs = 0;
        polyObjs = 0;

        entityDatabase = 0;
    }

    void destroyPolyObjs()
    {
        if(polyObjs)
        {
            for(uint i = 0; i < numPolyObjs; ++i)
            {
                Polyobj *po = polyObjs[i];
                M_Free(po->lines);
                M_Free(po);
            }
            M_Free(polyObjs);
        }
        polyObjs = 0;
        numPolyObjs = 0;
    }

    void clear()
    {
        vertexes.clear();
        lines.clear();
        sideDefs.clear();
        sectors.clear();

        destroyPolyObjs();
    }

    uint vertexCount() const { return vertexes.count(); }
    uint lineCount() const { return lines.count(); }
    uint sideDefCount() const { return sideDefs.count(); }
    uint sectorCount() const { return sectors.count(); }
};

static EditMap editMap; // singleton
static bool editMapInited;
static bool lastBuiltMapResult;

static GameMap *lastBuiltMap;

static int bspFactor = 7;

void MPE_Register()
{
    C_VAR_INT("bsp-factor", &bspFactor, CVF_NO_MAX, 0, 0);
}

/**
 * @defgroup pruneMapElementFlags Prune Map Element Flags
 * Flags for pruneMapElements()
 */
///@{
#define PRUNE_LINES         0x1
#define PRUNE_VERTEXES      0x2
#define PRUNE_SIDEDEFS      0x4
#define PRUNE_SECTORS       0x8
#define PRUNE_ALL           (PRUNE_LINES|PRUNE_VERTEXES|PRUNE_SIDEDEFS|PRUNE_SECTORS)
///@}

/**
 * Material name references specified during map conversion are recorded in
 * this dictionary. A dictionary is used to avoid repeatedly resolving the same
 * URIs and to facilitate a log of missing materials encountered during the
 * process.
 *
 * The pointer user value holds a pointer to the resolved Material (if found).
 * The integer user value tracks the number of times a reference occurs.
 */
static StringPool *materialDict;

/**
 * Either print or count-the-number-of unresolved references in the
 * material dictionary.
 *
 * @param internId    Unique id associated with the reference.
 * @param parameters  If a uint pointer operate in "count" mode (total written here).
 *                    Else operate in "print" mode.
 * @return Always @c 0 (for use as an iterator).
 */
static int printMissingMaterialWorker(StringPool::Id internId, void *parameters)
{
    uint *count = (uint *)parameters;

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
                String const &materialUri = materialDict->string(internId);
                QByteArray materialUriUtf8 = materialUri.toUtf8();
                Con_Message(" %4u x \"%s\"", refCount, materialUriUtf8.constData());
            }
        }
    }
    return 0; // Continue iteration.
}

/**
 * Destroy the missing material dictionary.
 */
static void clearMaterialDict()
{
    // Initialized?
    if(!materialDict) return;

    materialDict->clear();
    delete materialDict; materialDict = 0;
}

/**
 * Print any "missing" materials in the dictionary to the log.
 */
static void printMissingMaterialsInDict()
{
    // Initialized?
    if(!materialDict) return;

    // Count missing materials.
    uint numMissing = 0;
    materialDict->iterate(printMissingMaterialWorker, &numMissing);
    if(!numMissing) return;

    Con_Message("  [110] Warning: Found %u unknown %s:", numMissing, numMissing == 1? "material":"materials");
    // List the missing materials.
    materialDict->iterate(printMissingMaterialWorker, 0);
}

/**
 * Attempt to locate a material by its URI. A dictionary of previously
 * searched-for URIs is maintained to avoid repeated searching and to record
 * "missing" materials.
 *
 * @param materialUriStr  URI of the material to search for.
 *
 * @return  Pointer to the found material; otherwise @c 0.
 */
static Material *findMaterialInDict(ddstring_t const *materialUriStr)
{
    if(!materialUriStr || Str_IsEmpty(materialUriStr)) return 0;

    // Are we yet to instantiate the dictionary?
    if(!materialDict)
    {
        materialDict = new StringPool;
    }

    de::Uri materialUri(Str_Text(materialUriStr), RC_NULL);

    // Intern this reference.
    StringPool::Id internId = materialDict->intern(materialUri.compose());
    Material *material = 0;

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

    return material;
}

static Vertex *createVertex(coord_t x, coord_t y)
{
    Vertex *vtx = new Vertex(x, y);

    editMap.vertexes.append(vtx);
    vtx->_buildData.index = editMap.vertexes.count(); // 1-based index, 0 = NIL.

    return vtx;
}

static LineDef *createLine()
{
    LineDef *line = new LineDef;

    editMap.lines.append(line);
    line->_origIndex = editMap.lines.count(); // 1-based index, 0 = NIL.

    return line;
}

static SideDef *createSideDef()
{
    SideDef *sideDef = new SideDef;

    editMap.sideDefs.append(sideDef);
    sideDef->_buildData.index = editMap.sideDefs.count(); // 1-based index, 0 = NIL.

    return sideDef;
}

static Sector *createSector(Vector3f const &ambientLightColor, float lightLevel)
{
    Sector *sec = new Sector;

    sec->_lightColor[CR] = de::clamp(0.f, ambientLightColor.x, 1.f);
    sec->_lightColor[CG] = de::clamp(0.f, ambientLightColor.y, 1.f);
    sec->_lightColor[CB] = de::clamp(0.f, ambientLightColor.z, 1.f);
    sec->_lightLevel = de::clamp(0.f, lightLevel, 1.f);
    
    editMap.sectors.append(sec);
    sec->_origIndex = editMap.sectors.count(); // 1-based index, 0 = NIL.

    return sec;
}

static Polyobj *createPolyobj()
{
    Polyobj *po = (Polyobj *) M_Calloc(sizeof(*po));

    editMap.polyObjs = (Polyobj **) M_Realloc(editMap.polyObjs, sizeof(po) * (++editMap.numPolyObjs + 1));
    editMap.polyObjs[editMap.numPolyObjs-1] = po;
    editMap.polyObjs[editMap.numPolyObjs] = NULL;

    po->buildData.index = editMap.numPolyObjs; // 1-based index, 0 = NIL.

    return po;
}

#if 0
static int vertexCompare(void const *p1, void const *p2)
{
    Vertex const *a = (Vertex const *) *((void const **) p1);
    Vertex const *b = (Vertex const *) *((void const **) p2);

    if(a == b) return 0;

    if(int(a->origin()[VX]) != int(b->origin()[VX]))
        return int(a->origin()[VX]) - int(b->origin()[VX]);

    return int(a->origin()[VY]) - int(b->origin()[VY]);
}

static void markDuplicateVertexes(EditMap &e_map)
{
    Vertex **hits = (Vertex **) M_Malloc(e_map.vertexCount() * sizeof(Vertex *));

    // Sort array of ptrs.
    for(uint i = 0; i < e_map.vertexCount(); ++i)
    {
        hits[i] = e_map.vertexes[i];
    }
    qsort(hits, e_map.vertexCount(), sizeof(Vertex *), vertexCompare);

    // Now mark them off.
    for(uint i = 0; i < e_map.vertexCount() - 1; ++i)
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
        Vertex *v = map->vertexes[i];

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
        SideDef *s = map->sideDefs[i];

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
    for(uint i = 0; i < map->numSideDefs; ++i)
    {
        SideDef* s = map->sideDefs[i];

        if(s->sector)
            s->sector->buildData.refCount++;
    }

    // Scan all sectors.
    uint newNum = 0;
    for(uint i = 0; i < map->sectorCount(); ++i)
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

/**
 * @param e_map     Editable map to prune elements from.
 * @param flags     @ref pruneMapElementFlags
 */
static void pruneMapElements(EditMap & /*e_map*/, int /*flags*/)
{
    /**
     * @todo Pruning cannot be done as game map data object properties
     * are currently indexed by their original indices as determined by the
     * position in the map data. The same problem occurs within ACS scripts
     * and XG line/sector references.
     *
     * @warning Order here is critical!
     */
    if(!editMapInited) return;

    findEquivalentVertexes(e_map);

    if(flags & PRUNE_LINES)
        pruneLines(e_map);

    if(flags & PRUNE_VERTEXES)
        pruneVertices(e_map);

    if(flags & PRUNE_SIDEDEFS)
        pruneUnusedSidedefs(e_map);

    if(flags & PRUNE_SECTORS)
        pruneUnusedSectors(e_map);
}
#endif

static void buildSectorBspLeafLists(GameMap &map)
{
    foreach(Sector *sector, map.sectors())
    {
        sector->_bspLeafs.clear();

#ifdef DENG2_QT_4_7_OR_NEWER
        uint count = 0;
        for(uint k = 0; k < map.bspLeafCount(); ++k)
        {
            BspLeaf *bspLeaf = map.bspLeafs[k];
            if(bspLeaf->sectorPtr() == sector)
                ++count;
        }

        if(0 == count) continue;

        sector->_bspLeafs.reserve(count);
#endif

        for(uint i = 0; i < map.bspLeafCount(); ++i)
        {
            BspLeaf *bspLeaf = map.bspLeafs[i];
            if(bspLeaf->sectorPtr() == sector)
            {
                // Ownership of the BSP leaf is not given to the sector.
                sector->_bspLeafs.append(bspLeaf);
            }
        }
    }
}

static void buildSectorLineLists(GameMap &map)
{
    LOG_VERBOSE("Building Sector line lists...");

    struct LineLink
    {
        LineDef *line;
        LineLink *next;
    };

    // Collate a list of lines for each sector.
    zblockset_t *lineLinksBlockSet = ZBlockSet_New(sizeof(LineLink), 512, PU_APPSTATIC);
    LineLink **sectorLineLinks = (LineLink **) Z_Calloc(sizeof(*sectorLineLinks) * map.sectorCount(), PU_APPSTATIC, 0);

    foreach(LineDef *line, map.lines())
    {
        if(line->hasFrontSector())
        {
            int const sectorIndex = GameMap_SectorIndex(&map, &line->frontSector());

            LineLink *link = (LineLink *) ZBlockSet_Allocate(lineLinksBlockSet);
            link->line = line;
            link->next = sectorLineLinks[sectorIndex];
            sectorLineLinks[sectorIndex] = link;
        }

        if(line->hasBackSector() && !line->isSelfReferencing())
        {
            int const sectorIndex = GameMap_SectorIndex(&map, &line->backSector());

            LineLink *link = (LineLink *) ZBlockSet_Allocate(lineLinksBlockSet);
            link->line = line;
            link->next = sectorLineLinks[sectorIndex];
            sectorLineLinks[sectorIndex] = link;
        }
    }

    // Build the actual sector line lists.
    foreach(Sector *sector, map.sectors())
    {
        int const sectorIndex = GameMap_SectorIndex(&map, sector);

        sector->_lines.clear();

        if(!sectorLineLinks[sectorIndex]) continue;

#ifdef DENG2_QT_4_7_OR_NEWER
        // Count the total number of lines in this sector.
        uint numLines = 0;
        for(LineLink *link = sectorLineLinks[sectorIndex]; link; link = link->next)
        {
            numLines++;
        }
        // Reserve this much storage.
        sector->_lines.reserve(numLines);
#endif

        /**
         * The behavior of some algorithms used in the DOOM game logic are
         * dependant upon the order of these lists (e.g., EV_DoFloor and
         * EV_BuildStairs). Lets be helpful and use the same order.
         *
         * Sort: Original line index, ascending.
         */
        for(LineLink *link = sectorLineLinks[sectorIndex]; link; link = link->next)
        {
            // Ownership of the line is not given to the sector.
            sector->_lines.prepend(link->line);
        }
    }

    // Free temporary storage.
    ZBlockSet_Delete(lineLinksBlockSet);
    Z_Free(sectorLineLinks);
}

/**
 * @param sector  Sector in which to link @a base.
 * @param base  Mobj base to link in @a sector. Caller should ensure that the
 *              same object is not linked multiple times into the chain.
 */
static void linkToSectorEmitterChain(Sector &sector, ddmobj_base_t &otherEmitter)
{
    // The sector's base is always head of the chain, so link the other after it.
    otherEmitter.thinker.prev = &sector._soundEmitter.thinker;
    otherEmitter.thinker.next = sector._soundEmitter.thinker.next;
    if(otherEmitter.thinker.next)
        otherEmitter.thinker.next->prev = &otherEmitter.thinker;
    sector._soundEmitter.thinker.next = &otherEmitter.thinker;
}

static void finishSectors(GameMap &map)
{
    buildSectorBspLeafLists(map);
    buildSectorLineLists(map);

    foreach(Sector *sector, map.sectors())
    {
        sector->updateAABox();
        sector->updateRoughArea();
        sector->updateSoundEmitterOrigin();

        /*
         * Chain sound emitters (ddmobj_base_t) owned by all Surfaces in the
         * sector-> These chains are used for efficiently traversing all of the
         * sound emitters in a sector (e.g., when stopping all sounds emitted
         * in the sector).
         */
        ddmobj_base_t &emitter = sector->soundEmitter();

        // Clear the head of the emitter chain.
        emitter.thinker.next = emitter.thinker.prev = 0;

        // Link plane surface emitters:
        foreach(Plane *plane, sector->planes())
        {
            linkToSectorEmitterChain(*sector, plane->surface().soundEmitter());
        }

        // Link wall surface emitters:
        foreach(LineDef *line, sector->lines())
        {
            if(line->frontSectorPtr() == sector)
            {
                SideDef &side = line->frontSideDef();
                linkToSectorEmitterChain(*sector, side.middle().soundEmitter());
                linkToSectorEmitterChain(*sector, side.bottom().soundEmitter());
                linkToSectorEmitterChain(*sector, side.top().soundEmitter());
            }
            if(line->hasBackSideDef() && line->backSectorPtr() == sector)
            {
                SideDef &side = line->backSideDef();
                linkToSectorEmitterChain(*sector, side.middle().soundEmitter());
                linkToSectorEmitterChain(*sector, side.bottom().soundEmitter());
                linkToSectorEmitterChain(*sector, side.top().soundEmitter());
            }
        }
    }
}

static void finishSideDefs(GameMap &map)
{
    // Calculate the tangent space surface vectors.
    foreach(SideDef *sideDef, map.sideDefs())
    {
        sideDef->updateSurfaceTangents();
        sideDef->updateSoundEmitterOrigins();
    }
}

static void finishLines(GameMap &map)
{
    foreach(LineDef *line, map.lines())
    {
        LineDef::Side &front = line->front();

        if(!front._leftHEdge) continue;

        line->_v[0] = &front.leftHEdge().v1();
        line->_v[1] = &front.rightHEdge().v2();

        line->updateSlopeType();
        line->updateAABox();

        line->_length = V2d_Length(line->_direction);
        line->_angle = bamsAtan2(int( line->_direction[VY] ),
                                int( line->_direction[VX] ));
    }
}

/// Used when sorting vertex line owners.
static Vertex *rootVtx;

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

#ifdef DENG2_DEBUG
/**
 * Determines whether the specified vertex @a v has a correctly formed line
 * owner ring.
 */
static bool vertexHasValidLineOwnerRing(Vertex &v)
{
    LineOwner const *base = v.firstLineOwner();
    LineOwner const *cur = base;
    do
    {
        if(&cur->prev().next() != cur) return false;
        if(&cur->next().prev() != cur) return false;

    } while((cur = &cur->next()) != base);
    return true;
}
#endif

/**
 * Generates the line owner rings for each vertex. Each ring includes all
 * the lines which the vertex belongs to sorted by angle, (the rings are
 * arranged in clockwise order, east = 0).
 */
static void buildVertexLineOwnerRings()
{
    LOG_AS("buildVertexLineOwnerRings");

    /*
     * Step 1: Find and link up all line owners.
     */
    // We know how many vertex line owners we need (numLineDefs * 2).
    LineOwner *lineOwners = (LineOwner *) Z_Malloc(sizeof(LineOwner) * editMap.lines.count() * 2, PU_MAPSTATIC, 0);
    LineOwner *allocator = lineOwners;

    foreach(LineDef *line, editMap.lines)
    for(uint p = 0; p < 2; ++p)
    {
        setVertexLineOwner(&line->vertex(p), line, &allocator);
    }

    /*
     * Step 2: Sort line owners of each vertex and finalize the rings.
     */
    foreach(Vertex *v, editMap.vertexes)
    {
        if(!v->_numLineOwners) continue;

        // Sort them; ordered clockwise by angle.
        rootVtx = v;
        v->_lineOwners = sortLineOwners(v->_lineOwners, lineAngleSorter);

        // Finish the linking job and convert to relative angles.
        // They are only singly linked atm, we need them to be doubly
        // and circularly linked.
        binangle_t firstAngle = v->_lineOwners->angle();
        LineOwner *last = v->_lineOwners;
        LineOwner *p = &last->next();
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

/*#ifdef DENG2_DEBUG
        LOG_VERBOSE("Vertex #%i: line owners #%i")
            << editMap.vertexes.indexOf(v) << v->lineOwnerCount();

        LineOwner const *base = v->firstLineOwner();
        LineOwner const *cur = base;
        uint idx = 0;
        do
        {
            LOG_VERBOSE("  %i: p= #%05i this= #%05i n= #%05i, dANG= %-3.f")
                << idx << cur->prev().line().origIndex() << cur->line().origIndex()
                << cur->next().line().origIndex() << BANG2DEG(cur->angle());

            idx++;
        } while((cur = &cur->next()) != base);
#endif*/

        // Sanity check.
        DENG2_ASSERT(vertexHasValidLineOwnerRing(*v));
    }
}

static void finishPlanes(GameMap &map)
{
    foreach(Sector *sector, map.sectors())
    foreach(Plane *plane, sector->planes())
    {
        // Set target heights for each plane.
        plane->_targetHeight =
            plane->_oldHeight[0] =
                plane->_oldHeight[1] =
                    plane->_visHeight = plane->_height;

        plane->_visHeightDelta = 0;

        plane->surface().updateSoundEmitterOrigin();

#ifdef __CLIENT__
        // Resize the biassurface lists for the BSP leaf planes.
        foreach(BspLeaf *bspLeaf, sector->bspLeafs())
        {
            uint n = 0;

            biassurface_t **newList = (biassurface_t **) Z_Calloc(sector->planeCount() * sizeof(biassurface_t *), PU_MAP, 0);
            // Copy the existing list?
            if(bspLeaf->_bsuf)
            {
                for(; n < sector->planeCount() - 1 /* exclude newly added */; ++n)
                {
                    newList[n] = bspLeaf->_bsuf[n];
                }
                Z_Free(bspLeaf->_bsuf);
                bspLeaf->_bsuf = 0;
            }

            /*
             * @todo So where is this data initialized now? -ds
             * If we are in map setup mode, don't create the biassurfaces now,
             * as planes are created before the bias system is available.
             */
            /*if(!ddMapSetup)
            {
                biassurface_t *bsuf = SB_CreateSurface();

                bsuf->size = Rend_NumFanVerticesForBspLeaf(bspLeaf);
                bsuf->illum = (vertexillum_t *) Z_Calloc(sizeof(vertexillum_t) * bsuf->size, PU_MAP, 0);

                for(uint k = 0; k < bsuf->size; ++k)
                {
                    SB_InitVertexIllum(&bsuf->illum[k]);
                }

                newList[n] = bsuf;
            }*/

            bspLeaf->_bsuf = newList;
        }
#endif
    }
}

static void hardenPolyobjs(GameMap &dest, EditMap &e_map)
{
    if(e_map.numPolyObjs == 0)
    {
        dest.numPolyObjs = 0;
        dest.polyObjs = NULL;
        return;
    }

    dest.numPolyObjs = e_map.numPolyObjs;
    dest.polyObjs = (Polyobj **) Z_Malloc((dest.numPolyObjs + 1) * sizeof(Polyobj *), PU_MAP, 0);

    for(uint i = 0; i < dest.numPolyObjs; ++i)
    {
        Polyobj *srcP  = e_map.polyObjs[i];
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
            LineDef *line = GameMap_LineDef(&dest, srcP->lines[j]->_origIndex - 1);
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
        destP->lines[destP->lineCount] = 0; // Terminate.

        // Add this polyobj to the global list.
        dest.polyObjs[i] = destP;
    }
    dest.polyObjs[dest.numPolyObjs] = 0; // Terminate.
}

#if 0 /* Currently unused. */
/**
 * @return  The "lowest" vertex (normally the left-most, but if the line is vertical,
 *          then the bottom-most). @c => 0 for start, 1 for end.
 */
static inline int lineVertexLowest(LineDef const *l)
{
    if(int( l->v[0]->buildData.pos[VX] ) < int( l->v[1]->buildData.pos[VX] ))
        return 0;

    return (int( l->v[0]->buildData.pos[VX] ) == int( l->v[1]->buildData.pos[VX] ) &&
            int( l->v[0]->buildData.pos[VY] )  < int( l->v[1]->buildData.pos[VY] )? 0 : 1);
}

static int lineStartCompare(void const *p1, void const *p2)
{
    LineDef const *a = (LineDef const *) p1;
    LineDef const *b = (LineDef const *) p2;

    // Determine left-most vertex of each line.
    Vertex *c = (lineVertexLowest(a)? a->v[1] : a->v[0]);
    Vertex *d = (lineVertexLowest(b)? b->v[1] : b->v[0]);

    if(int( c->buildData.pos[VX] ) != int( d->buildData.pos[VX] ))
        return int( c->buildData.pos[VX] ) - int( d->buildData.pos[VX] );

    return int( c->buildData.pos[VY] ) - int( d->buildData.pos[VY] );
}

static int lineEndCompare(void const *p1, void const *p2)
{
    LineDef const *a = (LineDef const *) p1;
    LineDef const *b = (LineDef const *) p2;

    // Determine right-most vertex of each line.
    Vertex *c = (lineVertexLowest(a)? a->v[0] : a->v[1]);
    Vertex *d = (lineVertexLowest(b)? b->v[0] : b->v[1]);

    if(int( c->buildData.pos[VX] ) != int( d->buildData.pos[VX] ))
        return int( c->buildData.pos[VX] ) - int( d->buildData.pos[VX] );

    return int( c->buildData.pos[VY] ) - int( d->buildData.pos[VY] );
}

size_t numOverlaps;

int testOverlaps(LineDef *b, void *data)
{
    LineDef *a = (LineDef *) data;

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

struct findoverlaps_params_t
{
    GameMap *map;
    uint coords[2]; // Blockmap cell coordinates.
};

static int findOverlapsForLine(LineDef *l, void *parameters)
{
    findoverlaps_params_t *p = (findoverlaps_params_t *) parameters;
    GameMap_IterateCellLineDefs(p->map, p->coords, testOverlaps, l);
    return false; // Continue iteration.
}

/**
 * @note Does not detect partially overlapping lines!
 */
void MPE_DetectOverlappingLines(GameMap *map)
{
    numOverlaps = 0;

    uint bmapDimensions[2];
    Blockmap_Size(map->lineDefBlockmap, bmapDimensions);

    findoverlaps_params_t parms;
    for(uint y = 0; y < bmapSize[VY]; ++y)
    for(uint x = 0; x < bmapSize[VX]; ++x)
    {
        parms.map = map;
        parms.coords[VX] = x;
        parms.coords[VY] = y;

        GameMap_IterateCellLineDefs(map, params.coords, findOverlapsForLine, &parms);
    }

    if(numOverlaps == 0) return;

    VERBOSE( Con_Message("Detected %lu overlapping linedefs.", (unsigned long) numOverlaps) )
}
#endif

/**
 * Find the extremal coordinates for the given set of vertexes.
 *
 * @param vertexes  List of editable vertexes to be scanned.
 * @param min  Minimal coordinates will be written here.
 * @param max  Maximal coordinates will be written here.
 */
static void findBounds(QList<Vertex *> const &vertexes, vec2d_t min, vec2d_t max)
{
    if(!min && !max) return;

    if(!vertexes.count())
    {
        V2d_Set(min, DDMAXFLOAT, DDMAXFLOAT);
        V2d_Set(max, DDMINFLOAT, DDMINFLOAT);
        return;
    }

    vec2d_t bounds[2], point;

    QListIterator<Vertex *> vIt(vertexes);

    // Add the first vertex.
    Vertex *vertex = vIt.next();
    V2d_Set(point, vertex->origin()[VX], vertex->origin()[VY]);
    V2d_InitBox(bounds, point);

    // Add the rest of the vertexes.
    while(vIt.hasNext())
    {
        Vertex *vertex = vIt.next();
        V2d_Set(point, vertex->origin()[VX], vertex->origin()[VY]);
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

static void collateVertexes(BspBuilder &builder, GameMap &map,
    QList<Vertex *> const &editableVertexes)
{
    uint bspVertexCount = builder.numVertexes();

    DENG2_ASSERT(map._vertexes.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    map._vertexes.reserve(editableVertexes.count() + bspVertexCount);
#endif

    uint n = 0;
    foreach(Vertex *vertex, editableVertexes)
    {
        map._vertexes.append(vertex);
        ++n;
    }

    for(uint i = 0; i < bspVertexCount; ++i, ++n)
    {
        Vertex *vertex = &builder.vertex(i);

        builder.take(vertex);
        map._vertexes.append(vertex);
    }
}

static void collateBspLeafHEdges(GameMap &map, BspBuilder &builder, BspLeaf &leaf)
{
    HEdge *base = leaf.firstHEdge();
    HEdge *hedge = base;
    do
    {
        // Take ownership of this HEdge.
        builder.take(hedge);

        // Add this HEdge to the LUT.
        hedge->_origIndex = map.hedgeCount();
        map.hedges.append(hedge);

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

static void collateBspElements(GameMap &map, BspBuilder &builder, BspTreeNode &tree)
{
    if(tree.isLeaf())
    {
        // Take ownership of the built BspLeaf.
        DENG2_ASSERT(tree.userData() != 0);
        BspLeaf *leaf = tree.userData()->castTo<BspLeaf>();
        builder.take(leaf);

        // Add this BspLeaf to the LUT.
        leaf->_index = map.bspLeafs.count();
        map.bspLeafs.append(leaf);

        collateBspLeafHEdges(map, builder, *leaf);

        // The geometry of the leaf is now finalized; update dependent metadata.
        leaf->updateAABox();
        leaf->updateCenter();
        leaf->updateWorldGridOffset();

        return;
    }
    // Else; a node.

    // Take ownership of this BspNode.
    DENG2_ASSERT(tree.userData() != 0);
    BspNode *node = tree.userData()->castTo<BspNode>();
    builder.take(node);

    // Add this BspNode to the LUT.
    node->_index = map.bspNodes.count();
    map.bspNodes.append(node);
}

static bool buildBsp(GameMap &map)
{
    // It begins...
    Time begunAt;

    LOG_INFO("Building BSP using tunable split factor of %d...") << bspFactor;

    // Instantiate and configure a new BSP builder.
    BspBuilder nodeBuilder(map, editMap.vertexes, bspFactor);

    // Build the BSP.
    bool builtOK = nodeBuilder.buildBsp();
    if(builtOK)
    {
        BspTreeNode &treeRoot = *nodeBuilder.root();

        // Determine the max depth of the two main branches.
        dint32 rightBranchDpeth, leftBranchDepth;
        if(!treeRoot.isLeaf())
        {
            rightBranchDpeth = dint32( treeRoot.right().height() );
            leftBranchDepth  = dint32(  treeRoot.left().height() );
        }
        else
        {
            rightBranchDpeth = leftBranchDepth = 0;
        }

        LOG_INFO("BSP built: (%d:%d) %d Nodes, %d Leafs, %d HEdges, %d Vertexes.")
                << rightBranchDpeth << leftBranchDepth
                << nodeBuilder.numNodes()  << nodeBuilder.numLeafs()
                << nodeBuilder.numHEdges() << nodeBuilder.numVertexes();

        /*
         * Take ownership of all the built map data elements.
         */
        DENG2_ASSERT(map.hedges.isEmpty());
        DENG2_ASSERT(map.bspLeafs.isEmpty());
        DENG2_ASSERT(map.bspNodes.isEmpty());

#ifdef DENG2_QT_4_7_OR_NEWER
        map.hedges.reserve(nodeBuilder.numHEdges());
        map.bspNodes.reserve(nodeBuilder.numNodes());
        map.bspLeafs.reserve(nodeBuilder.numLeafs());
#endif

        collateVertexes(nodeBuilder, map, editMap.vertexes);

        BspTreeNode *rootNode = nodeBuilder.root();
        map._bspRoot = rootNode->userData(); // We'll formally take ownership shortly...

        // Iterative pre-order traversal of the BspBuilder's map element tree.
        BspTreeNode *cur = rootNode;
        BspTreeNode *prev = 0;
        while(cur)
        {
            while(cur)
            {
                if(cur->userData())
                {
                    // Acquire ownership of and collate all map data elements at
                    // this node of the tree.
                    collateBspElements(map, nodeBuilder, *cur);
                }

                if(prev == cur->parentPtr())
                {
                    // Descending - right first, then left.
                    prev = cur;
                    if(cur->hasRight()) cur = cur->rightPtr();
                    else                cur = cur->leftPtr();
                }
                else if(prev == cur->rightPtr())
                {
                    // Last moved up the right branch - descend the left.
                    prev = cur;
                    cur = cur->leftPtr();
                }
                else if(prev == cur->leftPtr())
                {
                    // Last moved up the left branch - continue upward.
                    prev = cur;
                    cur = cur->parentPtr();
                }
            }

            if(prev)
            {
                // No left child - back up.
                cur = prev->parentPtr();
            }
        }
    }

    // How much time did we spend?
    LOG_INFO(String("BSP build completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));

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
static void buildReject(GameMap *map)
{
    int *secGroups = M_Malloc(sizeof(int) * numSectors);
    int group = 0;

    for(uint i = 0; i < map->sectorCount(); ++i)
    {
        Sector &sector = map->sectors[i];
        secGroups[i] = group++;
        sector.rejNext = sector.rejPrev = &sector;
    }

    for(uint i = 0; i < map->lineCount(); ++i)
    {
        LineDef *line = &map->lines[i];

        if(!line->hasFrontSideDef() || !line->hasBackSideDef())
            continue;

        Sector *sec1 = line->frontSectorPtr();
        Sector *sec2 = line->backSectorPtr();

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
        Sector *p = 0;
        if(secGroups[sec1->index] > secGroups[sec2->index])
        {
            p = sec1;
            sec1 = sec2;
            sec2 = p;
        }

        // Update the group numbers in the second group
        secGroups[sec2->index] = secGroups[sec1->index];
        for(p = sec2->rejNext; p != sec2; p = p->rejNext)
        {
            secGroups[p->index] = secGroups[sec1->index];
        }

        // Merge 'em baby...
        sec1->rejNext->rejPrev = sec2;
        sec2->rejNext->rejPrev = sec1;

        p = sec1->rejNext;
        sec1->rejNext = sec2->rejNext;
        sec2->rejNext = p;
    }

    size_t rejectSize = (map->sectorCount() * map->sectorCount() + 7) / 8;
    byte *matrix = Z_Calloc(rejectSize, PU_MAPSTATIC, 0);

    for(int view = 0; view < map->sectorCount(); ++view)
    for(int target = 0; target < view; ++target)
    {
        if(secGroups[view] == secGroups[target])
            continue;

        // For symmetry, do two bits at a time.
        int p1 = view * map->sectorCount() + target;
        int p2 = target * map->sectorCount() + view;

        matrix[p1 >> 3] |= (1 << (p1 & 7));
        matrix[p2 >> 3] |= (1 << (p2 & 7));
    }

    M_Free(secGroups);
}
#endif

GameMap *MPE_GetLastBuiltMap()
{
    return lastBuiltMap;
}

bool MPE_GetLastBuiltMapResult()
{
    return lastBuiltMapResult;
}

#undef MPE_Begin
boolean MPE_Begin(char const *mapUri)
{
    /// @todo Do not ignore; assign to the editable map.
    DENG_UNUSED(mapUri);

    // Already been here?
    if(editMapInited) return true;

    // Initialize the game-specific map entity property database.
    editMap.entityDatabase = EntityDatabase_New();
    editMap.clear();

    lastBuiltMap = 0;
    lastBuiltMapResult = false; // Failed (default).

    editMapInited = true;
    return true;
}

#undef MPE_End
boolean MPE_End()
{
    if(!editMapInited)
        return false;

    GameMap *gamemap = new GameMap;

    /*
     * Log warnings about any issues we encountered during conversion of
     * the basic map data elements.
     */
    printMissingMaterialsInDict();
    clearMaterialDict();

    /*
     * Perform cleanup on the loaded map data, removing duplicate vertexes,
     * pruning unused sectors etc, etc...
     */
#if 0
    markDuplicateVertexes(editMap);
    pruneMapElements(editMap, PRUNE_ALL);
#endif

    /*
     * Acquire ownership of the map elements from the editable map.
     */
    gamemap->entityDatabase = editMap.entityDatabase; // Take ownership.

    // Collate sectors:
    DENG2_ASSERT(gamemap->_sectors.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    gamemap->_sectors.reserve(editMap.sectors.count());
#endif
    foreach(Sector *sector, editMap.sectors)
    {
        gamemap->_sectors.append(sector); // Take ownership.
    }

    // Collate sidedefs:
    DENG2_ASSERT(gamemap->_sideDefs.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    gamemap->_sideDefs.reserve(editMap.sideDefs.count());
#endif
    foreach(SideDef *sideDef, editMap.sideDefs)
    {
        gamemap->_sideDefs.append(sideDef); // Take ownership.
    }

    // Collate lines:
    DENG2_ASSERT(gamemap->_lines.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    gamemap->_lines.reserve(editMap.lines.count());
#endif
    foreach(LineDef *line, editMap.lines)
    {
        gamemap->_lines.append(line); // Take ownership.
    }

    buildVertexLineOwnerRings();

    hardenPolyobjs(*gamemap, editMap);
    editMap.destroyPolyObjs();

    /*
     * Build blockmaps.
     */
    vec2d_t min, max;
    findBounds(editMap.vertexes, min, max);

    GameMap_InitLineDefBlockmap(gamemap, min, max);
    for(int i = 0; i < gamemap->_lines.count(); ++i)
    {
        GameMap_LinkLineDef(gamemap, gamemap->_lines[i]);
    }

    // Mobj and Polyobj blockmaps are maintained dynamically.
    GameMap_InitMobjBlockmap(gamemap, min, max);
    GameMap_InitPolyobjBlockmap(gamemap, min, max);

    /*
     * Build a BSP for this map.
     */
    bool builtOK = buildBsp(*gamemap);

    // Finish the polyobjs (after the vertexes are hardened).
    for(uint i = 0; i < gamemap->numPolyObjs; ++i)
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

    // Destroy the rest of editable map, we are finished with it.
    editMap.clear();

    if(!builtOK)
    {
        // Darn, clean up...
        EntityDatabase_Delete(gamemap->entityDatabase);
        delete gamemap;

        lastBuiltMapResult = false; // Failed :$
        return lastBuiltMapResult;
    }

    editMapInited = false;

    finishSideDefs(*gamemap);
    finishLines(*gamemap);
    finishSectors(*gamemap);
    finishPlanes(*gamemap);

    gamemap->updateBounds();

    S_DetermineBspLeafsAffectingSectorReverb(gamemap);

    // Call the game's setup routines.
    if(gx.SetupForMapData)
    {
        gx.SetupForMapData(DMU_VERTEX,  gamemap->vertexCount());
        gx.SetupForMapData(DMU_LINEDEF, gamemap->lineCount());
        gx.SetupForMapData(DMU_SIDEDEF, gamemap->sideDefCount());
        gx.SetupForMapData(DMU_SECTOR,  gamemap->sectorCount());
    }

    // Are we caching this map?
    if(gamemap->uri && !Str_IsEmpty(Uri_Path(gamemap->uri)))
    {
        // Yes, write the cached map data file.
        char const *markerLumpName = Str_Text(Uri_Path(gamemap->uri));
        lumpnum_t markerLumpNum = F_LumpNumForName(markerLumpName);
        AutoStr *cachedMapDir = DAM_ComposeCacheDir(Str_Text(F_ComposeLumpFilePath(markerLumpNum)));

        ddstring_t cachedMapPath; Str_InitStd(&cachedMapPath);
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

#undef MPE_SidedefCreate
uint MPE_SidedefCreate(short flags, ddstring_t const *topMaterialUri,
    float topOffsetX, float topOffsetY, float topRed, float topGreen, float topBlue,
    ddstring_t const *middleMaterialUri, float middleOffsetX, float middleOffsetY, float middleRed,
    float middleGreen, float middleBlue, float middleAlpha, ddstring_t const *bottomMaterialUri,
    float bottomOffsetX, float bottomOffsetY, float bottomRed, float bottomGreen,
    float bottomBlue)
{
    if(!editMapInited) return 0;

    SideDef *s = createSideDef();
    s->_flags = flags;

    // Assign the resolved material if found.
    s->top().setMaterial(findMaterialInDict(topMaterialUri));
    s->top().setMaterialOrigin(topOffsetX, topOffsetY);
    s->top().setColorAndAlpha(topRed, topGreen, topBlue, 1);

    s->middle().setMaterial(findMaterialInDict(middleMaterialUri));
    s->middle().setMaterialOrigin(middleOffsetX, middleOffsetY);
    s->middle().setColorAndAlpha(middleRed, middleGreen, middleBlue, middleAlpha);

    s->bottom().setMaterial(findMaterialInDict(bottomMaterialUri));
    s->bottom().setMaterialOrigin(bottomOffsetX, bottomOffsetY);
    s->bottom().setColorAndAlpha(bottomRed, bottomGreen, bottomBlue, 1);

    return s->_buildData.index;
}

#undef MPE_LinedefCreate
uint MPE_LinedefCreate(uint v1, uint v2, uint frontSector, uint backSector,
    uint frontSide, uint backSide, int flags)
{
    if(!editMapInited) return 0;

    if(frontSector > editMap.sectorCount()) return 0;
    if(backSector > editMap.sectorCount()) return 0;
    if(frontSide > editMap.sideDefCount()) return 0;
    if(backSide > editMap.sideDefCount()) return 0;
    if(v1 == 0 || v1 > editMap.vertexCount()) return 0;
    if(v2 == 0 || v2 > editMap.vertexCount()) return 0;
    if(v1 == v2) return 0;

    // Ensure that the side indices are unique.
    if(frontSide && editMap.sideDefs[frontSide - 1]->_buildData.refCount)
        return 0;
    if(backSide && editMap.sideDefs[backSide - 1]->_buildData.refCount)
        return 0;

    // Next, check the length is not zero.
    Vertex *vtx1 = editMap.vertexes[v1 - 1];
    Vertex *vtx2 = editMap.vertexes[v2 - 1];
    coord_t length = V2d_Distance(vtx2->origin(), vtx1->origin());
    if(!(length > 0)) return 0;

    SideDef *front = frontSide? editMap.sideDefs[frontSide - 1] : 0;
    SideDef *back  = backSide?  editMap.sideDefs[backSide  - 1] : 0;

    LineDef *l = createLine();
    l->_v[FROM] = vtx1;
    l->_v[TO] = vtx2;

    l->_v[FROM]->_buildData.refCount++;
    l->_v[TO]->_buildData.refCount++;

    l->_sides[FRONT]._sector = (frontSector == 0? NULL: editMap.sectors[frontSector-1]);
    l->_sides[BACK]._sector  = (backSector  == 0? NULL: editMap.sectors[backSector-1]);

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
    if(sector == 0 || sector > editMap.sectorCount()) return 0;

    Sector *s = editMap.sectors[sector - 1];

    vec3f_t normal; V3f_Set(normal, normalX, normalY, normalZ);

    Plane *pln = new Plane(*s, normal, height);

    pln->surface().setMaterial(findMaterialInDict(materialUri));
    pln->surface().setColorAndAlpha(r, g, b, a);
    pln->surface().setMaterialOrigin(matOffsetX, matOffsetY);

    s->_planes.append(pln);
    pln->_inSectorIndex = s->planeCount() - 1;

    return pln->inSectorIndex() + 1; // 1-based index.
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
        if(lines[i] == 0 || lines[i] > editMap.lineCount()) return 0;

        LineDef *line = editMap.lines[lines[i] - 1];
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
        LineDef *line = editMap.lines[lines[i] - 1];

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

    return P_SetMapEntityProperty(editMap.entityDatabase, propertyDef, elementIndex, type, valueAdr);
}

// p_data.cpp
#undef P_RegisterMapObj
DENG_EXTERN_C boolean P_RegisterMapObj(int identifier, char const *name);

#undef P_RegisterMapObjProperty
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
