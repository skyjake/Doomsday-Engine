/** @file plane.h World map plane.
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

#ifndef DENG_WORLD_PLANE_H
#define DENG_WORLD_PLANE_H

#include <de/types.h>

#include <de/Observers>
#include <de/Vector>

#include "MapElement"

class Sector;
class Surface;

/**
 * World map sector plane.
 *
 * @ingroup world
 */
class Plane : public de::MapElement
{
    DENG2_NO_COPY  (Plane)
    DENG2_NO_ASSIGN(Plane)

public:
    /*
     * Observers to be notified when a Plane is about to be deleted.
     */
    DENG2_DEFINE_AUDIENCE(Deletion, void planeBeingDeleted(Plane const &plane))

    /*
     * Observers to be notified whenever a @em sharp height change occurs.
     */
    DENG2_DEFINE_AUDIENCE(HeightChange, void planeHeightChanged(Plane &plane, coord_t oldHeight))

    // Constants:
    static int const MAX_SMOOTH_MOVE = 64; ///< $smoothplane: Maximum speed for a smoothed plane.

public:
    /**
     * Construct a new plane.
     *
     * @param sector  Sector which will own the plane.
     * @param normal  Normal of the plane (will be normalized if necessary).
     * @param height  Height of the plane in map space coordinates.
     */
    Plane(Sector &sector,
          de::Vector3f const &normal = de::Vector3f(0, 0, 1),
          coord_t height             = 0);

    /**
     * Returns the owning Sector of the plane.
     */
    Sector &sector();

    /// @copydoc sector()
    Sector const &sector() const;

    /**
     * Returns the index of the plane within the owning sector.
     */
    int inSectorIndex() const;

    /**
     * Change the index of the plane within the owning sector.
     *
     * @param newIndex  New index to attribute the plane.
     */
    void setInSectorIndex(int newIndex);

    /**
     * Returns @c true iff this is the floor plane of the owning sector.
     */
    bool isSectorFloor() const;

    /**
     * Returns @c true iff this is the ceiling plane of the owning sector.
     */
    bool isSectorCeiling() const;

    /**
     * Returns the Surface of the plane.
     */
    Surface &surface();

    /// @copydoc surface()
    Surface const &surface() const;

    /**
     * Returns the sound emitter for the plane.
     */
    ddmobj_base_t &soundEmitter();

    /// @copydoc soundEmitter()
    ddmobj_base_t const &soundEmitter() const;

    /**
     * Update the sound emitter origin according to the point defined by the center
     * of the plane's owning Sector (on the XY plane) and the Z height of the plane.
     */
    void updateSoundEmitterOrigin();

    /**
     * Returns the current @em sharp height of the plane relative to @c 0 on the
     * map up axis. The HeightChange audience is notified whenever the height
     * changes.
     */
    coord_t height() const;

    /**
     * Returns the target height of the plane in the map coordinate space.
     * The target height is the destination height following a successful move.
     * Note that this may be the same as @ref height(), in which case the plane
     * is not currently moving. The HeightChange audience is notified whenever
     * the current @em sharp height changes.
     *
     * @see speed(), height()
     */
    coord_t targetHeight() const;

    /**
     * Returns the rate at which the plane height will be updated (units per tic)
     * when moving to the target height in the map coordinate space.
     *
     * @see targetHeight(), height()
     */
    coord_t speed() const;

#ifdef __CLIENT__

    /**
     * Returns the current interpolated visual height of the plane in the map
     * coordinate space.
     *
     * @see targetHeight(), height()
     */
    coord_t visHeight() const;

    /**
     * Returns the delta between current height and the interpolated visual
     * height of the plane in the map coordinate space.
     *
     * @see targetHeight()
     */
    coord_t visHeightDelta() const;

    /**
     * Set the visible offsets.
     *
     * @see visHeight(), targetHeight()
     */
    void lerpVisHeight();

    /**
     * Reset the plane's height tracking.
     *
     * @see visHeight(), targetHeight()
     */
    void resetVisHeight();

    /**
     * Roll the plane's height tracking buffer.
     *
     * @see targetHeight()
     */
    void updateHeightTracking();

#endif // __CLIENT__

    /**
     * Change the normal of the plane to @a newNormal (which if necessary will
     * be normalized before being assigned to the plane).
     *
     * @post The plane's tangent vectors and logical plane type will have been
     * updated also.
     */
    void setNormal(de::Vector3f const &newNormal);

protected:
    int property(DmuArgs &args) const;
    int setProperty(DmuArgs const &args);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_PLANE_H
