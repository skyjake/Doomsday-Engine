/** @file render/skyfixedge.h Sky Fix Edge Geometry.
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

#ifndef DENG_RENDER_SKY_FIX_EDGE
#define DENG_RENDER_SKY_FIX_EDGE

#include <de/Vector>

#include "TriangleStripBuilder" /// @todo remove me

class HEdge;

namespace de {

/**
 * @ingroup render
 */
class SkyFixEdge : public WorldEdge
{
public:
    enum FixType
    {
        Lower,
        Upper
    };

    class Event : public IEdge::IEvent
    {
    public:
        Event(SkyFixEdge &owner, coord_t distance = 0);

        bool operator < (Event const &other) const;

        coord_t distance() const;

        Vector3d origin() const;

        friend class SkyFixEdge;

    protected:
        void setDistance(double newDistance);

    private:
        DENG2_PRIVATE(d)
    };

public:
    /**
     * @param hedge    HEdge from which to determine sky fix coordinates.
     * @param fixType  Fix type.
     */
    SkyFixEdge(HEdge &hedge, FixType fixType, int edge, float materialOffsetS = 0);

    /// Implement IEdge.
    bool isValid() const;

    /// Implement IEdge.
    Event const &first() const;

    /// Implement IEdge.
    Event const &last() const;

    inline Event const &bottom() const { return first(); }
    inline Event const &top() const { return last(); }

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_RENDER_SKY_FIX_EDGE
