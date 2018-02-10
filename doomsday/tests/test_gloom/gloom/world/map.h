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
    ID sectors[2]; // front and back
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
    IDList lines; // must have clockwise winding
    IDList volumes; // must be ascending and share planes
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
    Map(const Map &);

    Map &operator=(const Map &);

    void clear();
    void removeInvalid();
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

    de::Block serialize() const;
    void deserialize(const de::Block &data);

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_MAP_H
