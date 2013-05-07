/** @file render/shadowedge.h FakeRadio Shadow Edge Geometry
 *
 * @authors Copyright &copy; 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RENDER_FAKERADIO_SHADOWEDGE_H
#define DENG_RENDER_FAKERADIO_SHADOWEDGE_H

#include <de/Vector>

#include "Line"

class BspLeaf;

namespace de {

static coord_t const SHADOWEDGE_OPEN_THRESHOLD = 8; // world units (Z axis)

/**
 * @ingroup render
 */
class ShadowEdge
{
public:
    ShadowEdge(BspLeaf &bspLeaf, Line::Side &side, int edge);

    void prepare(int planeIndex);

    /**
     * Returns the "side openness" factor for the shadow edge. This factor is
     * a measure of how open the @em open range is on "this" edge of the
     * shadowing surface.
     *
     * @see sectorOpenness()
     */
    float openness() const;

    /**
     * Returns the "sector openness" factor for the shadow edge. This factor is
     * a measure of how open the @em open range is on the sector edge of the
     * shadowing surface.
     *
     * @see openness()
     */
    float sectorOpenness() const;

    /**
     * Returns the origin of the @em outer vertex (that which is incident with
     * a map vertex from some line of the sector boundary).
     *
     * @see inner()
     */
    Vector3d const &outer() const;

    /**
     * Returns the origin of the @em inner vertex (that which extends away from
     * the "outer" vertex and across the shadowed surface).
     *
     * @see outer()
     */
    Vector3d const &inner() const;

    /**
     * Returns the length of the shadow edge, which is measured as the distance
     * from the outer vertex origin to the inner origin.
     *
     * @see outer(), inner()
     */
    inline coord_t length() const {
        return Vector3d(inner() - outer()).abs().length();
    }

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_RENDER_FAKERADIO_SHADOWEDGE_H
