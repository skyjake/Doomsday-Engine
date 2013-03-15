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
    line = 0;
    flags = 0;
    std::memset(&buildData, 0, sizeof(buildData));
    fakeRadioUpdateCount = 0;
    std::memset(topCorners, 0, sizeof(topCorners));
    std::memset(bottomCorners, 0, sizeof(bottomCorners));
    std::memset(sideCorners, 0, sizeof(sideCorners));
    std::memset(spans, 0, sizeof(spans));
}

SideDef::~SideDef()
{}

SideDef &SideDef::operator = (SideDef const &other)
{
    _middleSurface = other._middleSurface;
    _bottomSurface = other._bottomSurface;
    _topSurface    = other._topSurface;

    line = other.line;
    flags = other.flags;
    fakeRadioUpdateCount = other.fakeRadioUpdateCount;

    std::memcpy(&buildData, &other.buildData, sizeof(buildData));
    std::memcpy(&topCorners, &other.topCorners, sizeof(topCorners));
    std::memcpy(&bottomCorners, &other.bottomCorners, sizeof(bottomCorners));
    std::memcpy(&sideCorners, &other.sideCorners, sizeof(sideCorners));
    std::memcpy(&spans, &other.spans, sizeof(spans));

    return *this;
}

Surface &SideDef::surface(int surface)
{
    switch(SideDefSection(surface))
    {
    case SS_MIDDLE: return _middleSurface;
    case SS_BOTTOM: return _bottomSurface;
    case SS_TOP:    return _topSurface;
    }
    /// @throw InvalidSectionError The given surface section reference is not valid.
    throw InvalidSectionError("SideDef::surface", QString("Invalid section %1").arg(surface));
}

Surface const &SideDef::surface(int surface) const
{
    return const_cast<Surface const &>(const_cast<SideDef &>(*this).surface(surface));
}

void SideDef::updateSoundEmitterOrigins()
{
    if(!line) return;
    _middleSurface.updateSoundEmitterOrigin();
    _bottomSurface.updateSoundEmitterOrigin();
    _topSurface.updateSoundEmitterOrigin();
}

void SideDef::updateSurfaceTangents()
{
    if(!line) return;

    byte sid = line->frontSideDefPtr() == this? FRONT : BACK;

    V3f_Set(_topSurface.normal, (line->vertexOrigin(sid^1)[VY] - line->vertexOrigin(sid  )[VY]) / line->length(),
                                (line->vertexOrigin(sid  )[VX] - line->vertexOrigin(sid^1)[VX]) / line->length(),
                                0);

    V3f_BuildTangents(_topSurface.tangent, _topSurface.bitangent, _topSurface.normal);

    // All surfaces of a sidedef have the same tangent space vectors.
    V3f_Copy(_middleSurface.tangent,   _topSurface.tangent);
    V3f_Copy(_middleSurface.bitangent, _topSurface.bitangent);
    V3f_Copy(_middleSurface.normal,    _topSurface.normal);

    V3f_Copy(_bottomSurface.tangent,   _topSurface.tangent);
    V3f_Copy(_bottomSurface.bitangent, _topSurface.bitangent);
    V3f_Copy(_bottomSurface.normal,    _topSurface.normal);
}

int SideDef::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_SECTOR: {
        Sector *sector = line->sectorPtr(this == line->frontSideDefPtr()? FRONT : BACK);
        DMU_GetValue(DMT_SIDEDEF_SECTOR, &sector, &args, 0);
        break; }
    case DMU_LINEDEF:
        DMU_GetValue(DMT_SIDEDEF_LINE, &line, &args, 0);
        break;
    case DMU_FLAGS:
        DMU_GetValue(DMT_SIDEDEF_FLAGS, &flags, &args, 0);
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
        DMU_SetValue(DMT_SIDEDEF_FLAGS, &flags, &args, 0);
        break;

    case DMU_LINEDEF:
        DMU_SetValue(DMT_SIDEDEF_LINE, &line, &args, 0);
        break;

    default:
        /// @throw WritePropertyError  The requested property is not writable.
        throw WritePropertyError("SideDef::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
    }
    return false; // Continue iteration.
}
