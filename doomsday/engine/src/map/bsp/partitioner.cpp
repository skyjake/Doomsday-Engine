/**
 * @file partitioner.cpp
 * BSP Partitioner. Recursive node creation and sorting. @ingroup map
 *
 * Originally based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted
 * on SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @author Copyright &copy; 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @author Copyright &copy; 1998-2000 Colin Reed <cph@moria.org.uk>
 * @author Copyright &copy; 1998-2000 Lee Killough <killough@rsn.hp.com>
 * @author Copyright &copy; 1997-1998 Raphael.Quinet <raphael.quinet@eed.ericsson.se>
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
#include <list>
#include <map>
#include <vector>
#include <algorithm>

#include <de/Error>
#include <de/Log>
#include <de/memory.h>

#include "de_base.h"

#include "map/p_mapdata.h"
#include "map/bspleaf.h"
#include "map/bspnode.h"
#include "map/hedge.h"
#include "map/vertex.h"

#include "map/bsp/hedgeinfo.h"
#include "map/bsp/hedgeintercept.h"
#include "map/bsp/hedgetip.h"
#include "map/bsp/hplane.h"
#include "map/bsp/linedefinfo.h"
#include "map/bsp/partitioncost.h"
#include "map/bsp/superblockmap.h"
#include "map/bsp/vertexinfo.h"
#include "map/bsp/partitioner.h"

#include "render/r_main.h" // validCount

using namespace de::bsp;

// Misc utility routines (most don't belong here...):

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

static LineRelationship lineRelationship(coord_t a, coord_t b, coord_t distEpsilon);
static AABoxd findMapBounds(GameMap* map);
static AABox blockmapBounds(AABoxd const& mapBounds);
static bool findBspLeafCenter(BspLeaf const& leaf, pvec2d_t midPoint);
static void initAABoxFromEditableLineDefVertexes(AABoxd* aaBox, const LineDef* line);
static Sector* findFirstSectorInHEdgeList(const BspLeaf* leaf);
//DENG_DEBUG_ONLY(static bool bspLeafHasRealHEdge(BspLeaf const& leaf));
//DENG_DEBUG_ONLY(static void printBspLeafHEdges(BspLeaf const& leaf));
//DENG_DEBUG_ONLY(static int printSuperBlockHEdgesWorker(SuperBlock* block, void* /*parameters*/));
//DENG_DEBUG_ONLY(static void printPartitionIntercepts(HPlane const& partition));

struct Partitioner::Instance
{
    typedef std::map<runtime_mapdata_header_t*, BspTreeNode*> BspTreeNodeMap;

    /// Used when sorting BSP leaf half-edges by angle around midpoint.
    typedef std::vector<HEdge*> HEdgeSortBuffer;

    /// Used when collection half-edges from the blockmap to build a leaf.
    /// @todo Refactor me away.
    typedef std::list<HEdge*> HEdgeList;

    /// HEdge split cost factor.
    int splitCostFactor;

    /// Current map which we are building BSP data for.
    GameMap* map;

    AABoxd mapBounds;

    /// @todo Refactor me away:
    uint* numEditableVertexes;
    Vertex*** editableVertexes;

    /// Running totals of constructed BSP data objects.
    uint numNodes;
    uint numLeafs;
    uint numHEdges;
    uint numVertexes;

    /// Extended info about LineDefs in the current map.
    typedef std::vector<LineDefInfo> LineDefInfos;
    LineDefInfos lineDefInfos;

    /// Extended info about HEdges in the BSP object tree.
    typedef std::map<HEdge*, HEdgeInfo> HEdgeInfos;
    HEdgeInfos hedgeInfos;

    /// Extended info about Vertexes in the current map (including extras).
    /// @note May be larger than Instance::numVertexes (deallocation is lazy).
    typedef std::vector<VertexInfo> VertexInfos;
    VertexInfos vertexInfos;

    /// Extra vertexes allocated for the current map.
    /// @note May be larger than Instance::numVertexes (deallocation is lazy).
    typedef std::vector<Vertex*> Vertexes;
    Vertexes vertexes;

    /// Root node of our internal binary tree around which the final BSP data
    /// objects are constructed.
    BspTreeNode* rootNode;

    /// Built BSP object to internal tree node LUT.
    BspTreeNodeMap treeNodeMap;

    /// HPlane used to model the "current" binary space half-plane.
    HPlane partition;

    /// Extra info about the partition plane.
    HEdgeInfo partitionInfo;
    LineDef* partitionLineDef;

    /// Unclosed sectors are recorded here so we don't print too many warnings.
    struct UnclosedSectorRecord
    {
        Sector* sector;
        vec2d_t nearPoint;

        UnclosedSectorRecord(Sector* _sector, coord_t x, coord_t y)
            : sector(_sector)
        {
            V2d_Set(nearPoint, x, y);
        }
    };
    typedef std::map<Sector*, UnclosedSectorRecord> UnclosedSectors;
    UnclosedSectors unclosedSectors;

    /// Unclosed BSP leafs are recorded here so we don't print too many warnings.
    struct UnclosedBspLeafRecord
    {
        BspLeaf* leaf;
        uint gapTotal;

        UnclosedBspLeafRecord(BspLeaf* _leaf, uint _gapTotal)
            : leaf(_leaf), gapTotal(_gapTotal)
        {}
    };
    typedef std::map<BspLeaf*, UnclosedBspLeafRecord> UnclosedBspLeafs;
    UnclosedBspLeafs unclosedBspLeafs;

    /// Migrant hedges are recorded here so we don't print too many warnings.
    struct MigrantHEdgeRecord
    {
        HEdge* hedge;
        Sector* facingSector;

        MigrantHEdgeRecord(HEdge* _hedge, Sector* _facingSector)
            : hedge(_hedge), facingSector(_facingSector)
        {}
    };
    typedef std::map<HEdge*, MigrantHEdgeRecord> MigrantHEdges;
    MigrantHEdges migrantHEdges;

    /// @c true = a BSP for the current map has been built successfully.
    bool builtOK;

    Instance(GameMap* _map, uint* _numEditableVertexes,
             Vertex*** _editableVertexes, int _splitCostFactor)
      : splitCostFactor(_splitCostFactor),
        map(_map),
        numEditableVertexes(_numEditableVertexes), editableVertexes(_editableVertexes),
        numNodes(0), numLeafs(0), numHEdges(0), numVertexes(0),
        rootNode(0), treeNodeMap(), partition(), partitionLineDef(0),
        unclosedSectors(), unclosedBspLeafs(), migrantHEdges(),
        builtOK(false)
    {
        initPartitionInfo();
    }

    ~Instance()
    {
        clearAllHEdgeTips();

        // We are finished with the BSP data.
        if(rootNode)
        {
            // If ownership of the BSP data has been claimed this should be a no-op.
            clearAllBspObjects();

            // Destroy our private BSP tree.
            delete rootNode;
        }
    }

    /**
     * Retrieve the extended build info for the specified @a hedge.
     * @return  Extended info for that HEdge.
     */
    HEdgeInfo& hedgeInfo(HEdge& hedge)
    {
        HEdgeInfos::iterator found = hedgeInfos.find(&hedge);
        if(found != hedgeInfos.end()) return found->second;
        throw de::Error("Partitioner::hedgeInfo", QString("Failed locating HEdgeInfo for 0x%1").arg(de::dintptr(&hedge), 0, 16));
    }

    const HEdgeInfo& hedgeInfo(const HEdge& hedge) const
    {
        HEdgeInfos::const_iterator found = hedgeInfos.find(const_cast<HEdge*>(&hedge));
        if(found != hedgeInfos.end()) return found->second;
        throw de::Error("Partitioner::hedgeInfo", QString("Failed locating HEdgeInfo for 0x%1").arg(de::dintptr(&hedge), 0, 16));
    }

    /**
     * Retrieve the extended build info for the specified @a vertex.
     * @return  Extended info for that Vertex.
     */
    VertexInfo& vertexInfo(const Vertex& vertex)
    {
        return vertexInfos[vertex.buildData.index - 1];
    }

    inline bool lineMightHaveWindowEffect(LineDef const* line)
    {
        if(!line) return false;
        if(line->inFlags & LF_POLYOBJ) return false;
        if((line->L_frontsidedef && line->L_backsidedef) || !line->L_frontsidedef) return false;
        //if(line->length <= 0 || line->buildData.overlap) return false;

        // Look for window effects by checking for an odd number of one-sided
        // linedefs owners for a single vertex. Idea courtesy of Graham Jackson.
        VertexInfo const& v1Info = vertexInfo(*line->L_v1);
        if((v1Info.oneSidedOwnerCount % 2) == 1 && (v1Info.oneSidedOwnerCount + v1Info.twoSidedOwnerCount) > 1) return true;

        VertexInfo const& v2Info = vertexInfo(*line->L_v2);
        if((v2Info.oneSidedOwnerCount % 2) == 1 && (v2Info.oneSidedOwnerCount + v2Info.twoSidedOwnerCount) > 1) return true;

        return false;
    }

    struct testForWindowEffectParams
    {
        double frontDist, backDist;
        Sector* frontOpen, *backOpen;
        LineDef* frontLine, *backLine;
        LineDef* testLine;
        coord_t mX, mY;
        bool castHoriz;

        testForWindowEffectParams()
            : frontDist(0), backDist(0), frontOpen(0), backOpen(0),
              frontLine(0), backLine(0), testLine(0), mX(0), mY(0), castHoriz(false)
        {}
    };

    /// @return  Always @c 0 (used as a traverser callback).
    static int testForWindowEffectWorker(LineDef* line, void* parameters = 0)
    {
        testForWindowEffectParams& p = *reinterpret_cast<testForWindowEffectParams*>(parameters);

        if(line == p.testLine) return false;
        if(LINE_SELFREF(line) /*|| line->buildData.overlap || line->length <= 0*/) return false;

        double dist = 0;
        Sector* hitSector = 0;
        bool isFront = false;
        if(p.castHoriz)
        {
            if(fabs(line->direction[VY]) < DIST_EPSILON) return false;

            if((MAX_OF(line->L_v1origin[VY], line->L_v2origin[VY]) < p.mY - DIST_EPSILON) ||
               (MIN_OF(line->L_v1origin[VY], line->L_v2origin[VY]) > p.mY + DIST_EPSILON)) return false;

            dist = (line->L_v1origin[VX] + (p.mY - line->L_v1origin[VY]) * line->direction[VX] / line->direction[VY]) - p.mX;

            isFront = ((p.testLine->direction[VY] > 0) != (dist > 0));

            dist = fabs(dist);
            if(dist < DIST_EPSILON) return false; // Too close (overlapping lines ?)

            hitSector = line->L_sector((p.testLine->direction[VY] > 0) ^ (line->direction[VY] > 0) ^ !isFront);
        }
        else // Cast vertically.
        {
            if(fabs(line->direction[VX]) < DIST_EPSILON) return false;

            if((MAX_OF(line->L_v1origin[VX], line->L_v2origin[VX]) < p.mX - DIST_EPSILON) ||
               (MIN_OF(line->L_v1origin[VX], line->L_v2origin[VX]) > p.mX + DIST_EPSILON)) return false;

            dist = (line->L_v1origin[VY] + (p.mX - line->L_v1origin[VX]) * line->direction[VY] / line->direction[VX]) - p.mY;

            isFront = ((p.testLine->direction[VX] > 0) == (dist > 0));

            dist = fabs(dist);

            hitSector = line->L_sector((p.testLine->direction[VX] > 0) ^ (line->direction[VX] > 0) ^ !isFront);
        }

        if(dist < DIST_EPSILON) return false; // Too close (overlapping lines ?)

        if(isFront)
        {
            if(dist < p.frontDist)
            {
                p.frontDist = dist;
                p.frontOpen = hitSector;
                p.frontLine = line;
            }
        }
        else
        {
            if(dist < p.backDist)
            {
                p.backDist = dist;
                p.backOpen = hitSector;
                p.backLine = line;
            }
        }

        return false;
    }

    void testForWindowEffect(LineDefInfo& lineInfo)
    {
        LineDef* line = lineInfo.lineDef;
        if(!lineMightHaveWindowEffect(line)) return;

        testForWindowEffectParams p;
        p.frontDist = p.backDist = DDMAXFLOAT;
        p.testLine = line;
        p.mX = (line->L_v1origin[VX] + line->L_v2origin[VX]) / 2.0;
        p.mY = (line->L_v1origin[VY] + line->L_v2origin[VY]) / 2.0;
        p.castHoriz = (fabs(line->direction[VX]) < fabs(line->direction[VY])? true : false);

        AABoxd scanRegion = mapBounds;
        if(p.castHoriz)
        {
            scanRegion.minY = MIN_OF(line->L_v1origin[VY], line->L_v2origin[VY]) - DIST_EPSILON;
            scanRegion.maxY = MAX_OF(line->L_v1origin[VY], line->L_v2origin[VY]) + DIST_EPSILON;
        }
        else
        {
            scanRegion.minX = MIN_OF(line->L_v1origin[VX], line->L_v2origin[VX]) - DIST_EPSILON;
            scanRegion.maxX = MAX_OF(line->L_v1origin[VX], line->L_v2origin[VX]) + DIST_EPSILON;
        }
        validCount++;
        GameMap_LineDefsBoxIterator(map, &scanRegion, testForWindowEffectWorker, (void*)&p);

        if(p.backOpen && p.frontOpen && line->L_frontsector == p.backOpen)
        {
            LOG_VERBOSE("LineDef #%d seems to be a One-Sided Window (back faces sector #%d).")
                << line->origIndex - 1 << p.backOpen->buildData.index - 1;

            lineInfo.windowEffect = p.frontOpen;
            line->inFlags |= LF_BSPWINDOW; /// @todo Refactor away.
        }
    }

    void initForMap()
    {
        mapBounds = findMapBounds(map);

        // Initialize vertex info for the initial set of vertexes.
        vertexInfos.resize(*numEditableVertexes);

        // Count the total number of one and two-sided line owners for each vertex.
        // (Used in the process of locating window effect lines.)
        for(uint i = 0; i < *numEditableVertexes; ++i)
        {
            Vertex* vtx = (*editableVertexes)[i];
            VertexInfo& vtxInfo = vertexInfo(*vtx);
            Vertex_CountLineOwners(vtx, &vtxInfo.oneSidedOwnerCount, &vtxInfo.twoSidedOwnerCount);
        }

        // Initialize linedef info.
        uint numLineDefs = GameMap_LineDefCount(map);
        lineDefInfos.reserve(numLineDefs);
        for(uint i = 0; i < numLineDefs; ++i)
        {
            LineDef* line = GameMap_LineDef(map, i);
            lineDefInfos.push_back(LineDefInfo(line, DIST_EPSILON));
            LineDefInfo& lineInfo = lineDefInfos.back();

            testForWindowEffect(lineInfo);
        }
    }

    /// @return The right half-edge (from @a start to @a end).
    HEdge* buildHEdgesBetweenVertexes(Vertex* start, Vertex* end, Sector* frontSec,
        Sector* backSec, LineDef* lineDef, LineDef* partitionLineDef)
    {
        DENG2_ASSERT(start && end);

        HEdge* right = newHEdge(start, end, frontSec, lineDef, partitionLineDef);
        if(backSec)
        {
            HEdge* left  = newHEdge(end, start, backSec, lineDef, partitionLineDef);
            if(lineDef)
            {
                left->side = BACK;
            }

            // Twin the half-edges together.
            right->twin = left;
            left->twin  = right;
        }

        return right;
    }

    inline HEdge* linkHEdgeInSuperBlockmap(SuperBlock& block, HEdge* hedge)
    {
        DENG2_ASSERT(hedge);
        // Associate this half-edge with the final subblock.
        hedgeInfo(*hedge).bmapBlock = &block.push(*hedge);
        return hedge;
    }

    /**
     * Initially create all half-edges and add them to specified SuperBlock.
     */
    void createInitialHEdges(SuperBlock& hedgeList)
    {
        DENG2_FOR_EACH(LineDefInfos, i, lineDefInfos)
        {
            LineDefInfo const& lineInfo = *i;
            LineDef* line = lineInfo.lineDef;
            // Polyobj lines are completely ignored.
            if(line->inFlags & LF_POLYOBJ) continue;

            HEdge* front  = 0;
            coord_t angle = 0;

            if(!lineInfo.flags.testFlag(LineDefInfo::ZeroLength))
            {
                Sector* frontSec = line->L_frontsector;
                Sector* backSec  = 0;
                if(line->L_backsidedef)
                {
                    backSec = line->L_backsector;
                }
                else
                {
                    // Handle the 'One-Sided Window' trick.
                    Sector* windowSec = lineInfo.windowEffect;
                    if(windowSec) backSec = windowSec;
                }

                front = buildHEdgesBetweenVertexes(line->L_v1, line->L_v2,
                                                   frontSec, backSec, line, line);
                angle = hedgeInfo(*front).pAngle;

                linkHEdgeInSuperBlockmap(hedgeList, front);
                if(front->twin)
                {
                    linkHEdgeInSuperBlockmap(hedgeList, front->twin);
                }
            }

            // @todo edge tips should be created when half-edges are created.
            addHEdgeTip(line->L_v1, angle,                 front, front? front->twin : 0);
            addHEdgeTip(line->L_v2, M_InverseAngle(angle), front? front->twin : 0, front);
        }
    }

    bool buildBsp(SuperBlock& rootBlock)
    {
        try
        {
            createInitialHEdges(rootBlock);

            rootNode = buildNodes(rootBlock);
            // At this point we know that something useful was built.
            builtOK = true;

            windLeafs();
        }
        catch(de::Error& /*er*/)
        {
            // Don't handle the error here, simply record failure.
            builtOK = false;
            throw;
        }

        return builtOK;
    }

    const HPlaneIntercept* makePartitionIntersection(HEdge* hedge, bool leftSide)
    {
        DENG2_ASSERT(hedge);

        // Already present on this edge?
        Vertex* vertex = hedge->HE_v(leftSide);
        const HPlaneIntercept* inter = partitionInterceptByVertex(vertex);
        if(inter) return inter;

        HEdgeInfo& hInfo = hedgeInfo(*hedge);
        bool isSelfRefLine = (hInfo.lineDef && lineDefInfos[hInfo.lineDef->origIndex-1].flags.testFlag(LineDefInfo::SelfRef));
        HEdgeIntercept* intercept = newHEdgeIntercept(vertex, isSelfRefLine);

        return &partition.newIntercept(vertexDistanceFromPartition(vertex), intercept);
    }

    /**
     * @return  Same as @a final for caller convenience.
     */
    HEdgeIntercept& mergeHEdgeIntercepts(HEdgeIntercept& final, HEdgeIntercept& other)
    {
        /*
        LOG_TRACE("Merging intersections:");
        HEdgeIntercept::DebugPrint(final);
        HEdgeIntercept::DebugPrint(other);
        */

        if(final.selfRef && !other.selfRef)
        {
            if(final.before && other.before)
                final.before = other.before;

            if(final.after && other.after)
                final.after = other.after;

            final.selfRef = false;
        }

        if(!final.before && other.before)
            final.before = other.before;

        if(!final.after && other.after)
            final.after = other.after;

        /*
        LOG_TRACE("Result:");
        HEdgeIntercept::DebugPrint(final);
        */

        // Destroy the redundant other.
        delete &other;
        return final;
    }

    static bool mergeInterceptDecide(HPlaneIntercept& a, HPlaneIntercept& b, void* userData)
    {
        const coord_t distance = b - a;

        // Too great a distance between the two?
        if(distance > DIST_EPSILON) return false;

        HEdgeIntercept* cur  = reinterpret_cast<HEdgeIntercept*>(a.userData());
        HEdgeIntercept* next = reinterpret_cast<HEdgeIntercept*>(b.userData());

        // Merge info for the two intersections into one (next is destroyed).
        reinterpret_cast<Partitioner::Instance*>(userData)->mergeHEdgeIntercepts(*cur, *next);
        return true;
    }

    /**
     * @todo fixme: Logically this is very suspect. Implementing this logic by merging
     *       near-intercepts at hplane level is wrong because this does nothing about
     *       any intercepting half-edge vertices. Consequently, rather than moving the
     *       existing vertices and welding them, this will result in the creation of
     *       new gaps gaps along the partition (which buildHEdgesAtIntersectionGaps()
     *       will then warn about) and result in holes in the map.
     *
     *       This should be redesigned so that near-intercepting vertices are welded
     *       in a stable manner (i.e., not incrementally, which can result in vertices
     *       drifting away from the hplane). Logically, therefore, this should not be
     *       done prior to creating hedges along the partition - instead this should
     *       happen afterwards. -ds
     */
    void mergeIntercepts()
    {
        HPlane::mergepredicate_t callback = &Partitioner::Instance::mergeInterceptDecide;
        partition.mergeIntercepts(callback, this);
    }

    void buildHEdgesAtPartitionGaps(SuperBlock& rightList, SuperBlock& leftList)
    {
        HPlane::Intercepts::const_iterator node = partition.intercepts().begin();
        while(node != partition.intercepts().end())
        {
            HPlane::Intercepts::const_iterator np = node; np++;
            if(np == partition.intercepts().end()) break;

            HEdgeIntercept const* cur  = reinterpret_cast<HEdgeIntercept*>((*node).userData());
            HEdgeIntercept const* next = reinterpret_cast<HEdgeIntercept*>(  (*np).userData());

            if(!(!cur->after && !next->before))
            {
                // Check for some nasty open/closed or close/open cases.
                if(cur->after && !next->before)
                {
                    if(!cur->selfRef)
                    {
                        coord_t pos[2];

                        pos[VX] = cur->vertex->origin[VX] + next->vertex->origin[VX];
                        pos[VY] = cur->vertex->origin[VY] + next->vertex->origin[VY];
                        pos[VX] /= 2;
                        pos[VY] /= 2;

                        registerUnclosedSector(cur->after, pos[VX], pos[VY]);
                    }
                }
                else if(!cur->after && next->before)
                {
                    if(!next->selfRef)
                    {
                        coord_t pos[2];

                        pos[VX] = cur->vertex->origin[VX] + next->vertex->origin[VX];
                        pos[VY] = cur->vertex->origin[VY] + next->vertex->origin[VY];
                        pos[VX] /= 2;
                        pos[VY] /= 2;

                        registerUnclosedSector(next->before, pos[VX], pos[VY]);
                    }
                }
                else // This is definitely open space.
                {
                    // Choose the non-self-referencing sector when we can.
                    Sector* sector = cur->after;
                    if(cur->after != next->before)
                    {
                        // Do a sanity check on the sectors (just for good measure).
                        if(!cur->selfRef && !next->selfRef)
                        {
                            LOG_DEBUG("Sector mismatch (#%d [%1.1f, %1.1f] != #%d [%1.1f, %1.1f]).")
                                <<   cur->after->buildData.index-1 <<  cur->vertex->origin[VX] <<  cur->vertex->origin[VY]
                                << next->before->buildData.index-1 << next->vertex->origin[VX] << next->vertex->origin[VY];
                        }

                        if(cur->selfRef && !next->selfRef)
                            sector = next->before;
                    }

                    HEdge* right = buildHEdgesBetweenVertexes(cur->vertex, next->vertex, sector, sector,
                                                              0 /*no linedef*/, partitionLineDef);

                    // Add the new half-edges to the appropriate lists.
                    linkHEdgeInSuperBlockmap(rightList, right);
                    linkHEdgeInSuperBlockmap(leftList,  right->twin);

                    /*
                    HEdge* left = right->twin;
                    LOG_DEBUG("Capped partition gap:\n"
                              "  %p RIGHT sector #%d [%1.1f, %1.1f] to [%1.1f, %1.1f]\n"
                              "  %p LEFT  sector #%d [%1.1f, %1.1f] to [%1.1f, %1.1f]")
                        << de::dintptr(right) << (right->sector? right->sector->buildData.index - 1 : -1)
                        << right->HE_v1origin[VX] << right->HE_v1origin[VY]
                        << right->HE_v2origin[VX] << right->HE_v2origin[VY]
                        << de::dintptr(left) << (left->sector? left->sector->buildData.index - 1 : -1)
                        << left->HE_v1origin[VX]  << left->HE_v1origin[VY]
                        << left->HE_v2origin[VX]  << left->HE_v2origin[VY];
                    */
                }
            }

            node++;
        }
    }

    /**
     * Splits the given half-edge at the point (x,y). The new half-edge is returned.
     * The old half-edge is shortened (the original start vertex is unchanged), the
     * new half-edge becomes the cut-off tail (keeping the original end vertex).
     *
     * @note If the half-edge has a twin it is also split.
     */
    HEdge* splitHEdge(HEdge* oldHEdge, const_pvec2d_t point)
    {
        DENG2_ASSERT(oldHEdge);

        //LOG_DEBUG("Splitting hedge %p at [%1.1f, %1.1f].")
        //    << de::dintptr(oldHEdge) << point[VX] << point[VY];

        Vertex* newVert = newVertex(point);
        { HEdgeInfo& oldInfo = hedgeInfo(*oldHEdge);
        addHEdgeTip(newVert, M_InverseAngle(oldInfo.pAngle), oldHEdge->twin, oldHEdge);
        addHEdgeTip(newVert, oldInfo.pAngle, oldHEdge, oldHEdge->twin);
        }

        HEdge* newHEdge = cloneHEdge(*oldHEdge);
        HEdgeInfo& oldInfo = hedgeInfo(*oldHEdge);
        HEdgeInfo& newInfo = hedgeInfo(*newHEdge);

        newInfo.prevOnSide = oldHEdge;
        oldInfo.nextOnSide = newHEdge;

        oldHEdge->HE_v2 = newVert;
        oldInfo.initFromHEdge(*oldHEdge);

        newHEdge->HE_v1 = newVert;
        newInfo.initFromHEdge(*newHEdge);

        // Handle the twin.
        if(oldHEdge->twin)
        {
            //LOG_DEBUG("Splitting hedge twin %p.") << de::dintptr(oldHEdge->twin);

            // Copy the old hedge info.
            newHEdge->twin = cloneHEdge(*oldHEdge->twin);
            newHEdge->twin->twin = newHEdge;

            hedgeInfo(*newHEdge->twin).nextOnSide = oldHEdge->twin;
            hedgeInfo(*oldHEdge->twin).prevOnSide = newHEdge->twin;

            oldHEdge->twin->HE_v1 = newVert;
            hedgeInfo(*oldHEdge->twin).initFromHEdge(*oldHEdge->twin);

            newHEdge->twin->HE_v2 = newVert;
            hedgeInfo(*newHEdge->twin).initFromHEdge(*newHEdge->twin);

            // Has this already been added to a leaf?
            if(oldHEdge->twin->bspLeaf)
            {
                // Update the in-leaf references.
                oldHEdge->twin->next = newHEdge->twin;

                // There is now one more half-edge in this leaf.
                oldHEdge->twin->bspLeaf->hedgeCount += 1;
            }
        }

        return newHEdge;
    }

    void hedgePartitionDistance(HEdge const* hedge, HEdgeInfo const& pInfo, coord_t& a, coord_t& b)
    {
        DENG2_ASSERT(hedge);
        /// @attention Ensure half-edges produced from the partition's source linedef
        ///            are always handled as collinear. This special case is necessary
        ///            due to precision inaccuracies when a line is split into multiple
        ///            segments.
        HEdgeInfo const& hInfo = hedgeInfo(*hedge);
        if(hInfo.sourceLineDef == pInfo.sourceLineDef)
        {
            a = b = 0;
        }
        else
        {
            a = V2d_PointLinePerpDistance(hInfo.start, pInfo.direction, pInfo.pPerp, pInfo.pLength);
            b = V2d_PointLinePerpDistance(hInfo.end,   pInfo.direction, pInfo.pPerp, pInfo.pLength);
        }
    }

    /**
     * Determine the relationship half-edge @a hedge and the partition @a pInfo.
     * @return LineRelationship between the half-edge and the partition plane.
     */
    inline LineRelationship hedgePartitionRelationship(HEdge const* hedge,
        HEdgeInfo const& pInfo, coord_t& a, coord_t& b)
    {
        hedgePartitionDistance(hedge, pInfo, a, b);
        return lineRelationship(a, b, DIST_EPSILON);
    }

    /**
     * Take the given half-edge @a hedge, compare it with the partition plane and
     * determine which of the two sets it should be added to. If the half-edge is
     * found to intersect the partition, the intercept point is calculated and the
     * half-edge split at this point before then adding each to the relevant set
     * (any existing twin is handled uniformly, also).
     *
     * If the half-edge lies on, or crosses the partition then a new intercept is
     * added to the partition plane.
     *
     * @param hedge         Half-edge to be "divided".
     * @param rights        Set of half-edges on the right side of the partition.
     * @param lefts         Set of half-edges on the left side of the partition.
     */
    void divideHEdge(HEdge* hedge, SuperBlock& rights, SuperBlock& lefts)
    {
#define RIGHT false
#define LEFT  true

        coord_t a, b;
        LineRelationship rel = hedgePartitionRelationship(hedge, partitionInfo, a, b);
        switch(rel)
        {
        case Collinear: {
            makePartitionIntersection(hedge, RIGHT);
            makePartitionIntersection(hedge, LEFT);

            // Direction (vs that of the partition plane) determines in which subset
            // this half-edge belongs.
            HEdgeInfo const& hInfo = hedgeInfo(*hedge);
            if(hInfo.direction[VX] * partitionInfo.direction[VX] +
               hInfo.direction[VY] * partitionInfo.direction[VY] < 0)
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
                // Direction determines which side of the half-edge interfaces with
                // the new partition plane intercept.
                const bool leftSide = (a < DIST_EPSILON? RIGHT : LEFT);
                makePartitionIntersection(hedge, leftSide);
            }
            linkHEdgeInSuperBlockmap(rights, hedge);
            break;

        case Left:
        case LeftIntercept:
            if(rel == LeftIntercept)
            {
                const bool leftSide = (a > -DIST_EPSILON? RIGHT : LEFT);
                makePartitionIntersection(hedge, leftSide);
            }
            linkHEdgeInSuperBlockmap(lefts, hedge);
            break;

        case Intersects: {
            // Calculate the intercept point and split this half-edge.
            vec2d_t point;
            interceptHEdgePartition(hedge, a, b, point);
            HEdge* newHEdge = splitHEdge(hedge, point);

            // Ensure the new twin half-edge is inserted into the same block as the old twin.
            /// @todo This logic can now be moved into splitHEdge().
            if(hedge->twin && !hedge->twin->bspLeaf)
            {
                SuperBlock* bmapBlock = hedgeInfo(*hedge->twin).bmapBlock;
                DENG2_ASSERT(bmapBlock);
                linkHEdgeInSuperBlockmap(*bmapBlock, newHEdge->twin);
            }

            makePartitionIntersection(hedge, LEFT);

            // Direction determines which subset the hedges are added to.
            if(a < 0)
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
     * Remove all the half-edges from the list, partitioning them into the left or
     * right lists based on the given partition line. Adds any intersections onto the
     * intersection list as it goes.
     */
    void partitionHEdges(SuperBlock& hedgeList, SuperBlock& rights, SuperBlock& lefts)
    {
        // Iterative pre-order traversal of SuperBlock.
        SuperBlock* cur = &hedgeList;
        SuperBlock* prev = 0;
        while(cur)
        {
            while(cur)
            {
                HEdge* hedge;
                while((hedge = cur->pop()))
                {
                    // Disassociate the half-edge from the blockmap.
                    hedgeInfo(*hedge).bmapBlock = 0;

                    divideHEdge(hedge, rights, lefts);
                }

                if(prev == cur->parent())
                {
                    // Descending - right first, then left.
                    prev = cur;
                    if(cur->hasRight()) cur = cur->right();
                    else                cur = cur->left();
                }
                else if(prev == cur->right())
                {
                    // Last moved up the right branch - descend the left.
                    prev = cur;
                    cur = cur->left();
                }
                else if(prev == cur->left())
                {
                    // Last moved up the left branch - continue upward.
                    prev = cur;
                    cur = cur->parent();
                }
            }

            if(prev)
            {
                // No left child - back up.
                cur = prev->parent();
            }
        }

        // Sanity checks...
        if(!rights.totalHEdgeCount())
            throw de::Error("Partitioner::partitionhedges", "Right half-edge set is empty");

        if(!lefts.totalHEdgeCount())
            throw de::Error("Partitioner::partitionhedges", "Left half-edge set is empty");
    }

    /**
     * "Near miss" predicate.
     */
    static bool nearMiss(LineRelationship rel, coord_t a, coord_t b, coord_t* distance)
    {
        if(rel == Right &&
           !((a >= SHORT_HEDGE_EPSILON && b >= SHORT_HEDGE_EPSILON) ||
             (a <= DIST_EPSILON        && b >= SHORT_HEDGE_EPSILON) ||
             (b <= DIST_EPSILON        && a >= SHORT_HEDGE_EPSILON)))
        {
            // Need to know how close?
            if(distance)
            {
                if(a <= DIST_EPSILON || b <= DIST_EPSILON)
                    *distance = SHORT_HEDGE_EPSILON / MAX_OF(a, b);
                else
                    *distance = SHORT_HEDGE_EPSILON / MIN_OF(a, b);
            }
            return true;
        }

        if(rel == Left &&
           !((a <= -SHORT_HEDGE_EPSILON && b <= -SHORT_HEDGE_EPSILON) ||
             (a >= -DIST_EPSILON        && b <= -SHORT_HEDGE_EPSILON) ||
             (b >= -DIST_EPSILON        && a <= -SHORT_HEDGE_EPSILON)))
        {
            // Need to know how close?
            if(distance)
            {
                if(a >= -DIST_EPSILON || b >= -DIST_EPSILON)
                    *distance = SHORT_HEDGE_EPSILON / -MIN_OF(a, b);
                else
                    *distance = SHORT_HEDGE_EPSILON / -MAX_OF(a, b);
            }
            return true;
        }

        return false;
    }

    /**
     * "Near edge" predicate. Assumes intersecting line segment relationship.
     */
    static bool nearEdge(coord_t a, coord_t b, coord_t* distance)
    {
        if(abs(a) < SHORT_HEDGE_EPSILON || abs(b) < SHORT_HEDGE_EPSILON)
        {
            // Need to know how close?
            if(distance) *distance = SHORT_HEDGE_EPSILON / MIN_OF(abs(a), abs(b));
            return true;
        }
        return false;
    }

    void evalPartitionCostForHEdge(HEdgeInfo const& partInfo, HEdge const* hedge,
        PartitionCost& cost)
    {
        const int costFactorMultiplier = splitCostFactor;

        // Determine the relationship between this half-edge and the partition plane.
        coord_t a, b;
        LineRelationship rel = hedgePartitionRelationship(hedge, partInfo, a, b);
        switch(rel)
        {
        case Collinear: {
            // This half-edge runs along the same line as the partition.
            // Check whether it goes in the same direction or the opposite.
            const HEdgeInfo& hInfo = hedgeInfo(*hedge);
            if(hInfo.direction[VX] * partInfo.direction[VX] +
               hInfo.direction[VY] * partInfo.direction[VY] < 0)
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

            /**
             * Near misses are bad, as they have the potential to result in really short
             * half-hedges being produced further down the line.
             *
             * The closer the near miss, the higher the cost.
             */
            coord_t nearDist;
            if(nearMiss(rel, a, b, &nearDist))
            {
                cost.nearMiss += 1;
                cost.total += (int) (100 * costFactorMultiplier * (nearDist * nearDist - 1.0));
            }
            break; }

        case Left:
        case LeftIntercept: {
            cost.addHEdgeLeft(hedge);

            // Near miss?
            coord_t nearDist;
            if(nearMiss(rel, a, b, &nearDist))
            {
                /// @todo Why the cost multiplier imbalance between the left and right
                ///       edge near misses?
                cost.nearMiss += 1;
                cost.total += (int) (70 * costFactorMultiplier * (nearDist * nearDist - 1.0));
            }
            break; }

        case Intersects: {
            cost.splits += 1;
            cost.total  += 100 * costFactorMultiplier;

            /**
             * If the split point is very close to one end, which is quite an undesirable
             * situation (producing really short edges), thus a rather hefty surcharge.
             *
             * The closer to the edge, the higher the cost.
             */
            coord_t nearDist;
            if(nearEdge(a, b, &nearDist))
            {
                cost.iffy += 1;
                cost.total += (int) (140 * costFactorMultiplier * (nearDist * nearDist - 1.0));
            }
            break; }
        }
    }

    /**
     * @param block
     * @param best      Best half-edge found thus far.
     * @param bestCost  Running cost total result for the best half-edge thus far.
     * @param hedge     The candidate half-edge to be evaluated.
     * @param cost      PartitionCost analysis to be completed for this candidate.
     *                  Must have been initialized prior to calling.
     * @return  @c true iff this half-edge is suitable for use as a partition.
     */
    int evalPartitionCostForSuperBlock(SuperBlock const& block,
        HEdge* best, PartitionCost const& bestCost,
        HEdge* hedge, PartitionCost& cost)
    {
        /**
         * Test the whole block against the partition line to quickly handle all the
         * half-edges within it at once. Only when the partition line intercepts the
         * box do we need to go deeper into it.
         */
        HEdgeInfo const& hInfo = hedgeInfo(*hedge);
        AABox const& blockBounds = block.bounds();
        AABoxd bounds;

        /// @todo Why are we extending the bounding box for this test? Also, there is
        ///       no need to convert from integer to floating-point each time this is
        ///       tested. (If we intend to do this with floating-point then we should
        ///       return that representation in SuperBlock::bounds() ).
        bounds.minX = (double)blockBounds.minX - SHORT_HEDGE_EPSILON * 1.5;
        bounds.minY = (double)blockBounds.minY - SHORT_HEDGE_EPSILON * 1.5;
        bounds.maxX = (double)blockBounds.maxX + SHORT_HEDGE_EPSILON * 1.5;
        bounds.maxY = (double)blockBounds.maxY + SHORT_HEDGE_EPSILON * 1.5;

        int side = M_BoxOnLineSide2(&bounds, hInfo.start, hInfo.direction, hInfo.pPerp,
                                    hInfo.pLength, DIST_EPSILON);

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
        DENG2_FOR_EACH_CONST(SuperBlock::HEdges, it, block.hedges())
        {
            // Do we already have a better choice?
            if(best && !(cost < bestCost)) return false; // Stop iteration.

            // Evaluate the cost delta for this hedge.
            PartitionCost costDelta;
            evalPartitionCostForHEdge(hInfo, *it, costDelta);

            // Merge cost result into the cummulative total.
            cost += costDelta;
        }

        // Handle sub-blocks recursively.
        if(block.hasRight())
        {
            bool unsuitable = !evalPartitionCostForSuperBlock(*block.right(), best, bestCost, hedge, cost);
            if(unsuitable) return false;
        }

        if(block.hasLeft())
        {
            bool unsuitable = !evalPartitionCostForSuperBlock(*block.left(), best, bestCost, hedge, cost);
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
     * the line with least splits and has least difference of hald-edges on
     * either side of it.
     *
     * @return  @c true iff this half-edge is suitable for use as a partition.
     */
    bool evalPartition(SuperBlock const& block,
        HEdge* best, PartitionCost const& bestCost,
        HEdge* hedge, PartitionCost& cost)
    {
        if(!hedge) return false;

        // "Mini-hedges" are never potential candidates.
        LineDef* lineDef = hedge->lineDef;
        if(!lineDef) return false;

        if(!evalPartitionCostForSuperBlock(block, best, bestCost, hedge, cost))
        {
            // Unsuitable or we already have a better choice.
            return false;
        }

        // Make sure there is at least one real half-edge on each side.
        if(!cost.realLeft || !cost.realRight)
        {
            //LOG_DEBUG("evalPartition: No real half-edges on %s%sside")
            //    << (cost.realLeft? "" : "left ") << (cost.realRight? "" : "right ");
            return false;
        }

        // Increase cost by the difference between left and right.
        cost.total += 100 * abs(cost.realLeft - cost.realRight);

        // Allow minihedge counts to affect the outcome.
        cost.total += 50 * abs(cost.miniLeft - cost.miniRight);

        // Another little twist, here we show a slight preference for partition
        // lines that lie either purely horizontally or purely vertically.
        HEdgeInfo const& hInfo = hedgeInfo(*hedge);
        if(hInfo.pSlopeType != ST_HORIZONTAL && hInfo.pSlopeType != ST_VERTICAL)
            cost.total += 25;

        //LOG_DEBUG("evalPartition: %p: splits=%d iffy=%d near=%d left=%d+%d right=%d+%d cost=%d.%02d")
        //    << de::dintptr(&hInfo) << cost.splits << cost.iffy << cost.nearMiss
        //    << cost.realLeft << cost.miniLeft << cost.realRight << cost.miniRight
        //    << cost.total / 100 << cost.total % 100;

        return true;
    }

    void chooseNextPartitionFromSuperBlock(SuperBlock const& partList,
        SuperBlock const& hedgeList, HEdge** best, PartitionCost& bestCost)
    {
        DENG2_ASSERT(best);

        // Test each half-edge as a potential partition.
        DENG2_FOR_EACH_CONST(SuperBlock::HEdges, it, partList.hedges())
        {
            HEdge* hedge = *it;

            //LOG_DEBUG("chooseNextPartitionFromSuperBlock: %shedge %p sector:%d [%1.1f, %1.1f] -> [%1.1f, %1.1f]")
            //    << (hedge->lineDef? "" : "mini-") << de::dintptr(hedge)
            //    << (hedge->sector? hedge->sector->buildData.index - 1 : -1)
            //    << hedge->HE_v1origin[VX] << hedge->HE_v1origin[VY]
            //    << hedge->HE_v2origin[VX] << hedge->HE_v2origin[VY];

            // Optimization: Only the first half-edge produced from a given linedef
            // is tested per round of partition costing (they are all collinear).
            HEdgeInfo& hInfo = hedgeInfo(*hedge);
            if(hInfo.lineDef)
            {
                // Can we skip this half-edge?
                LineDefInfo& lInfo = lineDefInfos[hInfo.lineDef->origIndex-1];
                if(lInfo.validCount == validCount) continue; // Yes.

                lInfo.validCount = validCount;
            }

            // Calculate the cost metrics for this half-edge.
            PartitionCost cost;
            if(evalPartition(hedgeList, *best, bestCost, hedge, cost))
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
    HEdge* chooseNextPartition(SuperBlock const& hedgeList)
    {
        LOG_AS("Partitioner::choosePartition");

        PartitionCost bestCost;
        HEdge* best = 0;

        // Increment valid count so we can avoid testing the half edges produced
        // from a single linedef more than once per round of partition selection.
        validCount++;

        // Iterative pre-order traversal of SuperBlock.
        SuperBlock const* cur = &hedgeList;
        SuperBlock const* prev = 0;
        while(cur)
        {
            while(cur)
            {
                chooseNextPartitionFromSuperBlock(*cur, hedgeList, &best, bestCost);

                if(prev == cur->parent())
                {
                    // Descending - right first, then left.
                    prev = cur;
                    if(cur->hasRight()) cur = cur->right();
                    else                cur = cur->left();
                }
                else if(prev == cur->right())
                {
                    // Last moved up the right branch - descend the left.
                    prev = cur;
                    cur = cur->left();
                }
                else if(prev == cur->left())
                {
                    // Last moved up the left branch - continue upward.
                    prev = cur;
                    cur = cur->parent();
                }
            }

            if(prev)
            {
                // No left child - back up.
                cur = prev->parent();
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
     * Attempt to construct a new BspLeaf and from the supplied list of
     * half-edges.
     *
     * @param hedges  SuperBlock containing the list of half-edges with which
     *                to build the leaf using.
     *
     * @return  Newly created BspLeaf else @c NULL if degenerate.
     */
    BspLeaf* buildBspLeaf(HEdgeList const& hedges)
    {
        if(!hedges.size()) return 0;

        // Collapse all degenerate and orphaned leafs.
#if 0
        const bool isDegenerate = hedges.size() < 3;
        bool isOrphan = true;
        DENG2_FOR_EACH_CONST(HEdgeList, it, hedges)
        {
            const HEdge* hedge = *it;
            if(hedge->lineDef && hedge->lineDef->L_sector(hedge->side))
            {
                isOrphan = false;
                break;
            }
        }
#endif

        BspLeaf* leaf = 0;
        DENG2_FOR_EACH_CONST(HEdgeList, it, hedges)
        {
            HEdge* hedge = *it;
#if 0
            HEdgeInfos::iterator hInfoIt = hedgeInfos.find(hedge);
            HEdgeInfo& hInfo = hInfoIt->second;

            if(isDegenerate || isOrphan)
            {
                if(hInfo.prevOnSide)
                {
                    hedgeInfo(*hInfo.prevOnSide).nextOnSide = hInfo.nextOnSide;
                }
                if(hInfo.nextOnSide)
                {
                    hedgeInfo(*hInfo.nextOnSide).prevOnSide = hInfo.prevOnSide;
                }

                if(hedge->twin)
                {
                    hedge->twin->twin = 0;
                }

                /// @todo This is not logically correct from a mod compatibility
                ///       point of view. We should never clear the line > sector
                ///       references as these are used by the game(s) playsim in
                ///       various ways (for example, stair building). We should
                ///       instead flag the linedef accordingly. -ds
                if(hedge->lineDef)
                {
                    lineside_t* lside = HEDGE_SIDE(hedge);
                    lside->sector = 0;
                    /// @todo We could delete the SideDef also.
                    lside->sideDef = 0;

                    lineDefInfo(*hedge->lineDef).flags &= ~(LineDefInfo::SelfRef | LineDefInfo::Twosided);
                }

                hedgeInfos.erase(hInfoIt);
                HEdge_Delete(hedge);
                numHEdges -= 1;
                continue;
            }
#endif

            if(!leaf) leaf = BspLeaf_New();

            // Link it into head of the leaf's list.
            hedge->next = leaf->hedge;
            leaf->hedge = hedge;

            // Link hedge to this leaf.
            hedge->bspLeaf = leaf;

            // There is now one more half-edge in this leaf.
            leaf->hedgeCount += 1;
        }

        if(leaf)
        {
            // There is now one more BspLeaf;
            numLeafs += 1;
        }
        return leaf;
    }

    /**
     * Allocate another BspNode.
     *
     * @param origin       Origin of the half-plane in the map coordinate space.
     * @param angle        Angle of the half-plane in the map coordinate space.
     * @param rightBounds  Boundary of the right child map coordinate subspace. Can be @c NULL.
     * @param leftBounds   Boundary of the left child map coordinate subspace. Can be @a NULL.
     * @param rightChild   ?
     * @param leftChild    ?
     *
     * @return  Newly created BspNode.
     */
    BspNode* newBspNode(coord_t const origin[2], coord_t const angle[2],
        AABoxd& rightBounds, AABoxd& leftBounds,
        runtime_mapdata_header_t* rightChild = 0,
        runtime_mapdata_header_t* leftChild = 0)
    {
        BspNode* node = BspNode_New(origin, angle);
        if(rightChild)
        {
            BspNode_SetRight(node, rightChild);
        }
        if(leftChild)
        {
            BspNode_SetLeft(node, leftChild);
        }

        BspNode_SetRightBounds(node, &rightBounds);
        BspNode_SetLeftBounds(node,  &leftBounds);

        // There is now one more BspNode.
        numNodes += 1;
        return node;
    }

    BspTreeNode* newTreeNode(runtime_mapdata_header_t* bspOb,
        BspTreeNode* rightChild = 0, BspTreeNode* leftChild = 0)
    {
        BspTreeNode* subtree = new BspTreeNode(bspOb);
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

        treeNodeMap.insert(std::pair<runtime_mapdata_header_t*, BspTreeNode*>(bspOb, subtree));
        return subtree;
    }

    /**
     * Takes the half-edge list and determines if it is convex, possibly converting
     * it into a BSP leaf. Otherwise, the list is divided into two halves and recursion
     * will continue on the new sub list.
     *
     * This is done by scanning all of the half-edges and finding the one that does
     * the least splitting and has the least difference in numbers of half-edges on
     * either side.
     *
     * If the ones on the left side make a BspLeaf, then create another BspLeaf
     * else put the half-edges into the left list.
     *
     * If the ones on the right side make a BspLeaf, then create another BspLeaf
     * else put the half-edges into the right list.
     *
     * @param partList  The list of half-edges to be carved into convex subregions.
     * @return  Newly created subtree else @c NULL if degenerate.
     */
    BspTreeNode* buildNodes(SuperBlock& partList)
    {
        LOG_AS("Partitioner::buildNodes");

        runtime_mapdata_header_t* bspObject = 0; ///< Built BSP object at this node.
        BspTreeNode* rightTree = 0, *leftTree = 0;

        // Pick a half-edge to use as the next partition plane.
        HEdge* partHEdge = chooseNextPartition(partList);
        if(partHEdge)
        {
            //LOG_TRACE("Partition %p [%1.0f, %1.0f] -> [%1.0f, %1.0f].") << de::dintptr(hedge)
            //    << hedge->HE_v1origin[VX] << hedge->HE_v1origin[VY]
            //    << hedge->HE_v2origin[VX] << hedge->HE_v2origin[VY];

            // Reconfigure the half-plane for the next round of partitioning.
            configurePartition(partHEdge);
            HPlanePartition origPartition = HPlanePartition(partition.origin(), partition.direction());

            // Create left and right super blockmaps.
            /// @todo There should be no need to construct entirely independent
            ///       data structures to contain these hedge subsets.
            // Copy the bounding box of the edge list to the superblocks.
            SuperBlockmap rightHEdges = SuperBlockmap(partList.bounds());
            SuperBlockmap leftHEdges  = SuperBlockmap(partList.bounds());

            // Divide the half-edges into two subsets according to their spacial
            // relationship with the half-plane (splitting any which intersect).
            partitionHEdges(partList, rightHEdges, leftHEdges);
            partList.clear();
            addMiniHEdges(rightHEdges, leftHEdges);
            clearPartitionIntercepts();

            // Make a copy of the child bounds as we'll need this info to populate
            // a BspNode should we produce one (after the subtrees are processed).
            AABoxd rightBounds = rightHEdges.findHEdgeBounds();
            AABoxd leftBounds  = leftHEdges.findHEdgeBounds();

            // Recurse on each subset, right then left.
            rightTree = buildNodes(rightHEdges);
            leftTree  = buildNodes(leftHEdges);

            // Collapse degenerates upward.
            if(!rightTree || !leftTree)
                return rightTree? rightTree : leftTree;

            // Construct the new node and link up the subtrees.
            BspNode* node = newBspNode(origPartition.origin, origPartition.direction, rightBounds, leftBounds,
                                       rightTree->userData(), leftTree->userData());
            bspObject = reinterpret_cast<runtime_mapdata_header_t*>(node);
        }
        else
        {
            // No partition required/possible - already convex (or degenerate).
            BspLeaf* leaf = buildBspLeaf(collectHEdges(partList));
            partList.clear();

            // Not a leaf? (collapse upward).
            if(!leaf) return 0;

            bspObject = reinterpret_cast<runtime_mapdata_header_t*>(leaf);
        }

        return newTreeNode(bspObject, rightTree, leftTree);
    }

    /**
     * Determine the distance (euclidean) from @a vertex to the current partition plane.
     *
     * @param vertex  Vertex to test.
     */
    inline coord_t vertexDistanceFromPartition(Vertex const* vertex) const
    {
        DENG2_ASSERT(vertex);
        HEdgeInfo const& info = partitionInfo;
        return V2d_PointLineParaDistance(vertex->origin, info.direction, info.pPara, info.pLength);
    }

    /**
     * Calculate the intersection point between a half-edge and the current partition
     * plane. Takes advantage of some common situations like horizontal and vertical
     * lines to choose a 'nicer' intersection point.
     */
    void interceptHEdgePartition(HEdge const* hedge, coord_t perpC, coord_t perpD,
        coord_t point[2]) const
    {
        if(!hedge || !point) return;

        HEdgeInfo const& hInfo = hedgeInfo(*hedge);

        // Horizontal partition against vertical half-edge.
        if(partitionInfo.pSlopeType == ST_HORIZONTAL && hInfo.pSlopeType == ST_VERTICAL)
        {
            V2d_Set(point, hInfo.start[VX], partitionInfo.start[VY]);
            return;
        }

        // Vertical partition against horizontal half-edge.
        if(partitionInfo.pSlopeType == ST_VERTICAL && hInfo.pSlopeType == ST_HORIZONTAL)
        {
            V2d_Set(point, partitionInfo.start[VX], hInfo.start[VY]);
            return;
        }

        // 0 = start, 1 = end.
        coord_t ds = perpC / (perpC - perpD);

        if(hInfo.pSlopeType == ST_VERTICAL)
            point[VX] = hInfo.start[VX];
        else
            point[VX] = hInfo.start[VX] + (hInfo.direction[VX] * ds);

        if(hInfo.pSlopeType == ST_HORIZONTAL)
            point[VY] = hInfo.start[VY];
        else
            point[VY] = hInfo.start[VY] + (hInfo.direction[VY] * ds);
    }

    void clearPartitionIntercepts()
    {
        DENG2_FOR_EACH_CONST(HPlane::Intercepts, it, partition.intercepts())
        {
            HEdgeIntercept* intercept = static_cast<HEdgeIntercept*>((*it).userData());
            if(intercept) delete intercept;
        }
        partition.clear();
    }

    bool configurePartition(HEdge const* hedge)
    {
        LOG_AS("Partitioner::configurePartition");

        if(!hedge) return false;

        LineDef* lineDef = hedge->lineDef;
        if(!lineDef) return false; // A "mini hedge" is not suitable.

        // Clear the HEdge intercept data associated with points in the half-plane.
        clearPartitionIntercepts();

        // We can now reconfire the half-plane itself.
        setPartitionInfo(hedgeInfo(*hedge), lineDef);

        const Vertex* from = lineDef->L_v(hedge->side);
        const Vertex* to   = lineDef->L_v(hedge->side^1);
        partition.setOrigin(from->origin);

        vec2d_t angle; V2d_Subtract(angle, to->origin, from->origin);
        partition.setDirection(angle);

        //LOG_DEBUG("hedge %p [%1.1f, %1.1f] -> [%1.1f, %1.1f].")
        //    << de::dintptr(best) << from->origin[VX] << from->origin[VY]
        //    << angle[VX] << angle[VY];

        return true;
    }

    /**
     * Sort half-edges by angle (from the middle point to the start vertex).
     * The desired order (clockwise) means descending angles.
     */
    static void sortHEdgesByAngleAroundPoint(HEdgeSortBuffer::iterator begin,
        HEdgeSortBuffer::iterator end, pvec2d_t point)
    {
        DENG2_ASSERT(point);

        /// @par Algorithm
        /// "double bubble"
        bool done = false;
        while(begin != end && !done)
        {
            done = true;
            HEdgeSortBuffer::iterator it(begin);
            HEdgeSortBuffer::iterator next(begin);
            ++next;
            while(next != end)
            {
                HEdge* a = *it;
                HEdge* b = *next;
                coord_t angle1 = M_DirectionToAngleXY(a->v[0]->origin[VX] - point[VX],
                                                      a->v[0]->origin[VY] - point[VY]);
                coord_t angle2 = M_DirectionToAngleXY(b->v[0]->origin[VX] - point[VX],
                                                      b->v[0]->origin[VY] - point[VY]);

                if(angle1 + ANG_EPSILON < angle2)
                {
                    // Swap them.
                    std::swap(*next, *it);
                    done = false;
                }

                // Bubble down.
                ++it;
                ++next;
            }

            // Bubble up.
            --end;
        }
    }

    /**
     * Sort the half-edges linked within the given BSP leaf into a clockwise
     * order according to their position/orientation relative to to the
     * specified point.
     *
     * @param leaf  BSP leaf containing the list of hedges to be sorted.
     * @param point  Map space point around which to order.
     * @param sortBuffer  Buffer to use for sorting of the hedges.
     */
    static void clockwiseOrder(BspLeaf& leaf, pvec2d_t point, HEdgeSortBuffer& sortBuffer)
    {
        if(!leaf.hedge) return;

        // Ensure the sort buffer is large enough.
        if(leaf.hedgeCount > sortBuffer.size())
        {
            sortBuffer.resize(leaf.hedgeCount);
        }

        // Insert the hedges into the sort buffer.
        uint i = 0;
        for(HEdge* hedge = leaf.hedge; hedge; hedge = hedge->next, ++i)
        {
            sortBuffer[i] = hedge;
        }

        HEdgeSortBuffer::iterator begin = sortBuffer.begin();
        HEdgeSortBuffer::iterator end = begin + leaf.hedgeCount;
        sortHEdgesByAngleAroundPoint(begin, end, point);

        // Re-link the half-edge list in the order of the sort buffer.
        leaf.hedge = 0;
        for(uint i = 0; i < leaf.hedgeCount; ++i)
        {
            uint idx = (leaf.hedgeCount - 1) - i;
            uint j = idx % leaf.hedgeCount;

            sortBuffer[j]->next = leaf.hedge;
            leaf.hedge = sortBuffer[j];
        }

        // LOG_DEBUG("Sorted half-edges around [%1.1f, %1.1f]" << point[VX] << point[VY];
        // printBspLeafHEdges(leaf);
    }

    /**
     * Determine which sector this BSP leaf belongs to.
     */
    Sector* chooseSectorForBspLeaf(BspLeaf* leaf)
    {
        if(!leaf || !leaf->hedge) return 0;

        Sector* selfRefChoice = 0;
        HEdge* hedge = leaf->hedge;
        do
        {
            LineDef* line = hedge->lineDef;
            Sector* sector = line? line->L_sector(hedge->side) : 0;
            if(sector)
            {
                // The first sector from a non self-referencing line is our best choice.
                if(!LINE_SELFREF(line)) return sector;

                // Remember the self-referencing choice in case we've no better option.
                if(!selfRefChoice)
                    selfRefChoice = sector;
            }
        } while((hedge = hedge->next) != leaf->hedge);

        if(selfRefChoice) return selfRefChoice;

        // Last resort:
        /// @todo This is only necessary because of other failure cases in the
        ///       partitioning algorithm and to avoid producing a potentially
        ///       dangerous BSP - not assigning a sector to each leaf may result
        ///       in obscure fatal errors when in-game.
        hedge = leaf->hedge;
        do
        {
            if(hedge->sector)
            {
                return hedge->sector;
            }
        } while((hedge = hedge->next) != leaf->hedge);

        return 0; // Not reachable.
    }

    void clockwiseLeaf(BspTreeNode& tree, HEdgeSortBuffer& sortBuffer)
    {
        DENG2_ASSERT(tree.isLeaf());

        BspLeaf* leaf = reinterpret_cast<BspLeaf*>(tree.userData());
        vec2d_t center;

        V2d_Set(center, 0, 0);
        findBspLeafCenter(*leaf, center);
        clockwiseOrder(*leaf, center, sortBuffer);

        // Construct the leaf's hedge ring.
        if(leaf->hedge)
        {
            HEdge* hedge = leaf->hedge;
            forever
            {
                /// @todo Kludge: This should not be done here!
                if(hedge->lineDef)
                {
                    lineside_t* side = HEDGE_SIDE(hedge);
                    // Already processed?
                    if(!side->hedgeLeft)
                    {
                        side->hedgeLeft = hedge;
                        // Find the left-most hedge.
                        while(hedgeInfo(*side->hedgeLeft).prevOnSide)
                            side->hedgeLeft = hedgeInfo(*side->hedgeLeft).prevOnSide;

                        // Find the right-most hedge.
                        side->hedgeRight = hedge;
                        while(hedgeInfo(*side->hedgeRight).nextOnSide)
                            side->hedgeRight = hedgeInfo(*side->hedgeRight).nextOnSide;
                    }
                }
                /// kludge end

                if(hedge->next)
                {
                    // Reverse link.
                    hedge->next->prev = hedge;
                    hedge = hedge->next;
                }
                else
                {
                    // Circular link.
                    hedge->next = leaf->hedge;
                    hedge->next->prev = hedge;
                    break;
                }
            }
        }

        leaf->sector = chooseSectorForBspLeaf(leaf);
        if(!leaf->sector)
        {
            leaf->sector = 0;
            LOG_WARNING("BspLeaf %p is degenerate/orphan (%d HEdges).")
                << de::dintptr(leaf) << leaf->hedgeCount;
        }

        logMigrantHEdges(leaf);
        logUnclosedBspLeaf(leaf);

        if(!sanityCheckHasRealHEdge(leaf))
        {
            throw de::Error("Partitioner::clockwiseLeaf",
                            QString("BSP Leaf 0x%1 has no linedef-linked half-edge").arg(dintptr(leaf), 0, 16));
        }
    }

    /**
     * Sort all half-edges in each BSP leaf into a clockwise order.
     *
     * @note This cannot be done during Partitioner::buildNodes() as splitting
     * a half-edge with a twin will result in another half-edge being inserted
     * into that twin's leaf (usually in the wrong place order-wise).
     */
    void windLeafs()
    {
        HEdgeSortBuffer sortBuffer;
        DENG2_FOR_EACH(BspTreeNodeMap, it, treeNodeMap)
        {
            BspTreeNode* node = it->second;
            if(!node->isLeaf()) continue;
            clockwiseLeaf(*node, sortBuffer);
        }
    }

    /**
     * Analyze the partition intercepts, building new half-edges to cap
     * any gaps (new hedges are added onto the end of the appropriate list
     *(rights to @a rightList and lefts to @a leftList)).
     *
     * @param rightList  List of hedges on the right of the partition.
     * @param leftList   List of hedges on the left of the partition.
     */
    void addMiniHEdges(SuperBlock& rightList, SuperBlock& leftList)
    {
        LOG_TRACE("Building HEdges along partition [%1.1f, %1.1f] > [%1.1f, %1.1f]")
            <<     partitionInfo.start[VX] <<     partitionInfo.start[VY]
            << partitionInfo.direction[VX] << partitionInfo.direction[VY];

        //DEND_DEBUG_ONLY(printPartitionIntercepts(partition));

        // First, fix any near-distance issues with the intercepts.
        mergeIntercepts();
        buildHEdgesAtPartitionGaps(rightList, leftList);
    }

    /**
     * Search the given list for an intercept, if found; return it.
     *
     * @param vertex  Ptr to the vertex to look for.
     *
     * @return  Ptr to the found intercept, else @c NULL;
     */
    HPlaneIntercept const* partitionInterceptByVertex(Vertex* vertex)
    {
        if(!vertex) return NULL; // Hmm...

        DENG2_FOR_EACH_CONST(HPlane::Intercepts, it, partition.intercepts())
        {
            HPlaneIntercept const* inter = &*it;
            HEdgeIntercept* hedgeInter = reinterpret_cast<HEdgeIntercept*>(inter->userData());
            if(hedgeInter->vertex == vertex) return inter;
        }

        return NULL;
    }

    /**
     * Create a new intersection.
     */
    HEdgeIntercept* newHEdgeIntercept(Vertex* vertex, bool lineDefIsSelfReferencing)
    {
        DENG2_ASSERT(vertex);

        HEdgeIntercept* inter = new HEdgeIntercept();
        inter->vertex = vertex;
        inter->selfRef = lineDefIsSelfReferencing;

        inter->before = openSectorAtAngle(vertex, M_InverseAngle(partitionInfo.pAngle));
        inter->after  = openSectorAtAngle(vertex, partitionInfo.pAngle);

        return inter;
    }

    void clearBspObject(BspTreeNode& tree)
    {
        LOG_AS("Partitioner::clearBspObject");
        runtime_mapdata_header_t* dmuOb = tree.userData();
        if(!dmuOb) return;

        if(builtOK)
        {
            LOG_DEBUG("Clearing unclaimed %s %p.") << (tree.isLeaf()? "leaf" : "node") << de::dintptr(dmuOb);
        }

        if(tree.isLeaf())
        {
            BspLeaf_Delete(reinterpret_cast<BspLeaf*>(dmuOb));
            // There is now one less BspLeaf.
            numLeafs -= 1;
        }
        else
        {
            BspNode_Delete(reinterpret_cast<BspNode*>(dmuOb));
            // There is now one less BspNode.
            numNodes -= 1;
        }
        tree.setUserData(0);

        BspTreeNodeMap::iterator found = treeNodeMap.find(dmuOb);
        DENG2_ASSERT(found != treeNodeMap.end());
        treeNodeMap.erase(found);
    }

    void clearAllBspObjects()
    {
        DENG2_FOR_EACH(Vertexes, it, vertexes)
        {
            Vertex* vtx = *it;
            // Has ownership of this vertex been claimed?
            if(!vtx) continue;

            clearHEdgeTipsByVertex(vtx);
            M_Free(vtx);
        }

        DENG2_FOR_EACH(BspTreeNodeMap, it, treeNodeMap)
        {
            clearBspObject(*it->second);
        }
    }

    BspTreeNode* treeNodeForBspObject(runtime_mapdata_header_t* ob)
    {
        LOG_AS("Partitioner::treeNodeForBspObject");
        const int dmuType = DMU_GetType(ob);
        if(dmuType == DMU_BSPLEAF || dmuType == DMU_BSPNODE)
        {
            BspTreeNodeMap::const_iterator found = treeNodeMap.find(ob);
            if(found == treeNodeMap.end()) return 0;
            return found->second;
        }
        LOG_DEBUG("Attempted to locate using an unknown object %p (type: %d).")
            << de::dintptr(ob) << dmuType;
        return 0;
    }

    /**
     * Allocate another Vertex.
     *
     * @param origin  Origin of the vertex in the map coordinate space.
     *
     * @return  Newly created Vertex.
     */
    Vertex* newVertex(const_pvec2d_t origin)
    {
        Vertex* vtx;

        // Allocate with M_Calloc for uniformity with the editable vertexes.
        vtx = static_cast<Vertex*>(M_Calloc(sizeof *vtx));
        vtx->header.type = DMU_VERTEX;
        vtx->buildData.index = *numEditableVertexes + uint(vertexes.size() + 1); // 1-based index, 0 = NIL.
        vertexes.push_back(vtx);

        // There is now one more Vertex.
        numVertexes += 1;
        vertexInfos.push_back(VertexInfo());

        if(origin)
        {
            V2d_Copy(vtx->origin, origin);
        }
        return vtx;
    }

    inline void addHEdgeTip(Vertex* vtx, coord_t angle, HEdge* front, HEdge* back)
    {
        DENG2_ASSERT(vtx);
        vertexInfo(*vtx).addHEdgeTip(angle, front, back);
    }

    inline void clearHEdgeTipsByVertex(Vertex* vtx)
    {
        if(!vtx) return;
        vertexInfo(*vtx).clearHEdgeTips();
    }

    void clearAllHEdgeTips()
    {
        for(uint i = 0; i < *numEditableVertexes; ++i)
        {
            clearHEdgeTipsByVertex((*editableVertexes)[i]);
        }
    }

    /**
     * Create a new half-edge.
     */
    HEdge* newHEdge(Vertex* start, Vertex* end, Sector* sec, LineDef* lineDef = 0,
                    LineDef* sourceLineDef = 0)
    {
        DENG2_ASSERT(start && end && sec);

        HEdge* hedge = HEdge_New();
        hedge->HE_v1 = start;
        hedge->HE_v2 = end;
        hedge->sector = sec;
        hedge->lineDef = lineDef;

        // There is now one more HEdge.
        numHEdges += 1;
        hedgeInfos.insert(std::pair<HEdge*, HEdgeInfo>(hedge, HEdgeInfo()));

        HEdgeInfo& info = hedgeInfo(*hedge);
        info.lineDef = lineDef;
        info.sourceLineDef = sourceLineDef;
        info.initFromHEdge(*hedge);

        return hedge;
    }

    /**
     * Create a clone of an existing half-edge.
     */
    HEdge* cloneHEdge(const HEdge& other)
    {
        HEdge* hedge = HEdge_NewCopy(&other);

        // There is now one more HEdge.
        numHEdges += 1;
        hedgeInfos.insert(std::pair<HEdge*, HEdgeInfo>(hedge, HEdgeInfo()));

        memcpy(&hedgeInfo(*hedge), &hedgeInfo(other), sizeof(HEdgeInfo));

        return hedge;
    }

    HEdgeList collectHEdges(SuperBlock& partList)
    {
        HEdgeList hedges;

        // Iterative pre-order traversal of SuperBlock.
        SuperBlock* cur = &partList;
        SuperBlock* prev = 0;
        while(cur)
        {
            while(cur)
            {
                HEdge* hedge;
                while((hedge = cur->pop()))
                {
                    // Disassociate the half-edge from the blockmap.
                    hedgeInfo(*hedge).bmapBlock = 0;

                    hedges.push_back(hedge);
                }

                if(prev == cur->parent())
                {
                    // Descending - right first, then left.
                    prev = cur;
                    if(cur->hasRight()) cur = cur->right();
                    else                cur = cur->left();
                }
                else if(prev == cur->right())
                {
                    // Last moved up the right branch - descend the left.
                    prev = cur;
                    cur = cur->left();
                }
                else if(prev == cur->left())
                {
                    // Last moved up the left branch - continue upward.
                    prev = cur;
                    cur = cur->parent();
                }
            }

            if(prev)
            {
                // No left child - back up.
                cur = prev->parent();
            }
        }
        return hedges;
    }

    /**
     * Check whether a line with the given delta coordinates and beginning at this
     * vertex is open. Returns a sector reference if it's open, or NULL if closed
     * (void space or directly along a linedef).
     */
    Sector* openSectorAtAngle(Vertex* vtx, coord_t angle)
    {
        DENG2_ASSERT(vtx);
        const VertexInfo::HEdgeTips& hedgeTips = vertexInfo(*vtx).hedgeTips();

        if(hedgeTips.empty())
        {
            throw de::Error("Partitioner::openSectorAtAngle",
                            QString("Vertex #%1 has no hedge tips!").arg(vtx->buildData.index - 1));
        }

        // First check whether there's a wall_tip that lies in the exact direction of
        // the given direction (which is relative to the vtxex).
        DENG2_FOR_EACH_CONST(VertexInfo::HEdgeTips, it, hedgeTips)
        {
            const HEdgeTip& tip = *it;
            coord_t diff = fabs(tip.angle() - angle);
            if(diff < ANG_EPSILON || diff > (360.0 - ANG_EPSILON))
            {
                return NULL; // Yes, found one.
            }
        }

        // OK, now just find the first wall_tip whose angle is greater than the angle
        // we're interested in. Therefore we'll be on the FRONT side of that tip edge.
        DENG2_FOR_EACH_CONST(VertexInfo::HEdgeTips, it, hedgeTips)
        {
            const HEdgeTip& tip = *it;
            if(angle + ANG_EPSILON < tip.angle())
            {
                // Found it.
                return (tip.hasFront()? tip.front().sector : NULL);
            }
        }

        // Not found. The open sector will therefore be on the BACK of the tip at the
        // greatest angle.
        const HEdgeTip& tip = hedgeTips.back();
        return (tip.hasBack()? tip.back().sector : NULL);
    }

    /**
     * Initialize the extra info about the current partition plane.
     */
    void initPartitionInfo()
    {
        memset(&partitionInfo, 0, sizeof(partitionInfo));
        partitionLineDef = 0;
    }

    /**
     * Update the extra info about the current partition plane.
     */
    void setPartitionInfo(const HEdgeInfo& info, LineDef* lineDef)
    {
        memcpy(&partitionInfo, &info, sizeof(partitionInfo));
        partitionLineDef = lineDef;
    }

    bool release(runtime_mapdata_header_t* dmuOb)
    {
        int dmuType = DMU_GetType(dmuOb);
        switch(dmuType)
        {
        case DMU_VERTEX: {
            Vertex* vtx = reinterpret_cast<Vertex*>(dmuOb);
            if(vtx->buildData.index > 0 && uint(vtx->buildData.index) > *numEditableVertexes)
            {
                // Is this object owned?
                uint idx = uint(vtx->buildData.index) - 1 - *numEditableVertexes;
                if(idx < vertexes.size() && vertexes[idx])
                {
                    vertexes[idx] = 0;
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
            BspTreeNode* treeNode = treeNodeForBspObject(dmuOb);
            if(treeNode)
            {
                BspTreeNodeMap::iterator found = treeNodeMap.find(dmuOb);
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
     * Register the specified sector in the list of unclosed sectors.
     *
     * @param sector  Sector to be registered.
     * @param x  X coordinate near the open point.
     * @param y  X coordinate near the open point.
     *
     * @return @c true iff the sector was newly registered.
     */
    bool registerUnclosedSector(Sector* sector, coord_t x, coord_t y)
    {
        if(!sector) return false;

        // Has this sector already been registered?
        if(unclosedSectors.count(sector)) return false;

        // Add a new record.
        unclosedSectors.insert(std::pair<Sector*, UnclosedSectorRecord>(sector, UnclosedSectorRecord(sector, x, y)));

        // In the absence of a better mechanism, simply log this right away.
        /// @todo Implement something better!
        LOG_WARNING("Sector %p (#%d) is unclosed near [%1.1f, %1.1f].")
            << de::dintptr(sector) << sector->buildData.index - 1 << x << y;

        return true;
    }

    bool registerUnclosedBspLeaf(BspLeaf* leaf, uint gapTotal)
    {
        if(!leaf) return false;

        // Has this leaf already been registered?
        if(unclosedBspLeafs.count(leaf)) return false;

        // Add a new record.
        unclosedBspLeafs.insert(std::pair<BspLeaf*, UnclosedBspLeafRecord>(leaf, UnclosedBspLeafRecord(leaf, gapTotal)));

        // In the absence of a better mechanism, simply log this right away.
        /// @todo Implement something better!
        LOG_WARNING("HEdge list for BspLeaf %p is not closed (%u gaps, %u hedges).")
            << de::dintptr(leaf) << gapTotal << leaf->hedgeCount;

        /*
        HEdge* hedge = leaf->hedge;
        do
        {
            LOG_DEBUG("  half-edge %p [%1.1f, %1.1f] -> [%1.1f, %1.1f]")
                << de::dintptr(hedge)
                << hedge->HE_v1origin[VX] << hedge->HE_v1origin[VY],
                << hedge->HE_v2origin[VX] << hedge->HE_v2origin[VY];

        } while((hedge = hedge->next) != leaf->hedge);
        */
        return true;
    }

    void logUnclosedBspLeaf(BspLeaf* leaf)
    {
        if(!leaf) return;

        uint gaps = 0;
        const HEdge* hedge = leaf->hedge;
        do
        {
            HEdge* next = hedge->next;
            if(!FEQUAL(hedge->HE_v2origin[VX], next->HE_v1origin[VX]) ||
               !FEQUAL(hedge->HE_v2origin[VY], next->HE_v1origin[VY]))
            {
                gaps++;
            }
        } while((hedge = hedge->next) != leaf->hedge);

        if(gaps > 0)
        {
            registerUnclosedBspLeaf(leaf, gaps);
        }
    }

    /**
     * Register the specified half-edge as as migrant edge of the specified sector.
     *
     * @param migrant  Migrant half-edge to register.
     * @param sector  Sector containing the @a migrant half-edge.
     *
     * @return @c true iff the half-edge was newly registered.
     */
    bool registerMigrantHEdge(HEdge* migrant, Sector* sector)
    {
        if(!migrant || !sector) return false;

        // Has this pair already been registered?
        if(migrantHEdges.count(migrant)) return false;

        // Add a new record.
        migrantHEdges.insert(std::pair<HEdge*, MigrantHEdgeRecord>(migrant, MigrantHEdgeRecord(migrant, sector)));

        // In the absence of a better mechanism, simply log this right away.
        /// @todo Implement something better!
        if(migrant->lineDef)
            LOG_WARNING("Sector #%d has migrant HEdge facing #%d (line #%d).")
                << sector->buildData.index - 1 << migrant->sector->buildData.index - 1
                << migrant->lineDef->origIndex - 1;
        else
            LOG_WARNING("Sector #%d has migrant HEdge facing #%d.")
                << sector->buildData.index - 1 << migrant->sector->buildData.index - 1;

        return true;
    }

    /**
     * Look for migrant half-edges in the specified leaf and register them to the
     * list of migrants.
     *
     * @param leaf  BSP leaf to be searched.
     */
    void logMigrantHEdges(const BspLeaf* leaf)
    {
        if(!leaf) return;

        // Find a suitable half-edge for comparison.
        Sector* sector = findFirstSectorInHEdgeList(leaf);
        if(!sector) return;

        // Log migrants.
        HEdge* hedge = leaf->hedge;
        do
        {
            if(hedge->sector && hedge->sector != sector)
            {
                registerMigrantHEdge(hedge, sector);
            }
        } while((hedge = hedge->next) != leaf->hedge);
    }

    bool sanityCheckHasRealHEdge(const BspLeaf* leaf) const
    {
        DENG_ASSERT(leaf);
        HEdge* hedge = leaf->hedge;
        do
        {
            if(hedge->lineDef) return true;
        } while((hedge = hedge->next) != leaf->hedge);
        return false;
    }
};

Partitioner::Partitioner(GameMap& map, uint* numEditableVertexes,
    Vertex*** editableVertexes, int splitCostFactor)
{
    d = new Instance(&map, numEditableVertexes, editableVertexes, splitCostFactor);
    d->initForMap();
}

Partitioner::~Partitioner()
{
    delete d;
}

Partitioner& Partitioner::setSplitCostFactor(int factor)
{
    d->splitCostFactor = factor;
    return *this;
}

bool Partitioner::build()
{
    return d->buildBsp(SuperBlockmap(blockmapBounds(d->mapBounds)));
}

BspTreeNode* Partitioner::root() const
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

Vertex& Partitioner::vertex(uint idx)
{
    DENG2_ASSERT(idx < d->vertexes.size());
    DENG2_ASSERT(d->vertexes[idx]);
    return *d->vertexes[idx];
}

Partitioner& Partitioner::release(runtime_mapdata_header_t* ob)
{
    if(!d->release(ob))
    {
        LOG_AS("Partitioner::release");
        LOG_DEBUG("Attempted to release an unknown/unowned object %p.") << de::dintptr(ob);
    }
    return *this;
}

static LineRelationship lineRelationship(coord_t a, coord_t b, coord_t distEpsilon)
{
    // Collinear with the partition plane?
    if(abs(a) <= distEpsilon && abs(b) <= distEpsilon)
    {
        return Collinear;
    }

    // Right of the partition plane?.
    if(a > -distEpsilon && b > -distEpsilon)
    {
        // Close enough to intercept?
        if(a < distEpsilon || b < distEpsilon) return RightIntercept;
        return Right;
    }

    // Left of the partition plane?
    if(a < distEpsilon && b < distEpsilon)
    {
        // Close enough to intercept?
        if(a > -distEpsilon || b > -distEpsilon) return LeftIntercept;
        return Left;
    }

    return Intersects;
}

static bool findBspLeafCenter(BspLeaf const& leaf, pvec2d_t center)
{
    DENG2_ASSERT(center);

    vec2d_t avg;
    V2d_Set(avg, 0, 0);
    size_t numPoints = 0;

    for(HEdge* hedge = leaf.hedge; hedge; hedge = hedge->next)
    {
        V2d_Sum(avg, avg, hedge->HE_v1origin);
        V2d_Sum(avg, avg, hedge->HE_v2origin);
        numPoints += 2;
    }

    if(numPoints)
    {
        V2d_Set(center, avg[VX] / numPoints, avg[VY] / numPoints);
    }

    return true;
}

#if 0
DENG_DEBUG_ONLY(
static bool bspLeafHasRealHEdge(BspLeaf const& leaf)
{
    HEdge* hedge = leaf.hedge;
    do
    {
        if(hedge->lineDef) return true;
    } while((hedge = hedge->next) != leaf.hedge);
    return false;
})

DENG_DEBUG_ONLY(
static void printBspLeafHEdges(BspLeaf const& leaf)
{
    for(HEdge* hedge = leaf.hedge; hedge; hedge = hedge->next)
    {
        coord_t angle = M_DirectionToAngleXY(hedge->HE_v1origin[VX] - point[VX],
                                             hedge->HE_v1origin[VY] - point[VY]);

        LOG_DEBUG("  half-edge %p: Angle %1.6f [%1.1f, %1.1f] -> [%1.1f, %1.1f]")
            << de::dintptr(hedge) << angle
            << hedge->HE_v1origin[VX] << hedge->HE_v1origin[VY]
            << hedge->HE_v2origin[VX] << hedge->HE_v2origin[VY];
    }
})

DENG_DEBUG_ONLY(
static int printSuperBlockHEdgesWorker(SuperBlock* block, void* /*parameters*/)
{
    SuperBlock::DebugPrint(*block);
    return false; // Continue iteration.
})

DENG_DEBUG_ONLY(
static void printPartitionIntercepts(HPlane const& partition)
{
    uint index = 0;
    DENG2_FOR_EACH_CONST(HPlane::Intercepts, i, partition.intercepts())
    {
        Con_Printf(" %u: >%1.2f ", index++, i->distance());
        HEdgeIntercept::DebugPrint(*reinterpret_cast<HEdgeIntercept*>(i->userData()));
    }
})
#endif

static void initAABoxFromEditableLineDefVertexes(AABoxd* aaBox, const LineDef* line)
{
    const coord_t* from = line->L_v1origin;
    const coord_t* to   = line->L_v2origin;
    aaBox->minX = MIN_OF(from[VX], to[VX]);
    aaBox->minY = MIN_OF(from[VY], to[VY]);
    aaBox->maxX = MAX_OF(from[VX], to[VX]);
    aaBox->maxY = MAX_OF(from[VY], to[VY]);
}

static AABoxd findMapBounds2(GameMap* map)
{
    DENG2_ASSERT(map);
    AABoxd bounds;
    bool initialized = false;

    for(uint i = 0; i < GameMap_LineDefCount(map); ++i)
    {
        LineDef* line = GameMap_LineDef(map, i);

        // Do not consider zero-length LineDefs (already screened at a higher level).
        //if(lineDefInfo(*line).flags.testFlag(LineDefInfo::ZEROLENGTH)) continue;

        AABoxd lineAABox;
        initAABoxFromEditableLineDefVertexes(&lineAABox, line);

        if(initialized)
        {
            V2d_AddToBox(bounds.arvec2, lineAABox.min);
        }
        else
        {
            V2d_InitBox(bounds.arvec2, lineAABox.min);
            initialized = true;
        }

        V2d_AddToBox(bounds.arvec2, lineAABox.max);
    }

    if(!initialized)
    {
        // Clear.
        V2d_Set(bounds.min, DDMAXFLOAT, DDMAXFLOAT);
        V2d_Set(bounds.max, DDMINFLOAT, DDMINFLOAT);
    }
    return bounds;
}

static AABoxd findMapBounds(GameMap* map)
{
    AABoxd mapBounds = findMapBounds2(map);
    LOG_VERBOSE("Map bounds:")
        << " min[x:" << mapBounds.minX << ", y:" << mapBounds.minY << "]"
        << " max[x:" << mapBounds.maxX << ", y:" << mapBounds.maxY << "]";
    return mapBounds;
}

static AABox blockmapBounds(AABoxd const& mapBounds)
{
    AABox mapBoundsi;
    mapBoundsi.minX = (int) floor(mapBounds.minX);
    mapBoundsi.minY = (int) floor(mapBounds.minY);
    mapBoundsi.maxX = (int)  ceil(mapBounds.maxX);
    mapBoundsi.maxY = (int)  ceil(mapBounds.maxY);

    AABox blockBounds;
    blockBounds.minX = mapBoundsi.minX - (mapBoundsi.minX & 0x7);
    blockBounds.minY = mapBoundsi.minY - (mapBoundsi.minY & 0x7);
    int bw = ((mapBoundsi.maxX - blockBounds.minX) / 128) + 1;
    int bh = ((mapBoundsi.maxY - blockBounds.minY) / 128) + 1;

    blockBounds.maxX = blockBounds.minX + 128 * M_CeilPow2(bw);
    blockBounds.maxY = blockBounds.minY + 128 * M_CeilPow2(bh);
    return blockBounds;
}

static Sector* findFirstSectorInHEdgeList(const BspLeaf* leaf)
{
    DENG2_ASSERT(leaf);
    HEdge* hedge = leaf->hedge;
    do
    {
        if(hedge->sector)
        {
            return hedge->sector;
        }
    } while((hedge = hedge->next) != leaf->hedge);
    return NULL; // Nothing??
}
