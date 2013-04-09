/** @file linedef.h World Map Line.
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

#include <cmath>

#include <de/mathutil.h>
#include <de/Vector>

#include "de_base.h"
#include "m_misc.h"

#include "Materials"
#include "map/sector.h"
#include "map/sidedef.h"
#include "map/vertex.h"

#include "map/linedef.h"

using namespace de;

LineDef::Side::Side()
    : _sector(0),
      _sideDef(0),
      _leftHEdge(0),
      _rightHEdge(0),
      _shadowVisCount(0)
{}

bool LineDef::Side::hasSector() const
{
    return _sector != 0;
}

Sector &LineDef::Side::sector() const
{
    if(_sector)
    {
        return *_sector;
    }
    /// @throw LineDef::MissingSectorError Attempted with no sector attributed.
    throw LineDef::MissingSectorError("LineDef::Side::sector", "No sector is attributed");
}

bool LineDef::Side::hasSideDef() const
{
    return _sideDef != 0;
}

SideDef &LineDef::Side::sideDef() const
{
    if(_sideDef)
    {
        return *_sideDef;
    }
    /// @throw LineDef::MissingSideDefError Attempted with no sidedef configured.
    throw LineDef::MissingSideDefError("LineDef::Side::sideDef", "No sidedef is configured");
}

HEdge &LineDef::Side::leftHEdge() const
{
    DENG_ASSERT(_leftHEdge != 0);
    return *_leftHEdge;
}

HEdge &LineDef::Side::rightHEdge() const
{
    DENG_ASSERT(_rightHEdge != 0);
    return *_rightHEdge;
}

int LineDef::Side::shadowVisCount() const
{
    return _shadowVisCount;
}

void LineDef::Side::updateSoundEmitterOrigins()
{
    if(!_sideDef) return;

    _sideDef->middle().updateSoundEmitterOrigin();
    _sideDef->bottom().updateSoundEmitterOrigin();
    _sideDef->top().updateSoundEmitterOrigin();
}

void LineDef::Side::updateSurfaceNormals()
{
    if(!_sideDef) return;

    LineDef const &line = _sideDef->line();
    byte sid = line.frontSideDefPtr() == _sideDef? FRONT : BACK;

    Surface &middleSurface = _sideDef->middle();
    Surface &bottomSurface = _sideDef->bottom();
    Surface &topSurface    = _sideDef->top();

    Vector3f normal((line.vertexOrigin(sid^1)[VY] - line.vertexOrigin(sid  )[VY]) / line.length(),
                    (line.vertexOrigin(sid  )[VX] - line.vertexOrigin(sid^1)[VX]) / line.length(),
                    0);

    // All line side surfaces have the same normals.
    middleSurface.setNormal(normal); // will normalize
    bottomSurface.setNormal(normal);
    topSurface.setNormal(normal);
}

DENG2_PIMPL(LineDef)
{
    /// Logical line slope (i.e., world angle) classification.
    slopetype_t slopeType;

    /// Bounding box encompassing the map space coordinates of both vertexes.
    AABoxd aaBox;

    /// Logical sides:
    Side front;
    Side back;

    /// Original index in the archived map.
    uint origIndex;

    /// Whether the line has been mapped by each player yet.
    bool mapped[DDMAXPLAYERS];

    Instance(Public *i)
        : Base(i),
          slopeType(ST_HORIZONTAL),
          origIndex(0)
    {
        std::memset(mapped, 0, sizeof(mapped));
    }
};

LineDef::LineDef()
    : MapElement(DMU_LINEDEF),
      _v1(0),
      _v2(0),
      _vo1(0),
      _vo2(0),
      _flags(0),
      _inFlags(0),
      _angle(0),
      _length(0),
      _validCount(0),
      d(new Instance(this))
{
    V2d_Set(_direction, 0, 0);
}

int LineDef::flags() const
{
    return _flags;
}

uint LineDef::origIndex() const
{
    return d->origIndex;
}

void LineDef::setOrigIndex(uint newIndex)
{
    d->origIndex = newIndex;
}

bool LineDef::isBspWindow() const
{
    return (_inFlags & LF_BSPWINDOW) != 0;
}

bool LineDef::isFromPolyobj() const
{
    return (_inFlags & LF_POLYOBJ) != 0;
}

LineDef::Side &LineDef::side(int back)
{
    return back? d->back : d->front;
}

LineDef::Side const &LineDef::side(int back) const
{
    return back? d->back : d->front;
}

Vertex &LineDef::vertex(int to)
{
    DENG_ASSERT((to? _v2 : _v1) != 0);
    return to? *_v2 : *_v1;
}

Vertex const &LineDef::vertex(int to) const
{
    DENG_ASSERT((to? _v2 : _v1) != 0);
    return to? *_v2 : *_v1;
}

LineOwner *LineDef::vertexOwner(int to) const
{
    DENG_ASSERT((to? _vo2 : _vo1) != 0);
    return to? _vo2 : _vo1;
}

AABoxd const &LineDef::aaBox() const
{
    return d->aaBox;
}

void LineDef::updateAABox()
{
    V2d_InitBox(d->aaBox.arvec2, _v1->origin());
    V2d_AddToBox(d->aaBox.arvec2, _v2->origin());
}

coord_t LineDef::length() const
{
    return _length;
}

const_pvec2d_t &LineDef::direction() const
{
    return _direction;
}

slopetype_t LineDef::slopeType() const
{
    return d->slopeType;
}

void LineDef::updateSlopeType()
{
    V2d_Subtract(_direction, _v2->origin(), _v1->origin());
    d->slopeType = M_SlopeType(_direction);
}

binangle_t LineDef::angle() const
{
    return _angle;
}

int LineDef::boxOnSide(AABoxd const &box) const
{
    return M_BoxOnLineSide(&box, _v1->origin(), _direction);
}

int LineDef::boxOnSide_FixedPrecision(AABoxd const &box) const
{
    /*
     * Apply an offset to both the box and the line to bring everything into
     * the 16.16 fixed-point range. We'll use the midpoint of the line as the
     * origin, as typically this test is called when a bounding box is
     * somewhere in the vicinity of the line. The offset is floored to integers
     * so we won't change the discretization of the fractional part into 16-bit
     * precision.
     */
    coord_t offset[2] = { de::floor(_v1->origin()[VX] + _direction[VX]/2),
                          de::floor(_v1->origin()[VY] + _direction[VY]/2) };

    fixed_t boxx[4];
    boxx[BOXLEFT]   = FLT2FIX(box.minX - offset[VX]);
    boxx[BOXRIGHT]  = FLT2FIX(box.maxX - offset[VX]);
    boxx[BOXBOTTOM] = FLT2FIX(box.minY - offset[VY]);
    boxx[BOXTOP]    = FLT2FIX(box.maxY - offset[VY]);

    fixed_t pos[2] = { FLT2FIX(_v1->origin()[VX] - offset[VX]),
                       FLT2FIX(_v1->origin()[VY] - offset[VY]) };

    fixed_t delta[2] = { FLT2FIX(_direction[VX]),
                         FLT2FIX(_direction[VY]) };

    return M_BoxOnLineSide_FixedPrecision(boxx, pos, delta);
}

bool LineDef::isMappedByPlayer(int playerNum) const
{
    DENG2_ASSERT(playerNum >= 0 && playerNum < DDMAXPLAYERS);
    return d->mapped[playerNum];
}

void LineDef::markMappedByPlayer(int playerNum, bool yes)
{
    DENG2_ASSERT(playerNum >= 0 && playerNum < DDMAXPLAYERS);
    d->mapped[playerNum] = yes;
}

int LineDef::validCount() const
{
    return _validCount;
}

int LineDef::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_VERTEX0:
        DMU_GetValue(DMT_LINEDEF_V, &_v1, &args, 0);
        break;
    case DMU_VERTEX1:
        DMU_GetValue(DMT_LINEDEF_V, &_v2, &args, 0);
        break;
    case DMU_DX:
        DMU_GetValue(DMT_LINEDEF_DX, &_direction[VX], &args, 0);
        break;
    case DMU_DY:
        DMU_GetValue(DMT_LINEDEF_DY, &_direction[VY], &args, 0);
        break;
    case DMU_DXY:
        DMU_GetValue(DMT_LINEDEF_DX, &_direction[VX], &args, 0);
        DMU_GetValue(DMT_LINEDEF_DY, &_direction[VY], &args, 1);
        break;
    case DMU_LENGTH:
        DMU_GetValue(DMT_LINEDEF_LENGTH, &_length, &args, 0);
        break;
    case DMU_ANGLE: {
        angle_t lineAngle = BANG_TO_ANGLE(_angle);
        DMU_GetValue(DDVT_ANGLE, &lineAngle, &args, 0);
        break; }
    case DMU_SLOPETYPE:
        DMU_GetValue(DMT_LINEDEF_SLOPETYPE, &d->slopeType, &args, 0);
        break;
    case DMU_FRONT_SECTOR: {
        Sector const *frontSector = frontSectorPtr();
        DMU_GetValue(DMT_LINEDEF_SECTOR, &frontSector, &args, 0);
        break; }
    case DMU_BACK_SECTOR: {
        Sector const *backSector = backSectorPtr();
        DMU_GetValue(DMT_LINEDEF_SECTOR, &backSector, &args, 0);
        break; }
    case DMU_FLAGS:
        DMU_GetValue(DMT_LINEDEF_FLAGS, &_flags, &args, 0);
        break;
    case DMU_SIDEDEF0: {
        SideDef const *frontSideDef = frontSideDefPtr();
        DMU_GetValue(DDVT_PTR, &frontSideDef, &args, 0);
        break; }
    case DMU_SIDEDEF1: {
        SideDef const *backSideDef = backSideDefPtr();
        DMU_GetValue(DDVT_PTR, &backSideDef, &args, 0);
        break; }
    case DMU_BOUNDING_BOX:
        if(args.valueType == DDVT_PTR)
        {
            AABoxd const *aaBoxAdr = &d->aaBox;
            DMU_GetValue(DDVT_PTR, &aaBoxAdr, &args, 0);
        }
        else
        {
            DMU_GetValue(DMT_LINEDEF_AABOX, &d->aaBox.minX, &args, 0);
            DMU_GetValue(DMT_LINEDEF_AABOX, &d->aaBox.maxX, &args, 1);
            DMU_GetValue(DMT_LINEDEF_AABOX, &d->aaBox.minY, &args, 2);
            DMU_GetValue(DMT_LINEDEF_AABOX, &d->aaBox.maxY, &args, 3);
        }
        break;
    case DMU_VALID_COUNT:
        DMU_GetValue(DMT_LINEDEF_VALIDCOUNT, &_validCount, &args, 0);
        break;
    default:
        /// @throw UnknownPropertyError  The requested property does not exist.
        throw UnknownPropertyError("LineDef::property", QString("Property '%1' is unknown").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}

int LineDef::setProperty(setargs_t const &args)
{
    /// @todo fixme: Changing the sector and/or sidedef references via the DMU
    /// API should be disabled - it has no concept of what is actually needed
    /// to effect such changes at run time.
    ///
    /// At best this will result in strange glitches but more than likely a
    /// fatal error (or worse) would happen should anyone try to use this...

    switch(args.prop)
    {
    case DMU_FRONT_SECTOR: {
        Sector *newFrontSector = frontSectorPtr();
        DMU_SetValue(DMT_LINEDEF_SECTOR, &newFrontSector, &args, 0);
        d->front._sector = newFrontSector;
        break; }
    case DMU_BACK_SECTOR: {
        Sector *newBackSector = backSectorPtr();
        DMU_SetValue(DMT_LINEDEF_SECTOR, &newBackSector, &args, 0);
        d->back._sector = newBackSector;
        break; }
    case DMU_SIDEDEF0: {
        SideDef *newFrontSideDef = frontSideDefPtr();
        DMU_SetValue(DMT_LINEDEF_SIDEDEF, &newFrontSideDef, &args, 0);
        d->front._sideDef = newFrontSideDef;
        break; }
    case DMU_SIDEDEF1: {
        SideDef *newBackSideDef = backSideDefPtr();
        DMU_SetValue(DMT_LINEDEF_SIDEDEF, &newBackSideDef, &args, 0);
        d->back._sideDef = newBackSideDef;
        break; }

    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_LINEDEF_VALIDCOUNT, &_validCount, &args, 0);
        break;
    case DMU_FLAGS:
        DMU_SetValue(DMT_LINEDEF_FLAGS, &_flags, &args, 0);

#ifdef __CLIENT__
        /// @todo Surface should observe.
        if(hasFrontSideDef())
        {
            SideDef &frontDef = frontSideDef();
            frontDef.top().markAsNeedingDecorationUpdate();
            frontDef.bottom().markAsNeedingDecorationUpdate();
            frontDef.middle().markAsNeedingDecorationUpdate();
        }

        if(hasBackSideDef())
        {
            SideDef &backDef = backSideDef();
            backDef.top().markAsNeedingDecorationUpdate();
            backDef.bottom().markAsNeedingDecorationUpdate();
            backDef.middle().markAsNeedingDecorationUpdate();
        }
#endif // __CLIENT__
        break;

    default:
        /// @throw WritePropertyError  The requested property is not writable.
        throw WritePropertyError("LineDef::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}
