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

#include "bspbuilder/hedges.hh"
#include "bspbuilder/superblockmap.hh"

using namespace de;

void SuperBlock::clear()
{
    _hedges.clear();
    KdTreeNode_SetUserData(_tree, NULL);
}

const AABox& SuperBlock::bounds() const
{
    return *KdTreeNode_Bounds(_tree);
}

bool SuperBlock::hasChild(ChildId childId) const
{
    assertValidChildId(childId);
    return NULL != KdTreeNode_Child(_tree, childId==LEFT);
}

SuperBlock& SuperBlock::child(ChildId childId)
{
    assertValidChildId(childId);
    KdTreeNode* subtree = KdTreeNode_Child(_tree, childId==LEFT);
    if(!subtree) Con_Error("SuperBlock::child: Has no %s subblock.", childId==LEFT? "left" : "right");
    return *static_cast<SuperBlock*>(KdTreeNode_UserData(subtree));
}

SuperBlock* SuperBlock::addChild(ChildId childId, bool splitVertical)
{
    assertValidChildId(childId);
    SuperBlock* child = new SuperBlock(_bmap);
    child->_tree = KdTreeNode_AddChild(_tree, 0.5, int(splitVertical), childId==LEFT, child);
    return child;
}

uint SuperBlock::hedgeCount(bool addReal, bool addMini) const
{
    uint total = 0;
    if(addReal) total += _realNum;
    if(addMini) total += _miniNum;
    return total;
}

void SuperBlock::incrementHEdgeCount(bsp_hedge_t* hedge)
{
    if(!hedge) return;
    if(hedge->info.lineDef) _realNum++;
    else                    _miniNum++;
}

void SuperBlock::linkHEdge(bsp_hedge_t* hedge)
{
    if(!hedge) return;
    _hedges.push_front(hedge);
    // Associate ourself.
    hedge->block = this;
}

static void initAABoxFromHEdgeVertexes(AABoxf* aaBox, const bsp_hedge_t* hedge)
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

    for(HEdges::iterator it = _hedges.begin(); it != _hedges.end(); ++it)
    {
        bsp_hedge_t* hedge = *it;
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

SuperBlock* SuperBlock::hedgePush(bsp_hedge_t* hedge)
{
    if(!hedge) return this;

    SuperBlock* sb = this;
    for(;;)
    {
        // Update half-edge counts.
        sb->incrementHEdgeCount(hedge);

        if(sb->blockmap().isLeaf(*sb))
        {
            // No further subdivision possible.
            sb->linkHEdge(hedge);
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
            sb->linkHEdge(hedge);
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

bsp_hedge_t* SuperBlock::hedgePop()
{
    if(_hedges.empty()) return NULL;

    bsp_hedge_t* hedge = _hedges.front();
    _hedges.pop_front();

    // Update half-edge counts.
    if(hedge->info.lineDef) _realNum--;
    else                    _miniNum--;

    // Disassociate ourself.
    hedge->block = NULL;
    return hedge;
}

int SuperBlock::traverse(int (C_DECL *callback)(SuperBlock*, void*), void* parameters)
{
    if(!callback) return false; // Continue iteration.

    int result = callback(this, parameters);
    if(result) return result;

    if(_tree)
    {
        // Recursively handle subtrees.
        for(uint num = 0; num < 2; ++num)
        {
            KdTreeNode* node = KdTreeNode_Child(_tree, num);
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
    /**
     * The KdTree of SuperBlocks.
     *
     * Subblocks:
     * RIGHT - has the lower coordinates.
     * LEFT  - has the higher coordinates.
     * Division of a block always occurs horizontally:
     *     e.g. 512x512 -> 256x512 -> 256x256.
     */
    KdTree* kdTree;

    Instance(SuperBlockmap& bmap, const AABox& bounds)
    {
        kdTree = KdTree_New(&bounds);
        SuperBlock* block = new SuperBlock(bmap);
        block->_tree = KdTreeNode_SetUserData(KdTree_Root(kdTree), block);
    }

    ~Instance()
    {
        KdTree_Delete(kdTree);
    }

    void clearBlockWorker(SuperBlock& block)
    {
        if(block._tree)
        {
            // Recursively handle sub-blocks.
            KdTreeNode* child;
            for(uint num = 0; num < 2; ++num)
            {
                child = KdTreeNode_Child(block._tree, num);
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

SuperBlock* SuperBlockmap::root()
{
    return static_cast<SuperBlock*>(KdTreeNode_UserData(KdTree_Root(d->kdTree)));
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
    SuperBlock* block = root();
    if(block) d->clearBlockWorker(*block);
}

void SuperBlockmap::findHEdgeBounds(AABoxf& aaBox)
{
    SuperBlock* block = root();
    if(block)
    {
        findhedgelistboundsparams_t parm;
        parm.initialized = false;
        block->traverse(findHEdgeBoundsWorker, (void*)&parm);
        if(parm.initialized)
        {
            V2f_CopyBox(aaBox.arvec2, parm.bounds.arvec2);
            return;
        }
    }

    // Clear.
    V2f_Set(aaBox.min, DDMAXFLOAT, DDMAXFLOAT);
    V2f_Set(aaBox.max, DDMINFLOAT, DDMINFLOAT);
}
