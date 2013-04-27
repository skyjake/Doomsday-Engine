/** @file partitioner.cpp BSP Partitioner.
 *
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright &copy; 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright &copy; 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright &copy; 1998-2000 Lee Killough <killough@rsn.hp.com>
 * @authors Copyright &copy; 1997-1998 Raphael.Quinet <raphael.quinet@eed.ericsson.se>
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
#include <vector>
#include <algorithm>

#include <QList>
#include <QHash>
#include <QtAlgorithms>

#include <de/Error>
#include <de/Log>

#include "BspLeaf"
#include "BspNode"
#include "HEdge"
#include "Vertex"

#include "map/bsp/hedgeintercept.h"
#include "map/bsp/hedgetip.h"
#include "map/bsp/lineinfo.h"
#include "map/bsp/linesegment.h"
#include "map/bsp/partitioncost.h"
#include "map/bsp/superblockmap.h"
#include "map/bsp/vertexinfo.h"

#include "render/r_main.h"  // validCount
#include "m_misc.h"         // M_BoxOnLineSide2

#include "map/bsp/partitioner.h"

using namespace de;
using namespace de::bsp;

/**
 * Used to model the list of half-plane intersections.
 */
class Intercept : public HEdgeIntercept
{
public:
    /**
     * Distance along the owning Intercepts in the map coordinate space.
     */
    double distance;

    Intercept(double distance, HEdgeIntercept const &hedgeIntercept)
        : HEdgeIntercept(hedgeIntercept), distance(distance)
    {}

    bool operator < (Intercept const &other) const {
        return distance < other.distance;
    }

    /**
     * Determine the distance between two intercepts. It does not matter
     * if the intercepts are from different half-planes.
     */
    double operator - (Intercept const &other) const {
        return distance - other.distance;
    }
};

typedef QList<Intercept> HEdgeIntercepts;

typedef QHash<MapElement *, BspTreeNode *> BuiltBspElementMap;

/// Used when sorting half-edges by angle around a map point.
typedef QList<HEdge *> HEdgeSortBuffer;

/// Used when collecting half-edges from to build a leaf. @todo Refactor away.
typedef QList<HEdge *> HEdgeList;

/**
 * LineRelationship delineates the possible logical relationships between two
 * line segments in the plane.
 */
enum LineRelationship
{
    Collinear = 0,
    Right,
    RightIntercept, ///< Right vertex intercepts.
    Left,
    LeftIntercept,  ///< Left vertex intercepts.
    Intersects
};

/**
 * @param fromDist     Perpendicular distance from the start vertex to the partition.
 * @param toDist       Perpendicular distance from the end vertex to the partition.
 * @param distEpsilon  Rounding threshold within which points are considered
 *                     to intercept of the partition line.
 *
 * @return  Logical relationship between the line and the partition.
 */
static LineRelationship lineRelationship(coord_t fromDist, coord_t toDist,
    coord_t distEpsilon = DIST_EPSILON);

static Vector2d findBspLeafCenter(BspLeaf const &leaf);

DENG2_PIMPL(Partitioner)
{
    /// Cost factor attributed to splitting a half-edge.
    int splitCostFactor;

    /// The map we are building BSP data for.
    GameMap const *map;

    /// Running totals of constructed BSP data objects.
    uint numNodes;
    uint numLeafs;
    uint numHEdges;
    uint numVertexes;

    /// Extended info about Lines in the current map.
    typedef std::vector<LineInfo> LineInfos;
    LineInfos lineInfos;

    /// Line segments in the plane.
    typedef QHash<HEdge *, LineSegment> LineSegments;
    LineSegments lineSegments;

    /// Extended info about Vertexes in the current map (including extras).
    /// @note May be larger than Instance::numVertexes (deallocation is lazy).
    typedef std::vector<VertexInfo> VertexInfos;
    VertexInfos vertexInfos;

    /// Extra vertexes allocated for the current map.
    /// @note May be larger than Instance::numVertexes (deallocation is lazy).
    typedef std::vector<Vertex *> Vertexes;
    Vertexes vertexes;

    /// Root node of our internal binary tree around which the final BSP data
    /// objects are constructed.
    BspTreeNode *rootNode;

    /// Mapping table which relates built BSP map elements to their counterpart
    /// in the internal tree.
    BuiltBspElementMap treeNodeMap;

    /**
     * The "current" binary space half-plane.
     */
    struct HPlane
    {
        /// The partition line.
        Partition _partition;

        /// Map line segment which is the basis for the half-plane.
        LineSegment _lineSegment;

        /// Intercept points along the half-plane.
        HEdgeIntercepts _intercepts;

        Partition const &partition() const { return _partition; }

        LineSegment const &lineSegment() const { return _lineSegment; }

        HEdgeIntercepts const &intercepts() const { return _intercepts; }

        /**
         * Reconfigure the half-plane according to the given line segment.
         *
         * @param newLineSeg  The "new" line segment to configure using.
         * @param hedge  Map half-edge for the segment (@todo refactor away).
         */
        void configure(LineSegment const &newLineSeg, HEdge const &hedge)
        {
            // A "mini segment" is never suitable.
            DENG_ASSERT(hedge.hasLineSide());

            LOG_AS("HPlane::configure");

            // Clear the list of intersection points.
            clearIntercepts();

            Line::Side &side = hedge.lineSide();
            _partition.origin    = side.from().origin();
            _partition.direction = side.to().origin() - side.from().origin();

            // Update/store a copy of the line segment.
            _lineSegment = newLineSeg;

            //LOG_DEBUG("line segment %p %s %s.")
            //    << de::dintptr(&newLineSeg)
            //    << _partition.origin.asText() << _partition.direction.asText();
        }

        /**
         * Determine the distance from @a vertex to the half-plane
         * origin (along the partition).
         *
         * @param vertex  Vertex to test.
         */
        coord_t distanceToVertex(Vertex const &vertex) const
        {
            coord_t vertexOriginV1[2] = { vertex.x(), vertex.y() };
            return V2d_PointLineParaDistance(vertexOriginV1, _lineSegment.direction,
                                             _lineSegment.pPara, _lineSegment.pLength);
        }

        void clearIntercepts()
        {
            _intercepts.clear();
        }

        /**
         * Search the list of intercepts for to see if there is one for the
         * specified @a vertex.
         *
         * @param vertex  The vertex to look for.
         *
         * @return  @c true iff an intercept for @a vertex was found.
         */
        bool haveInterceptForVertex(Vertex const &vertex) const
        {
            foreach(HEdgeIntercept const &icpt, _intercepts)
            {
                if(icpt.vertex == &vertex)
                    return true;
            }
            return false;
        }

        void interceptLineSegment(LineSegment const &lineSeg, Vertex const &vertex,
                                  bool leftSide, Sector *beforeSector = 0, Sector *afterSector = 0)
        {
            // Already present for this vertex?
            if(haveInterceptForVertex(vertex)) return;

            HEdgeIntercept inter;
            inter.vertex  = const_cast<Vertex *>(&vertex);
            inter.selfRef = lineSeg.lineSide && lineSeg.lineSide->line().isSelfReferencing();

            inter.before  = beforeSector;
            inter.after   = afterSector;

            _intercepts.append(Intercept(distanceToVertex(vertex), inter));
        }

        /**
         * Sort and then merge near-intercepts from the given list.
         *
         * @todo fixme: Logically this is very suspect. Implementing this logic by
         * merging near-intercepts at hplane level is wrong because this does
         * nothing about any intercepting half-edge vertices. Consequently, rather
         * than moving the existing vertices and welding them, this will result in
         * the creation of new gaps gaps along the partition and result in holes
         * (which buildHEdgesAtIntersectionGaps() will then warn about).
         *
         * This should be redesigned so that near-intercepting vertices are welded
         * in a stable manner (i.e., not incrementally, which can result in vertices
         * drifting away from the hplane). Logically, therefore, this should not
         * be done prior to creating hedges along the partition - instead this
         * should happen afterwards. -ds
         *
         * @param intercepts  The list of intercepts to be sorted (in place).
         */
        void sortAndMergeIntercepts()
        {
            qSort(_intercepts.begin(), _intercepts.end());

            for(int i = 0; i < _intercepts.count() - 1; ++i)
            {
                Intercept &cur  = _intercepts[i];
                Intercept &next = _intercepts[i+1];

                // Sanity check.
                double distance = next.distance - cur.distance;
                if(distance < -0.1)
                {
                    throw Error("HPlane::sortAndMergeIntercepts", QString("Invalid intercept order - %1 > %2")
                                                                      .arg(cur.distance, 0, 'f', 3)
                                                                      .arg(next.distance, 0, 'f', 3));
                }

                // Are we merging this pair?
                if(distance <= DIST_EPSILON)
                {
                    // Yes - merge the two intercepts into one.
                    cur.merge(next);

                    // Destroy the "next" intercept.
                    _intercepts.removeAt(i+1);

                    // Process the new "this" and "next" pairing.
                    i -= 1;
                }
            }
        }

#ifdef DENG_DEBUG
        void printIntercepts() const
        {
            uint index = 0;
            foreach(Intercept const &icpt, _intercepts)
            {
                LOG_DEBUG(" %u: >%1.2f ") << (index++) << icpt.distance;
                icpt.debugPrint();
            }
        }
#endif
    } hplane;

    /// @c true = a BSP for the current map has been built successfully.
    bool builtOk;

    Instance(Public *i, GameMap const &_map, int _splitCostFactor)
      : Base(i),
        splitCostFactor(_splitCostFactor),
        map(&_map),
        numNodes(0), numLeafs(0), numHEdges(0), numVertexes(0),
        rootNode(0),
        builtOk(false)
    {}

    ~Instance()
    {
        clearAllHEdgeTips();

        if(rootNode)
        {
            // If ownership of the all built BSP map elements has been claimed
            // this should be a no-op.
            clearAllBspElements();

            // Destroy our private BSP tree.
            delete rootNode;
        }
    }

    /**
     * Returns the associated VertexInfo for the given @a vertex.
     */
    VertexInfo &vertexInfo(Vertex const &vertex)
    {
        return vertexInfos[vertex.indexInMap()];
    }

    /**
     * Returns the associated LineSegment for the given @a hedge.
     */
    LineSegment &lineSegment(HEdge &hedge)
    {
        LineSegments::iterator found = lineSegments.find(&hedge);
        if(found != lineSegments.end())
        {
            return found.value();
        }
        throw Error("Partitioner::lineSegment", QString("Failed locating a LineSegment for 0x%1")
                                                    .arg(de::dintptr(&hedge), 0, 16));
    }

    /// @copydoc lineSegment()
    LineSegment const &lineSegment(HEdge const &hedge) const
    {
        return const_cast<LineSegment const &>(
                    const_cast<Instance *>(this)->lineSegment(const_cast<HEdge &>(hedge)));
    }

    struct testForWindowEffectParams
    {
        double frontDist, backDist;
        Sector *frontOpen, *backOpen;
        Line *frontLine, *backLine;
        Line *testLine;
        Vector2d m;
        bool castHoriz;

        testForWindowEffectParams()
            : frontDist(0), backDist(0), frontOpen(0), backOpen(0),
              frontLine(0), backLine(0), testLine(0), castHoriz(false)
        {}
    };

    static void testForWindowEffect2(Line &line, testForWindowEffectParams &p)
    {
        if(&line == p.testLine) return;
        /// @todo Should the presence of sections really affect this? -ds
        if(line.hasFrontSections() && line.hasBackSections() && line.isSelfReferencing()) return;
        //if(line._buildData.overlap || line.length() <= 0) return;

        double dist = 0;
        Sector *hitSector = 0;
        bool isFront = false;
        if(p.castHoriz)
        {
            if(de::abs(line.direction().y) < DIST_EPSILON)
                return;

            if((line.aaBox().maxY < p.m.y - DIST_EPSILON) ||
               (line.aaBox().minY > p.m.y + DIST_EPSILON))
                return;

            dist = (line.fromOrigin().x + (p.m.y - line.fromOrigin().y) * line.direction().x / line.direction().y) - p.m.x;

            isFront = ((p.testLine->direction().y > 0) != (dist > 0));
            dist = de::abs(dist);

            // Too close? (overlapping lines?)
            if(dist < DIST_EPSILON)
                return;

            hitSector = line.sectorPtr((p.testLine->direction().y > 0) ^ (line.direction().y > 0) ^ !isFront);
        }
        else // Cast vertically.
        {
            if(de::abs(line.direction().x) < DIST_EPSILON)
                return;

            if((line.aaBox().maxX < p.m.x - DIST_EPSILON) ||
               (line.aaBox().minX > p.m.x + DIST_EPSILON))
                return;

            dist = (line.fromOrigin().y + (p.m.x - line.fromOrigin().x) * line.direction().y / line.direction().x) - p.m.y;

            isFront = ((p.testLine->direction().x > 0) == (dist > 0));
            dist = de::abs(dist);

            hitSector = line.sectorPtr((p.testLine->direction().x > 0) ^ (line.direction().x > 0) ^ !isFront);
        }

        // Too close? (overlapping lines?)
        if(dist < DIST_EPSILON)
            return;

        if(isFront)
        {
            if(dist < p.frontDist)
            {
                p.frontDist = dist;
                p.frontOpen = hitSector;
                p.frontLine = &line;
            }
        }
        else
        {
            if(dist < p.backDist)
            {
                p.backDist = dist;
                p.backOpen = hitSector;
                p.backLine = &line;
            }
        }
    }

    static int testForWindowEffectWorker(Line *line, void *parms)
    {
        testForWindowEffect2(*line, *reinterpret_cast<testForWindowEffectParams *>(parms));
        return false; // Continue iteration.
    }

    bool lineMightHaveWindowEffect(Line const &line)
    {
        if(line.isFromPolyobj()) return false;
        if(line.hasFrontSections() && line.hasBackSections()) return false;
        if(!line.hasFrontSections()) return false;
        //if(line.length() <= 0 || line._buildData.overlap) return false;

        // Look for window effects by checking for an odd number of one-sided
        // line owners for a single vertex. Idea courtesy of Graham Jackson.
        VertexInfo const &fromInfo = vertexInfo(line.from());
        if((fromInfo.oneSidedOwnerCount % 2) == 1 &&
           (fromInfo.oneSidedOwnerCount + fromInfo.twoSidedOwnerCount) > 1)
            return true;

        VertexInfo const &toInfo = vertexInfo(line.to());
        if((toInfo.oneSidedOwnerCount % 2) == 1 &&
           (toInfo.oneSidedOwnerCount + toInfo.twoSidedOwnerCount) > 1)
            return true;

        return false;
    }

    void testForWindowEffect(LineInfo &lineInfo)
    {
        Line *line = lineInfo.line;
        DENG_ASSERT(line != 0);

        if(!lineMightHaveWindowEffect(*line)) return;

        testForWindowEffectParams p;
        p.frontDist = p.backDist = DDMAXFLOAT;
        p.testLine  = line;
        p.m         = line->center();
        p.castHoriz = (de::abs(line->direction().x) < de::abs(line->direction().y)? true : false);

        AABoxd scanRegion = map->bounds();
        if(p.castHoriz)
        {
            scanRegion.minY = line->aaBox().minY - DIST_EPSILON;
            scanRegion.maxY = line->aaBox().maxY + DIST_EPSILON;
        }
        else
        {
            scanRegion.minX = line->aaBox().minX - DIST_EPSILON;
            scanRegion.maxX = line->aaBox().maxX + DIST_EPSILON;
        }
        validCount++;
        map->linesBoxIterator(scanRegion, testForWindowEffectWorker, &p);

        if(p.backOpen && p.frontOpen && line->frontSectorPtr() == p.backOpen)
        {
            notifyOneWayWindowFound(*line, *p.frontOpen);

            lineInfo.windowEffect = p.frontOpen;
            line->_inFlags |= LF_BSPWINDOW; /// @todo Refactor away.
        }
    }

    void initForMap()
    {
        // Initialize vertex info for the initial set of vertexes.
        vertexInfos.resize(map->vertexCount());

        // Count the total number of one and two-sided line owners for each
        // vertex. (Used in the process of locating window effect lines.)
        foreach(Vertex *vtx, map->vertexes())
        {
            VertexInfo &vtxInfo = vertexInfo(*vtx);
            vtx->countLineOwners(&vtxInfo.oneSidedOwnerCount,
                                 &vtxInfo.twoSidedOwnerCount);
        }

        // Initialize line info.
        lineInfos.reserve(map->lineCount());
        foreach(Line *line, map->lines())
        {
            lineInfos.push_back(LineInfo(line, DIST_EPSILON));
            LineInfo &lineInfo = lineInfos.back();

            testForWindowEffect(lineInfo);
        }
    }

    /**
     * @return The right half-edge (from @a start to @a end).
     */
    HEdge *buildHEdgesBetweenVertexes(Vertex &start, Vertex &end,
        Sector *frontSec, Sector *backSec, Line::Side *frontSide,
        Line::Side *partitionLineSide)
    {
        HEdge *right = newHEdge(start, end, *frontSec, frontSide, partitionLineSide);
        if(!backSec) return right;

        HEdge *left  = newHEdge(end, start, *backSec, frontSide? &frontSide->back() : 0, partitionLineSide);

        // Twin the half-edges together.
        right->_twin = left;
        left->_twin  = right;

        return right;
    }

    inline void linkHEdgeInSuperBlockmap(SuperBlock &block, HEdge &hedge)
    {
        // Associate this half-edge with the final subblock.
        lineSegment(hedge).bmapBlock = &block.push(hedge);
    }

    /**
     * Create all initial half-edges and add them to specified SuperBlock.
     */
    void createInitialHEdges(SuperBlock &hedgeList)
    {
        DENG2_FOR_EACH(LineInfos, i, lineInfos)
        {
            LineInfo const &lineInfo = *i;
            Line *line = lineInfo.line;

            // Polyobj lines are completely ignored.
            if(line->isFromPolyobj()) continue;

            HEdge *front  = 0;
            coord_t angle = 0;

            if(!lineInfo.flags.testFlag(LineInfo::ZeroLength))
            {
                Sector *frontSec = line->frontSectorPtr();
                Sector *backSec  = 0;

                if(line->hasBackSections())
                {
                    backSec = line->backSectorPtr();
                }
                else
                {
                    // Handle the 'One-Way Window' effect.
                    Sector *windowSec = lineInfo.windowEffect;
                    if(windowSec) backSec = windowSec;
                }

                front = buildHEdgesBetweenVertexes(line->from(), line->to(),
                                                   frontSec, backSec, &line->front(), &line->front());

                linkHEdgeInSuperBlockmap(hedgeList, *front);
                if(front->hasTwin())
                {
                    linkHEdgeInSuperBlockmap(hedgeList, front->twin());
                }

                angle = lineSegment(*front).pAngle;
            }

            /// @todo edge tips should be created when half-edges are created.
            addHEdgeTip(line->from(), angle,                 front, front? front->twinPtr() : 0);
            addHEdgeTip(  line->to(), M_InverseAngle(angle), front? front->twinPtr() : 0, front);
        }
    }

    void buildBsp(SuperBlock &rootBlock)
    {
        try
        {
            createInitialHEdges(rootBlock);

            rootNode = divideSubspace(rootBlock);
            // At this point we know that something useful was built.
            builtOk = true;

            windLeafs();
        }
        catch(Error & /*er*/)
        {
            // Don't handle the error here, simply record failure.
            builtOk = false;
            throw;
        }
    }

    /**
     * @param sortedIntercepts  Sorted list of intercept points along the half-plane.
     * @param rightSet          The set of line segments on the "right" of the half-plane.
     * @param leftList          The set of line segments on the "left" of the half-plane.
     */
    void buildHEdgesAtPartitionGaps(HEdgeIntercepts const &sortedIntercepts,
                                    SuperBlock &rightSet, SuperBlock &leftSet)
    {
        for(int i = 0; i < sortedIntercepts.count() - 1; ++i)
        {
            Intercept const &cur  = sortedIntercepts[i];
            Intercept const &next = sortedIntercepts[i+1];

            if(!(!cur.after && !next.before))
            {
                // Check for some nasty open/closed or close/open cases.
                if(cur.after && !next.before)
                {
                    if(!cur.selfRef)
                    {
                        Vector2d nearPoint = (cur.vertex->origin() + next.vertex->origin()) / 2;
                        notifyUnclosedSectorFound(*cur.after, nearPoint);
                    }
                }
                else if(!cur.after && next.before)
                {
                    if(!next.selfRef)
                    {
                        Vector2d nearPoint = (cur.vertex->origin() + next.vertex->origin()) / 2;
                        notifyUnclosedSectorFound(*next.before, nearPoint);
                    }
                }
                else // This is definitely open space.
                {
                    // Choose the non-self-referencing sector when we can.
                    Sector *sector = cur.after;
                    if(cur.after != next.before)
                    {
                        if(!cur.selfRef && !next.selfRef)
                        {
                            LOG_DEBUG("Sector mismatch (#%d %s != #%d %s.")
                                << cur.after->indexInMap()
                                << cur.vertex->origin().asText()
                                << next.before->indexInMap()
                                << next.vertex->origin().asText();
                        }

                        if(cur.selfRef && !next.selfRef)
                            sector = next.before;
                    }

                    HEdge *right = buildHEdgesBetweenVertexes(*cur.vertex, *next.vertex, sector, sector,
                                                              0 /*no line*/, hplane.lineSegment().lineSide);

                    // Add the new half-edges to the appropriate lists.
                    linkHEdgeInSuperBlockmap(rightSet, *right);
                    linkHEdgeInSuperBlockmap(leftSet,  right->twin());

                    /*
                    HEdge *left = right->twinPtr();
                    LOG_DEBUG("Capped partition gap:"
                              "\n %p RIGHT sector #%d %s to %s"
                              "\n %p LEFT  sector #%d %s to %s")
                        << de::dintptr(right)
                        << (lineSegment(*right).sector? lineSegment(*right).sector->indexInMap() : -1)
                        << right->fromOrigin().asText()
                        << right->toOrigin().asText()
                        << de::dintptr(left)
                        << (lineSegment(*left).sector? lineSegment(*left).sector->indexInMap() : -1)
                        << left->fromOrigin().asText()
                        << left->toOrigin().asText()
                    */
                }
            }
        }
    }

    /**
     * Splits the given half-edge at the point (x,y). The new half-edge is
     * returned. The old half-edge is shortened (the original start vertex is
     * unchanged), the new half-edge becomes the cut-off tail (keeping the
     * original end vertex).
     *
     * @note If the half-edge has a twin it is also split.
     */
    HEdge &splitHEdge(HEdge &oldHEdge, Vector2d const &point)
    {
        //LOG_DEBUG("Splitting hedge %p at %s.")
        //    << de::dintptr(&oldHEdge) << point.asText();

        Vertex *newVert = newVertex(point);
        {
            LineSegment &oldInfo = lineSegment(oldHEdge);
            addHEdgeTip(*newVert, M_InverseAngle(oldInfo.pAngle), oldHEdge.twinPtr(), &oldHEdge);
            addHEdgeTip(*newVert, oldInfo.pAngle, &oldHEdge, oldHEdge.twinPtr());
        }

        HEdge &newHEdge = cloneHEdge(oldHEdge);
        LineSegment &oldInfo = lineSegment(oldHEdge);
        LineSegment &newInfo = lineSegment(newHEdge);

        newInfo.prevOnSide = &oldHEdge;
        oldInfo.nextOnSide = &newHEdge;

        oldHEdge._to = newVert;
        oldInfo.initFromHEdge(oldHEdge);

        newHEdge._from = newVert;
        newInfo.initFromHEdge(newHEdge);

        // Handle the twin.
        if(oldHEdge.hasTwin())
        {
            //LOG_DEBUG("Splitting hedge twin %p.")
            //    << de::dintptr(oldHEdge.twinPtr());

            // Copy the old hedge info.
            newHEdge._twin = &cloneHEdge(oldHEdge.twin());
            newHEdge.twin()._twin = &newHEdge;

            lineSegment(newHEdge.twin()).nextOnSide = oldHEdge.twinPtr();
            lineSegment(oldHEdge.twin()).prevOnSide = newHEdge.twinPtr();

            oldHEdge.twin()._from = newVert;
            lineSegment(oldHEdge.twin()).initFromHEdge(oldHEdge.twin());

            newHEdge.twin()._to = newVert;
            lineSegment(newHEdge.twin()).initFromHEdge(newHEdge.twin());

            // Has this already been added to a leaf?
            if(oldHEdge.twin().hasBspLeaf())
            {
                // Update the in-leaf references.
                oldHEdge.twin()._next = newHEdge.twinPtr();

                // There is now one more half-edge in this leaf.
                oldHEdge.twin().bspLeaf()._hedgeCount += 1;
            }
        }

        return newHEdge;
    }

    /**
     * @param fromSeg  Line segment to determine the distance from.
     * @param toSeg    Line segment to determine the distance to.
     *
     * Return values:
     * @param fromDist  Perpendicular distance from the "from" vertex of @a fromSeg.
     * @param toDist    Perpendicular distance from the "to" vertex of @a toSeg.
     */
    inline void lineSegmentDistance(LineSegment const &fromSeg,
        LineSegment const &toSeg, coord_t &fromDist, coord_t &toDist)
    {
        /// @attention Ensure line segments produced from the partition's source
        /// line are always treated as collinear. This special case is only
        /// necessary due to precision inaccuracies when a line is split into
        /// multiple segments.
        if(&fromSeg.sourceLineSide->line() == &toSeg.sourceLineSide->line())
        {
            fromDist = toDist = 0;
        }
        else
        {
            fromDist = V2d_PointLinePerpDistance(fromSeg.start, toSeg.direction, toSeg.pPerp, toSeg.pLength);
            toDist   = V2d_PointLinePerpDistance(fromSeg.end,   toSeg.direction, toSeg.pPerp, toSeg.pLength);
        }
    }

    /**
     * Determine the relationship between the two line segments @a fromSeg and
     * @a toSeg.
     *
     * @return LineRelationship between the given line segments.
     */
    inline LineRelationship lineSegmentRelationship(LineSegment const &fromSeg,
        LineSegment const &toSeg, coord_t &fromDist, coord_t &toDist)
    {
        lineSegmentDistance(fromSeg, toSeg, fromDist, toDist);
        return lineRelationship(fromDist, toDist);
    }

    inline void interceptPartition(HEdge const &hedge, int edge)
    {
        Vertex const &vertex = hedge.vertex(edge);
        hplane.interceptLineSegment(lineSegment(hedge), vertex,
                                    openSectorAtAngle(vertex, M_InverseAngle(hplane.lineSegment().pAngle)),
                                    openSectorAtAngle(vertex, hplane.lineSegment().pAngle));
    }

    /**
     * Take the given half-edge @a hedge, compare it with the partition plane
     * and determine which of the two sets it should be added to. If the
     * half-edge is found to intersect the partition, the intercept point is
     * calculated and the half-edge split at this point before then adding
     * each to the relevant set (any existing twin is handled uniformly, also).
     *
     * If the half-edge lies on, or crosses the partition then a new intercept
     * is added to the partition plane.
     *
     * @param hedge   Half-edge to be "divided".
     * @param rights  Set of half-edges on the right side of the partition.
     * @param lefts   Set of half-edges on the left side of the partition.
     */
    void divideHEdge(HEdge &hedge, SuperBlock &rights, SuperBlock &lefts)
    {
        LineSegment const &lineSeg = lineSegment(hedge);

        coord_t fromDist, toDist;
        LineRelationship rel = lineSegmentRelationship(lineSeg, hplane.lineSegment(),
                                                       fromDist, toDist);
        switch(rel)
        {
        case Collinear: {
            interceptPartition(hedge, HEdge::From);
            interceptPartition(hedge, HEdge::To);

            // Direction (vs that of the partition plane) determines in which
            // subset this half-edge belongs.
            if(lineSeg.direction[VX] * hplane.lineSegment().direction[VX] +
               lineSeg.direction[VY] * hplane.lineSegment().direction[VY] < 0)
            {
                linkHEdgeInSuperBlockmap(lefts, hedge);
            }
            else
            {
                linkHEdgeInSuperBlockmap(rights, hedge);
            }
            break; }

        case Right:
        case RightIntercept:
            if(rel == RightIntercept)
            {
                // Direction determines which edge of the half-edge interfaces
                // with the new half-plane intercept.
                interceptPartition(hedge, (fromDist < DIST_EPSILON? HEdge::From : HEdge::To));
            }
            linkHEdgeInSuperBlockmap(rights, hedge);
            break;

        case Left:
        case LeftIntercept:
            if(rel == LeftIntercept)
            {
                interceptPartition(hedge, (fromDist > -DIST_EPSILON? HEdge::From : HEdge::To));
            }
            linkHEdgeInSuperBlockmap(lefts, hedge);
            break;

        case Intersects: {
            // Calculate the intercept point and split this half-edge.
            Vector2d point = interceptHEdgePartition(hedge, fromDist, toDist);
            HEdge &newHEdge = splitHEdge(hedge, point);

            // Ensure the new twin half-edge is inserted into the same block
            // as the old twin.
            /// @todo This logic can now be moved into splitHEdge().
            if(hedge.hasTwin() && !hedge.twin().hasBspLeaf())
            {
                SuperBlock *bmapBlock = lineSegment(hedge.twin()).bmapBlock;
                DENG2_ASSERT(bmapBlock != 0);
                linkHEdgeInSuperBlockmap(*bmapBlock, newHEdge.twin());
            }

            interceptPartition(hedge, HEdge::To);

            // Direction determines which subset the hedges are added to.
            if(fromDist < 0)
            {
                linkHEdgeInSuperBlockmap(rights, newHEdge);
                linkHEdgeInSuperBlockmap(lefts,  hedge);
            }
            else
            {
                linkHEdgeInSuperBlockmap(rights, hedge);
                linkHEdgeInSuperBlockmap(lefts,  newHEdge);
            }
            break; }
        }

#undef LEFT
#undef RIGHT
    }

    /**
     * Remove all the half-edges from the list, partitioning them into the
     * left or right lists based on the given partition line. Adds any
     * intersections onto the intersection list as it goes.
     */
    void partitionHEdges(SuperBlock &hedgeList, SuperBlock &rights,
                         SuperBlock &lefts)
    {
        // Iterative pre-order traversal of SuperBlock.
        SuperBlock *cur = &hedgeList;
        SuperBlock *prev = 0;
        while(cur)
        {
            while(cur)
            {
                HEdge *hedge;
                while((hedge = cur->pop()))
                {
                    // Disassociate the half-edge from the blockmap.
                    lineSegment(*hedge).bmapBlock = 0;

                    divideHEdge(*hedge, rights, lefts);
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

        // Sanity checks...
        if(!rights.totalHEdgeCount())
            throw Error("Partitioner::partitionhedges", "Right half-edge set is empty");

        if(!lefts.totalHEdgeCount())
            throw Error("Partitioner::partitionhedges", "Left half-edge set is empty");
    }

    /**
     * "Near miss" predicate.
     */
    static bool nearMiss(LineRelationship rel, coord_t fromDist, coord_t toDist,
                         coord_t *distance)
    {
        if(rel == Right &&
           !((fromDist >= SHORT_HEDGE_EPSILON && toDist >= SHORT_HEDGE_EPSILON) ||
             (fromDist <= DIST_EPSILON        && toDist >= SHORT_HEDGE_EPSILON) ||
             (toDist <= DIST_EPSILON && fromDist >= SHORT_HEDGE_EPSILON)))
        {
            // Need to know how close?
            if(distance)
            {
                if(fromDist <= DIST_EPSILON || toDist <= DIST_EPSILON)
                    *distance = SHORT_HEDGE_EPSILON / de::max(fromDist, toDist);
                else
                    *distance = SHORT_HEDGE_EPSILON / de::min(fromDist, toDist);
            }
            return true;
        }

        if(rel == Left &&
           !((fromDist <= -SHORT_HEDGE_EPSILON && toDist <= -SHORT_HEDGE_EPSILON) ||
             (fromDist >= -DIST_EPSILON        && toDist <= -SHORT_HEDGE_EPSILON) ||
             (toDist >= -DIST_EPSILON && fromDist <= -SHORT_HEDGE_EPSILON)))
        {
            // Need to know how close?
            if(distance)
            {
                if(fromDist >= -DIST_EPSILON || toDist >= -DIST_EPSILON)
                    *distance = SHORT_HEDGE_EPSILON / -de::min(fromDist, toDist);
                else
                    *distance = SHORT_HEDGE_EPSILON / -de::max(fromDist, toDist);
            }
            return true;
        }

        return false;
    }

    /**
     * "Near edge" predicate. Assumes intersecting line segment relationship.
     */
    static bool nearEdge(coord_t fromDist, coord_t toDist, coord_t *distance)
    {
        if(de::abs(fromDist) < SHORT_HEDGE_EPSILON || de::abs(toDist) < SHORT_HEDGE_EPSILON)
        {
            // Need to know how close?
            if(distance)
            {
                *distance = SHORT_HEDGE_EPSILON / de::min(de::abs(fromDist), de::abs(toDist));
            }
            return true;
        }
        return false;
    }

    void evalPartitionCostForHEdge(LineSegment const &plSeg, HEdge const &hedge,
                                   PartitionCost &cost)
    {
        int const costFactorMultiplier = splitCostFactor;

        LineSegment const &lineSeg = lineSegment(hedge);

        // Determine the relationship between this half-edge and the partition plane.
        coord_t fromDist, toDist;
        LineRelationship rel = lineSegmentRelationship(lineSeg, plSeg, fromDist, toDist);
        switch(rel)
        {
        case Collinear: {
            // This half-edge runs along the same line as the partition.
            // Check whether it goes in the same direction or the opposite.
            if(lineSeg.direction[VX] * plSeg.direction[VX] +
               lineSeg.direction[VY] * plSeg.direction[VY] < 0)
            {
                cost.addHEdgeLeft(hedge);
            }
            else
            {
                cost.addHEdgeRight(hedge);
            }
            break; }

        case Right:
        case RightIntercept: {
            cost.addHEdgeRight(hedge);

            /*
             * Near misses are bad, as they have the potential to result in
             * really short half-hedges being produced later on.
             *
             * The closer the near miss, the higher the cost.
             */
            coord_t nearDist;
            if(nearMiss(rel, fromDist, toDist, &nearDist))
            {
                cost.nearMiss += 1;
                cost.total += int( 100 * costFactorMultiplier * (nearDist * nearDist - 1.0) );
            }
            break; }

        case Left:
        case LeftIntercept: {
            cost.addHEdgeLeft(hedge);

            // Near miss?
            coord_t nearDist;
            if(nearMiss(rel, fromDist, toDist, &nearDist))
            {
                /// @todo Why the cost multiplier imbalance between the left
                /// and right edge near misses?
                cost.nearMiss += 1;
                cost.total += int( 70 * costFactorMultiplier * (nearDist * nearDist - 1.0) );
            }
            break; }

        case Intersects: {
            cost.splits += 1;
            cost.total  += 100 * costFactorMultiplier;

            /*
             * If the split point is very close to one end, which is quite an
             * undesirable situation (producing really short edges), thus a
             * rather hefty surcharge.
             *
             * The closer to the edge, the higher the cost.
             */
            coord_t nearDist;
            if(nearEdge(fromDist, toDist, &nearDist))
            {
                cost.iffy += 1;
                cost.total += int( 140 * costFactorMultiplier * (nearDist * nearDist - 1.0) );
            }
            break; }
        }
    }

    /**
     * @param block
     * @param best      Best half-edge found thus far.
     * @param bestCost  Running cost total result for the best half-edge yet.
     * @param hedge     The candidate half-edge to be evaluated.
     * @param cost      PartitionCost analysis to be completed for the candidate.
     *                  Must have been initialized prior to calling.
     *
     * @return  @c true iff @a hedge is suitable for use as a partition.
     */
    bool evalPartitionCostForSuperBlock(SuperBlock const &block,
        HEdge *best, PartitionCost const &bestCost,
        HEdge const &hedge, PartitionCost &cost)
    {
        /*
         * Test the whole block against the partition line to quickly handle
         * all the half-edges within it at once. Only when the partition line
         * intercepts the box do we need to go deeper into it.
         */
        LineSegment const &lineSeg = lineSegment(hedge);

        /// @todo Why are we extending the bounding box for this test? Also,
        /// there is no need to convert from integer to floating-point each
        /// time this is tested. (If we intend to do this with floating-point
        /// then we should return that representation in SuperBlock::bounds() ).
        AABox const &blockBounds = block.bounds();
        AABoxd bounds(coord_t( blockBounds.minX ) - SHORT_HEDGE_EPSILON * 1.5,
                      coord_t( blockBounds.minY ) - SHORT_HEDGE_EPSILON * 1.5,
                      coord_t( blockBounds.maxX ) + SHORT_HEDGE_EPSILON * 1.5,
                      coord_t( blockBounds.maxY ) + SHORT_HEDGE_EPSILON * 1.5);

        int side = M_BoxOnLineSide2(&bounds, lineSeg.start, lineSeg.direction,
                                    lineSeg.pPerp, lineSeg.pLength, DIST_EPSILON);
        if(side > 0)
        {
            // Right.
            cost.realRight += block.realHEdgeCount();
            cost.miniRight += block.miniHEdgeCount();
            return true;
        }
        if(side < 0)
        {
            // Left.
            cost.realLeft += block.realHEdgeCount();
            cost.miniLeft += block.miniHEdgeCount();
            return true;
        }

        // Check partition against all half-edges.
        foreach(HEdge *otherHedge, block.hedges())
        {
            // Do we already have a better choice?
            if(best && !(cost < bestCost)) return false;

            // Evaluate the cost delta for this hedge.
            PartitionCost costDelta;
            evalPartitionCostForHEdge(lineSeg, *otherHedge, costDelta);

            // Merge cost result into the cummulative total.
            cost += costDelta;
        }

        // Handle sub-blocks recursively.
        if(block.hasRight())
        {
            bool unsuitable = !evalPartitionCostForSuperBlock(*block.rightPtr(), best, bestCost, hedge, cost);
            if(unsuitable) return false;
        }

        if(block.hasLeft())
        {
            bool unsuitable = !evalPartitionCostForSuperBlock(*block.leftPtr(), best, bestCost, hedge, cost);
            if(unsuitable) return false;
        }

        // This is a suitable candidate.
        return true;
    }

    /**
     * Evaluate a partition and determine the cost, taking into account the
     * number of splits and the difference between left and right.
     *
     * To be able to divide the nodes down, evalPartition must decide which
     * is the best half-edge to use as a nodeline. It does this by selecting
     * the line with least splits and has least difference of half-edges on
     * either side of it.
     *
     * @return  @c true iff this half-edge is suitable for use as a partition.
     */
    bool evalPartition(SuperBlock const &block,
                       HEdge *best, PartitionCost const &bestCost,
                       HEdge const &hedge, PartitionCost &cost)
    {
        // "Mini-hedges" are never potential candidates.
        if(!hedge.hasLineSide()) return false;

        if(!evalPartitionCostForSuperBlock(block, best, bestCost, hedge, cost))
        {
            // Unsuitable or we already have a better choice.
            return false;
        }

        // Make sure there is at least one real half-edge on each side.
        if(!cost.realLeft || !cost.realRight)
        {
            //LOG_DEBUG("evalPartition: No real half-edges on %s%sside")
            //    << (cost.realLeft? "" : "left ")
            //    << (cost.realRight? "" : "right ");
            return false;
        }

        // Increase cost by the difference between left and right.
        cost.total += 100 * abs(cost.realLeft - cost.realRight);

        // Allow minihedge counts to affect the outcome.
        cost.total += 50 * abs(cost.miniLeft - cost.miniRight);

        // Another little twist, here we show a slight preference for partition
        // lines that lie either purely horizontally or purely vertically.
        LineSegment const &lineSeg = lineSegment(hedge);
        if(lineSeg.pSlopeType != ST_HORIZONTAL && lineSeg.pSlopeType != ST_VERTICAL)
            cost.total += 25;

        //LOG_DEBUG("evalPartition: %p: splits=%d iffy=%d near=%d"
        //          " left=%d+%d right=%d+%d cost=%d.%02d")
        //    << de::dintptr(&lineSeg) << cost.splits << cost.iffy << cost.nearMiss
        //    << cost.realLeft << cost.miniLeft << cost.realRight << cost.miniRight
        //    << cost.total / 100 << cost.total % 100;

        return true;
    }

    void chooseNextPartitionFromSuperBlock(SuperBlock const &partList,
        SuperBlock const &hedgeList, HEdge **best, PartitionCost &bestCost)
    {
        DENG2_ASSERT(best != 0);

        //LOG_AS("chooseNextPartitionFromSuperBlock");

        // Test each half-edge as a potential partition.
        foreach(HEdge *hedge, partList.hedges())
        {
            LineSegment &lineSeg = lineSegment(*hedge);

            //LOG_DEBUG("%shedge %p sector:%d %s -> %s")
            //    << (hedge->hasLine()? "" : "mini-") << de::dintptr(hedge)
            //    << (lineSeg.sector? lineSeg.sector->indexInMap() : -1)
            //    << hedge->fromOrigin().asText()
            //    << hedge->toOrigin().asText();

            // Optimization: Only the first half-edge produced from a given
            // line is tested per round of partition costing (they are all
            // collinear).
            if(lineSeg.lineSide)
            {
                // Can we skip this half-edge?
                LineInfo &lInfo = lineInfos[lineSeg.lineSide->line().indexInMap()];
                if(lInfo.validCount == validCount) continue; // Yes.

                lInfo.validCount = validCount;
            }

            // Calculate the cost metrics for this half-edge.
            PartitionCost cost;
            if(evalPartition(hedgeList, *best, bestCost, *hedge, cost))
            {
                // Suitable for use as a partition.
                if(!*best || cost < bestCost)
                {
                    // We have a new better choice.
                    bestCost = cost;

                    // Remember which half-edge.
                    *best = hedge;
                }
            }
        }
    }

    /**
     * Find the best half-edge in the list to use as the next partition.
     *
     * @param hedgeList     List of half-edges to choose from.
     * @return  The chosen half-edge.
     */
    HEdge *chooseNextPartition(SuperBlock const &hedgeList)
    {
        LOG_AS("Partitioner::choosePartition");

        PartitionCost bestCost;
        HEdge *best = 0;

        // Increment valid count so we can avoid testing the half edges
        // produced from a single line more than once per round of partition
        // selection.
        validCount++;

        // Iterative pre-order traversal of SuperBlock.
        SuperBlock const *cur = &hedgeList;
        SuperBlock const *prev = 0;
        while(cur)
        {
            while(cur)
            {
                chooseNextPartitionFromSuperBlock(*cur, hedgeList, &best, bestCost);

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

        /*if(best)
        {
            LOG_DEBUG("best %p score: %d.%02d.")
                << de::dintptr(best) << bestCost.total / 100 << bestCost.total % 100;
        }*/

        return best;
    }

    /**
     * Attempt to construct a new BspLeaf from the list of half-edges.
     *
     * @param hedges  List of half-edges from which to build the leaf.
     *                Ownership of the list and it's contents is given to this
     *                function. Once emptied, ownership of the list is then
     *                returned to the caller.
     *
     * @return  Newly created BspLeaf; otherwise @c 0 if degenerate.
     */
    BspLeaf *buildLeaf(HEdgeList &hedges)
    {
        if(!hedges.count()) return 0;

        // Collapse all degenerate and orphaned leafs.
#ifdef DENG_BSP_COLLAPSE_ORPHANED_LEAFS
        bool const isDegenerate = hedges.count() < 3;
        bool isOrphan = true;
        foreach(HEdge *hedge, hedges)
        {
            if(hedge->hasLineSide() && hedge->lineSide().hasSector())
            {
                isOrphan = false;
                break;
            }
        }
#endif

        BspLeaf *leaf = 0;
        while(!hedges.isEmpty())
        {
            HEdge *hedge = hedges.takeFirst();
#ifdef DENG_BSP_COLLAPSE_ORPHANED_LEAFS
            LineSegments::iterator lineSegIt = lineSegments.find(hedge);
            LineSegment &lineSeg = lineSegIt.value();

            if(isDegenerate || isOrphan)
            {
                if(lineSeg.prevOnSide)
                {
                    lineSegment(*lineSeg.prevOnSide).nextOnSide = lineSeg.nextOnSide;
                }
                if(lineSeg.nextOnSide)
                {
                    lineSegment(*lineSeg.nextOnSide).prevOnSide = lineSeg.prevOnSide;
                }

                if(hedge->hasTwin())
                {
                    hedge->twin()._twin = 0;
                }

                /**
                 * @todo This is incorrect from a mod compatibility point of
                 * view. We should never clear the line > sector references as
                 * these are used by the game(s) playsim in various ways (for
                 * example, stair building). We should instead flag the line
                 * accordingly. -ds
                 */
                if(hedge->hasLineSide())
                {
                    Line::Side &side = hedge->lineSide();

                    side._sector  = 0;
                    delete side._sections;
                    side._sections = 0;

                    lineInfos[hedge->line().indexInMap()].flags &=
                        ~(LineInfo::SelfRef | LineInfo::Twosided);
                }

                lineSegments.erase(lineSegIt);
                delete hedge;
                numHEdges -= 1;
                continue;
            }
#endif

            if(!leaf)
            {
                leaf = new BspLeaf;
            }

            // Link it into head of the leaf's list.
            hedge->_next = leaf->_hedge;
            leaf->_hedge = hedge;

            // Link hedge to this leaf.
            hedge->_bspLeaf = leaf;

            // There is now one more half-edge in this leaf.
            leaf->_hedgeCount += 1;
        }

        if(leaf)
        {
            // There is now one more BspLeaf;
            numLeafs += 1;
        }
        return leaf;
    }

    /**
     * Construct a new BspNode.
     *
     * @param partition    Partition which describes the half-plane.
     * @param rightBounds  Boundary of the right child map coordinate subspace. Can be @c NULL.
     * @param leftBounds   Boundary of the left child map coordinate subspace. Can be @a NULL.
     * @param rightChild   ?
     * @param leftChild    ?
     *
     * @return  Newly created BspNode.
     */
    BspNode *newBspNode(Partition const &partition,
        AABoxd &rightBounds, AABoxd &leftBounds,
        MapElement *rightChild = 0, MapElement *leftChild = 0)
    {
        BspNode *node = new BspNode(partition);
        if(rightChild)
        {
            node->setRight(rightChild);
        }
        if(leftChild)
        {
            node->setLeft(leftChild);
        }

        node->setRightAABox(&rightBounds);
        node->setLeftAABox(&leftBounds);

        // There is now one more BspNode.
        numNodes += 1;
        return node;
    }

    BspTreeNode *newTreeNode(MapElement *mapBspElement,
        BspTreeNode *rightChild = 0, BspTreeNode *leftChild = 0)
    {
        BspTreeNode *subtree = new BspTreeNode(mapBspElement);
        if(rightChild)
        {
            subtree->setRight(rightChild);
            rightChild->setParent(subtree);
        }
        if(leftChild)
        {
            subtree->setLeft(leftChild);
            leftChild->setParent(subtree);
        }

        treeNodeMap.insert(mapBspElement, subtree);
        return subtree;
    }

    /**
     * Takes the half-edge list and determines if it is convex, possibly
     * converting it into a BSP leaf. Otherwise, the list is divided into two
     * halves and recursion will continue on the new sub list.
     *
     * This is done by scanning all of the half-edges and finding the one that
     * does the least splitting and has the least difference in numbers of
     * half-edges on either side.
     *
     * If the half-edges on the left side are convex create another leaf
     * else put the half-edges into the left list.
     *
     * If the half-edges on the right side are convex create another leaf
     * else put the half-edges into the right list.
     *
     * @param hedgeBlockmap  Blockmap containing the list of half-edges to be
     *                       carved into convex subspaces.
     *
     * @return  Newly created subtree else @c NULL if degenerate.
     */
    BspTreeNode *divideSubspace(SuperBlock &hedgeBlockmap)
    {
        LOG_AS("Partitioner::divideSubspace");

        MapElement *bspElement = 0; ///< Built BSP map element at this node.
        BspTreeNode *rightTree = 0, *leftTree = 0;

        // Pick a half-edge to use as the next partition plane.
        HEdge *partHEdge = chooseNextPartition(hedgeBlockmap);
        if(partHEdge)
        {
            //LOG_TRACE("Partition %p %s -> %s.") << de::dintptr(hedge)
            //    << hedge->fromOrigin().asText() << hedge->toOrigin().asText();

            // Reconfigure the half-plane for the next round of partitioning.
            hplane.configure(lineSegment(*partHEdge), *partHEdge);

            // Take a copy of the current partition - we'll need this later.
            Partition partition(hplane.partition());

            // Create left and right super blockmaps.
            /// @todo There should be no need to use additional independent
            ///       structures to contain these subsets.
            // Copy the bounding box of the edge list to the superblocks.
            SuperBlockmap rightHEdges = SuperBlockmap(hedgeBlockmap.bounds());
            SuperBlockmap leftHEdges  = SuperBlockmap(hedgeBlockmap.bounds());

            // Divide the half-edges into two subsets according to their
            // spacial relationship with the half-plane (splitting any which
            // intersect).
            partitionHEdges(hedgeBlockmap, rightHEdges, leftHEdges);
            hedgeBlockmap.clear();
            addMiniHEdges(rightHEdges, leftHEdges);
            hplane.clearIntercepts();

            // Make a copy of the child bounds as we'll need this info to
            // populate a BspNode should we produce one (after the subtrees
            // are processed).
            AABoxd rightBounds = rightHEdges.findHEdgeBounds();
            AABoxd leftBounds  = leftHEdges.findHEdgeBounds();

            // Recurse on each suspace, first the right space then left.
            rightTree = divideSubspace(rightHEdges);
            leftTree  = divideSubspace(leftHEdges);

            // Collapse degenerates upward.
            if(!rightTree || !leftTree)
                return rightTree? rightTree : leftTree;

            // Construct the new node and link up the child elements.
            MapElement *rightChildBspElement = reinterpret_cast<MapElement *>(rightTree->userData());
            MapElement *leftChildBspElement  = reinterpret_cast<MapElement *>(leftTree->userData());

            bspElement = newBspNode(partition, rightBounds, leftBounds,
                                    rightChildBspElement, leftChildBspElement);
        }
        else
        {
            // No partition required/possible - already convex (or degenerate).
            HEdgeList collected = collectHEdges(hedgeBlockmap);
            BspLeaf *leaf = buildLeaf(collected);

            hedgeBlockmap.clear();

            // Not a leaf? (collapse upward).
            if(!leaf) return 0;

            bspElement = leaf;
        }

        return newTreeNode(bspElement, rightTree, leftTree);
    }

    /**
     * Find the intersection point between a half-edge and the current
     * partition plane. Takes advantage of some common situations like
     * horizontal and vertical lines to choose a 'nicer' intersection
     * point.
     */
    Vector2d interceptHEdgePartition(HEdge const &hedge, coord_t perpC,
                                     coord_t perpD) const
    {
        LineSegment const &lineSeg = lineSegment(hedge);

        // Horizontal partition against vertical half-edge.
        if(hplane.lineSegment().pSlopeType == ST_HORIZONTAL && lineSeg.pSlopeType == ST_VERTICAL)
        {
            return Vector2d(lineSeg.start[VX], hplane.lineSegment().start[VY]);
        }

        // Vertical partition against horizontal half-edge.
        if(hplane.lineSegment().pSlopeType == ST_VERTICAL && lineSeg.pSlopeType == ST_HORIZONTAL)
        {
            return Vector2d(hplane.lineSegment().start[VX], lineSeg.start[VY]);
        }

        // 0 = start, 1 = end.
        coord_t ds = perpC / (perpC - perpD);

        Vector2d point;
        if(lineSeg.pSlopeType == ST_VERTICAL)
            point.x = lineSeg.start[VX];
        else
            point.x = lineSeg.start[VX] + (lineSeg.direction[VX] * ds);

        if(lineSeg.pSlopeType == ST_HORIZONTAL)
            point.y = lineSeg.start[VY];
        else
            point.y = lineSeg.start[VY] + (lineSeg.direction[VY] * ds);

        return point;
    }

    /**
     * Sort half-edges by angle (from the middle point to the start vertex).
     * The desired order (clockwise) means descending angles.
     */
    static void sortHEdgesByAngleAroundPoint(HEdgeSortBuffer &hedges,
        int lastHEdgeIndex, Vector2d const &point)
    {
        /// @par Algorithm
        /// "double bubble"
        for(int pass = 0; pass < lastHEdgeIndex; ++pass)
        {
            bool swappedAny = false;
            for(int i = 0; i < lastHEdgeIndex; ++i)
            {
                HEdge const *hedge1 = hedges.at(i);
                HEdge const *hedge2 = hedges.at(i+1);

                Vector2d v1Dist = hedge1->fromOrigin() - point;
                Vector2d v2Dist = hedge2->fromOrigin() - point;

                coord_t v1Angle = M_DirectionToAngleXY(v1Dist.x, v1Dist.y);
                coord_t v2Angle = M_DirectionToAngleXY(v2Dist.x, v2Dist.y);

                if(v1Angle + ANG_EPSILON < v2Angle)
                {
                    hedges.swap(i, i + 1);
                    swappedAny = true;
                }
            }
            if(!swappedAny) break;
        }
    }

    /**
     * Sort the half-edges linked within the given BSP leaf into a clockwise
     * order according to their position/orientation relative to the specified
     * point.
     *
     * @param leaf        BSP leaf containing the list of hedges to be sorted.
     * @param point       Map space point around which to order.
     * @param sortBuffer  Buffer to use for sorting of the hedges.
     *
     * @attention Do NOT move this into BspLeaf. Although this clearly envies
     * the access rights of the BspLeaf class this algorithm belongs here in
     * the BSP partitioner. -ds
     */
    static void clockwiseOrder(BspLeaf &leaf, Vector2d const &point,
                               HEdgeSortBuffer &sortBuffer)
    {
        if(!leaf._hedge) return;

        // Insert the hedges into the sort buffer.
#ifdef DENG2_QT_4_7_OR_NEWER
        sortBuffer.reserve(leaf._hedgeCount);
#endif
        int i = 0;
        for(HEdge *hedge = leaf._hedge; hedge; hedge = hedge->_next, ++i)
        {
            sortBuffer.insert(i, hedge);
        }

        sortHEdgesByAngleAroundPoint(sortBuffer, leaf._hedgeCount -1, point);

        // Re-link the half-edge list in the order of the sort buffer.
        leaf._hedge = 0;
        for(uint i = 0; i < leaf._hedgeCount; ++i)
        {
            uint idx = (leaf._hedgeCount - 1) - i;
            uint j = idx % leaf._hedgeCount;

            sortBuffer[j]->_next = leaf._hedge;
            leaf._hedge = sortBuffer[j];
        }

        // LOG_DEBUG("Sorted half-edges around %s" << point.asText();
        // leaf.printHEdges();
    }

    /**
     * Determine which sector this BSP leaf belongs to.
     */
    Sector *chooseSectorForBspLeaf(BspLeaf const *leaf)
    {
        if(!leaf || !leaf->firstHEdge()) return 0;

        Sector *selfRefChoice = 0;
        HEdge *base = leaf->firstHEdge();
        HEdge *hedge = base;
        do
        {
            if(hedge->hasLineSide() && hedge->lineSide().hasSector())
            {
                Line &line = hedge->lineSide().line();
                Sector &sector = hedge->lineSide().sector();

                // The first sector from a non self-referencing line is our best choice.
                /// @todo Should the presence of sections really affect this? -ds
                if(!(line.isSelfReferencing() && line.hasFrontSections() && line.hasBackSections()))
                    return &sector;

                // Remember the self-referencing choice in case we've no better option.
                if(!selfRefChoice)
                    selfRefChoice = &sector;
            }
        } while((hedge = &hedge->next()) != base);

        if(selfRefChoice) return selfRefChoice;

        /*
         * Last resort:
         * @todo This is only necessary because of other failure cases in the
         * partitioning algorithm and to avoid producing a potentially
         * dangerous BSP - not assigning a sector to each leaf may result in
         * obscure fatal errors when in-game.
         */
        hedge = base;
        do
        {
            if(Sector *hedgeSector = lineSegment(*hedge).sector)
            {
                return hedgeSector;
            }
        } while((hedge = &hedge->next()) != base);

        return 0; // Not reachable.
    }

    void clockwiseLeaf(BspLeaf &leaf, HEdgeSortBuffer &sortBuffer)
    {
        clockwiseOrder(leaf, findBspLeafCenter(leaf), sortBuffer);

        // Construct the leaf's hedge ring.
        if(leaf._hedge)
        {
            HEdge *hedge = leaf._hedge;
            forever
            {
                /// @todo kludge: This should not be done here!
                if(hedge->hasLineSide())
                {
                    Line::Side &side = hedge->lineSide();

                    // Already processed?
                    if(!side.leftHEdge())
                    {
                        HEdge *leftHEdge = hedge;
                        // Find the left-most hedge.
                        while(lineSegment(*leftHEdge).prevOnSide)
                            leftHEdge = lineSegment(*leftHEdge).prevOnSide;
                        side.setLeftHEdge(leftHEdge);

                        // Find the right-most hedge.
                        HEdge *rightHEdge = hedge;
                        while(lineSegment(*rightHEdge).nextOnSide)
                            rightHEdge = lineSegment(*rightHEdge).nextOnSide;
                        side.setRightHEdge(rightHEdge);
                    }
                }
                /// kludge end

                if(hedge->_next)
                {
                    // Reverse link.
                    hedge->_next->_prev = hedge;
                    hedge = hedge->_next;
                }
                else
                {
                    // Circular link.
                    hedge->_next = leaf._hedge;
                    hedge->_next->_prev = hedge;
                    break;
                }
            }
        }
    }

    Sector *findFirstSectorInHEdgeList(BspLeaf const &leaf)
    {
        HEdge const *base = leaf.firstHEdge();
        HEdge const *hedge = base;
        do
        {
            if(Sector *hedgeSector = lineSegment(*hedge).sector)
            {
                return hedgeSector;
            }
        } while((hedge = &hedge->next()) != base);
        return 0; // Nothing??
    }

    /**
     * Sort all half-edges in each BSP leaf into a clockwise order.
     *
     * @note This cannot be done during Partitioner::divideSubspace() as
     * splitting a half-edge with a twin will result in another half-edge
     * being inserted into that twin's leaf (usually in the wrong place
     * order-wise).
     */
    void windLeafs()
    {
        HEdgeSortBuffer sortBuffer;
        foreach(BspTreeNode *node, treeNodeMap)
        {
            if(!node->isLeaf()) continue;
            BspLeaf *leaf = node->userData()->castTo<BspLeaf>();

            // Sort the leaf's half-edges.
            clockwiseLeaf(*leaf, sortBuffer);

            /*
             * Perform some post analysis on the built leaf.
             */
            if(!sanityCheckHasRealHEdge(*leaf))
                throw Error("Partitioner::clockwiseLeaf",
                            QString("BSP Leaf 0x%1 has no line-linked half-edge")
                                .arg(dintptr(leaf), 0, 16));

            // Look for migrant half-edges in the leaf.
            if(Sector *sector = findFirstSectorInHEdgeList(*leaf))
            {
                HEdge *base = leaf->firstHEdge();
                HEdge *hedge = base;
                do
                {
                    LineSegment const &info = lineSegment(*hedge);
                    if(info.sector && info.sector != sector)
                    {
                        notifyMigrantHEdgeBuilt(*hedge, *sector);
                    }
                } while((hedge = &hedge->next()) != base);
            }

            leaf->setSector(chooseSectorForBspLeaf(leaf));
            if(!leaf->hasSector())
            {
                LOG_WARNING("BspLeaf %p is degenerate/orphan (%d HEdges).")
                    << de::dintptr(leaf) << leaf->hedgeCount();
            }

            // See if we built a partial leaf...
            uint gaps = 0;
            HEdge const *base = leaf->firstHEdge();
            HEdge const *hedge = base;
            do
            {
                HEdge &next = hedge->next();
                if(hedge->toOrigin() != next.fromOrigin())
                {
                    gaps++;
                }
            } while((hedge = &hedge->next()) != base);

            if(gaps > 0)
            {
                /*
                HEdge const *base = leaf.firstHEdge();
                HEdge const *hedge = base;
                do
                {
                    LOG_DEBUG("  half-edge %p %s -> %s")
                        << de::dintptr(hedge)
                        << hedge->fromOrigin().asText(),
                        << hedge->toOrigin().asText();

                } while((hedge = hedge->next) != base);
                */

                notifyPartialBspLeafBuilt(*leaf, gaps);
            }
        }
    }

    /**
     * Analyze the partition intercepts, building new half-edges to cap any
     * gaps (new hedges are added onto the end of the appropriate list
     * (rights to @a rightList and lefts to @a leftList)).
     *
     * @param rightList  List of hedges on the right of the partition.
     * @param leftList   List of hedges on the left of the partition.
     */
    void addMiniHEdges(SuperBlock &rightList, SuperBlock &leftList)
    {
        LOG_TRACE("Building HEdges along partition %s %s")
            << Vector2d(hplane.lineSegment().start).asText()
            << Vector2d(hplane.lineSegment().direction).asText();

        //hplane.printIntercepts();

        // First, fix any near-distance issues with the intercepts.
        hplane.sortAndMergeIntercepts();
        buildHEdgesAtPartitionGaps(hplane.intercepts(), rightList, leftList);
    }

    void clearBspElement(BspTreeNode &tree)
    {
        LOG_AS("Partitioner::clearBspElement");

        MapElement *elm = tree.userData();
        if(!elm) return;

        if(builtOk)
        {
            LOG_DEBUG("Clearing unclaimed %s %p.")
                << (tree.isLeaf()? "leaf" : "node") << de::dintptr(elm);
        }

        if(tree.isLeaf())
        {
            DENG2_ASSERT(elm->type() == DMU_BSPLEAF);
            // There is now one less BspLeaf.
            numLeafs -= 1;
        }
        else
        {
            DENG2_ASSERT(elm->type() == DMU_BSPNODE);
            // There is now one less BspNode.
            numNodes -= 1;
        }
        delete elm;
        tree.setUserData(0);

        BuiltBspElementMap::iterator found = treeNodeMap.find(elm);
        DENG2_ASSERT(found != treeNodeMap.end());
        treeNodeMap.erase(found);
    }

    void clearAllBspElements()
    {
        DENG2_FOR_EACH(Vertexes, it, vertexes)
        {
            Vertex *vtx = *it;
            // Has ownership of this vertex been claimed?
            if(!vtx) continue;

            clearHEdgeTipsByVertex(*vtx);
            delete vtx;
        }

        foreach(BspTreeNode *node, treeNodeMap)
        {
            clearBspElement(*node);
        }
    }

    BspTreeNode *treeNodeForBspElement(MapElement *ob)
    {
        LOG_AS("Partitioner::treeNodeForBspElement");

        int const elemType = ob->type();
        if(elemType == DMU_BSPLEAF || elemType == DMU_BSPNODE)
        {
            BuiltBspElementMap::const_iterator found = treeNodeMap.find(ob);
            if(found == treeNodeMap.end()) return 0;
            return found.value();
        }
        LOG_DEBUG("Attempted to locate using an unknown element %p (type: %d).")
            << de::dintptr(ob) << elemType;
        return 0;
    }

    /**
     * Allocate another Vertex.
     *
     * @param origin  Origin of the vertex in the map coordinate space.
     *
     * @return  Newly created Vertex.
     */
    Vertex *newVertex(Vector2d const &origin)
    {
        Vertex *vtx = new Vertex(origin);

        /// @todo We do not have authorization to specify this index. -ds
        /// (This job should be done post BSP build.)
        vtx->setIndexInMap(map->vertexCount() + uint(vertexes.size()));
        vertexes.push_back(vtx);

        // There is now one more Vertex.
        numVertexes += 1;
        vertexInfos.push_back(VertexInfo());

        return vtx;
    }

    inline void addHEdgeTip(Vertex &vtx, coord_t angle, HEdge *front, HEdge *back)
    {
        vertexInfo(vtx).addHEdgeTip(angle, front, back);
    }

    inline void clearHEdgeTipsByVertex(Vertex const &vtx)
    {
        vertexInfo(vtx).clearHEdgeTips();
    }

    void clearAllHEdgeTips()
    {
        for(uint i = 0; i < vertexInfos.size(); ++i)
        {
            vertexInfos[i].clearHEdgeTips();
        }
    }

    /**
     * Create a new half-edge.
     */
    HEdge *newHEdge(Vertex &from, Vertex &to, Sector &sec, Line::Side *side = 0,
                    Line::Side *sourceSide = 0)
    {
        HEdge *hedge = new HEdge(from, side);
        hedge->_to = &to;

        // There is now one more HEdge.
        numHEdges += 1;
        lineSegments.insert(hedge, LineSegment());

        LineSegment &info = lineSegment(*hedge);

        info.lineSide       = side;
        info.sourceLineSide = sourceSide;
        info.sector         = &sec;
        info.initFromHEdge(*hedge);

        return hedge;
    }

    /**
     * Create a clone of an existing half-edge.
     */
    HEdge &cloneHEdge(HEdge const &other)
    {
        HEdge *hedge = new HEdge(other);

        // There is now one more HEdge.
        numHEdges += 1;
        lineSegments.insert(hedge, LineSegment(lineSegment(other)));

        return *hedge;
    }

    HEdgeList collectHEdges(SuperBlock &partList)
    {
        HEdgeList hedges;

        // Iterative pre-order traversal of SuperBlock.
        SuperBlock *cur = &partList;
        SuperBlock *prev = 0;
        while(cur)
        {
            while(cur)
            {
                HEdge *hedge;
                while((hedge = cur->pop()))
                {
                    // Disassociate the half-edge from the blockmap.
                    lineSegment(*hedge).bmapBlock = 0;

                    hedges.append(hedge);
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
        return hedges;
    }

    /**
     * Check whether a line with the given delta coordinates and beginning at
     * this vertex, is open.
     *
     * Returns a sector reference if it's open, or @c 0 if closed (void space
     * or directly along a line).
     */
    Sector *openSectorAtAngle(Vertex const &vtx, coord_t angle)
    {
        VertexInfo::HEdgeTips const &hedgeTips = vertexInfo(vtx).hedgeTips();

        if(hedgeTips.empty())
        {
            throw Error("Partitioner::openSectorAtAngle",
                        QString("Vertex #%1 has no hedge tips!")
                            .arg(vtx.indexInMap()));
        }

        // First check whether there's a wall_tip that lies in the exact
        // direction of the given direction (which is relative to the vertex).
        DENG2_FOR_EACH_CONST(VertexInfo::HEdgeTips, it, hedgeTips)
        {
            HEdgeTip const &tip = *it;
            coord_t diff = fabs(tip.angle() - angle);
            if(diff < ANG_EPSILON || diff > (360.0 - ANG_EPSILON))
            {
                return 0; // Yes, found one.
            }
        }

        // OK, now just find the first wall_tip whose angle is greater than
        // the angle we're interested in. Therefore we'll be on the front side
        // of that tip edge.
        DENG2_FOR_EACH_CONST(VertexInfo::HEdgeTips, it, hedgeTips)
        {
            HEdgeTip const &tip = *it;
            if(angle + ANG_EPSILON < tip.angle())
            {
                // Found it.
                return (tip.hasFront()? lineSegment(tip.front()).sector : 0);
            }
        }

        // Not found. The open sector will therefore be on the back of the tip
        // at the greatest angle.
        HEdgeTip const &tip = hedgeTips.back();
        return (tip.hasBack()? lineSegment(tip.back()).sector : 0);
    }

    bool release(MapElement *elm)
    {
        switch(elm->type())
        {
        case DMU_VERTEX: {
            Vertex *vtx = elm->castTo<Vertex>();
            /// @todo optimize: Poor performance O(n).
            for(uint i = 0; i < vertexes.size(); ++i)
            {
                if(vertexes[i] == vtx)
                {
                    vertexes[i] = 0;
                    // There is now one fewer Vertex.
                    numVertexes -= 1;
                    return true;
                }
            }
            break; }

        case DMU_HEDGE:
            /// @todo fixme: Implement a mechanic for tracking HEdge ownership.
            return true;

        case DMU_BSPLEAF:
        case DMU_BSPNODE: {
            BspTreeNode *treeNode = treeNodeForBspElement(elm);
            if(treeNode)
            {
                BuiltBspElementMap::iterator found = treeNodeMap.find(elm);
                DENG2_ASSERT(found != treeNodeMap.end());
                treeNodeMap.erase(found);

                treeNode->setUserData(0);
                if(treeNode->isLeaf())
                {
                    // There is now one fewer BspLeaf.
                    numLeafs -= 1;
                }
                else
                {
                    // There is now one fewer BspNode.
                    numNodes -= 1;
                }
                return true;
            }
            break; }

        default: break;
        }

        // This object is not owned by us.
        return false;
    }

    /**
     * Notify interested parties of a "one-way window" in the map.
     *
     * @param line    The window line.
     * @param backFacingSector  Sector that the back of the line is facing.
     */
    void notifyOneWayWindowFound(Line &line, Sector &backFacingSector)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(OneWayWindowFound, i)
        {
            i->oneWayWindowFound(line, backFacingSector);
        }
    }

    /**
     * Notify interested parties of an unclosed sector in the map.
     *
     * @param sector    The problem sector.
     * @param nearPoint  Coordinates near to where the problem was found.
     */
    void notifyUnclosedSectorFound(Sector &sector, Vector2d const &nearPoint)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(UnclosedSectorFound, i)
        {
            i->unclosedSectorFound(sector, nearPoint);
        }
    }

    /**
     * Notify interested parties that a partial BSP leaf was built.
     *
     * @param leaf      The incomplete BSP leaf.
     * @param gapTotal  Number of gaps in the leaf.
     */
    void notifyPartialBspLeafBuilt(BspLeaf &leaf, uint gapTotal)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(PartialBspLeafBuilt, i)
        {
            i->partialBspLeafBuilt(leaf, gapTotal);
        }
    }

    /**
     * Notify interested parties that a migrant half-edge was built.
     *
     * @param hedge         The migrant half-edge.
     * @param facingSector  Sector that the half-edge is facing.
     */
    void notifyMigrantHEdgeBuilt(HEdge &migrant, Sector &facingSector)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(MigrantHEdgeBuilt, i)
        {
            i->migrantHEdgeBuilt(migrant, facingSector);
        }
    }

    bool sanityCheckHasRealHEdge(BspLeaf const &leaf) const
    {
        HEdge const *base = leaf.firstHEdge();
        HEdge const *hedge = base;
        do
        {
            if(hedge->hasLineSide()) return true;
        } while((hedge = &hedge->next()) != base);
        return false;
    }

#ifdef DENG_DEBUG
    void printSuperBlockHEdges(SuperBlock const &block)
    {
        foreach(HEdge const *hedge, block.hedges())
        {
            LineSegment const &info = lineSegment(*hedge);

            LOG_DEBUG("Build: %s %p sector: %d %s -> %s")
                << (hedge->hasLineSide()? "NORM" : "MINI")
                << hedge << (info.sector != 0? info.sector->indexInMap() : -1)
                << hedge->fromOrigin().asText() << hedge->toOrigin().asText();
        }
    }
#endif
};

Partitioner::Partitioner(GameMap const &map, int splitCostFactor)
    : d(new Instance(this, map, splitCostFactor))
{
    d->initForMap();
}

void Partitioner::setSplitCostFactor(int newFactor)
{
    if(d->splitCostFactor != newFactor)
    {
        d->splitCostFactor = newFactor;
    }
}

static AABox blockmapBounds(AABoxd const &mapBounds)
{
    AABox mapBoundsi;
    mapBoundsi.minX = int( de::floor(mapBounds.minX) );
    mapBoundsi.minY = int( de::floor(mapBounds.minY) );
    mapBoundsi.maxX = int(  de::ceil(mapBounds.maxX) );
    mapBoundsi.maxY = int(  de::ceil(mapBounds.maxY) );

    AABox blockBounds;
    blockBounds.minX = mapBoundsi.minX - (mapBoundsi.minX & 0x7);
    blockBounds.minY = mapBoundsi.minY - (mapBoundsi.minY & 0x7);
    int bw = ((mapBoundsi.maxX - blockBounds.minX) / 128) + 1;
    int bh = ((mapBoundsi.maxY - blockBounds.minY) / 128) + 1;

    blockBounds.maxX = blockBounds.minX + 128 * M_CeilPow2(bw);
    blockBounds.maxY = blockBounds.minY + 128 * M_CeilPow2(bh);
    return blockBounds;
}

void Partitioner::build()
{
    d->buildBsp(SuperBlockmap(blockmapBounds(d->map->bounds())));
}

BspTreeNode *Partitioner::root() const
{
    return d->rootNode;
}

uint Partitioner::numNodes()
{
    return d->numNodes;
}

uint Partitioner::numLeafs()
{
    return d->numLeafs;
}

uint Partitioner::numHEdges()
{
    return d->numHEdges;
}

uint Partitioner::numVertexes()
{
    return d->numVertexes;
}

Vertex &Partitioner::vertex(uint idx)
{
    DENG2_ASSERT(idx < d->vertexes.size());
    DENG2_ASSERT(d->vertexes[idx]);
    return *d->vertexes[idx];
}

void Partitioner::release(MapElement *mapElement)
{
    if(!d->release(mapElement))
    {
        LOG_AS("Partitioner::release");
        LOG_DEBUG("Attempted to release an unknown/unowned %s %p.")
            << DMU_Str(mapElement->type()) << de::dintptr(mapElement);
    }
}

static LineRelationship lineRelationship(coord_t fromDist, coord_t toDist, coord_t distEpsilon)
{
    // Collinear with "this" line?
    if(abs(fromDist) <= distEpsilon && abs(toDist) <= distEpsilon)
    {
        return Collinear;
    }

    // To the right of "this" line?.
    if(fromDist > -distEpsilon && toDist > -distEpsilon)
    {
        // Close enough to intercept?
        if(fromDist < distEpsilon || toDist < distEpsilon) return RightIntercept;
        return Right;
    }

    // To the left of "this" line?
    if(fromDist < distEpsilon && toDist < distEpsilon)
    {
        // Close enough to intercept?
        if(fromDist > -distEpsilon || toDist > -distEpsilon) return LeftIntercept;
        return Left;
    }

    return Intersects;
}

static Vector2d findBspLeafCenter(BspLeaf const &leaf)
{
    Vector2d center;
    size_t numPoints = 0;

    for(HEdge const *hedge = leaf.firstHEdge(); hedge; hedge = hedge->_next)
    {
        center += hedge->fromOrigin();
        center += hedge->toOrigin();
        numPoints += 2;
    }

    if(numPoints)
    {
        center.x /= numPoints;
        center.y /= numPoints;
    }

    return center;
}
