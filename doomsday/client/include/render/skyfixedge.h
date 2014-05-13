/** @file skyfixedge.h  Sky Fix Edge Geometry.
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

#ifndef DENG_CLIENT_RENDER_SKYFIXEDGE
#define DENG_CLIENT_RENDER_SKYFIXEDGE

//#include <QList>
#include <de/Error>
#include <de/Vector>
#include "Line"
#include "TriangleStripBuilder" /// @todo remove me
//#include "IHPlane"

namespace de {

class HEdge;

/**
 * @ingroup render
 */
class SkyFixEdge
{
    DENG2_NO_COPY  (SkyFixEdge)
    DENG2_NO_ASSIGN(SkyFixEdge)

public:
    /// An unknown section was referenced. @ingroup errors
    DENG2_ERROR(UnknownSectionError);

    class SkySection; // forward

    /// Logical section identifiers:
    enum SectionId {
        SkyBottom,
        SkyTop
    };

    /**
     * Sky edge event.
     */
    class Event : public AbstractEdge::Event
    {
    public:
        Event(double distance = 0);

        // Implement AbstractEdge::Event:
        double distance() const;
        Vector3d origin() const;
        bool operator < (Event const &other) const;

        /**
         * Returns the SkySection to which the event is attributed.
         */
        SkySection &section() const;

        // SkySection needs access to attribute events.
        friend class SkySection;

    private:
        double _distance;
        SkySection *_section;
    };

    /**
     * Sky edge section.
     */
    class SkySection : public AbstractEdge
    {
    public:
        /**
         * Returns the owning SkyFixEdge for the section.
         */
        SkyFixEdge &edge() const;

        Vector3d const &pOrigin() const;
        Vector3d const &pDirection() const;

        /// Implement IEdge.
        bool isValid() const;

        /// Implement AbstractEdge:
        SkyFixEdge::Event const &first() const;
        SkyFixEdge::Event const &last() const;
        Vector2f materialOrigin() const;

        /// Convenient aliases:
        inline SkyFixEdge::Event const &bottom() const { return first(); }
        inline SkyFixEdge::Event const &top() const    { return last(); }

        /**
         * Lookup an edge event by @a index.
         */
        SkyFixEdge::Event const &at(EventIndex index) const;

        // SkyFixEdge needs access to the private constructor.
        friend class SkyFixEdge;

    private:
        SkySection(SkyFixEdge &owner, SectionId id, float materialOffsetS = 0);

        DENG2_PRIVATE(d)
    };

    /**
     * @param hedge    HEdge from which to determine sky fix coordinates.
     * @param fixType  Fix type.
     */
    SkyFixEdge(HEdge &hedge, int edge, float materialOffsetS = 0);

    /**
     * Returns the X|Y origin of the edge in map space.
     */
    Vector2d const &origin() const;

    /**
     * Returns the halfedge from which the edge was built.
     */
    HEdge &hedge() const;

    /**
     * Returns the logical front side (0: left, 1: right) of the halfedge for the edge.
     */
    int side() const;

    /**
     * Returns the map Line::Side for the edge.
     */
    LineSide &lineSide() const;

    /**
     * Returns the offset along the map Line::Side for the edge.
     */
    coord_t lineSideOffset() const;

    /**
     * Returns the SkySection associated with @a id.
     */
    SkySection &section(SectionId id);

    /// Convenient accessor methods:
    inline SkySection &skyTop()    { return section(SkyTop); }
    inline SkySection &skyBottom() { return section(SkyBottom); }

private:
    DENG2_PRIVATE(d)
};

typedef SkyFixEdge::SkySection SkyFixEdgeSection;

} // namespace de

#endif // DENG_CLIENT_RENDER_SKYFIXEDGE
