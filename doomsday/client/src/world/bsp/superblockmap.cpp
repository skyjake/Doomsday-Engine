/** @file world/bsp/superblockmap.cpp BSP Builder Super Blockmap.
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

#include "world/bsp/linesegment.h"

#include "world/bsp/superblockmap.h"

using namespace de;
using namespace de::bsp;

struct SuperBlock::Instance
{
    /// Owning SuperBlockmap.
    SuperBlockmap &bmap;

    /// KdTree node in the owning SuperBlockmap.
    KdTreeNode *tree;

    /// Line segments (completely?) contained by the block.
    SuperBlock::Segments segments;

    /// Running totals of the number of line segments in this and all child
    /// blocks.
    int mapNum;
    int partNum;

    Instance(SuperBlockmap &blockmap)
      : bmap(blockmap), tree(0), mapNum(0), partNum(0)
    {}

    ~Instance()
    {
        KdTreeNode_Delete(tree);
    }

    inline void linkSegment(LineSegment::Side &seg)
    {
        segments.prepend(&seg);
    }

    inline void incrementSegmentCount(LineSegment::Side const &seg)
    {
        if(seg.hasMapSide()) mapNum++;
        else                 partNum++;
    }

    inline void decrementSegmentCount(LineSegment::Side const &seg)
    {
        if(seg.hasMapSide()) mapNum--;
        else                 partNum--;
    }
};

SuperBlock::SuperBlock(SuperBlockmap &blockmap)
    : d(new Instance(blockmap))
{}

SuperBlock::SuperBlock(SuperBlock& parent, ChildId childId, bool splitVertical)
    : d(new Instance(parent.blockmap()))
{
    d->tree = KdTreeNode_AddChild(parent.d->tree, 0.5, int(splitVertical),
                                  childId==Left, this);
}

SuperBlock::~SuperBlock()
{
    clear();
    delete d;
}

SuperBlock &SuperBlock::clear()
{
    if(d->tree)
    {
        // Recursively handle sub-blocks.
        KdTreeNode *child;
        for(uint num = 0; num < 2; ++num)
        {
            child = KdTreeNode_Child(d->tree, num);
            if(!child) continue;

            SuperBlock *blockPtr = static_cast<SuperBlock *>(KdTreeNode_UserData(child));
            if(blockPtr) delete blockPtr;
        }
    }
    return *this;
}

SuperBlockmap &SuperBlock::blockmap() const
{
    return d->bmap;
}

AABox const &SuperBlock::bounds() const
{
    return *KdTreeNode_Bounds(d->tree);
}

SuperBlock *SuperBlock::parent() const
{
    KdTreeNode *pNode = KdTreeNode_Parent(d->tree);
    if(!pNode) return 0;
    return static_cast<SuperBlock *>(KdTreeNode_UserData(pNode));
}

SuperBlock *SuperBlock::child(ChildId childId) const
{
    KdTreeNode *subtree = KdTreeNode_Child(d->tree, childId == Left);
    if(!subtree) return 0;
    return static_cast<SuperBlock *>(KdTreeNode_UserData(subtree));
}

SuperBlock *SuperBlock::addChild(ChildId childId, bool splitVertical)
{
    return new SuperBlock(*this, childId, splitVertical);
}

SuperBlock::Segments SuperBlock::collateAllSegments()
{
    Segments segments;

#ifdef DENG2_QT_4_7_OR_NEWER
    segments.reserve(totalSegmentCount());
#endif

    // Iterative pre-order traversal of SuperBlock.
    SuperBlock *cur = this;
    SuperBlock *prev = 0;
    while(cur)
    {
        while(cur)
        {
            LineSegment::Side *seg;
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

SuperBlock::Segments const &SuperBlock::segments() const
{
    return d->segments;
}

int SuperBlock::segmentCount(bool addMap, bool addPart) const
{
    int total = 0;
    if(addMap)  total += d->mapNum;
    if(addPart) total += d->partNum;
    return total;
}

/// @todo Optimize: Cache this result.
AABoxd SuperBlock::findSegmentBounds()
{
    AABoxd bounds;
    bool initialized = false;

    foreach(LineSegment::Side *seg, d->segments)
    {
        AABoxd segBounds = seg->aaBox();
        if(initialized)
        {
            V2d_UniteBox(bounds.arvec2, segBounds.arvec2);
        }
        else
        {
            V2d_CopyBox(bounds.arvec2, segBounds.arvec2);
            initialized = true;
        }
    }

    return bounds;
}

SuperBlock &SuperBlock::push(LineSegment::Side &seg)
{
    SuperBlock *sb = this;
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
            sb->addChild(p1, splitVertical);
        }

        sb = sb->child(p1);
    }
    return *sb;
}

LineSegment::Side *SuperBlock::pop()
{
    if(d->segments.isEmpty())
        return 0;

    LineSegment::Side *seg = d->segments.takeFirst();

    // Update line segment counts.
    d->decrementSegmentCount(*seg);

    return seg;
}

int SuperBlock::traverse(int (*callback)(SuperBlock *, void *), void *parameters)
{
    if(!callback) return false; // Continue iteration.

    int result = callback(this, parameters);
    if(result) return result;

    if(d->tree)
    {
        // Recursively handle subtrees.
        for(uint num = 0; num < 2; ++num)
        {
            KdTreeNode *node = KdTreeNode_Child(d->tree, num);
            if(!node) continue;

            SuperBlock *child = static_cast<SuperBlock *>(KdTreeNode_UserData(node));
            if(!child) continue;

            result = child->traverse(callback, parameters);
            if(result) return result;
        }
    }

    return false; // Continue iteration.
}

DENG2_PIMPL(SuperBlockmap)
{
    /// The KdTree of SuperBlocks.
    KdTree *kdTree;

    Instance(Public *i, AABox const &bounds)
        : Base(i), kdTree(KdTree_New(&bounds))
    {
        // Attach the root node.
        SuperBlock *block = new SuperBlock(self);
        block->d->tree = KdTreeNode_SetUserData(KdTree_Root(kdTree), block);
    }

    ~Instance()
    {
        self.clear();
        KdTree_Delete(kdTree);
    }
};

SuperBlockmap::SuperBlockmap(AABox const &bounds)
    : d(new Instance(this, bounds))
{}

SuperBlock &SuperBlockmap::root()
{
    return *static_cast<SuperBlock *>(KdTreeNode_UserData(KdTree_Root(d->kdTree)));
}

void SuperBlockmap::clear()
{
    root().clear();
}

static void findSegmentBoundsWorker(SuperBlock &block, AABoxd &bounds,
                                    bool *initialized)
{
    DENG2_ASSERT(initialized);
    if(block.segmentCount(true, true))
    {
        AABoxd blockSegmentBounds = block.findSegmentBounds();
        if(*initialized)
        {
            V2d_AddToBox(bounds.arvec2, blockSegmentBounds.min);
        }
        else
        {
            V2d_InitBox(bounds.arvec2, blockSegmentBounds.min);
            *initialized = true;
        }
        V2d_AddToBox(bounds.arvec2, blockSegmentBounds.max);
    }
}

AABoxd SuperBlockmap::findSegmentBounds()
{
    bool initialized = false;
    AABoxd bounds;

    // Iterative pre-order traversal of SuperBlock.
    SuperBlock *cur = &root();
    SuperBlock *prev = 0;
    while(cur)
    {
        while(cur)
        {
            findSegmentBoundsWorker(*cur, bounds, &initialized);

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
