/**
 * @file bspleaf.cpp
 * BspLeaf implementation. @ingroup map
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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
#include "de_play.h"
#include "m_misc.h"

BspLeaf* BspLeaf_New(void)
{
    BspLeaf* leaf = static_cast<BspLeaf*>(Z_Calloc(sizeof(*leaf), PU_MAP, 0));
    leaf->header.type = DMU_BSPLEAF;
    leaf->flags |= BLF_UPDATE_FANBASE;
    return leaf;
}

void BspLeaf_Delete(BspLeaf* leaf)
{
    Q_ASSERT(leaf);

    if(leaf->bsuf)
    {
        Sector* sec = leaf->sector;
        for(uint i = 0; i < sec->planeCount; ++i)
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
            // Break the ring, if linked.
            if(hedge->prev)
            {
                hedge->prev->next = NULL;
            }

            while(hedge)
            {
                HEdge* next = hedge->next;
                HEdge_Delete(hedge);
                hedge = next;
            }
        }
    }

    Z_Free(leaf);
}

biassurface_t* BspLeaf_BiasSurfaceForGeometryGroup(BspLeaf* leaf, uint groupId)
{
    Q_ASSERT(leaf);
    if(!leaf->sector || groupId > leaf->sector->planeCount) return NULL;
    return leaf->bsuf[groupId];
}

BspLeaf* BspLeaf_UpdateAABox(BspLeaf* leaf)
{
    Q_ASSERT(leaf);

    V2d_Set(leaf->aaBox.min, DDMAXFLOAT, DDMAXFLOAT);
    V2d_Set(leaf->aaBox.max, DDMINFLOAT, DDMINFLOAT);

    if(!leaf->hedge) return leaf; // Very odd...

    HEdge* hedge = leaf->hedge;
    V2d_InitBox(leaf->aaBox.arvec2, hedge->HE_v1origin);

    while((hedge = hedge->next) != leaf->hedge)
    {
        V2d_AddToBox(leaf->aaBox.arvec2, hedge->HE_v1origin);
    }

    return leaf;
}

BspLeaf* BspLeaf_UpdateMidPoint(BspLeaf* leaf)
{
    Q_ASSERT(leaf);
    // The middle is the center of our AABox.
    leaf->midPoint[VX] = leaf->aaBox.minX + (leaf->aaBox.maxX - leaf->aaBox.minX) / 2;
    leaf->midPoint[VY] = leaf->aaBox.minY + (leaf->aaBox.maxY - leaf->aaBox.minY) / 2;
    return leaf;
}

BspLeaf* BspLeaf_UpdateWorldGridOffset(BspLeaf* leaf)
{
    Q_ASSERT(leaf);
    leaf->worldGridOffset[VX] = fmod(leaf->aaBox.minX, 64);
    leaf->worldGridOffset[VY] = fmod(leaf->aaBox.maxY, 64);
    return leaf;
}

int BspLeaf_SetProperty(BspLeaf* leaf, const setargs_t* args)
{
    Q_ASSERT(leaf);
    DENG_UNUSED(leaf);
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
