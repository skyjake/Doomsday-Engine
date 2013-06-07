/** @file world/bsp/superblockmap.h World BSP SuperBlockmap.
 *
 * Originally based on glBSP 2.24 (in turn, based on BSP 2.3)
 * @see http://sourceforge.net/projects/glbsp/
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

#ifndef DENG_WORLD_BSP_SUPERBLOCKMAP_H
#define DENG_WORLD_BSP_SUPERBLOCKMAP_H

#include <QList>

#include <de/aabox.h>

#include <de/Log>
#include <de/Vector>

#include "world/bsp/linesegment.h"

namespace de {
namespace bsp {

class SuperBlock;

/**
 * Design is effectively that of a 2-dimensional kd-tree.
 *
 * @ingroup bsp
 */
class SuperBlockmap
{
public:
    /**
     * @param bounds  Bounding box in map coordinates for the whole blockmap.
     */
    SuperBlockmap(AABox const &bounds);

    /**
     * Retrieve the root SuperBlock.
     * @return  Root SuperBlock instance.
     */
    SuperBlock &root();

    /**
     * Find the axis-aligned bounding box defined by the vertices of all line
     * segments in the blockmap. If empty an AABox initialized to the "cleared"
     * state (i.e., min > max) will be returned.
     *
     * @return  Determined line segment bounds.
     */
    AABoxd findSegmentBounds();

    /**
     * Empty the SuperBlockmap unlinking the line segments and clearing all
     * blocks.
     */
    void clear();

    operator SuperBlock &() { return root(); }

    friend class SuperBlock;

private:
    DENG2_PRIVATE(d)
};

/**
 * Subblocks:
 * RIGHT - has the lower coordinates.
 * LEFT  - has the higher coordinates.
 * Division of a block always occurs horizontally:
 *     e.g. 512x512 -> 256x512 -> 256x256.
 *
 * @ingroup bsp
 */
class SuperBlock
{
public:
    typedef QList<LineSegment::Side *> Segments;

    /// A SuperBlock may be subdivided with two child subblocks which are
    /// uniquely identifiable by these associated ids.
    enum ChildId { Right, Left };

private:
    /**
     * SuperBlock objects must be constructed within the context of an owning
     * SuperBlockmap. Instantiation outside of this context is not permitted.
     * @ref SuperBlockmap
     */
    SuperBlock(SuperBlockmap &blockmap);
    SuperBlock(SuperBlock &parentPtr, ChildId childId, bool splitVertical);
    ~SuperBlock();

    /**
     * Attach a new SuperBlock instance as a child of this.
     * @param childId  Unique identifier of the child.
     * @param splitVertical  @c true= Subdivide this block on the y axis rather
     *                       than the x axis.
     */
    SuperBlock *addChild(ChildId childId, bool splitVertical);

    inline SuperBlock *addRight(bool splitVertical) { return addChild(Right, splitVertical); }
    inline SuperBlock *addLeft(bool splitVertical)  { return addChild(Left,  splitVertical); }

public:

    SuperBlock &clear();

    /**
     * Retrieve SuperBlockmap owner of the block.
     */
    SuperBlockmap &blockmap() const;

    /**
     * Retrieve the axis-aligned bounding box of the block in the blockmap.
     * Not to be confused with the bounds defined by the line segment geometry
     * which is determined by @ref findSegmentBounds().
     *
     * @return  Axis-aligned bounding box of the block.
     */
    AABox const &bounds() const;

    /**
     * Returns true if the block's dimensions are small enough that it should
     * be considered a "leaf". Blocks in a SuperBlockmap are only subdivided
     * until they reach an (x:256, y:256) lower bound on their dimensions. At
     * which point they are considered as a "leaf".
     */
    bool isLeaf() const
    {
        AABox const &aaBox = bounds();
        Vector2i dimensions = Vector2i(aaBox.max) - Vector2i(aaBox.min);
        return (dimensions.x <= 256 && dimensions.y <= 256);
    }

    /**
     * Returns a pointer to the parent block; otherwise @c 0.
     *
     * @see hasParent()
     */
    SuperBlock *parent() const;

    /**
     * Retrieve a child of this subblock. Callers must first determine if a
     * child is present using SuperBlock::hasChild() (or similar), attempting
     * to call this with no child present results in fatal error.
     *
     * @param childId  Subblock identifier.
     * @return  Selected subblock.
     */
    SuperBlock *child(ChildId childId) const;

    /**
     * Returns the right subblock.
     * @see SuperBlock::child()
     */
    inline SuperBlock *right() const { return child(Right); }

    /**
     * Returns the left subblock.
     * @see SuperBlock::childPtr()
     */
    inline SuperBlock *left()  const { return child(Left); }

    /**
     * Perform a depth-first traversal over all child superblocks and then
     * ultimately visiting this instance, making a callback for each object
     * visited. Iteration ends when all superblocks have been visited or a
     * callback returns a non-zero value.
     *
     * @param callback       Callback function ptr.
     * @param parameters     Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int traverse(int (*callback)(SuperBlock *, void *), void *parameters = 0);

    /**
     * Find the axis-aligned bounding box defined by the vertices of all line
     * segments in the block. If empty an AABox initialized to the "cleared"
     * state (i.e., min > max) will be returned.
     *
     * @return  Determined line segment bounds.
     */
    AABoxd findSegmentBounds();

    /**
     * Retrieve the total number of line segments in this and all child blocks.
     *
     * @param addMap  Include map line segments in the total.
     * @param addPart Include partition line segments in the total.
     *
     * @return  Determined line segment total.
     */
    int segmentCount(bool addMap, bool addPart) const;

    /// Convenience functions for retrieving line segment totals:
    inline int partSegmentCount() const  { return segmentCount(false, true); }
    inline int mapSegmentCount() const   { return segmentCount(true, false); }
    inline int totalSegmentCount() const { return segmentCount(true, true);  }

    /**
     * Push (link) the given line segment onto the FIFO list of segments linked
     * to this superblock.
     *
     * @param segment  Line segment to add.
     *
     * @return  SuperBlock that @a segment was linked to.
     */
    SuperBlock &push(LineSegment::Side &segment);

    /**
     * Pop (unlink) the next line segment from the FIFO list of segments linked
     * to this superblock.
     *
     * @return  Previous top-most line segment; otherwise @c 0 (empty).
     */
    LineSegment::Side *pop();

    /**
     * Collate (unlink) all line segments from "this" and all child blocks
     * to a new segment list.
     */
    Segments collateAllSegments();

    /**
     * Provides access to the list of line segments in the block, for efficient
     * traversal.
     */
    Segments const &segments() const;

    /**
     * SuperBlockmap creates instances of SuperBlock so it needs to use the
     * special private constructor that takes an existing superblock object
     * reference as a parameter.
     */
    friend struct SuperBlockmap::Instance;

private:
    struct Instance;
    Instance *d;
};

} // namespace bsp
} // namespace de

#endif // DENG_WORLD_BSP_SUPERBLOCKMAP_H
