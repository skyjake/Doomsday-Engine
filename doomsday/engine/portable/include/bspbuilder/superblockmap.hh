/**
 * @file superblockmap.hh
 * BSP Builder SuperBlock. @ingroup map
 *
 * Design is effectively that of a 2-dimensional kd-tree.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_BSPBUILDER_SUPERBLOCKMAP
#define LIBDENG_BSPBUILDER_SUPERBLOCKMAP

#include "dd_types.h"

#include "kdtree.h" /// @todo Remove me.
#include "bspbuilder/hedges.hh"

#include <list>
#include <assert.h>

#ifdef RIGHT
#  undef RIGHT
#endif

#ifdef LEFT
#  undef LEFT
#endif

namespace de {

class SuperBlockmap;

class SuperBlock {
friend class SuperBlockmap;

public:
    typedef std::list<bsp_hedge_t*> HEdges;

    enum ChildId
    {
        RIGHT,
        LEFT
    };

    static void inline assertValidChildId(ChildId childId)
    {
        assert(childId == RIGHT || childId == LEFT);
    }

private:
    SuperBlock(SuperBlockmap& blockmap) :
        _bmap(blockmap), _hedges(0), _realNum(0), _miniNum(0)
    {}

    ~SuperBlock() { clear(); }

public:
    /**
     * Retrieve the SuperBlockmap which owns this block.
     *
     * @return  The owning SuperBlockmap instance.
     */
    SuperBlockmap& blockmap() const { return _bmap; }

    /**
     * Retrieve the axis-aligned bounding box defined for this superblock
     * during instantiation. Note that this is NOT the bounds defined by
     * the linked HEdges' vertices (@see SuperBlock::findHEdgeBounds()).
     *
     * @return  Axis-aligned bounding box.
     */
    const AABox& bounds() const;

    bool hasChild(ChildId childId) const;

    inline bool hasRight() const { return hasChild(RIGHT); }
    inline bool hasLeft() const  { return hasChild(LEFT); }

    /**
     * Retrieve a pointer to a sub-block if present.
     * @param childId  Identifier to the sub-block.
     * @return  Selected sub-block else @c NULL.
     */
    SuperBlock& child(ChildId childId);

    /**
     * Retrieve the right sub-block. Use SuperBlock::hasRight() before calling.
     * @return  Right sub-block.
     */
    inline SuperBlock& right() { return child(RIGHT); }

    /**
     * Retrieve the right sub-block. Use SuperBlock::hasLeft() before calling.
     * @return  Left sub-block else @c NULL.
     */
    inline SuperBlock& left()  { return child(LEFT); }

    SuperBlock* addChild(ChildId childId, bool splitVertical);

    inline SuperBlock* addRight(bool splitVertical) { return addChild(RIGHT, splitVertical); }
    inline SuperBlock* addLeft(bool splitVertical)  { return addChild(LEFT,  splitVertical); }

    /**
     * Perform a depth-first traversal over all child superblocks and
     * then ultimately visiting this instance, making a callback for
     * each object visited. Iteration ends when all superblocks have
     * been visited or @a callback returns a non-zero value.
     *
     * @param callback       Callback function ptr.
     * @param parameters     Passed to the callback. Default=NULL.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int traverse(int (C_DECL *callback)(SuperBlock*, void*), void* parameters=NULL);

    inline HEdges::const_iterator hedgesBegin() const { return _hedges.begin(); }
    inline HEdges::const_iterator hedgesEnd() const { return _hedges.end(); }

    void findHEdgeBounds(AABoxf& bounds);

    /**
     * Retrieve the total number of HEdges linked in this superblock (including
     * any within child superblocks).
     *
     * @param addReal       Include the "real" half-edges in the total.
     * @param addMini       Include the "mini" half-edges in the total.
     *
     * @return  Total HEdge count.
     */
    uint hedgeCount(bool addReal, bool addMini) const;

    // Convenience functions for retrieving the HEdge totals:
    inline uint miniHEdgeCount() const {  return hedgeCount(false, true); }
    inline uint realHEdgeCount() const { return hedgeCount(true, false); }
    inline uint totalHEdgeCount() const { return hedgeCount(true, true); }

    /**
     * Push (link) the given HEdge onto the FIFO list of half-edges linked
     * to this superblock.
     *
     * @param hedge  HEdge instance to add.
     */
    SuperBlock* hedgePush(bsp_hedge_t* hedge);

    /**
     * Pop (unlink) the next HEdge from the FIFO list of half-edges linked
     * to this superblock.
     *
     * @return  Previous top-most HEdge instance or @c NULL if empty.
     */
    bsp_hedge_t* hedgePop();

protected:
    /// KdTree node in the owning SuperBlockmap.
    KdTreeNode* _tree;

private:
    void clear();

    void inline incrementHEdgeCount(bsp_hedge_t* hedge);
    void inline linkHEdge(bsp_hedge_t* hedge);

    /// SuperBlockmap that owns this SuperBlock.
    SuperBlockmap& _bmap;

    /// Half-edges completely contained by this block.
    HEdges _hedges;

    /// Number of real half-edges and minihedges contained by this block
    /// (including all sub-blocks below it).
    int _realNum;
    int _miniNum;
};

class SuperBlockmap {
public:
    SuperBlockmap(const AABox& bounds) { init(bounds); }
    ~SuperBlockmap() { clear(); }

    /**
     * Retrieve the root SuperBlock in this SuperBlockmap.
     *
     * @return  Root SuperBlock instance.
     */
    SuperBlock* root();

    /**
     * Find the axis-aligned bounding box defined by the vertices of all
     * HEdges within this superblock. If no HEdges are linked then @a bounds
     * will be set to the "cleared" state (i.e., min[x,y] > max[x,y]).
     *
     * @param bounds        Determined bounds are written here.
     */
    void findHEdgeBounds(AABoxf& aaBox);

    bool inline isLeaf(const SuperBlock& block) const;

private:
    void init(const AABox& bounds);

    void clear();
    void clearBlockWorker(SuperBlock& block);

    /**
     * The KdTree of SuperBlocks.
     *
     * Subblocks:
     * RIGHT - has the lower coordinates.
     * LEFT  - has the higher coordinates.
     * Division of a block always occurs horizontally:
     *     e.g. 512x512 -> 256x512 -> 256x256.
     */
    KdTree* _kdTree;
};

} // namespace de

#endif /// LIBDENG_BSPBUILDER_SUPERBLOCKMAP
