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

static __inline boolean isLeaf(SuperBlock* sb)
{
    assert(sb);
    return (sb->aaBox.maxX - sb->aaBox.minX <= 256 &&
            sb->aaBox.maxY - sb->aaBox.minY <= 256);
}

static void linkHEdge(SuperBlock* sb, bsp_hedge_t* hEdge)
{
    assert(sb && hEdge);
    hEdge->next = sb->hEdges;
    hEdge->block = sb;

    sb->hEdges = hEdge;
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

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        if(sb->subs[num])
            SuperBlock_Delete(sb->subs[num]);
    }

    BSP_RecycleSuperBlock(sb);
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

void SuperBlock_IncrementHEdgeCounts(SuperBlock* sb, boolean lineLinked)
{
    assert(sb);
    if(lineLinked)
        sb->realNum++;
    else
        sb->miniNum++;

    // Recursively handle parents.
    if(sb->parent)
    {
        SuperBlock_IncrementHEdgeCounts(sb->parent, lineLinked);
    }
}

void SuperBlock_HEdgePush(SuperBlock* sb, bsp_hedge_t* hEdge)
{
    assert(sb);
    if(!hEdge) return;

    for(;;)
    {
        int p1, p2, half, midPoint[2];
        SuperBlock* child;

        // Update half-edge counts.
        if(hEdge->lineDef)
            sb->realNum++;
        else
            sb->miniNum++;

        if(isLeaf(sb))
        {
            // No further subdivision possible.
            linkHEdge(sb, hEdge);
            return;
        }

        midPoint[VX] = (sb->aaBox.minX + sb->aaBox.maxX) / 2;
        midPoint[VY] = (sb->aaBox.minY + sb->aaBox.maxY) / 2;

        if(sb->aaBox.maxX - sb->aaBox.minX >=
           sb->aaBox.maxY - sb->aaBox.minY)
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
        if(!sb->subs[half])
        {
            AABox sub;

            if(sb->aaBox.maxX - sb->aaBox.minX >=
               sb->aaBox.maxY - sb->aaBox.minY)
            {
                sub.minX = (half? midPoint[VX] : sb->aaBox.minX);
                sub.minY = sb->aaBox.minY;

                sub.maxX = (half? sb->aaBox.maxX : midPoint[VX]);
                sub.maxY = sb->aaBox.maxY;
            }
            else
            {
                sub.minX = sb->aaBox.minX;
                sub.minY = (half? midPoint[VY] : sb->aaBox.minY);

                sub.maxX = sb->aaBox.maxX;
                sub.maxY = (half? sb->aaBox.maxY : midPoint[VY]);
            }

            sb->subs[half] = child = BSP_NewSuperBlock(&sub);
            child->parent = sb;
        }

        sb = sb->subs[half];
    }
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

int SuperBlock_IterateHEdges2(SuperBlock* sp, int (*callback)(bsp_hedge_t*, void*),
    void* parameters)
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

int SuperBlock_IterateHEdges(SuperBlock* sp, int (*callback)(bsp_hedge_t*, void*))
{
    return SuperBlock_IterateHEdges2(sp, callback, NULL/*no parameters*/);
}

SuperBlock* SuperBlock_Child(SuperBlock* sb, boolean left)
{
    assert(sb);
    return sb->subs[left?1:0];
}

int SuperBlock_Traverse2(SuperBlock* sb, int (*callback)(SuperBlock*, void*),
    void* parameters)
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

        result = SuperBlock_Traverse2(child, callback, parameters);
        if(result) return result;
    }

    return false; // Continue iteration.
}

int SuperBlock_Traverse(SuperBlock* sb, int (*callback)(SuperBlock*, void*))
{
    return SuperBlock_Traverse2(sb, callback, NULL/*no parameters*/);
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

static void findHEdgeListBoundsWorker(SuperBlock* sb, void* parameters)
{
    findhedgelistboundsparams_t* p = (findhedgelistboundsparams_t*)parameters;
    AABoxf hEdgeAABox;
    bsp_hedge_t* hEdge;
    uint i;

    for(hEdge = sb->hEdges; hEdge; hEdge = hEdge->next)
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
        if(sb->subs[i])
        {
            findHEdgeListBoundsWorker(sb->subs[i], parameters);
        }
    }
}

void SuperBlock_FindHEdgeListBounds(SuperBlock* sb, AABoxf* aaBox)
{
    findhedgelistboundsparams_t parm;
    assert(sb && aaBox);

    parm.initialized = false;
    findHEdgeListBoundsWorker(sb, (void*)&parm);
    if(parm.initialized)
    {
        V2_CopyBox(aaBox->arvec2, parm.bounds.arvec2);
        return;
    }

    // Clear.
    V2_Set(aaBox->min, DDMAXFLOAT, DDMAXFLOAT);
    V2_Set(aaBox->max, DDMINFLOAT, DDMINFLOAT);
}

/**
 * @todo The following does not belong in this module.
 */

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

void BSP_RecycleSuperBlock(SuperBlock* sb)
{
    if(!sb) return;

    if(sb->hEdges)
    {
        // This can happen, but only under abnormal circumstances.
#if _DEBUG
        Con_Error("BSP_RecycleSuperBlock: Superblock contains half-edges!");
#endif
        sb->hEdges = NULL;
    }

    // Add block to quick-alloc list. Note that subs[0] is used for
    // linking the blocks together.
    sb->subs[0] = quickAllocSupers;
    quickAllocSupers = sb;
}
