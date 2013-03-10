/** @file vertex.h Map Geometry Vertex
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

#ifndef LIBDENG_MAP_VERTEX
#define LIBDENG_MAP_VERTEX

#include <de/Error>
#include <de/vector1.h>
#include <de/binangle.h>
#include "resource/r_data.h"
#include "map/p_dmu.h"
#include "MapElement"

class LineDef;

/**
 * @todo Replace ring navigation with a circular iterator at Vertex level -ds.
 * @ingroup map
 */
class LineOwner
{
public:
    /// Ring navigation direction identifiers:
    enum Direction
    {
        /// Previous (anticlockwise).
        Previous,

        /// Next (clockwise).
        Next
    };

public: /// @todo Make private:
    LineDef *_lineDef;

    /// {Previous, Next} (i.e. {anticlk, clk}).
    LineOwner *_link[2];

    /// Angle between this and the next line owner, clockwise.
    binangle_t _angle;

    struct ShadowVert {
        vec2d_t inner;
        vec2d_t extended;
    } _shadowOffsets;

public:
    /*LineOwner() : _lineDef(0), _angle(0)
    {
        _link[Previous] = 0;
        _link[Next] = 0;

        V2d_Set(_shadowOffsets.inner, 0, 0);
        V2d_Set(_shadowOffsets.extended, 0, 0);
    }*/

    /**
     * Returns @c true iff the previous line owner in the ring (anticlockwise)
     * is not the same as this LineOwner.
     *
     * @see prev()
     */
    inline bool hasPrev() const { return &prev() != this; }

    /**
     * Returns @c true iff the next line owner in the ring (clockwise) is not
     * the same as this LineOwner.
     *
     * @see next()
     */
    inline bool hasNext() const { return &next() != this; }

    /**
     * Navigate to the adjacent line owner in the ring (if any). Note this may
     * be the same LineOwner.
     */
    LineOwner &navigate(Direction dir = Previous) { return *_link[dir]; }

    /// @copydoc navigate()
    LineOwner const &navigate(Direction dir = Previous) const { return *_link[dir]; }

    /**
     * Returns the previous line owner in the ring (anticlockwise). Note that
     * this may be the same LineOwner.
     *
     * @see hasPrev()
     */
    inline LineOwner &prev() { return navigate(Previous); }

    /// @copydoc prev()
    inline LineOwner const &prev() const { return navigate(Previous); }

    /**
     * Returns the next line owner in the ring (clockwise). Note that this may
     * be the same LineOwner.
     *
     * @see hasNext()
     */
    inline LineOwner &next() { return navigate(Next); }

    /// @copydoc next()
    inline LineOwner const &next() const { return navigate(Next); }

    /**
     * Returns the LineDef "owner".
     */
    LineDef &lineDef() const { return *_lineDef; }

    /**
     * Returns the angle between the line owner and the next in the ring (clockwise).
     */
    binangle_t angle() const { return _angle; }

    /**
     * Returns the inner shadow offset of the line owner.
     */
    vec2d_t const &innerShadowOffset() const { return _shadowOffsets.inner; }

    /**
     * Returns the extended shadow offset of the line owner.
     */
    vec2d_t const &extendedShadowOffset() const { return _shadowOffsets.extended; }
};

/**
 * Map geometry vertex.
 *
 * An @em owner in this context is any linedef whose start or end points are
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

    /// @todo This temporary load-time data does not belong here. -ds
    struct {
        /// Vertex index. Always valid after loading and pruning of unused
        /// vertices has occurred.
        int index;

        /// Reference count. When building normal node info, unused vertices
        /// will be pruned.
        int refCount;

        /// Usually NULL, unless this vertex occupies the same location as a
        /// previous vertex. Only used during the pruning phase.
        Vertex *equiv;
    } _buildData;

public:
    Vertex(coord_t x = 0, coord_t y = 0);

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
     * Returns the total number of LineDef owners for the vertex.
     *
     * @see countLineOwners()
     */
    uint lineOwnerCount() const;

    /**
     * Utility function for determining the number of one and two-sided LineDef
     * owners for the vertex.
     *
     * @note That if only the combined total is desired, it is more efficent to
     * call lineOwnerCount() instead.
     *
     * @pre Line owner rings must have already been calculated.
     * @pre @a oneSided and/or @a twoSided must have already been initialized.
     *
     * @param oneSided  The number of one-sided LineDef owners will be added to
     *                  the pointed value if not @a NULL.
     * @param twoSided  The number of two-sided LineDef owners will be added to
     *                  the pointed value if not @c NULL.
     *
     * @todo Optimize: Cache this result.
     */
    void countLineOwners(uint *oneSided, uint *twoSided) const;

    /**
     * Returns the first LineDef owner for the vertex; otherwise @c 0 if unowned.
     */
    LineOwner *firstLineOwner() const;

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

#endif // LIBDENG_MAP_VERTEX
