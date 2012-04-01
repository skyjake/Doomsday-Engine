/**
 * @file superblockmap.cpp
 * BSP Builder Superblock. @ingroup map
 *
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "de_console.h"
#include "p_mapdata.h"

#include "kdtree.h"
#include "bspbuilder/hedges.hh"
#include "bspbuilder/superblockmap.hh"

using namespace de;

struct SuperBlock::Instance
{
    /// SuperBlockmap that owns this SuperBlock.
    SuperBlockmap& bmap;

    /// KdTree node in the owning SuperBlockmap.
    KdTreeNode* tree;

    /// Half-edges completely contained by this block.
    SuperBlock::HEdges hedges;

    /// Number of real half-edges and minihedges contained by this block
    /// (including all sub-blocks below it).
    int realNum;
    int miniNum;

    Instance(SuperBlockmap& blockmap) :
        bmap(blockmap), tree(0), hedges(0), realNum(0), miniNum(0)
    {}

    ~Instance()
    {
        KdTreeNode_SetUserData(tree, NULL);
    }

    void inline linkHEdge(HEdge* hedge)
    {
        if(!hedge) return;
        hedges.push_front(hedge);
    }

    void incrementHEdgeCount(HEdge* hedge)
    {
        if(!hedge) return;
        if(hedge->bspBuildInfo->lineDef) realNum++;
        else                         miniNum++;
    }

    void decrementHEdgeCount(HEdge* hedge)
    {
        if(!hedge) return;
        if(hedge->bspBuildInfo->lineDef) realNum--;
        else                         miniNum--;
    }
};

SuperBlock::SuperBlock(SuperBlockmap& blockmap)
{
    d = new Instance(blockmap);
}

SuperBlock::SuperBlock(SuperBlock& parent, ChildId childId, bool splitVertical)
{
    d = new Instance(parent.blockmap());
    d->tree = KdTreeNode_AddChild(parent.d->tree, 0.5, int(splitVertical), childId==LEFT, this);
}

SuperBlock::~SuperBlock()
{
    delete d;
}

SuperBlockmap& SuperBlock::blockmap() const
{
    return d->bmap;
}

const AABox& SuperBlock::bounds() const
{
    return *KdTreeNode_Bounds(d->tree);
}

bool SuperBlock::hasChild(ChildId childId) const
{
    assertValidChildId(childId);
    return NULL != KdTreeNode_Child(d->tree, childId==LEFT);
}

SuperBlock& SuperBlock::child(ChildId childId)
{
    assertValidChildId(childId);
    KdTreeNode* subtree = KdTreeNode_Child(d->tree, childId==LEFT);
    if(!subtree) Con_Error("SuperBlock::child: Has no %s subblock.", childId==LEFT? "left" : "right");
    return *static_cast<SuperBlock*>(KdTreeNode_UserData(subtree));
}

SuperBlock* SuperBlock::addChild(ChildId childId, bool splitVertical)
{
    assertValidChildId(childId);
    SuperBlock* child = new SuperBlock(*this, childId, splitVertical);
    return child;
}

SuperBlock::HEdges::const_iterator SuperBlock::hedgesBegin() const
{
    return d->hedges.begin();
}

SuperBlock::HEdges::const_iterator SuperBlock::hedgesEnd() const
{
    return d->hedges.end();
}

uint SuperBlock::hedgeCount(bool addReal, bool addMini) const
{
    uint total = 0;
    if(addReal) total += d->realNum;
    if(addMini) total += d->miniNum;
    return total;
}

static void initAABoxFromHEdgeVertexes(AABoxf* aaBox, const HEdge* hedge)
{
    assert(aaBox && hedge);
    const double* from = hedge->v[0]->buildData.pos;
    const double* to   = hedge->v[1]->buildData.pos;
    aaBox->minX = (float)MIN_OF(from[0], to[0]);
    aaBox->minY = (float)MIN_OF(from[1], to[1]);
    aaBox->maxX = (float)MAX_OF(from[0], to[0]);
    aaBox->maxY = (float)MAX_OF(from[1], to[1]);
}

/// @todo Optimize: Cache this result.
void SuperBlock::findHEdgeBounds(AABoxf& bounds)
{
    bool initialized = false;
    AABoxf hedgeAABox;

    for(HEdges::iterator it = d->hedges.begin(); it != d->hedges.end(); ++it)
    {
        HEdge* hedge = *it;
        initAABoxFromHEdgeVertexes(&hedgeAABox, hedge);
        if(initialized)
        {
            V2f_AddToBox(bounds.arvec2, hedgeAABox.min);
        }
        else
        {
            V2f_InitBox(bounds.arvec2, hedgeAABox.min);
            initialized = true;
        }
        V2f_AddToBox(bounds.arvec2, hedgeAABox.max);
    }
}

SuperBlock* SuperBlock::hedgePush(HEdge* hedge)
{
    if(!hedge) return this;

    SuperBlock* sb = this;
    for(;;)
    {
        // Update half-edge counts.
        sb->d->incrementHEdgeCount(hedge);

        if(sb->blockmap().isLeaf(*sb))
        {
            // No further subdivision possible.
            sb->d->linkHEdge(hedge);
            // Associate ourself.
            hedge->bspBuildInfo->block = sb;
            break;
        }

        ChildId p1, p2;
        bool splitVertical;
        if(sb->bounds().maxX - sb->bounds().minX >=
           sb->bounds().maxY - sb->bounds().minY)
        {
            // Wider than tall.
            int midPoint = (sb->bounds().minX + sb->bounds().maxX) / 2;
            p1 = hedge->v[0]->buildData.pos[0] >= midPoint? LEFT : RIGHT;
            p2 = hedge->v[1]->buildData.pos[0] >= midPoint? LEFT : RIGHT;
            splitVertical = false;
        }
        else
        {
            // Taller than wide.
            int midPoint = (sb->bounds().minY + sb->bounds().maxY) / 2;
            p1 = hedge->v[0]->buildData.pos[1] >= midPoint? LEFT : RIGHT;
            p2 = hedge->v[1]->buildData.pos[1] >= midPoint? LEFT : RIGHT;
            splitVertical = true;
        }

        if(p1 != p2)
        {
            // Line crosses midpoint; link it in and return.
            sb->d->linkHEdge(hedge);
            // Associate ourself.
            hedge->bspBuildInfo->block = sb;
            break;
        }

        // The hedge lies in one half of this block. Create the sub-block
        // if it doesn't already exist, and loop back to add the hedge.
        if(!sb->hasChild(p1))
        {
            sb->addChild(p1, (int)splitVertical);
        }

        sb = &sb->child(p1);
    }

    return this;
}

HEdge* SuperBlock::hedgePop()
{
    if(d->hedges.empty()) return NULL;

    HEdge* hedge = d->hedges.front();
    d->hedges.pop_front();

    // Update half-edge counts.
    d->decrementHEdgeCount(hedge);

    // Disassociate ourself.
    hedge->bspBuildInfo->block = NULL;
    return hedge;
}

int SuperBlock::traverse(int (C_DECL *callback)(SuperBlock*, void*), void* parameters)
{
    if(!callback) return false; // Continue iteration.

    int result = callback(this, parameters);
    if(result) return result;

    if(d->tree)
    {
        // Recursively handle subtrees.
        for(uint num = 0; num < 2; ++num)
        {
            KdTreeNode* node = KdTreeNode_Child(d->tree, num);
            if(!node) continue;

            SuperBlock* child = static_cast<SuperBlock*>(KdTreeNode_UserData(node));
            if(!child) continue;

            result = child->traverse(callback, parameters);
            if(result) return result;
        }
    }

    return false; // Continue iteration.
}

struct SuperBlockmap::Instance
{
    /// The KdTree of SuperBlocks.
    KdTree* kdTree;

    Instance(SuperBlockmap& bmap, const AABox& bounds)
    {
        kdTree = KdTree_New(&bounds);
        // Attach the root node.
        SuperBlock* block = new SuperBlock(bmap);
        block->d->tree = KdTreeNode_SetUserData(KdTree_Root(kdTree), block);
    }

    ~Instance()
    {
        KdTree_Delete(kdTree);
    }

    void clearBlockWorker(SuperBlock& block)
    {
        if(block.d->tree)
        {
            // Recursively handle sub-blocks.
            KdTreeNode* child;
            for(uint num = 0; num < 2; ++num)
            {
                child = KdTreeNode_Child(block.d->tree, num);
                if(!child) continue;

                SuperBlock* blockPtr = static_cast<SuperBlock*>(KdTreeNode_UserData(child));
                if(blockPtr) clearBlockWorker(*blockPtr);
            }
        }
        delete &block;
    }
};

SuperBlockmap::SuperBlockmap(const AABox& bounds)
{
    d = new Instance(*this, bounds);
}

SuperBlockmap::~SuperBlockmap()
{
    clear();
    delete d;
}

SuperBlock& SuperBlockmap::root()
{
    return *static_cast<SuperBlock*>(KdTreeNode_UserData(KdTree_Root(d->kdTree)));
}

bool SuperBlockmap::isLeaf(const SuperBlock& block) const
{
    return (block.bounds().maxX - block.bounds().minX <= 256 &&
            block.bounds().maxY - block.bounds().minY <= 256);
}

typedef struct {
    AABoxf bounds;
    boolean initialized;
} findhedgelistboundsparams_t;

static int findHEdgeBoundsWorker(SuperBlock* block, void* parameters)
{
    findhedgelistboundsparams_t* p = (findhedgelistboundsparams_t*)parameters;
    if(block->hedgeCount(true, true))
    {
        AABoxf blockHEdgeAABox;
        block->findHEdgeBounds(blockHEdgeAABox);
        if(p->initialized)
        {
            V2f_AddToBox(p->bounds.arvec2, blockHEdgeAABox.min);
        }
        else
        {
            V2f_InitBox(p->bounds.arvec2, blockHEdgeAABox.min);
            p->initialized = true;
        }
        V2f_AddToBox(p->bounds.arvec2, blockHEdgeAABox.max);
    }
    return false; // Continue iteration.
}

void SuperBlockmap::clear()
{
    d->clearBlockWorker(root());
}

void SuperBlockmap::findHEdgeBounds(AABoxf& aaBox)
{
    findhedgelistboundsparams_t parm;
    parm.initialized = false;
    root().traverse(findHEdgeBoundsWorker, (void*)&parm);
    if(parm.initialized)
    {
        V2f_CopyBox(aaBox.arvec2, parm.bounds.arvec2);
        return;
    }

    // Clear.
    V2f_Set(aaBox.min, DDMAXFLOAT, DDMAXFLOAT);
    V2f_Set(aaBox.max, DDMINFLOAT, DDMINFLOAT);
}
