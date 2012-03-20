/**
 * @file bsp_superblock.c
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

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

#include "de_base.h"
#include "de_console.h"
#include "de_bsp.h"
#include "de_play.h"
#include "de_misc.h"

#include "kdtree.h"

struct superblock_s {
    /// SuperBlockmap which owns this block.
    struct superblockmap_s* blockmap;

    /// KdTree node in the owning SuperBlockmap.
    KdTree* tree;

    /// Number of real half-edges and minihedges contained by this block
    /// (including all sub-blocks below it).
    int realNum;
    int miniNum;

    /// List of half-edges completely contained by this block.
    struct bsp_hedge_s* hEdges;
};

/**
 * Constructs a new superblock. The superblock must be destroyed with
 * SuperBlock_Delete() when no longer needed.
 */
static SuperBlock* SuperBlock_New(SuperBlockmap* blockmap, const AABox* bounds);

/**
 * Destroys the superblock.
 */
static void SuperBlock_Delete(SuperBlock* superblock);

/**
 * Subblocks:
 * RIGHT - has the lower coordinates.
 * LEFT  - has the higher coordinates.
 * Division of a block always occurs horizontally, e.g. 512x512 -> 256x512 -> 256x256.
 */
struct superblockmap_s {
    SuperBlock* root;
};

SuperBlockmap* SuperBlockmap_New(const AABox* bounds)
{
    SuperBlockmap* bmap = malloc(sizeof *bmap);
    if(!bmap) Con_Error("SuperBlockmap_New: Failed on allocation of %lu bytes for new SuperBlockmap.", (unsigned long) sizeof *bmap);
    bmap->root = SuperBlock_New(bmap, bounds);
    return bmap;
}

void SuperBlockmap_Delete(SuperBlockmap* bmap)
{
    assert(bmap);
    SuperBlock_Delete(bmap->root);
    free(bmap);
}

SuperBlock* SuperBlockmap_Root(SuperBlockmap* bmap)
{
    assert(bmap);
    return bmap->root;
}

static __inline boolean isLeaf(SuperBlockmap* blockmap, SuperBlock* sb)
{
    const AABox* bounds = SuperBlock_Bounds(sb);
    return (bounds->maxX - bounds->minX <= 256 &&
            bounds->maxY - bounds->minY <= 256);
}

static void linkHEdge(SuperBlock* sb, bsp_hedge_t* hEdge)
{
    assert(sb && hEdge);
    hEdge->nextInBlock = sb->hEdges;
    hEdge->block = sb;

    sb->hEdges = hEdge;
}

SuperBlock* SuperBlock_New(SuperBlockmap* blockmap, const AABox* bounds)
{
    SuperBlock* sb = malloc(sizeof *sb);
    if(!sb) Con_Error("SuperBlock_New: Failed on allocation of %lu bytes for new SuperBlock.", (unsigned long) sizeof *sb);
    sb->blockmap = blockmap;
    sb->tree = KdTree_NewWithUserData(bounds, sb);
    sb->realNum = 0;
    sb->miniNum = 0;
    sb->hEdges = NULL;
    return sb;
}

static int deleteSuperBlock(SuperBlock* sb, void* parameters)
{
    M_Free(sb);
    return false; // Continue iteration.
}

void SuperBlock_Delete(SuperBlock* sb)
{
    KdTree* tree;
    assert(sb);

    tree = sb->tree;
    SuperBlock_PostTraverse(sb, deleteSuperBlock);
    KdTree_Delete(tree);
}

SuperBlockmap* SuperBlock_Blockmap(SuperBlock* sb)
{
    assert(sb);
    return sb->blockmap;
}

const AABox* SuperBlock_Bounds(SuperBlock* sb)
{
    assert(sb);
    return KdTree_Bounds(sb->tree);
}

uint SuperBlock_HEdgeCount(SuperBlock* sb, boolean addReal, boolean addMini)
{
    uint total = 0;
    assert(sb);
    if(addReal) total += sb->realNum;
    if(addMini) total += sb->miniNum;
    return total;
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

        if(isLeaf(SuperBlock_Blockmap(sb), sb))
        {
            // No further subdivision possible.
            linkHEdge(sb, hEdge);
            return;
        }

        bounds = SuperBlock_Bounds(sb);
        midPoint[VX] = (bounds->minX + bounds->maxX) / 2;
        midPoint[VY] = (bounds->minY + bounds->maxY) / 2;

        if(bounds->maxX - bounds->minX >=
           bounds->maxY - bounds->minY)
        {
            // Wider than tall.
            p1 = hEdge->v[0]->buildData.pos[VX] >= midPoint[VX];
            p2 = hEdge->v[1]->buildData.pos[VX] >= midPoint[VX];
        }
        else
        {
            // Taller than wide.
            p1 = hEdge->v[0]->buildData.pos[VY] >= midPoint[VY];
            p2 = hEdge->v[1]->buildData.pos[VY] >= midPoint[VY];
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
            linkHEdge(sb, hEdge);
            return;
        }

        // The hedge lies in one half of this block. Create the sub-block
        // if it doesn't already exist, and loop back to add the hedge.
        if(!KdTree_Child(sb->tree, half))
        {
            SuperBlock* child;
            AABox sub;

            bounds = SuperBlock_Bounds(sb);
            if(bounds->maxX - bounds->minX >= bounds->maxY - bounds->minY)
            {
                sub.minX = (half? midPoint[VX] : bounds->minX);
                sub.minY = bounds->minY;

                sub.maxX = (half? bounds->maxX : midPoint[VX]);
                sub.maxY = bounds->maxY;
            }
            else
            {
                sub.minX = bounds->minX;
                sub.minY = (half? midPoint[VY] : bounds->minY);

                sub.maxX = bounds->maxX;
                sub.maxY = (half? bounds->maxY : midPoint[VY]);
            }

            child = SuperBlock_New(SuperBlock_Blockmap(sb), &sub);
            child->tree = KdTree_AddChild(sb->tree, &sub, half, child);
        }

        sb = KdTree_UserData(KdTree_Child(sb->tree, half));
    }
}

bsp_hedge_t* SuperBlock_HEdgePop(SuperBlock* sb)
{
    bsp_hedge_t* hEdge;
    assert(sb);

    hEdge = sb->hEdges;
    if(hEdge)
    {
        sb->hEdges = hEdge->nextInBlock;
        hEdge->block = NULL;
        hEdge->nextInBlock = NULL;

        // Update half-edge counts.
        if(hEdge->lineDef)
            sb->realNum--;
        else
            sb->miniNum--;
    }
    return hEdge;
}

int SuperBlock_IterateHEdges2(SuperBlock* sp, int (*callback)(bsp_hedge_t*, void*),
    void* parameters)
{
    assert(sp);
    if(callback)
    {
        bsp_hedge_t* hEdge;
        for(hEdge = sp->hEdges; hEdge; hEdge = hEdge->nextInBlock)
        {
            int result = callback(hEdge, parameters);
            if(result) return result; // Stop iteration.
        }
    }
    return false; // Continue iteration.
}

int SuperBlock_IterateHEdges(SuperBlock* sp, int (*callback)(bsp_hedge_t*, void*))
{
    return SuperBlock_IterateHEdges2(sp, callback, NULL/*no parameters*/);
}

SuperBlock* SuperBlock_Child(SuperBlock* sb, int left)
{
    KdTree* child;
    assert(sb);
    child = KdTree_Child(sb->tree, left);
    if(!child) return NULL;
    return (SuperBlock*)KdTree_UserData(child);
}

typedef struct {
    int (*callback)(SuperBlock*, void*);
    void* parameters;
} treetraverserparams_t;

static int SuperBlock_TreeTraverser(KdTree* kd, void* parameters)
{
    treetraverserparams_t* p = (treetraverserparams_t*)parameters;
    return p->callback(KdTree_UserData(kd), p->parameters);
}

int SuperBlock_Traverse2(SuperBlock* sb, int (*callback)(SuperBlock*, void*),
    void* parameters)
{
    treetraverserparams_t parm;
    assert(sb);
    if(!callback) return false; // Continue iteration.

    parm.callback = callback;
    parm.parameters = parameters;
    return KdTree_Traverse2(sb->tree, SuperBlock_TreeTraverser, (void*)&parm);
}

int SuperBlock_Traverse(SuperBlock* sb, int (*callback)(SuperBlock*, void*))
{
    return SuperBlock_Traverse2(sb, callback, NULL/*no parameters*/);
}

int SuperBlock_PostTraverse2(SuperBlock* sb, int(*callback)(SuperBlock*, void*),
    void* parameters)
{
    treetraverserparams_t parm;
    assert(sb);
    if(!callback) return false; // Continue iteration.

    parm.callback = callback;
    parm.parameters = parameters;
    return KdTree_PostTraverse2(sb->tree, SuperBlock_TreeTraverser, (void*)&parm);
}

int SuperBlock_PostTraverse(SuperBlock* sb, int(*callback)(SuperBlock*, void*))
{
    return SuperBlock_PostTraverse2(sb, callback, NULL/*no parameters*/);
}

static void initAABoxFromHEdgeVertexes(AABoxf* aaBox, const bsp_hedge_t* hEdge)
{
    const double* from = hEdge->HE_v1->buildData.pos;
    const double* to   = hEdge->HE_v2->buildData.pos;
    aaBox->minX = MIN_OF(from[VX], to[VX]);
    aaBox->minY = MIN_OF(from[VY], to[VY]);
    aaBox->maxX = MAX_OF(from[VX], to[VX]);
    aaBox->maxY = MAX_OF(from[VY], to[VY]);
}

typedef struct {
    AABoxf bounds;
    boolean initialized;
} findhedgelistboundsparams_t;

static int findHEdgeListBoundsWorker(SuperBlock* sb, void* parameters)
{
    findhedgelistboundsparams_t* p = (findhedgelistboundsparams_t*)parameters;
    AABoxf hEdgeAABox;
    bsp_hedge_t* hEdge;

    for(hEdge = sb->hEdges; hEdge; hEdge = hEdge->nextInBlock)
    {
        initAABoxFromHEdgeVertexes(&hEdgeAABox, hEdge);
        if(p->initialized)
        {
            V2_AddToBox(p->bounds.arvec2, hEdgeAABox.min);
        }
        else
        {
            V2_InitBox(p->bounds.arvec2, hEdgeAABox.min);
            p->initialized = true;
        }
        V2_AddToBox(p->bounds.arvec2, hEdgeAABox.max);
    }

    return false; // Continue iteration.
}

void SuperBlockmap_FindHEdgeBounds(SuperBlockmap* sbmap, AABoxf* aaBox)
{
    findhedgelistboundsparams_t parm;
    assert(sbmap && aaBox);

    parm.initialized = false;
    SuperBlock_Traverse2(sbmap->root, findHEdgeListBoundsWorker, (void*)&parm);
    if(parm.initialized)
    {
        V2_CopyBox(aaBox->arvec2, parm.bounds.arvec2);
        return;
    }

    // Clear.
    V2_Set(aaBox->min, DDMAXFLOAT, DDMAXFLOAT);
    V2_Set(aaBox->max, DDMINFLOAT, DDMINFLOAT);
}
