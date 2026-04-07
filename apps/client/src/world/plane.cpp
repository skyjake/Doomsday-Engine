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

#include "world/plane.h"
#include "resource/materialanimator.h"
#include "render/rend_main.h"
#include "world/map.h"
#include <doomsday/world/materialmanifest.h>
#include <doomsday/world/sector.h>
#include <doomsday/world/thinkers.h>

#include "dd_loop.h"  // frameTimePos
#include "dd_main.h"  // App_Resources()

#include <de/logbuffer.h>

using namespace de;

Plane::Plane(world::Sector &sector, const Vec3f &normal, double height)
    : world::Plane(sector, normal, height)
{
    setHeight(height); // init smooth height

    surface().audienceForMaterialChange() += this;
    audienceForHeightChange() += [this]() {
        if (!world::World::ddMapSetup)
        {
            // Add ourself to tracked plane list (for movement interpolation).
            map().trackedPlanes().insert(this);
        }
    };
}

Plane::~Plane()
{
    // Stop movement tracking of this plane.
    map().trackedPlanes().remove(this);
}

Map &Plane::map() const
{
    return world::Plane::map().as<Map>();
}

void Plane::setHeight(double newHeight)
{
    world::Plane::setHeight(newHeight);

    _heightSmoothed = newHeight;
    _oldHeight[0] = _oldHeight[1] = newHeight;
}

double Plane::heightSmoothed() const
{
    return _heightSmoothed;
}

double Plane::heightSmoothedDelta() const
{
    return _heightSmoothedDelta;
}

void Plane::lerpSmoothedHeight()
{
    // Interpolate.
    _heightSmoothedDelta =
        _oldHeight[0] * (1 - frameTimePos) + _height * frameTimePos - _height;

    double newHeightSmoothed = _height + _heightSmoothedDelta;
    if(!de::fequal(_heightSmoothed, newHeightSmoothed))
    {
        _heightSmoothed = newHeightSmoothed;
        notifySmoothedHeightChanged();
    }
}

void Plane::resetSmoothedHeight()
{
    // Reset interpolation.
    _heightSmoothedDelta = 0;

    double newHeightSmoothed = _oldHeight[0] = _oldHeight[1] = _height;
    if(!de::fequal(_heightSmoothed, newHeightSmoothed))
    {
        _heightSmoothed = newHeightSmoothed;
        notifySmoothedHeightChanged();
    }
}

void Plane::updateHeightTracking()
{
    _oldHeight[0] = _oldHeight[1];
    _oldHeight[1] = _height;

    if(!de::fequal(_oldHeight[0], _oldHeight[1]))
    {
        if(de::abs(_oldHeight[0] - _oldHeight[1]) >= MAX_SMOOTH_MOVE)
        {
            // Too fast: make an instantaneous jump.
            _oldHeight[0] = _oldHeight[1];
        }
    }
}

bool Plane::hasGenerator() const
{
    return tryFindGenerator() != nullptr;
}

Generator &Plane::generator() const
{
    if (auto *gen = tryFindGenerator()) return *gen;
    /// @throw MissingGeneratorError No generator is attached.
    throw MissingGeneratorError("Plane::generator", "No generator is attached");
}

void Plane::spawnParticleGen(const ded_ptcgen_t *def)
{
    if (!def) return;

    // Plane we spawn relative to may not be this one.
    int relPlane = indexInSector();
    if (def->flags & Generator::SpawnCeiling) relPlane = world::Sector::Ceiling;
    if (def->flags & Generator::SpawnFloor) relPlane = world::Sector::Floor;

    if (relPlane != indexInSector())
    {
        sector().plane(relPlane).as<Plane>().spawnParticleGen(def);
        return;
    }

    // Only planes in sectors with volume on the world X/Y axis can support generators.
    if (!sector().sideCount()) return;

    // Only one generator per plane.
    if (hasGenerator()) return;

    // Are we out of generators?
    Generator *gen = map().newGenerator();
    if (!gen) return;

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
    if (_mover)
    {
        LOG_MAP_XVERBOSE("Removing existing mover %p in sector #%i, plane %i",
                         &_mover->thinker() << sector().indexInMap() << indexInSector());

        map().thinkers().remove(_mover->thinker());

        DE_ASSERT(!_mover);
    }

    _mover = &mover;
}

void Plane::removeMover(ClPlaneMover &mover)
{
    if (_mover == &mover)
    {
        _mover = nullptr;
    }
}

bool Plane::castsShadow() const
{
    if (auto *matAnim = surface().materialAnimator())
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

Generator *Plane::tryFindGenerator() const
{
    /// @todo Cache this result.
    
    Generator *found = nullptr;
    map().forAllGenerators([this, &found](Generator &gen) {
        if (gen.plane == this)
        {
            found = &gen;
            return LoopAbort; // Found it.
        }
        return LoopContinue;
    });
    return found;
}

void Plane::notifySmoothedHeightChanged()
{
    DE_NOTIFY_VAR(HeightSmoothedChange, i) i->planeHeightSmoothedChanged(*this);
}

void Plane::surfaceMaterialChanged(world::Surface &)
{
    if (!world::World::ddMapSetup && surface().hasMaterial())
    {
        const res::Uri uri = surface().material().manifest().composeUri();
        spawnParticleGen(Def_GetGenerator(reinterpret_cast<const uri_s *>(&uri)));
    }
}
