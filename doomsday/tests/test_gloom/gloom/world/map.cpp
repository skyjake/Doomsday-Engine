/** @file map.cpp
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

#include "map.h"
#include "../geomath.h"

using namespace de;

namespace gloom {

DENG2_PIMPL(Map)
{
    ID      idGen{0};
    Points  points;
    Lines   lines;
    Planes  planes;
    Sectors sectors;
    Volumes volumes;

    Impl(Public *i) : Base(i)
    {}
};

Map::Map() : d(new Impl(this))
{
    /*
    {
        geo::Line<Vector2d> line(Vector2d(1, 1), Vector2d(1, 0));
        double t;
        if (line.intersect(geo::Line<Vector2d>(Vector2d(-2, 2), Vector2d(.9, 2)), t))
        {
            qDebug() << "Line(0,1) intersects at t =" << t << "at:"
                     << (line.start + line.span()*t).asText();
        }
        else
        {
            qDebug() << "Line(0,1) does not intersect";
        }
    }
*/
#if 0
    ID vols[2];
    {
        ID floor = append(planes(), Plane{{0, 0, 0}, {0, 1, 0}});
        ID ceil  = append(planes(), Plane{{0, 3, 0}, {0, -1, 0}});
        vols[0] = append(volumes(), Volume{{floor, ceil}});
    }
    {
        ID floor = append(planes(), Plane{{0, .25, 0}, {0, 1, 0}});
        ID ceil  = append(planes(), Plane{{0, 2.5, 0}, {0, -1, 0}});
        vols[1] = append(volumes(), Volume{{floor, ceil}});
    }

    ID rooms[2] = {
        append(sectors(), Sector{IDList(), IDList({vols[0]})}),
        append(sectors(), Sector{IDList(), IDList({vols[1]})})
    };

    ID doorwayID[2];
    IDList corners{{ append(points(), Point{-4, -4}),
                     append(points(), Point{ 4, -4}),
                     append(points(), Point{ 4,  4}),
                     append(points(), Point{ 0,  4}),
                     append(points(), Point{ 0,  8}),
                     append(points(), Point{-4,  8}),
                     doorwayID[0] = append(points(), Point(-4,  5)),
                     doorwayID[1] = append(points(), Point(-4,  3)),
                     append(points(), Point{ 8,  3}),
                     append(points(), Point{ 8,  5}),
                   }};

    ID openLine;
    sectors()[rooms[0]].lines =
        IDList{{append(lines(), Line{{corners[0], corners[1]}, {rooms[0], 0}}),
                append(lines(), Line{{corners[1], corners[2]}, {rooms[0], 0}}),
                append(lines(), Line{{corners[2], corners[3]}, {rooms[0], 0}}),
                append(lines(), Line{{corners[3], corners[4]}, {rooms[0], 0}}),
                append(lines(), Line{{corners[4], corners[5]}, {rooms[0], 0}}),
                append(lines(), Line{{corners[5], corners[6]}, {rooms[0], 0}}),
                openLine = append(lines(), Line{{corners[6], corners[7]}, {rooms[0], rooms[1]}}),
                append(lines(), Line{{corners[7], corners[0]}, {rooms[0], 0}})}};

    sectors()[rooms[1]].lines =
        IDList{{openLine,
                append(lines(), Line{{doorwayID[1], corners[8]}, {rooms[1], 0}}),
                append(lines(), Line{{corners[8], corners[9]}, {rooms[1], 0}}),
                append(lines(), Line{{corners[9], doorwayID[0]}, {rooms[1], 0}})}};
#endif
}

gloom::ID Map::newID()
{
    return ++d->idGen;
}

Points &Map::points()
{
    return d->points;
}

Lines &Map::lines()
{
    return d->lines;
}

Planes &Map::planes()
{
    return d->planes;
}

Sectors &Map::sectors()
{
    return d->sectors;
}

Volumes &Map::volumes()
{
    return d->volumes;
}

const Points &Map::points() const
{
    return d->points;
}

const Lines &Map::lines() const
{
    return d->lines;
}

const Planes &Map::planes() const
{
    return d->planes;
}

const Sectors &Map::sectors() const
{
    return d->sectors;
}

const Volumes &Map::volumes() const
{
    return d->volumes;
}

} // namespace gloom
