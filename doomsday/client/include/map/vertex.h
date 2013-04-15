/** @file vertex.h World Map Vertex.
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

#ifndef DENG_WORLD_MAP_VERTEX
#define DENG_WORLD_MAP_VERTEX

#include <de/vector1.h> /// @todo remove me

#include <de/Error>
#include <de/Vector>

#include "MapElement"
#include "map/p_dmu.h"

class Line;
class LineOwner;

/**
 * World map geometry vertex.
 *
 * An @em owner in this context is any line whose start or end points are
 * defined as the vertex.
 *
 * @ingroup map
 */
class Vertex : public de::MapElement
{
public:
    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

public: /// @todo Make private:
    coord_t _origin[2];

    /// Head of the LineOwner rings (an array of [numLineOwners] size). The
    /// owner ring is a doubly, circularly linked list. The head is the owner
    /// with the lowest angle and the next-most being that with greater angle.
    LineOwner *_lineOwners;
    uint _numLineOwners; ///< Total number of line owners.

public:
    Vertex(de::Vector2d const &origin = de::Vector2d(0, 0));

    /**
     * Returns the origin (i.e., location) of the vertex in the map coordinate space.
     */
    vec2d_t const &origin() const;

    /**
     * Returns the X axis origin (i.e., location) of the vertex in the map coordinate space.
     */
    inline coord_t x() const { return origin()[VX]; }

    /**
     * Returns the Y axis origin (i.e., location) of the vertex in the map coordinate space.
     */
    inline coord_t y() const { return origin()[VY]; }

    /**
     * Returns the total number of Line owners for the vertex.
     *
     * @see countLineOwners()
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
     * @param oneSided  The number of one-sided Line owners will be added to
     *                  the pointed value if not @a NULL.
     * @param twoSided  The number of two-sided Line owners will be added to
     *                  the pointed value if not @c NULL.
     *
     * @todo Optimize: Cache this result.
     */
    void countLineOwners(uint *oneSided, uint *twoSided) const;

    /**
     * Returns the first Line owner for the vertex; otherwise @c 0 if unowned.
     */
    LineOwner *firstLineOwner() const;

    /**
     * Returns the original index of the vertex.
     */
    uint origIndex() const;

    /**
     * Change the original index of the vertex.
     *
     * @param newIndex  New original index.
     */
    void setOrigIndex(uint newIndex);

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

#endif // DENG_WORLD_MAP_VERTEX
