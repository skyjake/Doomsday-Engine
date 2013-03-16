/** @file plane.h Map Plane.
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_PLANE
#define LIBDENG_MAP_PLANE

#include <QSet>
#include <de/Error>
#include <de/Vector>
#include "MapElement"
#include "resource/r_data.h"
#include "map/p_dmu.h"
#include "map/surface.h"

class Sector;

/*
 * Helper macros for accessing the Surface of a Plane:
 */
/// @addtogroup map
///@{
#define PS_base                 surface().soundEmitter()
#define PS_tangent              surface().tangent()
#define PS_bitangent            surface().bitangent()
#define PS_normal               surface().normal()
#define PS_material             surface().material()
#define PS_offset               surface().materialOrigin()
#define PS_visoffset            surface().visMaterialOrigin()
#define PS_rgba                 surface().colorAndAlpha()
#define PS_flags                surface().flags()
///@}

/**
 * Map sector plane.
 *
 * @ingroup map
 */
class Plane : public de::MapElement
{
public:
    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

    /// In-Sector plane types: @todo move to Sector
    enum Type
    {
        Floor,
        Ceiling,
        Middle
    };

public: /// @todo Make private:
    Surface     _surface;
    Sector     *_sector;       ///< Owner of the plane.
    coord_t     _height;       ///< Current height.
    coord_t     _oldHeight[2];
    coord_t     _targetHeight; ///< Target height.
    coord_t     _speed;        ///< Move speed.
    coord_t     _visHeight;    ///< Visual plane height (smoothed).
    coord_t     _visHeightDelta;
    Type        _type;
    int         _inSectorIndex;

public:
    /**
     * Construct a new plane.
     *
     * @param sector  Sector which will own the plane.
     * @param normal  Normal of the plane (will be normalized if necessary).
     * @param height  Height of the plane in map space coordinates.
     */
    Plane(Sector &sector, de::Vector3f const &normal, coord_t height = 0);

    /**
     * Returns the owning Sector of the plane.
     */
    Sector &sector();

    /// @copydoc sector()
    Sector const &sector() const;

    /**
     * Returns the index of the plane within the owning Sector.
     */
    int inSectorIndex() const;

    /**
     * Returns the Surface of the plane.
     */
    Surface &surface();

    /// @copydoc surface()
    Surface const &surface() const;

    /**
     * Returns the current height of the plane in the map coordinate space.
     */
    coord_t height() const;

    /**
     * Returns the target height of the plane in the map coordinate space.
     * The target height is the destination height following a successful move.
     * Note that this may be the same as height(), in which case the plane is
     * not currently moving.
     */
    coord_t targetHeight() const;

    /**
     * Returns the rate at which the plane height will be updated (units per tic)
     * when moving to the target height in the map coordinate space.
     *
     * @see targetHeight
     */
    coord_t speed() const;

    /**
     * Returns the current interpolated visual height of the plane in the map
     * coordinate space.
     *
     * @see targetHeight()
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

    /**
     * Change the normal of the plane to @a newNormal (which if necessary will
     * be normalized before being assigned to the plane).
     *
     * @post The plane's tangent vectors and logical plane type will have been
     * updated also.
     */
    void setNormal(de::Vector3f const &newNormal);

    /**
     * Returns the logical type of the plane.
     */
    Type type() const;

    /**
     * Get a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int property(setargs_t &args) const;

    /**
     * Update a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int setProperty(setargs_t const &args);

private:
    DENG2_PRIVATE(d)
};

/**
 * A set of Planes.
 * @ingroup map
 */
typedef QSet<Plane *> PlaneSet;

#endif // LIBDENG_MAP_PLANE
