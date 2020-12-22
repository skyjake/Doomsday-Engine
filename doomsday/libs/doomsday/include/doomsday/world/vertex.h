/** @file vertex.h  World map vertex.
 * @ingroup world
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#include "mapelement.h"
#include "../mesh/mesh.h"

#include <de/observers.h>
#include <de/vector.h>

namespace world {

class Line;
class LineOwner;
class Map;

/**
 * Map geometry vertex.
 *
 * An @em owner in this context is any line whose start or end points are
 * defined as the vertex.
 */
class LIBDOOMSDAY_PUBLIC Vertex : public MapElement, public mesh::MeshElement
{
    DE_NO_COPY  (Vertex)
    DE_NO_ASSIGN(Vertex)

public:
    /// Notified whenever the origin changes.
    DE_DEFINE_AUDIENCE(OriginChange, void vertexOriginChanged(Vertex &vertex))

public:
    Vertex(mesh::Mesh &mesh, const de::Vec2d &origin = {});

    /**
     * Returns the origin (i.e., position) of the vertex in the map coordinate space.
     */
    const de::Vec2d &origin() const;

    /**
     * Returns the X axis origin (i.e., position) of the vertex in the map coordinate space.
     */
    inline double x() const { return origin().x; }

    /**
     * Returns the Y axis origin (i.e., position) of the vertex in the map coordinate space.
     */
    inline double y() const { return origin().y; }

    /**
     * Change the origin (i.e., position) of the vertex in the map coordinate
     * space. The OriginChange audience is notified whenever the origin changes.
     *
     * @param newOrigin  New origin in map coordinate space units.
     */
    void setOrigin(const de::Vec2d &newOrigin);

    /**
     * @copydoc setOrigin()
     *
     * @param x  New X origin in map coordinate space units.
     * @param y  New Y origin in map coordinate space units.
     */
    inline void setOrigin(float x, float y) { return setOrigin(de::Vec2d(x, y)); }

public:  //- Deprecated -----------------------------------------------------------------

    /**
     * Returns the total number of Line owners for the vertex.
     *
     * @see countLineOwners()
     *
     * @deprecated Will be replaced with half-edge ring iterator/rover. -ds
     */
    uint lineOwnerCount() const;

    /**
     * Utility function for determining the number of one and two-sided Line
     * owners for the vertex.
     *
     * @note That if only the combined total is desired, it is more efficent to
     * call lineOwnerCount() instead.
     *
     * @pre Line owner rings must have already been calculated.
     * @pre @a oneSided and/or @a twoSided must have already been initialized.
     *
     * @todo Optimize: Cache this result.
     *
     * @deprecated Will be replaced with half-edge ring iterator/rover. -ds
     */
    void countLineOwners();

    /**
     * Returns the first Line owner for the vertex; otherwise @c 0 if unowned.
     *
     * @deprecated Will be replaced with half-edge ring iterator/rover. -ds
     */
    LineOwner *firstLineOwner() const;

protected:
    int property(world::DmuArgs &args) const;

private:
    de::Vec2d _origin;  ///< Map-space coordinates.

    friend class Map; // Map accesses the members below.
    
    // Head of the LineOwner rings (an array of [numLineOwners] size). The
    // owner ring is a doubly, circularly linked list. The head is the owner
    // with the lowest angle and the next-most being that with greater angle.
    LineOwner *_lineOwners = nullptr;
    uint _numLineOwners = 0;  // Total number of line owners.
    uint _onesOwnerCount = 0;
    uint _twosOwnerCount = 0;

};

} // namespace world

