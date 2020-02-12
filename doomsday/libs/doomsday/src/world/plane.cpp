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
#include "doomsday/world/surface.h"
#include "doomsday/world/sector.h"
#include "doomsday/world/world.h"

#include <de/LogBuffer>
#include <array>

namespace world {

using namespace de;

DE_PIMPL(Plane)
{
    std::unique_ptr<Surface> surface;
    ThinkerT<SoundEmitter>   soundEmitter;

    int indexInSector = -1;     // Index in the owning sector.

    double heightTarget = 0;    // Target height.
    double targetSetAt = 0;     // When the target was set.
    double initialHeight = 0;   // Height when the target was set.
    double speed = 0;           // Movement speed (map space units per tic).

    Impl(Public *i)
        : Base(i)
        , surface(Factory::newSurface(*dynamic_cast<MapElement *>(i)))
    {}

    ~Impl()
    {
        DE_NOTIFY_PUBLIC(Deletion, i) i->planeBeingDeleted(self());
    }

    inline world::Map &map() const { return self().map(); }

    void maybeBeginNewMovement(double target)
    {
        if (!fequal(target, heightTarget))
        {
            auto &wld = world::World::get();

            heightTarget  = target;
            targetSetAt   = wld.time();
            initialHeight = self().height();

            wld.notifyBeginPlaneMovement(self());
        }
    }

    void applySharpHeightChange(ddouble newHeight)
    {
        // No change?
        if(de::fequal(newHeight, self()._height))
            return;

        self()._height = newHeight;

        if (!World::ddMapSetup)
        {
            // Update the sound emitter origin for the plane.
            self().updateSoundEmitterOrigin();
        }

        DE_NOTIFY_PUBLIC(HeightChange, i) i->planeHeightChanged(self());
    }

    DE_PIMPL_AUDIENCE(Deletion)
    DE_PIMPL_AUDIENCE(HeightChange)
};

DE_AUDIENCE_METHOD(Plane, Deletion)
DE_AUDIENCE_METHOD(Plane, HeightChange)

Plane::Plane(Sector &sector, const Vec3f &normal, ddouble height)
    : MapElement(DMU_PLANE, &sector)
    , d(new Impl(this))
{
    setHeight(height);
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
    return *d->surface;
}

const Surface &Plane::surface() const
{
    return *d->surface;
}

Surface *Plane::surfacePtr() const
{
    return d->surface.get();
}

void Plane::setNormal(const Vec3f &newNormal)
{
    d->surface->setNormal(newNormal);  // will normalize
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
    d->soundEmitter->origin[2] = _height;
}

void Plane::setHeight(double newHeight)
{
    _height = d->heightTarget = newHeight;
    d->maybeBeginNewMovement(newHeight);
}

double Plane::movementBeganAt() const
{
    return d->targetSetAt;
}

double Plane::initialHeightOfMovement() const
{
    return d->initialHeight;
}

ddouble Plane::height() const
{
    return _height;
}

ddouble Plane::heightTarget() const
{
    return d->heightTarget;
}

ddouble Plane::speed() const
{
    return d->speed;
}

dint Plane::property(DmuArgs &args) const
{
    switch (args.prop)
    {
        case DMU_EMITTER: {
            const SoundEmitter *emitterPtr = &soundEmitter();
            args.setValue(DMT_PLANE_EMITTER, &emitterPtr, 0);
            break;
        }
        case DMU_SECTOR: {
            const Sector *secPtr = &sector();
            args.setValue(DMT_PLANE_SECTOR, &secPtr, 0);
            break;
        }
        case DMU_HEIGHT:        args.setValue(DMT_PLANE_HEIGHT, &_height, 0); break;
        case DMU_TARGET_HEIGHT: args.setValue(DMT_PLANE_TARGET, &d->heightTarget, 0); break;
        case DMU_SPEED:         args.setValue(DMT_PLANE_SPEED, &d->speed, 0); break;
        default: return MapElement::property(args);
    }

    return false;  // Continue iteration.
}

dint Plane::setProperty(const DmuArgs &args)
{
    switch (args.prop)
    {
        case DMU_HEIGHT: {
            ddouble newHeight = _height;
            args.value(DMT_PLANE_HEIGHT, &newHeight, 0);
            d->applySharpHeightChange(newHeight);
            break;
        }
        case DMU_TARGET_HEIGHT: {
            double newTarget;
            args.value(DMT_PLANE_TARGET, &newTarget, 0);
            d->maybeBeginNewMovement(newTarget);
            break;
        }
        case DMU_SPEED: {
            args.value(DMT_PLANE_SPEED, &d->speed, 0);
            break;
        }
        default: return MapElement::setProperty(args);
    }
    return false;  // Continue iteration.
}

} // namespace world
