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
#include "map/polyobj.h"
#include "map/sector.h"
#include "map/vertex.h"
#include <de/Log>
#include <de/vector1.h>

#include "map/bspleaf.h"

using namespace de;

BspLeaf::BspLeaf() : MapElement(DMU_BSPLEAF)
{
    _hedge = 0;
    _flags = BLF_UPDATE_FANBASE;
    _index = 0;
    _addSpriteCount = 0;
    _validCount = 0;
    _hedgeCount = 0;
    _sector = 0;
    _polyObj = 0;
    _fanBase = 0;
    _shadows = 0;
    std::memset(&_aaBox, 0, sizeof(_aaBox));
    std::memset(_center, 0, sizeof(_center));
    std::memset(_worldGridOffset, 0, sizeof(_worldGridOffset));
    _bsuf = 0;
    std::memset(_reverb, 0, sizeof(_reverb));
}

BspLeaf::~BspLeaf()
{
    if(_bsuf)
    {
#ifdef __CLIENT__
        for(uint i = 0; i < _sector->planeCount(); ++i)
        {
            SB_DestroySurface(_bsuf[i]);
        }
#endif
        Z_Free(_bsuf);
    }

    // Clear the HEdges.
    if(_hedge)
    {
        HEdge *he = _hedge;
        if(he->_next == he)
        {
            delete he;
        }
        else
        {
            // Break the ring, if linked.
            if(he->_prev)
            {
                he->_prev->_next = NULL;
            }

            while(he)
            {
                HEdge *next = he->_next;
                delete he;
                he = next;
            }
        }
    }
}

AABoxd const &BspLeaf::aaBox() const
{
    return _aaBox;
}

void BspLeaf::updateAABox()
{
    V2d_Set(_aaBox.min, DDMAXFLOAT, DDMAXFLOAT);
    V2d_Set(_aaBox.max, DDMINFLOAT, DDMINFLOAT);

    if(!_hedge) return; // Very odd...

    HEdge *hedgeIt = _hedge;
    V2d_InitBox(_aaBox.arvec2, hedgeIt->v1Origin());

    while((hedgeIt = &hedgeIt->next()) != _hedge)
    {
        V2d_AddToBox(_aaBox.arvec2, hedgeIt->v1Origin());
    }
}

vec2d_t const &BspLeaf::center() const
{
    return _center;
}

void BspLeaf::updateCenter()
{
    // The middle is the center of our AABox.
    _center[VX] = _aaBox.minX + (_aaBox.maxX - _aaBox.minX) / 2;
    _center[VY] = _aaBox.minY + (_aaBox.maxY - _aaBox.minY) / 2;
}

HEdge *BspLeaf::firstHEdge() const
{
    return _hedge;
}

uint BspLeaf::hedgeCount() const
{
    return _hedgeCount;
}

bool BspLeaf::hasSector() const
{
    return !!_sector;
}

Sector &BspLeaf::sector() const
{
    if(_sector)
    {
        return *_sector;
    }
    /// @throw MissingSectorError Attempted with no sector attributed.
    throw MissingSectorError("BspLeaf::sector", "No sector is attributed");
}

struct polyobj_s *BspLeaf::firstPolyobj() const
{
    return _polyObj;
}

int BspLeaf::flags() const
{
    return _flags;
}

uint BspLeaf::origIndex() const
{
    return _index;
}

int BspLeaf::addSpriteCount() const
{
    return _addSpriteCount;
}

int BspLeaf::validCount() const
{
    return _validCount;
}

biassurface_t &BspLeaf::biasSurfaceForGeometryGroup(uint groupId)
{
    DENG2_ASSERT(_sector);
    if(groupId > _sector->planeCount())
        /// @throw InvalidGeometryGroupError Attempted with an invalid geometry group id.
        throw UnknownGeometryGroupError("BspLeaf::biasSurfaceForGeometryGroup", QString("Invalid group id %1").arg(groupId));

    DENG2_ASSERT(_bsuf && _bsuf[groupId]);
    return *_bsuf[groupId];
}

HEdge *BspLeaf::fanBase() const
{
    return _fanBase;
}

ShadowLink *BspLeaf::firstShadowLink() const
{
    return _shadows;
}

vec2d_t const &BspLeaf::worldGridOffset() const
{
    return _worldGridOffset;
}

void BspLeaf::updateWorldGridOffset()
{
    _worldGridOffset[VX] = fmod(_aaBox.minX, 64);
    _worldGridOffset[VY] = fmod(_aaBox.maxY, 64);
}

int BspLeaf::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_SECTOR:
        DMU_GetValue(DMT_BSPLEAF_SECTOR, &_sector, &args, 0);
        break;
    case DMU_HEDGE_COUNT: {
        int val = int( _hedgeCount );
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
