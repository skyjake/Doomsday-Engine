/** @file gamemap.cpp World Map.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <cmath>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_system.h"

#include <BspBuilder>
#include "edit_map.h"
#include "map/blockmap.h"
#include "map/generators.h"
#include "render/r_main.h" // validCount
#include "r_util.h"

#include "map/gamemap.h"

/// Size of Blockmap blocks in map units. Must be an integer power of two.
#define MAPBLOCKUNITS               (128)

/// $smoothmatoffset: Maximum speed for a smoothed material offset.
#define MAX_SMOOTH_MATERIAL_MOVE    (8)

using namespace de;

DENG2_PIMPL(GameMap)
{
    /// Boundary points which encompass the entire map.
    AABoxd bounds;

    /// BSP root element.
    MapElement *bspRoot;

    /// BSP element LUTs:
    HEdges hedges;
    BspNodes bspNodes;
    BspLeafs bspLeafs;

    struct generators_s *generators;

    PlaneSet trackedPlanes;
    SurfaceSet scrollingSurfaces;
#ifdef __CLIENT__
    SurfaceSet decoratedSurfaces;
    SurfaceSet glowingSurfaces;
#endif

    skyfix_t skyFix[2]; // [floor, ceiling]

    // Current LOS trace state.
    /// @todo Does not belong here.
    TraceOpening traceOpening;
    divline_t traceLine;

    Instance(Public *i)
        : Base(i),
          bspRoot(0),
          generators(0)
    {
        std::memset(&bounds,       0, sizeof(bounds));
        std::memset(skyFix,        0, sizeof(skyFix));
        std::memset(&traceOpening, 0, sizeof(traceOpening));
        std::memset(&traceLine,    0, sizeof(traceLine));
    }

    void collateVertexes(BspBuilder &builder,
                         QList<Vertex *> const &editableVertexes)
    {
        uint bspVertexCount = builder.numVertexes();

        DENG2_ASSERT(self._vertexes.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
        self._vertexes.reserve(editableVertexes.count() + bspVertexCount);
#endif

        uint n = 0;
        foreach(Vertex *vertex, editableVertexes)
        {
            self._vertexes.append(vertex);
            ++n;
        }

        for(uint i = 0; i < bspVertexCount; ++i, ++n)
        {
            Vertex *vertex = &builder.vertex(i);

            builder.take(vertex);
            self._vertexes.append(vertex);
        }
    }

    void collateBspLeafHEdges(BspBuilder &builder, BspLeaf &leaf)
    {
        HEdge *base = leaf.firstHEdge();
        HEdge *hedge = base;
        do
        {
            // Take ownership of this HEdge.
            builder.take(hedge);

            // Add this HEdge to the LUT.
            hedge->_origIndex = hedges.count();
            hedges.append(hedge);

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

    void collateBspElements(BspBuilder &builder, BspTreeNode &tree)
    {
        if(tree.isLeaf())
        {
            // Take ownership of the built BspLeaf.
            DENG2_ASSERT(tree.userData() != 0);
            BspLeaf *leaf = tree.userData()->castTo<BspLeaf>();
            builder.take(leaf);

            // Add this BspLeaf to the LUT.
            leaf->_index = bspLeafs.count();
            bspLeafs.append(leaf);

            collateBspLeafHEdges(builder, *leaf);

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
        node->_index = bspNodes.count();
        bspNodes.append(node);
    }

#ifdef __CLIENT__

    void addSurfaceToLists(Surface &suf)
    {
        if(!suf.hasMaterial()) return;

        if(suf.material().hasGlow())
        {
            glowingSurfaces.insert(&suf);
        }

        if(suf.material().isDecorated())
        {
            decoratedSurfaces.insert(&suf);
        }
    }

#endif // __CLIENT__

    void updateMapSkyFixForSector(Sector &sector)
    {
        if(!sector.lineCount()) return;

        bool skyFloor = sector.floorSurface().hasSkyMaskedMaterial();
        bool skyCeil  = sector.ceilingSurface().hasSkyMaskedMaterial();

        if(!skyFloor && !skyCeil) return;

        if(skyCeil)
        {
            // Adjust for the plane height.
            if(sector.ceiling().visHeight() > self.skyFixCeiling())
            {
                // Must raise the skyfix ceiling.
                self.setSkyFixCeiling(sector.ceiling().visHeight());
            }

            // Check that all the mobjs in the sector fit in.
            for(mobj_t *mo = sector.firstMobj(); mo; mo = mo->sNext)
            {
                coord_t extent = mo->origin[VZ] + mo->height;

                if(extent > self.skyFixCeiling())
                {
                    // Must raise the skyfix ceiling.
                    self.setSkyFixCeiling(extent);
                }
            }
        }

        if(skyFloor)
        {
            // Adjust for the plane height.
            if(sector.floor().visHeight() < self.skyFixFloor())
            {
                // Must lower the skyfix floor.
                self.setSkyFixFloor(sector.floor().visHeight());
            }
        }

        // Update for middle textures on two sided lines which intersect the
        // floor and/or ceiling of their front and/or back sectors.
        foreach(LineDef *line, sector.lines())
        {
            // Must be twosided.
            if(!line->hasFrontSideDef() || !line->hasBackSideDef())
                continue;

            int side = line->frontSectorPtr() == &sector? FRONT : BACK;
            SideDef const &sideDef = line->sideDef(side);

            if(!sideDef.middle().hasMaterial())
                continue;

            if(skyCeil)
            {
                coord_t const top = sector.ceiling().visHeight() + sideDef.middle().visMaterialOrigin()[VY];

                if(top > self.skyFixCeiling())
                {
                    // Must raise the skyfix ceiling.
                    self.setSkyFixCeiling(top);
                }
            }

            if(skyFloor)
            {
                coord_t const bottom = sector.floor().visHeight() +
                    sideDef.middle().visMaterialOrigin()[VY] - sideDef.middle().material().height();

                if(bottom < self.skyFixFloor())
                {
                    // Must lower the skyfix floor.
                    self.setSkyFixFloor(bottom);
                }
            }
        }
    }
};

GameMap::GameMap() : d(new Instance(this))
{
    std::memset(_oldUniqueId, 0, sizeof(_oldUniqueId));
    std::memset(&thinkers, 0, sizeof(thinkers));
    std::memset(&clMobjHash, 0, sizeof(clMobjHash));
    std::memset(&clActivePlanes, 0, sizeof(clActivePlanes));
    std::memset(&clActivePolyobjs, 0, sizeof(clActivePolyobjs));
    entityDatabase = 0;
    mobjBlockmap = 0;
    polyobjBlockmap = 0;
    lineBlockmap = 0;
    bspLeafBlockmap = 0;
    std::memset(&mobjNodes, 0, sizeof(mobjNodes));
    std::memset(&lineNodes, 0, sizeof(lineNodes));
    lineLinks = 0;
    _globalGravity = 0;
    _effectiveGravity = 0;
    _ambientLightLevel = 0;
}

MapElement *GameMap::bspRoot() const
{
    return d->bspRoot;
}

GameMap::HEdges const &GameMap::hedges() const
{
    return d->hedges;
}

GameMap::BspNodes const &GameMap::bspNodes() const
{
    return d->bspNodes;
}

GameMap::BspLeafs const &GameMap::bspLeafs() const
{
    return d->bspLeafs;
}

bool GameMap::buildBsp()
{
    /// @todo Test @em ALL preconditions!
    DENG2_ASSERT(d->hedges.isEmpty());
    DENG2_ASSERT(d->bspLeafs.isEmpty());
    DENG2_ASSERT(d->bspNodes.isEmpty());

    // It begins...
    Time begunAt;

    LOG_INFO("Building BSP using tunable split factor of %d...") << bspFactor;

    Vertexes &editableVertexes = MPE_EditableVertexes();

    // Instantiate and configure a new BSP builder.
    BspBuilder nodeBuilder(*this, editableVertexes, bspFactor);

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
#ifdef DENG2_QT_4_7_OR_NEWER
        d->hedges.reserve(nodeBuilder.numHEdges());
        d->bspNodes.reserve(nodeBuilder.numNodes());
        d->bspLeafs.reserve(nodeBuilder.numLeafs());
#endif

        d->collateVertexes(nodeBuilder, editableVertexes);

        BspTreeNode *rootNode = nodeBuilder.root();
        d->bspRoot = rootNode->userData(); // We'll formally take ownership shortly...

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
                    d->collateBspElements(nodeBuilder, *cur);
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

/// @todo Don't use sector bounds here, derive from lines instead. -ds
void GameMap::updateBounds()
{
    bool isFirst = true;
    foreach(Sector *sector, _sectors)
    {
        // Sectors with no lines have invalid bounds; skip them.
        if(!sector->lineCount()) continue;

        if(isFirst)
        {
            // The first sector is used as is.
            V2d_CopyBox(d->bounds.arvec2, sector->aaBox().arvec2);
            isFirst = false;
        }
        else
        {
            // Expand the bounding box.
            V2d_UniteBox(d->bounds.arvec2, sector->aaBox().arvec2);
        }
    }
}

#ifdef __CLIENT__

SurfaceSet &GameMap::decoratedSurfaces()
{
    return d->decoratedSurfaces;
}

SurfaceSet &GameMap::glowingSurfaces()
{
    return d->glowingSurfaces;
}

void GameMap::buildSurfaceLists()
{
    d->decoratedSurfaces.clear();
    d->glowingSurfaces.clear();

    foreach(SideDef *sideDef, _sideDefs)
    {
        d->addSurfaceToLists(sideDef->middle());
        d->addSurfaceToLists(sideDef->top());
        d->addSurfaceToLists(sideDef->bottom());
    }

    foreach(Sector *sector, _sectors)
    {
        // Skip sectors with no lines as their planes will never be drawn.
        if(!sector->lineCount()) continue;

        foreach(Plane *plane, sector->planes())
        {
            d->addSurfaceToLists(plane->surface());
        }
    }
}

#endif // __CLIENT__

de::Uri GameMap::uri() const
{
    return _uri;
}

char const *GameMap::oldUniqueId() const
{
    return _oldUniqueId;
}

AABoxd const &GameMap::bounds() const
{
    return d->bounds;
}

coord_t GameMap::gravity() const
{
    return _effectiveGravity;
}

void GameMap::setGravity(coord_t newGravity)
{
    _effectiveGravity = newGravity;
}

divline_t const &GameMap::traceLine() const
{
    return d->traceLine;
}

TraceOpening const &GameMap::traceOpening() const
{
    return d->traceOpening;
}

void GameMap::setTraceOpening(LineDef &line)
{
    // Is the linedef part of this map?
    if(lineIndex(&line) < 0) return; // Odd...

    line.configureTraceOpening(d->traceOpening);
}

int GameMap::ambientLightLevel() const
{
    return _ambientLightLevel;
}

void GameMap::initSkyFix()
{
    Time begunAt;

    d->skyFix[Plane::Floor].height   = DDMAXFLOAT;
    d->skyFix[Plane::Ceiling].height = DDMINFLOAT;

    // Update for sector plane heights and mobjs which intersect the ceiling.
    foreach(Sector *sector, _sectors)
    {
        d->updateMapSkyFixForSector(*sector);
    }

    LOG_INFO(String("GameMap::initSkyFix: Done in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
}

coord_t GameMap::skyFix(bool ceiling) const
{
    Plane::Type plane = ceiling? Plane::Ceiling : Plane::Floor;
    return d->skyFix[plane].height;
}

void GameMap::setSkyFix(bool ceiling, coord_t height)
{
    Plane::Type plane = ceiling? Plane::Ceiling : Plane::Floor;
    d->skyFix[plane].height = height;
}

int GameMap::vertexIndex(Vertex const *vtx) const
{
    if(!vtx) return -1;
    return _vertexes.indexOf(const_cast<Vertex *>(vtx)); // Bad performance: O(n)
}

int GameMap::lineIndex(LineDef const *line) const
{
    if(!line) return -1;
    return _lines.indexOf(const_cast<LineDef *>(line)); // Bad performance: O(n)
}

int GameMap::sideDefIndex(SideDef const *side) const
{
    if(!side) return -1;
    return _sideDefs.indexOf(const_cast<SideDef *>(side)); // Bad performance: O(n)
}

int GameMap::sectorIndex(Sector const *sec) const
{
    if(!sec) return -1;
    return _sectors.indexOf(const_cast<Sector *>(sec)); // Bad performance: O(n)
}

Sector *GameMap::sectorBySoundEmitter(ddmobj_base_t const &soundEmitter) const
{
    foreach(Sector *sector, _sectors)
    {
        if(&soundEmitter == &sector->soundEmitter())
            return sector;
    }
    return 0; // Not found.
}

/// @todo Optimize: All sound emitters in a sector are linked together forming
/// a chain. Make use of the chains instead of iterating the sidedefs.
Surface *GameMap::surfaceBySoundEmitter(ddmobj_base_t const &soundEmitter) const
{
    // First try plane surfaces.
    foreach(Sector *sector, _sectors)
    foreach(Plane *plane, sector->planes())
    {
        if(&soundEmitter == &plane->surface().soundEmitter())
        {
            return &plane->surface();
        }
    }

    // Perhaps a wall surface?
    foreach(SideDef *sideDef, _sideDefs)
    {
        if(&soundEmitter == &sideDef->middle().soundEmitter())
        {
            return &sideDef->middle();
        }
        if(&soundEmitter == &sideDef->bottom().soundEmitter())
        {
            return &sideDef->bottom();
        }
        if(&soundEmitter == &sideDef->top().soundEmitter())
        {
            return &sideDef->top();
        }
    }

    return 0; // Not found.
}

int GameMap::bspLeafIndex(BspLeaf const *bspLeaf) const
{
    if(!bspLeaf) return -1;
    return bspLeaf->origIndex();
}

int GameMap::hedgeIndex(HEdge const *hedge) const
{
    if(!hedge) return -1;
    return hedge->origIndex();
}

int GameMap::bspNodeIndex(BspNode const *bspLeaf) const
{
    if(!bspLeaf) return -1;
    return bspLeaf->origIndex();
}

Polyobj *GameMap::polyobjByTag(int tag) const
{
    foreach(Polyobj *polyobj, _polyobjs)
    {
        if(polyobj->tag == tag)
            return polyobj;
    }
    return 0;
}

Polyobj *GameMap::polyobjByBase(ddmobj_base_t const &ddMobjBase) const
{
    foreach(Polyobj *polyobj, _polyobjs)
    {
        if(reinterpret_cast<ddmobj_base_t *>(polyobj) == &ddMobjBase)
            return polyobj;
    }
    return 0;
}

void GameMap::initPolyobjs()
{
    foreach(Polyobj *po, _polyobjs)
    {
        vec2d_t avg; /// < Used to find a polyobj's center, and hence BSP leaf.
        V2d_Set(avg, 0, 0);

        for(LineDef **lineIter = po->lines; *lineIter; lineIter++)
        {
            LineDef *line = *lineIter;
            V2d_Sum(avg, avg, line->v1Origin());
        }
        V2d_Scale(avg, 1.f / po->lineCount);

        if(BspLeaf *bspLeaf = P_BspLeafAtPoint(avg))
        {
            if(bspLeaf->hasPolyobj())
            {
                Con_Message("Warning: GameMap::initPolyobjs: Multiple polyobjs in a single BSP leaf\n"
                            "  (BSP leaf %lu, sector %lu). Previous polyobj overridden.",
                            ulong( theMap->bspLeafIndex(bspLeaf) ),
                            ulong( theMap->sectorIndex(bspLeaf->sectorPtr()) ));
            }
            bspLeaf->_polyObj = po;
            po->bspLeaf = bspLeaf;
        }

        po->updateAABox();
        po->updateSurfaceTangents();

        P_PolyobjUnlink(po);
        P_PolyobjLink(po);
    }
}

Generators *GameMap::generators()
{
    // Time to initialize a new collection?
    if(!d->generators)
    {
        d->generators = Generators_New(sectorCount());
    }
    return d->generators;
}

void GameMap::initNodePiles()
{
    DENG_ASSERT(lineLinks == 0);

    uint starttime = 0;

    VERBOSE( Con_Message("GameMap::InitNodePiles: Initializing...") )
    VERBOSE2( starttime = Timer_RealMilliseconds() )

    // Initialize node piles and line rings.
    NP_Init(&mobjNodes, 256);  // Allocate a small pile.
    NP_Init(&lineNodes, lineCount() + 1000);

    // Allocate the rings.
    lineLinks = (nodeindex_t *) Z_Malloc(sizeof(*lineLinks) * lineCount(), PU_MAPSTATIC, 0);

    for(uint i = 0; i < lineCount(); ++i)
    {
        lineLinks[i] = NP_New(&lineNodes, NP_ROOT_NODE);
    }

    // How much time did we spend?
    VERBOSE2( Con_Message("  Done in %.2f seconds.", (Timer_RealMilliseconds() - starttime) / 1000.0f) )
}

void GameMap::initLineBlockmap(const_pvec2d_t min_, const_pvec2d_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    DENG2_ASSERT(min_ && max_ && lineBlockmap == 0);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    vec2d_t min; V2d_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                              min_[VY] - BLOCKMAP_MARGIN);
    vec2d_t max; V2d_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                              max_[VY] + BLOCKMAP_MARGIN);

    lineBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void GameMap::initMobjBlockmap(const_pvec2d_t min_, const_pvec2d_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    DENG2_ASSERT(min_ && max_ && mobjBlockmap == 0);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    vec2d_t min; V2d_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                              min_[VY] - BLOCKMAP_MARGIN);
    vec2d_t max; V2d_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                              max_[VY] + BLOCKMAP_MARGIN);

    mobjBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void GameMap::initPolyobjBlockmap(const_pvec2d_t min_, const_pvec2d_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    DENG2_ASSERT(min_ && max_ && polyobjBlockmap == 0);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    vec2d_t min; V2d_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                              min_[VY] - BLOCKMAP_MARGIN);
    vec2d_t max; V2d_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                              max_[VY] + BLOCKMAP_MARGIN);

    polyobjBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void GameMap::initBspLeafBlockmap(const_pvec2d_t min_, const_pvec2d_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    DENG2_ASSERT(min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    vec2d_t min; V2d_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                              min_[VY] - BLOCKMAP_MARGIN);
    vec2d_t max; V2d_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                              max_[VY] + BLOCKMAP_MARGIN);

    bspLeafBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void GameMap::linkMobj(mobj_t &mo)
{
    DENG_ASSERT(mobjBlockmap != 0);
    BlockmapCell cell;
    Blockmap_Cell(mobjBlockmap, cell, mo.origin);
    Blockmap_CreateCellAndLinkObject(mobjBlockmap, cell, &mo);
}

bool GameMap::unlinkMobj(mobj_t &mo)
{
    DENG_ASSERT(mobjBlockmap != 0);
    BlockmapCell cell;
    Blockmap_Cell(mobjBlockmap, cell, mo.origin);
    return Blockmap_UnlinkObjectInCell(mobjBlockmap, cell, &mo);
}

struct bmapmoiterparams_t
{
    int localValidCount;
    int (*func) (mobj_t *, void *);
    void *parms;
};

static int blockmapCellMobjsIterator(void *object, void *context)
{
    mobj_t *mobj = (mobj_t *)object;
    bmapmoiterparams_t *args = (bmapmoiterparams_t *) context;
    if(mobj->validCount != args->localValidCount)
    {
        int result;

        // This mobj has now been processed for the current iteration.
        mobj->validCount = args->localValidCount;

        // Action the callback.
        result = args->func(mobj, args->parms);
        if(result) return result; // Stop iteration.
    }
    return false; // Continue iteration.
}

static int iterateCellMobjs(Blockmap &mobjBlockmap, const_BlockmapCell cell,
    int (*callback) (mobj_t *, void *), void *context = 0)
{
    bmapmoiterparams_t args;
    args.localValidCount = validCount;
    args.func            = callback;
    args.parms           = context;

    return Blockmap_IterateCellObjects(&mobjBlockmap, cell,
                                       blockmapCellMobjsIterator, (void *) &args);
}

static int iterateCellBlockMobjs(Blockmap &mobjBlockmap, BlockmapCellBlock const *cellBlock,
    int (*callback) (mobj_t *, void *), void *context = 0)
{
    bmapmoiterparams_t args;
    args.localValidCount = validCount;
    args.func            = callback;
    args.parms           = context;

    return Blockmap_IterateCellBlockObjects(&mobjBlockmap, cellBlock,
                                            blockmapCellMobjsIterator, (void*) &args);
}

int GameMap::mobjsBoxIterator(AABoxd const &box,
    int (*callback) (mobj_t *, void *), void *parameters)
{
    DENG_ASSERT(mobjBlockmap != 0);
    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(mobjBlockmap, &cellBlock, &box);
    return iterateCellBlockMobjs(*mobjBlockmap, &cellBlock, callback, parameters);
}

void GameMap::linkLine(LineDef &line)
{
    DENG_ASSERT(lineBlockmap != 0);

    // LineDefs of Polyobjs don't get into the blockmap (presently...).
    if(line.isFromPolyobj()) return;

    vec2d_t origin; V2d_Copy(origin, Blockmap_Origin(lineBlockmap));
    vec2d_t cellSize; V2d_Copy(cellSize, Blockmap_CellSize(lineBlockmap));

    // Determine the block of cells we'll be working within.
    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(lineBlockmap, &cellBlock, &line.aaBox());

    vec2d_t cell, from, to;

    for(uint y = cellBlock.minY; y <= cellBlock.maxY; ++y)
    for(uint x = cellBlock.minX; x <= cellBlock.maxX; ++x)
    {
        if(line.slopeType() == ST_VERTICAL ||
           line.slopeType() == ST_HORIZONTAL)
        {
            Blockmap_CreateCellAndLinkObjectXY(lineBlockmap, x, y, &line);
            continue;
        }

        // Calculate cell origin.
        V2d_Copy(cell, Blockmap_CellSize(lineBlockmap));
        cell[VX] *= x; cell[VY] *= y;
        V2d_Sum(cell, cell, Blockmap_Origin(lineBlockmap));

        // Choose a cell diagonal to test.
        if(line.slopeType() == ST_POSITIVE)
        {
            // LineDef slope / vs \ cell diagonal.
            V2d_Set(from, cell[VX], cell[VY] + cellSize[VY]);
            V2d_Set(to,   cell[VX] + cellSize[VX], cell[VY]);
        }
        else
        {
            // LineDef slope \ vs / cell diagonal.
            V2d_Set(from, cell[VX] + cellSize[VX], cell[VY] + cellSize[VY]);
            V2d_Set(to,   cell[VX], cell[VY]);
        }

        // Would LineDef intersect this?
        if((line.pointOnSide(from) < 0) != (line.pointOnSide(to) < 0))
        {
            Blockmap_CreateCellAndLinkObjectXY(lineBlockmap, x, y, &line);
        }
    }
}

struct bmapiterparams_t
{
    int localValidCount;
    int (*callback) (LineDef *, void *);
    void *parms;
};

static int blockmapCellLinesIterator(void *mapElement, void *context)
{
    LineDef *line = static_cast<LineDef *>(mapElement);
    bmapiterparams_t *parms = static_cast<bmapiterparams_t *>(context);

    if(line->validCount() != parms->localValidCount)
    {
        int result;

        // This linedef has now been processed for the current iteration.
        line->_validCount = parms->localValidCount;

        // Action the callback.
        result = parms->callback(line, parms->parms);
        if(result) return result; // Stop iteration.
    }

    return false; // Continue iteration.
}

static int iterateCellLines(Blockmap &lineBlockmap, const_BlockmapCell cell,
    int (*callback) (LineDef *, void *), void *context = 0)
{
    bmapiterparams_t parms;
    parms.localValidCount = validCount;
    parms.callback        = callback;
    parms.parms           = context;

    return Blockmap_IterateCellObjects(&lineBlockmap, cell,
                                       blockmapCellLinesIterator, (void *)&parms);
}

static int iterateCellBlockLines(Blockmap &lineBlockmap, BlockmapCellBlock const *cellBlock,
    int (*callback) (LineDef *, void *), void *context = 0)
{
    bmapiterparams_t parms;
    parms.localValidCount = validCount;
    parms.callback        = callback;
    parms.parms           = context;

    return Blockmap_IterateCellBlockObjects(&lineBlockmap, cellBlock,
                                            blockmapCellLinesIterator, (void *) &parms);
}

void GameMap::linkBspLeaf(BspLeaf &bspLeaf)
{
    DENG_ASSERT(bspLeafBlockmap != 0);

    // BspLeafs without sectors don't get in.
    if(!bspLeaf.hasSector()) return;

    AABoxd aaBox(bspLeaf.aaBox().minX, bspLeaf.aaBox().minY,
                 bspLeaf.aaBox().maxX, bspLeaf.aaBox().maxY);

    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(bspLeafBlockmap, &cellBlock, &aaBox);

    for(uint y = cellBlock.minY; y <= cellBlock.maxY; ++y)
    for(uint x = cellBlock.minX; x <= cellBlock.maxX; ++x)
    {
        Blockmap_CreateCellAndLinkObjectXY(bspLeafBlockmap, x, y, &bspLeaf);
    }
}

struct bmapbspleafiterateparams_t
{
    AABoxd const *box;
    Sector *sector;
    int localValidCount;
    int (*func) (BspLeaf *, void *);
    void *parms;
};

static int blockmapCellBspLeafsIterator(void *object, void *context)
{
    BspLeaf *bspLeaf = (BspLeaf *)object;
    bmapbspleafiterateparams_t *args = (bmapbspleafiterateparams_t *) context;
    if(bspLeaf->validCount() != args->localValidCount)
    {
        bool ok = true;

        // This BspLeaf has now been processed for the current iteration.
        bspLeaf->_validCount = args->localValidCount;

        // Check the sector restriction.
        if(args->sector && bspLeaf->sectorPtr() != args->sector)
            ok = false;

        // Check the bounds.
        if(args->box &&
           (bspLeaf->aaBox().maxX < args->box->minX ||
            bspLeaf->aaBox().minX > args->box->maxX ||
            bspLeaf->aaBox().minY > args->box->maxY ||
            bspLeaf->aaBox().maxY < args->box->minY))
            ok = false;

        if(ok)
        {
            // Action the callback.
            int result = args->func(bspLeaf, args->parms);
            if(result) return result; // Stop iteration.
        }
    }
    return false; // Continue iteration.
}

/*
static int iterateCellBspLeafs(Blockmap &bspLeafBlockmap, const_BlockmapCell cell,
    Sector *sector, AABoxd const &box, int localValidCount,
    int (*callback) (BspLeaf *, void *), void *context = 0)
{
    bmapbspleafiterateparams_t args;
    args.localValidCount = localValidCount;
    args.func            = callback;
    args.param           = context;
    args.sector          = sector;
    args.box             = &box;

    return Blockmap_IterateCellObjects(bspLeafBlockmap, cell,
                                       blockmapCellBspLeafsIterator, (void*)&args);
}
*/

static int iterateCellBlockBspLeafs(Blockmap &bspLeafBlockmap,
    BlockmapCellBlock const &cellBlock, Sector *sector,  AABoxd const &box,
    int localValidCount,
    int (*callback) (BspLeaf *, void *), void *context = 0)
{
    bmapbspleafiterateparams_t args;
    args.localValidCount = localValidCount;
    args.func            = callback;
    args.parms           = context;
    args.sector          = sector;
    args.box             = &box;

    return Blockmap_IterateCellBlockObjects(&bspLeafBlockmap, &cellBlock,
                                            blockmapCellBspLeafsIterator, (void *) &args);
}

int GameMap::bspLeafsBoxIterator(AABoxd const &box, Sector *sector,
    int (*callback) (BspLeaf *, void *), void *parameters)
{
    DENG_ASSERT(bspLeafBlockmap != 0);
    static int localValidCount = 0;

    // This is only used here.
    localValidCount++;

    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(bspLeafBlockmap, &cellBlock, &box);

    return iterateCellBlockBspLeafs(*bspLeafBlockmap, cellBlock, sector, box,
                                    localValidCount, callback, parameters);
}

void GameMap::linkPolyobj(Polyobj &polyobj)
{
    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(polyobjBlockmap, &cellBlock, &polyobj.aaBox);

    for(uint y = cellBlock.minY; y <= cellBlock.maxY; ++y)
    for(uint x = cellBlock.minX; x <= cellBlock.maxX; ++x)
    {
        Blockmap_CreateCellAndLinkObjectXY(polyobjBlockmap, x, y, &polyobj);
    }
}

void GameMap::unlinkPolyobj(Polyobj &polyobj)
{
    DENG_ASSERT(polyobjBlockmap != 0);
    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(polyobjBlockmap, &cellBlock, &polyobj.aaBox);
    Blockmap_UnlinkObjectInCellBlock(polyobjBlockmap, &cellBlock, &polyobj);
}

struct bmappoiterparams_t
{
    int localValidCount;
    int (*func) (Polyobj *, void *);
    void *parms;
};

static int blockmapCellPolyobjsIterator(void *object, void *context)
{
    Polyobj *polyobj = (Polyobj *)object;
    bmappoiterparams_t *args = (bmappoiterparams_t *) context;
    if(polyobj->validCount != args->localValidCount)
    {
        int result;

        // This polyobj has now been processed for the current iteration.
        polyobj->validCount = args->localValidCount;

        // Action the callback.
        result = args->func(polyobj, args->parms);
        if(result) return result; // Stop iteration.
    }
    return false; // Continue iteration.
}

static int iterateCellPolyobjs(Blockmap &polyobjBlockmap, const_BlockmapCell cell,
    int (*callback) (Polyobj *, void *), void *context = 0)
{
    bmappoiterparams_t args;
    args.localValidCount = validCount;
    args.func            = callback;
    args.parms           = context;

    return Blockmap_IterateCellObjects(&polyobjBlockmap, cell,
                                       blockmapCellPolyobjsIterator, (void *)&args);
}

static int iterateCellBlockPolyobjs(Blockmap &polyobjBlockmap, BlockmapCellBlock const &cellBlock,
    int (*callback) (Polyobj *, void *), void *context = 0)
{
    bmappoiterparams_t args;
    args.localValidCount = validCount;
    args.func            = callback;
    args.parms           = context;

    return Blockmap_IterateCellBlockObjects(&polyobjBlockmap, &cellBlock,
                                            blockmapCellPolyobjsIterator, (void*) &args);
}

int GameMap::polyobjsBoxIterator(AABoxd const &box,
    int (*callback) (struct polyobj_s *, void *), void *parameters)
{
    DENG_ASSERT(polyobjBlockmap != 0);
    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(polyobjBlockmap, &cellBlock, &box);
    return iterateCellBlockPolyobjs(*polyobjBlockmap, cellBlock, callback, parameters);
}

struct poiterparams_t
{
    int (*func) (LineDef *, void *);
    void *parms;
};

int PTR_PolyobjLines(Polyobj *po, void* context)
{
    poiterparams_t *args = (poiterparams_t *) context;

    return po->lineIterator(args->func, args->parms);
}

/*
static int iterateCellPolyobjLinesIterator(Blockmap &polyobjBlockmap, const_BlockmapCell cell,
    int (*callback) (LineDef *, void *), void *context = 0)
{
    poiterparams_t poargs;
    poargs.func  = callback;
    poargs.param = context;

    bmappoiterparams_t args;
    args.localValidCount = validCount;
    args.func            = PTR_PolyobjLines;
    args.param           = &poargs;

    return Blockmap_IterateCellObjects(&polyobjBlockmap, cell,
                                       blockmapCellPolyobjsIterator, &args);
}
*/

static int iterateCellBlockPolyobjLines(Blockmap &polyobjBlockmap,
    BlockmapCellBlock const &cellBlock,
    int (*callback) (LineDef *, void *), void *context = 0)
{
    poiterparams_t poargs;
    poargs.func  = callback;
    poargs.parms = context;

    bmappoiterparams_t args;
    args.localValidCount = validCount;
    args.func            = PTR_PolyobjLines;
    args.parms           = &poargs;

    return Blockmap_IterateCellBlockObjects(&polyobjBlockmap, &cellBlock,
                                            blockmapCellPolyobjsIterator, (void*) &args);
}

int GameMap::linesBoxIterator(AABoxd const &box,
    int (*callback) (LineDef *, void *), void *parameters)
{
    DENG_ASSERT(polyobjBlockmap != 0);
    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(lineBlockmap, &cellBlock, &box);
    return iterateCellBlockLines(*lineBlockmap, &cellBlock, callback, parameters);
}

int GameMap::polyobjLinesBoxIterator(AABoxd const &box,
    int (*callback) (LineDef *, void *), void *parameters)
{
    DENG_ASSERT(polyobjBlockmap != 0);
    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(polyobjBlockmap, &cellBlock, &box);
    return iterateCellBlockPolyobjLines(*polyobjBlockmap, cellBlock, callback, parameters);
}

int GameMap::allLinesBoxIterator(AABoxd const &box,
    int (*callback) (LineDef *, void *), void *parameters)
{
    if(!_polyobjs.isEmpty())
    {
        int result = P_PolyobjLinesBoxIterator(&box, callback, parameters);
        if(result) return result;
    }
    return P_LinesBoxIterator(&box, callback, parameters);
}

static int traverseCellPath2(Blockmap *bmap, uint const fromBlock[2],
    uint const toBlock[2], const_pvec2d_t from, const_pvec2d_t to,
    int (*callback) (uint const block[2], void *parameters), void *parameters)
{
    DENG2_ASSERT(bmap != 0);

    int result = false; // Continue iteration.
    coord_t intercept[2], delta[2], partial;
    uint count, block[2];
    int stepDir[2];

    if(toBlock[VX] > fromBlock[VX])
    {
        stepDir[VX] = 1;
        partial = from[VX] / Blockmap_CellWidth(bmap);
        partial = 1 - (partial - (int) partial);
        delta[VY] = (to[VY] - from[VY]) / fabs(to[VX] - from[VX]);
    }
    else if(toBlock[VX] < fromBlock[VX])
    {
        stepDir[VX] = -1;
        partial = from[VX] / Blockmap_CellWidth(bmap);
        partial = (partial - (int) partial);
        delta[VY] = (to[VY] - from[VY]) / fabs(to[VX] - from[VX]);
    }
    else
    {
        stepDir[VX] = 0;
        partial = 1;
        delta[VY] = 256;
    }
    intercept[VY] = from[VY] / Blockmap_CellHeight(bmap) + partial * delta[VY];

    if(toBlock[VY] > fromBlock[VY])
    {
        stepDir[VY] = 1;
        partial = from[VY] / Blockmap_CellHeight(bmap);
        partial = 1 - (partial - (int) partial);
        delta[VX] = (to[VX] - from[VX]) / fabs(to[VY] - from[VY]);
    }
    else if(toBlock[VY] < fromBlock[VY])
    {
        stepDir[VY] = -1;
        partial = from[VY] / Blockmap_CellHeight(bmap);
        partial = (partial - (int) partial);
        delta[VX] = (to[VX] - from[VX]) / fabs(to[VY] - from[VY]);
    }
    else
    {
        stepDir[VY] = 0;
        partial = 1;
        delta[VX] = 256;
    }
    intercept[VX] = from[VX] / Blockmap_CellWidth(bmap) + partial * delta[VX];

    //
    // Step through map blocks.
    //

    // Count is present to prevent a round off error from skipping the
    // break and ending up in an infinite loop..
    block[VX] = fromBlock[VX];
    block[VY] = fromBlock[VY];
    for(count = 0; count < 64; ++count)
    {
        result = callback(block, parameters);
        if(result) return result; // Early out.

        if(block[VX] == toBlock[VX] && block[VY] == toBlock[VY])
            break;

        /// @todo Replace incremental translation?
        if((uint)intercept[VY] == block[VY])
        {
            block[VX] += stepDir[VX];
            intercept[VY] += delta[VY];
        }
        else if((uint)intercept[VX] == block[VX])
        {
            block[VY] += stepDir[VY];
            intercept[VX] += delta[VX];
        }
    }

    return false; // Continue iteration.
}

static int traverseCellPath(divline_t &traceLine, Blockmap *bmap,
    const_pvec2d_t from_, const_pvec2d_t to_,
    int (*callback) (uint const block[2], void *parameters), void *parameters = 0)
{
    DENG2_ASSERT(bmap != 0);

    // Constant terms implicitly defined by DOOM's original version of this
    // algorithm (we must honor these fudge factors for compatibility).
    coord_t const epsilon    = FIX2FLT(FRACUNIT);
    coord_t const unitOffset = FIX2FLT(FRACUNIT);

    vec2d_t min; V2d_Copy(min, Blockmap_Bounds(bmap)->min);
    vec2d_t max; V2d_Copy(max, Blockmap_Bounds(bmap)->max);

    // We may need to clip and/or fudge these points.
    vec2d_t from; V2d_Copy(from, from_);
    vec2d_t to;   V2d_Copy(to, to_);

    if(!(from[VX] >= min[VX] && from[VX] <= max[VX] &&
         from[VY] >= min[VY] && from[VY] <= max[VY]))
    {
        // 'From' is outside the blockmap (really? very unusual...)
        return true;
    }

    // Check the easy case of a path that lies completely outside the bmap.
    if((from[VX] < min[VX] && to[VX] < min[VX]) ||
       (from[VX] > max[VX] && to[VX] > max[VX]) ||
       (from[VY] < min[VY] && to[VY] < min[VY]) ||
       (from[VY] > max[VY] && to[VY] > max[VY]))
    {
        // Nothing intercepts outside the blockmap!
        return true;
    }

    // Lines should not be perfectly parallel to a blockmap axis.
    // We honor these so-called fudge factors for compatible behavior
    // with DOOM's algorithm.
    coord_t dX = (from[VX] - Blockmap_Origin(bmap)[VX]) / Blockmap_CellWidth(bmap);
    coord_t dY = (from[VY] - Blockmap_Origin(bmap)[VY]) / Blockmap_CellHeight(bmap);
    if(INRANGE_OF(dX, 0, epsilon)) from[VX] += unitOffset;
    if(INRANGE_OF(dY, 0, epsilon)) from[VY] += unitOffset;

    traceLine.origin[VX] = FLT2FIX(from[VX]);
    traceLine.origin[VY] = FLT2FIX(from[VY]);
    traceLine.direction[VX] = FLT2FIX(to[VX] - from[VX]);
    traceLine.direction[VY] = FLT2FIX(to[VY] - from[VY]);

    /**
     * It is possible that one or both points are outside the blockmap.
     * Clip path so that 'to' is within the AABB of the blockmap (note we
     * would have already abandoned if 'from' lay outside..
     */
    if(!(to[VX] >= min[VX] && to[VX] <= max[VX] &&
         to[VY] >= min[VY] && to[VY] <= max[VY]))
    {
        // 'to' is outside the blockmap.
        vec2d_t bounds[4], point;
        coord_t ab;

        V2d_Set(bounds[0], min[VX], min[VY]);
        V2d_Set(bounds[1], min[VX], max[VY]);
        V2d_Set(bounds[2], max[VX], max[VY]);
        V2d_Set(bounds[3], max[VX], min[VY]);

        ab = V2d_Intercept(from, to, bounds[0], bounds[1], point);
        if(ab >= 0 && ab <= 1)
            V2d_Copy(to, point);

        ab = V2d_Intercept(from, to, bounds[1], bounds[2], point);
        if(ab >= 0 && ab <= 1)
            V2d_Copy(to, point);

        ab = V2d_Intercept(from, to, bounds[2], bounds[3], point);
        if(ab >= 0 && ab <= 1)
            V2d_Copy(to, point);

        ab = V2d_Intercept(from, to, bounds[3], bounds[0], point);
        if(ab >= 0 && ab <= 1)
            V2d_Copy(to, point);
    }

    // Clipping already applied above, so we don't need to check it again...
    uint fromBlock[2]; Blockmap_Cell(bmap, fromBlock, from);
    uint toBlock[2];   Blockmap_Cell(bmap, toBlock, to);

    V2d_Subtract(from, from, min);
    V2d_Subtract(to, to, min);
    return traverseCellPath2(bmap, fromBlock, toBlock, from, to, callback, parameters);
}

struct iteratepolyobjlines_params_t
{
    int (*callback) (LineDef *, void *);
    void *parms;
};

static int iteratePolyobjLines(Polyobj *po, void *parameters = 0)
{
    iteratepolyobjlines_params_t const *p = (iteratepolyobjlines_params_t *)parameters;
    return po->lineIterator(p->callback, p->parms);
}

static int collectPolyobjLineIntercepts(uint const block[2], void *parameters)
{
    Blockmap *polyobjBlockmap = (Blockmap *)parameters;
    iteratepolyobjlines_params_t iplParams;
    iplParams.callback = PIT_AddLineDefIntercepts;
    iplParams.parms    = 0;
    return iterateCellPolyobjs(*polyobjBlockmap, block,
                               iteratePolyobjLines, (void *)&iplParams);
}

static int collectLineIntercepts(uint const block[2], void *parameters)
{
    Blockmap *lineBlockmap = (Blockmap *)parameters;
    return iterateCellLines(*lineBlockmap, block, PIT_AddLineDefIntercepts);
}

static int collectMobjIntercepts(uint const block[2], void *parameters)
{
    Blockmap *mobjBlockmap = (Blockmap *)parameters;
    return iterateCellMobjs(*mobjBlockmap, block, PIT_AddMobjIntercepts);
}

int GameMap::pathTraverse(const_pvec2d_t from, const_pvec2d_t to, int flags,
    traverser_t callback, void *parameters)
{
    // A new intercept trace begins...
    P_ClearIntercepts();
    validCount++;

    // Step #1: Collect intercepts.
    if(flags & PT_ADDLINES)
    {
        if(!_polyobjs.isEmpty())
        {
            DENG_ASSERT(polyobjBlockmap != 0);
            traverseCellPath(d->traceLine, polyobjBlockmap, from, to,
                             collectPolyobjLineIntercepts, (void *)polyobjBlockmap);
        }

        DENG_ASSERT(lineBlockmap != 0);
        traverseCellPath(d->traceLine, lineBlockmap, from, to, collectLineIntercepts,
                         (void *)lineBlockmap);
    }
    if(flags & PT_ADDMOBJS)
    {
        DENG_ASSERT(mobjBlockmap != 0);
        traverseCellPath(d->traceLine, mobjBlockmap, from, to, collectMobjIntercepts,
                         (void *)mobjBlockmap);
    }

    // Step #2: Process sorted intercepts.
    return P_TraverseIntercepts(callback, parameters);
}

BspLeaf *GameMap::bspLeafAtPoint(const_pvec2d_t const point)
{
    MapElement *bspElement = d->bspRoot;
    while(bspElement->type() != DMU_BSPLEAF)
    {
        BspNode const &node = *bspElement->castTo<BspNode>();
        int side = node.partition().pointOnSide(point);

        // Decend to the next child subspace.
        bspElement = node.childPtr(side);
    }
    return bspElement->castTo<BspLeaf>();
}

void GameMap::lerpScrollingSurfaces(bool resetNextViewer)
{
    SurfaceSet::iterator it = d->scrollingSurfaces.begin();
    while(it != d->scrollingSurfaces.end())
    {
        Surface &suf = **it;

        if(resetNextViewer)
        {
            // Reset the material offset trackers.
            // X Offset.
            suf._visOffsetDelta[0] = 0;
            suf._oldOffset[0][0] = suf._oldOffset[0][1] = suf._offset[0];

            // Y Offset.
            suf._visOffsetDelta[1] = 0;
            suf._oldOffset[1][0] = suf._oldOffset[1][1] = suf._offset[1];

#ifdef __CLIENT__
            suf.markAsNeedingDecorationUpdate();
#endif

            it++;
        }
        // While the game is paused there is no need to calculate any
        // visual material offsets.
        else //if(!clientPaused)
        {
            // Set the visible material offsets.
            // X Offset.
            suf._visOffsetDelta[0] =
                suf._oldOffset[0][0] * (1 - frameTimePos) +
                        suf._offset[0] * frameTimePos - suf._offset[0];

            // Y Offset.
            suf._visOffsetDelta[1] =
                suf._oldOffset[1][0] * (1 - frameTimePos) +
                        suf._offset[1] * frameTimePos - suf._offset[1];

            // Visible material offset.
            suf._visOffset[0] = suf._offset[0] + suf._visOffsetDelta[0];
            suf._visOffset[1] = suf._offset[1] + suf._visOffsetDelta[1];

#ifdef __CLIENT__
            suf.markAsNeedingDecorationUpdate();
#endif

            // Has this material reached its destination?
            if(suf._visOffset[0] == suf._offset[0] && suf._visOffset[1] == suf._offset[1])
            {
                it = d->scrollingSurfaces.erase(it);
            }
            else
            {
                it++;
            }
        }
    }
}

void GameMap::updateScrollingSurfaces()
{
    foreach(Surface *surface, d->scrollingSurfaces)
    {
        // X Offset
        surface->_oldOffset[0][0] = surface->_oldOffset[0][1];
        surface->_oldOffset[0][1] = surface->_offset[0];

        if(surface->_oldOffset[0][0] != surface->_oldOffset[0][1])
        {
            if(de::abs(surface->_oldOffset[0][0] - surface->_oldOffset[0][1]) >=
               MAX_SMOOTH_MATERIAL_MOVE)
            {
                // Too fast: make an instantaneous jump.
                surface->_oldOffset[0][0] = surface->_oldOffset[0][1];
            }
        }

        // Y Offset
        surface->_oldOffset[1][0] = surface->_oldOffset[1][1];
        surface->_oldOffset[1][1] = surface->_offset[1];
        if(surface->_oldOffset[1][0] != surface->_oldOffset[1][1])
        {
            if(de::abs(surface->_oldOffset[1][0] - surface->_oldOffset[1][1]) >=
               MAX_SMOOTH_MATERIAL_MOVE)
            {
                // Too fast: make an instantaneous jump.
                surface->_oldOffset[1][0] = surface->_oldOffset[1][1];
            }
        }
    }
}

SurfaceSet &GameMap::scrollingSurfaces()
{
    return d->scrollingSurfaces;
}

void GameMap::lerpTrackedPlanes(bool resetNextViewer)
{
    if(resetNextViewer)
    {
        // Reset the plane height trackers.
        foreach(Plane *plane, d->trackedPlanes)
        {
            plane->resetVisHeight();
        }

        // Tracked movement is now all done.
        d->trackedPlanes.clear();
    }
    // While the game is paused there is no need to calculate any
    // visual plane offsets $smoothplane.
    else //if(!clientPaused)
    {
        // Set the visible offsets.
        QMutableSetIterator<Plane *> iter(d->trackedPlanes);
        while(iter.hasNext())
        {
            Plane *plane = iter.next();

            plane->lerpVisHeight();

            // Has this plane reached its destination?
            if(plane->visHeight() == plane->height()) /// @todo  Can this fail? (float equality)
            {
                iter.remove();
            }
        }
    }
}

void GameMap::updateTrackedPlanes()
{
    foreach(Plane *plane, d->trackedPlanes)
    {
        plane->updateHeightTracking();
    }
}

PlaneSet &GameMap::trackedPlanes()
{
    return d->trackedPlanes;
}

void GameMap::updateSurfacesOnMaterialChange(Material &material)
{
    if(ddMapSetup) return;

#ifdef __CLIENT__
    foreach(Surface *surface, d->decoratedSurfaces)
    {
        if(&material == surface->materialPtr())
        {
            surface->markAsNeedingDecorationUpdate();
        }
    }
#endif
}
