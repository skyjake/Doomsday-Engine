/** @file walledge.h  Wall Edge Geometry.
 *
 * @authors Copyright © 2011-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef RENDER_WALLEDGE
#define RENDER_WALLEDGE

#include "world/line.h"
#include "wallspec.h"
#include "trianglestripbuilder.h"
#include "misc/ihplane.h"

#include <doomsday/mesh/hedge.h>
#include <de/error.h>
#include <de/vector.h>

class Surface;

/// Maximum number of intercepts in a WallEdge.
#define WALLEDGE_MAX_INTERCEPTS          64

/**
 * Helper/utility class intended to simplify the process of generating sections of wall
 * geometry from a map Line segment.
 *
 * @ingroup world
 */
class WallEdge : public WorldEdge
{
    DE_NO_COPY  (WallEdge)
    DE_NO_ASSIGN(WallEdge)

public:
    /// Invalid range geometry was found during prepare() @ingroup errors
    DE_ERROR(InvalidError);

    class Event : public WorldEdge::Event, public de::IHPlane::IIntercept
    {
    public:
        Event();
        Event(WallEdge &owner, double distance = 0);

        Event &operator = (const Event &other);
        bool operator < (const Event &other) const;
        double distance() const;
        de::Vec3d origin() const;

    private:
        WallEdge *_owner;
    };

public:
    /**
     * @param spec   Geometry specification for the wall section. A copy is made.
     *
     * @param hedge  Assumed to have a mapped LineSideSegment with sections.
     */
    WallEdge(const WallSpec &spec, mesh::HEdge &hedge, int edge);

    virtual ~WallEdge();

    inline const Event &operator [] (EventIndex index) const {
        return at(index);
    }

    const de::Vec3d &pOrigin() const;
    const de::Vec3d &pDirection() const;

    de::Vec2f materialOrigin() const;

    de::Vec3f normal() const;

    const WallSpec &spec() const;

    inline LineSide &lineSide() const {
        return lineSideSegment().lineSide().as<LineSide>();
    }

    coord_t lineSideOffset() const;

    LineSideSegment &lineSideSegment() const;

    /// Implement IEdge.
    bool isValid() const;

    /// Implement IEdge.
    const Event &first() const;

    /// Implement IEdge.
    const Event &last() const;

    int divisionCount() const;

    EventIndex firstDivision() const;

    EventIndex lastDivision() const;

    inline const Event &bottom() const { return first(); }
    inline const Event &top   () const { return last();  }

    //const Events &events() const;

    const Event &at(EventIndex index) const;

private:
    struct Impl;
    Impl *d;

    static de::List<WallEdge::Impl *> recycledImpls;
    static Impl *getRecycledImpl();
    static void recycleImpl(Impl *d);
};

#endif  // RENDER_WALLEDGE
