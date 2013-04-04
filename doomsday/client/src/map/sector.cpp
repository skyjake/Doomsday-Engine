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
#include <QListIterator>
#include <QtAlgorithms>

#include "map/sector.h"

using namespace de;

Sector::Sector() : MapElement(DMU_SECTOR)
{
    _frameFlags = 0;
    _validCount = 0;
    std::memset(&_aaBox, 0, sizeof(_aaBox));
    _roughArea = 0;
    _lightLevel = 0;
    _oldLightLevel = 0;
    V3f_Set(_lightColor, 0, 0, 0);
    V3f_Set(_oldLightColor, 0, 0, 0);
    _mobjList = 0;
    std::memset(&_soundEmitter, 0, sizeof(_soundEmitter));
    std::memset(&_lightGridData, 0, sizeof(_lightGridData));
    std::memset(_reverb, 0, sizeof(_reverb));
    _origIndex = 0;
}

Sector::~Sector()
{
    qDeleteAll(_planes);
}

float Sector::lightLevel() const
{
    return _lightLevel;
}

const_pvec3f_t &Sector::lightColor() const
{
    return _lightColor;
}

struct mobj_s *Sector::firstMobj() const
{
    return _mobjList;
}

ddmobj_base_t &Sector::soundEmitter()
{
    return _soundEmitter;
}

ddmobj_base_t const &Sector::soundEmitter() const
{
    return const_cast<ddmobj_base_t const &>(const_cast<Sector &>(*this).soundEmitter());
}

AudioEnvironmentFactors const &Sector::audioEnvironmentFactors() const
{
    return _reverb;
}

uint Sector::origIndex() const
{
    return _origIndex;
}

int Sector::frameFlags() const
{
    return _frameFlags;
}

int Sector::validCount() const
{
    return _validCount;
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

AABoxd const &Sector::aaBox() const
{
    return _aaBox;
}

coord_t Sector::roughArea() const
{
    return _roughArea;
}

void Sector::updateAABox()
{
    V2d_Set(_aaBox.min, DDMAXFLOAT, DDMAXFLOAT);
    V2d_Set(_aaBox.max, DDMINFLOAT, DDMINFLOAT);

    if(!_lines.count()) return;

    QListIterator<LineDef *> lineIt(_lines);

    LineDef *line = lineIt.next();
    V2d_InitBox(_aaBox.arvec2, line->aaBox().min);
    V2d_AddToBox(_aaBox.arvec2, line->aaBox().max);

    while(lineIt.hasNext())
    {
        line = lineIt.next();
        V2d_AddToBox(_aaBox.arvec2, line->aaBox().min);
        V2d_AddToBox(_aaBox.arvec2, line->aaBox().max);
    }
}

void Sector::updateRoughArea()
{
    // Only a very rough estimate is required.
    _roughArea = ((_aaBox.maxX - _aaBox.minX) / 128) *
                 ((_aaBox.maxY - _aaBox.minY) / 128);
}

void Sector::linkSoundEmitter(ddmobj_base_t &newEmitter)
{
    // The sector's base is always head of the chain, so link the other after it.
    newEmitter.thinker.prev = &_soundEmitter.thinker;
    newEmitter.thinker.next = _soundEmitter.thinker.next;
    if(newEmitter.thinker.next)
        newEmitter.thinker.next->prev = &newEmitter.thinker;
    _soundEmitter.thinker.next = &newEmitter.thinker;
}

void Sector::updateSoundEmitterOrigin()
{
    _soundEmitter.origin[VX] = (_aaBox.minX + _aaBox.maxX) / 2;
    _soundEmitter.origin[VY] = (_aaBox.minY + _aaBox.maxY) / 2;
    _soundEmitter.origin[VZ] = (floor().height() + ceiling().height()) / 2;
}

int Sector::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_LIGHT_LEVEL:
        DMU_GetValue(DMT_SECTOR_LIGHTLEVEL, &_lightLevel, &args, 0);
        break;
    case DMU_COLOR:
        DMU_GetValue(DMT_SECTOR_RGB, &_lightColor[0], &args, 0);
        DMU_GetValue(DMT_SECTOR_RGB, &_lightColor[1], &args, 1);
        DMU_GetValue(DMT_SECTOR_RGB, &_lightColor[2], &args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_GetValue(DMT_SECTOR_RGB, &_lightColor[0], &args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_GetValue(DMT_SECTOR_RGB, &_lightColor[1], &args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_GetValue(DMT_SECTOR_RGB, &_lightColor[2], &args, 0);
        break;
    case DMU_BASE: {
        ddmobj_base_t const *soundEmitterAdr = &_soundEmitter;
        DMU_GetValue(DMT_SECTOR_BASE, &soundEmitterAdr, &args, 0);
        break; }
    case DMU_LINEDEF_COUNT: {
        int val = _lines.count();
        DMU_GetValue(DDVT_INT, &val, &args, 0);
        break; }
    case DMT_MOBJS:
        DMU_GetValue(DMT_SECTOR_MOBJLIST, &_mobjList, &args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_GetValue(DMT_SECTOR_VALIDCOUNT, &_validCount, &args, 0);
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
        DMU_SetValue(DMT_SECTOR_RGB, &_lightColor[0], &args, 0);
        DMU_SetValue(DMT_SECTOR_RGB, &_lightColor[1], &args, 1);
        DMU_SetValue(DMT_SECTOR_RGB, &_lightColor[2], &args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_SetValue(DMT_SECTOR_RGB, &_lightColor[0], &args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_SetValue(DMT_SECTOR_RGB, &_lightColor[1], &args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_SetValue(DMT_SECTOR_RGB, &_lightColor[2], &args, 0);
        break;
    case DMU_LIGHT_LEVEL:
        DMU_SetValue(DMT_SECTOR_LIGHTLEVEL, &_lightLevel, &args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_SECTOR_VALIDCOUNT, &_validCount, &args, 0);
        break;
    default:
        /// @throw WritePropertyError  The requested property is not writable.
        throw WritePropertyError("Sector::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}
