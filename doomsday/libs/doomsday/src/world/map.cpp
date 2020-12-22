/** @file world.cpp  Base for world maps.
 *
 * @authors Copyright � 2014-2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/map.h"
#include "doomsday/world/mobj.h"
#include "doomsday/world/sector.h"
#include "doomsday/world/blockmap.h"
#include "doomsday/world/ithinkermapping.h"
#include "doomsday/world/line.h"
#include "doomsday/world/lineblockmap.h"
#include "doomsday/world/lineowner.h"
#include "doomsday/world/bspleaf.h"
#include "doomsday/world/convexsubspace.h"
#include "doomsday/world/bsp/partitioner.h"
#include "doomsday/world/factory.h"
#include "doomsday/world/thinkers.h"
#include "doomsday/world/thinkerdata.h"
#include "doomsday/world/mobjthinkerdata.h"
#include "doomsday/world/sky.h"
#include "doomsday/world/world.h"
#include "doomsday/mesh/face.h"
#include "doomsday/world/entitydatabase.h"
#include "doomsday/console/var.h"
#include "doomsday/doomsdayapp.h"
#include "doomsday/network/protocol.h"

#include <de/legacy/nodepile.h>
#include <de/legacy/memory.h>
#include <de/legacy/memoryzone.h>
#include <de/charsymbols.h>
#include <de/rectangle.h>
#include <de/logbuffer.h>

using namespace de;

namespace world {

static int bspSplitFactor = 7;  // cvar

/*
 * Additional data for all dummy elements.
 */
struct DummyData
{
    void *extraData; /// Pointer to user data.

    DummyData() : extraData(0) {}
    virtual ~DummyData() {} // polymorphic
};

class DummySector : public Sector, public DummyData {};

class DummyLine : public Line, public DummyData // TODO: enough to be derived from world::Line? (add to Factory?)
{
public:
    DummyLine(Vertex &v1, Vertex &v2) : Line(v1, v2) {}
};

struct DummyGlobals
{
    Set<MapElement *> dummies;
    mesh::Mesh        dummyMesh;

    static inline DummyGlobals &get()
    {
        static DummyGlobals *s_mapGlobals = nullptr;
        if (!s_mapGlobals) s_mapGlobals = new DummyGlobals;
        return *s_mapGlobals;
    }
};

struct EditableElements
{
    List<Line *>    lines;
    List<Sector *>  sectors;
    List<Polyobj *> polyobjs;

    ~EditableElements() { clearAll(); }

    void clearAll()
    {
        deleteAll(lines); lines.clear();
        deleteAll(sectors); sectors.clear();

        for (Polyobj *pob : polyobjs)
        {
            pob->~Polyobj();
            M_Free(pob);
        }
        polyobjs.clear();
    }
};

// Used when sorting vertex line owners.
static Vertex *rootVtx;

DE_PIMPL(Map)
, DE_OBSERVES(Record, Deletion)
, DE_OBSERVES(bsp::Partitioner, UnclosedSectorFound)
{
    struct Bsp
    {
        BspTree *tree = nullptr;  ///< Owns the BspElements.

        ~Bsp() { clear(); }

        void clear()
        {
            if (!tree) return;
            tree->traversePostOrder(clearUserDataWorker);
            delete tree; tree = nullptr;
        }

    private:
        static int clearUserDataWorker(BspTree &subtree, void *)
        {
            delete subtree.userData();
            return 0;
        }
    };

    res::MapManifest *manifest = nullptr; // Not owned, may be @c nullptr.

    double           globalGravity     = 0; // The defined gravity for this map.
    double           effectiveGravity  = 0; // The effective gravity for this map.
    int              ambientLightLevel = 0; // Ambient lightlevel for the current map.
    bool             editingEnabled    = true;
    EditableElements editable;

    mesh::Mesh             mesh; // All map geometries.
    Bsp                    bsp;
    List<ConvexSubspace *> subspaces;      ///< All player-traversable subspaces.
    Hash<Id, Subsector *>  subsectorsById; ///< Not owned.
    AABoxd                 bounds;         ///< Boundary points which encompass the entire map

    //
    // Map entities and element properties (things, line specials, etc...).
    //
    EntityDatabase            entityDatabase;
    std::unique_ptr<Thinkers> thinkers;
    std::unique_ptr<Sky>      sky;
    List<Sector *>            sectors;
    List<Polyobj *>           polyobjs;
    List<Line *>              lines;

    std::unique_ptr<Blockmap>     mobjBlockmap;
    std::unique_ptr<Blockmap>     polyobjBlockmap;
    std::unique_ptr<LineBlockmap> lineBlockmap;
    std::unique_ptr<Blockmap>     subspaceBlockmap;
    nodepile_t                    mobjNodes;
    nodepile_t                    lineNodes;
    nodeindex_t *                 lineLinks = nullptr; ///< Indices to roots.

    Impl(Public *i) : Base(i)
    {
        sky.reset(Factory::newSky(nullptr));
        sky->setMap(thisPublic);
        sky->setIndexInMap(0);
    }

    ~Impl()
    {
        DE_NOTIFY_PUBLIC(Deletion, i) i->mapBeingDeleted(self());
        clearData();
    }
    
    void clearData()
    {
        // Delete thinkers before the map elements, because thinkers may reference them
        // in their private data destructors.
        thinkers.reset();

        deleteAll(sectors);
        sectors.clear();
        
        deleteAll(subspaces);
        
        subspaces.clear();
        for (Polyobj *polyobj : polyobjs)
        {
            polyobj->~Polyobj();
            M_Free(polyobj);
        }
        polyobjs.clear();
        
        deleteAll(lines);
        lines.clear();

        // Note: These free allocations from the memory zone, so this needs to be done before
        // a tag-based purge of the zone.
        mobjBlockmap.reset();
        polyobjBlockmap.reset();
        lineBlockmap.reset();
        subspaceBlockmap.reset();
    }

    void recordBeingDeleted(Record &record)
    {
        // The manifest is not owned by us, it may be deleted by others.
        if (manifest == &record)
        {
            manifest = nullptr;
        }
    }

    /**
     * @pre Axis-aligned bounding boxes of all Sectors must be initialized.
     */
    void updateBounds()
    {
        bool haveGeometry = false;
        for (Line *line : lines)
        {
            // Polyobj lines don't count.
            if (line->definesPolyobj()) continue;

            if (haveGeometry)
            {
                // Expand the bounding box.
                V2d_UniteBox(bounds.arvec2, line->bounds().arvec2);
            }
            else
            {
                // The first line's bounds are used as is.
                V2d_CopyBox(bounds.arvec2, line->bounds().arvec2);
                haveGeometry = true;
            }
        }
    }

    // Observes bsp::Partitioner UnclosedSectorFound.
    void unclosedSectorFound(Sector &sector, const Vec2d &nearPoint)
    {
        // Notify interested parties that an unclosed sector was found.
        DE_NOTIFY_PUBLIC(UnclosedSectorFound, i) i->unclosedSectorFound(sector, nearPoint);
    }

    /**
     * Notify interested parties of a "one-way window" in the map.
     *
     * @param line              The window line.
     * @param backFacingSector  Sector that the back of the line is facing.
     */
    void notifyOneWayWindowFound(Line &line, Sector &backFacingSector)
    {
        DE_NOTIFY_PUBLIC(OneWayWindowFound, i) i->oneWayWindowFound(line, backFacingSector);
    }

    struct testForWindowEffectParams
    {
        double  frontDist      = 0;
        double  backDist       = 0;
        Sector *frontOpen      = nullptr;
        Sector *backOpen       = nullptr;
        Line *  frontLine      = nullptr;
        Line *  backLine       = nullptr;
        Line *  testLine       = nullptr;
        bool    castHorizontal = false;
        Vec2d   testLineCenter;
    };

    static bool lineHasZeroLength(const Line &line)
    {
        return de::abs(line.length()) < 1.0 / 128.0;
    }

    static void testForWindowEffect2(Line &line, testForWindowEffectParams &p)
    {
        namespace bsp = world::bsp;

        if (&line == p.testLine) return;
        if (line.isSelfReferencing()) return;
        if (lineHasZeroLength(line)) return;

        double dist = 0;
        Sector *hitSector = nullptr;
        bool isFront = false;
        if (p.castHorizontal)
        {
            if (de::abs(line.direction().y) < bsp::DIST_EPSILON)
                return;

            if (   (line.bounds().maxY < p.testLineCenter.y - bsp::DIST_EPSILON)
                || (line.bounds().minY > p.testLineCenter.y + bsp::DIST_EPSILON))
                return;

            dist = (line.from().x() + (p.testLineCenter.y - line.from().y()) * line.direction().x / line.direction().y)
                   - p.testLineCenter.x;

            isFront = ((p.testLine->direction().y > 0) != (dist > 0));
            dist = de::abs(dist);

            // Too close? (overlapping lines?)
            if (dist < bsp::DIST_EPSILON)
                return;

            bool dir = (p.testLine->direction().y > 0) ^ (line.direction().y > 0);
            hitSector = line.side(dir ^ !isFront).sectorPtr();
        }
        else  // Cast vertically.
        {
            if (de::abs(line.direction().x) < bsp::DIST_EPSILON)
                return;

            if (   (line.bounds().maxX < p.testLineCenter.x - bsp::DIST_EPSILON)
                || (line.bounds().minX > p.testLineCenter.x + bsp::DIST_EPSILON))
                return;

            dist = (line.from().y() +
                    (p.testLineCenter.x - line.from().x()) * line.direction().y / line.direction().x)
                   - p.testLineCenter.y;

            isFront = ((p.testLine->direction().x > 0) == (dist > 0));
            dist = de::abs(dist);

            bool dir = (p.testLine->direction().x > 0) ^ (line.direction().x > 0);
            hitSector = line.side(dir ^ !isFront).sectorPtr();
        }

        // Too close? (overlapping lines?)
        if (dist < bsp::DIST_EPSILON)
            return;

        if (isFront)
        {
            if (dist < p.frontDist)
            {
                p.frontDist = dist;
                p.frontOpen = hitSector;
                p.frontLine = &line;
            }
        }
        else
        {
            if (dist < p.backDist)
            {
                p.backDist = dist;
                p.backOpen = hitSector;
                p.backLine = &line;
            }
        }
    }

    bool lineMightHaveWindowEffect(const Line &line)
    {
        if (line.definesPolyobj()) return false;
        if (line.front().hasSector() && line.back().hasSector()) return false;
        if (!line.front().hasSector()) return false;
        if (lineHasZeroLength(line)) return false;

        // Look for window effects by checking for an odd number of one-sided
        // line owners for a single vertex. Idea courtesy of Graham Jackson.
        if ((line.from()._onesOwnerCount % 2) == 1
            && (line.from()._onesOwnerCount + line.from()._twosOwnerCount) > 1)
            return true;

        if  ((line.to()._onesOwnerCount % 2) == 1
            && (line.to()._onesOwnerCount + line.to()._twosOwnerCount) > 1)
            return true;

        return false;
    }

    void findOneWayWindows()
    {
        using namespace world::bsp;

        for (auto *vertex : mesh.vertices())
        {
            // Count the total number of one and two-sided line owners for each
            // vertex. (Used in the process of locating window effect lines.)
            vertex->countLineOwners();
        }

        // Search for "one-way window" effects.
        for (Line *line : lines)
        {
            if (!lineMightHaveWindowEffect(*line))
                continue;

            testForWindowEffectParams p;
            p.frontDist      = p.backDist = DDMAXFLOAT;
            p.testLine       = line;
            p.testLineCenter = line->center();
            p.castHorizontal = (de::abs(line->direction().x) < de::abs(line->direction().y)? true : false);

            AABoxd scanRegion = bounds;
            if (p.castHorizontal)
            {
                scanRegion.minY = line->bounds().minY - DIST_EPSILON;
                scanRegion.maxY = line->bounds().maxY + DIST_EPSILON;
            }
            else
            {
                scanRegion.minX = line->bounds().minX - DIST_EPSILON;
                scanRegion.maxX = line->bounds().maxX + DIST_EPSILON;
            }

            World::validCount++;
            self().forAllLinesInBox(scanRegion, LIF_SECTOR, [&p] (Line &line)
                                    {
                                        testForWindowEffect2(line, p);
                                        return LoopContinue;
                                    });

            if (p.backOpen && p.frontOpen && line->front().sectorPtr() == p.backOpen)
            {
                notifyOneWayWindowFound(*line, *p.frontOpen);

                line->_bspWindowSector = p.frontOpen; /// @todo Refactor away.
            }
        }
    }

    /**
     * Build a new BSP tree.
     *
     * @pre Map line bounds have been determined and a line blockmap constructed.
     */
    bool buildBspTree()
    {
        DE_ASSERT(bsp.tree == nullptr);
        DE_ASSERT(subspaces.isEmpty());

        // It begins...
        Time begunAt;

        LOGDEV_MAP_XVERBOSE("Building BSP for \"%s\" with split cost factor %d...",
                            (self().hasManifest() ? self().manifest().composeUri() : "(unknown map)") << bspSplitFactor);

        // First we'll scan for so-called "one-way window" constructs and mark
        // them so that the space partitioner can treat them specially.
        findOneWayWindows();

        // Remember the current next vertex ordinal as we'll need to index any
        // new vertexes produced during the build process.
        int nextVertexOrd = mesh.vertexCount();

        // Determine the set of lines for which we will build a BSP.
        auto linesToBuildFor = compose<Set<Line *>>(lines.begin(), lines.end());

        // Polyobj lines should be excluded.
        for (Polyobj *pob : polyobjs)
        {
            for (Line *line : pob->lines())
            {
                linesToBuildFor.remove(line);
            }
        }

        try
        {
            // Configure a space partitioner.
            world::bsp::Partitioner partitioner(bspSplitFactor);
            partitioner.audienceForUnclosedSectorFound += this;

            // Build a new BSP tree.
            bsp.tree = partitioner.makeBspTree(linesToBuildFor, mesh);
            DE_ASSERT(bsp.tree);

            LOG_MAP_VERBOSE("BSP built: %s. With %d Segments and %d Vertexes.")
                << bsp.tree->summary()
                << partitioner.segmentCount()
                << partitioner.vertexCount();

            // Attribute an index to any new vertexes.
            for (int i = nextVertexOrd; i < mesh.vertexCount(); ++i)
            {
                Vertex *vtx = mesh.vertices().at(i);
                vtx->setMap(thisPublic);
                vtx->setIndexInMap(i);
            }

#ifdef DE_QT_4_7_OR_NEWER
            /// @todo Determine the actual number of subspaces needed.
            subspaces.reserve(bsp.tree->leafCount());
#endif

            // Iterative pre-order traversal of the map element tree.
            const BspTree *cur  = bsp.tree;
            const BspTree *prev = nullptr;
            while (cur)
            {
                while (cur)
                {
                    if (cur->userData())
                    {
                        if (cur->isLeaf())
                        {
                            auto &leaf = cur->userData()->as<BspLeaf>();
                            if (!leaf.sectorPtr())
                            {
                                LOGDEV_MAP_WARNING("BSP leaf %p has degenerate geometry (%d half-edges).")
                                    << &leaf << (leaf.hasSubspace() ? leaf.subspace().poly().hedgeCount() : 0);
                            }

                            if (leaf.hasSubspace())
                            {
                                // Add this subspace to the LUT.
                                ConvexSubspace &subspace = leaf.subspace();
                                subspace.setIndexInMap(subspaces.count());
                                subspaces.append(&subspace);

#ifdef DE_DEBUG  // See if we received a partial geometry...
                                int discontinuities = 0;
                                auto *hedge = subspace.poly().hedge();
                                do
                                {
                                    if (hedge->next().origin() != hedge->twin().origin())
                                    {
                                        discontinuities++;
                                    }
                                } while ((hedge = &hedge->next()) != subspace.poly().hedge());

                                if (discontinuities)
                                {
                                    LOGDEV_MAP_WARNING("Face geometry for BSP leaf [%p] at %s in sector %i "
                                                       "is not contiguous (%i gaps/overlaps).\n%s")
                                        << &leaf << subspace.poly().center().asText()
                                        << (leaf.sectorPtr()? leaf.sectorPtr()->indexInArchive() : -1)
                                        << discontinuities
                                        << subspace.poly().description();
                                }
#endif
                            }
                        }
                    }

                    if (prev == cur->parentPtr())
                    {
                        // Descending - right first, then left.
                        prev = cur;
                        if (cur->hasRight()) cur = cur->rightPtr();
                        else                 cur = cur->leftPtr();
                    }
                    else if (prev == cur->rightPtr())
                    {
                        // Last moved up the right branch - descend the left.
                        prev = cur;
                        cur  = cur->leftPtr();
                    }
                    else if (prev == cur->leftPtr())
                    {
                        // Last moved up the left branch - continue upward.
                        prev = cur;
                        cur  = cur->parentPtr();
                    }
                }

                if (prev)
                {
                    // No left child - back up.
                    cur = prev->parentPtr();
                }
            }
        }
        catch (const Error &er)
        {
            LOG_MAP_WARNING("%s.") << er.asText();
        }

        // How much time did we spend?
        LOGDEV_MAP_VERBOSE("BSP built in %.2f seconds") << begunAt.since();

        return bsp.tree != nullptr;
    }

    /**
     * Build Subsectors for the given @a sector.
     */
    void buildSubsectors(Sector &sector)
    {
        DE_ASSERT(!sector.hasSubsectors());

        // Group the subspaces into sets which share at least one common edge. We'll do
        // this by starting with a set per subspace and then keep merging the sets until
        // no two sets share a common edge ("clustering").

        typedef List<ConvexSubspace *> Subspaces;
        typedef List<Subspaces> SubspaceSets;
        SubspaceSets subspaceSets;

        for (ConvexSubspace *subspace : subspaces)
        {
            if (subspace->bspLeaf().sectorPtr() == &sector)
            {
                subspaceSets.append(Subspaces());
                subspaceSets.last().append(subspace);
            }
        }

        if (subspaceSets.isEmpty()) return;

        // Merge sets whose subspaces share a common edge.
        while (subspaceSets.count() > 1)
        {
            bool didMerge = false;
            for (int i = 0; i < subspaceSets.count(); ++i)
                for (int k = 0; k < subspaceSets.count(); ++k)
                {
                    if (i == k) continue;

                    for (ConvexSubspace *subspace : subspaceSets[i])
                    {
                        auto *baseHEdge = subspace->poly().hedge();
                        auto *hedge = baseHEdge;
                        do
                        {
                            if (hedge->twin().hasFace()
                                && hedge->twin().face().hasMapElement())
                            {
                                auto &otherSubspace = hedge->twin().face().mapElementAs<ConvexSubspace>();
                                if (otherSubspace.bspLeaf().sectorPtr() == &sector
                                    && subspaceSets[k].contains(&otherSubspace))
                                {
                                    // Merge k into i.
                                    subspaceSets[i] += subspaceSets[k];
                                    subspaceSets.removeAt(k);

                                    // Compare the next pair.
                                    if (i >= k) i -= 1;
                                    k -= 1;

                                    // We'll need to repeat in any case.
                                    didMerge = true;
                                    break;
                                }
                            }
                        } while ((hedge = &hedge->next()) != baseHEdge);

                        if (didMerge) break;
                    }
                }

            if (!didMerge) break;
        }
        // Clustering complete.

        // Build subsectors.
        int needed = 0;
        for (const Subspaces &subspaceSet : subspaceSets)
        {
            needed += subspaceSet.count();
        }
        subsectorsById.clear();
        subsectorsById.reserve(needed);
        for (const Subspaces &subspaceSet : subspaceSets)
        {
            Subsector *subsec = sector.addSubsector(subspaceSet);
            DE_ASSERT(subsec != nullptr);
            subsectorsById.insert(subsec->id(), subsec);
        }
    }

    /// @return  @c true= @a mobj was unlinked successfully.
    bool unlinkMobjFromSectors(mobj_t &mob)
    {
        if (Mobj_IsSectorLinked(mob))
        {
            Mobj_Sector(&mob)->unlink(&mob);
            return true;
        }
        return false;
    }

    /**
     * Construct an initial (empty) line blockmap.
     *
     * @pre Coordinate space bounds have already been determined.
     */
    void initLineBlockmap(double margin = 8)
    {
        // Setup the blockmap area to enclose the whole map, plus a margin
        // (margin is needed for a map that fits entirely inside one blockmap cell).
        lineBlockmap.reset(
            new LineBlockmap(AABoxd(bounds.minX - margin, bounds.minY - margin,
                                    bounds.maxX + margin, bounds.maxY + margin)));

        LOG_MAP_VERBOSE("Line blockmap dimensions:")
            << lineBlockmap->dimensions().asText();

        // Populate the blockmap.
        lineBlockmap->link(lines);
    }

    /**
     * Construct an initial (empty) mobj blockmap.
     *
     * @pre Coordinate space bounds have already been determined.
     */
    void initMobjBlockmap(double margin = 8)
    {
        // Setup the blockmap area to enclose the whole map, plus a margin
        // (margin is needed for a map that fits entirely inside one blockmap cell).
        mobjBlockmap.reset(
            new Blockmap(AABoxd(bounds.minX - margin, bounds.minY - margin,
                                bounds.maxX + margin, bounds.maxY + margin)));

        LOG_MAP_VERBOSE("Mobj blockmap dimensions:")
            << mobjBlockmap->dimensions().asText();
    }

    /**
     * Unlinks the mobj from all the lines it's been linked to. Can be called without
     * checking that the list does indeed contain lines.
     */
    bool unlinkMobjFromLines(mobj_t &mo)
    {
        // Try unlinking from lines.
        if (!mo.lineRoot)
            return false; // A zero index means it's not linked.

        // Unlink from each line.
        linknode_t *tn = mobjNodes.nodes;
        for (nodeindex_t nix = tn[mo.lineRoot].next; nix != mo.lineRoot; nix = tn[nix].next)
        {
            // Data is the linenode index that corresponds this mobj.
            NP_Unlink((&lineNodes), tn[nix].data);
            // We don't need these nodes any more, mark them as unused.
            // Dismissing is a macro.
            NP_Dismiss((&lineNodes), tn[nix].data);
            NP_Dismiss((&mobjNodes), nix);
        }

        // The mobj no longer has a line ring.
        NP_Dismiss((&mobjNodes), mo.lineRoot);
        mo.lineRoot = 0;

        return true;
    }

    /**
     * @note Caller must ensure a mobj is linked only once to any given line.
     *
     * @param mob   Map-object to be linked.
     * @param line  Line to link the mobj to.
     */
    void linkMobjToLine(mobj_t *mob, Line *line)
    {
        if (!mob || !line) return;

        // Lines with only one sector will not be linked to because a mobj can't
        // legally cross one.
        if (!line->front().hasSector()) return;
        if (!line->back().hasSector()) return;

        // Add a node to the mobj's ring.
        nodeindex_t nodeIndex = NP_New(&mobjNodes, line);
        NP_Link(&mobjNodes, nodeIndex, mob->lineRoot);

        // Add a node to the line's ring. Also store the linenode's index
        // into the mobjring's node, so unlinking is easy.
        nodeIndex = mobjNodes.nodes[nodeIndex].data = NP_New(&lineNodes, mob);
        NP_Link(&lineNodes, nodeIndex, lineLinks[line->indexInMap()]);
    }

    /**
     * @note Caller must ensure that the map-object @a mob is @em not linked.
     */
    void linkMobjToLines(mobj_t &mob)
    {
        const AABoxd box = Mobj_Bounds(mob);

        // Get a new root node.
        mob.lineRoot = NP_New(&mobjNodes, NP_ROOT_NODE);

        World::validCount++;
        self().forAllLinesInBox(box, [this, &mob, &box] (Line &line)
        {
            // Do the bounding boxes intercept?
            if (!(   box.minX >= line.bounds().maxX
                  || box.minY >= line.bounds().maxY
                  || box.maxX <= line.bounds().minX
                  || box.maxY <= line.bounds().minY))
            {
                // Line crosses the mobj's bounding box?
                if (!line.boxOnSide(box))
                {
                    this->linkMobjToLine(&mob, &line);
                }
            }
            return LoopContinue;
        });
    }

    /**
     * Construct an initial (empty) polyobj blockmap.
     *
     * @pre Coordinate space bounds have already been determined.
     */
    void initPolyobjBlockmap(double margin = 8)
    {
        // Setup the blockmap area to enclose the whole map, plus a margin
        // (margin is needed for a map that fits entirely inside one blockmap cell).
        polyobjBlockmap.reset(
            new Blockmap(AABoxd(bounds.minX - margin, bounds.minY - margin,
                                bounds.maxX + margin, bounds.maxY + margin)));

        LOG_MAP_VERBOSE("Polyobj blockmap dimensions:")
            << polyobjBlockmap->dimensions().asText();
    }

    /**
     * Construct an initial (empty) convex subspace blockmap.
     *
     * @pre Coordinate space bounds have already been determined.
     */
    void initSubspaceBlockmap(double margin = 8)
    {
        // Setup the blockmap area to enclose the whole map, plus a margin
        // (margin is needed for a map that fits entirely inside one blockmap cell).
        subspaceBlockmap.reset(
            new Blockmap(AABoxd(bounds.minX - margin, bounds.minY - margin,
                                bounds.maxX + margin, bounds.maxY + margin)));

        LOG_MAP_VERBOSE("Convex subspace blockmap dimensions:")
            << subspaceBlockmap->dimensions().asText();

        // Populate the blockmap.
        for (ConvexSubspace *subspace : subspaces)
        {
            subspaceBlockmap->link(subspace->poly().bounds(), subspace);
        }
    }

    /**
     * Locate a polyobj by sound emitter.
     *
     * @param soundEmitter  SoundEmitter to search for.
     *
     * @return  Pointer to the referenced Polyobj instance; otherwise @c nullptr.
     */
    Polyobj *polyobjBySoundEmitter(const SoundEmitter &soundEmitter) const
    {
        for (Polyobj *polyobj : polyobjs)
        {
            if (&soundEmitter == &polyobj->soundEmitter())
                return polyobj;
        }
        return nullptr; // Not found.
    }

    /**
     * Locate a sector by sound emitter.
     *
     * @param soundEmitter  SoundEmitter to search for.
     *
     * @return  Pointer to the referenced Sector instance; otherwise @c nullptr.
     */
    Sector *sectorBySoundEmitter(const SoundEmitter &soundEmitter) const
    {
        for (Sector *sector : sectors)
        {
            if (&soundEmitter == &sector->soundEmitter())
                return sector;
        }
        return nullptr; // Not found.
    }

    /**
     * Locate a sector plane by sound emitter.
     *
     * @param soundEmitter  SoundEmitter to search for.
     *
     * @return  Pointer to the referenced Plane instance; otherwise @c nullptr.
     */
    Plane *planeBySoundEmitter(const SoundEmitter &soundEmitter) const
    {
        Plane *found = nullptr;  // Not found.
        for (Sector *sector : sectors)
        {
            LoopResult located = sector->forAllPlanes([&soundEmitter, &found] (Plane &plane)
                                                      {
                                                          if (&soundEmitter == &plane.soundEmitter())
                                                          {
                                                              found = &plane;
                                                              return LoopAbort;
                                                          }
                                                          return LoopContinue;
                                                      });
            if (located) break;
        }
        return found;
    }

    /**
     * Locate a surface by sound emitter.
     *
     * @param soundEmitter  SoundEmitter to search for.
     *
     * @return  Pointer to the referenced Surface instance; otherwise @c nullptr.
     */
    Surface *surfaceBySoundEmitter(const SoundEmitter &soundEmitter) const
    {
        // Perhaps a wall surface?
        for (Line *line : lines)
            for (int i = 0; i < 2; ++i)
            {
                LineSide &side = line->side(i);
                if (!side.hasSections()) continue;

                if (&soundEmitter == &side.middleSoundEmitter())
                {
                    return &side.middle();
                }
                if (&soundEmitter == &side.bottomSoundEmitter())
                {
                    return &side.bottom();
                }
                if (&soundEmitter == &side.topSoundEmitter())
                {
                    return &side.top();
                }
            }

        return nullptr;  // Not found.
    }

    static void setVertexLineOwner(Vertex *vtx, Line *lineptr, LineOwner **storage)
    {
        if (!lineptr) return;

        // Has this line already been registered with this vertex?
        const LineOwner *own = vtx->firstLineOwner();
        while (own)
        {
            if (&own->line() == lineptr)
                return;  // Yes, we can exit.

            own = own->next();
        }

        // Add a new owner.
        vtx->_numLineOwners++;
        LineOwner *newOwner = (*storage)++;

        newOwner->_line = lineptr;
        newOwner->_link[CounterClockwise] = nullptr;

        // Link it in.
        // NOTE: We don't bother linking everything at this stage since we'll
        // be sorting the lists anyway. After which we'll finish the job by
        // setting the prev and circular links.
        // So, for now this is only linked singlely, forward.
        newOwner->_link[Clockwise] = vtx->_lineOwners;
        vtx->_lineOwners = newOwner;

        // Link the line to its respective owner node.
        if (vtx == &lineptr->from())
            lineptr->_vo1 = newOwner;
        else
            lineptr->_vo2 = newOwner;
    }
    
#ifdef DE_DEBUG
    /**
     * Determines whether the specified vertex @a v has a correctly formed line owner ring.
     */
    static bool vertexHasValidLineOwnerRing(Vertex &v)
    {
        const LineOwner *base = v.firstLineOwner();
        const LineOwner *cur = base;
        do
        {
            if (cur->prev()->next() != cur) return false;
            if (cur->next()->prev() != cur) return false;

        } while ((cur = cur->next()) != base);
        return true;
    }
#endif
    
    /**
     * Merge left and right line owner lists into a new list.
     *
     * @return  The newly merged list.
     */
    static LineOwner *mergeLineOwners(LineOwner *left, LineOwner *right,
                                      int (*compare) (LineOwner *, LineOwner *))
    {
        LineOwner tmp;
        LineOwner *np = &tmp;

        tmp._link[Clockwise] = np;
        while (left && right)
        {
            if (compare(left, right) <= 0)
            {
                np->_link[Clockwise] = left;
                np = left;

                left = left->next();
            }
            else
            {
                np->_link[Clockwise] = right;
                np = right;

                right = right->next();
            }
        }

        // At least one of these lists is now empty.
        if (left)
        {
            np->_link[Clockwise] = left;
        }
        if (right)
        {
            np->_link[Clockwise] = right;
        }

        // Is the list empty?
        if (!tmp.hasNext())
            return nullptr;

        return tmp.next();
    }

    static LineOwner *splitLineOwners(LineOwner *list)
    {
        if (!list) return nullptr;

        LineOwner *lista = list;
        LineOwner *listb = list;
        LineOwner *listc = list;

        do
        {
            listc = listb;
            listb = listb->next();
            lista = lista->next();
            if (lista)
            {
                lista = lista->next();
            }
        } while (lista);

        listc->_link[Clockwise] = nullptr;
        return listb;
    }
    
    /**
     * This routine uses a recursive mergesort algorithm; O(NlogN)
     */
    static LineOwner *sortLineOwners(LineOwner *list,
                                     int (*compare) (LineOwner *a, LineOwner *b))
    {
        if (list && list->next())
        {
            LineOwner *p = splitLineOwners(list);

            // Sort both halves and merge them back.
            list = mergeLineOwners(sortLineOwners(list, compare),
                                   sortLineOwners(p, compare), compare);
        }
        return list;
    }
    
    /**
     * Generates the line owner rings for each vertex. Each ring includes all the lines which
     * the vertex belongs to sorted by angle, (the rings are arranged in clockwise order, east = 0).
     */
    static void buildVertexLineOwnerRings(const List<Vertex *> &vertices, List<Line *> &editableLines)
    {
        LOG_AS("buildVertexLineOwnerRings");

        //
        // Step 1: Find and link up all line owners.
        //
        // We know how many vertex line owners we need (numLines * 2).
        auto *lineOwners = (LineOwner *) Z_Malloc(sizeof(LineOwner) * editableLines.count() * 2, PU_MAPSTATIC, 0);
        LineOwner *allocator = lineOwners;

        for (Line *line : editableLines)
            for (int p = 0; p < 2; ++p)
            {
                setVertexLineOwner(&line->vertex(p), line, &allocator);
            }

        //
        // Step 2: Sort line owners of each vertex and finalize the rings.
        //
        for (Vertex *v : vertices)
        {
            if (!v->_numLineOwners) continue;

            // Sort them; ordered clockwise by angle.
            rootVtx = v;
            v->_lineOwners = sortLineOwners(v->_lineOwners, lineAngleSorter);

            // Finish the linking job and convert to relative angles.
            // They are only singly linked atm, we need them to be doubly
            // and circularly linked.
            binangle_t firstAngle = v->_lineOwners->angle();
            LineOwner *last = v->_lineOwners;
            LineOwner *p = last->next();
            while (p)
            {
                p->_link[CounterClockwise] = last;

                // Convert to a relative angle between last and this.
                last->_angle = last->angle() - p->angle();

                last = p;
                p = p->next();
            }
            last->_link[Clockwise] = v->_lineOwners;
            v->_lineOwners->_link[CounterClockwise] = last;

            // Set the angle of the last owner.
            last->_angle = last->angle() - firstAngle;

            /*#ifdef DE_DEBUG
            LOG_MAP_VERBOSE("Vertex #%i: line owners #%i")
                << editmap.vertexes.indexOf(v) << v->lineOwnerCount();

            const LineOwner *base = v->firstLineOwner();
            const LineOwner *cur = base;
            duint idx = 0;
            do
            {
                LOG_MAP_VERBOSE("  %i: p= #%05i this= #%05i n= #%05i, dANG= %-3.f")
                    << idx << cur->prev().line().indexInMap() << cur->line().indexInMap()
                    << cur->next().line().indexInMap() << BANG2DEG(cur->angle());

                idx++;
            } while ((cur = &cur->next()) != base);
    #endif*/

            // Sanity check.
            DE_ASSERT(vertexHasValidLineOwnerRing(*v));
        }
    }

    /**
     * Compares the angles of two lines that share a common vertex.
     *
     * pre: rootVtx must point to the vertex common between a and b
     *      which are (lineowner_t*) ptrs.
     */
    static int lineAngleSorter(LineOwner *a, LineOwner *b)
    {
        binangle_t angles[2];

        world::LineOwner *own[2] = { a, b };
        for (duint i = 0; i < 2; ++i)
        {
            if (own[i]->_link[CounterClockwise]) // We have a cached result.
            {
                angles[i] = own[i]->angle();
            }
            else
            {
                const auto *line = &own[i]->line();
                const auto &otherVtx = line->vertex(&line->from() == rootVtx? 1:0);

                auto dx = otherVtx.origin().x - rootVtx->origin().x;
                auto dy = otherVtx.origin().y - rootVtx->origin().y;

                own[i]->_angle = angles[i] = bamsAtan2(int(-100 * dx), int(100 * dy));

                // Mark as having a cached angle.
                own[i]->_link[CounterClockwise] = (world::LineOwner *) 1;
            }
        }

        return (angles[1] - angles[0]);
    }

    DE_PIMPL_AUDIENCE(OneWayWindowFound)
    DE_PIMPL_AUDIENCE(UnclosedSectorFound)
    DE_PIMPL_AUDIENCE(Deletion)
};

DE_AUDIENCE_METHOD(Map, OneWayWindowFound)
DE_AUDIENCE_METHOD(Map, UnclosedSectorFound)
DE_AUDIENCE_METHOD(Map, Deletion)

Map::Map(res::MapManifest *manifest) : d(new Impl(this))
{
    setManifest(manifest);
}

Map::~Map()
{}

void Map::clearData()
{
    d->clearData();
}

String Map::id() const
{
    if (!hasManifest()) return "";
    return manifest().gets("id");
}

bool Map::hasManifest() const
{
    return d->manifest != nullptr;
}

res::MapManifest &Map::manifest() const
{
    if (hasManifest())
    {
        DE_ASSERT(d->manifest != nullptr);
        return *d->manifest;
    }
    /// @throw MissingResourceManifestError  No associated resource manifest.
    throw MissingResourceManifestError("Map::manifest", "No associated resource manifest");
}

void Map::setManifest(res::MapManifest *newManifest)
{
    if (d->manifest) d->manifest->audienceForDeletion() -= d;

    d->manifest = newManifest;

    if (d->manifest) d->manifest->audienceForDeletion() += d;
}

res::Uri Map::uri() const
{
    return hasManifest() ? manifest().composeUri() : res::makeUri("Maps:");
}

const Record &Map::mapInfo() const
{
    return World::get().mapInfoForMapUri(uri());
}

const mesh::Mesh &Map::mesh() const
{
    return d->mesh;
}

bool Map::hasBspTree() const
{
    return d->bsp.tree != nullptr;
}

const BspTree &Map::bspTree() const
{
    if (d->bsp.tree) return *d->bsp.tree;
    /// @throw MissingBspTreeError  Attempted with no BSP tree available.
    throw MissingBspTreeError("Map::bspTree", "No BSP tree is available");
}

BspLeaf &Map::bspLeafAt(const Vec2d &point) const
{
    if (!d->bsp.tree)
        /// @throw MissingBspTreeError  No BSP data is available.
        throw MissingBspTreeError("Map::bspLeafAt", "No BSP data available");

    const BspTree *bspTree = d->bsp.tree;
    while (!bspTree->isLeaf())
    {
        auto &bspNode = bspTree->userData()->as<BspNode>();
        int side     = bspNode.pointOnSide(point) < 0;

        // Descend to the child subspace on "this" side.
        bspTree = bspTree->childPtr(BspTree::ChildId(side));
    }

    // We've arrived at a leaf.
    return bspTree->userData()->as<BspLeaf>();
}

BspLeaf &Map::bspLeafAt_FixedPrecision(const Vec2d &point) const
{
    if (!d->bsp.tree)
        /// @throw MissingBspTreeError  No BSP data is available.
        throw MissingBspTreeError("Map::bspLeafAt_FixedPrecision", "No BSP data available");

    fixed_t pointX[2] = { DBL2FIX(point.x), DBL2FIX(point.y) };

    const BspTree *bspTree = d->bsp.tree;
    while (!bspTree->isLeaf())
    {
        const auto &bspNode = bspTree->userData()->as<BspNode>();

        fixed_t lineOriginX[2]    = { DBL2FIX(bspNode.origin.x),    DBL2FIX(bspNode.origin.y) };
        fixed_t lineDirectionX[2] = { DBL2FIX(bspNode.direction.x), DBL2FIX(bspNode.direction.y) };
        int side = V2x_PointOnLineSide(pointX, lineOriginX, lineDirectionX);

        // Decend to the child subspace on "this" side.
        bspTree = bspTree->childPtr(BspTree::ChildId(side));
    }

    // We've arrived at a leaf.
    return bspTree->userData()->as<BspLeaf>();
}

EntityDatabase &Map::entityDatabase() const
{
    return d->entityDatabase;
}

void Map::update()
{
    // Reapply values defined in MapInfo (they may have changed).
    const Record &inf = mapInfo();

    d->ambientLightLevel = inf.getf("ambient") * 255;
    d->globalGravity     = inf.getf("gravity");
    d->effectiveGravity  = d->globalGravity;
}

void Map::serializeInternalState(Writer &) const
{}

void Map::deserializeInternalState(Reader &, const IThinkerMapping &)
{}

int Map::lineCount() const
{
    return d->lines.count();
}

Line &Map::line(int index) const
{
    if (Line *li = linePtr(index)) return *li;
    /// @throw MissingElementError  Invalid Line reference specified.
    throw MissingElementError("Map::line", "Unknown Line index:" + String::asText(index));
}

Line *Map::linePtr(int index) const
{
    if (index >= 0 && index < d->lines.count())
    {
        return d->lines.at(index);
    }
    return nullptr;
}

LoopResult Map::forAllLines(const std::function<LoopResult (Line &)>& func) const
{
    for (Line *li : d->lines)
    {
        if (auto result = func(*li)) return result;
    }
    return LoopContinue;
}

LoopResult Map::forAllLinesInBox(const AABoxd &box, int flags,
                                 const std::function<LoopResult (Line &line)> &func) const
{
    LoopResult result = LoopContinue;

    // Process polyobj lines?
    if ((flags & LIF_POLYOBJ) && polyobjCount())
    {
        const int localValidCount = World::validCount;
        result = polyobjBlockmap().forAllInBox(box, [&func, &localValidCount](void *object) {
            auto &pob = *reinterpret_cast<Polyobj *>(object);
            if (pob.validCount != localValidCount) // not yet processed
            {
                pob.validCount = localValidCount;
                for (Line *line : pob.lines())
                {
                    if (line->validCount() != localValidCount) // not yet processed
                    {
                        line->setValidCount(localValidCount);
                        if (auto result = func(*line)) return result;
                    }
                }
            }
            return LoopResult(); // continue
        });
    }

    // Process sector lines?
    if (!result && (flags & LIF_SECTOR))
    {
        const int localValidCount = World::validCount;
        result = lineBlockmap().forAllInBox(box, [&func, &localValidCount](void *object) {
            auto &line = *reinterpret_cast<Line *>(object);
            if (line.validCount() != localValidCount) // not yet processed
            {
                line.setValidCount(localValidCount);
                return func(line);
            }
            return LoopResult(); // continue
        });
    }

    return result;
}

LoopResult Map::forAllMobjsTouchingLine(Line &line, const std::function<LoopResult (mobj_t &)> &func) const
{
    /// @todo Optimize: It should not be necessary to collate the objects first in
    /// in order to perform the iteration. This kind of "belt and braces" safety
    /// measure would not be necessary at this level if the caller(s) instead took
    /// responsibility for managing relationship changes during the iteration. -ds

    if (&line.map() == this)
    {
        List<mobj_t *> linkStore;

        // Collate mobjs touching the given line in case these relationships change.
        linknode_t *ln   = d->lineNodes.nodes;
        nodeindex_t root = d->lineLinks[line.indexInMap()];
        for (nodeindex_t nix = ln[root].next; nix != root; nix = ln[nix].next)
        {
            linkStore << reinterpret_cast<mobj_t *>(ln[nix].ptr);
        }

        for (int i = 0; i < linkStore.sizei(); ++i)
        {
            if (auto result = func(*linkStore[i]))
                return result;
        }
    }
    return LoopContinue;
}

LoopResult Map::forAllMobjsTouchingSector(Sector &sector, const std::function<LoopResult (mobj_t &)>& func) const
{
    /// @todo Optimize: It should not be necessary to collate the objects first in
    /// in order to perform the iteration. This kind of "belt and braces" safety
    /// measure would not be necessary at this level if the caller(s) instead took
    /// responsibility for managing relationship changes during the iteration. -ds

    if (&sector.map() == this)
    {
        List<mobj_t *> linkStore;

        // Collate mobjs that obviously are in the sector.
        for (mobj_t *mob = sector.firstMobj(); mob; mob = mob->sNext)
        {
            if (mob->validCount != World::validCount)
            {
                mob->validCount = World::validCount;
                linkStore << mob;
            }
        }

        // Collate mobjs linked to the sector's lines.
        const linknode_t *ln = d->lineNodes.nodes;
        sector.forAllSides([this, &linkStore, &ln] (LineSide &side)
                           {
                               nodeindex_t root = d->lineLinks[side.line().indexInMap()];
                               for (nodeindex_t nix = ln[root].next; nix != root; nix = ln[nix].next)
                               {
                                   auto *mob = reinterpret_cast<mobj_t *>(ln[nix].ptr);
                                   if (mob->validCount != World::validCount)
                                   {
                                       mob->validCount = World::validCount;
                                       linkStore.append(mob);
                                   }
                               }
                               return LoopContinue;
                           });

        // Process all collected mobjs.
        for (int i = 0; i < linkStore.sizei(); ++i)
        {
            if (auto result = func(*linkStore[i]))
                return result;
        }
    }
    return LoopContinue;
}

LoopResult Map::forAllSectorsTouchingMobj(mobj_t &mob, const std::function<LoopResult (Sector &)>& func) const
{
    /// @todo Optimize: It should not be necessary to collate the objects first in
    /// in order to perform the iteration. This kind of "belt and braces" safety
    /// measure would not be necessary at this level if the caller(s) instead took
    /// responsibility for managing relationship changes during the iteration. -ds

    if (&Mobj_Map(mob) == this && Mobj_IsLinked(mob))
    {
        List<Sector *> linkStore;

        // Always process the mobj's own sector first.
        Sector &ownSec = *Mobj_BspLeafAtOrigin(mob).sectorPtr();
        ownSec.setValidCount(World::validCount);
        linkStore << &ownSec;

        // Any good lines around here?
        if (mob.lineRoot)
        {
            linknode_t *tn = d->mobjNodes.nodes;
            for (nodeindex_t nix = tn[mob.lineRoot].next; nix != mob.lineRoot; nix = tn[nix].next)
            {
                auto *ld = reinterpret_cast<Line *>(tn[nix].ptr);

                // All these lines have sectors on both sides.
                // First, try the front.
                Sector &frontSec = ld->front().sector();
                if (frontSec.validCount() != World::validCount)
                {
                    frontSec.setValidCount(World::validCount);
                    linkStore.append(&frontSec);
                }

                // And then the back.
                /// @todo Above comment suggest always twosided, which is it? -ds
                if (ld->back().hasSector())
                {
                    Sector &backSec = ld->back().sector();
                    if (backSec.validCount() != World::validCount)
                    {
                        backSec.setValidCount(World::validCount);
                        linkStore.append(&backSec);
                    }
                }
            }
        }

        for (int i = 0; i < linkStore.count(); ++i)
        {
            if (auto result = func(*linkStore[i]))
                return result;
        }
    }

    return LoopContinue;
}

LoopResult Map::forAllLinesTouchingMobj(mobj_t &mob, const std::function<LoopResult (Line &)> &func) const
{
    /// @todo Optimize: It should not be necessary to collate the objects first in
    /// in order to perform the iteration. This kind of "belt and braces" safety
    /// measure would not be necessary at this level if the caller(s) instead took
    /// responsibility for managing relationship changes during the iteration. -ds

    if (&Mobj_Map(mob) == this && Mobj_IsLinked(mob) && mob.lineRoot)
    {
        List<Line *> linkStore;

        linknode_t *tn = d->mobjNodes.nodes;
        for (nodeindex_t nix = tn[mob.lineRoot].next; nix != mob.lineRoot; nix = tn[nix].next)
        {
            linkStore.append(reinterpret_cast<Line *>(tn[nix].ptr));
        }

        for (int i = 0; i < linkStore.count(); ++i)
        {
            if (auto result = func(*linkStore[i]))
                return result;
        }
    }
    return LoopContinue;
}

void Map::initPolyobjs()
{
    LOG_AS("Map::initPolyobjs");

    for (Polyobj *po : d->polyobjs)
    {
        /// @todo Is this still necessary? -ds
        /// (This data is updated automatically when moving/rotating).
        po->updateBounds();
        po->updateSurfaceTangents();

        po->unlink();
        po->link();
    }
}

int Map::ambientLightLevel() const
{
    return d->ambientLightLevel;
}

const AABoxd &Map::bounds() const
{
    return d->bounds;
}

coord_t Map::gravity() const
{
    return d->effectiveGravity;
}

void Map::setGravity(coord_t newGravity)
{
    if (!de::fequal(d->effectiveGravity, newGravity))
    {
        d->effectiveGravity = newGravity;
        LOG_MAP_VERBOSE("Effective gravity for %s now %.1f")
            << (hasManifest() ? manifest().gets("id") : "(unknown map)") << d->effectiveGravity;
    }
}

Thinkers &Map::thinkers() const
{
    if (bool( d->thinkers )) return *d->thinkers;
    /// @throw MissingThinkersError  The thinker lists are not yet initialized.
    throw MissingThinkersError("Map::thinkers", "Thinkers not initialized");
}

Sky &Map::sky() const
{
    return *d->sky;
}

int Map::vertexCount() const
{
    return d->mesh.vertexCount();
}

Vertex &Map::vertex(int index) const
{
    if (Vertex *vtx = vertexPtr(index)) return *vtx;
    /// @throw MissingElementError  Invalid Vertex reference specified.
    throw MissingElementError("Map::vertex", "Unknown Vertex index:" + String::asText(index));
}

Vertex *Map::vertexPtr(int index) const
{
    if (index >= 0 && index < d->mesh.vertexCount())
    {
        return d->mesh.vertices().at(index);
    }
    return nullptr;
}

LoopResult Map::forAllVertices(const std::function<LoopResult (Vertex &)>& func) const
{
    for (Vertex *vtx : d->mesh.vertices())
    {
        if (auto result = func(*vtx)) return result;
    }
    return LoopContinue;
}

int Map::sectorCount() const
{
    return d->sectors.count();
}

int Map::subspaceCount() const
{
    return d->subspaces.count();
}

ConvexSubspace &Map::subspace(int index) const
{
    if (ConvexSubspace *sub = subspacePtr(index)) return *sub;
    /// @throw MissingElementError  Invalid ConvexSubspace reference specified.
    throw MissingElementError("Map::subspace", "Unknown subspace index:" + String::asText(index));
}

ConvexSubspace *Map::subspacePtr(int index) const
{
    if (index >= 0 && index < d->subspaces.count())
    {
        return d->subspaces.at(index);
    }
    return nullptr;
}

LoopResult Map::forAllSubspaces(const std::function<LoopResult (ConvexSubspace &)>& func) const
{
    for (ConvexSubspace *sub : d->subspaces)
    {
        if (auto result = func(*sub)) return result;
    }
    return LoopContinue;
}

LineSide &Map::side(int index) const
{
    if (LineSide *side = sidePtr(index)) return *side;
    /// @throw MissingElementError  Invalid LineSide reference specified.
    throw MissingElementError("Map::side", stringf("Unknown LineSide index: %i", index));
}

LineSide *Map::sidePtr(int index) const
{
    if (index < 0) return nullptr;
    return &d->lines.at(index / 2)->side(index % 2);
}

int Map::toSideIndex(int lineIndex, int backSide) // static
{
    DE_ASSERT(lineIndex >= 0);
    return lineIndex * 2 + (backSide? 1 : 0);
}

bool Map::identifySoundEmitter(const SoundEmitter &emitter, Sector **sector,
                               Polyobj **poly, Plane **plane, Surface **surface) const
{
    *sector  = nullptr;
    *poly    = nullptr;
    *plane   = nullptr;
    *surface = nullptr;

    /// @todo Optimize: All sound emitters in a sector are linked together forming
    /// a chain. Make use of the chains instead.

    *poly = d->polyobjBySoundEmitter(emitter);
    if (!*poly)
    {
        // Not a polyobj. Try the sectors next.
        *sector = d->sectorBySoundEmitter(emitter);
        if (!*sector)
        {
            // Not a sector. Try the planes next.
            *plane = d->planeBySoundEmitter(emitter);
            if (!*plane)
            {
                // Not a plane. Try the surfaces next.
                *surface = d->surfaceBySoundEmitter(emitter);
            }
        }
    }

    return (*sector != 0 || *poly != 0|| *plane != 0|| *surface != 0);
}

void Map::initNodePiles()
{
    LOG_AS("Map");

    Time begunAt;

    // Initialize node piles and line rings.
    NP_Init(&d->mobjNodes, 256);  // Allocate a small pile.
    NP_Init(&d->lineNodes, lineCount() + 1000);

    // Allocate the rings.
    DE_ASSERT(d->lineLinks == nullptr);
    d->lineLinks = (nodeindex_t *) Z_Malloc(sizeof(*d->lineLinks) * lineCount(), PU_MAPSTATIC, 0);

    for (int i = 0; i < lineCount(); ++i)
    {
        d->lineLinks[i] = NP_New(&d->lineNodes, NP_ROOT_NODE);
    }

    // How much time did we spend?
    LOGDEV_MAP_MSG("Initialized node piles in %.2f seconds") << begunAt.since();
}

Sector &Map::sector(int index) const
{
    if (Sector *sec = sectorPtr(index)) return *sec;
    /// @throw MissingElementError  Invalid Sector reference specified.
    throw MissingElementError("Map::sector", "Unknown Sector index:" + String::asText(index));
}

Sector *Map::sectorPtr(int index) const
{
    if (index >= 0 && index < d->sectors.count())
    {
        return d->sectors.at(index);
    }
    return nullptr;
}

LoopResult Map::forAllSectors(const std::function<LoopResult (Sector &)> &func) const
{
    for (Sector *sec : d->sectors)
    {
        if (auto result = func(*sec)) return result;
    }
    return LoopContinue;
}

Subsector *Map::subsectorAt(const Vec2d &point) const
{
    BspLeaf &bspLeaf = bspLeafAt(point);
    if (bspLeaf.hasSubspace() && bspLeaf.subspace().contains(point))
    {
        return bspLeaf.subspace().subsectorPtr();
    }
    return nullptr;
}

Subsector &Map::subsector(de::Id id) const
{
    if (Subsector *subsec = subsectorPtr(id)) return *subsec;
    /// @throw MissingElementError  Invalid Sector reference specified.
    throw MissingSubsectorError("Map::subsector", "Unknown Subsector \"" + id.asText() + "\"");
}

Subsector *Map::subsectorPtr(de::Id id) const
{
    auto found = d->subsectorsById.find(id);
    if (found != d->subsectorsById.end())
    {
        return found->second;
    }
    return nullptr;
}

const Blockmap &Map::mobjBlockmap() const
{
    if (bool(d->mobjBlockmap)) return *d->mobjBlockmap;
    /// @throw MissingBlockmapError  The mobj blockmap is not yet initialized.
    throw MissingBlockmapError("Map::mobjBlockmap", "Mobj blockmap is not initialized");
}

const Blockmap &Map::polyobjBlockmap() const
{
    if (bool(d->polyobjBlockmap)) return *d->polyobjBlockmap;
    /// @throw MissingBlockmapError  The polyobj blockmap is not yet initialized.
    throw MissingBlockmapError("Map::polyobjBlockmap", "Polyobj blockmap is not initialized");
}

const LineBlockmap &Map::lineBlockmap() const
{
    if (bool(d->lineBlockmap)) return *d->lineBlockmap;
    /// @throw MissingBlockmapError  The line blockmap is not yet initialized.
    throw MissingBlockmapError("Map::lineBlockmap", "Line blockmap is not initialized");
}

const Blockmap &Map::subspaceBlockmap() const
{
    if (bool(d->subspaceBlockmap)) return *d->subspaceBlockmap;
    /// @throw MissingBlockmapError  The subspace blockmap is not yet initialized.
    throw MissingBlockmapError("Map::subspaceBlockmap", "Convex subspace blockmap is not initialized");
}

int Map::unlink(mobj_t &mob)
{
    int links = 0;

    if (d->unlinkMobjFromSectors(mob))
        links |= MLF_SECTOR;

    BlockmapCell cell = d->mobjBlockmap->toCell(Mobj_Origin(mob));
    if (d->mobjBlockmap->unlink(cell, &mob))
        links |= MLF_BLOCKMAP;

    if (!d->unlinkMobjFromLines(mob))
        links |= MLF_NOLINE;

    return links;
}

void Map::link(mobj_t &mob, int flags)
{
    BspLeaf &bspLeafAtOrigin = bspLeafAt_FixedPrecision(Mobj_Origin(mob));

    // Link into the sector?
    if (flags & MLF_SECTOR)
    {
        d->unlinkMobjFromSectors(mob);
        bspLeafAtOrigin.sectorPtr()->link(&mob);
    }
    mob._bspLeaf = &bspLeafAtOrigin;

    // Link into blockmap?
    if (flags & MLF_BLOCKMAP)
    {
        BlockmapCell cell = d->mobjBlockmap->toCell(Mobj_Origin(mob));
        d->mobjBlockmap->link(cell, &mob);
    }

    // Link into lines?
    if (!(flags & MLF_NOLINE))
    {
        d->unlinkMobjFromLines(mob);
        d->linkMobjToLines(mob);
    }
}

void Map::unlink(Polyobj &polyobj)
{
    d->polyobjBlockmap->unlink(polyobj.bounds, &polyobj);
}

void Map::link(Polyobj &polyobj)
{
    d->polyobjBlockmap->link(polyobj.bounds, &polyobj);
}

int Map::polyobjCount() const
{
    return d->polyobjs.count();
}

Polyobj &Map::polyobj(int index) const
{
    if (Polyobj *pob = polyobjPtr(index)) return *pob;
    /// @throw MissingObjectError  Invalid ConvexSubspace reference specified.
    throw MissingObjectError("Map::subspace", "Unknown Polyobj index:" + String::asText(index));
}

Polyobj *Map::polyobjPtr(int index) const
{
    if (index >= 0 && index < d->polyobjs.count())
    {
        return d->polyobjs.at(index);
    }
    return nullptr;
}

LoopResult Map::forAllPolyobjs(const std::function<LoopResult (Polyobj &)>& func) const
{
    for (Polyobj *pob : d->polyobjs)
    {
        if (auto result = func(*pob)) return result;
    }
    return LoopContinue;
}

//- Runtime map editing -----------------------------------------------------------------

bool Map::isEditable() const
{
    return d->editingEnabled;
}

struct VertexInfo
{
    Vertex *vertex = nullptr;  ///< Vertex for this info.
    Vertex *equiv  = nullptr;  ///< Determined equivalent vertex.
    duint refCount = 0;        ///< Line -> Vertex reference count.

    /// @todo Math here is not correct (rounding directionality). -ds
    int compareVertexOrigins(const VertexInfo &other) const
    {
        DE_ASSERT(vertex && other.vertex);

        if (this == &other) return 0;
        if (vertex == other.vertex) return 0;

        // Order is firstly X axis major.
        if (int(vertex->origin().x) != int(other.vertex->origin().x))
        {
            return int(vertex->origin().x) - int(other.vertex->origin().x);
        }

        // Order is secondly Y axis major.
        return int(vertex->origin().y) - int(other.vertex->origin().y);
    }

    bool operator < (const VertexInfo &other) const
    {
        return compareVertexOrigins(other) < 0;
    }
};

static void pruneVertexes(mesh::Mesh &mesh, const Map::Lines &lines)
{
    //
    // Step 1 - Find equivalent vertexes:
    //
    // Populate the vertex info.
    List<VertexInfo> vertexInfo(mesh.vertexCount());
    int ord = 0;
    for (Vertex *vertex : mesh.vertices())
    {
        vertexInfo[ord++].vertex = vertex;
    }

    {
        // Sort a copy to place near vertexes adjacently.
        List<VertexInfo> sortedInfo(vertexInfo);
        sortedInfo.sort();

        // Locate equivalent vertexes in the sorted info.
        for (int i = 0; i < sortedInfo.count() - 1; ++i)
        {
            VertexInfo &a = sortedInfo[i];
            VertexInfo &b = sortedInfo[i + 1];

            // Are these equivalent?
            /// @todo fixme: What about polyobjs? They need unique vertexes! -ds
            if (a.compareVertexOrigins(b) == 0)
            {
                b.equiv = (a.equiv? a.equiv : a.vertex);
            }
        }
    }

    //
    // Step 2 - Replace line references to equivalent vertexes:
    //
    // Count line -> vertex references.
    for (Line *line : lines)
    {
        vertexInfo[line->from().indexInMap()].refCount++;
        vertexInfo[  line->to().indexInMap()].refCount++;
    }

    // Perform the replacement.
    for (Line *line : lines)
    {
        while (vertexInfo[line->from().indexInMap()].equiv)
        {
            VertexInfo &info = vertexInfo[line->from().indexInMap()];

            info.refCount--;
            line->replaceVertex(Line::From, *info.equiv);

            vertexInfo[line->from().indexInMap()].refCount++;
        }

        while (vertexInfo[line->to().indexInMap()].equiv)
        {
            VertexInfo &info = vertexInfo[line->to().indexInMap()];

            info.refCount--;
            line->replaceVertex(Line::To, *info.equiv);

            vertexInfo[line->to().indexInMap()].refCount++;
        }
    }

    //
    // Step 3 - Prune vertexes:
    //
    int prunedCount = 0, numUnused = 0;
    for (const VertexInfo &info : vertexInfo)
    {
        Vertex *vertex = info.vertex;

        if (info.refCount) continue;

        mesh.removeVertex(*vertex);

        prunedCount += 1;
        if (!info.equiv) numUnused += 1;
    }

    if (prunedCount)
    {
        // Re-index with a contiguous range of indices.
        int ord = 0;
        for (Vertex *vertex : mesh.vertices())
        {
            vertex->setIndexInMap(ord++);
        }

        LOGDEV_MAP_NOTE("Pruned %d vertexes (%d equivalents, %d unused)")
            << prunedCount << (prunedCount - numUnused) << numUnused;
    }
}

bool Map::endEditing()
{
    if (!d->editingEnabled) return true; // Huh?

    d->editingEnabled = false;

    LOG_AS("Map");
    LOG_MAP_VERBOSE("Editing ended");
    LOGDEV_MAP_VERBOSE("New elements: %d Vertexes, %d Lines, %d Polyobjs and %d Sectors")
        << d->mesh.vertexCount()        << d->editable.lines.count()
        << d->editable.polyobjs.count() << d->editable.sectors.count();

    //
    // Perform cleanup on the new map elements.
    //
    pruneVertexes(d->mesh, d->editable.lines);

    // Ensure all map Lines with only one Sector are flagged as blocking.
    for (Line *line : d->editable.lines)
    {
        if (!line->front().hasSector() || !line->back().hasSector())
            line->setFlags(DDLF_BLOCKING);
    }

    Impl::buildVertexLineOwnerRings(d->mesh.vertices(), d->editable.lines);

    //
    // Move the editable elements to the "static" element lists.
    //
    // Collate sectors:
    DE_ASSERT(d->sectors.isEmpty());
    d->sectors.reserve(d->editable.sectors.count());
    d->sectors.append(d->editable.sectors);
    d->editable.sectors.clear();

    // Collate lines:
    DE_ASSERT(d->lines.isEmpty());
    d->lines.reserve(d->editable.lines.count());
    d->lines.append(d->editable.lines);
    d->editable.lines.clear();

    // Collate polyobjs:
    DE_ASSERT(d->polyobjs.isEmpty());
    d->polyobjs.reserve(d->editable.polyobjs.count());
    while (!d->editable.polyobjs.isEmpty())
    {
        d->polyobjs.append(d->editable.polyobjs.takeFirst());
        Polyobj *polyobj = d->polyobjs.back();

        // Create half-edge geometry and line segments for each line.
        for (Line *line : polyobj->lines())
        {
            auto *hedge = polyobj->mesh().newHEdge(line->from());

            hedge->setTwin(polyobj->mesh().newHEdge(line->to()));
            hedge->twin().setTwin(hedge);

            LineSideSegment *seg = line->front().addSegment(*hedge);
            seg->setLength(line->length());
        }

        polyobj->buildUniqueVertexes();
        polyobj->updateOriginalVertexCoords();
    }

    // Determine the map bounds.
    d->updateBounds();
    LOG_MAP_VERBOSE("Geometry bounds:")
        << Rectangled(Vec2d(d->bounds.min), Vec2d(d->bounds.max)).asText();

    // Build a line blockmap.
    d->initLineBlockmap();

    // Build a new BspTree.
    if (!d->buildBspTree())
        return false;

    // The mobj and polyobj blockmaps are maintained dynamically.
    d->initMobjBlockmap();
    d->initPolyobjBlockmap();

    // Finish lines.
    for (Line *line : d->lines)
    {
        line->forAllSides([] (LineSide &side)
                          {
                              side.updateAllSurfaceNormals();
                              return LoopContinue;
                          });
    }

    // Finish sectors.
    for (Sector *sector : d->sectors)
    {
        d->buildSubsectors(*sector);
        sector->buildSides();
        sector->chainSoundEmitters();
    }

    // Finish planes.
    for (Sector *sector : d->sectors)
    {
        sector->forAllPlanes([](Plane &plane) {
            plane.updateSoundEmitterOrigin();
            return LoopContinue;
        });
    }

    // We can now initialize the subspace blockmap.
    d->initSubspaceBlockmap();

    // Prepare the thinker lists.
    d->thinkers.reset(new Thinkers);

    return true;
}

Vertex *Map::createVertex(const Vec2d &origin, int archiveIndex)
{
    if (!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::createVertex", "Editing is not enabled");

    Vertex *vtx = d->mesh.newVertex(origin);

    vtx->setMap(this);
    vtx->setIndexInArchive(archiveIndex);

    /// @todo Don't do this here.
    vtx->setIndexInMap(d->mesh.vertexCount() - 1);

    return vtx;
}

Line *Map::createLine(Vertex &v1, Vertex &v2, int flags, Sector *frontSector,
                      Sector *backSector, int archiveIndex)
{
    if (!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::createLine", "Editing is not enabled");

    auto *line = Factory::newLine(v1, v2, flags, frontSector, backSector);
    d->editable.lines.append(line);

    line->setMap(this);
    line->setIndexInArchive(archiveIndex);

    /// @todo Don't do this here.
    line->setIndexInMap(d->editable.lines.count() - 1);
    line->front().setIndexInMap(Map::toSideIndex(line->indexInMap(), Line::Front));
    line->back ().setIndexInMap(Map::toSideIndex(line->indexInMap(), Line::Back));

    return line;
}

Sector *Map::createSector(float lightLevel, const Vec3f &lightColor, int archiveIndex,
                          const struct de_api_sector_hacks_s *hacks)
{
    if (!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::createSector", "Editing is not enabled");

    auto *sector = new Sector(lightLevel, lightColor);
    d->editable.sectors.append(sector);

    sector->setMap(this);
    sector->setIndexInArchive(archiveIndex);

    // Render hacks.
    if (hacks)
    {
        applySectorHacks(*sector, hacks);
    }

    sector->setIndexInMap(d->editable.sectors.count() - 1);

    return sector;
}

void Map::applySectorHacks(Sector &, const struct de_api_sector_hacks_s *)
{}

Polyobj *Map::createPolyobj(const Vec2d &origin)
{
    if (!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::createPolyobj", "Editing is not enabled");

    Polyobj *pob = Factory::newPolyobj(origin);
    d->editable.polyobjs.append(pob);

    pob->setIndexInMap(d->editable.polyobjs.count() - 1);

    return pob;
}

const Map::Lines &Map::editableLines() const
{
    if (!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::editableLines", "Editing is not enabled");
    return d->editable.lines;
}

const Map::Sectors &Map::editableSectors() const
{
    if (!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::editableSectors", "Editing is not enabled");
    return d->editable.sectors;
}

void Map::initMapOutlinePacket(network::MapOutlinePacket &packet)
{
    forAllLines([&packet](Line &line) {
        packet.addLine(line.from().origin().toVec2i(),
                       line.to().origin().toVec2i(),
                       (line.front().hasSector() && line.back().hasSector())
                           ? network::MapOutlinePacket::TwoSidedLine
                           : network::MapOutlinePacket::OneSidedLine);
        return LoopContinue;
    });
}

const Map::Polyobjs &Map::editablePolyobjs() const
{
    if (!d->editingEnabled)
    {
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::editablePolyobjs", "Editing is not enabled");
    }
    return d->editable.polyobjs;
}

String Map::objectSummaryAsStyledText() const
{
    int thCountInStasis = 0;
    int thCount = thinkers().count(&thCountInStasis);
    String str;

#define TABBED(count, label) Stringf(_E(Ta) "  %i " _E(Tb) "%s\n", count, label)
    if (thCount) str += TABBED(thCount, stringf("Thinkers (%i in stasis)", thCountInStasis).c_str());
#undef TABBED

    return str.rightStrip();
}

String Map::elementSummaryAsStyledText() const
{
    String str;

#define TABBED(count, label) Stringf(_E(Ta) "  %i " _E(Tb) "%s\n", count, label)
    if (lineCount())    str += TABBED(lineCount(),    "Lines");
    //if (sideCount())    str += TABBED(sideCount(),    "Sides");
    if (sectorCount())  str += TABBED(sectorCount(),  "Sectors");
    if (vertexCount())  str += TABBED(vertexCount(),  "Vertexes");
    if (polyobjCount()) str += TABBED(polyobjCount(), "Polyobjs");
#undef TABBED

    return str.rightStrip();
}

String Map::objectsDescription() const
{
    auto &gx = DoomsdayApp::plugins().gameExports();

    String str;
    if (gx.MobjStateAsInfo)
    {
        // Print out a state description for each thinker.
        thinkers().forAll(0x3, [&gx, &str](thinker_t *th) {
            if (Thinker_IsMobj(th))
            {
                str += gx.MobjStateAsInfo(reinterpret_cast<const mobj_t *>(th));
            }
            return LoopContinue;
        });
    }
    return str;
}

void Map::restoreObjects(const Info &objState, const world::IThinkerMapping &thinkerMapping) const
{
    /// @todo Generalize from mobjs to all thinkers?
    LOG_AS("Map::restoreObjects");

    auto &gx = DoomsdayApp::app().plugins().gameExports();

    if (!gx.MobjStateAsInfo || !gx.MobjRestoreState) return;

    bool problemsDetected = false;

    // Look up all the mobjs.
    List<const thinker_t *> mobjs;
    thinkers().forAll(0x3, [&mobjs] (thinker_t *th) {
        if (Thinker_IsMobj(th)) mobjs << th;
        return LoopContinue;
    });

    // Check that all objects are found in the state description.
    if (objState.root().contents().size() != mobjs.size())
    {
        LOGDEV_MAP_WARNING("Different number of objects: %i in map, but got %i in restore data")
            << mobjs.size()
            << objState.root().contents().size();
    }

    // Check the cross-references.
    for (auto i  = objState.root().contentsInOrder().begin();
         i != objState.root().contentsInOrder().end();
         ++i)
    {
        const Info::BlockElement &state = (*i)->as<Info::BlockElement>();
        const Id::Type privateId = state.name().toUInt32();
        DE_ASSERT(privateId != 0);

        if (thinker_t *th = thinkerMapping.thinkerForPrivateId(privateId))
        {
            if (ThinkerData *found = ThinkerData::find(privateId))
            {
                DE_ASSERT(&found->thinker() == th);

                // Restore the state according to the serialized info.
                gx.MobjRestoreState(found->as<MobjThinkerData>().mobj(), state);

#if defined (DE_DEBUG)
                {
                    // Verify that the state is now correct.
                    Info const currentDesc(gx.MobjStateAsInfo(found->as<MobjThinkerData>().mobj()));
                    const Info::BlockElement &currentState =
                        currentDesc.root().contentsInOrder().first()->as<Info::BlockElement>();
                    DE_ASSERT(currentState.name() == state.name());
                    for (const auto &i : state.contents())
                    {
                        if (state.keyValue(i.first).text != currentState.keyValue(i.first).text)
                        {
                            problemsDetected = true;
                            const String msg =
                                Stringf("Object %u has mismatching '%s' (current:%s != arch:%s)",
                                        privateId,
                                        i.first.c_str(),
                                        currentState.keyValue(i.first).text.c_str(),
                                        state.keyValue(i.first).text.c_str());
                            LOGDEV_MAP_WARNING("%s") << msg;
                        }
                    }
                }
#endif
            }
            else
            {
                LOGDEV_MAP_ERROR("Map does not have a thinker matching ID 0x%x")
                    << privateId;
            }
        }
        else
        {
            LOGDEV_MAP_ERROR("Thinker mapping does not have a thinker matching ID 0x%x")
                << privateId;
        }
    }

    if (problemsDetected)
    {
        LOG_MAP_WARNING("Map objects were not fully restored " DE_CHAR_MDASH
                        " gameplay may be affected (enable Developer log entries for details)");
    }
    else
    {
        LOGDEV_MAP_MSG("State of map objects has been restored");
    }
}

void Map::initDummyElements() // static
{
    // TODO: free existing/old dummies here?

    auto &g = DummyGlobals::get();
    g.dummies.clear();
    g.dummyMesh.clear();
}

int Map::dummyElementType(const void *dummy)
{
    const MapElement *elem = reinterpret_cast<const MapElement *>(dummy);
    if (!dynamic_cast<const DummyData *>(elem))
    {
        // Not a dummy.
        return DMU_NONE;
    }
    DE_ASSERT(DummyGlobals::get().dummies.contains(const_cast<MapElement *>(elem)));
    return elem->type();
}

void *Map::createDummyElement(int type, void *extraData)
{
    auto &g = DummyGlobals::get();
    switch(type)
    {
        case DMU_LINE:
        {
            // Time to allocate the dummy vertex?
            if(g.dummyMesh.verticesIsEmpty())
            {
                g.dummyMesh.newVertex();
            }
            Vertex &dummyVertex = *g.dummyMesh.vertices().first();
            DummyLine *dl = new DummyLine(dummyVertex, dummyVertex);
            g.dummies.insert(dl);
            dl->extraData = extraData;
            return dl;
        }
        case DMU_SECTOR:
        {
            DummySector *ds = new DummySector;
            g.dummies.insert(ds);
            ds->extraData = extraData;
            return ds;
        }
        default:
            break;
    }
    throw Error("Map::createDummyElement",
                stringf("Dummies of type %s are not supported", DMU_Str(type)));
}

void Map::destroyDummyElement(void *mapElement)
{
    MapElement *elem = reinterpret_cast<MapElement *>(mapElement);

    const int type = dummyElementType(mapElement);
    if (type == DMU_NONE)
    {
        throw Error("Map::destroyDummyMapElement", "Dummy is of unknown type");
    }

    DE_ASSERT(DummyGlobals::get().dummies.contains(elem));

    DummyGlobals::get().dummies.remove(elem);
    delete elem;
}

void *Map::dummyElementExtraData(void *mapElement)
{
    if (dummyElementType(mapElement) != DMU_NONE)
    {
        MapElement *elem = reinterpret_cast<MapElement *>(mapElement);
        return maybeAs<DummyData>(elem)->extraData;
    }
    return nullptr;
}

D_CMD(InspectMap)
{
    DE_UNUSED(src, argc, argv);

    LOG_AS("inspectmap (Cmd)");

    if (!World::get().hasMap())
    {
        LOG_SCR_WARNING("No map is currently loaded");
        return false;
    }

    Map &map = World::get().map();

    LOG_SCR_NOTE(_E(b) "%s - %s")
        << CVar_String(Con_FindVariable("map-name"))
        << CVar_String(Con_FindVariable("map-author"));
    LOG_SCR_MSG("\n");

    LOG_SCR_MSG(    _E(l) "Uri: "    _E(.) _E(i) "%s" _E(.)
                /*" " _E(l) " OldUid: " _E(.) _E(i) "%s" _E(.)*/
                _E(l) " Music: "  _E(.) _E(i) "%i")
        << (map.hasManifest() ? map.manifest().composeUri().asText() : "(unknown map)")
        /*<< map.oldUniqueId()*/
        << CVar_Integer(Con_FindVariable("map-music"));

    if (map.hasManifest() && map.manifest().sourceFile()->hasCustom())
    {
        LOG_SCR_MSG(_E(l) "Source: " _E(.) _E(i) "\"%s\"")
            << NativePath(map.manifest().sourceFile()->composePath()).pretty();
    }

    LOG_SCR_MSG("\n");

    if (map.isEditable())
    {
        LOG_MSG(_E(D) "Editing " _E(b) "Enabled");
    }

    LOG_SCR_MSG(_E(D) "Elements:");
    LOG_SCR_MSG("%s") << map.elementSummaryAsStyledText();

    if (map.thinkers().isInited())
    {
        LOG_SCR_MSG(_E(D) "Objects:");
        LOG_SCR_MSG("%s") << map.objectSummaryAsStyledText();
    }

    LOG_SCR_MSG(_E(R) "\n");

    auto geometryDimensions = Vec2d(map.bounds().max) - Vec2d(map.bounds().min);
    LOG_SCR_MSG(_E(l) "Geometry dimensions: " _E(.) _E(i)) << geometryDimensions.asText();

    if (map.hasBspTree())
    {
        LOG_SCR_MSG(_E(l) "BSP: " _E(.) _E(i)) << map.bspTree().summary();
    }

    if (!map.subspaceBlockmap().isNull())
    {
        LOG_SCR_MSG(_E(l) "Subspace blockmap: " _E(.) _E(i)) << map.subspaceBlockmap().dimensions().asText();
    }
    if (!map.lineBlockmap().isNull())
    {
        LOG_SCR_MSG(_E(l) "Line blockmap: "     _E(.) _E(i)) << map.lineBlockmap().dimensions().asText();
    }
    if (!map.mobjBlockmap().isNull())
    {
        LOG_SCR_MSG(_E(l) "Mobj blockmap: "     _E(.) _E(i)) << map.mobjBlockmap().dimensions().asText();
    }
    if (!map.polyobjBlockmap().isNull())
    {
        LOG_SCR_MSG(_E(l) "Polyobj blockmap: "  _E(.) _E(i)) << map.polyobjBlockmap().dimensions().asText();
    }

    return true;

#undef TABBED
}

void Map::consoleRegister() // static
{
    Line::consoleRegister();
    Sector::consoleRegister();

    C_VAR_INT("bsp-factor", &bspSplitFactor, CVF_NO_MAX, 0, 0);

    C_CMD("inspectmap", "", InspectMap);
}

} // namespace world
