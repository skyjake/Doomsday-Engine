/** @file plane.h Map Plane.
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
#include "de_console.h"
#include "de_play.h"
#include "map/linedef.h"
#include "render/r_main.h" // frameTimePos

#include "map/surface.h"

using namespace de;

// $smoothplane: Maximum speed for a smoothed plane.
static int const MAX_SMOOTH_MOVE = 64;

DENG2_PIMPL(Plane)
{
    Instance(Public *i) : Base(i)
    {}

#ifdef __CLIENT__
    /**
     * To be called when the height changes to update the plotted decoration
     * origins for surfaces whose material offset is dependant upon this.
     */
    void markDependantSurfacesForDecorationUpdate()
    {
        // "Middle" planes have no dependent surfaces.
        if(self._type == Plane::Middle) return;

        // Mark the decor lights on the sides of this plane as requiring
        // an update.
        foreach(LineDef *line, self._sector->lines())
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
    _surface(dynamic_cast<MapElement &>(*this)),
    _sector(&sector), _height(height),
    d(new Instance(this))
{
    std::memset(_oldHeight, 0, sizeof(_oldHeight));
    _targetHeight = 0;
    _speed = 0;
    _visHeight = 0;
    _visHeightDelta = 0;
    _type = Floor;
    _inSectorIndex = 0;

    setNormal(normal);
}

Plane::~Plane()
{
    // If this plane is currently being watched, remove it.
    theMap->trackedPlanes().remove(this);

    // If this plane's surface is in the moving list, remove it.
    theMap->scrollingSurfaces().remove(&_surface);

#ifdef __CLIENT__

    // If this plane's surface is in the glowing list, remove it.
    theMap->glowingSurfaces().remove(&_surface);

    // If this plane's surface is in the decorated list, remove it.
    theMap->decoratedSurfaces().remove(&_surface);

    // Destroy the biassurfaces for this plane.
    foreach(BspLeaf *bspLeaf, _sector->bspLeafs())
    {
        DENG2_ASSERT(bspLeaf->_bsuf != 0);
        SB_DestroySurface(bspLeaf->_bsuf[_inSectorIndex]);
        if(_inSectorIndex < _sector->planeCount())
        {
            std::memmove(bspLeaf->_bsuf + _inSectorIndex, bspLeaf->_bsuf + _inSectorIndex + 1, sizeof(biassurface_t *));
        }
    }

#endif // __CLIENT__

    _sector->_planes.removeOne(this);
}

Sector &Plane::sector()
{
    return *_sector;
}

Sector const &Plane::sector() const
{
    return *_sector;
}

uint Plane::inSectorIndex() const
{
    return _inSectorIndex;
}

Surface &Plane::surface()
{
    return _surface;
}

Surface const &Plane::surface() const
{
    return _surface;
}

coord_t Plane::height() const
{
    return _height;
}

coord_t Plane::targetHeight() const
{
    return _targetHeight;
}

coord_t Plane::speed() const
{
    return _speed;
}

coord_t Plane::visHeight() const
{
    // $smoothplane
    return _visHeight;
}

coord_t Plane::visHeightDelta() const
{
    // $smoothplane
    return _visHeightDelta;
}

void Plane::lerpVisHeight()
{
    // $smoothplane
    _visHeightDelta = _oldHeight[0] * (1 - frameTimePos) + _height * frameTimePos - _height;

    // Visible plane height.
    _visHeight = _height + _visHeightDelta;

#ifdef __CLIENT__
    d->markDependantSurfacesForDecorationUpdate();
#endif
}

void Plane::resetVisHeight()
{
    // $smoothplane
    _visHeightDelta = 0;
    _visHeight = _oldHeight[0] = _oldHeight[1] = _height;

#ifdef __CLIENT__
    d->markDependantSurfacesForDecorationUpdate();
#endif
}

void Plane::updateHeightTracking()
{
    // $smoothplane
    _oldHeight[0] = _oldHeight[1];
    _oldHeight[1] = _height;

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
    V3f_Set(_surface._normal, newNormal.x, newNormal.y, newNormal.z);
    V3f_Normalize(_surface._normal);
    V3f_BuildTangents(_surface._tangent, _surface._bitangent, _surface._normal);

    _type = (_surface._normal[VZ] < 0? Ceiling : Floor);
}

Plane::Type Plane::type() const
{
    return _type;
}

int Plane::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_SECTOR:
        DMU_GetValue(DMT_PLANE_SECTOR, &_sector, &args, 0);
        break;
    case DMU_HEIGHT:
        DMU_GetValue(DMT_PLANE_HEIGHT, &_height, &args, 0);
        break;
    case DMU_TARGET_HEIGHT:
        DMU_GetValue(DMT_PLANE_TARGET, &_targetHeight, &args, 0);
        break;
    case DMU_SPEED:
        DMU_GetValue(DMT_PLANE_SPEED, &_speed, &args, 0);
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
    case DMU_HEIGHT:
        DMU_SetValue(DMT_PLANE_HEIGHT, &_height, &args, 0);
        if(!ddMapSetup)
        {
            if(theMap)
            {
                theMap->trackedPlanes().insert(this);
            }

#ifdef __CLIENT__
            d->markDependantSurfacesForDecorationUpdate();
#endif
        }
        break;
    case DMU_TARGET_HEIGHT:
        DMU_SetValue(DMT_PLANE_TARGET, &_targetHeight, &args, 0);
        break;
    case DMU_SPEED:
        DMU_SetValue(DMT_PLANE_SPEED, &_speed, &args, 0);
        break;
    default:
        /// @throw WritePropertyError  The requested property is not writable.
        throw WritePropertyError("Plane::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}
