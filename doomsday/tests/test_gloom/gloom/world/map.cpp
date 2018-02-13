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
    DENG2_ASSERT(!d->points.contains(0));
    DENG2_ASSERT(!d->planes.contains(0));
    DENG2_ASSERT(!d->lines.contains(0));
    DENG2_ASSERT(!d->sectors.contains(0));
    DENG2_ASSERT(!d->volumes.contains(0));

    // Lines.
    {
        for (QMutableHashIterator<ID, Line> iter(d->lines); iter.hasNext(); )
        {
            auto &line = iter.next().value();
            // Invalid sector references.
            for (auto &secId : line.sectors)
            {
                if (!d->sectors.contains(secId))
                {
                    secId = 0;
                }
            }
            // References to invalid points.
            if (!d->points.contains(line.points[0]) || !d->points.contains(line.points[1]))
            {
                iter.remove();
                continue;
            }
            // Degenerate lines.
            if (line.points[0] == line.points[1])
            {
                iter.remove();
                continue;
            }
            // Merge lines that share endpoints.
            for (ID id : d->lines.keys())
            {
                Line &other = Map::line(id);
                if (line.isOneSided() && other.isOneSided() && other.points[1] == line.points[0] &&
                    other.points[0] == line.points[1])
                {
                    other.sectors[1] = line.sectors[0];
                    // Sectors referencing the line must be updated.
                    for (Sector &sec : d->sectors)
                    {
                        sec.replaceLine(iter.key(), id);
                    }
                    iter.remove();
                    break;
                }
            }
        }
    }

    // Sectors.
    {
        for (QMutableHashIterator<ID, Sector> iter(d->sectors); iter.hasNext(); )
        {
            const ID secId = iter.key();
            auto &sector = iter.next().value();
            // Remove missing points.
            for (QMutableListIterator<ID> i(sector.points); i.hasNext(); )
            {
                i.next();
                if (!d->points.contains(i.value()))
                {
                    i.remove();
                }
            }
            // Remove missing lines.
            for (QMutableListIterator<ID> i(sector.walls); i.hasNext(); )
            {
                i.next();
                if (!d->lines.contains(i.value()))
                {
                    i.remove();
                }
                else
                {
                    // Missing references?
                    const Line &line = Map::line(i.value());
                    if (line.sectors[0] != secId && line.sectors[1] != secId)
                    {
                        i.remove();
                    }
                }
            }
            // Remove empty sectors.
            if (sector.points.isEmpty() || sector.walls.isEmpty())
            {
                iter.remove();
                continue;
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

Point &Map::point(ID id)
{
    DENG2_ASSERT(d->points.contains(id));
    return d->points[id];
}

Line &Map::line(ID id)
{
    DENG2_ASSERT(d->lines.contains(id));
    return d->lines[id];
}

Plane &Map::plane(ID id)
{
    DENG2_ASSERT(d->planes.contains(id));
    return d->planes[id];
}

Sector &Map::sector(ID id)
{
    DENG2_ASSERT(d->sectors.contains(id));
    return d->sectors[id];
}

Volume &Map::volume(ID id)
{
    DENG2_ASSERT(d->volumes.contains(id));
    return d->volumes[id];
}

const Point &Map::point(ID id) const
{
    DENG2_ASSERT(d->points.contains(id));
    return d->points[id];
}

const Line &Map::line(ID id) const
{
    DENG2_ASSERT(d->lines.contains(id));
    return d->lines[id];
}

const Plane &Map::plane(ID id) const
{
    DENG2_ASSERT(d->planes.contains(id));
    return d->planes[id];
}

const Sector &Map::sector(ID id) const
{
    DENG2_ASSERT(d->sectors.contains(id));
    return d->sectors[id];
}

const Volume &Map::volume(ID id) const
{
    DENG2_ASSERT(d->volumes.contains(id));
    return d->volumes[id];
}

bool Map::isLine(ID id) const
{
    return d->lines.contains(id);
}

void Map::forLinesAscendingDistance(const Point &pos, std::function<bool (ID)> func) const
{
    using DistLine = std::pair<ID, double>;
    QVector<DistLine> distLines;

    for (auto i = d->lines.begin(); i != d->lines.end(); ++i)
    {
        distLines << DistLine{i.key(), geoLine(i.key()).distanceTo(pos)};
    }

    qSort(distLines.begin(), distLines.end(), [](const DistLine &a, const DistLine &b) {
        return a.second < b.second;
    });

    for (const auto &dl : distLines)
    {
        if (!func(dl.first)) break;
    }
}

IDList Map::findLines(ID pointId) const
{
    IDList ids;
    for (auto i = d->lines.begin(); i != d->lines.end(); ++i)
    {
        if (i.value().points[0] == pointId || i.value().points[1] == pointId)
        {
            ids << i.key();
        }
    }
    return ids;
}

geo::Line2d Map::geoLine(ID lineId) const
{
    const auto &line = d->lines[lineId];
    return geo::Line2d{point(line.points[0]), point(line.points[1])};
}

geo::Polygon Map::sectorPolygon(ID sectorId) const
{
    // TODO: Store geo::Polygon in Sector; no need to rebuild it all the time.

    const auto &sec = sector(sectorId);
    //ID prevPointId = 0;
    //ID atPoint;

    geo::Polygon poly;
    for (ID pid : sec.points)
    {
        poly.points << geo::Polygon::Point{point(pid), pid};

        //const auto &line = Map::line(pid);
        //const int   dir  = (line.sectors[0] == sectorId ? 0 : 1);
        /*
        ID          pointId = line.points[idx];
        if (line.points[idx^1] == prevPointId)
        {
            pointId = line.points[idx^1];
        }
        poly.points << geo::Polygon::Point{d->points[pointId], pointId};
        prevPointId = pointId;
        */
        /*for (int idx : order[dir])
        {
            ID pointId = line.points[idx];
            if (prevPointId != pointId)
            {
                poly.points << geo::Polygon::Point{d->points[pointId], pointId};
            }
            prevPointId = pointId;
        }*/
    }
    poly.updateBounds();
    return poly;
}

void Map::buildSector(QSet<ID> sourceLines, IDList &sectorPoints, IDList &sectorWalls,
                      bool createNewSector)
{

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
            secObj.insert("pt", idListToJsonArray(i.value().points));
            secObj.insert("wl", idListToJsonArray(i.value().walls));
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
    const QJsonObject   json     = document.object();
    const auto          map      = json.toVariantHash();

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
            const auto obj     = i.value().toHash();
            const auto points  = obj["pt"].toList();
            const auto walls   = obj["wl"].toList();
            const auto volumes = obj["vol"].toList();
            d->sectors.insert(getId(i.key()),
                              Sector{variantListToIDList(points),
                                     variantListToIDList(walls),
                                     variantListToIDList(volumes)});
        }
    }

    // Volumes.
    {
        const auto volumes = map["volumes"].toHash();
        for (auto i = volumes.begin(); i != volumes.end(); ++i)
        {
            const auto obj = i.value().toHash();
            const IDList planes = variantListToIDList(obj["pln"].toList());
            d->volumes.insert(getId(i.key()), Volume{{planes[0], planes[1]}});
        }
    }

    removeInvalid();
}

void Sector::replaceLine(ID oldId, ID newId)
{
    for (int i = 0; i < walls.size(); ++i)
    {
        if (walls[i] == oldId) walls[i] = newId;
    }
}

} // namespace gloom
