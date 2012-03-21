/**
 * @file superblockmap.c
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
#include "de_bsp.h"

#include "superblockmap.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <assert.h>
#include <list>

static int deleteSuperBlockIterator(SuperBlock* block, void* /*parameters*/);

typedef std::list<bsp_hedge_t*> HEdges;

struct superblock_s {
    /// SuperBlockmap which owns this block.
    struct superblockmap_s* bmap;

    /// KdTree node in the owning SuperBlockmap.
    KdTreeNode* tree;

    /// Number of real half-edges and minihedges contained by this block
    /// (including all sub-blocks below it).
    int realNum;
    int miniNum;

    /// Half-edges completely contained by this block.
    HEdges hEdges;

    superblock_s(SuperBlockmap* blockmap) :
        bmap(blockmap), realNum(0), miniNum(0), hEdges(0)
    {}

    ~superblock_s()
    {
        hEdges.clear();
        KdTreeNode_SetUserData(tree, NULL);
    }

    SuperBlockmap* blockmap() { return bmap; }

    const AABox* bounds() { return KdTreeNode_Bounds(tree); }

    bool inline isLeaf()
    {
        const AABox* aaBox = bounds();
        return (aaBox->maxX - aaBox->minX <= 256 &&
                aaBox->maxY - aaBox->minY <= 256);
    }

    SuperBlock* child(int left)
    {
        KdTreeNode* subtree = KdTreeNode_Child(tree, left);
        if(!subtree) return NULL;
        return (SuperBlock*)KdTreeNode_UserData(subtree);
    }

private:
    void initAABoxFromHEdgeVertexes(AABoxf* aaBox, const bsp_hedge_t* hEdge)
    {
        assert(aaBox && hEdge);
        const double* from = hEdge->HE_v1->buildData.pos;
        const double* to   = hEdge->HE_v2->buildData.pos;
        aaBox->minX = (float)MIN_OF(from[0], to[0]);
        aaBox->minY = (float)MIN_OF(from[1], to[1]);
        aaBox->maxX = (float)MAX_OF(from[0], to[0]);
        aaBox->maxY = (float)MAX_OF(from[1], to[1]);
    }

public:
    /// @todo Optimize: Cache this result.
    void findHEdgeBounds(AABoxf* bounds)
    {
        if(!bounds) return;

        bool initialized = false;
        AABoxf hEdgeAABox;

        for(HEdges::iterator it = hEdges.begin(); it != hEdges.end(); ++it)
        {
            bsp_hedge_t* hEdge = *it;
            initAABoxFromHEdgeVertexes(&hEdgeAABox, hEdge);
            if(initialized)
            {
                V2_AddToBox(bounds->arvec2, hEdgeAABox.min);
            }
            else
            {
                V2_InitBox(bounds->arvec2, hEdgeAABox.min);
                initialized = true;
            }
            V2_AddToBox(bounds->arvec2, hEdgeAABox.max);
        }
    }

    uint hEdgeCount(bool addReal, bool addMini)
    {
        uint total = 0;
        if(addReal) total += realNum;
        if(addMini) total += miniNum;
        return total;
    }

    void inline linkHEdge(bsp_hedge_t* hEdge)
    {
        if(!hEdge) return;

        hEdges.push_front(hEdge);
        // Associate ourself.
        hEdge->block = this;
    }

    bsp_hedge_t* popHEdge()
    {
        if(hEdges.empty()) return NULL;

        bsp_hedge_t* hEdge = hEdges.front();
        hEdges.pop_front();

        // Update half-edge counts.
        if(hEdge->lineDef)
            realNum--;
        else
            miniNum--;

        // Disassociate ourself.
        hEdge->block = NULL;
        return hEdge;
    }
};

SuperBlockmap* SuperBlock_Blockmap(SuperBlock* block)
{
    assert(block);
    return block->blockmap();
}

const AABox* SuperBlock_Bounds(SuperBlock* block)
{
    assert(block);
    return block->bounds();
}

uint SuperBlock_HEdgeCount(SuperBlock* block, boolean addReal, boolean addMini)
{
    assert(block);
    return block->hEdgeCount(addReal, addMini);
}

SuperBlock* SuperBlock_Child(SuperBlock* block, int left)
{
    assert(block);
    return block->child(left);
}

void SuperBlock_HEdgePush(SuperBlock* sb, bsp_hedge_t* hEdge)
{
    assert(sb);
    if(!hEdge) return;

    for(;;)
    {
        int p1, p2, half, midPoint[2];
        const AABox* bounds;

        // Update half-edge counts.
        if(hEdge->lineDef)
            sb->realNum++;
        else
            sb->miniNum++;

        if(sb->isLeaf())
        {
            // No further subdivision possible.
            sb->linkHEdge(hEdge);
            return;
        }

        bounds = SuperBlock_Bounds(sb);
        midPoint[0] = (bounds->minX + bounds->maxX) / 2;
        midPoint[1] = (bounds->minY + bounds->maxY) / 2;

        if(bounds->maxX - bounds->minX >=
           bounds->maxY - bounds->minY)
        {
            // Wider than tall.
            p1 = hEdge->v[0]->buildData.pos[0] >= midPoint[0];
            p2 = hEdge->v[1]->buildData.pos[0] >= midPoint[0];
        }
        else
        {
            // Taller than wide.
            p1 = hEdge->v[0]->buildData.pos[1] >= midPoint[1];
            p2 = hEdge->v[1]->buildData.pos[1] >= midPoint[1];
        }

        if(p1 && p2)
        {
            half = 1;
        }
        else if(!p1 && !p2)
        {
            half = 0;
        }
        else
        {
            // Line crosses midpoint -- link it in and return.
            sb->linkHEdge(hEdge);
            return;
        }

        // The hedge lies in one half of this block. Create the sub-block
        // if it doesn't already exist, and loop back to add the hedge.
        if(!KdTreeNode_Child(sb->tree, half))
        {
            SuperBlock* child;
            AABox sub;

            bounds = SuperBlock_Bounds(sb);
            if(bounds->maxX - bounds->minX >= bounds->maxY - bounds->minY)
            {
                sub.minX = (half? midPoint[0] : bounds->minX);
                sub.minY = bounds->minY;

                sub.maxX = (half? bounds->maxX : midPoint[0]);
                sub.maxY = bounds->maxY;
            }
            else
            {
                sub.minX = bounds->minX;
                sub.minY = (half? midPoint[1] : bounds->minY);

                sub.maxX = bounds->maxX;
                sub.maxY = (half? bounds->maxY : midPoint[1]);
            }

            child = new SuperBlock(SuperBlock_Blockmap(sb)/*, &sub*/);
            child->tree = KdTreeNode_AddChild(sb->tree, &sub, half, child);
        }

        sb = (SuperBlock*)KdTreeNode_UserData(KdTreeNode_Child(sb->tree, half));
    }
}

bsp_hedge_t* SuperBlock_HEdgePop(SuperBlock* block)
{
    assert(block);
    return block->popHEdge();
}

int SuperBlock_IterateHEdges2(SuperBlock* block, int (*callback)(bsp_hedge_t*, void*),
    void* parameters)
{
    assert(block);
    if(!callback) return 0;
    for(HEdges::iterator it = block->hEdges.begin(); it != block->hEdges.end(); ++it)
    {
        bsp_hedge_t* hEdge = *it;
        int result = callback(hEdge, parameters);
        if(result) return result; // Stop iteration.
    }
    return 0; // Continue iteration.
}

int SuperBlock_IterateHEdges(SuperBlock* block, int (*callback)(bsp_hedge_t*, void*))
{
    return SuperBlock_IterateHEdges2(block, callback, NULL/*no parameters*/);
}

typedef struct {
    int (*callback)(SuperBlock*, void*);
    void* parameters;
} treetraverserparams_t;

static int SuperBlock_TreeTraverser(KdTreeNode* kd, void* parameters)
{
    treetraverserparams_t* p = (treetraverserparams_t*)parameters;
    return p->callback((SuperBlock*)KdTreeNode_UserData(kd), p->parameters);
}

int SuperBlock_Traverse2(SuperBlock* sb, int (*callback)(SuperBlock*, void*),
    void* parameters)
{
    treetraverserparams_t parm;
    assert(sb);
    if(!callback) return false; // Continue iteration.

    parm.callback = callback;
    parm.parameters = parameters;
    return KdTreeNode_Traverse2(sb->tree, SuperBlock_TreeTraverser, (void*)&parm);
}

int SuperBlock_Traverse(SuperBlock* sb, int (*callback)(SuperBlock*, void*))
{
    return SuperBlock_Traverse2(sb, callback, NULL/*no parameters*/);
}

struct superblockmap_s {
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

    superblockmap_s(const AABox* bounds)
    {
        SuperBlock* block;
        kdTree = KdTree_New(bounds);

        block = new SuperBlock(this/*, bounds*/);
        block->tree = KdTreeNode_SetUserData(KdTree_Root(kdTree), block);
    }

    ~superblockmap_s()
    {
        SuperBlockmap_PostTraverse(this, deleteSuperBlockIterator);
        KdTree_Delete(kdTree);
    }

    SuperBlock* root()
    {
        return (SuperBlock*)KdTreeNode_UserData(KdTree_Root(kdTree));
    }
};

static int deleteSuperBlockIterator(SuperBlock* block, void* /*parameters*/)
{
    delete block;
    return false; // Continue iteration.
}

SuperBlockmap* SuperBlockmap_New(const AABox* bounds)
{
    SuperBlockmap* bmap = new SuperBlockmap(bounds);
    return bmap;
}

void SuperBlockmap_Delete(SuperBlockmap* bmap)
{
    assert(bmap);
    delete bmap;
}

SuperBlock* SuperBlockmap_Root(SuperBlockmap* bmap)
{
    assert(bmap);
    return bmap->root();
}

int SuperBlockmap_PostTraverse2(SuperBlockmap* sbmap, int(*callback)(SuperBlock*, void*),
    void* parameters)
{
    treetraverserparams_t parm;
    assert(sbmap);
    if(!callback) return false; // Continue iteration.

    parm.callback = callback;
    parm.parameters = parameters;
    return KdTree_PostTraverse2(sbmap->kdTree, SuperBlock_TreeTraverser, (void*)&parm);
}

int SuperBlockmap_PostTraverse(SuperBlockmap* sbmap, int(*callback)(SuperBlock*, void*))
{
    return SuperBlockmap_PostTraverse2(sbmap, callback, NULL/*no parameters*/);
}

typedef struct {
    AABoxf bounds;
    boolean initialized;
} findhedgelistboundsparams_t;

static int findHEdgeBoundsWorker(SuperBlock* block, void* parameters)
{
    findhedgelistboundsparams_t* p = (findhedgelistboundsparams_t*)parameters;
    if(block->hEdgeCount(true, true))
    {
        AABoxf blockHEdgeAABox;
        block->findHEdgeBounds(&blockHEdgeAABox);
        if(p->initialized)
        {
            V2_AddToBox(p->bounds.arvec2, blockHEdgeAABox.min);
        }
        else
        {
            V2_InitBox(p->bounds.arvec2, blockHEdgeAABox.min);
            p->initialized = true;
        }
        V2_AddToBox(p->bounds.arvec2, blockHEdgeAABox.max);
    }
    return false; // Continue iteration.
}

void SuperBlockmap_FindHEdgeBounds(SuperBlockmap* sbmap, AABoxf* aaBox)
{
    findhedgelistboundsparams_t parm;
    assert(sbmap && aaBox);

    parm.initialized = false;
    SuperBlock_Traverse2(SuperBlockmap_Root(sbmap), findHEdgeBoundsWorker, (void*)&parm);
    if(parm.initialized)
    {
        V2_CopyBox(aaBox->arvec2, parm.bounds.arvec2);
        return;
    }

    // Clear.
    V2_Set(aaBox->min, DDMAXFLOAT, DDMAXFLOAT);
    V2_Set(aaBox->max, DDMINFLOAT, DDMINFLOAT);
}
