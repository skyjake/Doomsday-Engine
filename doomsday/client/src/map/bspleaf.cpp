/** @file bspleaf.cpp Map BSP Leaf
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <cmath> // fmod

#include "de_base.h"
#include "m_misc.h"
#include "map/hedge.h"
#include "map/sector.h"
#include "map/vertex.h"
#include <de/Log>
#include <de/vector1.h>

#include "map/bspleaf.h"

using namespace de;

BspLeaf::BspLeaf() : MapElement(DMU_BSPLEAF)
{
    hedge = 0;
    flags = BLF_UPDATE_FANBASE;
    index = 0;
    addSpriteCount = 0;
    validCount = 0;
    hedgeCount = 0;
    sector = 0;
    polyObj = 0;
    fanBase = 0;
    shadows = 0;
    std::memset(&aaBox, 0, sizeof(aaBox));
    std::memset(midPoint, 0, sizeof(midPoint));
    std::memset(worldGridOffset, 0, sizeof(worldGridOffset));
    bsuf = 0;
    std::memset(reverb, 0, sizeof(reverb));
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
            delete he;
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
                delete he;
                he = next;
            }
        }
    }
}

biassurface_t &BspLeaf::biasSurfaceForGeometryGroup(uint groupId)
{
    DENG2_ASSERT(sector);
    if(groupId > sector->planeCount())
        /// @throw InvalidGeometryGroupError Attempted with an invalid geometry group id.
        throw UnknownGeometryGroupError("BspLeaf::biasSurfaceForGeometryGroup", QString("Invalid group id %1").arg(groupId));

    DENG2_ASSERT(bsuf && bsuf[groupId]);
    return *bsuf[groupId];
}

void BspLeaf::updateAABox()
{
    V2d_Set(aaBox.min, DDMAXFLOAT, DDMAXFLOAT);
    V2d_Set(aaBox.max, DDMINFLOAT, DDMINFLOAT);

    if(!hedge) return; // Very odd...

    HEdge *hedgeIt = hedge;
    V2d_InitBox(aaBox.arvec2, hedgeIt->HE_v1origin);

    while((hedgeIt = hedgeIt->next) != hedge)
    {
        V2d_AddToBox(aaBox.arvec2, hedgeIt->HE_v1origin);
    }
}

void BspLeaf::updateMidPoint()
{
    // The middle is the center of our AABox.
    midPoint[VX] = aaBox.minX + (aaBox.maxX - aaBox.minX) / 2;
    midPoint[VY] = aaBox.minY + (aaBox.maxY - aaBox.minY) / 2;
}

void BspLeaf::updateWorldGridOffset()
{
    worldGridOffset[VX] = fmod(aaBox.minX, 64);
    worldGridOffset[VY] = fmod(aaBox.maxY, 64);
}

int BspLeaf::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_SECTOR:
        DMU_GetValue(DMT_BSPLEAF_SECTOR, &sector, &args, 0);
        break;
    case DMU_HEDGE_COUNT: {
        int val = hedgeCount;
        DMU_GetValue(DDVT_INT, &val, &args, 0);
        break; }
    default:
        /// @throw UnknownPropertyError  The requested property does not exist.
        throw UnknownPropertyError("BspLeaf::property", QString("Property '%1' is unknown").arg(DMU_Str(args.prop)));
    }
    return false; // Continue iteration.
}

int BspLeaf::setProperty(setargs_t const &args)
{
    /// @throw WritePropertyError  The requested property is not writable.
    throw WritePropertyError("Vertex::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
}
