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
#include "map/line.h"

#include "map/sidedef.h"

using namespace de;

SideDef::SideDef(Line &line)
    : MapElement(DMU_SIDEDEF),
      _line(&line)
{}

SideDef::~SideDef()
{}

Line &SideDef::line() const
{
    return *_line;
}

#if 0
Surface &SideDef::surface(int sectionId)
{
    Line::Side &side = _line->side(this == _line->frontSideDefPtr()? FRONT : BACK);
    return side.section(SideDefSection(sectionId)).surface();
}

Surface const &SideDef::surface(int surface) const
{
    return const_cast<Surface const &>(const_cast<SideDef &>(*this).surface(surface));
}
#endif

int SideDef::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_SECTOR: {
        Sector *sector = _line->sectorPtr(this == _line->frontSideDefPtr()? FRONT : BACK);
        DMU_GetValue(DMT_SIDEDEF_SECTOR, &sector, &args, 0);
        break; }
    case DMU_LINE:
        DMU_GetValue(DMT_SIDEDEF_LINE, &_line, &args, 0);
        break;
    case DMU_FLAGS: {
        DENG2_ASSERT(_line != 0);
        short flags = _line->side(this == _line->frontSideDefPtr()? FRONT : BACK).flags();
        DMU_GetValue(DMT_SIDEDEF_FLAGS, &flags, &args, 0);
        break; }
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
    case DMU_FLAGS: {
        Line::Side &side =_line->side(this == _line->frontSideDefPtr()? FRONT : BACK);
        short newFlags = side.flags();
        DMU_SetValue(DMT_SIDEDEF_FLAGS, &newFlags, &args, 0);
        side._flags = newFlags;
        break; }

    case DMU_LINE:
        DMU_SetValue(DMT_SIDEDEF_LINE, &_line, &args, 0);
        break;

    default:
        /// @throw WritePropertyError  The requested property is not writable.
        throw WritePropertyError("SideDef::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
    }
    return false; // Continue iteration.
}
