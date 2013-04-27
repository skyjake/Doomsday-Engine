/** @file superblockmap.cpp BSP Builder Superblock. 
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

#include "map/bsp/linesegment.h"

#include "map/bsp/superblockmap.h"

using namespace de;
using namespace de::bsp;

struct SuperBlock::Instance
{
    /// SuperBlockmap that owns this SuperBlock.
    SuperBlockmap &bmap;

    /// KdTree node in the owning SuperBlockmap.
    KdTreeNode *tree;

    /// LineSegments completely contained by this block.
    SuperBlock::LineSegments lineSegments;

    /// Number of "real" and "mini" line segments contained by this block
    /// (including all sub-blocks below it).
    int realNum;
    int miniNum;

    Instance(SuperBlockmap &blockmap)
      : bmap(blockmap), tree(0), realNum(0), miniNum(0)
    {}

    ~Instance()
    {
        KdTreeNode_Delete(tree);
    }

    inline void linkLineSegment(LineSegment &lineSeg)
    {
        lineSegments.prepend(&lineSeg);
    }

    inline void incrementLineSegmentCount(LineSegment const &lineSeg)
    {
        if(lineSeg.hasMapLineSide()) realNum++;
        else                      miniNum++;
    }

    inline void decrementLineSegmentCount(LineSegment const &lineSeg)
    {
        if(lineSeg.hasMapLineSide()) realNum--;
        else                      miniNum--;
    }
};

SuperBlock::SuperBlock(SuperBlockmap &blockmap)
    : d(new Instance(blockmap))
{}

SuperBlock::SuperBlock(SuperBlock& parent, ChildId childId, bool splitVertical)
    : d(new Instance(parent.blockmap()))
{
    d->tree = KdTreeNode_AddChild(parent.d->tree, 0.5, int(splitVertical),
                                  childId==LEFT, this);
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

SuperBlock *SuperBlock::parentPtr() const
{
    KdTreeNode *pNode = KdTreeNode_Parent(d->tree);
    if(!pNode) return 0;
    return static_cast<SuperBlock *>(KdTreeNode_UserData(pNode));
}

bool SuperBlock::hasParent() const
{
    return parentPtr() != 0;
}

SuperBlock *SuperBlock::childPtr(ChildId childId) const
{
    assertValidChildId(childId);
    KdTreeNode *subtree = KdTreeNode_Child(d->tree, childId==LEFT);
    if(!subtree) return 0;
    return static_cast<SuperBlock *>(KdTreeNode_UserData(subtree));
}

bool SuperBlock::hasChild(ChildId childId) const
{
    assertValidChildId(childId);
    return 0 != childPtr(childId);
}

SuperBlock *SuperBlock::addChild(ChildId childId, bool splitVertical)
{
    assertValidChildId(childId);
    return new SuperBlock(*this, childId, splitVertical);
}

SuperBlock::LineSegments const &SuperBlock::lineSegments() const
{
    return d->lineSegments;
}

uint SuperBlock::lineSegmentCount(bool addReal, bool addMini) const
{
    uint total = 0;
    if(addReal) total += d->realNum;
    if(addMini) total += d->miniNum;
    return total;
}

static void initAABoxFromLineSegmentVertexes(AABoxd &aaBox, LineSegment const &lineSeg)
{
    Vector2d min = lineSeg.fromOrigin().min(lineSeg.toOrigin());
    Vector2d max = lineSeg.fromOrigin().max(lineSeg.toOrigin());
    V2d_Set(aaBox.min, min.x, min.y);
    V2d_Set(aaBox.max, max.x, max.y);
}

/// @todo Optimize: Cache this result.
void SuperBlock::findLineSegmentBounds(AABoxd &bounds)
{
    bool initialized = false;
    AABoxd lineSegBounds;

    foreach(LineSegment *lineSeg, d->lineSegments)
    {
        initAABoxFromLineSegmentVertexes(lineSegBounds, *lineSeg);
        if(initialized)
        {
            V2d_UniteBox(bounds.arvec2, lineSegBounds.arvec2);
        }
        else
        {
            V2d_CopyBox(bounds.arvec2, lineSegBounds.arvec2);
            initialized = true;
        }
    }
}

SuperBlock &SuperBlock::push(LineSegment &lineSeg)
{
    SuperBlock *sb = this;
    forever
    {
        DENG2_ASSERT(sb);

        // Update line segment counts.
        sb->d->incrementLineSegmentCount(lineSeg);

        if(sb->isLeaf())
        {
            // No further subdivision possible.
            sb->d->linkLineSegment(lineSeg);
            break;
        }

        ChildId p1, p2;
        bool splitVertical;
        if(sb->bounds().maxX - sb->bounds().minX >=
           sb->bounds().maxY - sb->bounds().minY)
        {
            // Wider than tall.
            int midPoint = (sb->bounds().minX + sb->bounds().maxX) / 2;
            p1 = lineSeg.fromOrigin().x >= midPoint? LEFT : RIGHT;
            p2 =   lineSeg.toOrigin().x >= midPoint? LEFT : RIGHT;
            splitVertical = false;
        }
        else
        {
            // Taller than wide.
            int midPoint = (sb->bounds().minY + sb->bounds().maxY) / 2;
            p1 = lineSeg.fromOrigin().y >= midPoint? LEFT : RIGHT;
            p2 =   lineSeg.toOrigin().y >= midPoint? LEFT : RIGHT;
            splitVertical = true;
        }

        if(p1 != p2)
        {
            // Line crosses midpoint; link it in and return.
            sb->d->linkLineSegment(lineSeg);
            break;
        }

        // The lineSeg lies in one half of this block. Create the sub-block
        // if it doesn't already exist, and loop back to add the lineSeg.
        if(!sb->hasChild(p1))
        {
            sb->addChild(p1, (int)splitVertical);
        }

        sb = sb->childPtr(p1);
    }
    return *sb;
}

LineSegment *SuperBlock::pop()
{
    if(d->lineSegments.isEmpty())
        return 0;

    LineSegment *lineSeg = d->lineSegments.takeFirst();

    // Update line segment counts.
    d->decrementLineSegmentCount(*lineSeg);

    return lineSeg;
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

static void findLineSegmentBoundsWorker(SuperBlock &block, AABoxd &bounds, bool *initialized)
{
    DENG2_ASSERT(initialized);
    if(block.lineSegmentCount(true, true))
    {
        AABoxd lineSegBounds;
        block.findLineSegmentBounds(lineSegBounds);
        if(*initialized)
        {
            V2d_AddToBox(bounds.arvec2, lineSegBounds.min);
        }
        else
        {
            V2d_InitBox(bounds.arvec2, lineSegBounds.min);
            *initialized = true;
        }
        V2d_AddToBox(bounds.arvec2, lineSegBounds.max);
    }
}

AABoxd SuperBlockmap::findLineSegmentBounds()
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
            findLineSegmentBoundsWorker(*cur, bounds, &initialized);

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

    if(!initialized)
    {
        // Clear.
        V2d_Set(bounds.min, DDMAXFLOAT, DDMAXFLOAT);
        V2d_Set(bounds.max, DDMINFLOAT, DDMINFLOAT);
    }

    return bounds;
}
