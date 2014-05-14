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

#include <QFlags>
#include <QList>
#include <de/Error>
#include <de/Vector>
#include "Line"
#include "IEdge"
#include "IHPlane"

namespace de {

class HEdge;

/**
 * Helper/utility class intended to simplify the process of generating sections
 * of edge geometry from a map Line side segment.
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

    class Section; // forward

    /// Logical section identifiers:
    enum SectionId {
        SkyTop,
        SkyBottom,
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
         * Returns the Section to which the event is attributed.
         */
        Section &section() const;

        // Section needs access to attribute events.
        friend class Section;

    private:
        Section *_section; ///< The attributed section.
    };

    /**
     * Wall edge section.
     */
    class Section : public AbstractEdge
    {
    public:
        enum Flag
        {
            /// Force the geometry to be opaque, irrespective of material opacity.
            ForceOpaque           = 0x001,

            /// Fade out the geometry the closer it is to the viewer.
            NearFade              = 0x002,

            /// Clip the geometry if the neighbor plane surface relevant for the
            /// section has a sky-masked material bound to it.
            SkyClip               = 0x004,

            /// Sort the dynamic light projections by descending luminosity.
            SortDynLights         = 0x008,

            /// Do not project dynamic lights for the geometry.
            NoDynLights           = 0x010,

            /// Do not project dynamic (mobj) shadows for the geometry.
            NoDynShadows          = 0x020,

            /// Do not generate faked radiosity for the geometry.
            NoFakeRadio           = 0x040,

            /// Do not apply angle based light level deltas.
            NoLightDeltas         = 0x080,

            /// Do not intercept with the events of neighbor edges.
            NoEdgeDivisions       = 0x100,

            /// Do not smooth the edge normal.
            NoEdgeNormalSmoothing = 0x200
        };
        Q_DECLARE_FLAGS(Flags, Flag)

        /// A valid edge is required. @ingroup errors
        DENG2_ERROR(InvalidError);

        typedef QList<WallEdge::Event *> Events;

    public:
        /**
         * Returns the owning WallEdge for the section.
         */
        WallEdge &edge() const;

        /**
         * Returns the identifier for the section.
         */
        SectionId id() const;

        /**
         * Returns the specification flags for the section.
         */
        Flags flags() const;

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
        Section(WallEdge &owner, SectionId id, Flags const &flags = 0,
                Vector2f const &materialOrigin = Vector2f());

        DENG2_PRIVATE(d)
    };

    /**
     * Construct a new WallEdge for the specified @a hedge, @a side.
     *
     * @param hedge  Assumed to have a mapped LineSideSegment with sections.
     * @param side   Logical front side of the halfedge (0: left, 1: right).
     */
    WallEdge(HEdge &hedge, int side, float materialOffsetS = 0);

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
     * Returns the Section associated with @a id.
     *
     * @see sectionIdFromLineSideSection()
     */
    Section &section(SectionId id);

    /// Convenient accessor methods:
    inline Section &skyBottom()  { return section(SkyBottom); }
    inline Section &skyTop()     { return section(SkyTop); }
    inline Section &wallMiddle() { return section(WallMiddle); }
    inline Section &wallBottom() { return section(WallBottom); }
    inline Section &wallTop()    { return section(WallTop); }

private:
    DENG2_PRIVATE(d)
};

typedef WallEdge::Section WallEdgeSection;

Q_DECLARE_OPERATORS_FOR_FLAGS(WallEdge::Section::Flags)

} // namespace de

#endif // DENG_CLIENT_RENDER_WALLEDGE
