/** @file vertex.h  World map vertex.
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

#ifndef DENG_WORLD_VERTEX_H
#define DENG_WORLD_VERTEX_H

#include <de/Observers>
#include <de/Vector>

#include "MapElement"
#include "Mesh"

class Line;
class LineOwner;

/**
 * World map geometry vertex.
 *
 * An @em owner in this context is any line whose start or end points are
 * defined as the vertex.
 *
 * @ingroup world
 */
class Vertex : public de::MapElement, public de::MeshElement
{
    DENG2_NO_COPY  (Vertex)
    DENG2_NO_ASSIGN(Vertex)

public:
    /// Notified whenever the origin changes.
    DENG2_DEFINE_AUDIENCE(OriginChange, void vertexOriginChanged(Vertex &vertex))

public: /// @todo Move to the map loader:
    /// Head of the LineOwner rings (an array of [numLineOwners] size). The
    /// owner ring is a doubly, circularly linked list. The head is the owner
    /// with the lowest angle and the next-most being that with greater angle.
    LineOwner *_lineOwners;
    uint _numLineOwners; ///< Total number of line owners.

    // Total numbers of line owners.
    uint _onesOwnerCount;
    uint _twosOwnerCount;

public:
    Vertex(de::Mesh &mesh, de::Vector2d const &origin = de::Vector2d());

    /**
     * Returns the origin (i.e., position) of the vertex in the map coordinate space.
     */
    de::Vector2d const &origin() const;

    /**
     * Returns the X axis origin (i.e., position) of the vertex in the map coordinate space.
     */
    inline coord_t x() const { return origin().x; }

    /**
     * Returns the Y axis origin (i.e., position) of the vertex in the map coordinate space.
     */
    inline coord_t y() const { return origin().y; }

    /**
     * Change the origin (i.e., position) of the vertex in the map coordinate
     * space. The OriginChange audience is notified whenever the origin changes.
     *
     * @param newOrigin  New origin in map coordinate space units.
     */
    void setOrigin(de::Vector2d const &newOrigin);

    /**
     * @copydoc setOrigin()
     *
     * @param x  New X origin in map coordinate space units.
     * @param y  New Y origin in map coordinate space units.
     */
    inline void setOrigin(float x, float y) { return setOrigin(de::Vector2d(x, y)); }

public: // Deprecated ----------------------------------------------------------

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
    int property(DmuArgs &args) const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_VERTEX_H
