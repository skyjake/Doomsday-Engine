/** @file render/walledge.h Wall Edge Geometry.
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

#ifndef DENG_RENDER_WALLEDGE
#define DENG_RENDER_WALLEDGE

#include <QList>

#include <de/Error>
#include <de/Vector>

#include "Line"
#include "WallSpec"

#include "TriangleStripBuilder"
#include "IHPlane"

class HEdge;
class Surface;

/// Maximum number of intercepts in a WallEdge.
#define WALLEDGE_MAX_INTERCEPTS          64

namespace de {

/**
 * Helper/utility class intended to simplify the process of generating
 * sections of wall geometry from a map Line segment.
 *
 * @ingroup map
 */
class WallEdge : public WorldEdge
{
    DENG2_NO_COPY  (WallEdge)
    DENG2_NO_ASSIGN(WallEdge)

public:
    /// Invalid range geometry was found during prepare() @ingroup errors
    DENG2_ERROR(InvalidError);

    class Event : public WorldEdge::Event, public IHPlane::IIntercept
    {
    public:
        Event(WallEdge &owner, double distance = 0);

        bool operator < (Event const &other) const;

        double distance() const;

        Vector3d origin() const;

        friend class WallEdge;

    protected:
        void setDistance(double newDistance);

    private:
        DENG2_PRIVATE(d)
    };

    typedef QList<Event *> Events;

public:
    /**
     * @param spec  Geometry specification for the wall section. A copy is made.
     */
    WallEdge(WallSpec const &spec, HEdge &hedge, int edge);

    inline Event const &operator [] (EventIndex index) const {
        return at(index);
    }

    WallSpec const &spec() const;

    Line::Side &mapSide() const;

    coord_t mapSideOffset() const;

    /// Implement IEdge.
    bool isValid() const;

    /// Implement IEdge.
    Event const &first() const;

    /// Implement IEdge.
    Event const &last() const;

    int divisionCount() const;

    EventIndex firstDivision() const;

    EventIndex lastDivision() const;

    inline Event const &bottom() const { return first(); }
    inline Event const &top() const { return last(); }

    Events const &events() const;

    Event const &at(EventIndex index) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_RENDER_WALLEDGE
