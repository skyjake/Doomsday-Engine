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

KdTreeNode::KdTreeNode() : _tree(0) {}

KdTreeNode::~KdTreeNode()
{
    KdTreeNode_Delete(_tree);
}

KdTreeNode *KdTreeNode::parent() const
{
    kdtreenode_s *pNode = KdTreeNode_Parent(_tree);
    if(!pNode) return 0;
    return static_cast<KdTreeNode *>(KdTreeNode_UserData(pNode));
}

KdTreeNode *KdTreeNode::child(ChildId childId) const
{
    kdtreenode_s *subtree = KdTreeNode_Child(_tree, childId == Left);
    if(!subtree) return 0;
    return static_cast<KdTreeNode *>(KdTreeNode_UserData(subtree));
}

} // namespace de

/// ----------------------------------------------------------------------------

#include "world/bsp/linesegment.h"

using namespace de;
using namespace de::bsp;

DENG2_PIMPL_NOREF(SuperBlockmap::Block)
{
    SuperBlockmap &owner;
    AABox bounds;

    Segments segments; ///< Line segments contained by the block.
    int mapNum;        ///< Running total of map-line segments at this node.
    int partNum;       ///< Running total of partition-line segments at this node.

    Instance(SuperBlockmap &owner, AABox const &bounds)
        : owner  (owner)
        , bounds (bounds)
        , mapNum (0)
        , partNum(0)
    {}

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

SuperBlockmap::Block::Block(SuperBlockmap &owner, AABox const &bounds)
    : KdTreeNode()
    , d(new Instance(owner, bounds))
{}

SuperBlockmap::Block::~Block()
{
    clear();
}

AABox const &SuperBlockmap::Block::bounds() const
{
    return d->bounds;
}

SuperBlockmap::Block &SuperBlockmap::Block::clear()
{
    if(_tree)
    {
        // Recursively handle sub-blocks.
        for(uint num = 0; num < 2; ++num)
        {
            if(kdtreenode_s *child = KdTreeNode_Child(_tree, num))
            {
                delete KdTreeNode_UserData(child);
                KdTreeNode_SetUserData(child, 0);
            }
        }
    }
    return *this;
}

SuperBlockmap::Block::Segments SuperBlockmap::Block::collateAllSegments()
{
    Segments allSegs;

#ifdef DENG2_QT_4_7_OR_NEWER
    allSegs.reserve(totalSegmentCount());
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
                allSegs << seg;
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

    return allSegs;
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

        // Determine whether further subdivision is necessary.
        Vector2i dimensions(Vector2i(sb->bounds().max) - Vector2i(sb->bounds().min));
        if(dimensions.x <= 256 && dimensions.y <= 256)
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
            //child->_tree = KdTreeNode_AddChild(sb->_tree, 0.5, int(splitVertical), p1 == Left, child);
            // Note that the tree retains a pointer to the block; no leak.

            AABox const *aaBox = &static_cast<Block *>(KdTreeNode_UserData(sb->_tree))->bounds();
            AABox sub;

            double distance = 0.5;
            int left = (p1 == Left);
            if(!int(splitVertical))
            {
                int division = (int) (aaBox->minX + 0.5 + distance * (aaBox->maxX - aaBox->minX));

                sub.minX = (left? division : aaBox->minX);
                sub.minY = aaBox->minY;

                sub.maxX = (left? aaBox->maxX : division);
                sub.maxY = aaBox->maxY;
            }
            else
            {
                int division = (int) (aaBox->minY + 0.5 + distance * (aaBox->maxY - aaBox->minY));

                sub.minX = aaBox->minX;
                sub.minY = (left? division : aaBox->minY);

                sub.maxX = aaBox->maxX;
                sub.maxY = (left? aaBox->maxY : division);
            }

            ::KdTreeNode *subtree = KdTreeNode_Child(sb->_tree, left);
            if(!subtree)
            {
                subtree = KdTreeNode_SetChild(sb->_tree, left) = KdTreeNode_New(KdTreeNode_KdTree(sb->_tree), &sub);
                KdTreeNode_SetParent(subtree, sb->_tree);
            }

            Block *child = new Block(d->owner, sub);
            child->_tree = subtree;
            KdTreeNode_SetUserData(subtree, child);
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

DENG2_PIMPL(SuperBlockmap)//, public de::KdTree
{
    /// Tree of KdTreeNode.
    kdtree_s *_nodes;

    /// @param bounds  Bounding for the logical coordinate space.
    Instance(Public *i, AABox const &bounds)
        : Base(i)
        , _nodes(KdTree_New(&bounds))
    {
        // Attach the root node.
        SuperBlock *block = new SuperBlock(*thisPublic, bounds);
        block->_tree = KdTreeNode_SetUserData(KdTree_Root(_nodes), block);
    }

    ~Instance()
    {
        rootBlock().clear();
        KdTree_Delete(_nodes);
    }

    SuperBlock &rootBlock()
    {
        return *static_cast<SuperBlock *>(KdTreeNode_UserData(KdTree_Root(_nodes)));
    }

    void clear()
    {
        rootBlock().clear();
    }

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
    : d(new Instance(this, bounds))
{}

SuperBlockmap::operator SuperBlockmap::Block &()
{
    return static_cast<Block &>(d->rootBlock());
}

AABoxd SuperBlockmap::findSegmentBounds()
{
    bool initialized = false;
    AABoxd bounds;

    // Iterative pre-order traversal.
    Block *cur = &d->rootBlock();
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
