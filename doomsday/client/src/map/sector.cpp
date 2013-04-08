/** @file sector.h World Map Sector.
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

#include <QListIterator>
#include <QtAlgorithms>

#include <de/Log>

#include "map/bspleaf.h"
#include "map/linedef.h"
#include "map/gamemap.h"
#ifdef __CLIENT__
#  include "render/rend_bias.h"
#endif

#include "map/sector.h"

using namespace de;

DENG2_PIMPL(Sector)
{
    /// Bounding box for the sector.
    AABoxd aaBox;

    /// Rough approximation of sector area (map units squared).
    coord_t roughArea;

    /// Primary sound emitter. Others are linked to this, forming a chain.
    ddmobj_base_t soundEmitter;

    /// List of lines which reference the sector (not owned).
    Lines lines;

    /// List of BSP leafs which reference the sector (not owned).
    BspLeafs bspLeafs;

    Instance(Public *i)
        : Base(i),
          roughArea(0)
    {
        std::memset(&soundEmitter, 0, sizeof(soundEmitter));
    }

    ~Instance()
    {
#ifdef __CLIENT__
        // Destroy the biassurfaces for each plane.
        foreach(BspLeaf *bspLeaf, bspLeafs)
        for(uint i = 0; i < self.planeCount(); ++i)
        {
            SB_DestroySurface(bspLeaf->_bsuf[i]);
        }
#endif

        qDeleteAll(self._planes);
    }

    void notifyLightLevelChanged(float oldLightLevel)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(LightLevelChange, i)
        {
            i->lightLevelChanged(self, oldLightLevel);
        }
    }

    void notifyLightColorChanged(Vector3f const &oldLightColor)
    {
        // Predetermine which components have changed.
        int changedComponents = 0;
        for(int i = 0; i < 3; ++i)
        {
            if(!de::fequal(self._lightColor[i], oldLightColor[i]))
                changedComponents |= (1 << i);
        }

        DENG2_FOR_PUBLIC_AUDIENCE(LightColorChange, i)
        {
            i->lightColorChanged(self, oldLightColor, changedComponents);
        }
    }
};

Sector::Sector(float lightLevel, Vector3f const &lightColor)
    : MapElement(DMU_SECTOR),
      d(new Instance(this)),
      _lightLevel(lightLevel), _oldLightLevel(_lightLevel),
      _lightColor(lightColor), _oldLightColor(_lightColor)
{
    _frameFlags = 0;
    _validCount = 0;
    _mobjList = 0;
    std::memset(&_lightGridData, 0, sizeof(_lightGridData));
    std::memset(_reverb, 0, sizeof(_reverb));
    _origIndex = 0;
}

float Sector::lightLevel() const
{
    return _lightLevel;
}

void Sector::setLightLevel(float newLightLevel_)
{
    float newLightLevel = de::clamp(0.f, newLightLevel_, 1.f);
    if(!de::fequal(_lightLevel, newLightLevel))
    {
        float oldLightLevel = _lightLevel;
        _lightLevel = newLightLevel;

        // Notify interested parties of the change.
        d->notifyLightLevelChanged(oldLightLevel);
    }
}

Vector3f const &Sector::lightColor() const
{
    return _lightColor;
}

void Sector::setLightColor(Vector3f const &newLightColor_)
{
    Vector3f newLightColor = Vector3f(de::clamp(0.f, newLightColor_.x, 1.f),
                                      de::clamp(0.f, newLightColor_.y, 1.f),
                                      de::clamp(0.f, newLightColor_.z, 1.f));
    if(_lightColor != newLightColor)
    {
        Vector3f oldLightColor = _lightColor;
        _lightColor = newLightColor;

        // Notify interested parties of the change.
        d->notifyLightColorChanged(oldLightColor);
    }
}

void Sector::setLightColorComponent(int component, float newStrength)
{
    DENG_ASSERT(component >= 0 && component < 3);
    newStrength = de::clamp(0.f, newStrength, 1.f);
    if(!de::fequal(_lightColor[component], newStrength))
    {
        Vector3f oldLightColor = _lightColor;
        _lightColor[component] = newStrength;

        // Notify interested parties of the change.
        d->notifyLightColorChanged(oldLightColor);
    }
}

struct mobj_s *Sector::firstMobj() const
{
    return _mobjList;
}

ddmobj_base_t &Sector::soundEmitter()
{
    return d->soundEmitter;
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
    return d->lines;
}

void Sector::buildLines(GameMap const &map)
{
    d->lines.clear();

#ifdef DENG2_QT_4_7_OR_NEWER
    uint count = 0;
    foreach(LineDef *line, map.lines())
    {
        if(line->frontSectorPtr() == this ||
           line->backSectorPtr()  == this)
            ++count;
    }

    if(0 == count) return;

    d->lines.reserve(count);
#endif

    foreach(LineDef *line, map.lines())
    {
        if(line->frontSectorPtr() == this ||
           line->backSectorPtr()  == this)
        {
            // Ownership of the line is not given to the sector.
            d->lines.append(line);
        }
    }
}

Sector::Planes const &Sector::planes() const
{
    return _planes;
}

Sector::BspLeafs const &Sector::bspLeafs() const
{
    return d->bspLeafs;
}

void Sector::buildBspLeafs(GameMap const &map)
{
    d->bspLeafs.clear();

#ifdef DENG2_QT_4_7_OR_NEWER
    uint count = 0;
    foreach(BspLeaf *bspLeaf, map.bspLeafs())
    {
        if(bspLeaf->sectorPtr() == this)
            ++count;
    }

    if(0 == count) return;

    d->bspLeafs.reserve(count);
#endif

    foreach(BspLeaf *bspLeaf, map.bspLeafs())
    {
        if(bspLeaf->sectorPtr() == this)
        {
            // Ownership of the BSP leaf is not given to the sector.
            d->bspLeafs.append(bspLeaf);
        }
    }
}

Sector::BspLeafs const &Sector::reverbBspLeafs() const
{
    return _reverbBspLeafs;
}

AABoxd const &Sector::aaBox() const
{
    return d->aaBox;
}

coord_t Sector::roughArea() const
{
    return d->roughArea;
}

void Sector::updateAABox()
{
    V2d_Set(d->aaBox.min, DDMAXFLOAT, DDMAXFLOAT);
    V2d_Set(d->aaBox.max, DDMINFLOAT, DDMINFLOAT);

    if(!d->lines.count()) return;

    QListIterator<LineDef *> lineIt(d->lines);

    LineDef *line = lineIt.next();
    V2d_CopyBox(d->aaBox.arvec2, line->aaBox().arvec2);

    while(lineIt.hasNext())
    {
        line = lineIt.next();
        V2d_UniteBox(d->aaBox.arvec2, line->aaBox().arvec2);
    }
}

void Sector::updateRoughArea()
{
    // Only a very rough estimate is required.
    d->roughArea = ((d->aaBox.maxX - d->aaBox.minX) / 128) *
                   ((d->aaBox.maxY - d->aaBox.minY) / 128);
}

void Sector::linkSoundEmitter(ddmobj_base_t &newEmitter)
{
    // The sector's base is always head of the chain, so link the other after it.
    newEmitter.thinker.prev = &d->soundEmitter.thinker;
    newEmitter.thinker.next = d->soundEmitter.thinker.next;
    if(newEmitter.thinker.next)
        newEmitter.thinker.next->prev = &newEmitter.thinker;
    d->soundEmitter.thinker.next = &newEmitter.thinker;
}

void Sector::updateSoundEmitterOrigin()
{
    d->soundEmitter.origin[VX] = (d->aaBox.minX + d->aaBox.maxX) / 2;
    d->soundEmitter.origin[VY] = (d->aaBox.minY + d->aaBox.maxY) / 2;
    d->soundEmitter.origin[VZ] = (floor().height() + ceiling().height()) / 2;
}

int Sector::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_LIGHT_LEVEL:
        DMU_GetValue(DMT_SECTOR_LIGHTLEVEL, &_lightLevel, &args, 0);
        break;
    case DMU_COLOR:
        DMU_GetValue(DMT_SECTOR_RGB, &_lightColor.x, &args, 0);
        DMU_GetValue(DMT_SECTOR_RGB, &_lightColor.y, &args, 1);
        DMU_GetValue(DMT_SECTOR_RGB, &_lightColor.z, &args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_GetValue(DMT_SECTOR_RGB, &_lightColor.x, &args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_GetValue(DMT_SECTOR_RGB, &_lightColor.y, &args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_GetValue(DMT_SECTOR_RGB, &_lightColor.z, &args, 0);
        break;
    case DMU_BASE: {
        ddmobj_base_t const *soundEmitterAdr = &d->soundEmitter;
        DMU_GetValue(DMT_SECTOR_BASE, &soundEmitterAdr, &args, 0);
        break; }
    case DMU_LINEDEF_COUNT: {
        int val = lineCount();
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
    case DMU_COLOR: {
        Vector3f newLightColor;
        DMU_SetValue(DMT_SECTOR_RGB, &newLightColor.x, &args, 0);
        DMU_SetValue(DMT_SECTOR_RGB, &newLightColor.y, &args, 1);
        DMU_SetValue(DMT_SECTOR_RGB, &newLightColor.z, &args, 2);
        setLightColor(newLightColor);
        break; }
    case DMU_COLOR_RED: {
        float newStrength;
        DMU_SetValue(DMT_SECTOR_RGB, &newStrength, &args, 0);
        setLightRed(newStrength);
        break; }
    case DMU_COLOR_GREEN: {
        float newStrength;
        DMU_SetValue(DMT_SECTOR_RGB, &newStrength, &args, 0);
        setLightGreen(newStrength);
        break; }
    case DMU_COLOR_BLUE: {
        float newStrength;
        DMU_SetValue(DMT_SECTOR_RGB, &newStrength, &args, 0);
        setLightBlue(newStrength);
        break; }
    case DMU_LIGHT_LEVEL: {
        float newLightLevel;
        DMU_SetValue(DMT_SECTOR_LIGHTLEVEL, &newLightLevel, &args, 0);
        setLightLevel(newLightLevel);
        break; }
    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_SECTOR_VALIDCOUNT, &_validCount, &args, 0);
        break;
    default:
        /// @throw WritePropertyError  The requested property is not writable.
        throw WritePropertyError("Sector::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}
