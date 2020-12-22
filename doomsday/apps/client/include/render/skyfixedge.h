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

#ifndef DE_RENDER_SKY_FIX_EDGE
#define DE_RENDER_SKY_FIX_EDGE

#include <doomsday/mesh/hedge.h>
#include <de/vector.h>

#include "trianglestripbuilder.h" /// @todo remove me

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

    class Event : public WorldEdge::Event
    {
    public:
        Event(SkyFixEdge &owner, double distance = 0);

        bool operator < (const Event &other) const;

        double distance() const;

        de::Vec3d origin() const;

    private:
        DE_PRIVATE(d)
    };

public:
    /**
     * @param hedge    HEdge from which to determine sky fix coordinates.
     * @param fixType  Fix type.
     */
    SkyFixEdge(mesh::HEdge &hedge, FixType fixType, int edge, float materialOffsetS = 0);

    const de::Vec3d &pOrigin() const;
    const de::Vec3d &pDirection() const;

    de::Vec2f materialOrigin() const;

    /// Implement IEdge.
    bool isValid() const;

    /// Implement IEdge.
    const Event &first() const;

    /// Implement IEdge.
    const Event &last() const;

    inline const Event &bottom() const { return first(); }
    inline const Event &top() const { return last(); }

    const Event &at(EventIndex index) const;

private:
    DE_PRIVATE(d)
};

#endif // DE_RENDER_SKY_FIX_EDGE
