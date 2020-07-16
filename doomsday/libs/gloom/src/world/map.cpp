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

#include <de/block.h>
#include <de/set.h>
#include <nlohmann/json.hpp>
#include <string>

namespace gloom {

using namespace de;
using json = nlohmann::json;

DE_PIMPL(Map)
{
    ID       idGen{0};
    Vec3d    metersPerUnit{1.0, 1.0, 1.0};
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
        , metersPerUnit(other.metersPerUnit)
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
    DE_ASSERT(!d->points.contains(0));
    DE_ASSERT(!d->planes.contains(0));
    DE_ASSERT(!d->lines.contains(0));
    DE_ASSERT(!d->sectors.contains(0));
    DE_ASSERT(!d->volumes.contains(0));
    DE_ASSERT(!d->entities.contains(0));

    // Lines.
    {
//        for (QMutableHashIterator<ID, Line> iter(d->lines); iter.hasNext(); )
        for (auto iter = d->lines.begin(); iter != d->lines.end(); )
        {
            auto &line = iter->second; //iter.next().value();
            // Invalid sector references.
            for (auto &surface : line.surfaces)
            {
                if (!d->sectors.contains(surface.sector))
                {
                    surface.sector = 0;
                }
            }
            // References to invalid points.
            if (!d->points.contains(line.points[0]) || !d->points.contains(line.points[1]))
            {
                //iter.remove();
                iter = d->lines.erase(iter);
                continue;
            }
            // Degenerate lines.
            if (line.points[0] == line.points[1])
            {
//                iter.remove();
                iter = d->lines.erase(iter);
                continue;
            }
            // Merge lines that share endpoints.
            bool erased = false;
            for (const auto &lineIter : d->lines)
            {
                const ID id = lineIter.first; //.key();
                if (id == iter->first) continue;

                Line &other = Map::line(id);
                if (line.isOneSided() && other.isOneSided() && other.points[1] == line.points[0] &&
                    other.points[0] == line.points[1])
                {
                    other.surfaces[1].sector = line.surfaces[0].sector;
                    // Sectors referencing the line must be updated.
                    for (auto &sec : d->sectors)
                    {
                        sec.second.replaceLine(iter->first, id);
                    }
                    iter = d->lines.erase(iter);
                    erased = true;
                    break;
                }
            }
            if (erased) continue;

            ++iter;
        }
    }

    // Sectors.
    {
//        for (QMutableHashIterator<ID, Sector> iter(d->sectors); iter.hasNext(); )
        for (auto iter = d->sectors.begin(); iter != d->sectors.end(); )
        {
            //            iter.next();
            const ID secId  = iter->first;
            auto &   sector = iter->second;
            // Remove missing points.
//            for (QMutableListIterator<ID> i(sector.points); i.hasNext(); )
            for (auto i = sector.points.begin(); i != sector.points.end(); )
            {
//                i.next();
                if (*i && !d->points.contains(*i)) // zero is a separator
                {
                    i = sector.points.erase(i);
                }
                else ++i;
            }
            // Remove missing lines.
            for (auto i = sector.walls.begin(); i != sector.walls.end(); )
            {
                if (!d->lines.contains(*i))
                {
                    i = sector.walls.erase(i);
                    continue;
                }
                else
                {
                    // Missing references?
                    const Line &line = Map::line(*i);
                    if (line.surfaces[0].sector != secId && line.surfaces[1].sector != secId)
                    {
                        i = sector.walls.erase(i);
                        continue;
                    }
                }
                ++i;
            }
            // Remove empty sectors.
            if (sector.points.isEmpty() || sector.walls.isEmpty())
            {
                iter = d->sectors.erase(iter);
                continue;
            }
            ++iter;
        }
    }
}

gloom::ID Map::newID()
{
    return ++d->idGen;
}

void Map::setMetersPerUnit(const Vec3d &metersPerUnit)
{
    d->metersPerUnit = metersPerUnit;
}

Vec3d Map::metersPerUnit() const
{
    return d->metersPerUnit;
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
    DE_ASSERT(id != 0);
    DE_ASSERT(d->points.contains(id));
    return d->points[id];
}

Line &Map::line(ID id)
{
    DE_ASSERT(id != 0);
    DE_ASSERT(d->lines.contains(id));
    return d->lines[id];
}

Plane &Map::plane(ID id)
{
    DE_ASSERT(id != 0);
    DE_ASSERT(d->planes.contains(id));
    return d->planes[id];
}

Sector &Map::sector(ID id)
{
    DE_ASSERT(id != 0);
    DE_ASSERT(d->sectors.contains(id));
    return d->sectors[id];
}

Volume &Map::volume(ID id)
{
    DE_ASSERT(id != 0);
    DE_ASSERT(d->volumes.contains(id));
    return d->volumes[id];
}

Entity &Map::entity(ID id)
{
    return *d->entities[id];
}

const Point &Map::point(ID id) const
{
    DE_ASSERT(id != 0);
    DE_ASSERT(d->points.contains(id));
    return d->points[id];
}

const Line &Map::line(ID id) const
{
    DE_ASSERT(id != 0);
    DE_ASSERT(d->lines.contains(id));
    return d->lines[id];
}

const Plane &Map::plane(ID id) const
{
    DE_ASSERT(id != 0);
    DE_ASSERT(d->planes.contains(id));
    return d->planes[id];
}

const Sector &Map::sector(ID id) const
{
    DE_ASSERT(id != 0);
    DE_ASSERT(d->sectors.contains(id));
    return d->sectors[id];
}

const Volume &Map::volume(ID id) const
{
    DE_ASSERT(id != 0);
    DE_ASSERT(d->volumes.contains(id));
    return d->volumes[id];
}

const Entity &Map::entity(ID id) const
{
    DE_ASSERT(id != 0);
    DE_ASSERT(d->entities.contains(id));
    return *d->entities[id];
}

Rectangled Map::bounds() const
{
    const auto *_d = d.getConst();
    Rectangled rect;
    if (!_d->points.isEmpty())
    {
        rect = Rectangled(_d->points.begin()->second.coord, _d->points.begin()->second.coord);
        for (const auto &p : _d->points)
        {
            rect.include(p.second.coord);
        }
    }
    return rect;
}

StringList Map::materials() const
{
    const auto *_d = d.getConst();
    Set<String> mats;
    for (const auto &line : _d->lines)
    {
        for (const auto &surf : line.second.surfaces)
        {
            for (const auto &name : surf.material)
            {
                if (name) mats.insert(name);
            }
        }
    }
    for (const auto &plane : _d->planes)
    {
        for (const auto &name : plane.second.material)
        {
            if (name) mats.insert(name);
        }
    }
    return compose<StringList>(mats.begin(), mats.end());
}

bool Map::isLine(ID id) const
{
    return d->lines.contains(id);
}

bool Map::isPlane(ID id) const
{
    return d->planes.contains(id);
}

void Map::forLinesAscendingDistance(const Point &pos, const std::function<bool (ID)> &func) const
{
    using DistLine = std::pair<ID, double>;
    List<DistLine> distLines;

    for (auto i = d->lines.begin(); i != d->lines.end(); ++i)
    {
        distLines << DistLine{i->first, geoLine(i->first).distanceTo(pos.coord)};
    }

    std::sort(distLines.begin(), distLines.end(), [](const DistLine &a, const DistLine &b) {
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
        if (i->second.points[0] == pointId || i->second.points[1] == pointId)
        {
            ids << i->first;
        }
    }
    return ids;
}

IDList Map::findLinesStartingFrom(ID pointId, Line::Side side) const
{
    IDList ids;
    for (auto i = d->lines.begin(); i != d->lines.end(); ++i)
    {
        if (i->second.startPoint(side) == pointId)
        {
            ids << i->first;
        }
    }
    return ids;
}

geo::Line2d Map::geoLine(ID lineId) const
{
    DE_ASSERT(d->lines.contains(lineId));
    const auto &line = d->lines[lineId];
    return geo::Line2d{point(line.points[0]).coord, point(line.points[1]).coord};
}

geo::Line2d Map::geoLine(Edge ref) const
{
    const Line &line = Map::line(ref.line);
    return geo::Line2d{point(line.startPoint(ref.side)).coord,
                       point(line.endPoint(ref.side)).coord};
}

Polygons Map::sectorPolygons(ID sectorId) const
{
    return sectorPolygons(sector(sectorId));
}

Polygons Map::sectorPolygons(const Sector &sector) const
{
    Polygons polys;
    geo::Polygon poly;
    for (ID pid : sector.points)
    {
        if (pid)
        {
            poly.points << geo::Polygon::Point{point(pid).coord, pid};
        }
        else
        {
            polys << poly;
            poly.clear();
        }
    }
    if (poly.size())
    {
        polys << poly;
    }
    for (auto &p : polys) p.updateBounds();
    return polys;
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
    for (const auto &poly : sectorPolygons(sector))
    {
        for (const auto &pp : poly.points)
        {
            if (!verts.contains(pp.id))
            {
                verts.insert(pp.id, plane.projectPoint(point(pp.id)));
            }
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

Hash<ID, Map::WorldPlaneVerts> Map::worldSectorPlaneVerts() const
{
    Hash<ID, WorldPlaneVerts> sectorPlaneVerts;
    for (const auto &i : d->sectors)
    {
        sectorPlaneVerts.insert(i.first, worldSectorPlaneVerts(i.second));
    }
    return sectorPlaneVerts;
}

bool Map::buildSector(Edge        startSide,
                      IDList &    sectorPoints,
                      IDList &    sectorWalls,
                      List<Edge> &sectorEdges)
{
    Set<Edge> assigned; // these have already been assigned to the sector
    Set<ID>   assignedLines;

    sectorPoints.clear();

    //DE_ASSERT(sourceLines.contains(startSide.line));

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
        List<Candidate> candidates;

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

            std::sort(candidates.begin(),
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
        Sector &sector = s->second;

        for (dsize i = 0; i < sector.walls.size(); ++i)
        {
            if (sector.walls[i] == lineId)
            {
                sector.walls.insert(i + 1, newLine);

                const int side = line(lineId).sectorSide(s->first);

                // Find the corresponding corner points.
                for (dsize j = 0; j < sector.points.size(); ++j)
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
    for (const auto &i : d->sectors)
    {
        const ID sectorId = i.first;
        for (const auto &poly : sectorPolygons(sectorId))
        {
            if (poly.isPointInside(pos.xz()))
            {
                // Which volume?
                const Sector &sector = i.second;
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
    }
    return std::make_pair(ID{0}, ID{0});
}

namespace util {

static std::string idStr(ID id)
{
    return stringf("%x", id);
}

static ID idNum(const std::string &str)
{
    return ID(std::stoul(str, nullptr, 16));
}

static json idListToJsonArray(const IDList &list)
{
    json array = json::array();
    for (auto id : list) array.push_back(idStr(id));
    return array;
}

static IDList jsonArrayToIDList(const json &list)
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
    json obj;

    // Metadata.
    {
        obj["metersPerUnit"] = json::array({_d->metersPerUnit.x, _d->metersPerUnit.y, _d->metersPerUnit.z});
    }

    // Points.
    {
        json points;
        for (auto i = _d->points.begin(); i != _d->points.end(); ++i)
        {
            points[idStr(i->first)] = json::array({i->second.coord.x, i->second.coord.y});
        }
        obj["points"] = points;
    }

    // Lines.
    {
        json lines;
        for (auto i = _d->lines.begin(); i != _d->lines.end(); ++i)
        {
            json lineObj;
            lineObj["pt"] = json::array({idStr(i->second.points[0]), idStr(i->second.points[1])});
            lineObj["sec"] = json::array({idStr(i->second.surfaces[0].sector),
                                          idStr(i->second.surfaces[1].sector)});
            lineObj["mtl"] = json::array({
                               i->second.surfaces[0].material[0],
                               i->second.surfaces[0].material[1],
                               i->second.surfaces[0].material[2],
                               i->second.surfaces[1].material[0],
                               i->second.surfaces[1].material[1],
                               i->second.surfaces[1].material[2]});
            lines[idStr(i->first)] = lineObj;
        }
        obj["lines"] = lines;
    }

    // Planes.
    {
        json planes;
        for (auto i = _d->planes.begin(); i != _d->planes.end(); ++i)
        {
            const Plane &plane = i->second;
            planes[idStr(i->first)] =
                          json::array({plane.point.x,
                                       plane.point.y,
                                       plane.point.z,
                                       plane.normal.x,
                                       plane.normal.y,
                                       plane.normal.z,
                                       plane.material[0],
                                       plane.material[1]});
        }
        obj["planes"] = planes;
    }

    // Sectors.
    {
        json sectors;
        for (auto i = _d->sectors.begin(); i != _d->sectors.end(); ++i)
        {
            json secObj;
            secObj["pt"]  = idListToJsonArray(i->second.points);
            secObj["wl"]  = idListToJsonArray(i->second.walls);
            secObj["vol"] = idListToJsonArray(i->second.volumes);
            sectors[idStr(i->first)] = secObj;
        }
        obj["sectors"] = sectors;
    }

    // Volumes.
    {
        json volumes;
        for (auto i = _d->volumes.begin(); i != _d->volumes.end(); ++i)
        {
            const Volume &vol = i->second;
            json volObj;
            volObj["pln"] = json::array({idStr(vol.planes[0]), idStr(vol.planes[1])});
            volumes[idStr(i->first)] = volObj;
        }
        obj["volumes"] = volumes;
    }

    // Entities.
    {
        json ents;
        for (auto i = _d->entities.begin(); i != _d->entities.end(); ++i)
        {
            const auto &ent = i->second;
            json entObj;
            entObj["pos"] = json::array({ent->position().x, ent->position().y, ent->position().z});
            entObj["angle"] = ent->angle();
            entObj["type"] = int(ent->type());
            entObj["scale"] = json::array({ent->scale().x, ent->scale().y, ent->scale().z});
            ents[idStr(i->first)] = entObj;
        }
        obj["entities"] = ents;
    }

    return obj.dump();
}

void Map::deserialize(const Block &data)
{
    using namespace util;

    const json map = json::parse(data.c_str());

    clear();

    auto getId = [this](String idStr)
    {
        const ID id = idNum(idStr);
        if (id == 0) warning("[Map] Deserialized data contains ID 0");
        d->idGen = de::max(d->idGen, id);
        return id;
    };

    // Metadata.
    {
        if (map.find("metersPerUnit") != map.end())
        {
            const auto mpu = map["metersPerUnit"];
            d->metersPerUnit = Vec3d{mpu[0], mpu[1], mpu[2]};
        }
    }

    // Points.
    {
        const auto &points = map["points"];
        for (auto i = points.begin(); i != points.end(); ++i)
        {
            const auto &pos = i.value();
            Point point{Vec2d{pos[0], pos[1]}};
            d->points.insert(getId(i.key()), point);
        }
    }

    // Line.
    {
        const json &lines = map["lines"];
        for (auto i = lines.begin(); i != lines.end(); ++i)
        {
            const auto &obj       = i.value();
            const auto &points    = obj["pt"];
            const auto &sectors   = obj["sec"];
            json        materials = obj["mtl"];

            if (materials.empty())
            {
                materials = json::array({"", "", "", "", "", ""});
            }

            Line::Surface frontSurface{idNum(sectors[0]),
                                       {materials[0].get<std::string>(),
                                        materials[1].get<std::string>(),
                                        materials[2].get<std::string>()}};
            Line::Surface backSurface{idNum(sectors[1]),
                                      {materials[3].get<std::string>(),
                                       materials[4].get<std::string>(),
                                       materials[5].get<std::string>()}};
            d->lines.insert(
                getId(i.key()),
                Line{{{idNum(points[0]), idNum(points[1])}}, {{frontSurface, backSurface}}});
        }
    }

    // Planes.
    {
        const auto &planes = map["planes"];
        for (auto i = planes.begin(); i != planes.end(); ++i)
        {
            const auto &obj = i.value();
            Vec3d point{obj[0], obj[1], obj[2]};
            Vec3f normal{obj[3], obj[4], obj[5]};
            String material[2];
            if (obj.size() >= 8)
            {
                material[0] = obj[6].get<std::string>();
                material[1] = obj[7].get<std::string>();
            }
            d->planes.insert(getId(i.key()), Plane{point, normal, {material[0], material[1]}});
        }
    }

    // Sectors.
    {
        const auto &sectors = map["sectors"];
        for (auto i = sectors.begin(); i != sectors.end(); ++i)
        {
            const auto &obj     = i.value();
            const auto &points  = obj["pt"];
            const auto &walls   = obj["wl"];
            const auto &volumes = obj["vol"];
            d->sectors.insert(getId(i.key()),
                              Sector{jsonArrayToIDList(points),
                                     jsonArrayToIDList(walls),
                                     jsonArrayToIDList(volumes)});
        }
    }

    // Volumes.
    {
        const auto &volumes = map["volumes"];
        for (auto i = volumes.begin(); i != volumes.end(); ++i)
        {
            const auto &obj = i.value();
            const IDList planes = jsonArrayToIDList(obj["pln"]);
            d->volumes.insert(getId(i.key()), Volume{{planes[0], planes[1]}});
        }
    }

    // Entities.
    {
        const auto &ents = map["entities"];
        for (auto i = ents.begin(); i != ents.end(); ++i)
        {
            const auto &ent = i.value();
            const auto &pos = ent["pos"];
            const auto &sc  = ent["scale"];
            const ID   id   = getId(i.key());

            std::shared_ptr<Entity> entity(new Entity);
            entity->setId(id);
            entity->setType(Entity::Type(ent["type"]));
            entity->setPosition(Vec3d{pos[0], pos[1], pos[2]});
            entity->setAngle(ent["angle"]);
            entity->setScale(Vec3f{sc[0], sc[1], sc[2]});

            d->entities.insert(id, entity);
        }
    }

    removeInvalid();
}

bool Map::isPoint(ID id) const
{
    return d->points.contains(id);
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
    for (dsize i = 0; i < walls.size(); ++i)
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

} // namespace gloom
