/** @file geomath.h
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

#ifndef GLOOM_GEOMATH_H
#define GLOOM_GEOMATH_H

#include <de/Vector>
#include <QVector2D>

namespace gloom { namespace geo {

using namespace de;

inline Vec2d toVec2d(const QVector2D &vec)
{
    return Vec2d(vec.x(), vec.y());
}

inline QVector2D toQVector2D(const Vec2d &vec)
{
    return QVector2D(float(vec.x), float(vec.y));
}

/**
 * 2D line.
 */
template <typename T>
struct Line
{
    T start;
    T end;

    Line() {}

    Line(const T &span)
        : end(span)
    {}

    Line(const T &a, const T &b)
        : start(a)
        , end(b)
    {}

    double length() const
    {
        return span().length();
    }

    T span() const
    {
        return end - start;
    }

    T dir() const
    {
        return span().normalize();
    }

    T normal() const
    {
        const auto vec = dir();
        return T(-vec.y, vec.x);
    }

    void flip()
    {
        std::swap(start, end);
    }

    bool isFrontSide(const T &pos) const
    {
        return normal().dot(pos - start) > 0;
    }

    bool isBackSide(const T &pos) const
    {
        return normal().dot(pos - start) < 0;
    }

    int side(const T &pos) const
    {
        return isFrontSide(pos)? 0 : 1;
    }

    /**
     * Determines if this line intersects the line @a other.
     * The end point of @a other is excluded from the intersection.
     *
     * @param other  Other line.
     * @param t      Normalized intersection position on @em this line.
     *
     * @return @c true, if the lines intersect; otherwise @c false.
     */
    bool intersect(const Line &other, double &t) const
    {
        const auto &p = other.start;
        const auto &r = other.span();

        const auto &q = start;
        const auto  s = span();

        const double r_s = r.cross(s);
        if (abs(r_s) < EPSILON)
        {
            return false;
        }
        t = (q - p).cross(r) / r_s;

        // It has to hit somewhere on `other`.
        const double u = (q - p).cross(s) / r_s;
        return u >= 0 && u < 1;
    }

    /**
     * Determines the distance to the point @a p along the normal of the line.
     *
     * @param p  Any point.
     * @param t  Normalized distance along the line to the point nearest to @a p.
     *
     * @return  Distance along the line normal to the point.
     */
    double normalDistance(const T &p, double &t) const
    {
        const T delta = p - start;
        t = dir().dot(delta) / length();
        return abs(normal().dot(delta));
    }

    /**
     * Shortest distance from the line to the point @a p.
     *
     * @param p  Any point.
     * @param t
     * @return
     */
    double distanceTo(const T &p) const
    {
        const T delta = p - start;
        double dist = min(delta.length(), (p - end).length());
        double n = abs(normal().dot(delta));
        double t = dir().dot(delta);
        if (t >= 0 && t <= length())
        {
            return min(n, dist);
        }
        return dist;
    }

    double angle(const Line &other) const
    {
        // If the lines point to the same direction, the angle is 180 degrees.
        const T d1 = dir();
        const T n = normal();
        const T d2 = other.dir();
        double deg = 180 + radianToDegree(std::acos(clamp(-1.0, d1.dot(d2), 1.0)));
        if (n.dot(d2) > 0) deg = 360 - deg;
        return deg;
    }

    T nearestPoint(const T &p) const
    {
        return start + dir() * dir().dot(p - start);
    }
};

using Line2d = Line<Vec2d>;

struct Plane
{
    Vec3d point;
    Vec3f normal;

    bool   isPointAbove(const Vec3d &pos) const;
    double project2D(const Vec2d &posXZ) const;
    Vec3d  project(const Vec3d &pos) const;
    Vec3d  intersectRay(const Vec3d &pos, const Vec3f &rayDir) const;
};

}} // namespace gloom::geo

#endif // GLOOM_GEOMATH_H
