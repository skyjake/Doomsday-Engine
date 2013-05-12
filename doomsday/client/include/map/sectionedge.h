/** @file map/sectionedge.h World Map (Wall) Section Edge.
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

#ifndef DENG_WORLD_MAP_SECTIONEDGE
#define DENG_WORLD_MAP_SECTIONEDGE

#include <QList>

#include <de/Error>
#include <de/Vector>

#include "Line"
#include "HEdge"
#include "IHPlane"

class Surface;

/// Maximum number of intercepts in a SectionEdge.
#define SECTIONEDGE_MAX_INTERCEPTS          64

/**
 * Helper/utility class intended to simplify the process of generating
 * sections of geometry from a map line segment.
 *
 * @ingroup map
 */
class SectionEdge
{
public:
    /// Invalid range geometry was found during prepare() @ingroup errors
    DENG2_ERROR(InvalidError);

    class Intercept : public de::IHPlane::IIntercept
    {
    public:
        Intercept(SectionEdge *owner, double distance = 0);
        Intercept(Intercept const &other);

        SectionEdge &owner() const;

        de::Vector3d origin() const;

    private:
        SectionEdge *_owner;
    };

    typedef QList<Intercept> Intercepts;

public:
    SectionEdge(Line::Side &lineSide, int section, coord_t lineOffset,
                Vertex &lineVertex, de::ClockDirection neighborScanDirection);

    SectionEdge(HEdge &hedge, int section, int edge);

    SectionEdge(SectionEdge const &other);

    inline SectionEdge &operator = (SectionEdge other) {
        d.swap(other.d);
        return *this;
    }

    inline void swap(SectionEdge &other) {
        d.swap(other.d);
    }

    inline Intercept const & operator [] (int index) const { return at(index); }

    Intercept const &at(int index) const;

    void prepare();

    bool isValid() const;

    de::Vector2d const &origin() const;

    coord_t lineOffset() const;

    Line::Side &lineSide() const;

    Surface &surface() const;

    int section() const;

    int divisionCount() const;

    Intercept const &bottom() const;

    Intercept const &top() const;

    int firstDivision() const;

    int lastDivision() const;

    de::Vector2f const &materialOrigin() const;

    Intercepts const &intercepts() const;

private:
    DENG2_PRIVATE(d)
};

namespace std {
// std::swap specialization for SectionEdge
template <>
inline void swap<SectionEdge>(SectionEdge &a, SectionEdge &b) {
    a.swap(b);
}
}

#endif // DENG_WORLD_MAP_SECTION_EDGE
