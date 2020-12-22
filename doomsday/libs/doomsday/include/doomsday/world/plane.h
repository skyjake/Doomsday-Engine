/** @file plane.h  Map plane.
 * @ingroup world
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

#pragma once

#include <de/error.h>
#include <de/observers.h>
#include <de/string.h>
#include <de/vector.h>
#include "mapelement.h"

#include "dd_share.h"  // SoundEmitter

namespace world {

class Sector;
class Surface;

/**
 * World map sector plane.
 */
class LIBDOOMSDAY_PUBLIC Plane : public MapElement
{
    DE_NO_COPY  (Plane)
    DE_NO_ASSIGN(Plane)

public:

    /// Notified when the plane is about to be deleted.
    DE_AUDIENCE(Deletion, void planeBeingDeleted(const Plane &plane))

    /// Notified whenever a @em sharp height change occurs.
    DE_AUDIENCE(HeightChange, void planeHeightChanged(Plane &plane))

    /// Maximum speed for a smoothed plane.
    static const int MAX_SMOOTH_MOVE = 64;

public:
    /**
     * Construct a new plane.
     *
     * @param sector  Sector parent which will own the plane.
     * @param normal  Normal of the plane (will be normalized if necessary).
     * @param height  Height of the plane in map space coordinates.
     */
    Plane(Sector &sector, const de::Vec3f &normal = de::Vec3f(0, 0, 1), double height = 0);

    /**
     * Composes a human-friendly, styled, textual description of the plane.
     */
    de::String description() const;

    /**
     * Returns the owning Sector of the plane.
     */
    Sector       &sector();
    const Sector &sector() const;

    /**
     * Returns the index of the plane within the owning sector.
     */
    int indexInSector() const;

    /**
     * Change the index of the plane within the owning sector.
     *
     * @param newIndex  New index to attribute the plane.
     */
    void setIndexInSector(int newIndex);

    /**
     * Returns @c true iff this is the floor plane of the owning sector.
     */
    bool isSectorFloor() const;

    /**
     * Returns @c true iff this is the ceiling plane of the owning sector.
     */
    bool isSectorCeiling() const;

    /**
     * Returns the Surface component of the plane.
     */
    Surface       &surface();
    const Surface &surface() const;

    /**
     * Returns a pointer to the Surface component of the plane (never @c nullptr).
     */
    Surface *surfacePtr() const;

    /**
     * Change the normal of the plane to @a newNormal (which if necessary will
     * be normalized before being assigned to the plane).
     *
     * @post The plane's tangent vectors and logical plane type will have been
     * updated also.
     */
    void setNormal(const de::Vec3f &newNormal);

    /**
     * Returns the sound emitter for the plane.
     */
    SoundEmitter       &soundEmitter();
    const SoundEmitter &soundEmitter() const;

    /**
     * Update the sound emitter origin according to the point defined by the center
     * of the plane's owning Sector (on the XY plane) and the Z height of the plane.
     */
    void updateSoundEmitterOrigin();

    virtual void setHeight(double newHeight);

    /**
     * Returns the @em current sharp height of the plane relative to @c 0 on the
     * map up axis. The HeightChange audience is notified whenever the height
     * changes.
     */
    double height() const;

    /**
     * Returns the @em target sharp height of the plane in world map units. The target
     * height is the destination height following a successful move. Note that this may
     * be the same as @ref height(), in which case the plane is not currently moving.
     * The HeightChange audience is notified whenever the current @em sharp height changes.
     *
     * @see speed(), height()
     */
    double heightTarget() const;

    double movementBeganAt() const;
    double initialHeightOfMovement() const;

    /**
     * Returns the rate at which the plane height will be updated (units per tic)
     * when moving to the target height in the map coordinate space.
     *
     * @see heightTarget(), height()
     */
    double speed() const;

protected:
    int property(world::DmuArgs &args) const;
    int setProperty(const world::DmuArgs &args);

    double _height = 0;                // Current sharp height.

private:
    DE_PRIVATE(d)
};

} // namespace world
