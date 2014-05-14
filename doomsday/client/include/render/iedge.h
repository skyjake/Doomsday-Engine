/** @file iedge.h  Interface for an edge geometry.
 *
 * @authors Copyright © 2011-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_RENDER_IEDGE
#define DENG_CLIENT_RENDER_IEDGE

#include <de/libcore.h>
#include <de/Vector>

namespace de {

/**
 * Abstract interface for a component that can be interpreted as an "edge" geometry.
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

        virtual double distance() const = 0;
    };

public:
    virtual ~IEdge() {}

    virtual bool isValid() const = 0;

    virtual IEvent const &first() const = 0;

    virtual IEvent const &last() const = 0;
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

        virtual Vector3d origin() const = 0;

        inline double x() const { return origin().x; }
        inline double y() const { return origin().y; }
        inline double z() const { return origin().z; }
    };

public:
    virtual ~AbstractEdge() {}

    virtual Event const &first() const = 0;

    virtual Event const &last() const = 0;

    virtual Vector2f materialOrigin() const { return Vector2f(); }

    virtual Vector3f normal() const { return Vector3f(); }
};

} // namespace de

#endif // DENG_CLIENT_RENDER_IEDGE
