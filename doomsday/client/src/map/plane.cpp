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
#include "map/linedef.h"
#include "map/gamemap.h"
#include "render/r_main.h" // frameTimePos

#include "map/plane.h"

using namespace de;

// $smoothplane: Maximum speed for a smoothed plane.
int const Plane::MAX_SMOOTH_MOVE = 64;

DENG2_PIMPL(Plane)
{
    /// Sector that owns the plane.
    Sector *sector;

    /// Index of the plane in the owning sector.
    uint inSectorIndex;

    /// Current @em sharp height relative to @c 0 on the map up axis (positive is up).
    coord_t height;

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
          inSectorIndex(0),
          height(height),
          visHeightDelta(0),
          speed(0),
          surface(dynamic_cast<MapElement &>(*i)),
          type(Floor)
    {}

    ~Instance()
    {
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
            surface.updateSoundEmitterOrigin();

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
        foreach(LineDef *line, sector->lines())
        {
            SideDef &frontSideDef = line->frontSideDef();
            frontSideDef.surface(SS_MIDDLE).markAsNeedingDecorationUpdate();
            frontSideDef.surface(SS_BOTTOM).markAsNeedingDecorationUpdate();
            frontSideDef.surface(SS_TOP).markAsNeedingDecorationUpdate();

            if(line->hasBackSideDef())
            {
                SideDef &backSideDef = line->backSideDef();
                backSideDef.surface(SS_MIDDLE).markAsNeedingDecorationUpdate();
                backSideDef.surface(SS_BOTTOM).markAsNeedingDecorationUpdate();
                backSideDef.surface(SS_TOP).markAsNeedingDecorationUpdate();
            }
        }
    }
#endif // __CLIENT__
};

Plane::Plane(Sector &sector, Vector3f const &normal, coord_t height)
    : MapElement(DMU_PLANE),
      _targetHeight(height),
      _visHeight(height),
      d(new Instance(this, sector, height))
{
    _oldHeight[0] = _oldHeight[1] = d->height;
    setNormal(normal);
}

Plane::~Plane()
{
    DENG2_FOR_AUDIENCE(Deletion, i) i->planeBeingDeleted(*this);
}

Sector &Plane::sector()
{
    return *d->sector;
}

Sector const &Plane::sector() const
{
    return *d->sector;
}

uint Plane::inSectorIndex() const
{
    return d->inSectorIndex;
}

void Plane::setInSectorIndex(uint newIndex)
{
    d->inSectorIndex = newIndex;
}

Surface &Plane::surface()
{
    return d->surface;
}

Surface const &Plane::surface() const
{
    return d->surface;
}

coord_t Plane::height() const
{
    return d->height;
}

coord_t Plane::targetHeight() const
{
    return _targetHeight;
}

coord_t Plane::speed() const
{
    return d->speed;
}

coord_t Plane::visHeight() const
{
    // $smoothplane
    return _visHeight;
}

coord_t Plane::visHeightDelta() const
{
    // $smoothplane
    return d->visHeightDelta;
}

void Plane::lerpVisHeight()
{
    // $smoothplane
    d->visHeightDelta = _oldHeight[0] * (1 - frameTimePos) + d->height * frameTimePos - d->height;

    // Visible plane height.
    _visHeight = d->height + d->visHeightDelta;

#ifdef __CLIENT__
    d->markDependantSurfacesForDecorationUpdate();
#endif
}

void Plane::resetVisHeight()
{
    // $smoothplane
    d->visHeightDelta = 0;
    _visHeight = _oldHeight[0] = _oldHeight[1] = d->height;

#ifdef __CLIENT__
    d->markDependantSurfacesForDecorationUpdate();
#endif
}

void Plane::updateHeightTracking()
{
    // $smoothplane
    _oldHeight[0] = _oldHeight[1];
    _oldHeight[1] = d->height;

    if(_oldHeight[0] != _oldHeight[1])
    {
        if(de::abs(_oldHeight[0] - _oldHeight[1]) >= MAX_SMOOTH_MOVE)
        {
            // Too fast: make an instantaneous jump.
            _oldHeight[0] = _oldHeight[1];
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
    case DMU_SECTOR:
        DMU_GetValue(DMT_PLANE_SECTOR, &d->sector, &args, 0);
        break;
    case DMU_HEIGHT:
        DMU_GetValue(DMT_PLANE_HEIGHT, &d->height, &args, 0);
        break;
    case DMU_TARGET_HEIGHT:
        DMU_GetValue(DMT_PLANE_TARGET, &_targetHeight, &args, 0);
        break;
    case DMU_SPEED:
        DMU_GetValue(DMT_PLANE_SPEED, &d->speed, &args, 0);
        break;
    default:
        /// @throw UnknownPropertyError  The requested property does not exist.
        throw UnknownPropertyError("Plane::property", QString("Property '%1' is unknown").arg(DMU_Str(args.prop)));
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
        DMU_SetValue(DMT_PLANE_TARGET, &_targetHeight, &args, 0);
        break;
    case DMU_SPEED:
        DMU_SetValue(DMT_PLANE_SPEED, &d->speed, &args, 0);
        break;
    default:
        /// @throw WritePropertyError  The requested property is not writable.
        throw WritePropertyError("Plane::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}
