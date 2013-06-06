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

#include <de/libdeng2.h>

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

using namespace de;

DENG2_PIMPL(GameMap)
{
    /// Boundary points which encompass the entire map.
    AABoxd bounds;

    /// BSP root element.
    MapElement *bspRoot;

    /// BSP element LUTs:
    Segments segments;
    BspNodes bspNodes;
    BspLeafs bspLeafs;

    struct generators_s *generators;

    PlaneSet trackedPlanes;
    SurfaceSet scrollingSurfaces;
#ifdef __CLIENT__
    SurfaceSet decoratedSurfaces;
    SurfaceSet glowingSurfaces;

    QScopedPointer<LightGrid> lightGrid;
#endif

    coord_t skyFix[2]; // [floor, ceiling]

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

    void collateVertexes(BspBuilder &builder)
    {
        int bspVertexCount = builder.numVertexes();
        for(int i = 0; i < bspVertexCount; ++i)
        {
            // Take ownership of this Vertex.
            Vertex *vertex = &builder.vertex(i);
            builder.take(vertex);

            self._vertexes.append(vertex);
        }
    }

    void collateBspElements(BspBuilder &builder, BspTreeNode &tree)
    {
        if(tree.isLeaf())
        {
            // Take ownership of the BspLeaf.
            DENG2_ASSERT(tree.userData() != 0);
            BspLeaf *leaf = tree.userData()->castTo<BspLeaf>();
            builder.take(leaf);

            // Add this BspLeaf to the LUT.
            leaf->setIndexInMap(bspLeafs.count());
            bspLeafs.append(leaf);

            foreach(Segment *seg, leaf->allSegments())
            {
                // Take ownership of the Segment.
                builder.take(seg);

                // Add this segment to the LUT.
                seg->setIndexInMap(segments.count());
                segments.append(seg);
            }

            return;
        }
        // Else; a node.

        // Take ownership of this BspNode.
        DENG2_ASSERT(tree.userData() != 0);
        BspNode *node = tree.userData()->castTo<BspNode>();
        builder.take(node);

        // Add this BspNode to the LUT.
        node->setIndexInMap(bspNodes.count());
        bspNodes.append(node);
    }

    void finishLines()
    {
        foreach(Line *line, self._lines)
        for(int i = 0; i < 2; ++i)
        {
            line->side(i).updateSurfaceNormals();
            line->side(i).updateAllSoundEmitterOrigins();
        }
    }

    void finishSectors()
    {
        foreach(Sector *sector, self._sectors)
        {
            sector->buildBspLeafs(self);
            sector->buildSides(self);
            sector->updateAABox();
            sector->updateRoughArea();

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
                sector->linkSoundEmitter(plane->soundEmitter());
            }

            // Link wall surface emitters:
            foreach(Line::Side *side, sector->sides())
            {
                if(side->hasSections())
                {
                    sector->linkSoundEmitter(side->middleSoundEmitter());
                    sector->linkSoundEmitter(side->bottomSoundEmitter());
                    sector->linkSoundEmitter(side->topSoundEmitter());
                }
                if(side->line().isSelfReferencing() && side->back().hasSections())
                {
                    Line::Side &back = side->back();
                    sector->linkSoundEmitter(back.middleSoundEmitter());
                    sector->linkSoundEmitter(back.bottomSoundEmitter());
                    sector->linkSoundEmitter(back.topSoundEmitter());
                }
            }

            sector->updateSoundEmitterOrigin();
        }
    }

    /// @todo Relocate this work to R_SetupMap() -ds
    void finishPlanes()
    {
        foreach(Sector *sector, self._sectors)
        foreach(Plane *plane, sector->planes())
        {
            plane->updateSoundEmitterOrigin();

#ifdef __CLIENT__
            // Resize the biassurface lists for the BSP leaf planes.
            foreach(BspLeaf *bspLeaf, sector->bspLeafs())
            {
                if(bspLeaf->isDegenerate()) continue;

                int n = 0;

                BiasSurface **newList = (BiasSurface **) Z_Calloc(sector->planeCount() * sizeof(BiasSurface *), PU_MAP, 0);
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
                    BiasSurface *bsuf = SB_CreateSurface();

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

    void updateMapSkyFixForSector(Sector const &sector)
    {
        if(!sector.sideCount()) return;

        bool const skyFloor = sector.floorSurface().hasSkyMaskedMaterial();
        bool const skyCeil  = sector.ceilingSurface().hasSkyMaskedMaterial();

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

        // Update for middle materials on lines which intersect the
        // floor and/or ceiling on the front (i.e., sector) side.
        foreach(Line::Side *side, sector.sides())
        {
            if(!side->hasSections()) continue;
            if(!side->middle().hasMaterial()) continue;

            // There must be a sector on both sides.
            if(!side->hasSector() || !side->back().hasSector()) continue;

            coord_t bottomZ, topZ;
            Vector2f materialOrigin;
            R_SideSectionCoords(*side, Line::Side::Middle, 0,
                                &bottomZ, &topZ, &materialOrigin);
            if(skyCeil && topZ + materialOrigin.y > self.skyFixCeiling())
            {
                // Must raise the skyfix ceiling.
                self.setSkyFixCeiling(topZ + materialOrigin.y);
            }

            if(skyFloor && bottomZ + materialOrigin.y < self.skyFixFloor())
            {
                // Must lower the skyfix floor.
                self.setSkyFixFloor(bottomZ + materialOrigin.y);
            }
        }
    }

    /**
     * Locate a polyobj in the map by sound emitter.
     *
     * @param soundEmitter  ddmobj_base_t to search for.
     *
     * @return  Pointer to the referenced Polyobj instance; otherwise @c 0.
     */
    Polyobj *polyobjBySoundEmitter(ddmobj_base_t const &soundEmitter) const
    {
        foreach(Polyobj *polyobj, self._polyobjs)
        {
            if(&soundEmitter == &polyobj->soundEmitter())
                return polyobj;
        }
        return 0;
    }

    /**
     * Locate a sector in the map by sound emitter.
     *
     * @param soundEmitter  ddmobj_base_t to search for.
     *
     * @return  Pointer to the referenced Sector instance; otherwise @c 0.
     */
    Sector *sectorBySoundEmitter(ddmobj_base_t const &soundEmitter) const
    {
        foreach(Sector *sector, self._sectors)
        {
            if(&soundEmitter == &sector->soundEmitter())
                return sector;
        }
        return 0; // Not found.
    }

    /**
     * Locate a sector plane in the map by sound emitter.
     *
     * @param soundEmitter  ddmobj_base_t to search for.
     *
     * @return  Pointer to the referenced Plane instance; otherwise @c 0.
     */
    Plane *planeBySoundEmitter(ddmobj_base_t const &soundEmitter) const
    {
        foreach(Sector *sector, self._sectors)
        foreach(Plane *plane, sector->planes())
        {
            if(&soundEmitter == &plane->soundEmitter())
            {
                return plane;
            }
        }
        return 0; // Not found.
    }

    /**
     * Locate a surface in the map by sound emitter.
     *
     * @param soundEmitter  ddmobj_base_t to search for.
     *
     * @return  Pointer to the referenced Surface instance; otherwise @c 0.
     */
    Surface *surfaceBySoundEmitter(ddmobj_base_t const &soundEmitter) const
    {
        // Perhaps a wall surface?
        foreach(Line *line, self._lines)
        for(int i = 0; i < 2; ++i)
        {
            Line::Side &side = line->side(i);
            if(!side.hasSections()) continue;

            if(&soundEmitter == &side.middleSoundEmitter())
            {
                return &side.middle();
            }
            if(&soundEmitter == &side.bottomSoundEmitter())
            {
                return &side.bottom();
            }
            if(&soundEmitter == &side.topSoundEmitter())
            {
                return &side.top();
            }
        }

        return 0; // Not found.
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

/// @todo fixme: Free all memory we have ownership of.
GameMap::~GameMap()
{
    // thinker lists - free them!

    // Client only data:
    // mobjHash/activePlanes/activePolyobjs - free them!
    // End client only data.

    qDeleteAll(_vertexes);
    qDeleteAll(_sectors);
    qDeleteAll(_lines);
    foreach(Polyobj *polyobj, _polyobjs)
    {
        polyobj->~Polyobj();
        M_Free(polyobj);
    }

    EntityDatabase_Delete(entityDatabase);

    // mobj/line/polyobj/bspLeaf blockmaps - free them!
    // mobjNodes/lineNodes/lineLinks - free them!
}

MapElement *GameMap::bspRoot() const
{
    return d->bspRoot;
}

GameMap::Segments const &GameMap::segments() const
{
    return d->segments;
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
    DENG2_ASSERT(d->segments.isEmpty());
    DENG2_ASSERT(d->bspLeafs.isEmpty());
    DENG2_ASSERT(d->bspNodes.isEmpty());

    // It begins...
    Time begunAt;

    LOG_INFO("Building BSP using tunable split factor of %d...") << bspFactor;

    // Instantiate and configure a new BSP builder.
    BspBuilder nodeBuilder(*this, bspFactor);

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

        LOG_INFO("BSP built: (%d:%d) %d Nodes, %d Leafs, %d Segments, %d Vertexes.")
                << rightBranchDpeth << leftBranchDepth
                << nodeBuilder.numNodes() << nodeBuilder.numLeafs()
                << nodeBuilder.numSegments() << nodeBuilder.numVertexes();

        /*
         * Take ownership of all the built map data elements.
         */
#ifdef DENG2_QT_4_7_OR_NEWER
        _vertexes.reserve(_vertexes.count() + nodeBuilder.numVertexes());
        d->segments.reserve(nodeBuilder.numSegments());
        d->bspNodes.reserve(nodeBuilder.numNodes());
        d->bspLeafs.reserve(nodeBuilder.numLeafs());
#endif
        d->collateVertexes(nodeBuilder);

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

void GameMap::finishMapElements()
{
    d->finishLines();
    d->finishSectors();
    d->finishPlanes();
}

void GameMap::updateBounds()
{
    bool isFirst = true;
    foreach(Line *line, _lines)
    {
        if(isFirst)
        {
            // The first line's bounds are used as is.
            V2d_CopyBox(d->bounds.arvec2, line->aaBox().arvec2);
            isFirst = false;
        }
        else
        {
            // Expand the bounding box.
            V2d_UniteBox(d->bounds.arvec2, line->aaBox().arvec2);
        }
    }
}

#ifdef __CLIENT__

GameMap::SurfaceSet &GameMap::decoratedSurfaces()
{
    return d->decoratedSurfaces;
}

GameMap::SurfaceSet &GameMap::glowingSurfaces()
{
    return d->glowingSurfaces;
}

void GameMap::buildSurfaceLists()
{
    d->decoratedSurfaces.clear();
    d->glowingSurfaces.clear();

    foreach(Line *line, _lines)
    for(int i = 0; i < 2; ++i)
    {
        Line::Side &side = line->side(i);
        if(!side.hasSections()) continue;

        d->addSurfaceToLists(side.middle());
        d->addSurfaceToLists(side.top());
        d->addSurfaceToLists(side.bottom());
    }

    foreach(Sector *sector, _sectors)
    {
        // Skip sectors with no lines as their planes will never be drawn.
        if(!sector->sideCount()) continue;

        foreach(Plane *plane, sector->planes())
        {
            d->addSurfaceToLists(plane->surface());
        }
    }
}

bool GameMap::hasLightGrid()
{
    return !d->lightGrid.isNull();
}

LightGrid &GameMap::lightGrid()
{
    if(!d->lightGrid.isNull())
    {
        return *d->lightGrid;
    }
    /// @throw MissingLightGrid Attempted with no LightGrid initialized.
    throw MissingLightGridError("GameMap::lightGrid", "No light grid is initialized");
}

void GameMap::initLightGrid()
{
    // Disabled?
    if(!Con_GetInteger("rend-bias-grid"))
        return;

    // Time to initialize the LightGrid?
    if(d->lightGrid.isNull())
    {
        d->lightGrid.reset(new LightGrid(*this));
    }
    // Perform a full update right away.
    d->lightGrid->update();
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

void GameMap::setTraceOpening(Line &line)
{
    if(!line.hasBackSector())
    {
        d->traceOpening.range = 0;
        return;
    }

    coord_t bottom, top;
    d->traceOpening.range  = float( R_OpenRange(line.front(), &bottom, &top) );
    d->traceOpening.bottom = float( bottom );
    d->traceOpening.top    = float( top );

    // Determine the "low floor".
    Sector const &frontSector = line.frontSector();
    Sector const &backSector  = line.backSector();

    if(frontSector.floor().height() > backSector.floor().height())
    {
        d->traceOpening.lowFloor = float( backSector.floor().height() );
    }
    else
    {
        d->traceOpening.lowFloor = float( frontSector.floor().height() );
    }
}

int GameMap::ambientLightLevel() const
{
    return _ambientLightLevel;
}

void GameMap::initSkyFix()
{
    Time begunAt;

    d->skyFix[Plane::Floor]   = DDMAXFLOAT;
    d->skyFix[Plane::Ceiling] = DDMINFLOAT;

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
    return d->skyFix[plane];
}

void GameMap::setSkyFix(bool ceiling, coord_t newHeight)
{
    Plane::Type plane = ceiling? Plane::Ceiling : Plane::Floor;
    d->skyFix[plane] = newHeight;
}

int GameMap::toSideIndex(int lineIndex, int backSide) // static
{
    DENG_ASSERT(lineIndex >= 0);
    return lineIndex * 2 + (backSide? 1 : 0);
}

Line::Side *GameMap::sideByIndex(int index) const
{
    if(index < 0) return 0;
    return &_lines.at(index / 2)->side(index % 2);
}

bool GameMap::identifySoundEmitter(ddmobj_base_t const &emitter, Sector **sector,
    Polyobj **poly, Plane **plane, Surface **surface) const
{
    *sector  = 0;
    *poly    = 0;
    *plane   = 0;
    *surface = 0;

    /// @todo Optimize: All sound emitters in a sector are linked together forming
    /// a chain. Make use of the chains instead.

    *poly = d->polyobjBySoundEmitter(emitter);
    if(!*poly)
    {
        // Not a polyobj. Try the sectors next.
        *sector = d->sectorBySoundEmitter(emitter);
        if(!*sector)
        {
            // Not a sector. Try the planes next.
            *plane = d->planeBySoundEmitter(emitter);
            if(!*plane)
            {
                // Not a plane. Try the surfaces next.
                *surface = d->surfaceBySoundEmitter(emitter);
            }
        }
    }

    return (*sector != 0 || *poly != 0|| *plane != 0|| *surface != 0);
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

void GameMap::initPolyobjs()
{
    LOG_AS("GameMap::initPolyobjs");

    foreach(Polyobj *po, _polyobjs)
    {
        /// @todo Is this still necessary? -ds
        /// (This data is updated automatically when moving/rotating).
        po->updateAABox();
        po->updateSurfaceTangents();

        po->unlink();
        po->link();
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

    for(int i = 0; i < lineCount(); ++i)
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
    int (*callback) (mobj_t *, void *), void *parameters) const
{
    DENG_ASSERT(mobjBlockmap != 0);
    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(mobjBlockmap, &cellBlock, &box);
    return iterateCellBlockMobjs(*mobjBlockmap, &cellBlock, callback, parameters);
}

void GameMap::linkLine(Line &line)
{
    DENG_ASSERT(lineBlockmap != 0);

    // Lines of Polyobjs don't get into the blockmap (presently...).
    if(line.definesPolyobj()) return;

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
            // Line slope / vs \ cell diagonal.
            V2d_Set(from, cell[VX], cell[VY] + cellSize[VY]);
            V2d_Set(to,   cell[VX] + cellSize[VX], cell[VY]);
        }
        else
        {
            // Line slope \ vs / cell diagonal.
            V2d_Set(from, cell[VX] + cellSize[VX], cell[VY] + cellSize[VY]);
            V2d_Set(to,   cell[VX], cell[VY]);
        }

        // Would Line intersect this?
        if((line.pointOnSide(from) < 0) != (line.pointOnSide(to) < 0))
        {
            Blockmap_CreateCellAndLinkObjectXY(lineBlockmap, x, y, &line);
        }
    }
}

struct bmapiterparams_t
{
    int localValidCount;
    int (*callback) (Line *, void *);
    void *parms;
};

static int blockmapCellLinesIterator(void *mapElement, void *context)
{
    Line *line = static_cast<Line *>(mapElement);
    bmapiterparams_t *parms = static_cast<bmapiterparams_t *>(context);

    if(line->validCount() != parms->localValidCount)
    {
        int result;

        // This line has now been processed for the current iteration.
        line->setValidCount(parms->localValidCount);

        // Action the callback.
        result = parms->callback(line, parms->parms);
        if(result) return result; // Stop iteration.
    }

    return false; // Continue iteration.
}

static int iterateCellLines(Blockmap &lineBlockmap, const_BlockmapCell cell,
    int (*callback) (Line *, void *), void *context = 0)
{
    bmapiterparams_t parms;
    parms.localValidCount = validCount;
    parms.callback        = callback;
    parms.parms           = context;

    return Blockmap_IterateCellObjects(&lineBlockmap, cell,
                                       blockmapCellLinesIterator, (void *)&parms);
}

static int iterateCellBlockLines(Blockmap &lineBlockmap, BlockmapCellBlock const *cellBlock,
    int (*callback) (Line *, void *), void *context = 0)
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

    // Degenerate BspLeafs don't get in.
    if(bspLeaf.isDegenerate()) return;

    // BspLeafs without sectors don't get in.
    if(!bspLeaf.hasSector()) return;

    AABoxd aaBox = bspLeaf.face().aaBox();

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
        bspLeaf->setValidCount(args->localValidCount);

        // Check the sector restriction.
        if(args->sector && bspLeaf->sectorPtr() != args->sector)
            ok = false;

        // Check the bounds.
        AABoxd const &leafAABox = bspLeaf->face().aaBox();

        if(args->box &&
           (leafAABox.maxX < args->box->minX ||
            leafAABox.minX > args->box->maxX ||
            leafAABox.minY > args->box->maxY ||
            leafAABox.maxY < args->box->minY))
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
    int (*callback) (BspLeaf *, void *), void *parameters) const
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
    int (*callback) (struct polyobj_s *, void *), void *parameters) const
{
    DENG_ASSERT(polyobjBlockmap != 0);
    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(polyobjBlockmap, &cellBlock, &box);
    return iterateCellBlockPolyobjs(*polyobjBlockmap, cellBlock, callback, parameters);
}

struct poiterparams_t
{
    int (*func) (Line *, void *);
    void *parms;
};

int PTR_PolyobjLines(Polyobj *po, void* context)
{
    poiterparams_t *args = (poiterparams_t *) context;
    foreach(Line *line, po->lines())
    {
        if(line->validCount() == validCount)
            continue;

        line->setValidCount(validCount);
        int result = args->func(line, args->parms);
        if(result) return result;
    }
    return false; // Continue iteration.
}

/*
static int iterateCellPolyobjLinesIterator(Blockmap &polyobjBlockmap, const_BlockmapCell cell,
    int (*callback) (Line *, void *), void *context = 0)
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
    int (*callback) (Line *, void *), void *context = 0)
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
    int (*callback) (Line *, void *), void *parameters) const
{
    DENG_ASSERT(polyobjBlockmap != 0);
    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(lineBlockmap, &cellBlock, &box);
    return iterateCellBlockLines(*lineBlockmap, &cellBlock, callback, parameters);
}

int GameMap::polyobjLinesBoxIterator(AABoxd const &box,
    int (*callback) (Line *, void *), void *parameters) const
{
    DENG_ASSERT(polyobjBlockmap != 0);
    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(polyobjBlockmap, &cellBlock, &box);
    return iterateCellBlockPolyobjLines(*polyobjBlockmap, cellBlock, callback, parameters);
}

int GameMap::allLinesBoxIterator(AABoxd const &box,
    int (*callback) (Line *, void *), void *parameters) const
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
    int (*callback) (Line *, void *);
    void *parms;
};

static int iteratePolyobjLines(Polyobj *po, void *parameters = 0)
{
    iteratepolyobjlines_params_t const *p = (iteratepolyobjlines_params_t *)parameters;
    foreach(Line *line, po->lines())
    {
        if(line->validCount() == validCount)
            continue;

        line->setValidCount(validCount);
        int result = p->callback(line, p->parms);
        if(result) return result;
    }
    return false; // Continue iteration.
}

static int collectPolyobjLineIntercepts(uint const block[2], void *parameters)
{
    Blockmap *polyobjBlockmap = (Blockmap *)parameters;
    iteratepolyobjlines_params_t iplParams;
    iplParams.callback = PIT_AddLineIntercepts;
    iplParams.parms    = 0;
    return iterateCellPolyobjs(*polyobjBlockmap, block,
                               iteratePolyobjLines, (void *)&iplParams);
}

static int collectLineIntercepts(uint const block[2], void *parameters)
{
    Blockmap *lineBlockmap = (Blockmap *)parameters;
    return iterateCellLines(*lineBlockmap, block, PIT_AddLineIntercepts);
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

BspLeaf *GameMap::bspLeafAtPoint(Vector2d const &point) const
{
    MapElement *bspElement = d->bspRoot;
    while(bspElement->type() != DMU_BSPLEAF)
    {
        BspNode const *bspNode = bspElement->castTo<BspNode>();

        int side = bspNode->partition().pointOnSide(point) < 0;

        // Decend to the child subspace on "this" side.
        bspElement = bspNode->childPtr(side);
    }

    // We've arrived at a leaf.
    return bspElement->castTo<BspLeaf>();
}

BspLeaf *GameMap::bspLeafAtPoint_FixedPrecision(Vector2d const &point) const
{
    fixed_t pointX[2] = { DBL2FIX(point.x), DBL2FIX(point.y) };

    MapElement *bspElement = d->bspRoot;
    while(bspElement->type() != DMU_BSPLEAF)
    {
        BspNode const *bspNode = bspElement->castTo<BspNode>();
        Partition const &partition = bspNode->partition();

        fixed_t lineOriginX[2]    = { DBL2FIX(partition.origin.x),    DBL2FIX(partition.origin.y) };
        fixed_t lineDirectionX[2] = { DBL2FIX(partition.direction.x), DBL2FIX(partition.direction.y) };

        int side = V2x_PointOnLineSide(pointX, lineOriginX, lineDirectionX);

        // Decend to the child subspace on "this" side.
        bspElement = bspNode->childPtr(side);
    }

    // We've arrived at a leaf.
    return bspElement->castTo<BspLeaf>();
}

void GameMap::lerpScrollingSurfaces(bool resetNextViewer)
{
    if(resetNextViewer)
    {
        // Reset the surface material origin trackers.
        foreach(Surface *surface, d->scrollingSurfaces)
        {
            surface->resetVisMaterialOrigin();
        }

        // Tracked movement is now all done.
        d->scrollingSurfaces.clear();
    }
    // While the game is paused there is no need to calculate any
    // visual material origin offsets $smoothmaterialorigin.
    else //if(!clientPaused)
    {
        // Set the visible origins.
        QMutableSetIterator<Surface *> iter(d->scrollingSurfaces);
        while(iter.hasNext())
        {
            Surface *surface = iter.next();

            surface->lerpVisMaterialOrigin();

            // Has this material reached its destination?
            if(surface->visMaterialOrigin() == surface->materialOrigin())
            {
                iter.remove();
            }
        }
    }
}

void GameMap::updateScrollingSurfaces()
{
    foreach(Surface *surface, d->scrollingSurfaces)
    {
        surface->updateMaterialOriginTracking();
    }
}

GameMap::SurfaceSet &GameMap::scrollingSurfaces()
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
            if(de::fequal(plane->visHeight(), plane->height()))
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

GameMap::PlaneSet &GameMap::trackedPlanes()
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

#ifdef __CLIENT__

/**
 * Given a side section, look at the neighbouring surfaces and pick the
 * best choice of material used on those surfaces to be applied to "this"
 * surface.
 *
 * Material on back neighbour plane has priority.
 * Non-animated materials are preferred.
 * Sky materials are ignored.
 */
static Material *chooseFixMaterial(Line::Side &side, int section)
{
    Material *choice1 = 0, *choice2 = 0;

    Sector *frontSec = side.sectorPtr();
    Sector *backSec  = side.back().sectorPtr();

    if(backSec)
    {
        // Our first choice is a material in the other sector.
        if(section == Line::Side::Bottom)
        {
            if(frontSec->floor().height() < backSec->floor().height())
            {
                choice1 = backSec->floorSurface().materialPtr();
            }
        }
        else if(section == Line::Side::Top)
        {
            if(frontSec->ceiling().height()  > backSec->ceiling().height())
            {
                choice1 = backSec->ceilingSurface().materialPtr();
            }
        }

        // In the special case of sky mask on the back plane, our best
        // choice is always this material.
        if(choice1 && choice1->isSkyMasked())
        {
            return choice1;
        }
    }
    else
    {
        // Our first choice is a material on an adjacent wall section.
        // Try the left neighbor first.
        Line *other = R_FindLineNeighbor(frontSec, &side.line(), side.line().vertexOwner(side.sideId()),
                                         false /*next clockwise*/);
        if(!other)
            // Try the right neighbor.
            other = R_FindLineNeighbor(frontSec, &side.line(), side.line().vertexOwner(side.sideId()^1),
                                       true /*next anti-clockwise*/);

        if(other)
        {
            if(!other->hasBackSector())
            {
                // Our choice is clear - the middle material.
                choice1 = other->front().middle().materialPtr();
            }
            else
            {
                // Compare the relative heights to decide.
                Line::Side &otherSide = other->side(&other->frontSector() == frontSec? Line::Front : Line::Back);
                Sector &otherSec = other->side(&other->frontSector() == frontSec? Line::Back : Line::Front).sector();

                if(otherSec.ceiling().height() <= frontSec->floor().height())
                    choice1 = otherSide.top().materialPtr();
                else if(otherSec.floor().height() >= frontSec->ceiling().height())
                    choice1 = otherSide.bottom().materialPtr();
                else if(otherSec.ceiling().height() < frontSec->ceiling().height())
                    choice1 = otherSide.top().materialPtr();
                else if(otherSec.floor().height() > frontSec->floor().height())
                    choice1 = otherSide.bottom().materialPtr();
                // else we'll settle for a plane material.
            }
        }
    }

    // Our second choice is a material from this sector.
    choice2 = frontSec->planeSurface(section == Line::Side::Bottom? Plane::Floor : Plane::Ceiling).materialPtr();

    // Prefer a non-animated, non-masked material.
    if(choice1 && !choice1->isAnimated() && !choice1->isSkyMasked())
        return choice1;
    if(choice2 && !choice2->isAnimated() && !choice2->isSkyMasked())
        return choice2;

    // Prefer a non-masked material.
    if(choice1 && !choice1->isSkyMasked())
        return choice1;
    if(choice2 && !choice2->isSkyMasked())
        return choice2;

    // At this point we'll accept anything if it means avoiding HOM.
    if(choice1) return choice1;
    if(choice2) return choice2;

    // We'll assign the special "missing" material...
    return &App_Materials().find(de::Uri("System", Path("missing"))).material();
}

static void addMissingMaterial(Line::Side &side, int section)
{
    // Sides without sections need no fixing.
    if(!side.hasSections()) return;
    // ...nor those of self-referencing lines.
    if(side.line().isSelfReferencing()) return;
    // ...nor those of "one-way window" lines.
    if(!side.back().hasSections() && side.back().hasSector()) return;

    // A material must actually be missing to qualify for fixing.
    Surface &surface = side.surface(section);
    if(surface.hasMaterial()) return;

    // Look for and apply a suitable replacement if found.
    surface.setMaterial(chooseFixMaterial(side, section), true/* is missing fix */);

    // During map load we log missing materials.
    if(ddMapSetup && verbose)
    {
        String path = surface.hasMaterial()? surface.material().manifest().composeUri().asText() : "<null>";

        LOG_WARNING("%s of Line #%d is missing a material for the %s section.\n"
                    "  %s was chosen to complete the definition.")
            << (side.isBack()? "Back" : "Front") << side.line().indexInMap()
            << (section == Line::Side::Middle? "middle" : section == Line::Side::Top? "top" : "bottom")
            << path;
    }
}

void GameMap::updateMissingMaterialsForLinesOfSector(Sector const &sec)
{
    foreach(Line::Side *side, sec.sides())
    {
        /**
         * Do as in the original Doom if the texture has not been defined -
         * extend the floor/ceiling to fill the space (unless it is skymasked),
         * or if there is a midtexture use that instead.
         */
        if(side->hasSector() && side->back().hasSector())
        {
            Sector const &frontSec = side->sector();
            Sector const &backSec  = side->back().sector();

            // A potential bottom section fix?
            if(!(frontSec.floorSurface().hasSkyMaskedMaterial() &&
                  backSec.floorSurface().hasSkyMaskedMaterial()))
            {
                if(frontSec.floor().height() < backSec.floor().height())
                {
                    addMissingMaterial(*side, Line::Side::Bottom);
                }
                else if(frontSec.floor().height() > backSec.floor().height())
                {
                    addMissingMaterial(side->back(), Line::Side::Bottom);
                }
            }

            // A potential top section fix?
            if(!(frontSec.ceilingSurface().hasSkyMaskedMaterial() &&
                  backSec.ceilingSurface().hasSkyMaskedMaterial()))
            {
                if(backSec.ceiling().height() < frontSec.ceiling().height())
                {
                    addMissingMaterial(*side, Line::Side::Top);
                }
                else if(backSec.ceiling().height() > frontSec.ceiling().height())
                {
                    addMissingMaterial(side->back(), Line::Side::Top);
                }
            }
        }
        else
        {
            // A potential middle section fix.
            addMissingMaterial(*side, Line::Side::Middle);
        }
    }
}

#endif // __CLIENT__
