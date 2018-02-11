/** @file polygon.cpp
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

#include "polygon.h"
#include "../geomath.h"

using namespace de;

namespace gloom { namespace geo {

Polygon::Polygon(const Points &points)
    : points(points)
{
    updateBounds();
}

void Polygon::updateBounds()
{
    if (size() == 0)
    {
        bounds = Rectangled();
        return;
    }
    bounds = Rectangled(at(0), at(0));
    for (int i = 1; i < size(); ++i)
    {
        bounds.include(points[i].pos);
    }
}

const Vector2d &Polygon::at(int pos) const
{
    return points[mod(pos, size())].pos;
}

const Polygon::Line Polygon::lineAt(int pos) const
{
    return Line(at(pos), at(pos + 1));
}

bool Polygon::isConvex() const
{
    if (points.size() <= 3)
    {
        return true;
    }
    for (int i = 0; i < points.size(); ++i)
    {
        if (lineAt(i).normal().dot(lineAt(i + 1).span()) < 0)
        {
            return false;
        }
    }
    return true;
}

QVector<int> Polygon::concavePoints() const
{
    QVector<int> concave;
    if (points.size() <= 3) return concave;
    for (int i = 0; i < points.size(); ++i)
    {
        if (lineAt(i - 1).normal().dot(lineAt(i).span()) < 0)
        {
            concave << i;
        }
    }
    return concave;
}

bool Polygon::isLineInside(int start, int end) const
{
    //if (mod(start + 1, size()) == end  ) return false;
    //if (mod(end   + 1, size()) == start) return false;

    const auto &a = points[start].pos;
    const auto &b = points[end  ].pos;
    const Line line(a, b);

    // Does the line a--b intersect the polygon?
    for (int i = 0; i < size(); ++i)
    {
        const int j = mod(i + 1, size());
        if (i == start || i == end || j == start || j == end)
        {
            // Ignore other lines connecting to the specified points.
            continue;
        }
        double t;
        if (line.intersect(lineAt(i), t))
        {
            if (t >= 0.0 && t <= 1.0) return false;
        }
    }

    // Is the mid point inside the polygon?
    return isPointInside((a + b) / 2);
}

bool Polygon::isPointInside(const Vector2d &point) const
{
    DENG2_ASSERT(!bounds.isNull());
    if (bounds.contains(point))
    {
        const Vector2d offset{bounds.width() + 1, 0};
        return intersect(Line(point, point + offset)) % 2 == 1;
    }
    return false;
}

int Polygon::intersect(const Line &line) const
{
    int count = 0;
    for (int i = 0; i < points.size(); ++i)
    {
        double t;
        if (line.intersect(lineAt(i), t))
        {
            if (t >= 0.0 && t < 1.0)
            {
                count++;
            }
        }
    }
    return count;
}

bool Polygon::split(int a, int b, Polygon halves[2]) const
{
    for (int pos = a; pos != b; pos = mod(pos + 1, size()))
    {
        halves[0].points << points[pos];
    }
    halves[0].points << points[b];

    for (int pos = b; pos != a; pos = mod(pos + 1, size()))
    {
        halves[1].points << points[pos];
    }
    halves[1].points << points[a];

    for (int i = 0; i < 2; ++i)
    {
        halves[i].updateBounds();
    }

    // Each half must at least be a triangle.
    return halves[0].size() >= 3 && halves[1].size() >= 3;
}

static bool areAllConvex(const QList<Polygon> &polygon)
{
    for (const auto &poly : polygon)
    {
        if (!poly.isConvex()) return false;
    }
    return true;
}

QList<Polygon> Polygon::splitConvexParts() const
{
    QList<Polygon> parts({*this});

    // The parts that are not convex will be split to smaller parts.
    for (int i = 0; i < parts.size(); ++i)
    {
        Polygon &poly = parts[i];
        const auto insets = poly.concavePoints();
        if (insets.size() > 0)
        {
            const int j = insets.first();
            for (int k = mod(j + 2, poly.size()); k != j; k = mod(k + 1, poly.size()))
            {
                if (poly.isLineInside(j, k))
                {
                    Polygon halves[2];
                    if (poly.split(j, k, halves))
                    {
                        parts.removeAt(i);
                        parts.append(halves[0]);
                        parts.append(halves[1]);
                        --i;
                        break;
                    }
                }
            }
        }
    }

    DENG2_ASSERT(areAllConvex(parts));
    return parts;
}

}} // namespace gloom::geo
