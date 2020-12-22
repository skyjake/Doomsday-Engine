/** @file superblockmap.h  World BSP line segment block.
 *
 * Originally based on glBSP 2.24 (in turn, based on BSP 2.3)
 * @see http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2016 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_WORLD_BSP_LINESEGMENTBLOCK_H
#define DE_WORLD_BSP_LINESEGMENTBLOCK_H

#include <de/list.h>
#include <de/legacy/aabox.h>
#include <de/binarytree.h>
#include "linesegment.h"

namespace world {
namespace bsp {

/**
 * @ingroup bsp
 */
class LineSegmentBlock
{
public:
    typedef de::List<LineSegmentSide *> All;

public:
    LineSegmentBlock(const AABox &bounds);

    /**
     * Retrieve the axis-aligned bounding box of the block in the blockmap.
     * Not to be confused with the bounds defined by the line segment geometry
     * which is determined by @ref findSegmentBounds().
     *
     * @return  Axis-aligned bounding box of the block.
     */
    const AABox &bounds() const;

    void link(LineSegmentSide &seg);

    void addRef(const LineSegmentSide &seg);

    void decRef(const LineSegmentSide &seg);

    /**
     * Pop (unlink) the next line segment from the FIFO list of segments
     * linked to the node.
     *
     * @return  Previous top-most line segment; otherwise @c nullptr (empty).
     */
    LineSegmentSide *pop();

    int mapCount() const;
    int partCount() const;

    /**
     * Returns the total number of line segments in this and all child blocks.
     */
    int totalCount() const;

    /**
     * Provides access to the list of line segments in the block, for efficient
     * traversal.
     */
    const All &all() const;

private:
    DE_PRIVATE(d)
};

struct LineSegmentBlockTreeNode : public de::BinaryTree<LineSegmentBlock *> {
    LineSegmentBlockTreeNode(LineSegmentBlock *        lsb, // owned
                             LineSegmentBlockTreeNode *parent = nullptr);
    ~LineSegmentBlockTreeNode();

    LineSegmentBlockTreeNode *childPtr(ChildId side) const;
    LineSegmentBlockTreeNode *parentPtr() const;
    LineSegmentBlockTreeNode *rightPtr() const { return childPtr(Right); }
    LineSegmentBlockTreeNode *leftPtr() const  { return childPtr(Left); }
};

}  // namespace bsp
}  // namespace world

#endif  // DE_WORLD_BSP_LINESEGMENTBLOCK_H
