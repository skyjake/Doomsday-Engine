/** @file vertex.cpp
 *
 * @authors Copyright (c) 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "world/vertex.h"

#include <doomsday/world/line.h>
#include <doomsday/world/lineowner.h>
#include <de/partition.h>

Vertex::Vertex(mesh::Mesh &mesh, const de::Vec2d &origin)
    : world::Vertex(mesh, origin)
{}

/**
 * Given two lines "connected" by shared origin coordinates (0, 0) at a "corner"
 * vertex, calculate the point which lies @a distA away from @a lineA and also
 * @a distB from @a lineB. The point should also be the nearest point to the
 * origin (in case of parallel lines).
 *
 * @param lineADirection  Direction vector for the "left" line.
 * @param dist1           Distance from @a lineA to offset the corner point.
 * @param lineBDirection  Direction vector for the "right" line.
 * @param dist2           Distance from @a lineB to offset the corner point.
 *
 * Return values:
 * @param point  Coordinates for the corner point are written here. Can be @c nullptr.
 * @param lp     Coordinates for the "extended" point are written here. Can be @c nullptr.
 */
static void cornerNormalPoint(const de::Vec2d &lineADirection,
                              double           dist1,
                              const de::Vec2d &lineBDirection,
                              double           dist2,
                              de::Vec2d *      point,
                              de::Vec2d *      lp)
{
    using namespace de;

    // Any work to be done?
    if(!point && !lp) return;

    // Length of both lines.
    double len1 = lineADirection.length();
    double len2 = lineBDirection.length();

    // Calculate normals for both lines.
    Vec2d norm1(-lineADirection.y / len1 * dist1,  lineADirection.x / len1 * dist1);
    Vec2d norm2( lineBDirection.y / len2 * dist2, -lineBDirection.x / len2 * dist2);

    // Do we need to calculate the extended points, too?  Check that
    // the extension does not bleed too badly outside the legal shadow
    // area.
    if(lp)
    {
        *lp = lineBDirection / len2 * dist2;
    }

    // Do we need to determine the intercept point?
    if(point)
    {
        // Normal shift to produce the lines we need to find the intersection.
        Partition lineA(lineADirection, norm1);
        Partition lineB(lineBDirection, norm2);

        if(lineA.isParallelTo(lineB))
        {
            // There will be no intersection at any point therefore it will not be
            // possible to determine our corner point (so just use a normal as the
            // point instead).
            *point = norm1;
        }
        else
        {
            *point = lineA.intercept(lineB);
        }
    }
}

/**
 * Returns the width (world units) of a shadow edge (scaled depending on the length of @a edge).
 */
static double shadowEdgeWidth(const de::Vec2d &edge)
{
    const double normalWidth = 20; //16;
    const double maxWidth    = 60;

    // A long edge?
    double length = edge.length();
    if(length > 600)
    {
        double w = length - 600;
        if(w > 1000) w = 1000;
        return normalWidth + w / 1000 * maxWidth;
    }

    return normalWidth;
}

void Vertex::updateShadowOffsets()
{
    using namespace de;

    world::LineOwner *base = firstLineOwner();
    if(!base) return;

    world::LineOwner *own = base;
    do
    {
        const auto &lineB = own->line();
        const auto &lineA = own->next()->line();

        const Vec2d rightDir = (&lineB.from() == this?  lineB.direction() : -lineB.direction());
        const Vec2d leftDir  = (&lineA.from() == this? -lineA.direction() :  lineA.direction()) * -1;  // Always flipped.

        cornerNormalPoint(leftDir,  shadowEdgeWidth(leftDir),
                          rightDir, shadowEdgeWidth(rightDir),
                          &own->_shadowOffsets.inner,
                          &own->_shadowOffsets.extended);

        own = own->next();
    } while(own != base);
}
