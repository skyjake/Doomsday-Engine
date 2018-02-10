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

#include <de/Block>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

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

    Impl(Public *i, const Impl &other)
        : Base(i)
        , idGen(other.idGen)
        , points(other.points)
        , lines(other.lines)
        , planes(other.planes)
        , sectors(other.sectors)
        , volumes(other.volumes)
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

Map::Map(const Map &other)
    : d(new Impl(this, *other.d))
{}

Map &Map::operator=(const Map &other)
{
    *d = Impl(this, *other.d);
    return *this;
}

void Map::clear()
{
    *d = Impl(this);
}

void Map::removeInvalid()
{
    d->points.remove(0);

    // Check for lines that connect to missing points.
    {
        QMutableHashIterator<ID, Line> iter(d->lines);
        while (iter.hasNext())
        {
            iter.next();
            const auto &line = iter.value();
            if (!d->points.contains(line.points[0]) || !d->points.contains(line.points[1]))
            {
                iter.remove();
            }
            if (line.points[0] == line.points[1])
            {
                iter.remove();
            }
        }
    }
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

static QString idStr(ID id)
{
    return String::number(id, 16);
}

static ID idNum(QVariant str)
{
    return str.toString().toUInt(0, 16);
}

static QJsonArray idListToJsonArray(const IDList &list)
{
    QJsonArray array;
    for (auto id : list) array << idStr(id);
    return array;
}

static IDList variantListToIDList(const QList<QVariant> &list)
{
    IDList ids;
    for (const auto &var : list) ids << idNum(var);
    return ids;
}

Block Map::serialize() const
{
    const Impl *_d = d;
    QJsonObject obj;

    // Points.
    {
        QJsonObject points;
        for (auto i = _d->points.begin(); i != _d->points.end(); ++i)
        {
            points.insert(idStr(i.key()), QJsonArray{{i.value().x, i.value().y}});
        }
        obj.insert("points", points);
    }

    // Lines.
    {
        QJsonObject lines;
        for (auto i = _d->lines.begin(); i != _d->lines.end(); ++i)
        {
            QJsonObject lineObj;
            lineObj.insert("pt",
                           QJsonArray({idStr(i.value().points[0]), idStr(i.value().points[1])}));
            lineObj.insert("sec",
                           QJsonArray({idStr(i.value().sectors[0]), idStr(i.value().sectors[1])}));
            lines.insert(idStr(i.key()), lineObj);
        }
        obj.insert("lines", lines);
    }

    // Planes.
    {
        QJsonObject planes;
        for (auto i = _d->planes.begin(); i != _d->planes.end(); ++i)
        {
            const Plane &plane = i.value();
            planes.insert(idStr(i.key()),
                          QJsonArray({plane.point.x,
                                      plane.point.y,
                                      plane.point.z,
                                      plane.normal.x,
                                      plane.normal.y,
                                      plane.normal.z}));
        }
        obj.insert("planes", planes);
    }

    // Sectors.
    {
        QJsonObject sectors;
        for (auto i = _d->sectors.begin(); i != _d->sectors.end(); ++i)
        {
            QJsonObject secObj;
            secObj.insert("ln", idListToJsonArray(i.value().lines));
            secObj.insert("vol", idListToJsonArray(i.value().volumes));
            sectors.insert(idStr(i.key()), secObj);
        }
        obj.insert("sectors", sectors);
    }

    // Volumes.
    {
        QJsonObject volumes;
        for (auto i = _d->volumes.begin(); i != _d->volumes.end(); ++i)
        {
            const Volume &vol = i.value();
            QJsonObject volObj;
            volObj.insert("pln", QJsonArray({idStr(vol.planes[0]), idStr(vol.planes[1])}));
            volumes.insert(idStr(i.key()), volObj);
        }
        obj.insert("volumes", volumes);
    }

    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

void Map::deserialize(const Block &data)
{
    const QJsonDocument document = QJsonDocument::fromJson(data);
    const QJsonObject json = document.object();
    const auto map = json.toVariantHash();

    clear();

    auto getId = [this](QString idStr)
    {
        const ID id = idNum(idStr);
        if (id == 0) qWarning() << "[Map] Deserialized data contains ID 0";
        d->idGen = de::max(d->idGen, id);
        return id;
    };

    // Points.
    {
        const auto points = map["points"].toHash();
        for (auto i = points.begin(); i != points.end(); ++i)
        {
            const auto pos = i.value().toList();
            Point point(pos[0].toDouble(), pos[1].toDouble());
            d->points.insert(getId(i.key()), point);
        }
    }

    // Line.
    {
        const auto lines = map["lines"].toHash();
        for (auto i = lines.begin(); i != lines.end(); ++i)
        {
            const auto obj     = i.value().toHash();
            const auto points  = obj["pt"].toList();
            const auto sectors = obj["sec"].toList();
            d->lines.insert(getId(i.key()),
                            Line{{idNum(points[0]), idNum(points[1])},
                                 {idNum(sectors[0]), idNum(sectors[1])}});
        }
    }

    // Planes.
    {
        const auto planes = map["planes"].toHash();
        for (auto i = planes.begin(); i != planes.end(); ++i)
        {
            const auto obj = i.value().toList();
            Vector3d point{obj[0].toDouble(), obj[1].toDouble(), obj[2].toDouble()};
            Vector3f normal{obj[3].toFloat(), obj[4].toFloat(), obj[5].toFloat()};
            d->planes.insert(getId(i.key()), Plane{point, normal});
        }
    }

    // Sectors.
    {
        const auto sectors = map["sectors"].toHash();
        for (auto i = sectors.begin(); i != sectors.end(); ++i)
        {
            const auto obj = i.value().toHash();
            const auto lines = obj["ln"].toList();
            const auto volumes = obj["vol"].toList();
            d->sectors.insert(getId(i.key()),
                              Sector{variantListToIDList(lines), variantListToIDList(volumes)});
        }
    }

    // Volumes.
    {
        const auto volumes = map["volumes"].toHash();
        for (auto i = volumes.begin(); i != volumes.end(); ++i)
        {
            const auto obj = i.value().toHash();
            const auto planes = variantListToIDList(obj["pln"].toList());
            d->volumes.insert(getId(i.key()), Volume{{idNum(planes[0]), idNum(planes[1])}});
        }
    }

    removeInvalid();
}

} // namespace gloom
