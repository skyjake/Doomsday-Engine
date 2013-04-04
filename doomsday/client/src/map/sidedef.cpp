/** @file sidedef.cpp Map SideDef.
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

#include <de/Log>
#include "de_base.h"
#include "map/linedef.h"

#include "map/sidedef.h"

using namespace de;

SideDef::SideDef()
    : MapElement(DMU_SIDEDEF),
      _middleSurface(dynamic_cast<MapElement &>(*this)),
      _bottomSurface(dynamic_cast<MapElement &>(*this)),
      _topSurface(dynamic_cast<MapElement &>(*this))
{
    _line = 0;
    _flags = 0;
    std::memset(&_buildData, 0, sizeof(_buildData));

#ifdef __CLIENT__
    _fakeRadioData.updateCount = 0;
    std::memset(_fakeRadioData.topCorners,    0, sizeof(_fakeRadioData.topCorners));
    std::memset(_fakeRadioData.bottomCorners, 0, sizeof(_fakeRadioData.bottomCorners));
    std::memset(_fakeRadioData.sideCorners,   0, sizeof(_fakeRadioData.sideCorners));
    std::memset(_fakeRadioData.spans,         0, sizeof(_fakeRadioData.spans));
#endif
}

SideDef::~SideDef()
{}

LineDef &SideDef::line() const
{
    DENG2_ASSERT(_line != 0);
    return *_line;
}

Surface &SideDef::surface(int surface)
{
    switch(SideDefSection(surface))
    {
    case SS_MIDDLE: return _middleSurface;
    case SS_BOTTOM: return _bottomSurface;
    case SS_TOP:    return _topSurface;
    }
    /// @throw InvalidSectionError The given surface section identifier is not valid.
    throw InvalidSectionError("SideDef::surface", QString("Invalid section %1").arg(surface));
}

Surface const &SideDef::surface(int surface) const
{
    return const_cast<Surface const &>(const_cast<SideDef &>(*this).surface(surface));
}

short SideDef::flags() const
{
    return _flags;
}

#ifdef __CLIENT__

SideDef::FakeRadioData &SideDef::fakeRadioData()
{
    return _fakeRadioData;
}

SideDef::FakeRadioData const &SideDef::fakeRadioData() const
{
    return const_cast<FakeRadioData const &>(const_cast<SideDef *>(this)->fakeRadioData());
}

#endif // __CLIENT__

int SideDef::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_SECTOR: {
        DENG2_ASSERT(_line != 0);
        Sector *sector = _line->sectorPtr(this == _line->frontSideDefPtr()? FRONT : BACK);
        DMU_GetValue(DMT_SIDEDEF_SECTOR, &sector, &args, 0);
        break; }
    case DMU_LINEDEF:
        DMU_GetValue(DMT_SIDEDEF_LINE, &_line, &args, 0);
        break;
    case DMU_FLAGS:
        DMU_GetValue(DMT_SIDEDEF_FLAGS, &_flags, &args, 0);
        break;
    default:
        /// @throw UnknownPropertyError  The requested property does not exist.
        throw UnknownPropertyError("SideDef::property", QString("Property '%1' is unknown").arg(DMU_Str(args.prop)));
    }
    return false; // Continue iteration.
}

int SideDef::setProperty(setargs_t const &args)
{
    switch(args.prop)
    {
    case DMU_FLAGS:
        DMU_SetValue(DMT_SIDEDEF_FLAGS, &_flags, &args, 0);
        break;

    case DMU_LINEDEF:
        DMU_SetValue(DMT_SIDEDEF_LINE, &_line, &args, 0);
        break;

    default:
        /// @throw WritePropertyError  The requested property is not writable.
        throw WritePropertyError("SideDef::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
    }
    return false; // Continue iteration.
}
