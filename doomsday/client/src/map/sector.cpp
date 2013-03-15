/** @file sector.h Map Sector.
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

#include "de_base.h"
#include "map/bspleaf.h"
#include "map/linedef.h"
#include <de/Log>
#include <QtAlgorithms>

#include "map/sector.h"

using namespace de;

Sector::Sector() : MapElement(DMU_SECTOR)
{
    frameFlags = 0;
    validCount = 0;
    std::memset(&aaBox, 0, sizeof(aaBox));
    roughArea = 0;
    lightLevel = 0;
    oldLightLevel = 0;
    std::memset(rgb, 0, sizeof(rgb));
    std::memset(oldRGB, 0, sizeof(oldRGB));
    mobjList = 0;
    std::memset(&base, 0, sizeof(base));
    blockCount = 0;
    changedBlockCount = 0;
    blocks = 0;
    std::memset(reverb, 0, sizeof(reverb));
    std::memset(&buildData, 0, sizeof(buildData));
}

Sector::~Sector()
{
    qDeleteAll(_planes);
}

Plane &Sector::plane(int planeIndex)
{
    if(planeIndex >= 0 && planeIndex < _planes.count())
    {
        return *_planes[planeIndex];
    }
    /// @throw MissingPlaneError The referenced plane does not exist.
    throw MissingPlaneError("Sector::plane", QString("Missing plane %1").arg(planeIndex));
}

Plane const &Sector::plane(int planeIndex) const
{
    return const_cast<Plane const &>(const_cast<Sector &>(*this).plane(planeIndex));
}

Sector::Lines const &Sector::lines() const
{
    return _lines;
}

Sector::Planes const &Sector::planes() const
{
    return _planes;
}

Sector::BspLeafs const &Sector::bspLeafs() const
{
    return _bspLeafs;
}

Sector::BspLeafs const &Sector::reverbBspLeafs() const
{
    return _reverbBspLeafs;
}

void Sector::updateAABox()
{
    V2d_Set(aaBox.min, DDMAXFLOAT, DDMAXFLOAT);
    V2d_Set(aaBox.max, DDMINFLOAT, DDMINFLOAT);

    if(!_lines.count()) return;

    QListIterator<LineDef *> lineIt(_lines);

    LineDef *line = lineIt.peekNext();
    V2d_InitBox(aaBox.arvec2, line->aaBox().min);
    V2d_AddToBox(aaBox.arvec2, line->aaBox().max);

    while(lineIt.hasNext())
    {
        line = lineIt.next();
        V2d_AddToBox(aaBox.arvec2, line->aaBox().min);
        V2d_AddToBox(aaBox.arvec2, line->aaBox().max);
    }
}

void Sector::updateArea()
{
    // Only a very rough estimate is required.
    roughArea = ((aaBox.maxX - aaBox.minX) / 128) *
                ((aaBox.maxY - aaBox.minY) / 128);
}

void Sector::updateBaseOrigin()
{
    base.origin[VX] = (aaBox.minX + aaBox.maxX) / 2;
    base.origin[VY] = (aaBox.minY + aaBox.maxY) / 2;
    base.origin[VZ] = (floor().height() + ceiling().height()) / 2;
}

int Sector::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_LIGHT_LEVEL:
        DMU_GetValue(DMT_SECTOR_LIGHTLEVEL, &lightLevel, &args, 0);
        break;
    case DMU_COLOR:
        DMU_GetValue(DMT_SECTOR_RGB, &rgb[0], &args, 0);
        DMU_GetValue(DMT_SECTOR_RGB, &rgb[1], &args, 1);
        DMU_GetValue(DMT_SECTOR_RGB, &rgb[2], &args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_GetValue(DMT_SECTOR_RGB, &rgb[0], &args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_GetValue(DMT_SECTOR_RGB, &rgb[1], &args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_GetValue(DMT_SECTOR_RGB, &rgb[2], &args, 0);
        break;
    case DMU_BASE: {
        ddmobj_base_t const *baseAdr = &base;
        DMU_GetValue(DMT_SECTOR_BASE, &baseAdr, &args, 0);
        break; }
    case DMU_LINEDEF_COUNT: {
        int val = _lines.count();
        DMU_GetValue(DDVT_INT, &val, &args, 0);
        break; }
    case DMT_MOBJS:
        DMU_GetValue(DMT_SECTOR_MOBJLIST, &mobjList, &args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_GetValue(DMT_SECTOR_VALIDCOUNT, &validCount, &args, 0);
        break;
    case DMU_FLOOR_PLANE: {
        Plane *pln = _planes[Plane::Floor];
        DMU_GetValue(DMT_SECTOR_FLOORPLANE, &pln, &args, 0);
        break; }
    case DMU_CEILING_PLANE: {
        Plane* pln = _planes[Plane::Ceiling];
        DMU_GetValue(DMT_SECTOR_CEILINGPLANE, &pln, &args, 0);
        break; }
    default:
        /// @throw UnknownPropertyError  The requested property does not exist.
        throw UnknownPropertyError("Sector::property", QString("Property '%1' is unknown").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}

int Sector::setProperty(setargs_t const &args)
{
    switch(args.prop)
    {
    case DMU_COLOR:
        DMU_SetValue(DMT_SECTOR_RGB, &rgb[0], &args, 0);
        DMU_SetValue(DMT_SECTOR_RGB, &rgb[1], &args, 1);
        DMU_SetValue(DMT_SECTOR_RGB, &rgb[2], &args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_SetValue(DMT_SECTOR_RGB, &rgb[0], &args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_SetValue(DMT_SECTOR_RGB, &rgb[1], &args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_SetValue(DMT_SECTOR_RGB, &rgb[2], &args, 0);
        break;
    case DMU_LIGHT_LEVEL:
        DMU_SetValue(DMT_SECTOR_LIGHTLEVEL, &lightLevel, &args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_SECTOR_VALIDCOUNT, &validCount, &args, 0);
        break;
    default:
        /// @throw WritePropertyError  The requested property is not writable.
        throw WritePropertyError("Sector::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}
