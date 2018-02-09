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

#include <QVector>
#include <de/Vector>
#include <de/Rectangle>
#include "../geomath.h"

namespace gloom {

struct Polygon
{
    struct Point {
        de::Vector2d pos;
        uint32_t id;
    };
    using Points = QVector<Point>;
    using Line   = geo::Line<de::Vector2d>;

    Points points; // clockwise winding
    de::Rectangled bounds;

    Polygon(const Points &points = Points());
    Polygon(const Polygon &) = default;

    int size() const { return points.size(); }
    const de::Vector2d &at(int pos) const;
    const Line lineAt(int pos) const;
    bool isConvex() const;
    QList<Polygon> splitConvexParts() const;
    QVector<int> concavePoints() const;
    bool isPointInside(const de::Vector2d &point) const;
    bool isLineInside(int start, int end) const;
    int intersect(const Line &line) const;
    bool split(int a, int b, Polygon halves[2]) const;
    void updateBounds();
};

} // namespace gloom

#endif // GLOOM_POLYGON_H
