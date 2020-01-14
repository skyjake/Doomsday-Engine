/** @file line.h  Map line.
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#pragma once

#include "render/rend_main.h" // edgespan_t, shadowcorner_t
#include <doomsday/world/line.h>

class Line;
class LineSideSegment;
class Plane;

class LineSide : public world::LineSide
{
public:
    /**
     * POD: FakeRadio geometry and shadow state.
     */
    struct RadioData
    {
        std::array<edgespan_t, 2> spans;              ///< { bottom, top }
        std::array<shadowcorner_t, 2> topCorners;     ///< { left, right }
        std::array<shadowcorner_t, 2> bottomCorners;  ///< { left, right }
        std::array<shadowcorner_t, 2> sideCorners;    ///< { left, right }
        int updateFrame = 0;
    };

public:
    using world::LineSide::LineSide;
    
    inline Line &      line();
    inline const Line &line() const;
    
    /**
     * To be called to update the shadow properties for the line side.
     *
     * @todo Handle this internally -ds.
     */
    void updateRadioForFrame(int frameNumber);

    /**
     * Provides access to the FakeRadio shadowcorner_t data.
     */
    const shadowcorner_t &radioCornerTop   (bool right) const;
    const shadowcorner_t &radioCornerBottom(bool right) const;
    const shadowcorner_t &radioCornerSide  (bool right) const;

    /**
     * Provides access to the FakeRadio edgespan_t data.
     */
    const edgespan_t &radioEdgeSpan(bool top) const;

    void updateRadioCorner(shadowcorner_t &sc, float openness, Plane *proximityPlane = nullptr, bool top = false);

    /**
     * Change the FakeRadio side corner properties.
     */
    inline void setRadioCornerTop(bool right, float openness, Plane *proximityPlane = nullptr)
    {
        updateRadioCorner(radioData.topCorners[int(right)], openness, proximityPlane, true/*top*/);
    }

    inline void setRadioCornerBottom(bool right, float openness, Plane *proximityPlane = nullptr)
    {
        updateRadioCorner(radioData.bottomCorners[int(right)], openness, proximityPlane, false/*bottom*/);
    }

    inline void setRadioCornerSide(bool right, float openness)
    {
        updateRadioCorner(radioData.sideCorners[int(right)], openness);
    }

    /**
     * Change the FakeRadio "edge span" metrics.
     * @todo Replace shadow edge enumeration with a shadow corner enumeration. -ds
     */
    void setRadioEdgeSpan(bool top, bool right, double length);

private:
    RadioData radioData;
};

class LineSideSegment : public world::LineSideSegment
{
public:
    using world::LineSideSegment::LineSideSegment;

    /**
     * Returns @c true iff the segment is marked as "front facing".
     */
    bool isFrontFacing() const { return _frontFacing; }

    /**
     * Mark the current segment as "front facing".
     */
    void setFrontFacing(bool yes = true) { _frontFacing = yes; }

private:
    bool _frontFacing = false;
};

class Line : public world::Line
{
public:
    using world::Line::Line;
    
    /**
     * Returns @c true if the line qualifies for FakeRadio shadow casting (on planes).
     */
    bool isShadowCaster() const;
};

Line &LineSide::line()
{
    return world::LineSide::line().as<Line>();
};

const Line &LineSide::line() const
{
    return world::LineSide::line().as<Line>();
};
