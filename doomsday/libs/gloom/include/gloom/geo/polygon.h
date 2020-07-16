/** @file polygon.h
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef GLOOM_POLYGON_H
#define GLOOM_POLYGON_H

#include "gloom/geo/geomath.h"
#include "gloom/identity.h"

#include <de/hash.h>
#include <de/list.h>
#include <de/rectangle.h>
#include <de/vector.h>

namespace gloom { namespace geo {

using namespace de;

struct LIBGLOOM_PUBLIC Polygon
{
    struct Point {
        Vec2d pos; // world X, Z
        ID id;
    };
    using Points = List<Point>;
    using Line   = geo::Line<Vec2d>;

    Points     points; // clockwise winding
    Rectangled bounds;

    explicit Polygon(const Points &points = Points());
    Polygon(const Polygon &) = default;

    int             size() const { return points.sizei(); }
    inline bool     isEmpty() const { return size() == 0; }
    const Vec2d &   at(int pos) const;
    const Point &   pointAt(int pos) const;
    const Line      lineAt(int pos) const;
    bool            isConvex() const;
    List<Polygon>   splitConvexParts() const;
    List<int>       concavePoints() const;
    Rangei          findLoop(int findStartPos = 0) const;
    bool            hasDegenerateEdges() const;
    bool            isClockwiseWinding() const;
    bool            isUnique(int pos) const;
    bool            isEdgeLine(int start, int end) const;
    bool            isPointInside(const Vec2d &point) const;
    bool            isLineInside(int start, int end) const;
    bool            isInsideOf(const Polygon &largerPoly) const;
    int             intersect(const Line &line) const;
    bool            split(int a, int b, Polygon halves[2]) const;
    bool            split(const Rangei &range, Polygon halves[2]) const;
    void            updateBounds();
    void            clear();
    Vec2d           center() const;
    Vec2d           expander(int pos) const;
    Hash<ID, Vec2d> expanders() const;
    String          asText() const;
};

}} // namespace gloom::geo

#endif // GLOOM_POLYGON_H
