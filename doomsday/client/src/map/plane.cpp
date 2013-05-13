/** @file plane.h World Map Plane.
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

#include "audio/s_environ.h"
#include "Line"
#include "map/gamemap.h" /// @todo remove me.
#include "map/r_world.h" // ddMapSetup
#include "render/r_main.h" // frameTimePos

#include "map/plane.h"

using namespace de;

// $smoothplane: Maximum speed for a smoothed plane.
int const Plane::MAX_SMOOTH_MOVE = 64;

DENG2_PIMPL(Plane)
{
    /// Sector that owns the plane.
    Sector *sector;

    /// Sound emitter.
    ddmobj_base_t soundEmitter;

    /// Index of the plane in the owning sector.
    int inSectorIndex;

    /// Current @em sharp height relative to @c 0 on the map up axis (positive is up).
    coord_t height;

    /// @em sharp height change tracking buffer (for smoothing).
    coord_t oldHeight[2];

    /// Target @em sharp height.
    coord_t targetHeight;

    /// Visual plane height (smoothed).
    coord_t visHeight;

    /// Delta between the current @em sharp height and the visual height.
    coord_t visHeightDelta;

    /// Movement speed (map space units per tic).
    coord_t speed;

    /// Plane surface.
    Surface surface;

    /// Logical type of the plane. @todo resolve ambiguous meaning -ds
    Type type;

    Instance(Public *i, Sector &sector, coord_t height)
        : Base(i),
          sector(&sector),
          inSectorIndex(-1),
          height(height),
          targetHeight(height),
          visHeight(height),
          visHeightDelta(0),
          speed(0),
          surface(dynamic_cast<MapElement &>(*i)),
          type(Floor)
    {
        oldHeight[0] = oldHeight[1] = height;
        std::memset(&soundEmitter, 0, sizeof(soundEmitter));
    }

    ~Instance()
    {
        DENG_ASSERT(theMap != 0);

        // If this plane is currently being watched, remove it.
        /// @todo GameMap should observe Deletion.
        theMap->trackedPlanes().remove(&self);

        // If this plane's surface is in the moving list, remove it.
        /// @todo GameMap should observe Deletion.
        theMap->scrollingSurfaces().remove(&surface);

#ifdef __CLIENT__

        // If this plane's surface is in the glowing list, remove it.
        /// @todo GameMap should observe Deletion.
        theMap->glowingSurfaces().remove(&surface);

        // If this plane's surface is in the decorated list, remove it.
        /// @todo GameMap should observe Deletion.
        theMap->decoratedSurfaces().remove(&surface);

#endif // __CLIENT__

        DENG2_FOR_PUBLIC_AUDIENCE(Deletion, i) i->planeBeingDeleted(self);
    }

    void notifyHeightChanged(coord_t oldHeight)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(HeightChange, i)
        {
            i->planeHeightChanged(self, oldHeight);
        }
    }

    void applySharpHeightChange(coord_t newHeight)
    {
        // No change?
        if(de::fequal(newHeight, height))
            return;

        coord_t oldHeight = height;
        height = newHeight;

        if(!ddMapSetup)
        {
            // Update the sound emitter origin for the plane.
            self.updateSoundEmitterOrigin();

#ifdef __CLIENT__
            // We need the decorations updated.
            surface.markAsNeedingDecorationUpdate();
#endif
        }

        // Notify interested parties of the change.
        notifyHeightChanged(oldHeight);

        /// @todo GameMap should observe.
        if(!ddMapSetup)
        {
            if(theMap)
            {
                theMap->trackedPlanes().insert(&self);
            }

#ifdef __CLIENT__
            markDependantSurfacesForDecorationUpdate();
#endif
        }
    }

#ifdef __CLIENT__
    /**
     * To be called when the height changes to update the plotted decoration
     * origins for surfaces whose material offset is dependant upon this.
     *
     * @todo Sector should observe instead.
     */
    void markDependantSurfacesForDecorationUpdate()
    {
        if(ddMapSetup) return;

        // "Middle" planes have no dependent surfaces.
        if(type == Plane::Middle) return;

        // Mark the decor lights on the sides of this plane as requiring
        // an update.
        foreach(Line::Side *side, sector->sides())
        {
            if(side->hasSections())
            {
                side->middle().markAsNeedingDecorationUpdate();
                side->bottom().markAsNeedingDecorationUpdate();
                side->top().markAsNeedingDecorationUpdate();
            }

            if(side->back().hasSections())
            {
                Line::Side &back = side->back();
                back.middle().markAsNeedingDecorationUpdate();
                back.bottom().markAsNeedingDecorationUpdate();
                back.top().markAsNeedingDecorationUpdate();
            }
        }
    }
#endif // __CLIENT__
};

Plane::Plane(Sector &sector, Vector3f const &normal, coord_t height)
    : MapElement(DMU_PLANE), d(new Instance(this, sector, height))
{
    setNormal(normal);
}

Sector &Plane::sector()
{
    return *d->sector;
}

Sector const &Plane::sector() const
{
    return *d->sector;
}

int Plane::inSectorIndex() const
{
    return d->inSectorIndex;
}

void Plane::setInSectorIndex(int newIndex)
{
    d->inSectorIndex = newIndex;
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

ddmobj_base_t &Plane::soundEmitter()
{
    return d->soundEmitter;
}

ddmobj_base_t const &Plane::soundEmitter() const
{
    return const_cast<ddmobj_base_t const &>(const_cast<Plane &>(*this).soundEmitter());
}

void Plane::updateSoundEmitterOrigin()
{
    LOG_AS("Plane::updateSoundEmitterOrigin");

    d->soundEmitter.origin[VX] = d->sector->soundEmitter().origin[VX];
    d->soundEmitter.origin[VY] = d->sector->soundEmitter().origin[VY];
    d->soundEmitter.origin[VZ] = d->height;
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

coord_t Plane::visHeight() const
{
    // $smoothplane
    return d->visHeight;
}

coord_t Plane::visHeightDelta() const
{
    // $smoothplane
    return d->visHeightDelta;
}

void Plane::lerpVisHeight()
{
    // $smoothplane
    d->visHeightDelta = d->oldHeight[0] * (1 - frameTimePos) + d->height * frameTimePos - d->height;

    // Visible plane height.
    d->visHeight = d->height + d->visHeightDelta;

#ifdef __CLIENT__
    d->markDependantSurfacesForDecorationUpdate();
#endif
}

void Plane::resetVisHeight()
{
    // $smoothplane
    d->visHeightDelta = 0;
    d->visHeight = d->oldHeight[0] = d->oldHeight[1] = d->height;

#ifdef __CLIENT__
    d->markDependantSurfacesForDecorationUpdate();
#endif
}

void Plane::updateHeightTracking()
{
    // $smoothplane
    d->oldHeight[0] = d->oldHeight[1];
    d->oldHeight[1] = d->height;

    if(d->oldHeight[0] != d->oldHeight[1])
    {
        if(de::abs(d->oldHeight[0] - d->oldHeight[1]) >= MAX_SMOOTH_MOVE)
        {
            // Too fast: make an instantaneous jump.
            d->oldHeight[0] = d->oldHeight[1];
        }
    }
}

void Plane::setNormal(Vector3f const &newNormal)
{
    d->surface.setNormal(newNormal); // will normalize

    d->type = (d->surface.normal().z < 0? Ceiling : Floor);
}

Plane::Type Plane::type() const
{
    return d->type;
}

int Plane::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_EMITTER:
        DMU_GetValue(DMT_PLANE_EMITTER, &d->soundEmitter, &args, 0);
        break;
    case DMU_SECTOR:
        DMU_GetValue(DMT_PLANE_SECTOR, &d->sector, &args, 0);
        break;
    case DMU_HEIGHT:
        DMU_GetValue(DMT_PLANE_HEIGHT, &d->height, &args, 0);
        break;
    case DMU_TARGET_HEIGHT:
        DMU_GetValue(DMT_PLANE_TARGET, &d->targetHeight, &args, 0);
        break;
    case DMU_SPEED:
        DMU_GetValue(DMT_PLANE_SPEED, &d->speed, &args, 0);
        break;
    default:
        return MapElement::property(args);
    }

    return false; // Continue iteration.
}

int Plane::setProperty(setargs_t const &args)
{
    switch(args.prop)
    {
    case DMU_HEIGHT: {
        coord_t newHeight = d->height;
        DMU_SetValue(DMT_PLANE_HEIGHT, &newHeight, &args, 0);
        d->applySharpHeightChange(newHeight);
        break; }
    case DMU_TARGET_HEIGHT:
        DMU_SetValue(DMT_PLANE_TARGET, &d->targetHeight, &args, 0);
        break;
    case DMU_SPEED:
        DMU_SetValue(DMT_PLANE_SPEED, &d->speed, &args, 0);
        break;
    default:
        return MapElement::setProperty(args);
    }

    return false; // Continue iteration.
}
