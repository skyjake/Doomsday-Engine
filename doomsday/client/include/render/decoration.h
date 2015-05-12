/** @file decoration.h  World surface decoration.
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

#ifndef CLIENT_RENDER_DECORATION_H
#define CLIENT_RENDER_DECORATION_H

#include <de/Error>
#include <de/Vector>

#include "MapObject"
#include "MaterialAnimator"

class Surface;

/// No decorations are visible beyond this.
#define MAX_DECOR_DISTANCE      (2048)

/**
 * World surface decoration.
 * @ingroup render
 */
class Decoration : public de::MapObject
{
public:
    /// Required surface is missing. @ingroup errors
    DENG2_ERROR(MissingSurfaceError);

public:
    /**
     * Construct a new decoration.
     *
     * @param source  Source of the decoration (a material).
     * @param origin  Origin of the decoration in map space.
     */
    Decoration(MaterialAnimator::Decoration const &source,
               de::Vector3d const &origin = de::Vector3d());
    virtual ~Decoration();

    /**
     * Returns the source of the decoration.
     *
     * @see hasSource(), setSource()
     */
    MaterialAnimator::Decoration const &source() const;

    /**
     * Returns @c true iff a surface is attributed for the decoration.
     *
     * @see surface(), setSurface()
     */
    bool hasSurface() const;

    /**
     * Convenient method which returns the surface owner of the decoration.
     */
    Surface       &surface();
    Surface const &surface() const;

    /**
     * Change the attributed surface of the decoration.
     *
     * @param newSurface  Map surface to attribute. Use @c 0 to clear.
     */
    void setSurface(Surface *newSurface);

private:
    DENG2_PRIVATE(d)
};

#endif  // CLIENT_RENDER_DECORATION_H
