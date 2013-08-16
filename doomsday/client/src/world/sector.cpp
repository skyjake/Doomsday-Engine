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

#include <QSet>
#include <QtAlgorithms>

#include <de/Log>

#include "dd_main.h"

#include "Face"

#include "BspLeaf"
#include "Line"
#include "Plane"

#include "world/map.h"
#include "world/p_object.h"
#include "world/p_players.h"

#ifdef __CLIENT__
#  include "render/rend_main.h" // useBias
#endif

#include "world/sector.h"

using namespace de;

#ifdef __CLIENT__
typedef QSet<BspLeaf *> ReverbBspLeafs;
#endif

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

    /// Set of BSP leafs which contribute to the environmental audio (reverb)
    /// characteristics.
    ReverbBspLeafs reverbBspLeafs;

    /// Final environmental audio characteristics.
    AudioEnvironmentFactors reverb;

    bool visible; ///< @c true= marked as visible for the current frame.
#endif

    /// if == validCount, already checked.
    int validCount;

    Instance(Public *i, float lightLevel, Vector3f const &lightColor)
        : Base(i),
          roughArea(0),
          lightLevel(lightLevel),
          lightColor(lightColor),
          validCount(0)
#ifdef __CLIENT__
         ,visible(false)
#endif
    {
        zap(soundEmitter);
#ifdef __CLIENT__
        zap(lightGridData);
        zap(reverb);
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

#ifdef __CLIENT__
    void addReverbBspLeaf(BspLeaf *bspLeaf)
    {
        // Degenerate leafs never contribute.
        if(!bspLeaf || !bspLeaf->isDegenerate())
            return;

        reverbBspLeafs.insert(bspLeaf);
    }

    static int addReverbBspLeafWorker(BspLeaf *bspLeaf, void *context)
    {
        static_cast<Instance *>(context)->addReverbBspLeaf(bspLeaf);
        return false; // Continue iteration.
    }
#endif

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
        self.map().updateMissingMaterialsForLinesOfSector(self);
        S_MarkSectorReverbDirty(&self);
#endif

        // Check if there are any camera players in this sector. If their
        // height is now above the ceiling/below the floor they are now in
        // the void.
        for(int i = 0; i < DDMAXPLAYERS; ++i)
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
        if(!ddMapSetup && useBias)
        {
            // Inform the shadow bias of changed geometry.
            foreach(BspLeaf *bspLeaf, bspLeafs)
            {
                bspLeaf->updateBiasAfterGeometryMove(plane.indexInSector());
            }
        }

#endif // __CLIENT__
    }
};

Sector::Sector(float lightLevel, Vector3f const &lightColor)
    : MapElement(DMU_SECTOR), d(new Instance(this, lightLevel, lightColor))
{
    _mobjList = 0;
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

#ifdef __CLIENT__

Sector::LightGridData &Sector::lightGridData()
{
    return d->lightGridData;
}

/**
 * Determine the BSP leafs which contribute to each sector's environmental
 * audio characteristics (reverb). Given that BSP leafs do not change shape
 * (on the XY plane, that is), they do not move and are not created/destroyed
 * once the map has been loaded; this step can be pre-processed.
 */
void Sector::initReverb()
{
    d->reverbBspLeafs.clear();

    // Sectors which no referencing line need no reverb.
    if(!sideCount()) return;

    AABoxd affectionBounds = aaBox();
    affectionBounds.minX -= 128;
    affectionBounds.minY -= 128;
    affectionBounds.maxX += 128;
    affectionBounds.maxY += 128;

    // LOG_DEBUG("Finding reverb BSP leafs for sector %u (min:%s  max:%s)")
    //    << map().sectorIndex(this)
    //    << Vector2d(affectionBounds.min).asText()
    //    << Vector2d(affectionBounds.max).asText();

    // Link all non-degenerate BspLeafs whose axis-aligned bounding box intersects
    // with the affection bounds to the reverb set.
    map().bspLeafsBoxIterator(affectionBounds, 0, Instance::addReverbBspLeafWorker, d);
}

void Sector::updateReverb()
{
    // Sectors which no referencing line need no reverb.
    if(!sideCount())
        return;

    uint spaceVolume = int((ceiling().height() - floor().height()) * roughArea());

    d->reverb[SRD_SPACE] = d->reverb[SRD_VOLUME] =
        d->reverb[SRD_DECAY] = d->reverb[SRD_DAMPING] = 0;

    foreach(BspLeaf *bspLeaf, d->reverbBspLeafs)
    {
        if(bspLeaf->updateReverb())
        {
            BspLeaf::AudioEnvironmentFactors const &leafReverb = bspLeaf->reverb();

            d->reverb[SRD_SPACE]   += leafReverb[SRD_SPACE];

            d->reverb[SRD_VOLUME]  += leafReverb[SRD_VOLUME]  / 255.0f * leafReverb[SRD_SPACE];
            d->reverb[SRD_DECAY]   += leafReverb[SRD_DECAY]   / 255.0f * leafReverb[SRD_SPACE];
            d->reverb[SRD_DAMPING] += leafReverb[SRD_DAMPING] / 255.0f * leafReverb[SRD_SPACE];
        }
    }

    float spaceScatter;
    if(d->reverb[SRD_SPACE])
    {
        spaceScatter = spaceVolume / d->reverb[SRD_SPACE];
        // These three are weighted by the space.
        d->reverb[SRD_VOLUME]  /= d->reverb[SRD_SPACE];
        d->reverb[SRD_DECAY]   /= d->reverb[SRD_SPACE];
        d->reverb[SRD_DAMPING] /= d->reverb[SRD_SPACE];
    }
    else
    {
        spaceScatter = 0;
        d->reverb[SRD_VOLUME]  = .2f;
        d->reverb[SRD_DECAY]   = .4f;
        d->reverb[SRD_DAMPING] = 1;
    }

    // If the space is scattered, the reverb effect lessens.
    d->reverb[SRD_SPACE] /= (spaceScatter > .8 ? 10 : spaceScatter > .6 ? 4 : 1);

    // Normalize the reverb space [0..1]
    //   0= very small
    // .99= very large
    // 1.0= only for open areas (special case).
    d->reverb[SRD_SPACE] /= 120e6;
    if(d->reverb[SRD_SPACE] > .99)
        d->reverb[SRD_SPACE] = .99f;

    if(ceilingSurface().hasSkyMaskedMaterial() || floorSurface().hasSkyMaskedMaterial())
    {
        // An "open" sector.
        // It can still be small, in which case; reverb is diminished a bit.
        if(d->reverb[SRD_SPACE] > .5)
            d->reverb[SRD_VOLUME] = 1; // Full volume.
        else
            d->reverb[SRD_VOLUME] = .5f; // Small, but still open.

        d->reverb[SRD_SPACE] = 1;
    }
    else
    {
        // A "closed" sector.
        // Large spaces have automatically a bit more audible reverb.
        d->reverb[SRD_VOLUME] += d->reverb[SRD_SPACE] / 4;
    }

    if(d->reverb[SRD_VOLUME] > 1)
        d->reverb[SRD_VOLUME] = 1;
}

AudioEnvironmentFactors const &Sector::reverb() const
{
    return d->reverb;
}

coord_t Sector::roughArea() const
{
    return d->roughArea;
}

void Sector::updateRoughArea()
{
    d->roughArea = 0;

    foreach(BspLeaf *leaf, d->bspLeafs)
    {
        if(leaf->isDegenerate()) continue;

        AABoxd const &leafAABox = leaf->poly().aaBox();

        d->roughArea += (leafAABox.maxX - leafAABox.minX) *
                        (leafAABox.maxY - leafAABox.minY);
    }
}

bool Sector::isVisible() const
{
    return d->visible;
}

void Sector::markVisible(bool yes)
{
    d->visible = yes;
}

#endif // __CLIENT__

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
    int count = 0;
    foreach(Line *line, map.lines())
    {
        if(line->frontSectorPtr() == this ||
           line->backSectorPtr()  == this)
            ++count;
    }

    if(!count) return;

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
    plane->setIndexInSector(d->planes.count());

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
    int count = 0;
    foreach(BspLeaf *bspLeaf, map.bspLeafs())
    {
        if(bspLeaf->sectorPtr() == this)
            ++count;
    }

    if(!count) return;

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

AABoxd const &Sector::aaBox() const
{
    return d->aaBox;
}

void Sector::updateAABox()
{
    d->aaBox.clear();

    bool isFirst = false;
    foreach(BspLeaf *leaf, d->bspLeafs)
    {
        if(leaf->isDegenerate()) continue;

        AABoxd const &leafAABox = leaf->poly().aaBox();

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

bool Sector::pointInside(Vector2d const &point) const
{
    BspLeaf const &bspLeaf = map().bspLeafAt(point);
    return bspLeaf.sectorPtr() == this && bspLeaf.pointInside(point);
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
