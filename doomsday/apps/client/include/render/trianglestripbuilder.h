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

#ifndef DE_RENDER_TRIANGLE_STRIP_BUILDER
#define DE_RENDER_TRIANGLE_STRIP_BUILDER

#include <de/libcore.h>
#include <de/vector.h>

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
        virtual ~IEvent() = default;

        virtual bool operator < (const IEvent &other) const {
            return distance() < other.distance();
        }

        virtual double distance() const = 0;
    };

public:
    virtual ~IEdge() = default;

    virtual bool isValid() const = 0;

    virtual const IEvent &first() const = 0;

    virtual const IEvent &last() const = 0;
};

/**
 * @ingroup render
 */
class AbstractEdge : public IEdge
{
public:
    typedef int EventIndex;

    /// Special identifier used to mark an invalid event index.
    enum { InvalidIndex = -1 };

    class Event : public IEvent
    {
    public:
        virtual ~Event() {}

        virtual de::Vec3d origin() const = 0;
    };

public:
    virtual ~AbstractEdge() {}

    virtual const Event &first() const = 0;
    virtual const Event &last() const  = 0;
    virtual de::Vec2f    materialOrigin() const { return de::Vec2f(); }
    virtual de::Vec3f    normal() const { return de::Vec3f(); }
};

/**
 * @ingroup world
 */
class WorldEdge : public AbstractEdge
{
public:
    class Event : public AbstractEdge::Event
    {
    public:
        virtual ~Event() {}
        virtual de::Vec3d origin() const = 0;
        inline double z() const { return origin().z; }
    };

public:
    WorldEdge(de::Vec2d origin_) : AbstractEdge(), _origin(origin_)
    {}

    virtual ~WorldEdge() {}

    /**
     * Returns the X|Y origin of the edge in the map coordinate space.
     */
    const de::Vec2d &origin() const { return _origin; }

    virtual const Event &first() const              = 0;
    virtual const Event &last() const               = 0;
    virtual const Event &at(EventIndex index) const = 0;

    virtual int        divisionCount() const { return 0; }
    virtual EventIndex firstDivision() const { return InvalidIndex; }
    virtual EventIndex lastDivision() const { return InvalidIndex; }

private:
    de::Vec2d _origin;
};

typedef de::List<de::Vec3f> PositionBuffer;
typedef de::List<de::Vec2f> TexCoordBuffer;

/**
 * Abstract triangle strip geometry builder.
 *
 * Encapsulates the logic of constructing triangle strip geometries.
 *
 * @ingroup render
 *
 * @todo Separate the backing store with an external allocator mechanism.
 *       (Geometry should not be owned by the builder.)
 * @todo Support custom vertex types.
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
     *   |  | or |  | if @a direction @c =CounterClockwise
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
    void begin(de::ClockDirection direction, int reserveElements = 0);

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
    DE_PRIVATE(d)
};

#endif // DE_RENDER_TRIANGLE_STRIP_BUILDER
