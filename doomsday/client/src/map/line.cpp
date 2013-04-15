#/** @file line.cpp World Map Line.
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
#include "map/vertex.h"

#include "map/line.h"

#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif

using namespace de;

Line::Side::Section::Section(Line::Side &side)
    : _surface(dynamic_cast<MapElement &>(side))
{
    std::memset(&_soundEmitter, 0, sizeof(_soundEmitter));
}

Surface &Line::Side::Section::surface() {
    return _surface;
}

Surface const &Line::Side::Section::surface() const {
    return const_cast<Surface const &>(const_cast<Section &>(*this).surface());
}

ddmobj_base_t &Line::Side::Section::soundEmitter() {
    return _soundEmitter;
}

ddmobj_base_t const &Line::Side::Section::soundEmitter() const {
    return const_cast<ddmobj_base_t const &>(const_cast<Section &>(*this).soundEmitter());
}

Line::Side::Side(Line &line, Sector *sector)
    : MapElement(DMU_SIDE),
      _flags(0),
      _line(line),
      _sector(sector),
      _sections(0),
      _sideDefArchiveIndex(0), // no-index
      _leftHEdge(0),
      _rightHEdge(0),
      _shadowVisCount(0)
{
#ifdef __CLIENT__
    _fakeRadioData.updateCount = 0;
    std::memset(_fakeRadioData.topCorners,    0, sizeof(_fakeRadioData.topCorners));
    std::memset(_fakeRadioData.bottomCorners, 0, sizeof(_fakeRadioData.bottomCorners));
    std::memset(_fakeRadioData.sideCorners,   0, sizeof(_fakeRadioData.sideCorners));
    std::memset(_fakeRadioData.spans,         0, sizeof(_fakeRadioData.spans));
#endif
}

Line::Side::~Side()
{
    delete _sections;
}

Line &Line::Side::line()
{
    return _line;
}

Line const &Line::Side::line() const
{
    return const_cast<Line const &>(const_cast<Side *>(this)->line());
}

bool Line::Side::isFront() const
{
    return &_line.front() == this;
}

bool Line::Side::hasSector() const
{
    return _sector != 0;
}

Sector &Line::Side::sector() const
{
    if(_sector)
    {
        return *_sector;
    }
    /// @throw Line::MissingSectorError Attempted with no sector attributed.
    throw Line::MissingSectorError("Line::Side::sector", "No sector is attributed");
}

bool Line::Side::hasSections() const
{
    return _sections != 0;
}

void Line::Side::addSections()
{
    // Already defined?
    if(_sections) return;

    _sections = new Sections(*this);
}

void Line::Side::setSideDefArchiveIndex(uint newIndex)
{
    _sideDefArchiveIndex = newIndex;
}

Line::Side::Section &Line::Side::section(SideSection sectionId)
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
    /// @throw Line::InvalidSectionIdError The given section identifier is not valid.
    throw Line::InvalidSectionIdError("Line::Side::section", QString("Invalid section id %1").arg(sectionId));
}

Line::Side::Section const &Line::Side::section(SideSection sectionId) const
{
    return const_cast<Section const &>(const_cast<Side *>(this)->section(sectionId));
}

HEdge *Line::Side::leftHEdge() const
{
    return _leftHEdge;
}

void Line::Side::setLeftHEdge(HEdge *newLeftHEdge)
{
    _leftHEdge = newLeftHEdge;
}

void Line::Side::setRightHEdge(HEdge *newRightHEdge)
{
    _rightHEdge = newRightHEdge;
}

HEdge *Line::Side::rightHEdge() const
{
    return _rightHEdge;
}

void Line::Side::updateMiddleSoundEmitterOrigin()
{
    LOG_AS("Line::Side::updateMiddleSoundEmitterOrigin");

    if(!_sections) return;

    _sections->middle._soundEmitter.origin[VX] = (_line.v1Origin()[VX] + _line.v2Origin()[VX]) / 2;
    _sections->middle._soundEmitter.origin[VY] = (_line.v1Origin()[VY] + _line.v2Origin()[VY]) / 2;

    DENG_ASSERT(_sector != 0);
    coord_t const ffloor = _sector->floor().height();
    coord_t const fceil  = _sector->ceiling().height();

    if(!_line.hasBackSections() || _line.isSelfReferencing())
        _sections->middle._soundEmitter.origin[VZ] = (ffloor + fceil) / 2;
    else
        _sections->middle._soundEmitter.origin[VZ] = (de::max(ffloor, _line.backSector().floor().height()) +
                                                      de::min(fceil,  _line.backSector().ceiling().height())) / 2;
}

void Line::Side::updateBottomSoundEmitterOrigin()
{
    LOG_AS("Line::Side::updateBottomSoundEmitterOrigin");

    if(!_sections) return;

    _sections->bottom._soundEmitter.origin[VX] = (_line.v1Origin()[VX] + _line.v2Origin()[VX]) / 2;
    _sections->bottom._soundEmitter.origin[VY] = (_line.v1Origin()[VY] + _line.v2Origin()[VY]) / 2;

    DENG_ASSERT(_sector != 0);
    coord_t const ffloor = _sector->floor().height();
    coord_t const fceil  = _sector->ceiling().height();

    if(!_line.hasBackSections() || _line.isSelfReferencing() ||
       _line.backSector().floor().height() <= ffloor)
    {
        _sections->bottom._soundEmitter.origin[VZ] = ffloor;
    }
    else
    {
        _sections->bottom._soundEmitter.origin[VZ] = (de::min(_line.backSector().floor().height(), fceil) + ffloor) / 2;
    }
}

void Line::Side::updateTopSoundEmitterOrigin()
{
    LOG_AS("Line::Side::updateTopSoundEmitterOrigin");

    if(!_sections) return;

    _sections->top._soundEmitter.origin[VX] = (_line.v1Origin()[VX] + _line.v2Origin()[VX]) / 2;
    _sections->top._soundEmitter.origin[VY] = (_line.v1Origin()[VY] + _line.v2Origin()[VY]) / 2;

    DENG_ASSERT(_sector != 0);
    coord_t const ffloor = _sector->floor().height();
    coord_t const fceil  = _sector->ceiling().height();

    if(!_line.hasBackSections() || _line.isSelfReferencing() ||
       _line.backSector().ceiling().height() >= fceil)
    {
        _sections->top._soundEmitter.origin[VZ] = fceil;
    }
    else
    {
        _sections->top._soundEmitter.origin[VZ] = (de::max(_line.backSector().ceiling().height(), ffloor) + fceil) / 2;
    }
}

void Line::Side::updateAllSoundEmitterOrigins()
{
    if(!_sections) return;

    updateMiddleSoundEmitterOrigin();
    updateBottomSoundEmitterOrigin();
    updateTopSoundEmitterOrigin();
}

void Line::Side::updateSurfaceNormals()
{
    if(!_sections) return;

    int edge = isFront()? 0 : 1;

    Vector3f normal((_line.vertexOrigin(edge^1)[VY] - _line.vertexOrigin(edge  )[VY]) / _line.length(),
                    (_line.vertexOrigin(edge  )[VX] - _line.vertexOrigin(edge^1)[VX]) / _line.length(),
                    0);

    // All line side surfaces have the same normals.
    middle().setNormal(normal); // will normalize
    bottom().setNormal(normal);
    top().setNormal(normal);
}

#ifdef __CLIENT__

Line::Side::FakeRadioData &Line::Side::fakeRadioData()
{
    return _fakeRadioData;
}

Line::Side::FakeRadioData const &Line::Side::fakeRadioData() const
{
    return const_cast<FakeRadioData const &>(const_cast<Side *>(this)->fakeRadioData());
}

#endif // __CLIENT__

short Line::Side::flags() const
{
    return _flags;
}

int Line::Side::shadowVisCount() const
{
    return _shadowVisCount;
}

void Line::Side::setShadowVisCount(int newCount)
{
    _shadowVisCount = newCount;
}

int Line::Side::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_SECTOR: {
        Sector *sector = sectorPtr();
        DMU_GetValue(DMT_LINESIDE_SECTOR, &sector, &args, 0);
        break; }
    case DMU_LINE: {
        Line *line = &_line;
        DMU_GetValue(DMT_LINESIDE_LINE, &line, &args, 0);
        break; }
    case DMU_FLAGS:
        DMU_GetValue(DMT_LINESIDE_FLAGS, &_flags, &args, 0);
        break;
    default:
        /// @throw UnknownPropertyError  The requested property does not exist.
        throw UnknownPropertyError("Line::Side::property", QString("Property '%1' is unknown").arg(DMU_Str(args.prop)));
    }
    return false; // Continue iteration.
}

int Line::Side::setProperty(setargs_t const &args)
{
    switch(args.prop)
    {
    case DMU_FLAGS: {
        short newFlags = _flags;
        DMU_SetValue(DMT_LINESIDE_FLAGS, &newFlags, &args, 0);
        _flags = newFlags;
        break; }

    /*case DMU_LINE: {
        Line *line = &_line;
        DMU_SetValue(DMT_LINESIDE_LINE, &line, &args, 0);
        break; }*/

    default:
        /// @throw WritePropertyError  The requested property is not writable.
        throw WritePropertyError("Line::Side::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
    }
    return false; // Continue iteration.
}

DENG2_PIMPL(Line)
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
          front(*i, frontSector),
          back(*i, backSector),
          origIndex(0),
          validCount(0)
    {
        std::memset(mapped, 0, sizeof(mapped));
    }
};

Line::Line(Vertex &from, Vertex &to, int flags, Sector *frontSector, Sector *backSector)
    : MapElement(DMU_LINE),
      _vo1(0),
      _vo2(0),
      _flags(flags),
      _inFlags(0),
      d(new Instance(this, from, to, frontSector, backSector))
{
    updateAABox();
}

int Line::flags() const
{
    return _flags;
}

uint Line::origIndex() const
{
    return d->origIndex;
}

void Line::setOrigIndex(uint newIndex)
{
    d->origIndex = newIndex;
}

bool Line::isBspWindow() const
{
    return (_inFlags & LF_BSPWINDOW) != 0;
}

bool Line::isFromPolyobj() const
{
    return (_inFlags & LF_POLYOBJ) != 0;
}

Line::Side &Line::side(int back)
{
    return back? d->back : d->front;
}

Line::Side const &Line::side(int back) const
{
    return back? d->back : d->front;
}

Vertex &Line::vertex(int to)
{
    DENG_ASSERT((to? d->to : d->from) != 0);
    return to? *d->to : *d->from;
}

Vertex const &Line::vertex(int to) const
{
    DENG_ASSERT((to? d->to : d->from) != 0);
    return to? *d->to : *d->from;
}

void Line::replaceVertex(int to, Vertex &newVertex)
{
    if(to) d->to   = &newVertex;
    else   d->from = &newVertex;
}

LineOwner *Line::vertexOwner(int to) const
{
    DENG_ASSERT((to? _vo2 : _vo1) != 0);
    return to? _vo2 : _vo1;
}

AABoxd const &Line::aaBox() const
{
    return d->aaBox;
}

void Line::updateAABox()
{
    V2d_InitBox(d->aaBox.arvec2, d->from->origin());
    V2d_AddToBox(d->aaBox.arvec2, d->to->origin());
}

coord_t Line::length() const
{
    return d->length;
}

Vector2d const &Line::direction() const
{
    return d->direction;
}

slopetype_t Line::slopeType() const
{
    return d->slopeType;
}

void Line::updateSlopeType()
{
    d->direction = Vector2d(d->to->origin()) - Vector2d(d->from->origin());
    d->angle     = bamsAtan2(int( d->direction.y ), int( d->direction.x ));
    d->slopeType = M_SlopeTypeXY(d->direction.x, d->direction.y);
}

binangle_t Line::angle() const
{
    return d->angle;
}

int Line::boxOnSide(AABoxd const &box) const
{
    coord_t v1Direction[2] = { direction().x, direction().y };
    return M_BoxOnLineSide(&box, d->from->origin(), v1Direction);
}

int Line::boxOnSide_FixedPrecision(AABoxd const &box) const
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

bool Line::isMappedByPlayer(int playerNum) const
{
    DENG2_ASSERT(playerNum >= 0 && playerNum < DDMAXPLAYERS);
    return d->mapped[playerNum];
}

void Line::markMappedByPlayer(int playerNum, bool yes)
{
    DENG2_ASSERT(playerNum >= 0 && playerNum < DDMAXPLAYERS);
    d->mapped[playerNum] = yes;
}

int Line::validCount() const
{
    return d->validCount;
}

void Line::setValidCount(int newValidCount)
{
    d->validCount = newValidCount;
}

int Line::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_VERTEX0:
        DMU_GetValue(DMT_LINE_V, &d->from, &args, 0);
        break;
    case DMU_VERTEX1:
        DMU_GetValue(DMT_LINE_V, &d->to, &args, 0);
        break;
    case DMU_DX:
        DMU_GetValue(DMT_LINE_DX, &d->direction.x, &args, 0);
        break;
    case DMU_DY:
        DMU_GetValue(DMT_LINE_DY, &d->direction.y, &args, 0);
        break;
    case DMU_DXY:
        DMU_GetValue(DMT_LINE_DX, &d->direction.x, &args, 0);
        DMU_GetValue(DMT_LINE_DY, &d->direction.y, &args, 1);
        break;
    case DMU_LENGTH:
        DMU_GetValue(DMT_LINE_LENGTH, &d->length, &args, 0);
        break;
    case DMU_ANGLE: {
        angle_t lineAngle = BANG_TO_ANGLE(d->angle);
        DMU_GetValue(DDVT_ANGLE, &lineAngle, &args, 0);
        break; }
    case DMU_SLOPETYPE:
        DMU_GetValue(DMT_LINE_SLOPETYPE, &d->slopeType, &args, 0);
        break;
    case DMU_FRONT_SECTOR: {
        Sector const *frontSector = frontSectorPtr();
        DMU_GetValue(DMT_LINE_SECTOR, &frontSector, &args, 0);
        break; }
    case DMU_BACK_SECTOR: {
        Sector const *backSector = backSectorPtr();
        DMU_GetValue(DMT_LINE_SECTOR, &backSector, &args, 0);
        break; }
    case DMU_FLAGS:
        DMU_GetValue(DMT_LINE_FLAGS, &_flags, &args, 0);
        break;
    case DMU_FRONT: {
        /// @todo Update the games so that sides without sections can be returned.
        Line::Side const *frontAdr = hasFrontSections()? &d->front : 0;
        DMU_GetValue(DDVT_PTR, &frontAdr, &args, 0);
        break; }
    case DMU_BACK: {
        /// @todo Update the games so that sides without sections can be returned.
        Line::Side const *backAdr = hasBackSections()? &d->back : 0;
        DMU_GetValue(DDVT_PTR, &backAdr, &args, 0);
        break; }
    case DMU_BOUNDING_BOX:
        if(args.valueType == DDVT_PTR)
        {
            AABoxd const *aaBoxAdr = &d->aaBox;
            DMU_GetValue(DDVT_PTR, &aaBoxAdr, &args, 0);
        }
        else
        {
            DMU_GetValue(DMT_LINE_AABOX, &d->aaBox.minX, &args, 0);
            DMU_GetValue(DMT_LINE_AABOX, &d->aaBox.maxX, &args, 1);
            DMU_GetValue(DMT_LINE_AABOX, &d->aaBox.minY, &args, 2);
            DMU_GetValue(DMT_LINE_AABOX, &d->aaBox.maxY, &args, 3);
        }
        break;
    case DMU_VALID_COUNT:
        DMU_GetValue(DMT_LINE_VALIDCOUNT, &d->validCount, &args, 0);
        break;
    default:
        /// @throw UnknownPropertyError  The requested property does not exist.
        throw UnknownPropertyError("Line::property", QString("Property '%1' is unknown").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}

int Line::setProperty(setargs_t const &args)
{
    /// @todo fixme: Changing the sector and/or side references via the DMU
    /// API should be disabled - it has no concept of what is actually needed
    /// to effect such changes at run time.
    ///
    /// At best this will result in strange glitches but more than likely a
    /// fatal error (or worse) would happen should anyone try to use this...

    switch(args.prop)
    {
    /*case DMU_FRONT_SECTOR: {
        Sector *newFrontSector = frontSectorPtr();
        DMU_SetValue(DMT_LINE_SECTOR, &newFrontSector, &args, 0);
        d->front._sector = newFrontSector;
        break; }
    case DMU_BACK_SECTOR: {
        Sector *newBackSector = backSectorPtr();
        DMU_SetValue(DMT_LINE_SECTOR, &newBackSector, &args, 0);
        d->back._sector = newBackSector;
        break; }
    case DMU_FRONT: {
        SideDef *newFrontSideDef = frontSideDefPtr();
        DMU_SetValue(DMT_LINE_SIDE, &newFrontSideDef, &args, 0);
        d->front._sideDef = newFrontSideDef;
        break; }
    case DMU_BACK: {
        SideDef *newBackSideDef = backSideDefPtr();
        DMU_SetValue(DMT_LINE_SIDE, &newBackSideDef, &args, 0);
        d->back._sideDef = newBackSideDef;
        break; }*/

    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_LINE_VALIDCOUNT, &d->validCount, &args, 0);
        break;
    case DMU_FLAGS: {
#ifdef __CLIENT__
        int oldFlags = _flags;
#endif
        DMU_SetValue(DMT_LINE_FLAGS, &_flags, &args, 0);

#ifdef __CLIENT__
        /// @todo Surface should observe.
        if(hasFrontSections())
        {
            if((_flags & DDLF_DONTPEGTOP) != (oldFlags & DDLF_DONTPEGTOP))
            {
                front().top().markAsNeedingDecorationUpdate();
            }
            if((_flags & DDLF_DONTPEGBOTTOM) != (oldFlags & DDLF_DONTPEGBOTTOM))
            {
                front().bottom().markAsNeedingDecorationUpdate();
            }
        }

#endif // __CLIENT__
        break; }

    default:
        /// @throw WritePropertyError  The requested property is not writable.
        throw WritePropertyError("Line::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}
