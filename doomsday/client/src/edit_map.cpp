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

#include <QList>
#include <QVector>
#include <QtAlgorithms>

#include <de/StringPool>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_misc.h"
#include "de_edit.h"
#include "de_dam.h"
#include "de_filesys.h"

#include "audio/s_environ.h"
#include "map/lineowner.h"
#include "map/gamemap.h"
#ifdef __CLIENT__
#  include "render/rend_main.h"
#endif

#include "edit_map.h"

using namespace de;

/**
 * Editable map data elements.
 *
 * @todo Move into GameMap.
 */
class EditableMap
{
public:
    GameMap::Vertexes vertexes;
    GameMap::Lines lines;
    GameMap::Sectors sectors;
    GameMap::Polyobjs polyobjs;

    /// Map entity data (things, line specials, etc...).
    EntityDatabase *entityDatabase;

    EditableMap()
        : entityDatabase(0)
    {}

    Vertex *createVertex(Vector2d const &origin,
                         int archiveIndex = MapElement::NoIndex)
    {
        Vertex *vtx = new Vertex(origin);

        vtx->setIndexInArchive(archiveIndex);
        vtx->setIndexInMap(vertexes.count());
        vertexes.append(vtx);

        return vtx;
    }

    Line *createLine(Vertex &v1, Vertex &v2, int flags = 0,
                     Sector *frontSector = 0, Sector *backSector = 0,
                     int archiveIndex = MapElement::NoIndex)
    {
        Line *line = new Line(v1, v2, flags, frontSector, backSector);

        line->setIndexInArchive(archiveIndex);
        line->setIndexInMap(lines.count());
        lines.append(line);

        line->front().setIndexInMap(GameMap::toSideIndex(line->indexInMap(), Line::Front));
        line->back().setIndexInMap(GameMap::toSideIndex(line->indexInMap(), Line::Back));

        return line;
    }

    Sector *createSector(float lightLevel, Vector3f const &lightColor,
                         int archiveIndex = MapElement::NoIndex)
    {
        Sector *sector = new Sector(lightLevel, lightColor);

        sector->setIndexInArchive(archiveIndex);
        sector->setIndexInMap(sectors.count());
        sectors.append(sector);

        return sector;
    }

    Polyobj *createPolyobj(Vector2d const &origin)
    {
        void *region = M_Calloc(POLYOBJ_SIZE);
        Polyobj *po = new (region) Polyobj(origin);

        po->setIndexInMap(polyobjs.count());
        polyobjs.append(po);

        return po;
    }

    void clear()
    {
        qDeleteAll(vertexes);
        vertexes.clear();

        qDeleteAll(lines);
        lines.clear();

        qDeleteAll(sectors);
        sectors.clear();

        foreach(Polyobj *po, polyobjs)
        {
            po->~Polyobj();
            M_Free(po);
        }
        polyobjs.clear();
    }

    struct VertexInfo
    {
        /// Vertex for this info.
        Vertex *vertex;

        /// Determined equivalent vertex.
        Vertex *equiv;

        /// Line -> Vertex reference count.
        uint refCount;

        VertexInfo()
            : vertex(0), equiv(0), refCount(0)
        {}

        /// @todo Math here is not correct (rounding directionality). -ds
        int compareVertexOrigins(VertexInfo const &other) const
        {
            DENG_ASSERT(vertex != 0 && other.vertex != 0);

            if(this == &other) return 0;
            if(vertex == other.vertex) return 0;

            // Order is firstly X axis major.
            if(int(vertex->origin().x) != int(other.vertex->origin().x))
                return int(vertex->origin().x) - int(other.vertex->origin().x);

            // Order is secondly Y axis major.
            return int(vertex->origin().y) - int(other.vertex->origin().y);
        }

        bool operator < (VertexInfo const &other) const {
            return compareVertexOrigins(other) < 0;
        }
    };

    void pruneVertexes()
    {
        /*
         * Step 1 - Find equivalent vertexes:
         */

        // Populate the vertex info.
        QVector<VertexInfo> vertexInfo(vertexes.count());
        for(int i = 0; i < vertexes.count(); ++i)
            vertexInfo[i].vertex = vertexes[i];

        {
            // Sort a copy to place near vertexes adjacently.
            QVector<VertexInfo> sortedInfo(vertexInfo);
            qSort(sortedInfo.begin(), sortedInfo.end());

            // Locate equivalent vertexes in the sorted info.
            for(int i = 0; i < sortedInfo.count() - 1; ++i)
            {
                VertexInfo &a = sortedInfo[i];
                VertexInfo &b = sortedInfo[i + 1];

                // Are these equivalent?
                /// @todo fixme: What about polyobjs? They need unique vertexes! -ds
                if(a.compareVertexOrigins(b) == 0)
                {
                    b.equiv = (a.equiv? a.equiv : a.vertex);
                }
            }
        }

        /*
         * Step 2 - Replace line references to equivalent vertexes:
         */

        // Count line -> vertex references.
        foreach(Line *line, lines)
        {
            vertexInfo[line->from().indexInMap()].refCount++;
            vertexInfo[  line->to().indexInMap()].refCount++;
        }

        // Perform the replacement.
        foreach(Line *line, lines)
        {
            while(vertexInfo[line->from().indexInMap()].equiv)
            {
                VertexInfo &info = vertexInfo[line->from().indexInMap()];

                info.refCount--;
                line->replaceFrom(*info.equiv);

                vertexInfo[line->from().indexInMap()].refCount++;
            }

            while(vertexInfo[line->to().indexInMap()].equiv)
            {
                VertexInfo &info = vertexInfo[line->to().indexInMap()];

                info.refCount--;
                line->replaceTo(*info.equiv);

                vertexInfo[line->to().indexInMap()].refCount++;
            }
        }

        /*
         * Step 3 - Prune vertexes:
         */
        uint prunedCount = 0, numUnused = 0;
        foreach(VertexInfo const &info, vertexInfo)
        {
            Vertex *vertex = info.vertex;

            if(info.refCount)
                continue;

            vertexes.removeOne(vertex);
            delete vertex;

            prunedCount += 1;
            if(!info.equiv) numUnused += 1;
        }

        if(prunedCount)
        {
            // Re-index with a contiguous range of indices.
            uint idx = 0;
            foreach(Vertex *vertex, vertexes)
                vertex->setIndexInMap(idx++);

            /// Update lines. @todo Line should handle this itself.
            foreach(Line *line, lines)
            {
                line->updateSlopeType();
                line->updateAABox();
            }

            LOG_INFO("Pruned %d vertexes (%d equivalents, %d unused).")
                << prunedCount << (prunedCount - numUnused) << numUnused;
        }
    }
};

static EditableMap editMap; // singleton

static bool editMapInited;
static bool lastBuiltMapResult;

static GameMap *lastBuiltMap;

int bspFactor = 7;

void MPE_Register()
{
    C_VAR_INT("bsp-factor", &bspFactor, CVF_NO_MAX, 0, 0);
}

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
        if(own[i]->_link[Anticlockwise]) // We have a cached result.
        {
            angles[i] = own[i]->angle();
        }
        else
        {
            Line *line = &own[i]->line();
            Vertex const &otherVtx = line->vertex(&line->from() == rootVtx? 1:0);

            fixed_t dx = otherVtx.origin().x - rootVtx->origin().x;
            fixed_t dy = otherVtx.origin().y - rootVtx->origin().y;

            own[i]->_angle = angles[i] = bamsAtan2(-100 *dx, 100 * dy);

            // Mark as having a cached angle.
            own[i]->_link[Anticlockwise] = (LineOwner *) 1;
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

    tmp._link[Clockwise] = np;
    while(left && right)
    {
        if(compare(left, right) <= 0)
        {
            np->_link[Clockwise] = left;
            np = left;

            left = &left->next();
        }
        else
        {
            np->_link[Clockwise] = right;
            np = right;

            right = &right->next();
        }
    }

    // At least one of these lists is now empty.
    if(left)
    {
        np->_link[Clockwise] = left;
    }
    if(right)
    {
        np->_link[Clockwise] = right;
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

    listc->_link[Clockwise] = NULL;
    return listb;
}

/**
 * This routine uses a recursive mergesort algorithm; O(NlogN)
 */
static LineOwner *sortLineOwners(LineOwner *list,
    int (*compare) (void const *a, void const *b))
{
    if(list && list->_link[Clockwise])
    {
        LineOwner *p = splitLineOwners(list);

        // Sort both halves and merge them back.
        list = mergeLineOwners(sortLineOwners(list, compare),
                               sortLineOwners(p, compare), compare);
    }
    return list;
}

static void setVertexLineOwner(Vertex *vtx, Line *lineptr, LineOwner **storage)
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
    newOwner->_link[Anticlockwise] = NULL;

    // Link it in.
    // NOTE: We don't bother linking everything at this stage since we'll
    // be sorting the lists anyway. After which we'll finish the job by
    // setting the prev and circular links.
    // So, for now this is only linked singlely, forward.
    newOwner->_link[Clockwise] = vtx->_lineOwners;
    vtx->_lineOwners = newOwner;

    // Link the line to its respective owner node.
    if(vtx == &lineptr->from())
        lineptr->_vo1 = newOwner;
    else
        lineptr->_vo2 = newOwner;
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
    // We know how many vertex line owners we need (numLines * 2).
    LineOwner *lineOwners = (LineOwner *) Z_Malloc(sizeof(LineOwner) * editMap.lines.count() * 2, PU_MAPSTATIC, 0);
    LineOwner *allocator = lineOwners;

    foreach(Line *line, editMap.lines)
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
            p->_link[Anticlockwise] = last;

            // Convert to a relative angle between last and this.
            last->_angle = last->angle() - p->angle();

            last = p;
            p = &p->next();
        }
        last->_link[Clockwise] = v->_lineOwners;
        v->_lineOwners->_link[Anticlockwise] = last;

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
                << idx << cur->prev().line().indexInMap() << cur->line().indexInMap()
                << cur->next().line().indexInMap() << BANG2DEG(cur->angle());

            idx++;
        } while((cur = &cur->next()) != base);
#endif*/

        // Sanity check.
        DENG2_ASSERT(vertexHasValidLineOwnerRing(*v));
    }
}

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

    LOG_DEBUG("New Elements: %d Vertexes, %d Lines, %d Sectors")
        << editMap.vertexes.count() << editMap.lines.count() << editMap.sectors.count();

    /*
     * Log warnings about any issues we encountered during conversion of
     * the basic map data elements.
     */
    printMissingMaterialsInDict();
    clearMaterialDict();

    GameMap *map = new GameMap;

    /*
     * Perform cleanup on the loaded map data.
     */

    editMap.pruneVertexes();

    /// Ensure lines with only one sector are flagged as blocking.
    /// @todo Refactor away.
    foreach(Line *line, editMap.lines)
    {
        if(!line->hasFrontSector() || !line->hasBackSector())
            line->setFlags(DDLF_BLOCKING);
    }

    buildVertexLineOwnerRings();

    /*
     * Acquire ownership of the map elements from the editable map.
     */
    map->entityDatabase = editMap.entityDatabase; // Take ownership.
    editMap.entityDatabase = 0;

    DENG2_ASSERT(map->_vertexes.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    map->_vertexes.reserve(editMap.vertexes.count());
#endif
    while(!editMap.vertexes.isEmpty())
    {
        map->_vertexes.append(editMap.vertexes.takeFirst());
    }

    // Collate sectors:
    DENG2_ASSERT(map->_sectors.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    map->_sectors.reserve(editMap.sectors.count());
#endif
    while(!editMap.sectors.isEmpty())
    {
        map->_sectors.append(editMap.sectors.takeFirst());
    }

    // Collate lines:
    DENG2_ASSERT(map->_lines.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    map->_lines.reserve(editMap.lines.count());
#endif
    while(!editMap.lines.isEmpty())
    {
        map->_lines.append(editMap.lines.takeFirst());
        map->_lines.back();
    }

    // Collate polyobjs:
    DENG2_ASSERT(map->_polyobjs.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    map->_polyobjs.reserve(editMap.polyobjs.count());
#endif
    while(!editMap.polyobjs.isEmpty())
    {
        map->_polyobjs.append(editMap.polyobjs.takeFirst());
        Polyobj *polyobj = map->_polyobjs.back();

        // Create a segment for each line of this polyobj.
        foreach(Line *line, polyobj->lines())
        {
            HEdge *hedge = polyobj->mesh().newHEdge(line->from());

            hedge->setTwin(polyobj->mesh().newHEdge(line->to()));
            hedge->twin().setTwin(hedge);

            // Polyobj has ownership of the line segments.
            Segment *segment = new Segment(&line->front(), hedge);
            segment->setLength(line->length());

            line->front().setLeftSegment(segment);
            line->front().setRightSegment(segment);
        }

        polyobj->buildUniqueVertexes();
        polyobj->updateOriginalVertexCoords();
    }

    map->updateBounds();
    AABoxd const &mapBounds = map->bounds();
    LOG_INFO("Map bounds: min%s max%s")
        << Vector2d(mapBounds.min).asText()
        << Vector2d(mapBounds.max).asText();

    /*
     * Build blockmaps.
     */
    vec2d_t min, max;
    map->bounds(min, max);

    map->initLineBlockmap(min, max);
    foreach(Line *line, map->lines())
    {
        map->linkLine(*line);
    }

    // The mobj and polyobj blockmaps are maintained dynamically.
    map->initMobjBlockmap(min, max);
    map->initPolyobjBlockmap(min, max);

    /*
     * Build a BSP.
     */
    bool builtOK = map->buildBsp();

    // Destroy the rest of editable map, we are finished with it.
    editMap.clear();

    if(!builtOK)
    {
        // Darn, clean up...
        delete map;

        lastBuiltMapResult = false; // Failed :$
        return lastBuiltMapResult;
    }

    editMapInited = false;

    map->finishMapElements();

    // We can now initialize the BSP leaf blockmap.
    map->initBspLeafBlockmap(min, max);
    foreach(BspLeaf *bspLeaf, map->bspLeafs())
    {
        map->linkBspLeaf(*bspLeaf);
    }

#ifdef __CLIENT__
    S_DetermineBspLeafsAffectingSectorReverb(map);
#endif

    lastBuiltMap = map;
    lastBuiltMapResult = true; // Success.

    return lastBuiltMapResult;
}

#undef MPE_VertexCreate
int MPE_VertexCreate(coord_t x, coord_t y, int archiveIndex)
{
    if(!editMapInited) return -1;
    return editMap.createVertex(Vector2d(x, y), archiveIndex)->indexInMap();
}

#undef MPE_VertexCreatev
boolean MPE_VertexCreatev(size_t num, coord_t *values, int *archiveIndices, int *retIndices)
{
    if(!editMapInited || !num || !values)
        return false;

    // Create many vertexes.
    for(size_t n = 0; n < num; ++n)
    {
        Vertex *vertex = editMap.createVertex(Vector2d(values[n * 2], values[n * 2 + 1]),
                                              archiveIndices[n]);
        if(retIndices)
        {
            retIndices[n] = vertex->indexInMap();
        }
    }

    return true;
}

#undef MPE_LineCreate
int MPE_LineCreate(int v1, int v2, int frontSectorIdx, int backSectorIdx, int flags,
                   int archiveIndex)
{
    if(!editMapInited) return -1;

    if(frontSectorIdx >= editMap.sectors.count()) return -1;
    if(backSectorIdx  >= editMap.sectors.count()) return -1;
    if(v1 < 0 || v1 >= editMap.vertexes.count()) return -1;
    if(v2 < 0 || v2 >= editMap.vertexes.count()) return -1;
    if(v1 == v2) return -1;

    // Next, check the length is not zero.
    /// @todo fixme: We need to allow these... -ds
    Vertex *vtx1 = editMap.vertexes[v1];
    Vertex *vtx2 = editMap.vertexes[v2];
    if(de::abs(Vector2d(vtx1->origin() - vtx2->origin()).length()) <= 0.0001) return -1;

    Sector *frontSector = (frontSectorIdx >= 0? editMap.sectors[frontSectorIdx] : 0);
    Sector *backSector  = (backSectorIdx  >= 0? editMap.sectors[ backSectorIdx] : 0);

    return editMap.createLine(*vtx1, *vtx2, flags, frontSector, backSector,
                              archiveIndex)->indexInMap();
}

#undef MPE_LineAddSide
void MPE_LineAddSide(int lineIdx, int sideId, short flags, ddstring_t const *topMaterialUri,
    float topOffsetX, float topOffsetY, float topRed, float topGreen, float topBlue,
    ddstring_t const *middleMaterialUri, float middleOffsetX, float middleOffsetY, float middleRed,
    float middleGreen, float middleBlue, float middleAlpha, ddstring_t const *bottomMaterialUri,
    float bottomOffsetX, float bottomOffsetY, float bottomRed, float bottomGreen,
    float bottomBlue, int archiveIndex)
{
    if(!editMapInited) return;

    if(lineIdx < 0 || lineIdx >= editMap.lines.count()) return;

    Line *line = editMap.lines[lineIdx];
    Line::Side &side = line->side(sideId);

    side.setFlags(flags);
    side.setIndexInArchive(archiveIndex);

    // Ensure sections are defined if they aren't already.
    side.addSections();

    // Assign the resolved material if found.
    side.top().setMaterial(findMaterialInDict(topMaterialUri));
    side.top().setMaterialOrigin(topOffsetX, topOffsetY);
    side.top().setTintColor(topRed, topGreen, topBlue);

    side.middle().setMaterial(findMaterialInDict(middleMaterialUri));
    side.middle().setMaterialOrigin(middleOffsetX, middleOffsetY);
    side.middle().setTintColor(middleRed, middleGreen, middleBlue);
    side.middle().setOpacity(middleAlpha);

    side.bottom().setMaterial(findMaterialInDict(bottomMaterialUri));
    side.bottom().setMaterialOrigin(bottomOffsetX, bottomOffsetY);
    side.bottom().setTintColor(bottomRed, bottomGreen, bottomBlue);
}

#undef MPE_PlaneCreate
int MPE_PlaneCreate(int sectorIdx, coord_t height, ddstring_t const *materialUri,
    float matOffsetX, float matOffsetY, float tintRed, float tintGreen, float tintBlue, float opacity,
    float normalX, float normalY, float normalZ, int archiveIndex)
{
    if(!editMapInited) return -1;

    if(sectorIdx < 0 || sectorIdx >= editMap.sectors.count()) return -1;

    Sector *sector = editMap.sectors[sectorIdx];
    Plane *plane = sector->addPlane(Vector3f(normalX, normalY, normalZ), height);

    plane->setIndexInArchive(archiveIndex);

    plane->surface().setMaterial(findMaterialInDict(materialUri));
    plane->surface().setTintColor(tintRed, tintGreen, tintBlue);
    plane->surface().setMaterialOrigin(matOffsetX, matOffsetY);

    if(!plane->isSectorFloor() && !plane->isSectorCeiling())
    {
        plane->surface().setOpacity(opacity);
    }

    return plane->inSectorIndex();
}

#undef MPE_SectorCreate
int MPE_SectorCreate(float lightlevel, float red, float green, float blue,
                     int archiveIndex)
{
    if(!editMapInited) return -1;
    return editMap.createSector(lightlevel, Vector3f(red, green, blue),
                                archiveIndex)->indexInMap();
}

#undef MPE_PolyobjCreate
int MPE_PolyobjCreate(int *lines, int lineCount, int tag, int sequenceType,
    coord_t originX, coord_t originY, int archiveIndex)
{
    DENG_UNUSED(archiveIndex); /// @todo Use this!

    if(!editMapInited) return -1;
    if(lineCount <= 0 || !lines) return -1;

    // First check that all the line indices are valid and that they arn't
    // already part of another polyobj.
    for(int i = 0; i < lineCount; ++i)
    {
        if(lines[i] < 0 || lines[i] >= editMap.lines.count()) return -1;

        Line *line = editMap.lines[lines[i]];
        if(line->definesPolyobj()) return -1;
    }

    Polyobj *po = editMap.createPolyobj(Vector2d(originX, originY));
    po->setSequenceType(sequenceType);
    po->setTag(tag);

    for(int i = 0; i < lineCount; ++i)
    {
        Line *line = editMap.lines[lines[i]];

        // This line belongs to a polyobj.
        line->setPolyobj(po);
        static_cast<Polyobj::Lines *>(po->_lines)->append(line);
    }

    return po->indexInMap();
}

#undef MPE_GameObjProperty
boolean MPE_GameObjProperty(char const *entityName, int elementIndex,
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
    if(MapEntityDef_PropertyByName(entityDef, propertyName, &propertyDef) < 0)
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
    MPE_LineCreate,
    MPE_LineAddSide,
    MPE_SectorCreate,
    MPE_PlaneCreate,
    MPE_PolyobjCreate,
    MPE_GameObjProperty
};
