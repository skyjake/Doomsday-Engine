/**
 * @file bspleaf.c
 * BspLeaf implementation. @ingroup map
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_play.h"
#include "m_misc.h"

#include <de/Log>

BspLeaf* BspLeaf_New(void)
{
    BspLeaf* leaf = static_cast<BspLeaf*>(Z_Calloc(sizeof(*leaf), PU_MAP, 0));
    leaf->header.type = DMU_BSPLEAF;
    return leaf;
}

void BspLeaf_ChooseFanBase(BspLeaf* leaf)
{
#define MIN_TRIANGLE_EPSILON  (0.1) ///< Area

    leaf->fanBase = leaf->hedge;

    if(leaf->hedgeCount > 3)
    {
        // Leafs with higher vertex counts demand checking.
        fvertex_t* baseVtx, *a, *b;

        // Search for a good base.
        do
        {
            HEdge* other = leaf->hedge;

            baseVtx = &leaf->fanBase->HE_v1->v;
            do
            {
                // Test this triangle?
                if(!(leaf->fanBase != leaf->hedge &&
                     (other == leaf->fanBase || other == leaf->fanBase->prev)))
                {
                    a = &other->HE_v1->v;
                    b = &other->HE_v2->v;

                    if(M_TriangleArea(baseVtx->pos, a->pos, b->pos) <= MIN_TRIANGLE_EPSILON)
                    {
                        // No good. We'll move on to the next vertex.
                        baseVtx = NULL;
                    }
                }

                // On to the next triangle.
            } while(baseVtx && (other = other->next) != leaf->hedge);

            if(!baseVtx)
            {
                // No good. Select the next vertex and start over.
                leaf->fanBase = leaf->fanBase->next;
            }
        } while(!baseVtx && leaf->fanBase != leaf->hedge);

        // Did we find something suitable?
        if(!baseVtx)
        {
            // No. The mid point will be used instead.
            leaf->fanBase = 0;
        }
    }
    else
    {
        // Implicitly suitable (or completely degenerate...).
    }

#undef MIN_TRIANGLE_EPSILON
}

uint BspLeaf_NumFanVertices(const BspLeaf* leaf)
{
    Q_ASSERT(leaf);
    return leaf->hedgeCount + (leaf->fanBase? 0 : 2);
}

void BspLeaf_PrepareFan(const BspLeaf* leaf, boolean antiClockwise, float height,
    rvertex_t* rvertices, uint numVertices)
{
    Q_ASSERT(leaf);

    if(!rvertices || !numVertices) return;

    if(numVertices < BspLeaf_NumFanVertices(leaf))
    {
        LOG_WARNING("BspLeaf::PrepareFan: Supplied buffer is not large enough for %u vertices (%u specified), ignoring.")
                << BspLeaf_NumFanVertices(leaf) << numVertices;
        return;
    }

    uint n = 0;
    // If this is a trifan the first vertex is always the midpoint.
    if(!leaf->fanBase)
    {
        rvertices[n].pos[VX] = leaf->midPoint[VX];
        rvertices[n].pos[VY] = leaf->midPoint[VY];
        rvertices[n].pos[VZ] = height;
        n++;
    }

    // Add the vertices for each hedge.
    HEdge* baseHEdge = leaf->fanBase? leaf->fanBase : leaf->hedge;
    HEdge* hedge = baseHEdge;
    do
    {
        rvertices[n].pos[VX] = hedge->HE_v1pos[VX];
        rvertices[n].pos[VY] = hedge->HE_v1pos[VY];
        rvertices[n].pos[VZ] = height;
        n++;
    } while((hedge = antiClockwise? hedge->prev : hedge->next) != baseHEdge);

    // The last vertex is always equal to the first.
    if(!leaf->fanBase)
    {
        rvertices[n].pos[VX] = leaf->hedge->HE_v1pos[VX];
        rvertices[n].pos[VY] = leaf->hedge->HE_v1pos[VY];
        rvertices[n].pos[VZ] = height;
    }
}

void BspLeaf_Delete(BspLeaf* leaf)
{
    Q_ASSERT(leaf);

    if(leaf->bsuf)
    {
        Sector* sec = leaf->sector;
        uint i;
        for(i = 0; i < sec->planeCount; ++i)
        {
            SB_DestroySurface(leaf->bsuf[i]);
        }
        Z_Free(leaf->bsuf);
    }

    // Clear the HEdges.
    if(leaf->hedge)
    {
        HEdge* hedge = leaf->hedge;
        if(hedge->next == hedge)
        {
            HEdge_Delete(hedge);
        }
        else
        {
            HEdge* next;

            // Break the ring.
            hedge->prev->next = NULL;
            do
            {
                next = hedge->next;
                HEdge_Delete(hedge);
            } while((hedge = next));
        }
    }

    Z_Free(leaf);
}

void BspLeaf_UpdateAABox(BspLeaf* leaf)
{
    Q_ASSERT(leaf);

    V2f_Set(leaf->aaBox.min, DDMAXFLOAT, DDMAXFLOAT);
    V2f_Set(leaf->aaBox.max, DDMINFLOAT, DDMINFLOAT);

    if(!leaf->hedge) return; // Very odd...

    HEdge* hedge = leaf->hedge;
    V2f_InitBox(leaf->aaBox.arvec2, hedge->HE_v1pos);

    while((hedge = hedge->next) != leaf->hedge)
    {
        V2f_AddToBox(leaf->aaBox.arvec2, hedge->HE_v1pos);
    }
}

void BspLeaf_UpdateMidPoint(BspLeaf* leaf)
{
    Q_ASSERT(leaf);
    // The middle is the center of our AABox.
    leaf->midPoint[VX] = leaf->aaBox.minX + (leaf->aaBox.maxX - leaf->aaBox.minX) / 2;
    leaf->midPoint[VY] = leaf->aaBox.minY + (leaf->aaBox.maxY - leaf->aaBox.minY) / 2;
}

void BspLeaf_UpdateWorldGridOffset(BspLeaf* leaf)
{
    Q_ASSERT(leaf);
    leaf->worldGridOffset[VX] = fmod(leaf->aaBox.minX, 64);
    leaf->worldGridOffset[VY] = fmod(leaf->aaBox.maxY, 64);
}

int BspLeaf_SetProperty(BspLeaf* leaf, const setargs_t* args)
{
    Q_ASSERT(leaf);
    Con_Error("BspLeaf::SetProperty: Property %s is not writable.\n", DMU_Str(args->prop));
    exit(1); // Unreachable.
}

int BspLeaf_GetProperty(const BspLeaf* leaf, setargs_t* args)
{
    Q_ASSERT(leaf);
    switch(args->prop)
    {
    case DMU_SECTOR:
        DMU_GetValue(DMT_BSPLEAF_SECTOR, &leaf->sector, args, 0);
        break;
    case DMU_LIGHT_LEVEL:
        DMU_GetValue(DMT_SECTOR_LIGHTLEVEL, &leaf->sector->lightLevel, args, 0);
        break;
    case DMT_MOBJS:
        DMU_GetValue(DMT_SECTOR_MOBJLIST, &leaf->sector->mobjList, args, 0);
        break;
    case DMU_HEDGE_COUNT: {
        int val = (int) leaf->hedgeCount;
        DMU_GetValue(DDVT_INT, &val, args, 0);
        break; }
    default:
        Con_Error("BspLeaf::GetProperty: No property %s.\n", DMU_Str(args->prop));
        exit(1); // Unreachable.
    }
    return false; // Continue iteration.
}
