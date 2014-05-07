/** @file superblockmap.cpp  BSP Builder Super Blockmap.
 *
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <de/kdtree.h>
#include <de/vector1.h>

#include "world/bsp/superblockmap.h"

namespace de {

KdTree::Node::Node(KdTree &tree) : _owner(tree), _tree (0)
{}

KdTree::Node::~Node()
{
    clear();
    KdTreeNode_Delete(_tree);
}

KdTree::Node &KdTree::Node::clear()
{
    if(_tree)
    {
        // Recursively handle sub-blocks.
        for(uint num = 0; num < 2; ++num)
        {
            if(KdTreeNode *child = KdTreeNode_Child(_tree, num))
            {
                delete KdTreeNode_UserData(child);
                KdTreeNode_SetUserData(child, 0);
            }
        }
    }
    return *this;
}

AABox const &KdTree::Node::bounds() const
{
    return *KdTreeNode_Bounds(_tree);
}

KdTree::Node *KdTree::Node::parent() const
{
    KdTreeNode *pNode = KdTreeNode_Parent(_tree);
    if(!pNode) return 0;
    return static_cast<Node *>(KdTreeNode_UserData(pNode));
}

KdTree::Node *KdTree::Node::child(ChildId childId) const
{
    KdTreeNode *subtree = KdTreeNode_Child(_tree, childId == Left);
    if(!subtree) return 0;
    return static_cast<Node *>(KdTreeNode_UserData(subtree));
}

KdTree::KdTree(AABox const &bounds)
    : _nodes(KdTree_New(&bounds))
{
    // Attach the root node.
    Node *block = new Node(*this);
    block->_tree = KdTreeNode_SetUserData(KdTree_Root(_nodes), block);
}

KdTree::~KdTree()
{
    rootNode().clear();
    KdTree_Delete(_nodes);
}

void KdTree::clear()
{
    rootNode().clear();
}

KdTree::Node &KdTree::rootNode()
{
    return *static_cast<Node *>(KdTreeNode_UserData(KdTree_Root(_nodes)));
}

} // namespace de

/// ----------------------------------------------------------------------------

#include "world/bsp/linesegment.h"

using namespace de;
using namespace de::bsp;

DENG2_PIMPL_NOREF(SuperBlockmap::Block)
{
    Segments segments; ///< Line segments contained by the block.
    int mapNum;        ///< Running total of map-line segments at this node.
    int partNum;       ///< Running total of partition-line segments at this node.

    Instance() : mapNum(0), partNum(0) {}

    inline void linkSegment(LineSegmentSide &seg) {
        segments.prepend(&seg);
    }

    inline void incrementSegmentCount(LineSegmentSide const &seg) {
        if(seg.hasMapSide()) mapNum++;
        else                 partNum++;
    }

    inline void decrementSegmentCount(LineSegmentSide const &seg) {
        if(seg.hasMapSide()) mapNum--;
        else                 partNum--;
    }
};

SuperBlockmap::Block::Block(SuperBlockmap &bmap)
    : KdTree::Node(bmap)
    , d(new Instance)
{}

SuperBlockmap::Block::Block(Block &parent, ChildId childId, bool splitVertical)
    : KdTree::Node(parent)
    , d(new Instance)
{
    _tree = KdTreeNode_AddChild(parent._tree, 0.5, int(splitVertical), childId == Left, this);
}

SuperBlockmap::Block::Segments SuperBlockmap::Block::collateAllSegments()
{
    Segments segments;

#ifdef DENG2_QT_4_7_OR_NEWER
    segments.reserve(totalSegmentCount());
#endif

    // Iterative pre-order traversal of SuperBlock.
    Block *cur = this;
    Block *prev = 0;
    while(cur)
    {
        while(cur)
        {
            LineSegmentSide *seg;
            while((seg = cur->pop()))
            {
                segments << seg;
            }

            if(prev == cur->parent())
            {
                // Descending - right first, then left.
                prev = cur;
                if(cur->right()) cur = cur->right();
                else             cur = cur->left();
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

    return segments;
}

SuperBlockmap::Block::Segments const &SuperBlockmap::Block::segments() const
{
    return d->segments;
}

int SuperBlockmap::Block::segmentCount(bool addMap, bool addPart) const
{
    int total = 0;
    if(addMap)  total += d->mapNum;
    if(addPart) total += d->partNum;
    return total;
}

SuperBlockmap::Block &SuperBlockmap::Block::push(LineSegmentSide &seg)
{
    Block *sb = this;
    forever
    {
        // Update line segment counts.
        sb->d->incrementSegmentCount(seg);

        if(sb->isLeaf())
        {
            // No further subdivision possible.
            sb->d->linkSegment(seg);
            break;
        }

        ChildId p1, p2;
        bool splitVertical;
        if(sb->bounds().maxX - sb->bounds().minX >=
           sb->bounds().maxY - sb->bounds().minY)
        {
            // Wider than tall.
            int midPoint = (sb->bounds().minX + sb->bounds().maxX) / 2;
            p1 = seg.from().origin().x >= midPoint? Left : Right;
            p2 =   seg.to().origin().x >= midPoint? Left : Right;
            splitVertical = false;
        }
        else
        {
            // Taller than wide.
            int midPoint = (sb->bounds().minY + sb->bounds().maxY) / 2;
            p1 = seg.from().origin().y >= midPoint? Left : Right;
            p2 =   seg.to().origin().y >= midPoint? Left : Right;
            splitVertical = true;
        }

        if(p1 != p2)
        {
            // Line crosses midpoint; link it in and return.
            sb->d->linkSegment(seg);
            break;
        }

        // The segments lies in one half of this block. Create the sub-block
        // if it doesn't already exist, and loop back to add the seg.
        if(!sb->child(p1))
        {
            new Block(*sb, p1, splitVertical);
            // Note that the tree retains a pointer to the block; no leak.
        }

        sb = sb->child(p1);
    }
    return *sb;
}

LineSegmentSide *SuperBlockmap::Block::pop()
{
    if(d->segments.isEmpty()) return 0;

    LineSegmentSide *seg = d->segments.takeFirst();

    // Update line segment counts.
    d->decrementSegmentCount(*seg);

    return seg;
}

DENG2_PIMPL_NOREF(SuperBlockmap), public de::KdTree
{
    Instance(AABox const &bounds) : KdTree(bounds)
    {}

    /**
     * Find the axis-aligned bounding box defined by the vertices of all line
     * segments in the block. If empty an AABox initialized to the "cleared"
     * state (i.e., min > max) will be returned.
     *
     * @return  Determined line segment bounds.
     *
     * @todo Optimize: Cache this result.
     */
    void findBlockSegmentBoundsWorker(SuperBlock &block, AABoxd &retBounds,
                                      bool *initialized)
    {
        DENG2_ASSERT(initialized);
        if(block.segmentCount(true, true))
        {
            AABoxd bounds;
            bool first = false;

            foreach(LineSegmentSide *seg, block.segments())
            {
                AABoxd segBounds = seg->aaBox();
                if(first)
                {
                    V2d_UniteBox(bounds.arvec2, segBounds.arvec2);
                }
                else
                {
                    V2d_CopyBox(bounds.arvec2, segBounds.arvec2);
                    first = true;
                }
            }

            if(*initialized)
            {
                V2d_AddToBox(retBounds.arvec2, bounds.min);
            }
            else
            {
                V2d_InitBox(retBounds.arvec2, bounds.min);
                *initialized = true;
            }
            V2d_AddToBox(retBounds.arvec2, bounds.max);
        }
    }
};

SuperBlockmap::SuperBlockmap(AABox const &bounds)
    : d(new Instance(bounds))
{}

SuperBlockmap::operator SuperBlockmap::Block &()
{
    return static_cast<Block &>(d->rootNode());
}

AABoxd SuperBlockmap::findSegmentBounds()
{
    bool initialized = false;
    AABoxd bounds;

    // Iterative pre-order traversal of SuperBlock.
    Block *cur = &d->rootNode();
    Block *prev = 0;
    while(cur)
    {
        while(cur)
        {
            d->findBlockSegmentBoundsWorker(*cur, bounds, &initialized);

            if(prev == cur->parent())
            {
                // Descending - right first, then left.
                prev = cur;
                if(cur->right()) cur = cur->right();
                else             cur = cur->left();
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

    if(!initialized)
    {
        bounds.clear();
    }

    return bounds;
}
