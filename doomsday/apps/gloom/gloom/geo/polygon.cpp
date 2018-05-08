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

#include "gloom/geo/polygon.h"
#include "gloom/geo/geomath.h"

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
        // Polygon points must be unique.
        DENG2_ASSERT(points[i].id  != points[i - 1].id);
        DENG2_ASSERT(points[i].pos != points[i - 1].pos);

        bounds.include(points[i].pos);
    }
}

void Polygon::clear()
{
    bounds = Rectangled();
    points.clear();
}

Vec2d Polygon::center() const
{
    Vec2d c;
    if (points.size())
    {
        for (const auto &p : points)
        {
            c += p.pos;
        }
        c /= points.size();
    }
    return c;
}

Vec2d Polygon::expander(int pos) const
{
    return (-lineAt(pos - 1).normal() - lineAt(pos).normal()).normalize();
}

QHash<ID, Vec2d> Polygon::expanders() const
{
    QHash<ID, Vec2d> exp;
    for (int i = 0; i < points.size(); ++i)
    {
        exp.insert(points[i].id, expander(i));
    }
    return exp;
}

String Polygon::asText() const
{
    String str;
    QTextStream os(&str);
    os << "Polygon: [" << points.size() << "]";
    for (int i = 0; i < points.size(); ++i)
    {
        os << String::format(" %x", points[i].id);
    }
    return str;
}

const Vec2d &Polygon::at(int pos) const
{
    return pointAt(pos).pos;
}

const Polygon::Point &Polygon::pointAt(int pos) const
{
    return points[mod(pos, size())];
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
        if (lineAt(i).normal().dot(lineAt(i + 1).dir()) < 0)
        {
            return false;
        }
    }
    return true;
}

QVector<int> Polygon::concavePoints() const
{
    QVector<int> concave;
    if (points.size() <= 3) return concave; // must be convex
    for (int i = 0; i < points.size(); ++i)
    {
        if (lineAt(i - 1).normal().dot(lineAt(i).dir()) < 0)
        {
            concave << i;
        }
    }
    return concave;
}

bool Polygon::isUnique(int pos) const
{
    const ID pointId = pointAt(pos).id;
    int count = 0;
    for (int i = 0; i < points.size(); ++i)
    {
        if (points[i].id == pointId) ++count;
    }
    return count == 1;
}

bool Polygon::isEdgeLine(int start, int end) const
{
    if (points[start].id == points[end].id) return true; // Edge point, to be accurate.

    for (int i = 0; i < size(); ++i)
    {
        const int j = mod(i + 1, size());
        if ((points[i].id == points[start].id && points[j].id == points[end].id) ||
            (points[i].id == points[end].id   && points[j].id == points[start].id))
        {
            return true;
        }
    }
    return false;
}

bool Polygon::isLineInside(int start, int end) const
{
    const auto   exp1  = expander(start); // points outward
    const auto   exp2  = expander(end);
    const double THICK = 0.001;
    const auto   a     = points[start].pos - exp1 * THICK;
    const auto   b     = points[end].pos   - exp2 * THICK;

    Line check(a, b);

//    Line check(points[start].pos, points[end].pos);
//    const auto dir = check.dir();
//    check.start += dir * THICK;
//    check.end   -= dir * THICK;

    // Both endpoints must be inside.
    if (!isPointInside(check.start))
    {
        qDebug("start %i outside", start);
        return false;
    }
    if (!isPointInside(check.end))
    {
        qDebug("end %i outside", end);
        return false;
    }

    // It can't intersect any of the lines.
    const int isc = intersect(check);
//    if (isc) qDebug("%i-%i isc=%i", start, end, isc);
    return isc == 0;

/*    // Does the line a--b intersect the polygon?
    for (int i = 0; i < size(); ++i)
    {
        const int j = mod(i + 1, size());
        if ((points[i].id == points[start].id && points[j].id == points[end].id) ||
            (points[i].id == points[end].id   && points[j].id == points[start].id))
        {
            // This is one of the polygon lines!
            return false;
        }
        if (i == start || i == end || j == start || j == end)
        {
            // Ignore other lines connecting to the specified points.
            continue;
        }
        double t;
        if (line.intersect(lineAt(i), t))
        {
            if (t > 0.0 && t < 1.0) return false;
        }
    }

    // Is the mid point inside the polygon?
    return isPointInside((a + b) / 2);*/
}

bool Polygon::isInsideOf(const Polygon &largerPoly) const
{
    if (!largerPoly.bounds.overlaps(bounds)) return false;

    // Check point-by-point.
    for (const auto &pp : points)
    {
        if (!largerPoly.isPointInside(pp.pos)) return false;
    }

    return true;
}

bool Polygon::isPointInside(const Vec2d &point) const
{
    if (points.size() < 3) return false;

    DENG2_ASSERT(!bounds.isNull());
    if (bounds.contains(point))
    {
        const Vec2d offset{bounds.width() + 1, 0};
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
//                qDebug("  %f,%f -> %f,%f intersects %f,%f -> %f,%f",
//                       lineAt(i).start.x,
//                       lineAt(i).start.y,
//                       lineAt(i).end.x,
//                       lineAt(i).end.y,
//                       line.start.x,
//                       line.start.y,
//                       line.end.x,
//                       line.end.y);

                count++;
            }
        }
    }
    return count;
}

bool Polygon::split(int a, int b, Polygon halves[2]) const
{
    int half = 0;
    for (int i = 0; i < size(); ++i)
    {
        halves[half].points << points[i];
        if (i == a || i == b)
        {
            half ^= 1;
            halves[half].points << points[i];
        }
    }

    for (int i = 0; i < 2; ++i)
    {
        halves[i].updateBounds();
        if (!halves[i].isClockwiseWinding()) return false;
    }

    // Each half must at least be a triangle.
    return !halves[0].hasDegenerateEdges() && !halves[1].hasDegenerateEdges();
}

static bool areAllConvex(const QList<Polygon> &polygon)
{
    for (const auto &poly : polygon)
    {
        if (!poly.isConvex()) return false;
    }
    return true;
}

Rangei Polygon::findLoop() const
{
    // Having a loop means there's at least two triangles.
    if (points.size() < 6) return Rangei();

    for (int i = 0; i < points.size(); ++i)
    {
        const ID startPoint = points[i].id;
        const ID endPoint   = pointAt(i + 3).id;
        if (startPoint == endPoint) // && pointAt(i - 1).id != pointAt(i + 4).id)
        {
            return Rangei(i, i + 3);
        }
    }
    return Rangei();
}

bool Polygon::hasDegenerateEdges() const
{
    if (points.isEmpty()) return false;
    if (size() < 3) return true;

    // None of the points can be exactly on the resulting edge lines.
    // This would result in degenerate triangles.

    const double EPSILON = 0.0001;

    for (int p = 0; p < size(); ++p)
    {
        if (points[p].id == pointAt(p + 2).id)
        {
            // This edge forms a zero-area line.
            return true;
        }

        const Vec2d check = points[p].pos;

        for (int j = 0; j < size(); ++j)
        {
            double t, dist = lineAt(j).normalDistance(check, t);
            if (dist < EPSILON && t > EPSILON && t < 1.0 - EPSILON)
            {
                // Not acceptable; the point falls too close to another line
                // on the polygon.
                return true;
            }
        }
    }
    return false;
}

bool Polygon::isClockwiseWinding() const
{
    double angles = 0.0;

    // Calculate sum of all line angles.
    for (int i = 0; i < size(); ++i)
    {
        angles += lineAt(i).angle(lineAt(i + 1)) - 180.0;
    }

    qDebug() << "Winding is" << angles << "for" << asText().toLatin1().constData();

    return angles < 0;
}

bool Polygon::split(const Rangei &range, Polygon halves[2]) const
{
    // Points in the loop.
    for (int i = range.start; i < range.end; ++i)
    {
        halves[0].points << pointAt(i);
    }

    // Points outside the loop.
    for (int i = 0; i < size(); ++i)
    {
        if (range.end <= size())
        {
            if (!range.contains(i))
            {
                halves[1].points << points[i];
            }
        }
        else
        {
            if (i >= mod(range.end, size()) && i < range.start)
            {
                halves[1].points << points[i];
            }
        }
    }

    halves[0].updateBounds();
    halves[1].updateBounds();

    if (!halves[0].isClockwiseWinding() ||
        !halves[1].isClockwiseWinding()) return false;

    return !halves[0].hasDegenerateEdges() && !halves[1].hasDegenerateEdges();
}

QList<Polygon> Polygon::splitConvexParts() const
{
    QList<Polygon> parts({*this});

    // The parts that are not convex will be split to smaller parts.
    for (int i = 0; i < parts.size(); ++i)
    {
        // Loops should be always split to separate polygons.
        while (Rangei loop = parts[i].findLoop())
        {
            qDebug() << "Found a loop in" << parts[i].asText() << "indices:" << loop.asText();
            Polygon halves[2];
            if (parts[i].split(loop, halves))
            {
                qDebug() << "  Splitting to:\n    " << halves[0].asText();
                qDebug() << "    " << halves[1].asText();
                parts.removeAt(i);
                parts.insert(i, halves[0]);
                parts.insert(i, halves[1]);
            }
            else break;
        }

        Polygon &poly = parts[i];

        const auto insets = poly.concavePoints();
        if (!insets.isEmpty())
        {
            qDebug() << "Splitting concave" << poly.asText() << "this is part:" << i;
            qDebug("- found %i concave inset points", insets.size());

            for (int j : insets) qDebug("   %i : %x", j, poly.pointAt(j).id);

            struct CandidateSplit
            {
                Polygon halves[2];
                int score;
            };
            QVector<CandidateSplit> availableSplits;

            const int maxAvailable = 50;

            for (const int j : insets)
            {
                DENG2_ASSERT(poly.size() >= 4);

                qDebug("   trying with %i", j);
                for (int k = mod(j + 2, poly.size()); k != j; k = mod(k + 1, poly.size()))
                {
                    if (!poly.isEdgeLine(j, k) && poly.isLineInside(j, k))
                    {
                        Polygon halves[2];
                        if (poly.split(j, k, halves))
                        {
                            qDebug("     possible split with line %x...%x : %i/%i (cvx:%i/%i, edge:%s, inside:%s)",
                                   poly.pointAt(j).id,
                                   poly.pointAt(k).id,
                                   halves[0].size(),
                                   halves[1].size(),
                                   int(halves[0].isConvex()),
                                   int(halves[1].isConvex()),
                                   poly.isEdgeLine(j, k)?"true":"false",
                                   poly.isLineInside(j, k)?"true":"false");

//                            parts.removeAt(i);
//                            parts.append(halves[0]);
//                            parts.append(halves[1]);
//                            qDebug() << "       Half 1:" << halves[0].asText();
//                            qDebug() << "       Half 2:" << halves[1].asText();
//                            --i;
//                            wasSplit = true;

                            int score = de::min(halves[0].size(), halves[1].size());
                            if (halves[0].isConvex())
                            {
                                score *= 4;
                            }
                            if (halves[1].isConvex())
                            {
                                score *= 4;
                            }
                            availableSplits << CandidateSplit{{halves[0], halves[1]}, score};
//                            break;
                        }
                        else
                        {
                            qDebug("     line %x...%x does not split to triangles",
                                   poly.pointAt(j).id,
                                   poly.pointAt(k).id);
                        }
                    }
                    else
                    {
                        qDebug("     line %x...%x not fully inside (u:%i,%i)",
                               poly.pointAt(j).id, poly.pointAt(k).id,
                               poly.isUnique(j), poly.isUnique(k));
                    }
                }
                if (availableSplits.size() >= maxAvailable) break; // That should be enough.
//                if (wasSplit) break;
            }
            if (availableSplits.isEmpty())
            {
                qDebug("have %i insets, couldn't find a split", insets.size());
                DENG2_ASSERT(!availableSplits.isEmpty());
            }
            else
            {
                std::sort(availableSplits.begin(), availableSplits.end(),
                          [](const CandidateSplit &a, const CandidateSplit &b) {
                              return a.score > b.score;
                          });

//                qDebug("    Possible solutions:");
//                for (const auto &sel : availableSplits)
//                {
//                    qDebug("      s:%i\n       %s\n       %s",
//                           sel.score,
//                           sel.halves[0].asText().toLatin1().constData(),
//                           sel.halves[1].asText().toLatin1().constData());
//                }

                const auto &solution = availableSplits.front();
                qDebug("     using solution with score %i (out of %i solutions)", solution.score,
                       availableSplits.size());
                qDebug() << "       Half 1:" << solution.halves[0].asText();
                qDebug() << "       Half 2:" << solution.halves[1].asText();

                parts.removeAt(i--);
                parts.append(solution.halves[0]);
                parts.append(solution.halves[1]);
            }
        }
    }

    if (parts.size() > 1)
    {
        qDebug("Polygon with %i points split to %i convex parts",
               size(),
               parts.size());
    }

    DENG2_ASSERT(areAllConvex(parts));
    return parts;
}

}} // namespace gloom::geo
