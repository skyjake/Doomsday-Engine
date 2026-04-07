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
        DE_ASSERT(points[i].id  != points[i - 1].id);
        DE_ASSERT(points[i].pos != points[i - 1].pos);

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

Hash<ID, Vec2d> Polygon::expanders() const
{
    Hash<ID, Vec2d> exp;
    for (int i = 0; i < points.sizei(); ++i)
    {
        exp.insert(points[i].id, expander(i));
    }
    return exp;
}

String Polygon::asText() const
{
    std::ostringstream os;
    os << "Polygon: [" << points.size() << "]";
    for (int i = 0; i < points.sizei(); ++i)
    {
        os << Stringf(" %x", points[i].id);
    }
    return os.str();
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
    for (int i = 0; i < points.sizei(); ++i)
    {
        if (lineAt(i).normal().dot(lineAt(i + 1).dir()) < 0)
        {
            return false;
        }
    }
    return true;
}

List<int> Polygon::concavePoints() const
{
    List<int> concave;
    if (points.size() <= 3) return concave; // must be convex
    for (int i = 0; i < points.sizei(); ++i)
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
    for (int i = 0; i < points.sizei(); ++i)
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
//        qDebug("\tstart %i (%x) outside", start, points[start].id);
        return false;
    }
    if (!isPointInside(check.end))
    {
//        qDebug("\tend %i (%x) outside", end, points[end].id);
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

    DE_ASSERT(!bounds.isNull());

    if (bounds.contains(point))
    {
        const double checkLen = max(bounds.width(), bounds.height()) + 1;

        // Find a line that doesn't hit exactly on any points.
        Line check{point, point + Vec2d(checkLen, 1)};
        /*
        bool pointOverlap = true;
        while (pointOverlap)
        {
            const double randomAngle = frand() * 2 * PI;
            check = Line{point, point + Vec2d(std::cos(randomAngle),
                                              std::sin(randomAngle)) * checkLen};
            pointOverlap = false;
            for (int i = 0; i < size(); ++i)
            {
                double t;
                if (check.normalDistance(points[i].pos, t) < EPSILON)
                {
                    if (t > 0)
                    {
                        pointOverlap = true;
                        break;
                    }
                }
            }
        }*/
        return intersect(check) % 2 == 1;
    }
    return false;
}

int Polygon::intersect(const Line &check) const
{
    int count = 0;
    const auto checkDir = check.dir();
    for (int i = 0; i < points.sizei(); ++i)
    {
        double t;
//        if (check.normalDistance(pointAt(i).pos, t) < 0.0001)
//        {
//            if (t >= 0.0 && t <= 1.0)
//            {
//                // The checked line is over the point, which means
//                // we can safely skip both adjacent lines.
//                count++;
//                continue;
//            }
//        }

        if (lineAt(i).intersect(check, t))
        {
            const bool isEndPeak = sign(checkDir.dot(lineAt(i    ).normal())) !=
                                   sign(checkDir.dot(lineAt(i + 1).normal()));

            if (t >= 0.0 && ((!isEndPeak && t < 1.0) || (isEndPeak && t <= 1.0)))
            {
//                qDebug("  %f,%f -> %f,%f intersects %f,%f -> %f,%f [t:%f]",
//                       lineAt(i).start.x,
//                       lineAt(i).start.y,
//                       lineAt(i).end.x,
//                       lineAt(i).end.y,
//                       check.start.x,
//                       check.start.y,
//                       check.end.x,
//                       check.end.y,
//                       t);

                count++;
            }
        }
    }
    return count;
}

bool Polygon::split(int a, int b, Polygon halves[2]) const
{
    for (int i = 0; i < 2; ++i)
    {
        halves[i].clear();
    }

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
        if (!halves[i].isClockwiseWinding())
        {
//            qDebug("\thalf %i has the wrong winding", i);
            return false;
        }
        // Each half must at least be a triangle.
        if (halves[i].hasDegenerateEdges())
        {
//            qDebug("\thalf %i has degenerate edges", i);
            return false;
        }
    }
    return true;
}

static bool areAllConvex(const List<Polygon> &polygon)
{
    for (const auto &poly : polygon)
    {
        if (!poly.isConvex()) return false;
    }
    return true;
}

Rangei Polygon::findLoop(int findStartPos) const
{
    // Having a loop means there's at least two triangles.
    if (points.size() < 6) return Rangei();

    for (int i = findStartPos; i < points.sizei(); ++i)
    {
        const ID startPoint = points[i].id;

        for (int j = 3; j < points.sizei() - 2; ++j)
        {
            const ID endPoint = pointAt(i + j).id;
            if (startPoint == endPoint)
            {
                return Rangei(i, i + j);
            }
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
//            qDebug("\tpoint %i +2 is an empty area", points[p].id);
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
//                qDebug("\tpoint %i (%x) is on the edge line %i (%x...%x)",
//                       p,
//                       points[p].id,
//                       j,
//                       points[j].id,
//                       pointAt(j + 1).id);
                return true;
            }
        }
    }
    return false;
}

bool Polygon::isClockwiseWinding() const
{
    if (size() < 3) return true;

    double angles = 0.0;

    // Calculate sum of all line angles.
    for (int i = 0; i < size(); ++i)
    {
        angles += lineAt(i).angle(lineAt(i + 1)) - 180.0;
    }

//    qDebug() << "\tWinding is" << angles << "for" << asText().toLatin1().constData();

    return angles < -180; // should be around -360
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

List<Polygon> Polygon::splitConvexParts() const
{
    List<Polygon> parts({*this});

    // The parts that are not convex will be split to smaller parts.
    for (int i = 0; i < parts.sizei(); ++i)
    {
        // Loops should be always split to separate polygons.
        int findBegin = 0;
        while (Rangei loop = parts[i].findLoop(findBegin))
        {
//            qDebug() << "Found a loop in" << parts[i].asText() << "indices:" << loop.asText();
            Polygon halves[2];
            if (parts[i].split(loop, halves))
            {
//                qDebug() << "  Splitting to:\n    " << halves[0].asText();
//                qDebug() << "    " << halves[1].asText();
                parts.removeAt(i);
                parts.insert(i, halves[0]);
                parts.insert(i, halves[1]); // part with the loop removed ends up at `i` again
                findBegin = 0;
            }
            else
            {
                findBegin = loop.end;
            }
        }

        Polygon &poly = parts[i];

        const auto insets = poly.concavePoints();
        if (!insets.isEmpty())
        {
//            qDebug() << "Splitting concave" << poly.asText() << "this is part:" << i;
//            qDebug("- found %i concave inset points", insets.size());
//            for (int j : insets) qDebug("   %i : %x", j, poly.pointAt(j).id);

            struct CandidateSplit
            {
                Polygon halves[2];
                int     score;
            };
            List<CandidateSplit> availableSplits;

            const int maxAvailable = 50;

            for (const int j : insets)
            {
                DE_ASSERT(poly.size() >= 4);

//                qDebug("   trying with %i", j);
                for (int k = mod(j + 2, poly.size()); k != j; k = mod(k + 1, poly.size()))
                {
                    if (!poly.isEdgeLine(j, k) && poly.isLineInside(j, k))
                    {
                        Polygon halves[2];

                        int splitStart = j;
                        int splitEnd   = k;

                        bool ok = poly.split(splitStart, splitEnd, halves);
                        if (!ok)
                        {
                            std::swap(splitStart, splitEnd);
                            ok = poly.split(splitStart, splitEnd, halves);
                        }

                        if (ok)
                        {
                            /*
                            qDebug("     possible split with line %x...%x : %i/%i (cvx:%i/%i, edge:%s, inside:%s)",
                                   poly.pointAt(splitStart).id,
                                   poly.pointAt(splitEnd).id,
                                   halves[0].size(),
                                   halves[1].size(),
                                   int(halves[0].isConvex()),
                                   int(halves[1].isConvex()),
                                   poly.isEdgeLine(splitStart, splitEnd)?"true":"false",
                                   poly.isLineInside(splitStart, splitEnd)?"true":"false");
                                   */

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
                        }
                        else
                        {
//                            qDebug("     line %x...%x fails to split",
//                                   poly.pointAt(j).id,
//                                   poly.pointAt(k).id);
                        }
                    }
                    else
                    {
//                        qDebug("     line %x...%x not fully inside (u:%i,%i)",
//                               poly.pointAt(j).id, poly.pointAt(k).id,
//                               poly.isUnique(j), poly.isUnique(k));
                    }
                }
                if (availableSplits.size() >= maxAvailable)
                {
                    break; // That should be enough.
                }
            }
            if (availableSplits.isEmpty())
            {
                debug("have %i insets, couldn't find a split\n%s",
                      insets.sizei(),
                      poly.asText().c_str());
                DE_ASSERT(!availableSplits.isEmpty());
            }
            else
            {
                std::sort(availableSplits.begin(), availableSplits.end(),
                          [](const CandidateSplit &a, const CandidateSplit &b) {
                              return a.score > b.score;
                          });

                const auto &solution = availableSplits.front();
//                qDebug("     using solution with score %i (out of %i solutions)", solution.score,
//                       availableSplits.size());
//                qDebug() << "       Half 1:" << solution.halves[0].asText();
//                qDebug() << "       Half 2:" << solution.halves[1].asText();

                parts.removeAt(i--);
                parts.append(solution.halves[0]);
                parts.append(solution.halves[1]);
            }
        }
    }

//    if (parts.size() > 1)
//    {
//        qDebug("Polygon with %i points split to %i convex parts",
//               size(),
//               parts.size());
//    }

    DE_ASSERT(areAllConvex(parts));
    return parts;
}

}} // namespace gloom::geo
