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

#include "gloom/world/map.h"

#include <de/Block>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

using namespace de;

namespace gloom {

DENG2_PIMPL(Map)
{
    ID       idGen{0};
    Points   points;
    Lines    lines;
    Planes   planes;
    Sectors  sectors;
    Volumes  volumes;
    Entities entities;

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
        , entities(other.entities)
    {}
};

Map::Map() : d(new Impl(this))
{}

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
    DENG2_ASSERT(!d->entities.contains(0));

    // Lines.
    {
        for (QMutableHashIterator<ID, Line> iter(d->lines); iter.hasNext(); )
        {
            auto &line = iter.next().value();
            // Invalid sector references.
            for (auto secId : line.sectors())
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
                    other.surfaces[1].sector = line.surfaces[0].sector;
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
            iter.next();
            const ID secId = iter.key();
            auto &sector = iter.value();
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
                    if (line.surfaces[0].sector != secId && line.surfaces[1].sector != secId)
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

Entities &Map::entities()
{
    return d->entities;
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

const Entities &Map::entities() const
{
    return d->entities;
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

Entity &Map::entity(ID id)
{
    return *d->entities[id];
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

const Entity &Map::entity(ID id) const
{
    DENG2_ASSERT(d->entities.contains(id));
    return *d->entities[id];
}

Rectangled Map::bounds() const
{
    Rectangled rect;
    if (!d->points.isEmpty())
    {
        rect = Rectangled(d->points.values().begin()->coord, d->points.values().begin()->coord);
        for (const auto &p : d->points.values())
        {
            rect.include(p.coord);
        }
    }
    return rect;
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
        distLines << DistLine{i.key(), geoLine(i.key()).distanceTo(pos.coord)};
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

IDList Map::findLinesStartingFrom(ID pointId, Line::Side side) const
{
    IDList ids;
    for (auto i = d->lines.begin(); i != d->lines.end(); ++i)
    {
        if (i.value().startPoint(side) == pointId)
        {
            ids << i.key();
        }
    }
    return ids;
}

geo::Line2d Map::geoLine(ID lineId) const
{
    DENG2_ASSERT(d->lines.contains(lineId));
    const auto &line = d->lines[lineId];
    return geo::Line2d{point(line.points[0]).coord, point(line.points[1]).coord};
}

geo::Line2d Map::geoLine(Edge ref) const
{
    const Line &line = Map::line(ref.line);
    return geo::Line2d{point(line.startPoint(ref.side)).coord, point(line.endPoint(ref.side)).coord};
}

geo::Polygon Map::sectorPolygon(ID sectorId) const
{
    return sectorPolygon(sector(sectorId));
}

geo::Polygon Map::sectorPolygon(const Sector &sector) const
{
    // TODO: Store geo::Polygon in Sector; no need to rebuild it all the time.
    geo::Polygon poly;
    for (ID pid : sector.points)
    {
        poly.points << geo::Polygon::Point{point(pid).coord, pid};
    }
    poly.updateBounds();
    return poly;
}

ID Map::floorPlaneId(ID sectorId) const
{
    return volume(sector(sectorId).volumes.front()).planes[0];
}

ID Map::ceilingPlaneId(ID sectorId) const
{
    return volume(sector(sectorId).volumes.back()).planes[1];
}

const Plane &Map::floorPlane(ID sectorId) const
{
    return plane(floorPlaneId(sectorId));
}

const Plane &Map::ceilingPlane(ID sectorId) const
{
    return plane(ceilingPlaneId(sectorId));
}

Map::WorldVerts Map::worldPlaneVerts(const Sector &sector, const Plane &plane) const
{
    WorldVerts verts;
    const auto poly = sectorPolygon(sector);
    for (const auto &pp : poly.points)
    {
        if (!verts.contains(pp.id))
        {
            verts.insert(pp.id, plane.projectPoint(d->points[pp.id]));
        }
    }
    return verts;
}

Map::WorldPlaneVerts Map::worldSectorPlaneVerts(const Sector &sector) const
{
    const Impl *_d = d;
    WorldPlaneVerts planeVerts;
    for (const ID volId : sector.volumes)
    {
        const Volume &volume = _d->volumes[volId];
        if (planeVerts.isEmpty())
        {
            planeVerts << worldPlaneVerts(sector, _d->planes[volume.planes[0]]);
        }
        planeVerts << worldPlaneVerts(sector, _d->planes[volume.planes[1]]);
    }
    return planeVerts;
}

QHash<ID, Map::WorldPlaneVerts> Map::worldSectorPlaneVerts() const
{
    QHash<ID, WorldPlaneVerts> sectorPlaneVerts;
    for (auto i = d->sectors.constBegin(), end = d->sectors.constEnd(); i != end; ++i)
    {
        sectorPlaneVerts.insert(i.key(), worldSectorPlaneVerts(i.value()));
    }
    return sectorPlaneVerts;
}

bool Map::buildSector(Edge         startSide,
                      IDList &     sectorPoints,
                      IDList &     sectorWalls,
                      QList<Edge> &sectorEdges)
{
    QSet<Edge> assigned; // these have already been assigned to the sector
    QSet<ID>   assignedLines;

    sectorPoints.clear();

    //DENG2_ASSERT(sourceLines.contains(startSide.line));

    Edge at = startSide;
    for (;;)
    {
        Line atLine = line(at.line);

        sectorEdges << at;

//        qDebug("At line %X:%i (%X->%X)",
//               at.line, at.side, atLine.startPoint(at.side), atLine.endPoint(at.side));

        if (sectorPoints.isEmpty() || sectorPoints.back() != atLine.startPoint(at.side))
        {
            sectorPoints << atLine.startPoint(at.side);
        }
        if (sectorPoints.back() != atLine.endPoint(at.side))
        {
            sectorPoints << atLine.endPoint(at.side);
        }
        assigned << at;
        assignedLines.insert(at.line);

        if (sectorPoints.back() == sectorPoints.front())
        {
            // Closed polygon.
            break;
        }

        geo::Line2d atGeoLine = geoLine(at);

        struct Candidate {
            Edge line;
            double angle;
        };
        QList<Candidate> candidates;

        // Find potential line to continue to.
        // This may be the other side of a line already assigned.
        const ID conPoint = atLine.endPoint(at.side);
        for (ID connectedLineId : findLines(conPoint))
        {
            const Line &conLine = line(connectedLineId);

            if (connectedLineId == at.line) continue;

            if ((conPoint == conLine.points[at.side]     && conLine.surfaces[at.side].sector == 0) ||
                (conPoint == conLine.points[at.side ^ 1] && conLine.surfaces[at.side ^ 1].sector == 0))
            {
                Edge conSide{
                    connectedLineId,
                    Line::Side(conPoint == conLine.points[at.side] ? at.side : (at.side ^ 1))
                };
                if (!assigned.contains(conSide))
                {
                    const geo::Line2d nextLine = geoLine(conSide);
                    candidates << Candidate{conSide, atGeoLine.angle(nextLine)};
                }
            }
        }

        if (atLine.surfaces[0].sector == 0 && atLine.surfaces[1].sector == 0 &&
            candidates.isEmpty())
        {
            // We may be switch to the other side of the line.
            Edge otherSide = at.flipped();
            if (!assigned.contains(otherSide))
            {
                candidates << Candidate{otherSide, 180};
            }
        }

        if (candidates.isEmpty()) return false;

        // Which line forms the tightest angle?
        {
//            qDebug() << "Choosing from:";
//            for (const auto &cand : candidates)
//            {
//                qDebug("    line %X:%d, angle %lf",
//                       cand.line.line, cand.line.side, cand.angle);
//            }
            qSort(candidates.begin(),
                  candidates.end(),
                  [](const Candidate &a, const Candidate &b) {
                      return a.angle < b.angle;
                  });
            const auto &chosen = candidates.front();

            /*selection.insert(chosen.line);
            atPoint = chosen.nextPoint;
//                            qDebug(" - at line %X, added line %X, moving to point %X",
//                                   atLine, chosen.line, chosen.nextPoint);
            atLine = chosen.line;*/

            at = chosen.line;

            if (at == startSide) break;
        }
    }
    for (ID id : assignedLines)
    {
        sectorWalls << id;
    }
    return true;
}

ID Map::splitLine(ID lineId, const Point &splitPoint)
{
    const ID newPoint = append(points(), splitPoint);
    const ID newLine  = append(lines(), line(lineId));

    for (auto s = d->sectors.begin(), end = d->sectors.end(); s != end; ++s)
    {
        Sector &sector = s.value();

        for (int i = 0; i < sector.walls.size(); ++i)
        {
            if (sector.walls[i] == lineId)
            {
                sector.walls.insert(i + 1, newLine);

                const int side = line(lineId).sectorSide(s.key());

                // Find the corresponding corner points.
                for (int j = 0; j < sector.points.size(); ++j)
                {
                    if (line(lineId).points[side] == sector.points[j])
                    {
                        sector.points.insert(j + 1, newPoint);
                        break;
                    }
                }
                break;
            }
        }
    }

    line(lineId) .points[1] = newPoint;
    line(newLine).points[0] = newPoint;

    return newPoint;
}

std::pair<ID, ID> Map::findSectorAndVolumeAt(const de::Vec3d &pos) const
{
    for (ID sectorId : d->sectors.keys())
    {
        if (sectorPolygon(sectorId).isPointInside(pos.xz()))
        {
            // Which volume?
            const Sector &sector = d->sectors[sectorId];
            for (ID volumeId : sector.volumes)
            {
                const Plane &floor   = d->planes[d->volumes[volumeId].planes[0]];
                const Plane &ceiling = d->planes[d->volumes[volumeId].planes[1]];
                if (floor.isPointAbove(pos) && ceiling.isPointAbove(pos))
                {
                    return std::make_pair(sectorId, volumeId);
                }
            }
            return std::make_pair(sectorId, sector.volumes[0]);
        }
    }
    return std::make_pair(ID{0}, ID{0});
}

namespace util {

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

} // namespace util

Block Map::serialize() const
{
    using namespace util;

    const Impl *_d = d;
    QJsonObject obj;

    // Points.
    {
        QJsonObject points;
        for (auto i = _d->points.begin(); i != _d->points.end(); ++i)
        {
            points.insert(idStr(i.key()), QJsonArray{{i.value().coord.x, i.value().coord.y}});
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
                           QJsonArray({idStr(i.value().surfaces[0].sector),
                                       idStr(i.value().surfaces[1].sector)}));
            lineObj.insert("mtl",
                           QJsonArray({
                               i.value().surfaces[0].material[0],
                               i.value().surfaces[0].material[1],
                               i.value().surfaces[0].material[2],
                               i.value().surfaces[1].material[0],
                               i.value().surfaces[1].material[1],
                               i.value().surfaces[1].material[2]}));
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
                                      plane.normal.z,
                                      plane.material[0],
                                      plane.material[1]}));
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

    // Entities.
    {
        QJsonObject ents;
        for (auto i = _d->entities.begin(); i != _d->entities.end(); ++i)
        {
            const auto &ent = i.value();
            QJsonObject entObj;
            entObj.insert("pos", QJsonArray({ent->position().x, ent->position().y, ent->position().z}));
            entObj.insert("angle", ent->angle());
            entObj.insert("type", int(ent->type()));
            entObj.insert("scale", QJsonArray({ent->scale().x, ent->scale().y, ent->scale().z}));
            ents.insert(idStr(i.key()), entObj);
        }
        obj.insert("entities", ents);
    }

    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

void Map::deserialize(const Block &data)
{
    using namespace util;

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
            Point point{Vec2d{pos[0].toDouble(), pos[1].toDouble()}};
            d->points.insert(getId(i.key()), point);
        }
    }

    // Line.
    {
        const auto lines = map["lines"].toHash();
        for (auto i = lines.begin(); i != lines.end(); ++i)
        {
            const auto   obj       = i.value().toHash();
            const auto   points    = obj["pt"].toList();
            const auto   sectors   = obj["sec"].toList();
            QVariantList materials = obj["mtl"].toList();

            if (materials.isEmpty())
            {
                materials = QVariantList({"", "", "", "", "", ""});
            }

            Line::Surface frontSurface{idNum(sectors[0]),
                                       {materials.at(0).toString(),
                                        materials.at(1).toString(),
                                        materials.at(2).toString()}};
            Line::Surface backSurface{idNum(sectors[1]),
                                      {materials.at(3).toString(),
                                       materials.at(4).toString(),
                                       materials.at(5).toString()}};
            d->lines.insert(
                getId(i.key()),
                Line{{idNum(points[0]), idNum(points[1])}, {frontSurface, backSurface}});
        }
    }

    // Planes.
    {
        const auto planes = map["planes"].toHash();
        for (auto i = planes.begin(); i != planes.end(); ++i)
        {
            const auto obj = i.value().toList();
            Vec3d point{obj[0].toDouble(), obj[1].toDouble(), obj[2].toDouble()};
            Vec3f normal{obj[3].toFloat(), obj[4].toFloat(), obj[5].toFloat()};
            String material[2];
            if (obj.size() >= 8)
            {
                material[0] = obj[6].toString();
                material[1] = obj[7].toString();
            }
            d->planes.insert(getId(i.key()), Plane{point, normal, {material[0], material[1]}});
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

    // Entities.
    {
        const auto ents = map["entities"].toHash();
        for (auto i = ents.begin(); i != ents.end(); ++i)
        {
            const auto ent = i.value().toHash();
            const auto pos = ent["pos"].toList();
            const auto sc  = ent["scale"].toList();
            const ID   id  = getId(i.key());

            std::shared_ptr<Entity> entity(new Entity);
            entity->setId(id);
            entity->setType(Entity::Type(ent["type"].toInt()));
            entity->setPosition(Vec3d{pos[0].toDouble(), pos[1].toDouble(), pos[2].toDouble()});
            entity->setAngle(ent["angle"].toFloat());
            entity->setScale(Vec3f{sc[0].toFloat(), sc[1].toFloat(), sc[2].toFloat()});

            d->entities.insert(id, entity);
        }
    }

    removeInvalid();
}

//-------------------------------------------------------------------------------------------------

bool Plane::isPointAbove(const Vec3d &pos) const
{
    return geo::Plane{point, normal}.isPointAbove(pos);
}

Vec3d Plane::projectPoint(const Point &pos) const
{
    const auto geo = geo::Plane{point, normal};
    const double y = geo.project2D(pos.coord);
    return Vec3d(pos.coord.x, y, pos.coord.y);
}

Vec3f Plane::tangent() const
{
    Vec3f vec = normal.cross(Vec3f(0, 0, 1)).normalize();
    //return normal.cross(vec);
    return vec;
}

void Sector::replaceLine(ID oldId, ID newId)
{
    for (int i = 0; i < walls.size(); ++i)
    {
        if (walls[i] == oldId) walls[i] = newId;
    }
}

void Edge::flip()
{
    side = (side == Line::Front? Line::Back : Line::Front);
}

Edge Edge::flipped() const
{
    Edge ref = *this;
    ref.flip();
    return ref;
}

bool Edge::operator==(const Edge &other) const
{
    return line == other.line && side == other.side;
}

uint qHash(const Edge &sideRef)
{
    return (sideRef.line << 1) | int(sideRef.side);
}

} // namespace gloom
