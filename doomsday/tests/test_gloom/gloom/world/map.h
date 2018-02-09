/** @file map.h
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

#ifndef GLOOM_MAP_H
#define GLOOM_MAP_H

#include <de/Vector>
#include <QHash>

namespace gloom {

typedef uint32_t     ID;
typedef QList<ID>    IDList;
typedef de::Vector2d Point;

struct Line
{
    ID points[2];
};
struct Plane
{
    de::Vector3d point;
    de::Vector3f normal;
};
struct Volume
{
    ID planes[2];
};
struct Sector
{
    IDList lines;
    IDList volumes;
};

typedef QHash<ID, Point>  Points;
typedef QHash<ID, Line>   Lines;
typedef QHash<ID, Plane>  Planes;
typedef QHash<ID, Sector> Sectors;
typedef QHash<ID, Volume> Volumes;

/**
 * Describes a map of polygon-based sectors.
 */
class Map
{
public:
    Map();

    ID newID();

    template <typename H, typename T>
    ID append(H &hash, const T& value)
    {
        const ID id = newID();
        hash.insert(id, value);
        return id;
    }

    Points & points();
    Lines &  lines();
    Planes & planes();
    Sectors &sectors();
    Volumes &volumes();

    const Points & points() const;
    const Lines &  lines() const;
    const Planes & planes() const;
    const Sectors &sectors() const;
    const Volumes &volumes() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_MAP_H
