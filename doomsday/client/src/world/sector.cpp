/** @file sector.h World map sector.
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

#include <QtAlgorithms>

#include <de/Log>

#include "dd_main.h"

#include "BspLeaf"
#include "Line"
#include "Plane"

#include "world/map.h"
#include "world/p_players.h"
#include "world/p_maptypes.h"
#ifdef __CLIENT__
#  include "world/r_world.h"
#  include "render/rend_bias.h"
#endif

#include "world/sector.h"

using namespace de;

DENG2_PIMPL(Sector),
DENG2_OBSERVES(Plane, HeightChange)
{
    /// Bounding box for the sector.
    AABoxd aaBox;

    /// Rough approximation of sector area (map units squared).
    coord_t roughArea;

    /// Primary sound emitter. Others are linked to this, forming a chain.
    ddmobj_base_t soundEmitter;

    /// List of planes (owned).
    Planes planes;

    /// List of line sides which reference the sector (not owned).
    Sides sides;

    /// List of BSP leafs which reference the sector (not owned).
    BspLeafs bspLeafs;

    /// Ambient light level in the sector.
    float lightLevel;

    /// Ambient light color in the sector.
    de::Vector3f lightColor;

#ifdef __CLIENT__
    /// LightGrid data values.
    LightGridData lightGridData;
#endif

    /// if == validCount, already checked.
    int validCount;

    Instance(Public *i, float lightLevel, Vector3f const &lightColor)
        : Base(i),
          roughArea(0),
          lightLevel(lightLevel),
          lightColor(lightColor),
          validCount(0)
    {
        zap(soundEmitter);
#ifdef __CLIENT__
        zap(lightGridData);
#endif
    }

    ~Instance()
    {
        qDeleteAll(planes);
    }

    void notifyLightLevelChanged(float oldLightLevel)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(LightLevelChange, i)
        {
            i->sectorLightLevelChanged(self, oldLightLevel);
        }
    }

    void notifyLightColorChanged(Vector3f const &oldLightColor,
                                 int changedComponents)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(LightColorChange, i)
        {
            i->sectorLightColorChanged(self, oldLightColor, changedComponents);
        }
    }

    void notifyLightColorChanged(Vector3f const &oldLightColor)
    {
        // Predetermine which components have changed.
        int changedComponents = 0;
        for(int i = 0; i < 3; ++i)
        {
            if(!de::fequal(lightColor[i], oldLightColor[i]))
                changedComponents |= (1 << i);
        }
        notifyLightColorChanged(oldLightColor, changedComponents);
    }

    // Observes Plane HeightChange.
    void planeHeightChanged(Plane &plane, coord_t oldHeight)
    {
        DENG2_UNUSED(oldHeight);

        // We are presently only interested in floor and/or ceiling height changes.
        if(!(&plane == &self.floor() || &plane == &self.ceiling()))
            return;

        self.updateSoundEmitterOrigin();

#ifdef __CLIENT__
        /// @todo Map should observe.
        App_World().map().updateMissingMaterialsForLinesOfSector(self);
        S_MarkSectorReverbDirty(&self);
#endif

        // Check if there are any camera players in this sector. If their
        // height is now above the ceiling/below the floor they are now in
        // the void.
        for(uint i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t *plr = &ddPlayers[i];
            ddplayer_t *ddpl = &plr->shared;

            if(!ddpl->inGame || !ddpl->mo || !ddpl->mo->bspLeaf)
                continue;

            if((ddpl->flags & DDPF_CAMERA) && ddpl->mo->bspLeaf->sectorPtr() == &self &&
               (ddpl->mo->origin[VZ] > self.ceiling().height() - 4 ||
                ddpl->mo->origin[VZ] < self.floor().height()))
            {
                ddpl->inVoid = true;
            }
        }

        // Update the sound emitter origins for all dependent wall surfaces.
        foreach(Line::Side *side, sides)
        {
            side->updateAllSoundEmitterOrigins();
            side->back().updateAllSoundEmitterOrigins();
        }

#ifdef __CLIENT__
        // Inform the shadow bias of changed geometry.
        foreach(BspLeaf *bspLeaf, bspLeafs)
        {
            if(bspLeaf->isDegenerate())
                 continue;

            foreach(Segment *seg, bspLeaf->allSegments())
            {
                if(!seg->hasLineSide())
                    continue;

                for(uint i = 0; i < 3; ++i)
                {
                    SB_SurfaceMoved(&seg->biasSurface(i));
                }
            }

            SB_SurfaceMoved(&bspLeaf->biasSurface(plane.inSectorIndex()));
        }

#endif // __CLIENT__
    }
};

Sector::Sector(float lightLevel, Vector3f const &lightColor)
    : MapElement(DMU_SECTOR), d(new Instance(this, lightLevel, lightColor))
{
    _frameFlags = 0;
    _mobjList = 0;
    zap(_reverb);
}

float Sector::lightLevel() const
{
    return d->lightLevel;
}

void Sector::setLightLevel(float newLightLevel)
{
    newLightLevel = de::clamp(0.f, newLightLevel, 1.f);

    if(!de::fequal(d->lightLevel, newLightLevel))
    {
        float oldLightLevel = d->lightLevel;
        d->lightLevel = newLightLevel;

        // Notify interested parties of the change.
        d->notifyLightLevelChanged(oldLightLevel);
    }
}

Vector3f const &Sector::lightColor() const
{
    return d->lightColor;
}

void Sector::setLightColor(Vector3f const &newLightColor)
{
    Vector3f newColorClamped = Vector3f(de::clamp(0.f, newLightColor.x, 1.f),
                                        de::clamp(0.f, newLightColor.y, 1.f),
                                        de::clamp(0.f, newLightColor.z, 1.f));

    if(d->lightColor != newColorClamped)
    {
        Vector3f oldLightColor = d->lightColor;
        d->lightColor = newColorClamped;

        // Notify interested parties of the change.
        d->notifyLightColorChanged(oldLightColor);
    }
}

void Sector::setLightColorComponent(int component, float newStrength)
{
    DENG_ASSERT(component >= 0 && component < 3);
    newStrength = de::clamp(0.f, newStrength, 1.f);
    if(!de::fequal(d->lightColor[component], newStrength))
    {
        Vector3f oldLightColor = d->lightColor;
        d->lightColor[component] = newStrength;

        // Notify interested parties of the change.
        d->notifyLightColorChanged(oldLightColor, (1 << component));
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

#ifdef __CLIENT__
Sector::LightGridData &Sector::lightGridData()
{
    return d->lightGridData;
}
#endif

int Sector::frameFlags() const
{
    return _frameFlags;
}

int Sector::validCount() const
{
    return d->validCount;
}

void Sector::setValidCount(int newValidCount)
{
    d->validCount = newValidCount;
}

Plane &Sector::plane(int planeIndex)
{
    if(planeIndex >= 0 && planeIndex < d->planes.count())
    {
        return *d->planes[planeIndex];
    }
    /// @throw MissingPlaneError The referenced plane does not exist.
    throw MissingPlaneError("Sector::plane", QString("Missing plane %1").arg(planeIndex));
}

Plane const &Sector::plane(int planeIndex) const
{
    return const_cast<Plane const &>(const_cast<Sector &>(*this).plane(planeIndex));
}

Sector::Sides const &Sector::sides() const
{
    return d->sides;
}

void Sector::buildSides(Map const &map)
{
    d->sides.clear();

#ifdef DENG2_QT_4_7_OR_NEWER
    uint count = 0;
    foreach(Line *line, map.lines())
    {
        if(line->frontSectorPtr() == this ||
           line->backSectorPtr()  == this)
            ++count;
    }

    if(0 == count) return;

    d->sides.reserve(count);
#endif

    foreach(Line *line, map.lines())
    {
        if(line->frontSectorPtr() == this)
        {
            // Ownership of the side is not given to the sector.
            d->sides.append(&line->front());
        }
        else if(line->backSectorPtr()  == this)
        {
            // Ownership of the side is not given to the sector.
            d->sides.append(&line->back());
        }
    }
}

Plane *Sector::addPlane(Vector3f const &normal, coord_t height)
{
    Plane *plane = new Plane(*this, normal, height);

    plane->audienceForHeightChange += d;
    plane->setInSectorIndex(d->planes.count());

    d->planes.append(plane);

    return plane;
}

Sector::Planes const &Sector::planes() const
{
    return d->planes;
}

bool Sector::hasSkyMaskedPlane() const
{
    foreach(Plane *plane, d->planes)
    {
        if(plane->surface().hasSkyMaskedMaterial())
            return true;
    }
    return false;
}

Sector::BspLeafs const &Sector::bspLeafs() const
{
    return d->bspLeafs;
}

void Sector::buildBspLeafs(Map const &map)
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
    d->aaBox.clear();

    bool isFirst = false;
    foreach(BspLeaf *leaf, d->bspLeafs)
    {
        if(leaf->isDegenerate()) continue;

        AABoxd const &leafAABox = leaf->face().aaBox();

        if(!isFirst)
        {
            V2d_UniteBox(d->aaBox.arvec2, leafAABox.arvec2);
        }
        else
        {
            V2d_CopyBox(d->aaBox.arvec2, leafAABox.arvec2);
            isFirst = false;
        }
    }
}

void Sector::updateRoughArea()
{
    d->roughArea = 0;

    foreach(BspLeaf *leaf, d->bspLeafs)
    {
        if(leaf->isDegenerate()) continue;

        AABoxd const &leafAABox = leaf->face().aaBox();

        d->roughArea += (leafAABox.maxX - leafAABox.minX) *
                        (leafAABox.maxY - leafAABox.minY);
    }
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

int Sector::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_LIGHT_LEVEL:
        args.setValue(DMT_SECTOR_LIGHTLEVEL, &d->lightLevel, 0);
        break;
    case DMU_COLOR:
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.x, 0);
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.y, 1);
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.z, 2);
        break;
    case DMU_COLOR_RED:
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.x, 0);
        break;
    case DMU_COLOR_GREEN:
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.y, 0);
        break;
    case DMU_COLOR_BLUE:
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.z, 0);
        break;
    case DMU_EMITTER: {
        ddmobj_base_t const *soundEmitterAdr = &d->soundEmitter;
        args.setValue(DMT_SECTOR_EMITTER, &soundEmitterAdr, 0);
        break; }
    case DMT_MOBJS:
        args.setValue(DMT_SECTOR_MOBJLIST, &_mobjList, 0);
        break;
    case DMU_VALID_COUNT:
        args.setValue(DMT_SECTOR_VALIDCOUNT, &d->validCount, 0);
        break;
    case DMU_FLOOR_PLANE: {
        Plane *pln = d->planes[Floor];
        args.setValue(DMT_SECTOR_FLOORPLANE, &pln, 0);
        break; }
    case DMU_CEILING_PLANE: {
        Plane *pln = d->planes[Ceiling];
        args.setValue(DMT_SECTOR_CEILINGPLANE, &pln, 0);
        break; }
    default:
        return MapElement::property(args);
    }

    return false; // Continue iteration.
}

int Sector::setProperty(DmuArgs const &args)
{
    switch(args.prop)
    {
    case DMU_COLOR: {
        Vector3f newLightColor;
        args.value(DMT_SECTOR_RGB, &newLightColor.x, 0);
        args.value(DMT_SECTOR_RGB, &newLightColor.y, 1);
        args.value(DMT_SECTOR_RGB, &newLightColor.z, 2);
        setLightColor(newLightColor);
        break; }
    case DMU_COLOR_RED: {
        float newStrength;
        args.value(DMT_SECTOR_RGB, &newStrength, 0);
        setLightRed(newStrength);
        break; }
    case DMU_COLOR_GREEN: {
        float newStrength;
        args.value(DMT_SECTOR_RGB, &newStrength, 0);
        setLightGreen(newStrength);
        break; }
    case DMU_COLOR_BLUE: {
        float newStrength;
        args.value(DMT_SECTOR_RGB, &newStrength, 0);
        setLightBlue(newStrength);
        break; }
    case DMU_LIGHT_LEVEL: {
        float newLightLevel;
        args.value(DMT_SECTOR_LIGHTLEVEL, &newLightLevel, 0);
        setLightLevel(newLightLevel);
        break; }
    case DMU_VALID_COUNT:
        args.value(DMT_SECTOR_VALIDCOUNT, &d->validCount, 0);
        break;
    default:
        return MapElement::setProperty(args);
    }

    return false; // Continue iteration.
}
