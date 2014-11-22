/** @file plane.h  World map plane.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "world/plane.h"

#include <de/Log>
#include "dd_loop.h" // frameTimePos
#include "world/map.h"
#include "world/thinkers.h"
#include "world/worldsystem.h" /// ddMapSetup
#include "Surface"
#include "Sector"

using namespace de;

DENG2_PIMPL(Plane)
{
    Surface surface;
    ThinkerT<SoundEmitter> soundEmitter;

    int indexInSector = -1;           ///< Index in the owning sector.

    coord_t height = 0;               ///< Current @em sharp height.
    coord_t targetHeight = 0;         ///< Target @em sharp height.
    coord_t speed = 0;                ///< Movement speed (map space units per tic).

#ifdef __CLIENT__
    coord_t oldHeight[2];             ///< @em sharp height change tracking buffer (for smoothing).
    coord_t heightSmoothed = 0;       ///< @ref height (smoothed).
    coord_t heightSmoothedDelta = 0;  ///< Delta between the current @em sharp height and the visual height.
    ClPlaneMover *mover = nullptr;    ///< The current mover.
#endif

    Instance(Public *i) : Base(i), surface(dynamic_cast<MapElement &>(*i))
    {
#ifdef __CLIENT__
        de::zap(oldHeight);
#endif
    }

    ~Instance()
    {
        DENG2_FOR_PUBLIC_AUDIENCE2(Deletion, i) i->planeBeingDeleted(self);

#ifdef __CLIENT__
        // Stop movement tracking of this plane.
        map().trackedPlanes().remove(&self);
#endif
    }

    inline Map &map() const { return self.map(); }

    void setHeight(coord_t newHeight)
    {
        height = targetHeight = newHeight;

#ifdef __CLIENT__
        heightSmoothed = newHeight;
        oldHeight[0] = oldHeight[1] = newHeight;
#endif
    }

    void applySharpHeightChange(coord_t newHeight)
    {
        // No change?
        if(de::fequal(newHeight, height))
            return;

        height = newHeight;

        if(!ddMapSetup)
        {
            // Update the sound emitter origin for the plane.
            self.updateSoundEmitterOrigin();

#ifdef __CLIENT__
            // We need the decorations updated.
            /// @todo optimize: Translation on the world up axis would be a
            /// trivial operation to perform, which, would not require plotting
            /// decorations again. This frequent case should be designed for.
            surface.markForDecorationUpdate();
#endif
        }

        notifyHeightChanged();

#ifdef __CLIENT__
        if(!ddMapSetup)
        {
            // Add ourself to tracked plane list (for movement interpolation).
            map().trackedPlanes().insert(&self);
        }
#endif
    }

#ifdef __CLIENT__
    struct findgeneratorworker_params_t
    {
        Plane *plane;
        Generator *found;
    };

    static int findGeneratorWorker(Generator *gen, void *context)
    {
        findgeneratorworker_params_t *p = (findgeneratorworker_params_t *)context;
        if(gen->plane == p->plane)
        {
            p->found = gen;
            return true; // Stop iteration.
        }
        return false; // Continue iteration.
    }

    /// @todo Cache this result.
    Generator *findGenerator()
    {
        findgeneratorworker_params_t parm;
        parm.plane = thisPublic;
        parm.found = 0;
        map().generatorIterator(findGeneratorWorker, &parm);
        return parm.found;
    }
#endif

    void notifyHeightChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE2(HeightChange, i) i->planeHeightChanged(self);
    }

#ifdef __CLIENT__
    void notifySmoothedHeightChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE2(HeightSmoothedChange, i) i->planeHeightSmoothedChanged(self);
    }
#endif

    DENG2_PIMPL_AUDIENCE(Deletion)
    DENG2_PIMPL_AUDIENCE(HeightChange)
#ifdef __CLIENT__
    DENG2_PIMPL_AUDIENCE(HeightSmoothedChange)
#endif
};

DENG2_AUDIENCE_METHOD(Plane, Deletion)
DENG2_AUDIENCE_METHOD(Plane, HeightChange)
#ifdef __CLIENT__
DENG2_AUDIENCE_METHOD(Plane, HeightSmoothedChange)
#endif

Plane::Plane(Sector &sector, Vector3f const &normal, coord_t height)
    : MapElement(DMU_PLANE, &sector)
    , d(new Instance(this))
{
    d->setHeight(height);
    setNormal(normal);
}

Sector &Plane::sector()
{
    return parent().as<Sector>();
}

Sector const &Plane::sector() const
{
    return parent().as<Sector>();
}

int Plane::indexInSector() const
{
    return d->indexInSector;
}

void Plane::setIndexInSector(int newIndex)
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

Surface const &Plane::surface() const
{
    return d->surface;
}

void Plane::setNormal(Vector3f const &newNormal)
{
    d->surface.setNormal(newNormal); // will normalize
}

SoundEmitter &Plane::soundEmitter()
{
    return d->soundEmitter;
}

SoundEmitter const &Plane::soundEmitter() const
{
    return d->soundEmitter;
}

void Plane::updateSoundEmitterOrigin()
{
    LOG_AS("Plane::updateSoundEmitterOrigin");

    d->soundEmitter->origin[VX] = sector().soundEmitter().origin[VX];
    d->soundEmitter->origin[VY] = sector().soundEmitter().origin[VY];
    d->soundEmitter->origin[VZ] = d->height;
}

coord_t Plane::height() const
{
    return d->height;
}

coord_t Plane::targetHeight() const
{
    return d->targetHeight;
}

coord_t Plane::speed() const
{
    return d->speed;
}

#ifdef __CLIENT__

coord_t Plane::heightSmoothed() const
{
    return d->heightSmoothed;
}

coord_t Plane::heightSmoothedDelta() const
{
    return d->heightSmoothedDelta;
}

void Plane::lerpSmoothedHeight()
{
    // Interpolate.
    d->heightSmoothedDelta = d->oldHeight[0] * (1 - frameTimePos)
                           + d->height * frameTimePos - d->height;

    coord_t newHeightSmoothed = d->height + d->heightSmoothedDelta;
    if(!de::fequal(d->heightSmoothed, newHeightSmoothed))
    {
        d->heightSmoothed = newHeightSmoothed;
        d->notifySmoothedHeightChanged();
        d->surface.markForDecorationUpdate();
    }
}

void Plane::resetSmoothedHeight()
{
    // Reset interpolation.
    d->heightSmoothedDelta = 0;

    coord_t newHeightSmoothed = d->oldHeight[0] = d->oldHeight[1] = d->height;
    if(!de::fequal(d->heightSmoothed, newHeightSmoothed))
    {
        d->heightSmoothed = newHeightSmoothed;
        d->notifySmoothedHeightChanged();
        d->surface.markForDecorationUpdate();
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
        d->surface.markForDecorationUpdate();
    }
}

bool Plane::hasGenerator() const
{
    return d->findGenerator() != 0;
}

Generator &Plane::generator() const
{
    if(Generator *gen = d->findGenerator())
    {
        return *gen;
    }
    /// @throw MissingGeneratorError No generator is attached.
    throw MissingGeneratorError("Plane::generator", "No generator is attached");
}

void Plane::spawnParticleGen(ded_ptcgen_t const *def)
{
    //if(!useParticles) return;

    if(!def) return;

    // Plane we spawn relative to may not be this one.
    int relPlane = indexInSector();
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
        LOG_MAP_XVERBOSE("Removing existing mover %p in sector #%i, plane %i")
                << &d->mover->thinker()
                << sector().indexInMap()
                << indexInSector();

        map().thinkers().remove(d->mover->thinker());

        DENG2_ASSERT(!d->mover);
    }

    d->mover = &mover;
}

void Plane::removeMover(ClPlaneMover &mover)
{
    if(d->mover == &mover)
    {
        d->mover = 0;
    }
}

#endif // __CLIENT__

int Plane::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_EMITTER:
        args.setValue(DMT_PLANE_EMITTER, d->soundEmitter, 0);
        break;
    case DMU_SECTOR: {
        Sector const *secPtr = &sector();
        args.setValue(DMT_PLANE_SECTOR, &secPtr, 0);
        break; }
    case DMU_HEIGHT:
        args.setValue(DMT_PLANE_HEIGHT, &d->height, 0);
        break;
    case DMU_TARGET_HEIGHT:
        args.setValue(DMT_PLANE_TARGET, &d->targetHeight, 0);
        break;
    case DMU_SPEED:
        args.setValue(DMT_PLANE_SPEED, &d->speed, 0);
        break;
    default:
        return MapElement::property(args);
    }

    return false; // Continue iteration.
}

int Plane::setProperty(DmuArgs const &args)
{
    switch(args.prop)
    {
    case DMU_HEIGHT: {
        coord_t newHeight = d->height;
        args.value(DMT_PLANE_HEIGHT, &newHeight, 0);
        d->applySharpHeightChange(newHeight);
        break; }
    case DMU_TARGET_HEIGHT:
        args.value(DMT_PLANE_TARGET, &d->targetHeight, 0);
        break;
    case DMU_SPEED:
        args.value(DMT_PLANE_SPEED, &d->speed, 0);
        break;
    default:
        return MapElement::setProperty(args);
    }

    return false; // Continue iteration.
}
