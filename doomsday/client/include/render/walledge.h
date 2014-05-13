/** @file walledge.h  Wall Edge Geometry.
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

#ifndef DENG_CLIENT_RENDER_WALLEDGE
#define DENG_CLIENT_RENDER_WALLEDGE

#include <QList>
#include <de/Error>
#include <de/Vector>
#include "Line"
#include "TriangleStripBuilder" /// @todo remove me
#include "IHPlane"

namespace de {

class HEdge;
struct WallSpec;

/**
 * Helper/utility class intended to simplify the process of generating
 * sections of wall geometry from a map Line segment.
 *
 * @ingroup world
 */
class WallEdge
{
    DENG2_NO_COPY  (WallEdge)
    DENG2_NO_ASSIGN(WallEdge)

public:
    /// An unknown section was referenced. @ingroup errors
    DENG2_ERROR(UnknownSectionError);

    class WallSection; // forward

    /// Logical section identifiers:
    enum SectionId {
        WallMiddle,
        WallBottom,
        WallTop
    };

    /**
     * Utility for converting a Line::Side @a section to SectionId.
     */
    static SectionId sectionIdFromLineSideSection(int section);

    /**
     * Wall edge event.
     */
    class Event : public AbstractEdge::Event, public IHPlane::IIntercept
    {
    public:
        Event(double distance = 0);

        // Implement AbstractEdge::Event:
        double distance() const;
        Vector3d origin() const;
        bool operator < (Event const &other) const;

        /**
         * Returns the WallSection to which the event is attributed.
         */
        WallSection &section() const;

        // WallSection needs access to attribute events.
        friend class WallSection;

    private:
        WallSection *_section; ///< The attributed section.
    };

    /**
     * Wall edge section.
     */
    class WallSection : public AbstractEdge
    {
    public:
        /// A valid edge is required. @ingroup errors
        DENG2_ERROR(InvalidError);

        typedef QList<WallEdge::Event *> Events;

    public:
        /**
         * Returns the owning WallEdge for the section.
         */
        WallEdge &edge() const;

        /**
         * Returns the WallSpec for the section.
         */
        WallSpec const &spec() const;
        SectionId id() const;

        Vector3d const &pOrigin() const;
        Vector3d const &pDirection() const;

        /// Implement IEdge:
        bool isValid() const;

        /// Implement AbstractEdge:
        WallEdge::Event const &first() const;
        WallEdge::Event const &last() const;
        Vector2f materialOrigin() const;
        Vector3f normal() const;

        /// Convenient aliases:
        inline WallEdge::Event const &bottom() const { return first(); }
        inline WallEdge::Event const &top() const    { return last(); }

        int divisionCount() const;
        EventIndex firstDivision() const;
        EventIndex lastDivision() const;

        /**
         * Lookup an edge event by @a index.
         */
        WallEdge::Event const &at(EventIndex index) const;

        /// @see at()
        inline WallEdge::Event const &operator [] (EventIndex index) const {
            return at(index);
        }

        /**
         * Provides access to all edge events for the section, for efficient traversal.
         */
        Events const &events() const;

        // WallEdge needs access to the private constructor.
        friend class WallEdge;

    private:
        WallSection(WallEdge &owner, SectionId id, WallSpec const &spec);

        DENG2_PRIVATE(d)
    };

    /**
     * Construct a new WallEdge for the specified @a hedge, @a side.
     *
     * @param hedge  Assumed to have a mapped LineSideSegment with sections.
     * @param side   Logical front side of the halfedge (0: left, 1: right).
     */
    WallEdge(HEdge &hedge, int side);

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
     * Returns the WallSection associated with @a id.
     *
     * @see sectionIdFromLineSideSection()
     */
    WallSection &section(SectionId id);

    /// Convenient accessor methods:
    inline WallSection &wallTop()    { return section(WallTop); }
    inline WallSection &wallMiddle() { return section(WallMiddle); }
    inline WallSection &wallBottom() { return section(WallBottom); }

private:
    DENG2_PRIVATE(d)
};

typedef WallEdge::WallSection WallEdgeSection;

} // namespace de

#endif // DENG_CLIENT_RENDER_WALLEDGE
