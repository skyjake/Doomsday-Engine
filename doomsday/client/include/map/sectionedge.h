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

#include <de/Error>
#include <de/Vector>

#include "Line"
#include "HEdge"

#include "render/walldiv.h"

/**
 * Helper/utility class intended to simplify the process of generating
 * sections of drawable geometry from a map line segment.
 *
 * @ingroup map
 */
class SectionEdge
{
public:
    /// Invalid range geometry was found during prepare() @ingroup errors
    DENG2_ERROR(InvalidError);

public:
    SectionEdge(HEdge &hedge, int edge, int section);

    void prepare();

    bool isValid() const;

    de::Vector2d const &origin() const;

    coord_t lineOffset() const;

    Line::Side &lineSide() const;

    int section() const;

    int divisionCount() const;

    de::WallDivs::Intercept &bottom() const;

    de::WallDivs::Intercept &top() const;

    de::WallDivs::Intercept &firstDivision() const;

    de::WallDivs::Intercept &lastDivision() const;

    de::Vector2f const &materialOrigin() const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_MAP_SECTION_EDGE
