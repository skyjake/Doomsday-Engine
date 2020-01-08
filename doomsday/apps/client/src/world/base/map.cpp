/** @file map.cpp  World map.
 *
 * @todo This file has grown far too large. It should be split up through the
 * introduction of new abstractions / collections.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "world/map.h"

#ifdef __CLIENT__
#  include "clientapp.h"
#  include "client/cl_mobj.h"
#endif

#include "api_console.h"
#include "api_mapedit.h"
#ifdef __CLIENT__
#  include "api_sound.h"
#endif

#include "world/bsp/partitioner.h"
#include "world/clientserverworld.h"  // ddMapSetup, validCount
#include "world/blockmap.h"
#include "world/lineblockmap.h"
#include "world/lineowner.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "world/polyobjdata.h"
#include "world/sky.h"
#include "world/thinkers.h"
#include "BspLeaf"
#include "ConvexSubspace"
#include "Face"
#include "Line"
#include "Polyobj"
#include "Sector"
#include "Subsector"
#include "Surface"
#include "Vertex"
#ifdef __CLIENT__
#  include "client/clientsubsector.h"
#  include "client/clskyplane.h"
#endif

#ifdef __CLIENT__
#  include "Contact"
#  include "ContactSpreader"

//#  include "BiasTracker"
#  include "LightDecoration"
#  include "Lumobj"
#  include "WallEdge"
#  include "render/viewports.h"
#  include "render/rend_main.h"
#  include "render/rend_particle.h"
#endif

#include <doomsday/defs/mapinfo.h>
#include <doomsday/defs/sky.h>
#include <doomsday/EntityDatabase>
#include <doomsday/BspNode>
#include <doomsday/world/Materials>

#include <de/LogBuffer>
#include <de/Rectangle>

#include <de/aabox.h>
#include <de/charsymbols.h>
#include <de/nodepile.h>
#include <de/vector1.h>
#include <de/timer.h>

#include <array>
#include <QBitArray>
#include <QMultiMap>
#include <QVarLengthArray>

using namespace de;

static dint bspSplitFactor = 7;  ///< cvar

#ifdef __CLIENT__
#if 0
static dint lgMXSample = 1;  ///< 5 samples per block.
#endif

/// Milliseconds it takes for Unpredictable and Hidden mobjs to be
/// removed from the hash. Under normal circumstances, the special
/// status should be removed fairly quickly.
#define CLMOBJ_TIMEOUT  4000
#endif

namespace world {

struct EditableElements
{
    QList<Line *> lines;
    QList<Sector *> sectors;
    QList<Polyobj *> polyobjs;

    ~EditableElements() { clearAll(); }

    void clearAll()
    {
        qDeleteAll(lines); lines.clear();
        qDeleteAll(sectors); sectors.clear();

        for (Polyobj *pob : polyobjs)
        {
            pob->~Polyobj();
            M_Free(pob);
        }
        polyobjs.clear();
    }
};

DENG2_PIMPL(Map)
, DENG2_OBSERVES(bsp::Partitioner, UnclosedSectorFound)
#ifdef __CLIENT__
, DENG2_OBSERVES(ThinkerData, Deletion)
#endif
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
        static dint clearUserDataWorker(BspTree &subtree, void *)
        {
            delete subtree.userData();
            return 0;
        }
    };

#ifdef __CLIENT__
    /**
     * All (particle) generators.
     */
    struct Generators
    {
        struct ListNode
        {
            ListNode *next;
            Generator *gen;
        };

        std::array<Generator *, MAX_GENERATORS> activeGens;

        // We can link 64 generators each into four lists each before running out of links.
        static dint const LINKSTORE_SIZE = 4 * MAX_GENERATORS;
        ListNode *linkStore = nullptr;
        duint linkStoreCursor = 0;

        duint listsSize = 0;
        // Array of list heads containing links from linkStore to generators in activeGens.
        ListNode **lists = nullptr;

        ~Generators()
        {
            Z_Free(lists);
            Z_Free(linkStore);
        }

        /**
         * Resize the collection.
         *
         * @param listCount  Number of lists the collection must support.
         */
        void resize(duint listCount)
        {
            if (!linkStore)
            {
                linkStore = (ListNode *) Z_Malloc(sizeof(*linkStore) * LINKSTORE_SIZE, PU_MAP, 0);
                linkStoreCursor = 0;
                activeGens.fill(nullptr);
            }

            listsSize = listCount;
            lists = (ListNode **) Z_Realloc(lists, sizeof(ListNode *) * listsSize, PU_MAP);
        }

        /**
         * Returns an unused link from the linkStore.
         */
        ListNode *newLink()
        {
            if (linkStoreCursor < (unsigned)LINKSTORE_SIZE)
            {
                return &linkStore[linkStoreCursor++];
            }
            LOG_MAP_WARNING("Exhausted generator link storage");
            return nullptr;
        }
    };

    struct ContactBlockmap : public Blockmap
    {
        QBitArray spreadBlocks;  ///< Used to prevent repeat processing.

        /**
         * Construct a new contact blockmap.
         *
         * @param bounds    Map space boundary.
         * @param cellSize  Width and height of a cell in map space units.
         */
        ContactBlockmap(AABoxd const &bounds, duint cellSize = 128)
            : Blockmap(bounds, cellSize)
            , spreadBlocks(width() * height())
        {}

        void clear()
        {
            spreadBlocks.fill(false);
            unlinkAll();
        }

        /**
         * @param contact  Contact to be linked. Note that if the object's origin
         *                 lies outside the blockmap it will not be linked!
         */
        void link(Contact &contact)
        {
            bool outside;
            BlockmapCell cell = toCell(contact.objectOrigin(), &outside);
            if (!outside)
            {
                Blockmap::link(cell, &contact);
            }
        }

        void spread(AABoxd const &region)
        {
            spreadContacts(*this, region, &spreadBlocks);
        }
    };

#if 0
    struct BiasData
    {
        duint currentTime = 0;        ///< The "current" frame in milliseconds.
        duint lastChangeOnFrame = 0;
        QList<BiasSource *> sources;  ///< All bias light sources (owned).
    };
#endif

#endif // __CLIENT__

    bool editingEnabled = true;
    EditableElements editable;

    AABoxd bounds;              ///< Boundary points which encompass the entire map

    Mesh mesh;                  ///< All map geometries.
    QList<Sector *> sectors;
    QList<Line *> lines;
    QList<Polyobj *> polyobjs;

    Bsp bsp;
    QVector<ConvexSubspace *> subspaces;     ///< All player-traversable subspaces.
    QHash<Id, Subsector *> subsectorsById; ///< Not owned.

    //
    // Map entities and element properties (things, line specials, etc...).
    //
    std::unique_ptr<Thinkers> thinkers;
    Sky sky;

    std::unique_ptr<Blockmap> mobjBlockmap;
    std::unique_ptr<Blockmap> polyobjBlockmap;
    std::unique_ptr<LineBlockmap> lineBlockmap;
    std::unique_ptr<Blockmap> subspaceBlockmap;
#ifdef __CLIENT__
    std::unique_ptr<ContactBlockmap> mobjContactBlockmap;  /// @todo Redundant?
    std::unique_ptr<ContactBlockmap> lumobjContactBlockmap;
#endif

    nodepile_t mobjNodes;
    nodepile_t lineNodes;
    nodeindex_t *lineLinks = nullptr;  ///< Indices to roots.

#ifdef __CLIENT__
    PlaneSet trackedPlanes;
    SurfaceSet scrollingSurfaces;
    SkyDrawable::Animator skyAnimator;
    std::unique_ptr<Generators> generators;
#if 0
    std::unique_ptr<LightGrid> lightGrid;
    BiasData bias;            ///< Map wide "global" data for Bias lighting.
#endif
    QList<Lumobj *> lumobjs;  ///< All lumobjs (owned).

    ClSkyPlane skyFloor;
    ClSkyPlane skyCeiling;

    ClMobjHash clMobjHash;
#endif

    Impl(Public *i)
        : Base(i)
#ifdef __CLIENT__
        , skyFloor  (Sector::Floor  , DDMAXFLOAT)
        , skyCeiling(Sector::Ceiling, DDMINFLOAT)
#endif
    {
        sky.setMap(thisPublic);
        sky.setIndexInMap(0);
    }

    ~Impl()
    {
#ifdef __CLIENT__
        self().removeAllLumobjs();
#if 0
        self().removeAllBiasSources();
#endif
#endif

        // Delete thinkers before the map elements, because thinkers may reference them
        // in their private data destructors.
        thinkers.reset();

        qDeleteAll(sectors);
        qDeleteAll(subspaces);
        for (Polyobj *polyobj : polyobjs)
        {
            polyobj->~Polyobj();
            M_Free(polyobj);
        }
        qDeleteAll(lines);

        /// @todo fixme: Free all memory we have ownership of.
        // mobjNodes/lineNodes/lineLinks
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
    void unclosedSectorFound(Sector &sector, Vector2d const &nearPoint)
    {
        // Notify interested parties that an unclosed sector was found.
        DENG2_FOR_PUBLIC_AUDIENCE(UnclosedSectorFound, i) i->unclosedSectorFound(sector, nearPoint);
    }

    /**
     * Notify interested parties of a "one-way window" in the map.
     *
     * @param line              The window line.
     * @param backFacingSector  Sector that the back of the line is facing.
     */
    void notifyOneWayWindowFound(Line &line, Sector &backFacingSector)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(OneWayWindowFound, i) i->oneWayWindowFound(line, backFacingSector);
    }

    struct testForWindowEffectParams
    {
        ddouble frontDist   = 0;
        ddouble backDist    = 0;
        Sector *frontOpen   = nullptr;
        Sector *backOpen    = nullptr;
        Line *frontLine     = nullptr;
        Line *backLine      = nullptr;
        Line *testLine      = nullptr;
        bool castHorizontal = false;
        Vector2d testLineCenter;
    };

    static bool lineHasZeroLength(Line const &line)
    {
        return de::abs(line.length()) < 1.0 / 128.0;
    }

    static void testForWindowEffect2(Line &line, testForWindowEffectParams &p)
    {
        if (&line == p.testLine) return;
        if (line.isSelfReferencing()) return;
        if (lineHasZeroLength(line)) return;

        ddouble dist = 0;
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

    bool lineMightHaveWindowEffect(Line const &line)
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
        for (Vertex *vertex : mesh.vertexs())
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
                scanRegion.minY = line->bounds().minY - bsp::DIST_EPSILON;
                scanRegion.maxY = line->bounds().maxY + bsp::DIST_EPSILON;
            }
            else
            {
                scanRegion.minX = line->bounds().minX - bsp::DIST_EPSILON;
                scanRegion.maxX = line->bounds().maxX + bsp::DIST_EPSILON;
            }

            validCount++;
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
        DENG2_ASSERT(bsp.tree == nullptr);
        DENG2_ASSERT(subspaces.isEmpty());

        // It begins...
        Time begunAt;

        LOGDEV_MAP_XVERBOSE("Building BSP for \"%s\" with split cost factor %d...",
                (self().hasManifest() ? self().manifest().composeUri() : "(unknown map)") << bspSplitFactor);

        // First we'll scan for so-called "one-way window" constructs and mark
        // them so that the space partitioner can treat them specially.
        findOneWayWindows();

        // Remember the current next vertex ordinal as we'll need to index any
        // new vertexes produced during the build process.
        dint nextVertexOrd = mesh.vertexCount();

        // Determine the set of lines for which we will build a BSP.
        auto linesToBuildFor = QSet<Line *>::fromList(lines);

        // Polyobj lines should be excluded.
        for (Polyobj *pob : polyobjs)
        for (Line *line : pob->lines())
        {
            linesToBuildFor.remove(line);
        }

        try
        {
            // Configure a space partitioner.
            bsp::Partitioner partitioner(bspSplitFactor);
            partitioner.audienceForUnclosedSectorFound += this;

            // Build a new BSP tree.
            bsp.tree = partitioner.makeBspTree(linesToBuildFor, mesh);
            DENG2_ASSERT(bsp.tree);

            LOG_MAP_VERBOSE("BSP built: %s. With %d Segments and %d Vertexes.")
                    << bsp.tree->summary()
                    << partitioner.segmentCount()
                    << partitioner.vertexCount();

            // Attribute an index to any new vertexes.
            for (dint i = nextVertexOrd; i < mesh.vertexCount(); ++i)
            {
                Vertex *vtx = mesh.vertexs().at(i);
                vtx->setMap(thisPublic);
                vtx->setIndexInMap(i);
            }

#ifdef DENG2_QT_4_7_OR_NEWER
            /// @todo Determine the actual number of subspaces needed.
            subspaces.reserve(bsp.tree->leafCount());
#endif

            // Iterative pre-order traversal of the map element tree.
            BspTree const *cur  = bsp.tree;
            BspTree const *prev = nullptr;
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
                                    << &leaf << (leaf.hasSubspace()? leaf.subspace().poly().hedgeCount() : 0);
                            }

                            if (leaf.hasSubspace())
                            {
                                // Add this subspace to the LUT.
                                ConvexSubspace &subspace = leaf.subspace();
                                subspace.setIndexInMap(subspaces.count());
                                subspaces.append(&subspace);

#ifdef DENG_DEBUG  // See if we received a partial geometry...
                                dint discontinuities = 0;
                                HEdge *hedge = subspace.poly().hedge();
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
        catch (Error const &er)
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
        DENG2_ASSERT(!sector.hasSubsectors());

        // Group the subspaces into sets which share at least one common edge. We'll do
        // this by starting with a set per subspace and then keep merging the sets until
        // no two sets share a common edge ("clustering").

        typedef QVector<ConvexSubspace *> Subspaces;
        typedef QList<Subspaces> SubspaceSets;
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
            for (dint i = 0; i < subspaceSets.count(); ++i)
            for (dint k = 0; k < subspaceSets.count(); ++k)
            {
                if (i == k) continue;

                for (ConvexSubspace *subspace : subspaceSets[i])
                {
                    HEdge *baseHEdge = subspace->poly().hedge();
                    HEdge *hedge = baseHEdge;
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
        dint needed = 0;
        for (Subspaces const &subspaceSet : subspaceSets)
        {
            needed += subspaceSet.count();
        }
        subsectorsById.clear();
        subsectorsById.reserve(needed);
        for (Subspaces const &subspaceSet : subspaceSets)
        {
            Subsector *subsec = sector.addSubsector(subspaceSet);
            DENG2_ASSERT(subsec != nullptr);
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
    void initLineBlockmap(ddouble margin = 8)
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
    void initMobjBlockmap(ddouble margin = 8)
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
        AABoxd const box = Mobj_Bounds(mob);

        // Get a new root node.
        mob.lineRoot = NP_New(&mobjNodes, NP_ROOT_NODE);

        validCount++;
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
    void initPolyobjBlockmap(ddouble margin = 8)
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
    void initSubspaceBlockmap(ddouble margin = 8)
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

#ifdef __CLIENT__
    void initContactBlockmaps(ddouble margin = 8)
    {
        // Setup the blockmap area to enclose the whole map, plus a margin
        // (margin is needed for a map that fits entirely inside one blockmap cell).
        AABoxd expandedBounds(bounds.minX - margin, bounds.minY - margin,
                              bounds.maxX + margin, bounds.maxY + margin);

        mobjContactBlockmap.reset(new ContactBlockmap(expandedBounds));
        lumobjContactBlockmap.reset(new ContactBlockmap(expandedBounds));
    }

    /**
     * Returns the appropriate contact blockmap for the specified contact @a type.
     */
    ContactBlockmap &contactBlockmap(ContactType type)
    {
        switch (type)
        {
        case ContactMobj:   return *mobjContactBlockmap;
        case ContactLumobj: return *lumobjContactBlockmap;

        default: throw Error("Map::contactBlockmap", "Invalid contact type");
        }
    }

    /**
     * To be called to link all contacts into the contact blockmaps.
     *
     * @todo Why don't we link contacts immediately? -ds
     */
    void linkAllContacts()
    {
        R_ForAllContacts([this] (Contact const &contact)
        {
            contactBlockmap(contact.type()).link(const_cast<Contact &>(contact));
            return LoopContinue;
        });
    }

    // Clear the "contact" blockmaps (BSP leaf => object).
    void removeAllContacts()
    {
        mobjContactBlockmap->clear();
        lumobjContactBlockmap->clear();

        R_ClearContactLists(*thisPublic);
    }
#endif

    /**
     * Locate a polyobj by sound emitter.
     *
     * @param soundEmitter  SoundEmitter to search for.
     *
     * @return  Pointer to the referenced Polyobj instance; otherwise @c nullptr.
     */
    Polyobj *polyobjBySoundEmitter(SoundEmitter const &soundEmitter) const
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
    Sector *sectorBySoundEmitter(SoundEmitter const &soundEmitter) const
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
    Plane *planeBySoundEmitter(SoundEmitter const &soundEmitter) const
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
    Surface *surfaceBySoundEmitter(SoundEmitter const &soundEmitter) const
    {
        // Perhaps a wall surface?
        for (Line *line : lines)
        for (dint i = 0; i < 2; ++i)
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

#ifdef __CLIENT__
    /**
     * Interpolate the smoothed height of planes.
     */
    void lerpTrackedPlanes(bool resetNextViewer)
    {
        if (resetNextViewer)
        {
            // Reset the plane height trackers.
            for (Plane *plane : trackedPlanes)
            {
                plane->resetSmoothedHeight();
            }

            // Tracked movement is now all done.
            trackedPlanes.clear();
        }
        // While the game is paused there is no need to smooth.
        else //if (!clientPaused)
        {
            QMutableSetIterator<Plane *> iter(trackedPlanes);
            while (iter.hasNext())
            {
                Plane *plane = iter.next();

                plane->lerpSmoothedHeight();

                // Has this plane reached its destination?
                if (de::fequal(plane->heightSmoothed(), plane->height()))
                {
                    iter.remove();
                }
            }
        }
    }

    /**
     * Interpolate the smoothed material origin of surfaces.
     */
    void lerpScrollingSurfaces(bool resetNextViewer)
    {
        if (resetNextViewer)
        {
            // Reset the surface material origin trackers.
            for (Surface *surface : scrollingSurfaces)
            {
                surface->resetSmoothedOrigin();
            }

            // Tracked movement is now all done.
            scrollingSurfaces.clear();
        }
        // While the game is paused there is no need to smooth.
        else //if (!clientPaused)
        {
            QMutableSetIterator<Surface *> iter(scrollingSurfaces);
            while (iter.hasNext())
            {
                Surface *surface = iter.next();
                surface->lerpSmoothedOrigin();
                // Has this material reached its destination?
                if (surface->originSmoothed() == surface->origin())
                {
                    iter.remove();
                }
            }
        }
    }

#if 0
    /**
     * Perform preprocessing which must be done before rendering a frame.
     */
    void biasBeginFrame()
    {
        if (!useBias) return;

        // The time that applies on this frame.
        bias.currentTime = Timer_RealMilliseconds();

        // Check which sources have changed and update the trackers for any
        // affected surfaces.
        QBitArray allChanges(BiasTracker::MAX_CONTRIBUTORS);
        bool needUpdateSurfaces = false;

        for (dint i = 0; i < bias.sources.count(); ++i)
        {
            BiasSource *bsrc = bias.sources.at(i);

            if (bsrc->trackChanges(allChanges, i, bias.currentTime))
            {
                // We'll need to redetermine source => surface affection.
                needUpdateSurfaces = true;
            }
        }

        if (!needUpdateSurfaces) return;

        // Apply changes to all surfaces:
        bias.lastChangeOnFrame = R_FrameCount();
        for (Sector *sec : sectors)
        {
            sec->forAllSubsectors([&allChanges] (Subsector &subsec)
            {
                subsec.as<ClientSubsector>().applyBiasChanges(allChanges);
                return LoopContinue;
            });
        }
    }
#endif

    /**
     * Create new mobj => BSP leaf contacts.
     */
    void generateMobjContacts()
    {
        for (Sector *sector : sectors)
        for (mobj_t *iter = sector->firstMobj(); iter; iter = iter->sNext)
        {
            R_AddContact(*iter);
        }
    }

    /**
     * Perform lazy initialization of the generator collection.
     */
    Generators &getGenerators()
    {
        // Time to initialize a new collection?
        if (!generators)
        {
            generators.reset(new Generators);
            generators->resize(sectors.count());
        }
        return *generators;
    }

    /**
     * Lookup the next available generator id.
     *
     * @todo Optimize: Cache this result.
     * @todo Optimize: We could maintain an age-sorted list of generators.
     *
     * @return  The next available id else @c 0 iff there are no unused ids.
     */
    Generator::Id findIdForNewGenerator()
    {
        Generators &gens = getGenerators();

        // Prefer allocating a new generator if we've a spare id.
        duint unused = 0;
        for (; unused < gens.activeGens.size(); ++unused)
        {
            if (!gens.activeGens[unused]) break;
        }
        if (unused < gens.activeGens.size()) return Generator::Id(unused + 1);

        // See if there is an active, non-static generator we can supplant.
        Generator *oldest = nullptr;
        for (Generator *gen : gens.activeGens)
        {
            if (!gen || gen->isStatic()) continue;

            if (!oldest || gen->age() > oldest->age())
            {
                oldest = gen;
            }
        }

        return (oldest? oldest->id() : 0);
    }

    void spawnMapParticleGens()
    {
        if (!self().hasManifest()) return;

        for (dint i = 0; i < DED_Definitions()->ptcGens.size(); ++i)
        {
            ded_ptcgen_t *genDef = &DED_Definitions()->ptcGens[i];

            if (!genDef->map) continue;

            if (*genDef->map != self().manifest().composeUri())
                continue;

            // Are we still spawning using this generator?
            if (genDef->spawnAge > 0 && App_World().time() > genDef->spawnAge)
                continue;

            Generator *gen = self().newGenerator();
            if (!gen) return;  // No more generators.

            // Initialize the particle generator.
            gen->count = genDef->particles;
            gen->spawnRateMultiplier = 1;

            gen->configureFromDef(genDef);
            gen->setUntriggered();

            // Is there a need to pre-simulate?
            gen->presimulate(genDef->preSim);
        }
    }

    /**
     * Spawns all type-triggered particle generators, regardless of whether
     * the type of mobj exists in the map or not (mobjs might be dynamically
     * created).
     */
    void spawnTypeParticleGens()
    {
        auto &defs = *DED_Definitions();

        for (dint i = 0; i < defs.ptcGens.size(); ++i)
        {
            ded_ptcgen_t *def = &defs.ptcGens[i];

            if (def->typeNum != DED_PTCGEN_ANY_MOBJ_TYPE && def->typeNum < 0)
                continue;

            Generator *gen = self().newGenerator();
            if (!gen) return;  // No more generators.

            // Initialize the particle generator.
            gen->count = def->particles;
            gen->spawnRateMultiplier = 1;

            gen->configureFromDef(def);
            gen->type  = def->typeNum;
            gen->type2 = def->type2Num;

            // Is there a need to pre-simulate?
            gen->presimulate(def->preSim);
        }
    }

    dint findDefForGenerator(Generator *gen)
    {
        DENG2_ASSERT(gen);

        auto &defs = *DED_Definitions();

        // Search for a suitable definition.
        for (dint i = 0; i < defs.ptcGens.size(); ++i)
        {
            ded_ptcgen_t *def = &defs.ptcGens[i];

            // A type generator?
            if (def->typeNum == DED_PTCGEN_ANY_MOBJ_TYPE && gen->type == DED_PTCGEN_ANY_MOBJ_TYPE)
            {
                return i + 1;  // Stop iteration.
            }
            if (def->typeNum >= 0 &&
               (gen->type == def->typeNum || gen->type2 == def->type2Num))
            {
                return i + 1;  // Stop iteration.
            }

            // A damage generator?
            if (gen->source && gen->source->type == def->damageNum)
            {
                return i + 1;  // Stop iteration.
            }

            // A flat generator?
            if (gen->plane && def->material)
            {
                try
                {
                    world::Material &defMat = world::Materials::get().material(*def->material);

                    Material *mat = gen->plane->surface().materialPtr();
                    if (def->flags & Generator::SpawnFloor)
                        mat = gen->plane->sector().floor().surface().materialPtr();
                    if (def->flags & Generator::SpawnCeiling)
                        mat = gen->plane->sector().ceiling().surface().materialPtr();

                    // Is this suitable?
                    if (mat == &defMat)
                    {
                        return i + 1; // 1-based index.
                    }

#if 0 /// @todo $revise-texture-animation
                    if (def->flags & PGF_GROUP)
                    {
                        // Generator triggered by all materials in the animation.
                        if (Material_IsGroupAnimated(defMat) && Material_IsGroupAnimated(mat)
                            && &Material_AnimGroup(defMat) == &Material_AnimGroup(mat))
                        {
                            // Both are in this animation! This def will do.
                            return i + 1;  // 1-based index.
                        }
                    }
#endif
                }
                catch (MaterialManifest::MissingMaterialError const &)
                {}  // Ignore this error.
                catch (Resources::MissingResourceManifestError const &)
                {}  // Ignore this error.
            }

            // A state generator?
            if (gen->source && def->state[0]
                && ::runtimeDefs.states.indexOf(gen->source->state) == DED_Definitions()->getStateNum(def->state))
            {
                return i + 1;  // 1-based index.
            }
        }

        return 0;  // Not found.
    }

    /**
     * Update existing generators in the map following an engine reset.
     */
    void updateParticleGens()
    {
        for (Generator *gen : getGenerators().activeGens)
        {
            // Only consider active generators.
            if (!gen) continue;

            // Map generators cannot be updated (we have no means to reliably
            // identify them), so destroy them.
            if (gen->isUntriggered())
            {
                Generator_Delete(gen);
                continue;  // Continue iteration.
            }

            if (dint defIndex = findDefForGenerator(gen))
            {
                // Update the generator using the new definition.
                gen->def = &DED_Definitions()->ptcGens[defIndex - 1];
            }
            else
            {
                // Nothing else we can do, destroy it.
                Generator_Delete(gen);
            }
        }

        // Re-spawn map generators.
        spawnMapParticleGens();
    }

    /**
     * Link all generated particles into the map so that they will be drawn.
     *
     * @todo Overkill?
     */
    void linkAllParticles()
    {
        //LOG_AS("Map::linkAllParticles");

        Generators &gens = getGenerators();

        // Empty all generator lists.
        std::memset(gens.lists, 0, sizeof(*gens.lists) * gens.listsSize);
        gens.linkStoreCursor = 0;

        if (useParticles)
        {
            for (Generator *gen : gens.activeGens)
            {
                if (!gen) continue;

                ParticleInfo const *pInfo = gen->particleInfo();
                for (dint i = 0; i < gen->count; ++i, pInfo++)
                {
                    if (pInfo->stage < 0 || !pInfo->bspLeaf)
                        continue;

                    dint listIndex = pInfo->bspLeaf->sectorPtr()->indexInMap();
                    DENG2_ASSERT((unsigned)listIndex < gens.listsSize);

                    // Must check that it isn't already there...
                    bool found = false;
                    for (Generators::ListNode *it = gens.lists[listIndex]; it; it = it->next)
                    {
                        if (it->gen == gen)
                        {
                            // Warning message disabled as these are occuring so thick and fast
                            // that logging is pointless (and negatively affecting performance).
                            //LOGDEV_MAP_VERBOSE("Attempted repeat link of generator %p to list %u.")
                            //        << gen << listIndex;
                            found = true; // No, no...
                        }
                    }

                    if (found) continue;

                    // We need a new link.
                    if (Generators::ListNode *link = gens.newLink())
                    {
                        link->gen  = gen;
                        link->next = gens.lists[listIndex];
                        gens.lists[listIndex] = link;
                    }
                }
            }
        }
    }

    void thinkerBeingDeleted(thinker_s &th)
    {
        clMobjHash.remove(th.id);
    }

#endif  // __CLIENT__
};

Map::Map(res::MapManifest *manifest)
    : BaseMap(manifest)
    , d(new Impl(this))
{}

Record const &Map::mapInfo() const
{
    return App_World().mapInfoForMapUri(hasManifest() ? manifest().composeUri() : de::makeUri("Maps:"));
}

Mesh const &Map::mesh() const
{
    return d->mesh;
}

bool Map::hasBspTree() const
{
    return d->bsp.tree != nullptr;
}

BspTree const &Map::bspTree() const
{
    if (d->bsp.tree) return *d->bsp.tree;
    /// @throw MissingBspTreeError  Attempted with no BSP tree available.
    throw MissingBspTreeError("Map::bspTree", "No BSP tree is available");
}

#ifdef __CLIENT__

SkyDrawable::Animator &Map::skyAnimator() const
{
    return d->skyAnimator;
}

#if 0
bool Map::hasLightGrid() const
{
    return bool(d->lightGrid);
}

LightGrid &Map::lightGrid()
{
    if (bool(d->lightGrid)) return *d->lightGrid;
    /// @throw MissingLightGrid Attempted with no LightGrid initialized.
    throw MissingLightGridError("Map::lightGrid", "No light grid is initialized");
}

LightGrid const &Map::lightGrid() const
{
    if (bool(d->lightGrid)) return *d->lightGrid;
    /// @throw MissingLightGrid Attempted with no LightGrid initialized.
    throw MissingLightGridError("Map::lightGrid", "No light grid is initialized");
}

void Map::initLightGrid()
{
    // Disabled?
    if (!Con_GetInteger("rend-bias-grid"))
        return;

    // Diagonal in maze arrangement of natural numbers.
    // Up to 65 samples per-block(!)
    static dint const MSFACTORS = 7;
    static dint multisample[] = { 1, 5, 9, 17, 25, 37, 49, 65 };

    // Time to initialize the LightGrid?
    if (bool(d->lightGrid))
    {
        d->lightGrid->updateIfNeeded();
        return;
    }

    Time begunAt;
    d->lightGrid.reset(new LightGrid(origin(), dimensions()));

    LightGrid &lg = *d->lightGrid;

    // Determine how many subsector samples we'll make per block and
    // allocate the tempoary storage.
    dint const numSamples = multisample[de::clamp(0, lgMXSample, MSFACTORS)];
    QVector<Vector2d> samplePoints(numSamples);
    QVector<dint>     sampleHits(numSamples);

    /// It would be possible to only allocate memory for the unique
    /// sample results. And then select the appropriate sample in the loop
    /// for initializing the grid instead of copying the previous results in
    /// the loop for acquiring the sample points.
    ///
    /// Calculate with the equation (number of unique sample points):
    ///
    /// ((1 + lgBlockHeight * lgMXSample) * (1 + lgBlockWidth * lgMXSample)) +
    ///     (size % 2 == 0? numBlocks : 0)
    /// OR
    ///
    /// We don't actually need to store the ENTIRE ssample array. It would be
    /// sufficent to only store the results from the start of the previous row
    /// to current col index. This would save a bit of memory.
    ///
    /// However until lightgrid init is finalized it would be rather silly to
    /// optimize this much further.

    // Allocate memory for all the sample results.
    QVector<world::ClientSubsector *> ssamples((lg.dimensions().x * lg.dimensions().y) * numSamples);

    // Determine the size^2 of the samplePoint array plus its center.
    dint size = 0, center = 0;
    if (numSamples > 1)
    {
        dfloat f = sqrt(dfloat( numSamples ));

        if (std::ceil(f) != std::floor(f))
        {
            size = sqrt(dfloat( numSamples - 1 ));
            center = 0;
        }
        else
        {
            size = dint( f );
            center = size + 1;
        }
    }

    // Construct the sample point offset array.
    // This way we can use addition only during calculation of:
    // (dimensions.y * dimensions.x) * numSamples
    if (center == 0)
    {
        // Zero is the center so do that first.
        samplePoints[0] = Vector2d(lg.blockSize() / 2, lg.blockSize() / 2);
    }

    if (numSamples > 1)
    {
        ddouble bSize = ddouble(lg.blockSize()) / (size - 1);

        // Is there an offset?
        dint idx = (center == 0? 1 : 0);

        for (dint y = 0; y < size; ++y)
        for (dint x = 0; x < size; ++x, ++idx)
        {
            samplePoints[idx] = Vector2d(de::round<ddouble>(x * bSize),
                                         de::round<ddouble>(y * bSize));
        }
    }

    // Acquire the subsectors at ALL the sample points.
    for (dint y = 0; y < lg.dimensions().y; ++y)
    for (dint x = 0; x < lg.dimensions().x; ++x)
    {
        LightGrid::Index const blk = lg.toIndex(x, y);
        Vector2d const off(x * lg.blockSize(), y * lg.blockSize());

        dint sampleOffset = 0;
        if (center == 0)
        {
            // Center point is not considered with the term 'size'. Sample this point and
            // place at index 0 (at the start of the samples for this block).
            Subsector *s = subsectorAt(lg.origin() + off + samplePoints[0]);
            ssamples[blk * numSamples] = s ? &s->as<world::ClientSubsector>() : nullptr;
            sampleOffset++;
        }

        dint count = blk * size;
        for (dint b = 0; b < size; ++b)
        {
            dint i = (b + count) * size;

            for (dint a = 0; a < size; ++a, ++sampleOffset)
            {
                dint idx = a + i + (center == 0? blk + 1 : 0);

                if (numSamples > 1 && ((x > 0 && a == 0) || (y > 0 && b == 0)))
                {
                    // We have already sampled this point.
                    // Get the previous result.
                    LightGrid::Ref prev(x, y);
                    LightGrid::Ref prevB(a, b);
                    dint prevIdx;

                    if (x > 0 && a == 0)
                    {
                        prevB.x = size -1;
                        prev.x--;
                    }
                    if (y > 0 && b == 0)
                    {
                        prevB.y = size -1;
                        prev.y--;
                    }

                    prevIdx = prevB.x + (prevB.y + lg.toIndex(prev) * size) * size;
                    if (center == 0)
                        prevIdx += lg.toIndex(prev) + 1;

                    ssamples[idx] = ssamples[prevIdx];
                }
                else
                {
                    // We haven't sampled this point yet.
                    Subsector *s = subsectorAt(lg.origin() + off + samplePoints[sampleOffset]);
                    ssamples[idx] = s ? &s->as<world::ClientSubsector>() : nullptr;
                }
            }
        }
    }

    // Allocate memory used for the collection of the sample results.
    QVector<world::ClientSubsector *> blkSampleSubsectors(numSamples);

    for (dint y = 0; y < lg.dimensions().y; ++y)
    for (dint x = 0; x < lg.dimensions().x; ++x)
    {
        /// Pick the subsector at each of the sample points.
        ///
        /// @todo We don't actually need the blkSampleSubsectors array anymore.
        /// Now that ssamples stores the results consecutively a simple index
        /// into ssamples would suffice. However if the optimization to save
        /// memory is implemented as described in the comments above we WOULD
        /// still require it.
        ///
        /// For now we'll make use of it to clarify the code.
        dint const sampleOffset = lg.toIndex(x, y) * numSamples;
        for (dint i = 0; i < numSamples; ++i)
        {
            blkSampleSubsectors[i] = ssamples[i + sampleOffset];
        }

        world::ClientSubsector *subsec = nullptr;
        if (numSamples == 1)
        {
            subsec = blkSampleSubsectors[center];
        }
        else
        {
            // Pick the sector which had the most hits.
            dint best = -1;
            sampleHits.fill(0);

            for (dint i = 0; i < numSamples; ++i)
            {
                if (!blkSampleSubsectors[i]) continue;

                for (dint k = 0; k < numSamples; ++k)
                {
                    if (blkSampleSubsectors[k] == blkSampleSubsectors[i] && blkSampleSubsectors[k])
                    {
                        sampleHits[k]++;
                        if (sampleHits[k] > best)
                        {
                            best = i;
                        }
                    }
                }
            }

            if (best != -1)
            {
                // Favor the center sample if its a draw.
                if (sampleHits[best] == sampleHits[center] && blkSampleSubsectors[center])
                {
                    subsec = blkSampleSubsectors[center];
                }
                else
                {
                    subsec = blkSampleSubsectors[best];
                }
            }
        }

        if (subsec)
        {
            lg.setPrimarySource(lg.toIndex(x, y), subsec);
        }
    }

    LOGDEV_GL_MSG("%i light blocks (%u bytes)")
        << lg.numBlocks() << lg.blockStorageSize();

    // How much time did we spend?
    LOGDEV_GL_MSG("LightGrid init completed in %.2f seconds") << begunAt.since();
}

void Map::initBias()
{
    Time begunAt;

    LOG_AS("Map::initBias");

    // Start with no sources whatsoever.
    d->bias.sources.clear();

    if (hasManifest())
    {
        String const oldUniqueId = manifest().composeUniqueId(App_CurrentGame());
        auto &defs = *DED_Definitions();

        // Load light sources from Light definitions.
        for (dint i = 0; i < defs.lights.size(); ++i)
        {
            ded_light_t *lightDef = &defs.lights[i];

            if (lightDef->state[0]) continue;
            if (oldUniqueId.compareWithoutCase(lightDef->uniqueMapID)) continue;

            // Already at maximum capacity?
            if (biasSourceCount() == MAX_BIAS_SOURCES)
                break;

            addBiasSource(BiasSource::fromDef(*lightDef));
        }
    }

    LOGDEV_MAP_VERBOSE("Completed in %.2f seconds") << begunAt.since();
}
#endif

void Map::initRadio()
{
    LOG_AS("Map::initRadio");

    Time begunAt;

    for (Vertex *vtx : d->mesh.vertexs())
    {
        vtx->updateShadowOffsets();
    }

    /// The algorithm:
    ///
    /// 1. Use the subspace blockmap to look for all the blocks that are within the line's shadow
    ///    bounding box.
    /// 2. Check the ConvexSubspaces whose sector is the same as the line.
    /// 3. If any of the shadow points are in the subspace, or any of the shadow edges cross one
    ///    of the subspace's edges (not parallel), link the line to the ConvexSubspace.
    for (Line *line : d->lines)
    {
        if (!line->isShadowCaster()) continue;

        // For each side of the line.
        for (dint i = 0; i < 2; ++i)
        {
            LineSide &side = line->side(i);

            if (!side.hasSector()) continue;
            if (!side.hasSections()) continue;

            // Skip sides which share one or more edge with malformed geometry.
            if (!side.leftHEdge() || !side.rightHEdge()) continue;

            Vertex const &vtx0   = line->vertex(i);
            Vertex const &vtx1   = line->vertex(i ^ 1);
            LineOwner const *vo0 = line->vertexOwner(i)->next();
            LineOwner const *vo1 = line->vertexOwner(i ^ 1)->prev();

            AABoxd bounds = line->bounds();

            // Use the extended points, they are wider than inoffsets.
            Vector2d const sv0 = vtx0.origin() + vo0->extendedShadowOffset();
            V2d_AddToBoxXY(bounds.arvec2, sv0.x, sv0.y);

            Vector2d const sv1 = vtx1.origin() + vo1->extendedShadowOffset();
            V2d_AddToBoxXY(bounds.arvec2, sv1.x, sv1.y);

            // Link the shadowing line to all the subspaces whose axis-aligned bounding box
            // intersects 'bounds'.
            ::validCount++;
            dint const localValidCount = ::validCount;
            subspaceBlockmap().forAllInBox(bounds, [&bounds, &side, &localValidCount] (void *object)
            {
                auto &sub = *(ConvexSubspace *)object;
                if (sub.validCount() != localValidCount)  // not yet processed
                {
                    sub.setValidCount(localValidCount);
                    if (&sub.subsector().sector() == side.sectorPtr())
                    {
                        // Check the bounds.
                        AABoxd const &polyBox = sub.poly().bounds();
                        if (!(   polyBox.maxX < bounds.minX
                              || polyBox.minX > bounds.maxX
                              || polyBox.minY > bounds.maxY
                              || polyBox.maxY < bounds.minY))
                        {
                            sub.addShadowLine(side);
                        }
                    }
                }
                return LoopContinue;
            });
        }
    }

    LOGDEV_GL_MSG("Completed in %.2f seconds") << begunAt.since();
}

void Map::initContactBlockmaps()
{
    d->initContactBlockmaps();
}

void Map::spreadAllContacts(AABoxd const &region)
{
    // Expand the region according by the maxium radius of each contact type.
    d->mobjContactBlockmap->
        spread(AABoxd(region.minX - DDMOBJ_RADIUS_MAX, region.minY - DDMOBJ_RADIUS_MAX,
                      region.maxX + DDMOBJ_RADIUS_MAX, region.maxY + DDMOBJ_RADIUS_MAX));

    d->lumobjContactBlockmap->
        spread(AABoxd(region.minX - Lumobj::radiusMax(), region.minY - Lumobj::radiusMax(),
                      region.maxX + Lumobj::radiusMax(), region.maxY + Lumobj::radiusMax()));
}

void Map::initGenerators()
{
    LOG_AS("Map::initGenerators");
    Time begunAt;
    d->spawnTypeParticleGens();
    d->spawnMapParticleGens();
    LOGDEV_MAP_VERBOSE("Completed in %.2f seconds") << begunAt.since();
}

void Map::spawnPlaneParticleGens()
{
    //if (!useParticles) return;

    for (Sector *sector : d->sectors)
    {
        Plane &floor = sector->floor();
        floor.spawnParticleGen(Def_GetGenerator(floor.surface().composeMaterialUri()));

        Plane &ceiling = sector->ceiling();
        ceiling.spawnParticleGen(Def_GetGenerator(ceiling.surface().composeMaterialUri()));
    }
}

void Map::clearClMobjs()
{
    d->clMobjHash.clear();
}

mobj_t *Map::clMobjFor(thid_t id, bool canCreate) const
{
    LOG_AS("Map::clMobjFor");

    ClMobjHash::const_iterator found = d->clMobjHash.constFind(id);
    if (found != d->clMobjHash.constEnd())
    {
        return found.value();
    }

    if (!canCreate) return nullptr;

    // Create a new client mobj. This is a regular mobj that has network state
    // associated with it.

    MobjThinker mob(Thinker::AllocateMemoryZone);
    mob.id       = id;
    mob.function = reinterpret_cast<thinkfunc_t>(gx.MobjThinker);

    auto *data = new ClientMobjThinkerData;
    data->remoteSync().flags = DDMF_REMOTE;
    mob.setData(data);

    d->clMobjHash.insert(id, mob);
    data->audienceForDeletion() += d;  // for removing from the hash

    d->thinkers->setMobjId(id);  // Mark this ID as used.

    // Client mobjs are full-fludged game mobjs as well.
    d->thinkers->add(*(thinker_t *)mob);

    return mob.take();
}

dint Map::clMobjIterator(dint (*callback)(mobj_t *, void *), void *context)
{
    ClMobjHash::const_iterator next;
    for (ClMobjHash::const_iterator i = d->clMobjHash.constBegin();
         i != d->clMobjHash.constEnd(); i = next)
    {
        next = i;
        next++;

        DENG2_ASSERT(THINKER_DATA(i.value()->thinker, ClientMobjThinkerData).hasRemoteSync());

        // Callback returns zero to continue.
        if (dint result = callback(i.value(), context))
            return result;
    }
    return 0;
}

Map::ClMobjHash const &Map::clMobjHash() const
{
    return d->clMobjHash;
}

#endif // __CLIENT__

AABoxd const &Map::bounds() const
{
    return d->bounds;
}

coord_t Map::gravity() const
{
    return _effectiveGravity;
}

void Map::setGravity(coord_t newGravity)
{
    if (!de::fequal(_effectiveGravity, newGravity))
    {
        _effectiveGravity = newGravity;
        LOG_MAP_VERBOSE("Effective gravity for %s now %.1f")
            << (hasManifest() ? manifest().gets("id") : "(unknown map)") << _effectiveGravity;
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
    return d->sky;
}

dint Map::vertexCount() const
{
    return d->mesh.vertexCount();
}

Vertex &Map::vertex(dint index) const
{
    if (Vertex *vtx = vertexPtr(index)) return *vtx;
    /// @throw MissingElementError  Invalid Vertex reference specified.
    throw MissingElementError("Map::vertex", "Unknown Vertex index:" + String::number(index));
}

Vertex *Map::vertexPtr(dint index) const
{
    if (index >= 0 && index < d->mesh.vertexCount())
    {
        return d->mesh.vertexs().at(index);
    }
    return nullptr;
}

LoopResult Map::forAllVertexs(std::function<LoopResult (Vertex &)> func) const
{
    for (Vertex *vtx : d->mesh.vertexs())
    {
        if (auto result = func(*vtx)) return result;
    }
    return LoopContinue;
}

dint Map::lineCount() const
{
    return d->lines.count();
}

Line &Map::line(dint index) const
{
    if (Line *li = linePtr(index)) return *li;
    /// @throw MissingElementError  Invalid Line reference specified.
    throw MissingElementError("Map::line", "Unknown Line index:" + String::number(index));
}

Line *Map::linePtr(dint index) const
{
    if (index >= 0 && index < d->lines.count())
    {
        return d->lines.at(index);
    }
    return nullptr;
}

LoopResult Map::forAllLines(std::function<LoopResult (Line &)> func) const
{
    for (Line *li : d->lines)
    {
        if (auto result = func(*li)) return result;
    }
    return LoopContinue;
}

dint Map::sectorCount() const
{
    return d->sectors.count();
}

#ifdef __CLIENT__
bool Map::isPointInVoid(de::Vector3d const &point) const
{
    BspLeaf const &bspLeaf = bspLeafAt(point);
    if (bspLeaf.hasSubspace() && bspLeaf.subspace().contains(point) && bspLeaf.subspace().hasSubsector())
    {
        auto const &subsec = bspLeaf.subspace().subsector().as<ClientSubsector>();
        return subsec.isHeightInVoid(point.z);
    }
    return true; // In the void.
}
#endif

dint Map::subspaceCount() const
{
    return d->subspaces.count();
}

ConvexSubspace &Map::subspace(dint index) const
{
    if (ConvexSubspace *sub = subspacePtr(index)) return *sub;
    /// @throw MissingElementError  Invalid ConvexSubspace reference specified.
    throw MissingElementError("Map::subspace", "Unknown subspace index:" + String::number(index));
}

ConvexSubspace *Map::subspacePtr(dint index) const
{
    if (index >= 0 && index < d->subspaces.count())
    {
        return d->subspaces.at(index);
    }
    return nullptr;
}

LoopResult Map::forAllSubspaces(std::function<LoopResult (ConvexSubspace &)> func) const
{
    for (ConvexSubspace *sub : d->subspaces)
    {
        if (auto result = func(*sub)) return result;
    }
    return LoopContinue;
}

dint Map::polyobjCount() const
{
    return d->polyobjs.count();
}

Polyobj &Map::polyobj(dint index) const
{
    if (Polyobj *pob = polyobjPtr(index)) return *pob;
    /// @throw MissingObjectError  Invalid ConvexSubspace reference specified.
    throw MissingObjectError("Map::subspace", "Unknown Polyobj index:" + String::number(index));
}

Polyobj *Map::polyobjPtr(dint index) const
{
    if (index >= 0 && index < d->polyobjs.count())
    {
        return d->polyobjs.at(index);
    }
    return nullptr;
}

LoopResult Map::forAllPolyobjs(std::function<LoopResult (Polyobj &)> func) const
{
    for (Polyobj *pob : d->polyobjs)
    {
        if (auto result = func(*pob)) return result;
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

dint Map::ambientLightLevel() const
{
    return _ambientLightLevel;
}

LineSide &Map::side(dint index) const
{
    if (LineSide *side = sidePtr(index)) return *side;
    /// @throw MissingElementError  Invalid LineSide reference specified.
    throw MissingElementError("Map::side", "Unknown LineSide index:" + String::number(index));
}

LineSide *Map::sidePtr(dint index) const
{
    if (index < 0) return nullptr;
    return &d->lines.at(index / 2)->side(index % 2);
}

dint Map::toSideIndex(dint lineIndex, dint backSide) // static
{
    DENG_ASSERT(lineIndex >= 0);
    return lineIndex * 2 + (backSide? 1 : 0);
}

bool Map::identifySoundEmitter(SoundEmitter const &emitter, Sector **sector,
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
    DENG_ASSERT(d->lineLinks == nullptr);
    d->lineLinks = (nodeindex_t *) Z_Malloc(sizeof(*d->lineLinks) * lineCount(), PU_MAPSTATIC, 0);

    for (dint i = 0; i < lineCount(); ++i)
    {
        d->lineLinks[i] = NP_New(&d->lineNodes, NP_ROOT_NODE);
    }

    // How much time did we spend?
    LOGDEV_MAP_MSG("Initialized node piles in %.2f seconds") << begunAt.since();
}

Sector &Map::sector(dint index) const
{
    if (Sector *sec = sectorPtr(index)) return *sec;
    /// @throw MissingElementError  Invalid Sector reference specified.
    throw MissingElementError("Map::sector", "Unknown Sector index:" + String::number(index));
}

Sector *Map::sectorPtr(dint index) const
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

Subsector *Map::subsectorAt(Vector2d const &point) const
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
    auto found = d->subsectorsById.constFind(id);
    if (found != d->subsectorsById.constEnd())
    {
        return found.value();
    }
    return nullptr;
}

Blockmap const &Map::mobjBlockmap() const
{
    if (bool(d->mobjBlockmap)) return *d->mobjBlockmap;
    /// @throw MissingBlockmapError  The mobj blockmap is not yet initialized.
    throw MissingBlockmapError("Map::mobjBlockmap", "Mobj blockmap is not initialized");
}

Blockmap const &Map::polyobjBlockmap() const
{
    if (bool(d->polyobjBlockmap)) return *d->polyobjBlockmap;
    /// @throw MissingBlockmapError  The polyobj blockmap is not yet initialized.
    throw MissingBlockmapError("Map::polyobjBlockmap", "Polyobj blockmap is not initialized");
}

LineBlockmap const &Map::lineBlockmap() const
{
    if (bool(d->lineBlockmap)) return *d->lineBlockmap;
    /// @throw MissingBlockmapError  The line blockmap is not yet initialized.
    throw MissingBlockmapError("Map::lineBlockmap", "Line blockmap is not initialized");
}

Blockmap const &Map::subspaceBlockmap() const
{
    if (bool(d->subspaceBlockmap)) return *d->subspaceBlockmap;
    /// @throw MissingBlockmapError  The subspace blockmap is not yet initialized.
    throw MissingBlockmapError("Map::subspaceBlockmap", "Convex subspace blockmap is not initialized");
}

LoopResult Map::forAllLinesTouchingMobj(mobj_t &mob, std::function<LoopResult (Line &)> func) const
{
    /// @todo Optimize: It should not be necessary to collate the objects first in
    /// in order to perform the iteration. This kind of "belt and braces" safety
    /// measure would not be necessary at this level if the caller(s) instead took
    /// responsibility for managing relationship changes during the iteration. -ds

    if (&Mobj_Map(mob) == this && Mobj_IsLinked(mob) && mob.lineRoot)
    {
        QVarLengthArray<Line *, 16> linkStore;

        linknode_t *tn = d->mobjNodes.nodes;
        for (nodeindex_t nix = tn[mob.lineRoot].next; nix != mob.lineRoot; nix = tn[nix].next)
        {
            linkStore.append((Line *)(tn[nix].ptr));
        }

        for (dint i = 0; i < linkStore.count(); ++i)
        {
            if (auto result = func(*linkStore[i]))
                return result;
        }
    }
    return LoopContinue;
}

LoopResult Map::forAllSectorsTouchingMobj(mobj_t &mob, std::function<LoopResult (Sector &)> func) const
{
    /// @todo Optimize: It should not be necessary to collate the objects first in
    /// in order to perform the iteration. This kind of "belt and braces" safety
    /// measure would not be necessary at this level if the caller(s) instead took
    /// responsibility for managing relationship changes during the iteration. -ds

    if (&Mobj_Map(mob) == this && Mobj_IsLinked(mob))
    {
        QVarLengthArray<Sector *, 16> linkStore;

        // Always process the mobj's own sector first.
        Sector &ownSec = *Mobj_BspLeafAtOrigin(mob).sectorPtr();
        ownSec.setValidCount(validCount);
        linkStore.append(&ownSec);

        // Any good lines around here?
        if (mob.lineRoot)
        {
            linknode_t *tn = d->mobjNodes.nodes;
            for (nodeindex_t nix = tn[mob.lineRoot].next; nix != mob.lineRoot; nix = tn[nix].next)
            {
                auto *ld = (Line *)(tn[nix].ptr);

                // All these lines have sectors on both sides.
                // First, try the front.
                Sector &frontSec = ld->front().sector();
                if (frontSec.validCount() != validCount)
                {
                    frontSec.setValidCount(validCount);
                    linkStore.append(&frontSec);
                }

                // And then the back.
                /// @todo Above comment suggest always twosided, which is it? -ds
                if (ld->back().hasSector())
                {
                    Sector &backSec = ld->back().sector();
                    if (backSec.validCount() != validCount)
                    {
                        backSec.setValidCount(validCount);
                        linkStore.append(&backSec);
                    }
                }
            }
        }

        for (dint i = 0; i < linkStore.count(); ++i)
        {
            if (auto result = func(*linkStore[i]))
                return result;
        }
    }

    return LoopContinue;
}

LoopResult Map::forAllMobjsTouchingLine(Line &line, std::function<LoopResult (mobj_t &)> func) const
{
    /// @todo Optimize: It should not be necessary to collate the objects first in
    /// in order to perform the iteration. This kind of "belt and braces" safety
    /// measure would not be necessary at this level if the caller(s) instead took
    /// responsibility for managing relationship changes during the iteration. -ds

    if (&line.map() == this)
    {
        QVarLengthArray<mobj_t *, 256> linkStore;

        // Collate mobjs touching the given line in case these relationships change.
        linknode_t *ln   = d->lineNodes.nodes;
        nodeindex_t root = d->lineLinks[line.indexInMap()];
        for (nodeindex_t nix = ln[root].next; nix != root; nix = ln[nix].next)
        {
            linkStore.append((mobj_t *)(ln[nix].ptr));
        }

        for (dint i = 0; i < linkStore.count(); ++i)
        {
            if (auto result = func(*linkStore[i]))
                return result;
        }
    }
    return LoopContinue;
}

LoopResult Map::forAllMobjsTouchingSector(Sector &sector, std::function<LoopResult (mobj_t &)> func) const
{
    /// @todo Optimize: It should not be necessary to collate the objects first in
    /// in order to perform the iteration. This kind of "belt and braces" safety
    /// measure would not be necessary at this level if the caller(s) instead took
    /// responsibility for managing relationship changes during the iteration. -ds

    if (&sector.map() == this)
    {
        QVarLengthArray<mobj_t *, 256> linkStore;

        // Collate mobjs that obviously are in the sector.
        for (mobj_t *mob = sector.firstMobj(); mob; mob = mob->sNext)
        {
            if (mob->validCount != validCount)
            {
                mob->validCount = validCount;
                linkStore.append(mob);
            }
        }

        // Collate mobjs linked to the sector's lines.
        linknode_t const *ln = d->lineNodes.nodes;
        sector.forAllSides([this, &linkStore, &ln] (LineSide &side)
        {
            nodeindex_t root = d->lineLinks[side.line().indexInMap()];
            for (nodeindex_t nix = ln[root].next; nix != root; nix = ln[nix].next)
            {
                auto *mob = (mobj_t *)(ln[nix].ptr);
                if (mob->validCount != validCount)
                {
                    mob->validCount = validCount;
                    linkStore.append(mob);
                }
            }
            return LoopContinue;
        });

        // Process all collected mobjs.
        for (dint i = 0; i < linkStore.count(); ++i)
        {
            if (auto result = func(*linkStore[i]))
                return result;
        }
    }
    return LoopContinue;
}

dint Map::unlink(mobj_t &mob)
{
    dint links = 0;

    if (d->unlinkMobjFromSectors(mob))
        links |= MLF_SECTOR;

    BlockmapCell cell = d->mobjBlockmap->toCell(Mobj_Origin(mob));
    if (d->mobjBlockmap->unlink(cell, &mob))
        links |= MLF_BLOCKMAP;

    if (!d->unlinkMobjFromLines(mob))
        links |= MLF_NOLINE;

    return links;
}

void Map::link(mobj_t &mob, dint flags)
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

#ifdef __CLIENT__
    // If this is a player - perform additional tests to see if they have either entered or exited the void.
    if (mob.dPlayer && mob.dPlayer->mo)
    {
        auto &client = ClientApp::player(P_GetDDPlayerIdx(mob.dPlayer));
        client.inVoid = true;
        if (Mobj_HasSubsector(mob))
        {
            auto &subsec = Mobj_Subsector(mob).as<world::ClientSubsector>();
            if (Mobj_BspLeafAtOrigin(mob).subspace().contains(Mobj_Origin(mob)))
            {
                if (   mob.origin[2] <  subsec.visCeiling().heightSmoothed() + 4
                    && mob.origin[2] >= subsec.  visFloor().heightSmoothed())
                {
                    client.inVoid = false;
                }
            }
        }
    }
#endif
}

void Map::unlink(Polyobj &polyobj)
{
    d->polyobjBlockmap->unlink(polyobj.bounds, &polyobj);
}

void Map::link(Polyobj &polyobj)
{
    d->polyobjBlockmap->link(polyobj.bounds, &polyobj);
}

LoopResult Map::forAllLinesInBox(AABoxd const &box, dint flags, std::function<LoopResult (Line &line)> func) const
{
    LoopResult result = LoopContinue;

    // Process polyobj lines?
    if ((flags & LIF_POLYOBJ) && polyobjCount())
    {
        dint const localValidCount = validCount;
        result = polyobjBlockmap().forAllInBox(box, [&func, &localValidCount] (void *object)
        {
            auto &pob = *(Polyobj *)object;
            if (pob.validCount != localValidCount) // not yet processed
            {
                pob.validCount = localValidCount;
                for (Line *line : pob.lines())
                {
                    if (line->validCount() != localValidCount) // not yet processed
                    {
                        line->setValidCount(localValidCount);
                        if (auto result = func(*line))
                            return result;
                    }
                }
            }
            return LoopResult(); // continue
        });
    }

    // Process sector lines?
    if (!result && (flags & LIF_SECTOR))
    {
        dint const localValidCount = validCount;
        result = lineBlockmap().forAllInBox(box, [&func, &localValidCount] (void *object)
        {
            auto &line = *(Line *)object;
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

BspLeaf &Map::bspLeafAt(Vector2d const &point) const
{
    if (!d->bsp.tree)
        /// @throw MissingBspTreeError  No BSP data is available.
        throw MissingBspTreeError("Map::bspLeafAt", "No BSP data available");

    BspTree const *bspTree = d->bsp.tree;
    while (!bspTree->isLeaf())
    {
        auto &bspNode = bspTree->userData()->as<BspNode>();
        dint side     = bspNode.pointOnSide(point) < 0;

        // Descend to the child subspace on "this" side.
        bspTree = bspTree->childPtr(BspTree::ChildId(side));
    }

    // We've arrived at a leaf.
    return bspTree->userData()->as<BspLeaf>();
}

BspLeaf &Map::bspLeafAt_FixedPrecision(Vector2d const &point) const
{
    if (!d->bsp.tree)
        /// @throw MissingBspTreeError  No BSP data is available.
        throw MissingBspTreeError("Map::bspLeafAt_FixedPrecision", "No BSP data available");

    fixed_t pointX[2] = { DBL2FIX(point.x), DBL2FIX(point.y) };

    BspTree const *bspTree = d->bsp.tree;
    while (!bspTree->isLeaf())
    {
        auto const &bspNode = bspTree->userData()->as<BspNode>();

        fixed_t lineOriginX[2]    = { DBL2FIX(bspNode.origin.x),    DBL2FIX(bspNode.origin.y) };
        fixed_t lineDirectionX[2] = { DBL2FIX(bspNode.direction.x), DBL2FIX(bspNode.direction.y) };
        dint side = V2x_PointOnLineSide(pointX, lineOriginX, lineDirectionX);

        // Decend to the child subspace on "this" side.
        bspTree = bspTree->childPtr(BspTree::ChildId(side));
    }

    // We've arrived at a leaf.
    return bspTree->userData()->as<BspLeaf>();
}

#ifdef __CLIENT__

void Map::updateScrollingSurfaces()
{
    for (Surface *surface : d->scrollingSurfaces)
    {
        surface->updateOriginTracking();
    }
}

Map::SurfaceSet &Map::scrollingSurfaces()
{
    return d->scrollingSurfaces;
}

void Map::updateTrackedPlanes()
{
    for (Plane *plane : d->trackedPlanes)
    {
        plane->updateHeightTracking();
    }
}

Map::PlaneSet &Map::trackedPlanes()
{
    return d->trackedPlanes;
}

void Map::initSkyFix()
{
    Time begunAt;

    LOG_AS("Map::initSkyFix");

    d->skyFloor  .setHeight(DDMAXFLOAT);
    d->skyCeiling.setHeight(DDMINFLOAT);

    // Update for sector plane heights and mobjs which intersect the ceiling.
    /// @todo Can't we defer this?
    for (Sector *sector : d->sectors)
    {
        if (!sector->sideCount()) continue;

        bool const skyFloor = sector->floor  ().surface().hasSkyMaskedMaterial();
        bool const skyCeil  = sector->ceiling().surface().hasSkyMaskedMaterial();

        if (!skyFloor && !skyCeil) continue;

        if (skyCeil)
        {
            // Adjust for the plane height.
            if (sector->ceiling().heightSmoothed() > d->skyCeiling.height())
            {
                // Must raise the skyfix ceiling.
                d->skyCeiling.setHeight(sector->ceiling().heightSmoothed());
            }

            // Check that all the mobjs in the sector fit in.
            for (mobj_t *mob = sector->firstMobj(); mob; mob = mob->sNext)
            {
                ddouble extent = mob->origin[2] + mob->height;
                if (extent > d->skyCeiling.height())
                {
                    // Must raise the skyfix ceiling.
                    d->skyCeiling.setHeight(extent);
                }
            }
        }

        if (skyFloor)
        {
            // Adjust for the plane height.
            if (sector->floor().heightSmoothed() < d->skyFloor.height())
            {
                // Must lower the skyfix floor.
                d->skyFloor.setHeight(sector->floor().heightSmoothed());
            }
        }

        // Update for middle materials on lines which intersect the floor and/or ceiling
        // on the front (i.e., sector) side.
        sector->forAllSides([this, &skyCeil, &skyFloor] (LineSide &side)
        {
            if (!side.hasSections()) return LoopContinue;
            if (!side.middle().hasMaterial()) return LoopContinue;

            // There must be a sector on both sides.
            if (!side.hasSector() || !side.back().hasSector())
                return LoopContinue;

            // Possibility of degenerate BSP leaf.
            if (!side.leftHEdge()) return LoopContinue;

            WallEdge edge(WallSpec::fromMapSide(side, LineSide::Middle),
                          *side.leftHEdge(), Line::From);

            if (edge.isValid() && edge.top().z() > edge.bottom().z())
            {
                if (skyCeil && edge.top().z() + edge.origin().y > d->skyCeiling.height())
                {
                    // Must raise the skyfix ceiling.
                    d->skyCeiling.setHeight(edge.top().z() + edge.origin().y);
                }

                if (skyFloor && edge.bottom().z() + edge.origin().y < d->skyFloor.height())
                {
                    // Must lower the skyfix floor.
                    d->skyFloor.setHeight(edge.bottom().z() + edge.origin().y);
                }
            }
            return LoopContinue;
        });
    }

    LOGDEV_MAP_VERBOSE("Completed in %.2f seconds") << begunAt.since();
}

ClSkyPlane &Map::skyFloor()
{
    return d->skyFloor;
}

ClSkyPlane const &Map::skyFloor() const
{
    return d->skyFloor;
}

ClSkyPlane &Map::skyCeiling()
{
    return d->skyCeiling;
}

ClSkyPlane const &Map::skyCeiling() const
{
    return d->skyCeiling;
}

Generator *Map::newGenerator()
{
    Generator::Id id = d->findIdForNewGenerator();  // 1-based
    if (!id) return nullptr;  // Failed; too many generators?

    Impl::Generators &gens = d->getGenerators();

    // If there is already a generator with that id - remove it.
    if (id > 0 && (unsigned)id <= gens.activeGens.size())
    {
        Generator_Delete(gens.activeGens[id - 1]);
    }

    /// @todo Linear allocation when in-game is not good...
    auto *gen = (Generator *) Z_Calloc(sizeof(Generator), PU_MAP, 0);

    gen->setId(id);

    // Link the thinker to the list of (private) thinkers.
    gen->thinker.function = (thinkfunc_t) Generator_Thinker;
    d->thinkers->add(gen->thinker, false /*not public*/);

    // Link the generator into the collection.
    gens.activeGens[id - 1] = gen;

    return gen;
}

dint Map::generatorCount() const
{
    if (!d->generators) return 0;
    dint count = 0;
    for (Generator *gen : d->getGenerators().activeGens)
    {
        if (gen) count += 1;
    }
    return count;
}

void Map::unlink(Generator &generator)
{
    Impl::Generators &gens = d->getGenerators();
    for (duint i = 0; i < gens.activeGens.size(); ++i)
    {
        if (gens.activeGens[i] == &generator)
        {
            gens.activeGens[i] = nullptr;
            break;
        }
    }
}

LoopResult Map::forAllGenerators(std::function<LoopResult (Generator &)> func) const
{
    for (Generator *gen : d->getGenerators().activeGens)
    {
        if (!gen) continue;

        if (auto result = func(*gen))
            return result;
    }
    return LoopContinue;
}

LoopResult Map::forAllGeneratorsInSector(Sector const &sector, std::function<LoopResult (Generator &)> func) const
{
    if (sector.mapPtr() == this)  // Ignore 'alien' sectors.
    {
        duint const listIndex = sector.indexInMap();

        Impl::Generators &gens = d->getGenerators();
        for (Impl::Generators::ListNode *it = gens.lists[listIndex]; it; it = it->next)
        {
            if (auto result = func(*it->gen))
                return result;
        }
    }
    return LoopContinue;
}

dint Map::lumobjCount() const
{
    return d->lumobjs.count();
}

Lumobj &Map::addLumobj(Lumobj *lumobj)
{
    DENG2_ASSERT(lumobj != nullptr);

    d->lumobjs.append(lumobj);

    lumobj->setMap(this);
    lumobj->setIndexInMap(d->lumobjs.count() - 1);
    DENG2_ASSERT(lumobj->bspLeafAtOrigin().hasSubspace());
    lumobj->bspLeafAtOrigin().subspace().link(*lumobj);
    R_AddContact(*lumobj);  // For spreading purposes.

    return *lumobj;
}

void Map::removeLumobj(dint which)
{
    if (which >= 0 && which < lumobjCount())
    {
        delete d->lumobjs.takeAt(which);
    }
}

void Map::removeAllLumobjs()
{
    for (ConvexSubspace *subspace : d->subspaces)
    {
        subspace->unlinkAllLumobjs();
    }
    qDeleteAll(d->lumobjs); d->lumobjs.clear();
}

Lumobj &Map::lumobj(dint index) const
{
    if (Lumobj *lum = lumobjPtr(index)) return *lum;
    /// @throw MissingObjectError  Invalid Lumobj reference specified.
    throw MissingObjectError("Map::lumobj", "Unknown Lumobj index:" + String::number(index));
}

Lumobj *Map::lumobjPtr(dint index) const
{
    if (index >= 0 && index < d->lumobjs.count())
    {
        return d->lumobjs.at(index);
    }
    return nullptr;
}

LoopResult Map::forAllLumobjs(std::function<LoopResult (Lumobj &)> func) const
{
    for (Lumobj *lob : d->lumobjs)
    {
        if (auto result = func(*lob)) return result;
    }
    return LoopContinue;
}

#if 0
dint Map::biasSourceCount() const
{
    return d->bias.sources.count();
}

BiasSource &Map::addBiasSource(BiasSource const &biasSource)
{
    if (biasSourceCount() < MAX_BIAS_SOURCES)
    {
        d->bias.sources.append(new BiasSource(biasSource));
        return *d->bias.sources.last();
    }
    /// @throw FullError  Attempt to add a new bias source when already at capcity.
    throw FullError("Map::addBiasSource", "Already at full capacity:" + String::number(MAX_BIAS_SOURCES));
}

void Map::removeBiasSource(dint which)
{
    if (which >= 0 && which < biasSourceCount())
    {
        delete d->bias.sources.takeAt(which);
    }
}

void Map::removeAllBiasSources()
{
    qDeleteAll(d->bias.sources); d->bias.sources.clear();
}

BiasSource &Map::biasSource(dint index) const
{
   if (BiasSource *bsrc = biasSourcePtr(index)) return *bsrc;
   /// @throw MissingObjectError  Invalid BiasSource reference specified.
   throw MissingObjectError("Map::biasSource", "Unknown BiasSource index:" + String::number(index));
}

BiasSource *Map::biasSourcePtr(dint index) const
{
   if (index >= 0 && index < d->bias.sources.count())
   {
       return d->bias.sources.at(index);
   }
   return nullptr;
}

/**
 * @todo Implement a blockmap for these?
 * @todo Cache this result (MRU?).
 */
BiasSource *Map::biasSourceNear(Vector3d const &point) const
{
    BiasSource *nearest = nullptr;
    ddouble minDist = 0;
    for (BiasSource *src : d->bias.sources)
    {
        ddouble dist = (src->origin() - point).length();
        if (!nearest || dist < minDist)
        {
            minDist = dist;
            nearest = src;
        }
    }
    return nearest;
}

LoopResult Map::forAllBiasSources(std::function<LoopResult (BiasSource &)> func) const
{
    for (BiasSource *bsrc : d->bias.sources)
    {
        if (auto result = func(*bsrc)) return result;
    }
    return LoopContinue;
}

dint Map::indexOf(BiasSource const &bsrc) const
{
    return d->bias.sources.indexOf(const_cast<BiasSource *>(&bsrc));
}

duint Map::biasCurrentTime() const
{
    return d->bias.currentTime;
}

duint Map::biasLastChangeOnFrame() const
{
    return d->bias.lastChangeOnFrame;
}
#endif // 0

#endif // __CLIENT__

void Map::update()
{
#ifdef __CLIENT__
    d->updateParticleGens();  // Defs might've changed.

    // Update all surfaces.
    for (Sector *sector : d->sectors)
    {
        sector->forAllSubsectors([] (Subsector &subsector)
        {
            subsector.as<ClientSubsector>().markForDecorationUpdate();
            return LoopContinue;
        });
    }
#endif // __CLIENT__

    // Reapply values defined in MapInfo (they may have changed).
    Record const &inf = mapInfo();

    _ambientLightLevel = inf.getf("ambient") * 255;
    _globalGravity     = inf.getf("gravity");
    _effectiveGravity  = _globalGravity;

#ifdef __CLIENT__
    // Reconfigure the sky.
    /// @todo Sky needs breaking up into multiple components. There should be
    /// a representation on server side and a logical entity which the renderer
    /// visualizes. We also need multiple concurrent skies for BOOM support.
    defn::Sky skyDef;
    if (Record const *def = DED_Definitions()->skies.tryFind("id", inf.gets("skyId")))
    {
        skyDef = *def;
    }
    else
    {
        skyDef = inf.subrecord("sky");
    }
    sky().configure(&skyDef);
#endif
}

#ifdef __CLIENT__

String Map::objectsDescription() const
{
    String str;
    QTextStream os(&str);

    if (gx.MobjStateAsInfo)
    {
        // Print out a state description for each thinker.
        thinkers().forAll(0x3, [&os] (thinker_t *th)
        {
            if (Thinker_IsMobj(th))
            {
                os << gx.MobjStateAsInfo(reinterpret_cast<mobj_t const *>(th));
            }
            return LoopContinue;
        });
    }

    return str;
}

void Map::restoreObjects(Info const &objState, IThinkerMapping const &thinkerMapping) const
{
    /// @todo Generalize from mobjs to all thinkers?
    LOG_AS("Map::restoreObjects");

    if (!gx.MobjStateAsInfo || !gx.MobjRestoreState) return;

    bool problemsDetected = false;

    // Look up all the mobjs.
    QList<thinker_t const *> mobjs;
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
        Info::BlockElement const &state = (*i)->as<Info::BlockElement>();
        Id::Type const privateId = state.name().toUInt32();
        DENG2_ASSERT(privateId != 0);

        if (thinker_t *th = thinkerMapping.thinkerForPrivateId(privateId))
        {
            if (ThinkerData *found = ThinkerData::find(privateId))
            {
                DENG2_ASSERT(&found->thinker() == th);

                // Restore the state according to the serialized info.
                gx.MobjRestoreState(found->as<MobjThinkerData>().mobj(), state);

                // Verify that the state is now correct.
                {
                    Info const currentDesc(gx.MobjStateAsInfo(found->as<MobjThinkerData>().mobj()));
                    Info::BlockElement const &currentState = currentDesc.root().contentsInOrder()
                            .first()->as<Info::BlockElement>();
                    DENG2_ASSERT(currentState.name() == state.name());
                    foreach (String const &key, state.contents().keys())
                    {
                        if (state.keyValue(key).text != currentState.keyValue(key).text)
                        {
                            problemsDetected = true;
                            const String msg = String("Object %1 has mismatching '%2' (current:%3 != arch:%4)")
                                    .arg(privateId)
                                    .arg(key)
                                    .arg(currentState.keyValue(key).text)
                                    .arg(state.keyValue(key).text);
                            LOGDEV_MAP_WARNING("%s") << msg;
                        }
                    }
                }
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
        LOG_MAP_WARNING("Map objects were not fully restored " DENG2_CHAR_MDASH
                        " gameplay may be affected (enable Developer log entries for details)");
    }
    else
    {
        LOGDEV_MAP_MSG("State of map objects has been restored");
    }
}

void Map::serializeInternalState(Writer &to) const
{
    BaseMap::serializeInternalState(to);

    // Internal state of thinkers.
    thinkers().forAll(0x3, [&to] (thinker_t *th)
    {
        if (th->d)
        {
            ThinkerData const &thinkerData = THINKER_DATA(*th, ThinkerData);
            if (ISerializable const *serial = THINKER_DATA_MAYBE(*th, ISerializable))
            {
                to << thinkerData.id()
                   << Writer::BeginSpan
                   << *serial
                   << Writer::EndSpan;
            }
        }
        return LoopContinue;
    });

    // Terminator.
    to << Id(Id::None);
}

void Map::deserializeInternalState(Reader &from, IThinkerMapping const &thinkerMapping)
{
    BaseMap::deserializeInternalState(from, thinkerMapping);

    try
    {
        // Internal state of thinkers.
        forever
        {
            Id id { Id::None };
            from >> id;
            if (!id) break; // Zero ID terminates the sequence.

            // Span length.
            duint32 size = 0;
            from >> size;

            auto const nextOffset = from.offset() + size;

            //qDebug() << "Found serialized internal state for private ID" << id.asText() << "size" << size;

            try
            {
                if (thinker_t *th = thinkerMapping.thinkerForPrivateId(id))
                {
                    // The identifier is changed if necessary.
                    Thinker_InitPrivateData(th, id);
                    if (ISerializable *serial = THINKER_DATA_MAYBE(*th, ISerializable))
                    {
                        from >> *serial;
                    }
                    else
                    {
                        LOG_MAP_WARNING("State for thinker %i is not deserializable "
                                        DENG2_CHAR_MDASH " internal representation may have "
                                        "changed, or save data is corrupt") << id;
                    }
                }
            }
            catch (Error const &er)
            {
                LOG_MAP_WARNING("Error when reading state of object %s: %s")
                        << id << er.asText();
            }

            from.setOffset(nextOffset);
        }
    }
    catch (Error const &er)
    {
        LOG_MAP_WARNING("Error when reading state: %s") << er.asText();
    }
}

void Map::redecorate()
{
    forAllSectors([](Sector &sector) {
        sector.forAllSubsectors([](Subsector &subsec) {
            subsec.as<ClientSubsector>().markForDecorationUpdate();
            return LoopContinue;
        });
        return LoopContinue;
    });
}

void Map::worldSystemFrameBegins(bool resetNextViewer)
{
    DENG2_ASSERT(&App_World().map() == this); // Sanity check.

    // Interpolate the map ready for drawing view(s) of it.
    d->lerpTrackedPlanes(resetNextViewer);
    d->lerpScrollingSurfaces(resetNextViewer);

    if (!freezeRLs)
    {
#if 0
        // Initialize and/or update the LightGrid.
        initLightGrid();

        d->biasBeginFrame();
#endif

        removeAllLumobjs();

        d->removeAllContacts();

        // Generate surface decorations for the frame.
        if (useLightDecorations)
        {
            for (Sector *sector : d->sectors)
            {
                sector->forAllSubsectors([] (Subsector &ssec)
                {
                    auto &clSubsector = ssec.as<ClientSubsector>();

                    // Perform scheduled redecoration.
                    clSubsector.decorate();

                    // Generate lumobjs for all decorations who want them.
                    clSubsector.generateLumobjs();
                    return LoopContinue;
                });
            }
        }

        // Spawn omnilights for mobjs?
        if (useDynLights)
        {
            for (Sector *sector : d->sectors)
            for (mobj_t *iter = sector->firstMobj(); iter; iter = iter->sNext)
            {
                Mobj_GenerateLumobjs(iter);
            }
        }

        d->generateMobjContacts();

        d->linkAllParticles();
        d->linkAllContacts();
    }
}

/// @return  @c false= Continue iteration.
static dint expireClMobjsWorker(mobj_t *mob, void *context)
{
    duint const nowTime = *static_cast<duint *>(context);

    // Already deleted?
    if (mob->thinker.function == (thinkfunc_t)-1)
        return 0;

    // Don't expire player mobjs.
    if (mob->dPlayer) return 0;

    ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(mob);
    DENG2_ASSERT(info);

    if ((info->flags & (CLMF_UNPREDICTABLE | CLMF_HIDDEN | CLMF_NULLED)) || !mob->info)
    {
        // Has this mobj timed out?
        if (nowTime - info->time > CLMOBJ_TIMEOUT)
        {
            LOGDEV_MAP_VERBOSE("Mobj %i has expired (%i << %i), in state %s [%c%c%c]")
                << mob->thinker.id
                << info->time << nowTime
                << Def_GetStateName(mob->state)
                << (info->flags & CLMF_UNPREDICTABLE? 'U' : '_')
                << (info->flags & CLMF_HIDDEN?        'H' : '_')
                << (info->flags & CLMF_NULLED?        '0' : '_');

            // Too long. The server will probably never send anything for this map-object,
            // so get rid of it. (Both unpredictable and hidden mobjs are not visible or
            // bl/seclinked.)
            Mobj_Destroy(mob);
        }
    }

    return 0;
}

void Map::expireClMobjs()
{
    duint nowTime = Timer_RealMilliseconds();
    clMobjIterator(expireClMobjsWorker, &nowTime);
}

#endif // __CLIENT__

String Map::elementSummaryAsStyledText() const
{
#define TABBED(count, label) String(_E(Ta) "  %1 " _E(Tb) "%2\n").arg(count).arg(label)

    String str;
    QTextStream os(&str);

    if (lineCount())    os << TABBED(lineCount(),    "Lines");
    //if (sideCount())    os << TABBED(sideCount(),    "Sides");
    if (sectorCount())  os << TABBED(sectorCount(),  "Sectors");
    if (vertexCount())  os << TABBED(vertexCount(),  "Vertexes");
    if (polyobjCount()) os << TABBED(polyobjCount(), "Polyobjs");

    return str.rightStrip();

#undef TABBED
}

String Map::objectSummaryAsStyledText() const
{
#define TABBED(count, label) String(_E(Ta) "  %1 " _E(Tb) "%2\n").arg(count).arg(label)

    dint thCountInStasis = 0;
    dint thCount = thinkers().count(&thCountInStasis);

    String str;
    QTextStream os(&str);

    if (thCount)           os << TABBED(thCount,            String("Thinkers (%1 in stasis)").arg(thCountInStasis));
#ifdef __CLIENT__
    //if (biasSourceCount()) os << TABBED(biasSourceCount(),  "Bias Sources");
    if (generatorCount())  os << TABBED(generatorCount(),   "Generators");
    if (lumobjCount())     os << TABBED(lumobjCount(),      "Lumobjs");
#endif

    return str.rightStrip();

#undef TABBED
}

D_CMD(InspectMap)
{
    DENG2_UNUSED3(src, argc, argv);

    LOG_AS("inspectmap (Cmd)");

    if (!App_World().hasMap())
    {
        LOG_SCR_WARNING("No map is currently loaded");
        return false;
    }

    Map &map = App_World().map();

    LOG_SCR_NOTE(_E(b) "%s - %s")
            << Con_GetString("map-name")
            << Con_GetString("map-author");
    LOG_SCR_MSG("\n");

    LOG_SCR_MSG(    _E(l) "Uri: "    _E(.) _E(i) "%s" _E(.)
              /*" " _E(l) " OldUid: " _E(.) _E(i) "%s" _E(.)*/
                    _E(l) " Music: "  _E(.) _E(i) "%i")
            << (map.hasManifest()? map.manifest().composeUri().asText() : "(unknown map)")
            /*<< map.oldUniqueId()*/
            << Con_GetInteger("map-music");

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

    auto geometryDimensions = Vector2d(map.bounds().max) - Vector2d(map.bounds().min);
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

#if 0
#ifdef __CLIENT__
    if (map.hasLightGrid())
    {
        LOG_SCR_MSG(_E(l) "LightGrid: " _E(.) _E(i)) << map.lightGrid().dimensions().asText();
    }
#endif
#endif // 0

    return true;

#undef TABBED
}

void Map::consoleRegister() // static
{
    Line::consoleRegister();
    Mobj_ConsoleRegister();
    Sector::consoleRegister();

    C_VAR_INT("bsp-factor",                 &bspSplitFactor, CVF_NO_MAX, 0, 0);
#if 0
#ifdef __CLIENT__
    C_VAR_INT("rend-bias-grid-multisample", &lgMXSample,     0, 0, 7);
#endif
#endif

    C_CMD("inspectmap", "", InspectMap);
}

//- Runtime map editing -----------------------------------------------------------------

/// Used when sorting vertex line owners.
static Vertex *rootVtx;

/**
 * Compares the angles of two lines that share a common vertex.
 *
 * pre: rootVtx must point to the vertex common between a and b
 *      which are (lineowner_t*) ptrs.
 */
static dint lineAngleSorter(void const *a, void const *b)
{
    binangle_t angles[2];

    LineOwner *own[2] = { (LineOwner *)a, (LineOwner *)b };
    for (duint i = 0; i < 2; ++i)
    {
        if (own[i]->_link[Anticlockwise]) // We have a cached result.
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
    dint (*compare) (void const *a, void const *b))
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
    dint (*compare) (void const *a, void const *b))
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

static void setVertexLineOwner(Vertex *vtx, Line *lineptr, LineOwner **storage)
{
    if (!lineptr) return;

    // Has this line already been registered with this vertex?
    LineOwner const *own = vtx->firstLineOwner();
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
    newOwner->_link[Anticlockwise] = nullptr;

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

#ifdef DENG2_DEBUG
/**
 * Determines whether the specified vertex @a v has a correctly formed line owner ring.
 */
static bool vertexHasValidLineOwnerRing(Vertex &v)
{
    LineOwner const *base = v.firstLineOwner();
    LineOwner const *cur = base;
    do
    {
        if (cur->prev()->next() != cur) return false;
        if (cur->next()->prev() != cur) return false;

    } while ((cur = cur->next()) != base);
    return true;
}
#endif

/**
 * Generates the line owner rings for each vertex. Each ring includes all the lines which
 * the vertex belongs to sorted by angle, (the rings are arranged in clockwise order, east = 0).
 */
void buildVertexLineOwnerRings(QList<Vertex *> const &vertexs, QList<Line *> &editableLines)
{
    LOG_AS("buildVertexLineOwnerRings");

    //
    // Step 1: Find and link up all line owners.
    //
    // We know how many vertex line owners we need (numLines * 2).
    auto *lineOwners = (LineOwner *) Z_Malloc(sizeof(LineOwner) * editableLines.count() * 2, PU_MAPSTATIC, 0);
    LineOwner *allocator = lineOwners;

    for (Line *line : editableLines)
    for (dint p = 0; p < 2; ++p)
    {
        setVertexLineOwner(&line->vertex(p), line, &allocator);
    }

    //
    // Step 2: Sort line owners of each vertex and finalize the rings.
    //
    for (Vertex *v : vertexs)
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
            p->_link[Anticlockwise] = last;

            // Convert to a relative angle between last and this.
            last->_angle = last->angle() - p->angle();

            last = p;
            p = p->next();
        }
        last->_link[Clockwise] = v->_lineOwners;
        v->_lineOwners->_link[Anticlockwise] = last;

        // Set the angle of the last owner.
        last->_angle = last->angle() - firstAngle;

/*#ifdef DENG2_DEBUG
        LOG_MAP_VERBOSE("Vertex #%i: line owners #%i")
            << editmap.vertexes.indexOf(v) << v->lineOwnerCount();

        LineOwner const *base = v->firstLineOwner();
        LineOwner const *cur = base;
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
        DENG2_ASSERT(vertexHasValidLineOwnerRing(*v));
    }
}

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
    dint compareVertexOrigins(VertexInfo const &other) const
    {
        DENG2_ASSERT(vertex && other.vertex);

        if (this == &other) return 0;
        if (vertex == other.vertex) return 0;

        // Order is firstly X axis major.
        if (dint(vertex->origin().x) != dint(other.vertex->origin().x))
        {
            return dint(vertex->origin().x) - dint(other.vertex->origin().x);
        }

        // Order is secondly Y axis major.
        return dint(vertex->origin().y) - dint(other.vertex->origin().y);
    }

    bool operator < (VertexInfo const &other) const
    {
        return compareVertexOrigins(other) < 0;
    }
};

void pruneVertexes(Mesh &mesh, Map::Lines const &lines)
{
    //
    // Step 1 - Find equivalent vertexes:
    //
    // Populate the vertex info.
    QVector<VertexInfo> vertexInfo(mesh.vertexCount());
    dint ord = 0;
    for (Vertex *vertex : mesh.vertexs())
    {
        vertexInfo[ord++].vertex = vertex;
    }

    {
        // Sort a copy to place near vertexes adjacently.
        QVector<VertexInfo> sortedInfo(vertexInfo);
        qSort(sortedInfo.begin(), sortedInfo.end());

        // Locate equivalent vertexes in the sorted info.
        for (dint i = 0; i < sortedInfo.count() - 1; ++i)
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
    dint prunedCount = 0, numUnused = 0;
    for (VertexInfo const &info : vertexInfo)
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
        dint ord = 0;
        for (Vertex *vertex : mesh.vertexs())
        {
            vertex->setIndexInMap(ord++);
        }

        LOGDEV_MAP_NOTE("Pruned %d vertexes (%d equivalents, %d unused)")
            << prunedCount << (prunedCount - numUnused) << numUnused;
    }
}

enum {
    LinkFloorBit           = 0x1,
    LinkCeilingBit         = 0x2,
    FlatBleedingFloorBit   = 0x4,
    FlatBleedingCeilingBit = 0x8,
    InvisibleFloorBit      = 0x10,
    InvisibleCeilingBit    = 0x20,
};

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

    buildVertexLineOwnerRings(d->mesh.vertexs(), d->editable.lines);

    //
    // Move the editable elements to the "static" element lists.
    //
    // Collate sectors:
    DENG2_ASSERT(d->sectors.isEmpty());
    d->sectors.reserve(d->editable.sectors.count());
    d->sectors.append(d->editable.sectors);
    d->editable.sectors.clear();

    // Collate lines:
    DENG2_ASSERT(d->lines.isEmpty());
    d->lines.reserve(d->editable.lines.count());
    d->lines.append(d->editable.lines);
    d->editable.lines.clear();

    // Collate polyobjs:
    DENG2_ASSERT(d->polyobjs.isEmpty());
    d->polyobjs.reserve(d->editable.polyobjs.count());
    while (!d->editable.polyobjs.isEmpty())
    {
        d->polyobjs.append(d->editable.polyobjs.takeFirst());
        Polyobj *polyobj = d->polyobjs.back();

        // Create half-edge geometry and line segments for each line.
        for (Line *line : polyobj->lines())
        {
            HEdge *hedge = polyobj->mesh().newHEdge(line->from());

            hedge->setTwin(polyobj->mesh().newHEdge(line->to()));
            hedge->twin().setTwin(hedge);

            LineSideSegment *seg = line->front().addSegment(*hedge);
#ifdef __CLIENT__
            seg->setLength(line->length());
#else
            DENG2_UNUSED(seg);
#endif
        }

        polyobj->buildUniqueVertexes();
        polyobj->updateOriginalVertexCoords();
    }

    // Determine the map bounds.
    d->updateBounds();
    LOG_MAP_VERBOSE("Geometry bounds:") << Rectangled(d->bounds.min, d->bounds.max).asText();

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
        line->forAllSides([] (Line::Side &side)
        {
            side.updateAllSurfaceNormals();
            return LoopContinue;
        });
    }

    // Finish sectors.
    std::map<int, Sector *> sectorsByArchiveIndex;
    for (Sector *sector : d->sectors)
    {
        sectorsByArchiveIndex[sector->indexInArchive()] = sector;

        d->buildSubsectors(*sector);
        sector->buildSides();
        sector->chainSoundEmitters();
    }

    // Finish planes.
    for (Sector *sector : d->sectors)
    {
#if defined(__CLIENT__)
        if (sector->visPlaneLinkTargetSector() != MapElement::NoIndex)
        {
            if (Sector *target = sectorsByArchiveIndex[sector->visPlaneLinkTargetSector()])
            {
                // Use the first subsector as the target.
                auto &targetSub = target->subsector(0).as<ClientSubsector>();

                int linkModes[2]{};
                if (sector->visPlaneBits() & FlatBleedingFloorBit)
                {
                    linkModes[Sector::Floor] |= ClientSubsector::LinkWhenLowerThanTarget;
                }
                if (sector->visPlaneBits() & FlatBleedingCeilingBit)
                {
                    linkModes[Sector::Ceiling] |= ClientSubsector::LinkWhenHigherThanTarget;
                }
                if (sector->visPlaneBits() & InvisibleFloorBit)
                {
                    linkModes[Sector::Floor] |= ClientSubsector::LinkWhenHigherThanTarget;
                }
                if (sector->visPlaneBits() & InvisibleCeilingBit)
                {
                    linkModes[Sector::Ceiling] |= ClientSubsector::LinkWhenLowerThanTarget;
                }

                // Fallback is to link always.
                for (auto &lm : linkModes)
                {
                    if (lm == 0) lm = ClientSubsector::LinkAlways;
                }

                // Linking is done for each subsector separately. (Necessary, though?)
                sector->forAllSubsectors([&targetSub, sector, linkModes](Subsector &sub) {
                    auto &clsub = sub.as<ClientSubsector>();
                    for (int plane = 0; plane < 2; ++plane)
                    {
                        if (sector->isVisPlaneLinked(plane))
                        {
                            clsub.linkVisPlane(plane,
                                               targetSub,
                                               ClientSubsector::VisPlaneLinkMode(linkModes[plane]));
                        }
                    }
                    return LoopContinue;
                });
            }
        }
#endif
        sector->forAllPlanes([] (Plane &plane)
        {
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

Vertex *Map::createVertex(Vector2d const &origin, dint archiveIndex)
{
    if (!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::createVertex", "Editing is not enabled");

    Vertex *vtx = d->mesh.newVertex(origin);

    vtx->setMap(this);
    vtx->setIndexInArchive(archiveIndex);

    vtx->setIndexInMap(d->mesh.vertexCount() - 1);

    return vtx;
}

Line *Map::createLine(Vertex &v1, Vertex &v2, int flags, Sector *frontSector,
                      Sector *backSector, int archiveIndex)
{
    if (!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::createLine", "Editing is not enabled");

    auto *line = new Line(v1, v2, flags, frontSector, backSector);
    d->editable.lines.append(line);

    line->setMap(this);
    line->setIndexInArchive(archiveIndex);

    line->setIndexInMap(d->editable.lines.count() - 1);
    line->front().setIndexInMap(Map::toSideIndex(line->indexInMap(), Line::Front));
    line->back ().setIndexInMap(Map::toSideIndex(line->indexInMap(), Line::Back));

    return line;
}

Sector *Map::createSector(float lightLevel, const Vector3f &lightColor, int archiveIndex,
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
        int linkFlags = 0;

        // Which planes to link.
        if (hacks->flags.linkFloorPlane)   linkFlags |= LinkFloorBit;
        if (hacks->flags.linkCeilingPlane) linkFlags |= LinkCeilingBit;

        // When to link the planes.
        if (hacks->flags.missingInsideBottom)  linkFlags |= FlatBleedingFloorBit;
        if (hacks->flags.missingInsideTop)     linkFlags |= FlatBleedingCeilingBit;
        if (hacks->flags.missingOutsideBottom) linkFlags |= InvisibleFloorBit;
        if (hacks->flags.missingOutsideTop)    linkFlags |= InvisibleCeilingBit;

        sector->setVisPlaneLinks(hacks->visPlaneLinkTargetSector, linkFlags);
    }

    sector->setIndexInMap(d->editable.sectors.count() - 1);

    return sector;
}

Polyobj *Map::createPolyobj(Vector2d const &origin)
{
    if (!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::createPolyobj", "Editing is not enabled");

    void *region = M_Calloc(gx.GetInteger(DD_POLYOBJ_SIZE));
    auto *pob = new (region) Polyobj(origin);
    d->editable.polyobjs.append(pob);

    /// @todo Don't do this here.
    pob->setIndexInMap(d->editable.polyobjs.count() - 1);

    return pob;
}

Map::Lines const &Map::editableLines() const
{
    if (!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::editableLines", "Editing is not enabled");
    return d->editable.lines;
}

Map::Sectors const &Map::editableSectors() const
{
    if (!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::editableSectors", "Editing is not enabled");
    return d->editable.sectors;
}

void Map::initMapOutlinePacket(shell::MapOutlinePacket &packet)
{
    forAllLines([&packet] (Line &line)
    {
        packet.addLine(line.from().origin().toVector2i(),
                       line.to  ().origin().toVector2i(),
                       (line.front().hasSector() && line.back().hasSector()) ?
                           shell::MapOutlinePacket::TwoSidedLine : shell::MapOutlinePacket::OneSidedLine);
        return LoopContinue;
    });
}

Map::Polyobjs const &Map::editablePolyobjs() const
{
    if (!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::editablePolyobjs", "Editing is not enabled");
    return d->editable.polyobjs;
}

}  // namespace world
