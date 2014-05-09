/** @file superblockmap.h  World BSP SuperBlockmap.
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
#include <de/BinaryTree>
#include <de/Vector>
#include "world/bsp/linesegment.h"

namespace de {
namespace bsp {

/**
 * @ingroup bsp
 */
class SuperBlockmap
{
public:
    class NodeData;
    typedef de::BinaryTree<NodeData *> Node;
    typedef QList<LineSegmentSide *>   Segments;

    class NodeData
    {
    public:
        /**
         * Retrieve the axis-aligned bounding box of the block in the blockmap.
         * Not to be confused with the bounds defined by the line segment geometry
         * which is determined by @ref findSegmentBounds().
         *
         * @return  Axis-aligned bounding box of the block.
         */
        AABox const &bounds() const;

        /**
         * Push (link) the given line segment onto the FIFO list of segments linked
         * to this superblock.
         *
         * @param segment  Line segment to add.
         *
         * @return  KdTreeBlockmap that @a segment was linked to.
         */
        Node &push(LineSegmentSide &segment);

        /**
         * Pop (unlink) the next line segment from the FIFO list of segments
         * linked to the node.
         *
         * @return  Previous top-most line segment; otherwise @c 0 (empty).
         */
        LineSegmentSide *pop();

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
         * Provides access to the list of line segments in the block, for efficient
         * traversal.
         */
        Segments const &segments() const;

    //private:
        /**
         * Block objects must be constructed within the context of an owning
         * SuperBlockmap. Instantiation outside of this context is not permitted.
         * @ref SuperBlockmap
         */
        NodeData(SuperBlockmap &owner, AABox const &bounds);

        Node *_node;

    private:
        DENG2_PRIVATE(d)
    };

public:
    /**
     * @param bounds  Bounding box in map coordinates for the whole blockmap.
     */
    SuperBlockmap(AABox const &bounds);

    /// Automatic translation from SuperBlockmap to the tree root.
    operator Node /*const*/ &();

private:
    DENG2_PRIVATE(d)
};

typedef SuperBlockmap::Node SuperBlockmapNode;

} // namespace bsp
} // namespace de

#endif // DENG_WORLD_BSP_SUPERBLOCKMAP_H
