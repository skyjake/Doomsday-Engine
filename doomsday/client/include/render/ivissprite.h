/** @file ivissprite.h  Interface for a distance-sorted "vissprite".
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_RENDER_IVISSPRITE_H
#define DENG_CLIENT_RENDER_IVISSPRITE_H

#include <de/Vector>

/**
 * Interface for an object that can treated as a drawable "vissprite", such as a
 * mobj or a masked wall.
 *
 * @ingroup render
 */
class IVissprite
{
public:
    virtual ~IVissprite() {}

    /**
     * Returns the distance from the viewer in map space units.
     */
    virtual de::ddouble distance() const = 0;

    /**
     * Returns the origin of the vissprite in map space.
     */
    virtual de::Vector3d const &origin() const = 0;

    virtual void draw() = 0;
};

#endif // DENG_CLIENT_RENDER_IVISSPRITE_H
