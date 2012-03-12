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

struct superblock_s {
    // Parent of this block, or NULL for a top-level block.
    struct superblock_s* parent;

    // Coordinates on map for this block, from lower-left corner to
    // upper-right corner. Pseudo-inclusive, i.e (x,y) is inside block
    // if and only if minX <= x < maxX and minY <= y < maxY.
    AABox aaBox;

    // Sub-blocks. NULL when empty. [0] has the lower coordinates, and
    // [1] has the higher coordinates. Division of a square always
    // occurs horizontally (e.g. 512x512 -> 256x512 -> 256x256).
    struct superblock_s* subs[2];

    // Number of real half-edges and minihedges contained by this block
    // (including all sub-blocks below it).
    int realNum;
    int miniNum;

    // List of half-edges completely contained by this block.
    struct bsp_hedge_s* hEdges;
};

static SuperBlock* quickAllocSupers;

void BSP_InitSuperBlockAllocator(void)
{
    quickAllocSupers = NULL;
}

void BSP_ShutdownSuperBlockAllocator(void)
{
    while(quickAllocSupers)
    {
        SuperBlock* block = quickAllocSupers;

        quickAllocSupers = block->subs[0];
        M_Free(block);
    }
}

/**
 * Acquire memory for a new superblock.
 */
SuperBlock* BSP_NewSuperBlock(const AABox* bounds)
{
    SuperBlock* sb;

    if(quickAllocSupers == NULL)
        return SuperBlock_New(bounds);

    sb = quickAllocSupers;
    quickAllocSupers = sb->subs[0];

    // Clear out any old rubbish.
    memset(sb, 0, sizeof(*sb));
    memcpy(&sb->aaBox, bounds, sizeof(sb->aaBox));

    return sb;
}

/**
 * Free all memory allocated for the specified superblock.
 */
void BSP_RecycleSuperBlock(SuperBlock* sb)
{
    if(!sb) return;

    // Add block to quick-alloc list. Note that subs[0] is used for
    // linking the blocks together.
    sb->subs[0] = quickAllocSupers;
    quickAllocSupers = sb;
}

SuperBlock* SuperBlock_New(const AABox* bounds)
{
    SuperBlock* sb = M_Calloc(sizeof(*sb));
    memcpy(&sb->aaBox, bounds, sizeof(sb->aaBox));
    return sb;
}

void SuperBlock_Delete(SuperBlock* sb)
{
    uint num;
    assert(sb);

    if(sb->hEdges)
    {
        // This can happen, but only under abnormal circumstances.
#if _DEBUG
        Con_Error("FreeSuper: Superblock contains half-edges!");
#endif
        sb->hEdges = NULL;
    }

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        if(sb->subs[num])
            SuperBlock_Delete(sb->subs[num]);
    }

    BSP_RecycleSuperBlock(sb);
}

static void linkHEdge(SuperBlock* superblock, bsp_hedge_t* hEdge)
{
    hEdge->next = superblock->hEdges;
    hEdge->block = superblock;

    superblock->hEdges = hEdge;
}

void SuperBlock_IncrementHEdgeCounts(SuperBlock* superblock, boolean lineLinked)
{
    do
    {
        if(lineLinked)
            superblock->realNum++;
        else
            superblock->miniNum++;

        superblock = superblock->parent;
    } while(superblock != NULL);
}

/**
 * Add the given half-edge to the specified list.
 */
void SuperBlock_HEdgePush(SuperBlock* block, bsp_hedge_t* hEdge)
{
#define SUPER_IS_LEAF(s)  \
    ((s)->aaBox.maxX - (s)->aaBox.minX <= 256 && \
     (s)->aaBox.maxY - (s)->aaBox.minY <= 256)

    for(;;)
    {
        int p1, p2, half, midPoint[2];
        SuperBlock* child;

        midPoint[VX] = (block->aaBox.minX + block->aaBox.maxX) / 2;
        midPoint[VY] = (block->aaBox.minY + block->aaBox.maxY) / 2;

        // Update half-edge counts.
        if(hEdge->lineDef)
            block->realNum++;
        else
            block->miniNum++;

        if(SUPER_IS_LEAF(block))
        {
            // Block is a leaf -- no subdivision possible.
            linkHEdge(block, hEdge);
            return;
        }

        if(block->aaBox.maxX - block->aaBox.minX >=
           block->aaBox.maxY - block->aaBox.minY)
        {
            // Block is wider than it is high, or square.
            p1 = hEdge->v[0]->buildData.pos[VX] >= midPoint[VX];
            p2 = hEdge->v[1]->buildData.pos[VX] >= midPoint[VX];
        }
        else
        {
            // Block is higher than it is wide.
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
            linkHEdge(block, hEdge);
            return;
        }

        // The hedge lies in one half of this block. Create the block if it
        // doesn't already exist, and loop back to add the hedge.
        if(!block->subs[half])
        {
            AABox sub;

            if(block->aaBox.maxX - block->aaBox.minX >=
               block->aaBox.maxY - block->aaBox.minY)
            {
                sub.minX = (half? midPoint[VX] : block->aaBox.minX);
                sub.minY = block->aaBox.minY;

                sub.maxX = (half? block->aaBox.maxX : midPoint[VX]);
                sub.maxY = block->aaBox.maxY;
            }
            else
            {
                sub.minX = block->aaBox.minX;
                sub.minY = (half? midPoint[VY] : block->aaBox.minY);

                sub.maxX = block->aaBox.maxX;
                sub.maxY = (half? block->aaBox.maxY : midPoint[VY]);
            }

            block->subs[half] = child = BSP_NewSuperBlock(&sub);
            child->parent = block;
        }

        block = block->subs[half];
    }

#undef SUPER_IS_LEAF
}

const AABox* SuperBlock_Bounds(SuperBlock* sb)
{
    assert(sb);
    return &sb->aaBox;
}

uint SuperBlock_HEdgeCount(SuperBlock* sb, boolean addReal, boolean addMini)
{
    uint total = 0;
    assert(sb);
    if(addReal) total += sb->realNum;
    if(addMini) total += sb->miniNum;
    return total;
}

bsp_hedge_t* SuperBlock_HEdgePop(SuperBlock* sb)
{
    bsp_hedge_t* hEdge;
    assert(sb);

    hEdge = sb->hEdges;
    if(hEdge)
    {
        sb->hEdges = hEdge->next;

        // Update half-edge counts.
        if(hEdge->lineDef)
            sb->realNum--;
        else
            sb->miniNum--;
    }
    return hEdge;
}

int SuperBlock_IterateHEdges(SuperBlock* sp, int (*callback)(bsp_hedge_t*, void*), void* parameters)
{
    assert(sp);
    if(callback)
    {
        bsp_hedge_t* hEdge;
        for(hEdge = sp->hEdges; hEdge; hEdge = hEdge->next)
        {
            int result = callback(hEdge, parameters);
            if(result) return result; // Stop iteration.
        }
    }
    return false; // Continue iteration.
}

SuperBlock* SuperBlock_Child(SuperBlock* sb, boolean left)
{
    assert(sb);
    return sb->subs[left?1:0];
}

int SuperBlock_Traverse(SuperBlock* sb, int (*callback)(SuperBlock*, void*), void* parameters)
{
    int num, result;
    assert(sb);

    if(!callback) return false; // Continue iteration.

    result = callback(sb, parameters);
    if(result) return result;

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        SuperBlock* child = sb->subs[num];
        if(!child) continue;

        result = SuperBlock_Traverse(child, callback, parameters);
        if(result) return result;
    }

    return false; // Continue iteration.
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

static void findHEdgeListBoundsWorker(SuperBlock* block, void* parameters)
{
    findhedgelistboundsparams_t* p = (findhedgelistboundsparams_t*)parameters;
    AABoxf hEdgeAABox;
    bsp_hedge_t* hEdge;
    uint i;

    for(hEdge = block->hEdges; hEdge; hEdge = hEdge->next)
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

    // Recursively handle sub-blocks.
    for(i = 0; i < 2; ++i)
    {
        if(block->subs[i])
        {
            findHEdgeListBoundsWorker(block->subs[i], parameters);
        }
    }
}

void SuperBlock_FindHEdgeListBounds(SuperBlock* hEdgeList, AABoxf* aaBox)
{
    findhedgelistboundsparams_t parm;
    assert(hEdgeList && aaBox);

    parm.initialized = false;
    findHEdgeListBoundsWorker(hEdgeList, (void*)&parm);
    if(parm.initialized)
    {
        V2_CopyBox(aaBox->arvec2, parm.bounds.arvec2);
        return;
    }

    // Clear.
    V2_Set(aaBox->min, DDMAXFLOAT, DDMAXFLOAT);
    V2_Set(aaBox->max, DDMINFLOAT, DDMINFLOAT);
}

#if _DEBUG
void BSP_PrintSuperblockHEdges(SuperBlock* superblock)
{
    bsp_hedge_t* hEdge;
    int num;

    for(hEdge = superblock->hEdges; hEdge; hEdge = hEdge->next)
    {
        Con_Message("Build: %s %p sector=%d (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                    (hEdge->lineDef? "NORM" : "MINI"), hEdge,
                    hEdge->sector->buildData.index,
                    hEdge->v[0]->buildData.pos[VX], hEdge->v[0]->buildData.pos[VY],
                    hEdge->v[1]->buildData.pos[VX], hEdge->v[1]->buildData.pos[VY]);
    }

    for(num = 0; num < 2; ++num)
    {
        if(superblock->subs[num])
            BSP_PrintSuperblockHEdges(superblock->subs[num]);
    }
}

static void testSuperWorker(SuperBlock* block, int* real, int* mini)
{
    int num;
    bsp_hedge_t* cur;

    for(cur = block->hEdges; cur; cur = cur->next)
    {
        if(cur->lineDef)
            (*real) += 1;
        else
            (*mini) += 1;
    }

    for(num = 0; num < 2; ++num)
    {
        if(block->subs[num])
            testSuperWorker(block->subs[num], real, mini);
    }
}

/**
 * For debugging.
 */
void testSuper(SuperBlock* block)
{
    int realNum = 0;
    int miniNum = 0;

    testSuperWorker(block, &realNum, &miniNum);

    if(realNum != block->realNum || miniNum != block->miniNum)
        Con_Error("testSuper: Failed, block=%p %d/%d != %d/%d", block, block->realNum,
                  block->miniNum, realNum, miniNum);
}
#endif
