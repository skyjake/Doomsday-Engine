/** @file angleclipper.h  Angle Clipper.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_RENDER_ANGLECLIPPER
#define CLIENT_RENDER_ANGLECLIPPER

#include <de/binangle.h>
#include <de/Vector>
#include "Face"

/**
 * 360 degree, polar angle(-range) clipper.
 *
 * The idea is to keep track of occluded angles around the camera. Since BSP leafs are
 * rendered front-to-back, the occlusion lists start a frame empty and eventually fill
 * up to cover the whole 360 degrees around the camera.
 *
 * Oranges (occlusion ranges) clip a half-space on an angle range. These are produced
 * by horizontal edges that have empty space behind.
 *
 * @ingroup render
 */
class AngleClipper
{
public:
    AngleClipper();

    /**
     * Returns non-zero if clipnodes cover the whole range [0..360] degrees.
     */
    de::dint isFull() const;

    /**
     * Returns non-zero if the given map-space @param angle is @em not-yet occluded.
     */
    de::dint isAngleVisible(binangle_t angle) const;

    /**
     * Returns non-zero if the given map-space @a point is @em not-yet occluded.
     *
     * @param point  Map-space coordinates to test.
     */
    de::dint isPointVisible(de::Vector3d const &point) const;

    /**
     * Returns non-zero if @em any portion of the given map-space, convex face geometry
     * is @em not-yet occluded.
     *
     * @param poly  Map-space convex face geometry to test.
     */
    de::dint isPolyVisible(de::Face const &poly) const;

public:  // ---------------------------------------------------------------------------

    /**
     *
     */
    void clearRanges();

    /**
     *
     */
    de::dint safeAddRange(binangle_t startAngle, binangle_t endAngle);

    /**
     * Add a segment relative to the current viewpoint.
     *
     * @param from  Map-space coordinates describing the start-point.
     * @param to    Map-space coordinates describing the end-point.
     */
    void addRangeFromViewRelPoints(de::Vector2d const &from, de::Vector2d const &to);

    /**
     * Add an occlusion segment relative to the current viewpoint.
     *
     * @param from     Map-space coordinates for the start-point.
     * @param to       Map-space coordinates for the end-point.
     * @param height
     * @param topHalf
     */
    void addViewRelOcclusion(de::Vector2d const &from, de::Vector2d const &to, coord_t height,
                             bool topHalf);

    /**
     * Check a segment relative to the current viewpoint.
     *
     * @param from  Map-space coordinates for the start-point.
     * @param to    Map-space coordinates for the end-point.
     */
    de::dint checkRangeFromViewRelPoints(de::Vector2d const &from, de::Vector2d const &to);

#ifdef DENG2_DEBUG
    /**
     * A debugging aid: checks if clipnode links are valid.
     */
    void validate();
#endif

private:
    DENG2_PRIVATE(d)
};

#endif  // CLIENT_RENDER_ANGLECLIPPER
