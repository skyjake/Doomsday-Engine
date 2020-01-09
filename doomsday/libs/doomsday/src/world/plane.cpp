/** @file plane.h  World map plane.
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/plane.h"

#include "doomsday/world/map.h"
//#include "world/thinkers.h"
#include "doomsday/world/surface.h"
#include "doomsday/world/sector.h"
#include "doomsday/world/world.h"

//#include "dd_loop.h"  // frameTimePos
//#include "dd_main.h"  // App_Resources()

#include <de/LogBuffer>
#include <array>

namespace world {

using namespace de;

DE_PIMPL(Plane)
{
    Surface surface;
    ThinkerT<SoundEmitter> soundEmitter;

    dint indexInSector = -1;           ///< Index in the owning sector.

    ddouble height = 0;                ///< Current @em sharp height.
    ddouble heightTarget = 0;          ///< Target @em sharp height.
    ddouble speed = 0;                 ///< Movement speed (map space units per tic).

    Impl(Public *i)
        : Base(i)
        , surface(dynamic_cast<MapElement &>(*i))
    {
#ifdef __CLIENT__
        surface.audienceForMaterialChange() += this;
#endif
    }

    ~Impl()
    {
        DE_NOTIFY_PUBLIC(Deletion, i) i->planeBeingDeleted(self());

#ifdef __CLIENT__
        // Stop movement tracking of this plane.
        map().trackedPlanes().remove(thisPublic);
#endif
    }

    inline world::Map &map() const { return self().map(); }

    void setHeight(ddouble newHeight)
    {
        height = heightTarget = newHeight;

#ifdef __CLIENT__
        heightSmoothed = newHeight;
        oldHeight[0] = oldHeight[1] = newHeight;
#endif
    }

    void applySharpHeightChange(ddouble newHeight)
    {
        // No change?
        if(de::fequal(newHeight, height))
            return;

        height = newHeight;

        if (!World::ddMapSetup)
        {
            // Update the sound emitter origin for the plane.
            self().updateSoundEmitterOrigin();
        }

        notifyHeightChanged();

#ifdef __CLIENT__
        if(!ddMapSetup)
        {
            // Add ourself to tracked plane list (for movement interpolation).
            map().trackedPlanes().insert(thisPublic);
        }
#endif
    }

#ifdef __CLIENT__
    /// @todo Cache this result.
    Generator *tryFindGenerator()
    {
        Generator *found = nullptr;
        map().forAllGenerators([this, &found] (Generator &gen)
        {
            if(gen.plane == thisPublic)
            {
                found = &gen;
                return LoopAbort;  // Found it.
            }
            return LoopContinue;
        });
        return found;
    }
#endif

    void notifyHeightChanged()
    {
        DE_NOTIFY_PUBLIC(HeightChange, i) i->planeHeightChanged(self());
    }

#ifdef __CLIENT__
    void notifySmoothedHeightChanged()
    {
        DE_NOTIFY_PUBLIC(HeightSmoothedChange, i) i->planeHeightSmoothedChanged(self());
    }

    void surfaceMaterialChanged(Surface &suf)
    {
        DE_ASSERT(&suf == &surface);
        DE_UNUSED(suf);
        if (!::ddMapSetup && surface.hasMaterial())
        {
            res::Uri uri = surface.material().manifest().composeUri();
            self().spawnParticleGen(Def_GetGenerator(reinterpret_cast<uri_s *>(&uri)));
        }
    }
#endif

    DE_PIMPL_AUDIENCE(Deletion)
    DE_PIMPL_AUDIENCE(HeightChange)
#ifdef __CLIENT__
    DE_PIMPL_AUDIENCE(HeightSmoothedChange)
#endif
};

DE_AUDIENCE_METHOD(Plane, Deletion)
DE_AUDIENCE_METHOD(Plane, HeightChange)
#ifdef __CLIENT__
DE_AUDIENCE_METHOD(Plane, HeightSmoothedChange)
#endif

Plane::Plane(Sector &sector, const Vec3f &normal, ddouble height)
    : MapElement(DMU_PLANE, &sector)
    , d(new Impl(this))
{
    d->setHeight(height);
    setNormal(normal);
}

String Plane::description() const
{
    auto desc = Stringf(    _E(l) "Sector: "        _E(.)_E(i) "%i" _E(.)
                               " " _E(l) "Height: "        _E(.)_E(i) "%f" _E(.)
                               " " _E(l) "Height Target: " _E(.)_E(i) "%f" _E(.)
                               " " _E(l) "Speed: "         _E(.)_E(i) "%f" _E(.),
                  sector().indexInMap(),
                  height(),
                  heightTarget(),
                  speed());

    DE_DEBUG_ONLY(
        desc.prepend(Stringf(_E(b) "Plane " _E(.) "[%p]\n", this));
    )
    return desc + "\n" + surface().description();
}

Sector &Plane::sector()
{
    return parent().as<Sector>();
}

const Sector &Plane::sector() const
{
    return parent().as<Sector>();
}

dint Plane::indexInSector() const
{
    return d->indexInSector;
}

void Plane::setIndexInSector(dint newIndex)
{
    d->indexInSector = newIndex;
}

bool Plane::isSectorFloor() const
{
    return this == &sector().floor();
}

bool Plane::isSectorCeiling() const
{
    return this == &sector().ceiling();
}

Surface &Plane::surface()
{
    return d->surface;
}

const Surface &Plane::surface() const
{
    return d->surface;
}

Surface *Plane::surfacePtr() const
{
    return &d->surface;
}

void Plane::setNormal(const Vec3f &newNormal)
{
    d->surface.setNormal(newNormal);  // will normalize
}

SoundEmitter &Plane::soundEmitter()
{
    return d->soundEmitter;
}

const SoundEmitter &Plane::soundEmitter() const
{
    return d->soundEmitter;
}

void Plane::updateSoundEmitterOrigin()
{
    LOG_AS("Plane::updateSoundEmitterOrigin");

    d->soundEmitter->origin[0] = sector().soundEmitter().origin[0];
    d->soundEmitter->origin[1] = sector().soundEmitter().origin[1];
    d->soundEmitter->origin[2] = d->height;
}

ddouble Plane::height() const
{
    return d->height;
}

ddouble Plane::heightTarget() const
{
    return d->heightTarget;
}

ddouble Plane::speed() const
{
    return d->speed;
}

#ifdef __CLIENT__

ddouble Plane::heightSmoothed() const
{
    return d->heightSmoothed;
}

ddouble Plane::heightSmoothedDelta() const
{
    return d->heightSmoothedDelta;
}

void Plane::lerpSmoothedHeight()
{
    // Interpolate.
    d->heightSmoothedDelta = d->oldHeight[0] * (1 - frameTimePos)
                           + d->height * frameTimePos - d->height;

    ddouble newHeightSmoothed = d->height + d->heightSmoothedDelta;
    if(!de::fequal(d->heightSmoothed, newHeightSmoothed))
    {
        d->heightSmoothed = newHeightSmoothed;
        d->notifySmoothedHeightChanged();
    }
}

void Plane::resetSmoothedHeight()
{
    // Reset interpolation.
    d->heightSmoothedDelta = 0;

    ddouble newHeightSmoothed = d->oldHeight[0] = d->oldHeight[1] = d->height;
    if(!de::fequal(d->heightSmoothed, newHeightSmoothed))
    {
        d->heightSmoothed = newHeightSmoothed;
        d->notifySmoothedHeightChanged();
    }
}

void Plane::updateHeightTracking()
{
    d->oldHeight[0] = d->oldHeight[1];
    d->oldHeight[1] = d->height;

    if(!de::fequal(d->oldHeight[0], d->oldHeight[1]))
    {
        if(de::abs(d->oldHeight[0] - d->oldHeight[1]) >= MAX_SMOOTH_MOVE)
        {
            // Too fast: make an instantaneous jump.
            d->oldHeight[0] = d->oldHeight[1];
        }
    }
}

bool Plane::hasGenerator() const
{
    return d->tryFindGenerator() != nullptr;
}

Generator &Plane::generator() const
{
    if(Generator *gen = d->tryFindGenerator()) return *gen;
    /// @throw MissingGeneratorError No generator is attached.
    throw MissingGeneratorError("Plane::generator", "No generator is attached");
}

void Plane::spawnParticleGen(const ded_ptcgen_t *def)
{
    //if(!useParticles) return;

    if(!def) return;

    // Plane we spawn relative to may not be this one.
    dint relPlane = indexInSector();
    if(def->flags & Generator::SpawnCeiling)
        relPlane = Sector::Ceiling;
    if(def->flags & Generator::SpawnFloor)
        relPlane = Sector::Floor;

    if(relPlane != indexInSector())
    {
        sector().plane(relPlane).spawnParticleGen(def);
        return;
    }

    // Only planes in sectors with volume on the world X/Y axis can support generators.
    if(!sector().sideCount()) return;

    // Only one generator per plane.
    if(hasGenerator()) return;

    // Are we out of generators?
    Generator *gen = map().newGenerator();
    if(!gen) return;

    gen->count = def->particles;
    // Size of source sector might determine count.
    if(def->flags & Generator::Density)
    {
        gen->spawnRateMultiplier = sector().roughArea() / (128 * 128);
    }
    else
    {
        gen->spawnRateMultiplier = 1;
    }

    // Initialize the particle generator.
    gen->configureFromDef(def);
    gen->plane = this;

    // Is there a need to pre-simulate?
    gen->presimulate(def->preSim);
}

void Plane::addMover(ClPlaneMover &mover)
{
    // Forcibly remove the existing mover for this plane.
    if(d->mover)
    {
        LOG_MAP_XVERBOSE("Removing existing mover %p in sector #%i, plane %i",
                         &d->mover->thinker() << sector().indexInMap()
                         << indexInSector());

        map().thinkers().remove(d->mover->thinker());

        DE_ASSERT(!d->mover);
    }

    d->mover = &mover;
}

void Plane::removeMover(ClPlaneMover &mover)
{
    if(d->mover == &mover)
    {
        d->mover = nullptr;
    }
}

bool Plane::castsShadow() const
{
    if (auto *matAnim = d->surface.materialAnimator())
    {
        // Ensure we have up to date info about the material.
        matAnim->prepare();

        if (!matAnim->material().isDrawable()) return false;
        if (matAnim->material().isSkyMasked()) return false;

        return de::fequal(matAnim->glowStrength(), 0);
    }
    return false;
}

bool Plane::receivesShadow() const
{
    return castsShadow();  // Qualification is the same as with casting.
}

#endif // __CLIENT__

dint Plane::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_EMITTER: {
        const SoundEmitter *emitterPtr = &soundEmitter();
        args.setValue(DMT_PLANE_EMITTER, &emitterPtr, 0);
        break; }
    case DMU_SECTOR: {
        const Sector *secPtr = &sector();
        args.setValue(DMT_PLANE_SECTOR, &secPtr, 0);
        break; }
    case DMU_HEIGHT:
        args.setValue(DMT_PLANE_HEIGHT, &d->height, 0);
        break;
    case DMU_TARGET_HEIGHT:
        args.setValue(DMT_PLANE_TARGET, &d->heightTarget, 0);
        break;
    case DMU_SPEED:
        args.setValue(DMT_PLANE_SPEED, &d->speed, 0);
        break;
    default:
        return MapElement::property(args);
    }

    return false;  // Continue iteration.
}

dint Plane::setProperty(const DmuArgs &args)
{
    switch(args.prop)
    {
    case DMU_HEIGHT: {
        ddouble newHeight = d->height;
        args.value(DMT_PLANE_HEIGHT, &newHeight, 0);
        d->applySharpHeightChange(newHeight);
        break; }
    case DMU_TARGET_HEIGHT:
        args.value(DMT_PLANE_TARGET, &d->heightTarget, 0);
        break;
    case DMU_SPEED:
        args.value(DMT_PLANE_SPEED, &d->speed, 0);
        break;
    default:
        return MapElement::setProperty(args);
    }

    return false;  // Continue iteration.
}

} // namespace world

#ifdef __CLIENT__

#include "MaterialAnimator"
#include "render/rend_main.h"
#include <doomsday/world/materialmanifest.h>

#endif
