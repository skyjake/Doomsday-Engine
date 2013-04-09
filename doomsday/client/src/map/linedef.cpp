/** @file linedef.h Map LineDef.
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
#include "de_render.h"
#include "m_misc.h"
#include "Materials"

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
    Instance(Public *i) : Base(i) {}

#ifdef __CLIENT__

    /**
     * @param line  LineDef instance.
     * @param ignoreOpacity  @c true= do not consider Material opacity.
     * @return  @c true if this LineDef's side is considered "closed" (i.e.,
     *     there is no opening through which the back Sector can be seen).
     *     Tests consider all Planes which interface with this and the "middle"
     *     Material used on the relative front side (if any).
     */
    bool backClosedForBlendNeighbor(int side, bool ignoreOpacity) const
    {
        if(!self.hasFrontSideDef()) return false;
        if(!self.hasBackSideDef()) return true;

        Sector const *frontSec = self.sectorPtr(side);
        Sector const *backSec  = self.sectorPtr(side^1);
        if(frontSec == backSec) return false; // Never.

        if(frontSec && backSec)
        {
            if(backSec->floor().visHeight()   >= backSec->ceiling().visHeight())  return true;
            if(backSec->ceiling().visHeight() <= frontSec->floor().visHeight())   return true;
            if(backSec->floor().visHeight()   >= frontSec->ceiling().visHeight()) return true;
        }

        return R_MiddleMaterialCoversLineOpening(&self, side, ignoreOpacity);
    }

    LineDef *findBlendNeighbor(byte side, byte right, binangle_t *diff) const
    {
        LineOwner const *farVertOwner = self.vertexOwner(right^side);
        if(backClosedForBlendNeighbor(side, true/*ignore opacity*/))
        {
            return R_FindSolidLineNeighbor(self.sectorPtr(side), &self, farVertOwner, right, diff);
        }
        return R_FindLineNeighbor(self.sectorPtr(side), &self, farVertOwner, right, diff);
    }

#endif // __CLIENT__
};

LineDef::LineDef()
    : MapElement(DMU_LINEDEF),
      _v1(0),
      _v2(0),
      _vo1(0),
      _vo2(0),
      _flags(0),
      _inFlags(0),
      _slopeType(ST_HORIZONTAL),
      _validCount(0),
      _angle(0),
      _length(0),
      _origIndex(0),
      d(new Instance(this))
{
    V2d_Set(_direction, 0, 0);
    std::memset(_mapped, 0, sizeof(_mapped));
}

int LineDef::flags() const
{
    return _flags;
}

uint LineDef::origIndex() const
{
    return _origIndex;
}

int LineDef::validCount() const
{
    return _validCount;
}

bool LineDef::mappedByPlayer(int playerNum) const
{
    DENG2_ASSERT(playerNum >= 0 && playerNum < DDMAXPLAYERS);
    return _mapped[playerNum];
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
    return back? _back : _front;
}

LineDef::Side const &LineDef::side(int back) const
{
    return back? _back : _front;
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

binangle_t LineDef::angle() const
{
    return _angle;
}

const_pvec2d_t &LineDef::direction() const
{
    return _direction;
}

slopetype_t LineDef::slopeType() const
{
    return _slopeType;
}

coord_t LineDef::length() const
{
    return _length;
}

AABoxd const &LineDef::aaBox() const
{
    return _aaBox;
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

void LineDef::configureDivline(divline_t &dl) const
{
    dl.origin[VX]    = FLT2FIX(_v1->origin()[VX]);
    dl.origin[VY]    = FLT2FIX(_v1->origin()[VY]);
    dl.direction[VX] = FLT2FIX(_direction[VX]);
    dl.direction[VY] = FLT2FIX(_direction[VY]);
}

coord_t LineDef::openRange(int side_, coord_t *retBottom, coord_t *retTop) const
{
    return R_OpenRange(side(side_).sectorPtr(), side(side_^1).sectorPtr(), retBottom, retTop);
}

coord_t LineDef::visOpenRange(int side_, coord_t *retBottom, coord_t *retTop) const
{
    return R_VisOpenRange(side(side_).sectorPtr(), side(side_^1).sectorPtr(), retBottom, retTop);
}

void LineDef::configureTraceOpening(TraceOpening &opening) const
{
    if(!hasBackSideDef())
    {
        opening.range = 0;
        return;
    }

    coord_t bottom, top;
    opening.range  = float( openRange(FRONT, &bottom, &top) );
    opening.bottom = float( bottom );
    opening.top    = float( top );

    // Determine the "low floor".
    Sector const &fsec = frontSector();
    Sector const &bsec = backSector();

    if(fsec.floor().height() > bsec.floor().height())
    {
        opening.lowFloor = float( bsec.floor().height() );
    }
    else
    {
        opening.lowFloor = float( fsec.floor().height() );
    }
}

void LineDef::updateSlopeType()
{
    V2d_Subtract(_direction, _v2->origin(), _v1->origin());
    _slopeType = M_SlopeType(_direction);
}

void LineDef::unitVector(pvec2f_t unitvec) const
{
    coord_t len = M_ApproxDistance(_direction[VX], _direction[VY]);
    if(len)
    {
        unitvec[VX] = _direction[VX] / len;
        unitvec[VY] = _direction[VY] / len;
    }
    else
    {
        unitvec[VX] = unitvec[VY] = 0;
    }
}

void LineDef::updateAABox()
{
    V2d_InitBox(_aaBox.arvec2, _v1->origin());
    V2d_AddToBox(_aaBox.arvec2, _v2->origin());
}

#ifdef __CLIENT__
static float calcLightLevelDelta(Vector2f const &normal)
{
    return (1.0f / 255) * (normal.x * 18) * rendLightWallAngle;
}

static Vector2f calcNormal(LineDef const &line, byte side)
{
    return Vector2f((line.vertexOrigin(side^1)[VY] - line.vertexOrigin(side)  [VY]) / line.length(),
                    (line.vertexOrigin(side)  [VX] - line.vertexOrigin(side^1)[VX]) / line.length());
}

void LineDef::lightLevelDelta(int side, float *deltaL, float *deltaR) const
{
    // Disabled?
    if(!(rendLightWallAngle > 0))
    {
        if(deltaL) *deltaL = 0;
        if(deltaR) *deltaR = 0;
        return;
    }

    Vector2f normal = calcNormal(*this, side);
    float delta = calcLightLevelDelta(normal);

    // If smoothing is disabled use this delta for left and right edges.
    // Must forcibly disable smoothing for polyobj linedefs as they have
    // no owner rings.
    if(!rendLightWallAngleSmooth || (_inFlags & LF_POLYOBJ))
    {
        if(deltaL) *deltaL = delta;
        if(deltaR) *deltaR = delta;
        return;
    }

    // Find the left neighbour linedef for which we will calculate the
    // lightlevel delta and then blend with this to produce the value for
    // the left edge. Blend iff the angle between the two linedefs is less
    // than 45 degrees.
    if(deltaL)
    {
        binangle_t diff = 0;
        LineDef *other = d->findBlendNeighbor(side, 0, &diff);
        if(other && INRANGE_OF(diff, BANG_180, BANG_45))
        {
            Vector2f otherNormal = calcNormal(*other, &other->v2() != &vertex(side));

            // Average normals.
            otherNormal += normal;
            otherNormal.x /= 2; otherNormal.y /= 2;

            *deltaL = calcLightLevelDelta(otherNormal);
        }
        else
        {
            *deltaL = delta;
        }
    }

    // Do the same for the right edge but with the right neighbor linedef.
    if(deltaR)
    {
        binangle_t diff = 0;
        LineDef *other = d->findBlendNeighbor(side, 1, &diff);
        if(other && INRANGE_OF(diff, BANG_180, BANG_45))
        {
            Vector2f otherNormal = calcNormal(*other, &other->v1() != &vertex(side^1));

            // Average normals.
            otherNormal += normal;
            otherNormal.x /= 2; otherNormal.y /= 2;

            *deltaR = calcLightLevelDelta(otherNormal);
        }
        else
        {
            *deltaR = delta;
        }
    }
}
#endif

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
        DMU_GetValue(DMT_LINEDEF_SLOPETYPE, &_slopeType, &args, 0);
        break;
    case DMU_FRONT_SECTOR: {
        Sector *frontSector = _front.sectorPtr();
        DMU_GetValue(DMT_LINEDEF_SECTOR, &frontSector, &args, 0);
        break; }
    case DMU_BACK_SECTOR: {
        Sector *backSector = _back.sectorPtr();
        DMU_GetValue(DMT_LINEDEF_SECTOR, &backSector, &args, 0);
        break; }
    case DMU_FLAGS:
        DMU_GetValue(DMT_LINEDEF_FLAGS, &_flags, &args, 0);
        break;
    case DMU_SIDEDEF0: {
        SideDef *frontSideDef = _front.sideDefPtr();
        DMU_GetValue(DDVT_PTR, &frontSideDef, &args, 0);
        break; }
    case DMU_SIDEDEF1: {
        SideDef *backSideDef = _back.sideDefPtr();
        DMU_GetValue(DDVT_PTR, &backSideDef, &args, 0);
        break; }
    case DMU_BOUNDING_BOX:
        if(args.valueType == DDVT_PTR)
        {
            AABoxd const *aaBoxAdr = &_aaBox;
            DMU_GetValue(DDVT_PTR, &aaBoxAdr, &args, 0);
        }
        else
        {
            DMU_GetValue(DMT_LINEDEF_AABOX, &_aaBox.minX, &args, 0);
            DMU_GetValue(DMT_LINEDEF_AABOX, &_aaBox.maxX, &args, 1);
            DMU_GetValue(DMT_LINEDEF_AABOX, &_aaBox.minY, &args, 2);
            DMU_GetValue(DMT_LINEDEF_AABOX, &_aaBox.maxY, &args, 3);
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
    switch(args.prop)
    {
    case DMU_FRONT_SECTOR:
        DMU_SetValue(DMT_LINEDEF_SECTOR, &_front._sector, &args, 0);
        break;
    case DMU_BACK_SECTOR:
        DMU_SetValue(DMT_LINEDEF_SECTOR, &_back._sector, &args, 0);
        break;
    case DMU_SIDEDEF0:
        DMU_SetValue(DMT_LINEDEF_SIDEDEF, &_front._sideDef, &args, 0);
        break;
    case DMU_SIDEDEF1:
        DMU_SetValue(DMT_LINEDEF_SIDEDEF, &_back._sideDef, &args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_LINEDEF_VALIDCOUNT, &_validCount, &args, 0);
        break;
    case DMU_FLAGS:
        DMU_SetValue(DMT_LINEDEF_FLAGS, &_flags, &args, 0);

#ifdef __CLIENT__
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
