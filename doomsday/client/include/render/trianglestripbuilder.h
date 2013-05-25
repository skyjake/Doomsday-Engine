/** @file render/trianglestripbuilder.h Triangle Strip Geometry Builder.
 *
 * @authors Copyright © 2011-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RENDER_TRIANGLE_STRIP_BUILDER
#define DENG_RENDER_TRIANGLE_STRIP_BUILDER

#include <QVarLengthArray> /// @todo Remove me

#include <de/libdeng2.h>
#include <de/Vector>

#include "render/rendpoly.h" /// @todo Remove me

namespace de {

/**
 * Abstract interface for a component that can be interpreted as an "edge"
 * geometry.
 *
 * @ingroup math
 */
class IEdge
{
public:
    class IEvent
    {
    public:
        virtual ~IEvent() {}

        virtual bool operator < (IEvent const &other) const {
            return distance() < other.distance();
        }

        virtual coord_t distance() const = 0;

        virtual Vector3d origin() const = 0;
    };

public:
    virtual ~IEdge() {}

    virtual bool isValid() const = 0;

    virtual IEvent const &first() const = 0;

    virtual IEvent const &last() const = 0;
};

} // namespace de

namespace de {

/**
 * @ingroup world
 */
struct EdgeAttribs
{
    Vector2d origin;
    Vector2f materialOrigin;
    Vector3f normal;

    EdgeAttribs(Vector2d const &origin        = Vector2d(),
                Vector2f const materialOrigin = Vector2f(),
                Vector3f const &normal        = Vector3f())
        : origin(origin),
          materialOrigin(materialOrigin),
          normal(normal)
    {}
};

} // namespace de

namespace de {

/**
 * @ingroup render
 */
class AbstractEdge : public EdgeAttribs, public IEdge
{
public:
    typedef IEvent Event;

    typedef int EventIndex;

    /// Special identifier used to mark an invalid event index.
    enum { InvalidIndex = -1 };

public:
    AbstractEdge(EdgeAttribs const &attribs = EdgeAttribs())
        : EdgeAttribs(attribs)
    {}

    virtual ~AbstractEdge() {}
};

} // namespace de

namespace de {

/**
 * @ingroup world
 */
typedef AbstractEdge WorldEdge;

} // namespace de

namespace de {

typedef QVarLengthArray<rvertex_t, 24> PositionBuffer;
typedef QVarLengthArray<rtexcoord_t, 24> TexCoordBuffer;

/**
 * Abstract triangle strip geometry builder.
 *
 * Encapsulates the logic of constructing triangle strip geometries.
 *
 * @ingroup render
 *
 * @todo Separate the backing store with an external allocator mechanism.
 *       (Geometry should not be owned by the builder.)
 * @todo Support custom vertex types/do not depend on rvertex_t (etc...).
 *       (Once implemented move this component to libgui.)
 * @todo Support building strips by adding single vertices.
 */
class TriangleStripBuilder
{
public:
    /**
     * Construct a new triangle strip builder.
     *
     * @param buildTexCoords   @c true= construct texture coordinates also.
     */
    TriangleStripBuilder(bool buildTexCoords = false);

    /**
     * Begin construction of a new triangle strip geometry. Any existing unclaimed
     * geometry is discarded.
     *
     * Vertex layout:
     *   1--3    2--0
     *   |  | or |  | if @a direction @c =Anticlockwise
     *   0--2    3--1
     *
     * @param direction        Initial vertex winding direction.
     *
     * @param reserveElements  Initial number of vertex elements to reserve. If the
     *  user knows in advance roughly how many elements are required for the geometry
     *  this number may be reserved from the outset, thereby improving performance
     *  by minimizing dynamic memory allocations. If the estimate is off the only
     *  side effect is reduced performance.
     */
    void begin(ClockDirection direction, int reserveElements = 0);

    /**
     * Submit an edge geometry to extend the current triangle strip geometry.
     *
     * @param edge  Edge geometry to extend with.
     */
    void extend(AbstractEdge &edge);

    /**
     * Submit an edge geometry to extend the current triangle strip geometry.
     *
     * @param edge  Edge geometry to extend with.
     *
     * @return  Reference to this trangle strip builder.
     *
     * @see extend()
     */
    inline TriangleStripBuilder &operator << (AbstractEdge &edge) {
        extend(edge);
        return *this;
    }

    /**
     * Returns the total number of vertex elements in the current strip geometry.
     * If no strip is currently being built @c 0 is returned.
     */
    int numElements() const;

    /**
     * Take ownership of the last built strip of geometry.
     *
     * @param positions  The address of the buffer containing the vertex position
     *                   values is written here.
     * @param texcoords  The address of the buffer containing the texture coord
     *                   values is written here. Can be @c 0.
     *
     * @return  Total number of vertex elements in the geometry (for convenience).
     */
    int take(PositionBuffer **positions, TexCoordBuffer **texcoords = 0);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_RENDER_TRIANGLE_STRIP_BUILDER
