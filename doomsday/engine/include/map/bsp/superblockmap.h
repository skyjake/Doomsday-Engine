/**
 * @file superblockmap.h
 * BSP Builder SuperBlockmap. @ingroup map
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

#ifndef LIBDENG_BSP_SUPERBLOCKMAP
#define LIBDENG_BSP_SUPERBLOCKMAP

#include "dd_def.h"
#include "dd_types.h"
#include "map/p_mapdata.h"

#include <de/Log>
#include <list>

namespace de {
namespace bsp {

class SuperBlock;

#ifdef RIGHT
#  undef RIGHT
#endif

#ifdef LEFT
#  undef LEFT
#endif

class SuperBlockmap
{
public:
    /**
     * @param bounds  Bounding box in map coordinates for the whole blockmap.
     */
    SuperBlockmap(const AABox& bounds);
    ~SuperBlockmap();

    /**
     * Retrieve the root SuperBlock.
     * @return  Root SuperBlock instance.
     */
    SuperBlock& root();

    /**
     * Find the axis-aligned bounding box defined by the vertices of all
     * HEdges within this superblockmap. If there are no HEdges linked to
     * this then @a bounds will be initialized to the "cleared" state
     * (i.e., min[x,y] > max[x,y]).
     *
     * @return bounds  Determined bounds are written here.
     */
    AABoxd findHEdgeBounds();

    /**
     * Empty this SuperBlockmap clearing all HEdges and sub-blocks.
     */
    void clear();

    operator SuperBlock&() { return root(); }

private:
    struct Instance;
    Instance* d;
    
    friend class SuperBlock;
};

/**
 * Subblocks:
 * RIGHT - has the lower coordinates.
 * LEFT  - has the higher coordinates.
 * Division of a block always occurs horizontally:
 *     e.g. 512x512 -> 256x512 -> 256x256.
 */
class SuperBlock
{
public:
    typedef std::list<HEdge*> HEdges;

    /// A SuperBlock may be subdivided with two child subblocks which are
    /// uniquely identifiable by these associated ids.
    enum ChildId
    {
        RIGHT,
        LEFT
    };

    /// Assert that the specified value is a valid @a childId.
    static void inline assertValidChildId(ChildId DENG_DEBUG_ONLY(childId))
    {
        Q_ASSERT(childId == RIGHT || childId == LEFT);
    }

public:
    SuperBlock& clear();

    /**
     * Retrieve the SuperBlockmap which owns this block.
     * @return  The owning SuperBlockmap instance.
     */
    SuperBlockmap& blockmap() const;

    /**
     * Retrieve the axis-aligned bounding box defined for this superblock
     * during instantiation. Note that this is NOT the bounds defined by
     * the linked HEdges' vertices (see SuperBlock::findHEdgeBounds()).
     *
     * @return  Axis-aligned bounding box.
     */
    const AABox& bounds() const;

    /**
     * Is this block small enough to be considered a "leaf"? A leaf in this
     * context is a block whose dimensions are <= [256, 256] and thus can
     * not be subdivided any further.
     */
    bool isLeaf() const;

    bool hasParent() const;

    SuperBlock* parent() const;

    /**
     * Does this block have a child subblock?
     */
    bool hasChild(ChildId childId) const;

    /**
     * Does this block have a Right child subblock?
     */
    inline bool hasRight() const { return hasChild(RIGHT); }

    /**
     * Does this block have a Left child subblock?
     */
    inline bool hasLeft() const  { return hasChild(LEFT); }

    /**
     * Retrieve a child of this subblock. Callers must first determine if a
     * child is present using SuperBlock::hasChild() (or similar), attempting
     * to call this with no child present results in fatal error.
     *
     * @param childId  Subblock identifier.
     * @return  Selected subblock.
     */
    SuperBlock* child(ChildId childId) const;

    /**
     * Returns the right sub-block.
     * @see SuperBlock::child()
     */
    inline SuperBlock* right() const { return child(RIGHT); }

    /**
     * Returns the left sub-block.
     * @see SuperBlock::child()
     */
    inline SuperBlock* left()  const { return child(LEFT); }

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

    /**
     * Find the axis-aligned bounding box defined by the vertices of all
     * HEdges within this superblock. If there are no HEdges linked to
     * this then @a bounds will be initialized to the "cleared" state
     * (i.e., min[x,y] > max[x,y]).
     *
     * @param bounds  Determined bounds are written here.
     */
    void findHEdgeBounds(AABoxd& bounds);

    /**
     * Retrieve the total number of HEdges linked in this superblock
     * (including any within child superblocks).
     *
     * @param addReal       Include the "real" half-edges in the total.
     * @param addMini       Include the "mini" half-edges in the total.
     *
     * @return  Total HEdge count.
     */
    uint hedgeCount(bool addReal, bool addMini) const;

    /// Convenience functions for retrieving HEdge totals:
    inline uint miniHEdgeCount() const {  return hedgeCount(false, true); }
    inline uint realHEdgeCount() const { return hedgeCount(true, false); }
    inline uint totalHEdgeCount() const { return hedgeCount(true, true); }

    /**
     * Push (link) the given HEdge onto the FIFO list of half-edges linked
     * to this superblock.
     *
     * @param hedge  HEdge instance to add.
     * @return  SuperBlock instance the HEdge was actually linked to.
     */
    SuperBlock& push(HEdge& hedge);

    /**
     * Pop (unlink) the next HEdge from the FIFO list of half-edges linked
     * to this superblock.
     *
     * @return  Previous top-most HEdge instance or @c NULL if empty.
     */
    HEdge* pop();

    const HEdges& hedges() const;

#ifdef DENG_DEBUG
    static void DebugPrint(SuperBlock const& inst)
    {
        DENG2_FOR_EACH_CONST(SuperBlock::HEdges, it, inst.hedges())
        {
            HEdge* hedge = *it;
            LOG_DEBUG("Build: %s %p sector: %d [%1.1f, %1.1f] -> [%1.1f, %1.1f]")
                << (hedge->lineDef? "NORM" : "MINI")
                << hedge << hedge->sector->buildData.index
                << hedge->v[0]->origin[VX] << hedge->v[0]->origin[VY]
                << hedge->v[1]->origin[VX] << hedge->v[1]->origin[VY];
        }
    }
#endif

private:
    /**
     * SuperBlock objects must be constructed within the context of an
     * owning SuperBlockmap. Instantiation outside of this context is not
     * permitted. @ref SuperBlockmap
     */
    SuperBlock(SuperBlockmap& blockmap);
    SuperBlock(SuperBlock& parent, ChildId childId, bool splitVertical);
    ~SuperBlock();

    /**
     * Attach a new SuperBlock instance as a child of this.
     * @param childId  Unique identifier of the child.
     * @param splitVertical  @c true= Subdivide this block on the y axis
     *                       rather than the x axis.
     */
    SuperBlock* addChild(ChildId childId, bool splitVertical);

    inline SuperBlock* addRight(bool splitVertical) { return addChild(RIGHT, splitVertical); }
    inline SuperBlock* addLeft(bool splitVertical)  { return addChild(LEFT,  splitVertical); }

private:
    struct Instance;
    Instance* d;

    /**
     * SuperBlockmap creates instances of SuperBlock so it needs to use
     * the special private constructor that takes an existing superblock
     * object reference as a parameter.
     */
    friend struct SuperBlockmap::Instance;
};

} // namespace bsp
} // namespace de

#endif /// LIBDENG_BSP_SUPERBLOCKMAP
