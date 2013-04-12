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

#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif

using namespace de;

LineDef::Side::Side(Sector *sector)
    : _sector(sector),
      _sections(0),
      _sideDef(0),
      _leftHEdge(0),
      _rightHEdge(0),
      _shadowVisCount(0),
      _flags(0)
{
#ifdef __CLIENT__
    _fakeRadioData.updateCount = 0;
    std::memset(_fakeRadioData.topCorners,    0, sizeof(_fakeRadioData.topCorners));
    std::memset(_fakeRadioData.bottomCorners, 0, sizeof(_fakeRadioData.bottomCorners));
    std::memset(_fakeRadioData.sideCorners,   0, sizeof(_fakeRadioData.sideCorners));
    std::memset(_fakeRadioData.spans,         0, sizeof(_fakeRadioData.spans));
#endif
}

LineDef::Side::~Side()
{
    delete _sections;
}

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

LineDef::Side::Section &LineDef::Side::section(SideDefSection sectionId)
{
    if(_sections)
    {
        switch(sectionId)
        {
        case SS_MIDDLE: return _sections->middle;
        case SS_BOTTOM: return _sections->bottom;
        case SS_TOP:    return _sections->top;
        default:        break;
        }
    }
    /// @throw LineDef::InvalidSectionIdError The given section identifier is not valid.
    throw LineDef::InvalidSectionIdError("LineDef::Side::section", QString("Invalid section id %1").arg(sectionId));
}

LineDef::Side::Section const &LineDef::Side::section(SideDefSection sectionId) const
{
    return const_cast<Section const &>(const_cast<Side &>(*this).section(sectionId));
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

ddmobj_base_t &LineDef::Side::middleSoundEmitter()
{
    return middle()._soundEmitter;
}

ddmobj_base_t const &LineDef::Side::middleSoundEmitter() const
{
    return const_cast<ddmobj_base_t const &>(const_cast<Side &>(*this).middleSoundEmitter());
}

void LineDef::Side::updateMiddleSoundEmitterOrigin()
{
    LOG_AS("LineDef::Side::updateMiddleSoundEmitterOrigin");

    if(!_sideDef) return;

    LineDef &line = _sideDef->line();

    middle()._soundEmitter.origin[VX] = (line.v1Origin()[VX] + line.v2Origin()[VX]) / 2;
    middle()._soundEmitter.origin[VY] = (line.v1Origin()[VY] + line.v2Origin()[VY]) / 2;

    DENG_ASSERT(_sector != 0);
    coord_t const ffloor = _sector->floor().height();
    coord_t const fceil  = _sector->ceiling().height();

    if(!line.hasBackSideDef() || line.isSelfReferencing())
        middle()._soundEmitter.origin[VZ] = (ffloor + fceil) / 2;
    else
        middle()._soundEmitter.origin[VZ] = (de::max(ffloor, line.backSector().floor().height()) +
                                             de::min(fceil,  line.backSector().ceiling().height())) / 2;
}

ddmobj_base_t &LineDef::Side::bottomSoundEmitter()
{
    return bottom()._soundEmitter;
}

ddmobj_base_t const &LineDef::Side::bottomSoundEmitter() const
{
    return const_cast<ddmobj_base_t const &>(const_cast<Side &>(*this).bottomSoundEmitter());
}

void LineDef::Side::updateBottomSoundEmitterOrigin()
{
    LOG_AS("LineDef::Side::updateBottomSoundEmitterOrigin");

    if(!_sideDef) return;

    LineDef &line = _sideDef->line();

    bottom()._soundEmitter.origin[VX] = (line.v1Origin()[VX] + line.v2Origin()[VX]) / 2;
    bottom()._soundEmitter.origin[VY] = (line.v1Origin()[VY] + line.v2Origin()[VY]) / 2;

    DENG_ASSERT(_sector != 0);
    coord_t const ffloor = _sector->floor().height();
    coord_t const fceil  = _sector->ceiling().height();

    if(!line.hasBackSideDef() || line.isSelfReferencing() ||
       line.backSector().floor().height() <= ffloor)
    {
        bottom()._soundEmitter.origin[VZ] = ffloor;
    }
    else
    {
        bottom()._soundEmitter.origin[VZ] = (de::min(line.backSector().floor().height(), fceil) + ffloor) / 2;
    }
}

ddmobj_base_t &LineDef::Side::topSoundEmitter()
{
    return top()._soundEmitter;
}

ddmobj_base_t const &LineDef::Side::topSoundEmitter() const
{
    return const_cast<ddmobj_base_t const &>(const_cast<Side &>(*this).topSoundEmitter());
}

void LineDef::Side::updateTopSoundEmitterOrigin()
{
    LOG_AS("LineDef::Side::updateTopSoundEmitterOrigin");

    if(!_sideDef) return;

    LineDef &line = _sideDef->line();

    top()._soundEmitter.origin[VX] = (line.v1Origin()[VX] + line.v2Origin()[VX]) / 2;
    top()._soundEmitter.origin[VY] = (line.v1Origin()[VY] + line.v2Origin()[VY]) / 2;

    DENG_ASSERT(_sector != 0);
    coord_t const ffloor = _sector->floor().height();
    coord_t const fceil  = _sector->ceiling().height();

    if(!line.hasBackSideDef() || line.isSelfReferencing() ||
       line.backSector().ceiling().height() >= fceil)
    {
        top()._soundEmitter.origin[VZ] = fceil;
    }
    else
    {
        top()._soundEmitter.origin[VZ] = (de::max(line.backSector().ceiling().height(), ffloor) + fceil) / 2;
    }
}

void LineDef::Side::updateAllSoundEmitterOrigins()
{
    if(!_sideDef) return;

    updateMiddleSoundEmitterOrigin();
    updateBottomSoundEmitterOrigin();
    updateTopSoundEmitterOrigin();
}

void LineDef::Side::updateSurfaceNormals()
{
    if(!_sideDef) return;

    LineDef const &line = _sideDef->line();
    byte sid = &line.front() == this? FRONT : BACK;

    Vector3f normal((line.vertexOrigin(sid^1)[VY] - line.vertexOrigin(sid  )[VY]) / line.length(),
                    (line.vertexOrigin(sid  )[VX] - line.vertexOrigin(sid^1)[VX]) / line.length(),
                    0);

    // All line side surfaces have the same normals.
    middle().surface().setNormal(normal); // will normalize
    bottom().surface().setNormal(normal);
    top().surface().setNormal(normal);
}

#ifdef __CLIENT__

LineDef::Side::FakeRadioData &LineDef::Side::fakeRadioData()
{
    return _fakeRadioData;
}

LineDef::Side::FakeRadioData const &LineDef::Side::fakeRadioData() const
{
    return const_cast<FakeRadioData const &>(const_cast<Side *>(this)->fakeRadioData());
}

#endif // __CLIENT__

short LineDef::Side::flags() const
{
    return _flags;
}

int LineDef::Side::shadowVisCount() const
{
    return _shadowVisCount;
}

DENG2_PIMPL(LineDef)
{
    /// Vertexes:
    Vertex *from;
    Vertex *to;

    /// Direction vector from -> to.
    Vector2d direction;

    /// Calculated from the direction vector.
    binangle_t angle;

    /// Logical line slope (i.e., world angle) classification.
    slopetype_t slopeType;

    /// Accurate length.
    coord_t length;

    /// Bounding box encompassing the map space coordinates of both vertexes.
    AABoxd aaBox;

    /// Logical sides:
    Side front;
    Side back;

    /// Original index in the archived map.
    uint origIndex;

    /// Used by legacy algorithms to prevent repeated processing.
    int validCount;

    /// Whether the line has been mapped by each player yet.
    bool mapped[DDMAXPLAYERS];

    Instance(Public *i, Vertex &from, Vertex &to,
             Sector *frontSector, Sector *backSector)
        : Base(i),
          from(&from),
          to(&to),
          direction(Vector2d(to.origin()) - Vector2d(from.origin())),
          angle(bamsAtan2(int( direction.y ), int( direction.x ))),
          slopeType(M_SlopeTypeXY(direction.x, direction.y)),
          length(direction.length()),
          front(frontSector),
          back(backSector),
          origIndex(0),
          validCount(0)
    {
        std::memset(mapped, 0, sizeof(mapped));
    }
};

LineDef::LineDef(Vertex &from, Vertex &to, Sector *frontSector, Sector *backSector)
    : MapElement(DMU_LINEDEF),
      _vo1(0),
      _vo2(0),
      _flags(0),
      _inFlags(0),
      d(new Instance(this, from, to, frontSector, backSector))
{
    updateAABox();
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
    DENG_ASSERT((to? d->to : d->from) != 0);
    return to? *d->to : *d->from;
}

Vertex const &LineDef::vertex(int to) const
{
    DENG_ASSERT((to? d->to : d->from) != 0);
    return to? *d->to : *d->from;
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
    V2d_InitBox(d->aaBox.arvec2, d->from->origin());
    V2d_AddToBox(d->aaBox.arvec2, d->to->origin());
}

coord_t LineDef::length() const
{
    return d->length;
}

Vector2d const &LineDef::direction() const
{
    return d->direction;
}

slopetype_t LineDef::slopeType() const
{
    return d->slopeType;
}

void LineDef::updateSlopeType()
{
    d->direction = Vector2d(d->to->origin()) - Vector2d(d->from->origin());
    d->angle     = bamsAtan2(int( d->direction.y ), int( d->direction.x ));
    d->slopeType = M_SlopeTypeXY(d->direction.x, d->direction.y);
}

binangle_t LineDef::angle() const
{
    return d->angle;
}

int LineDef::boxOnSide(AABoxd const &box) const
{
    coord_t v1Direction[2] = { direction().x, direction().y };
    return M_BoxOnLineSide(&box, d->from->origin(), v1Direction);
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
    coord_t offset[2] = { de::floor(d->from->origin()[VX] + d->direction.x/2),
                          de::floor(d->from->origin()[VY] + d->direction.y/2) };

    fixed_t boxx[4];
    boxx[BOXLEFT]   = FLT2FIX(box.minX - offset[VX]);
    boxx[BOXRIGHT]  = FLT2FIX(box.maxX - offset[VX]);
    boxx[BOXBOTTOM] = FLT2FIX(box.minY - offset[VY]);
    boxx[BOXTOP]    = FLT2FIX(box.maxY - offset[VY]);

    fixed_t pos[2] = { FLT2FIX(d->from->origin()[VX] - offset[VX]),
                       FLT2FIX(d->from->origin()[VY] - offset[VY]) };

    fixed_t delta[2] = { FLT2FIX(d->direction.x),
                         FLT2FIX(d->direction.y) };

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
    return d->validCount;
}

void LineDef::setValidCount(int newValidCount)
{
    d->validCount = newValidCount;
}

int LineDef::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_VERTEX0:
        DMU_GetValue(DMT_LINEDEF_V, &d->from, &args, 0);
        break;
    case DMU_VERTEX1:
        DMU_GetValue(DMT_LINEDEF_V, &d->to, &args, 0);
        break;
    case DMU_DX:
        DMU_GetValue(DMT_LINEDEF_DX, &d->direction.x, &args, 0);
        break;
    case DMU_DY:
        DMU_GetValue(DMT_LINEDEF_DY, &d->direction.y, &args, 0);
        break;
    case DMU_DXY:
        DMU_GetValue(DMT_LINEDEF_DX, &d->direction.x, &args, 0);
        DMU_GetValue(DMT_LINEDEF_DY, &d->direction.y, &args, 1);
        break;
    case DMU_LENGTH:
        DMU_GetValue(DMT_LINEDEF_LENGTH, &d->length, &args, 0);
        break;
    case DMU_ANGLE: {
        angle_t lineAngle = BANG_TO_ANGLE(d->angle);
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
        DMU_GetValue(DMT_LINEDEF_VALIDCOUNT, &d->validCount, &args, 0);
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
        DMU_SetValue(DMT_LINEDEF_VALIDCOUNT, &d->validCount, &args, 0);
        break;
    case DMU_FLAGS: {
#ifdef __CLIENT__
        int oldFlags = _flags;
#endif
        DMU_SetValue(DMT_LINEDEF_FLAGS, &_flags, &args, 0);

#ifdef __CLIENT__
        /// @todo Surface should observe.
        if(hasFrontSideDef())
        {
            if((_flags & DDLF_DONTPEGTOP) != (oldFlags & DDLF_DONTPEGTOP))
            {
                front().top().surface().markAsNeedingDecorationUpdate();
            }
            if((_flags & DDLF_DONTPEGBOTTOM) != (oldFlags & DDLF_DONTPEGBOTTOM))
            {
                front().bottom().surface().markAsNeedingDecorationUpdate();
            }
        }

#endif // __CLIENT__
        break; }

    default:
        /// @throw WritePropertyError  The requested property is not writable.
        throw WritePropertyError("LineDef::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}
