/** @file bspleaf.cpp BspLeaf implementation. 
 * @ingroup map
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

BspLeaf::BspLeaf() : de::MapElement(DMU_BSPLEAF)
{
    hedge = 0;
    flags = 0;
    index = 0;
    addSpriteCount = 0;
    validCount = 0;
    hedgeCount = 0;
    sector = 0;
    polyObj = 0;
    fanBase = 0;
    shadows = 0;
    memset(&aaBox, 0, sizeof(aaBox));
    memset(midPoint, 0, sizeof(midPoint));
    memset(worldGridOffset, 0, sizeof(worldGridOffset));
    bsuf = 0;
    memset(reverb, 0, sizeof(reverb));
}

BspLeaf::~BspLeaf()
{
    if(bsuf)
    {
#ifdef __CLIENT__
        for(uint i = 0; i < sector->planeCount(); ++i)
        {
            SB_DestroySurface(bsuf[i]);
        }
#endif
        Z_Free(bsuf);
    }

    // Clear the HEdges.
    if(hedge)
    {
        HEdge *he = hedge;
        if(he->next == he)
        {
            HEdge_Delete(he);
        }
        else
        {
            // Break the ring, if linked.
            if(he->prev)
            {
                he->prev->next = NULL;
            }

            while(he)
            {
                HEdge *next = he->next;
                HEdge_Delete(he);
                he = next;
            }
        }
    }
}

BspLeaf* BspLeaf_New(void)
{
    BspLeaf* leaf = new BspLeaf;
    //leaf->header.type = DMU_BSPLEAF;
    leaf->flags |= BLF_UPDATE_FANBASE;
    return leaf;
}

void BspLeaf_Delete(BspLeaf* leaf)
{
    delete leaf;
}

biassurface_t* BspLeaf_BiasSurfaceForGeometryGroup(BspLeaf* leaf, uint groupId)
{
    DENG2_ASSERT(leaf);
    if(!leaf->sector || groupId > leaf->sector->planeCount()) return NULL;
    DENG2_ASSERT(leaf->bsuf != 0);
    return leaf->bsuf[groupId];
}

BspLeaf* BspLeaf_UpdateAABox(BspLeaf* leaf)
{
    DENG2_ASSERT(leaf);

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
    DENG2_ASSERT(leaf);
    // The middle is the center of our AABox.
    leaf->midPoint[VX] = leaf->aaBox.minX + (leaf->aaBox.maxX - leaf->aaBox.minX) / 2;
    leaf->midPoint[VY] = leaf->aaBox.minY + (leaf->aaBox.maxY - leaf->aaBox.minY) / 2;
    return leaf;
}

BspLeaf* BspLeaf_UpdateWorldGridOffset(BspLeaf* leaf)
{
    DENG2_ASSERT(leaf);
    leaf->worldGridOffset[VX] = fmod(leaf->aaBox.minX, 64);
    leaf->worldGridOffset[VY] = fmod(leaf->aaBox.maxY, 64);
    return leaf;
}

int BspLeaf_SetProperty(BspLeaf* leaf, const setargs_t* args)
{
    DENG2_ASSERT(leaf);
    DENG_UNUSED(leaf);
    Con_Error("BspLeaf::SetProperty: Property %s is not writable.\n", DMU_Str(args->prop));
    exit(1); // Unreachable.
}

int BspLeaf_GetProperty(const BspLeaf* leaf, setargs_t* args)
{
    DENG2_ASSERT(leaf);
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
