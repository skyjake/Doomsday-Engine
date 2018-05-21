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

#include <de/Rectangle>
#include <de/Vector>
#include <QHash>
#include <array>

#include "gloom/identity.h"
#include "gloom/geo/geomath.h"
#include "gloom/geo/polygon.h"
#include "gloom/world/entity.h"

namespace gloom {

using namespace de;

class Map;

struct Point
{
    Vec2d coord; // X and Z world coordinates
};

struct Line
{
    enum Side { Front = 0, Back = 1 };
    enum Section { Bottom = 0, Middle = 1, Top = 2 };

    ID points[2];
    struct Surface {
        ID     sector;
        String material[3]; // Bottom, Middle, Top
    } surfaces[2]; // front and back

    Line(const std::array<ID, 2> &     points   = {{0, 0}},
         const std::array<Surface, 2> &surfaces = {
             {Surface{0, {"", "", ""}}, Surface{0, {"", "", ""}}}})
        : points{points[0], points[1]}
        , surfaces{surfaces[0], surfaces[1]}
    {}

    ID startPoint(Side side) const { return points[side == Front ? 0 : 1]; }
    ID endPoint(Side side) const { return points[side == Front ? 1 : 0]; }
    std::array<ID, 2> sectors() const { return std::array<ID, 2>{{surfaces[0].sector, surfaces[1].sector}}; }

    bool isSelfRef() const { return surfaces[Front].sector == surfaces[Back].sector; }
    bool isOneSided() const { return !surfaces[Front].sector || !surfaces[Back].sector; }
    bool isTwoSided() const { return surfaces[Front].sector && surfaces[Back].sector; }
    Side sectorSide(ID sector) const { return surfaces[Front].sector == sector? Front : Back; }

    ID startPointForSector(ID sector) const { return startPoint(sectorSide(sector)); }
    ID endPointForSector(ID sector) const { return endPoint(sectorSide(sector)); }
};

struct Plane
{
    Vec3d point;
    Vec3f normal;
    String material[2]; // front and back

    geo::Plane toGeoPlane() const { return geo::Plane{point, normal}; }

    bool  isPointAbove(const Vec3d &pos) const;
    Vec3f tangent() const;

    /// Converts a 2D map point to a 3D world point.
    Vec3d projectPoint(const Point &pos) const;
};

struct Volume
{
    ID planes[2];
};

struct Sector
{
    IDList points;  // polygon, clockwise winding (zero ID used to separate disjoint polygons)
    IDList walls;   // unordered
    IDList volumes; // must be ascending and share planes; bottom plane of first volume is the
                    // sector floor, top plane of last volume is the sector ceiling
    void replaceLine(ID oldId, ID newId);
    ID   splitLine(ID lineId, Map &map);
};

struct Edge
{
    ID         line;
    Line::Side side;

    void flip();
    Edge flipped() const;
    bool operator==(const Edge &other) const;
};

uint qHash(const Edge &edge);

typedef QHash<ID, Point>  Points;
typedef QHash<ID, Line>   Lines;
typedef QHash<ID, Plane>  Planes;
typedef QHash<ID, Sector> Sectors;
typedef QHash<ID, Volume> Volumes;
typedef QHash<ID, std::shared_ptr<Entity>> Entities;

typedef QVector<geo::Polygon> Polygons;

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
    ID   newID();

    template <typename H, typename T>
    ID append(H &hash, const T& value)
    {
        const ID id = newID();
        hash.insert(id, value);
        return id;
    }

    Points &  points();
    Lines &   lines();
    Planes &  planes();
    Sectors & sectors();
    Volumes & volumes();
    Entities &entities();

    const Points &  points() const;
    const Lines &   lines() const;
    const Planes &  planes() const;
    const Sectors & sectors() const;
    const Volumes & volumes() const;
    const Entities &entities() const;

    Point & point(ID id);
    Line &  line(ID id);
    Plane & plane(ID id);
    Sector &sector(ID id);
    Volume &volume(ID id);
    Entity &entity(ID id);

    const Point & point(ID id) const;
    const Line &  line(ID id) const;
    const Plane & plane(ID id) const;
    const Sector &sector(ID id) const;
    const Volume &volume(ID id) const;
    const Entity &entity(ID id) const;

    Rectangled        bounds() const;
    StringList        materials() const;
    bool              isPoint(ID id) const;
    bool              isLine(ID id) const;
    bool              isPlane(ID id) const;
    void              forLinesAscendingDistance(const Point &pos, const std::function<bool(ID)> &) const;
    IDList            findLines(ID pointId) const;
    IDList            findLinesStartingFrom(ID pointId, Line::Side side) const;
    std::pair<ID, ID> findSectorAndVolumeAt(const Vec3d &pos) const;
    geo::Line2d       geoLine(ID lineId) const;
    geo::Line2d       geoLine(Edge ef) const;
    Polygons          sectorPolygons(ID sectorId) const;
    Polygons          sectorPolygons(const Sector &sector) const;
    ID                floorPlaneId(ID sectorId) const;
    ID                ceilingPlaneId(ID sectorId) const;
    const Plane &     floorPlane(ID sectorId) const;
    const Plane &     ceilingPlane(ID sectorId) const;

    using WorldVerts      = QHash<ID, Vec3f>;
    using WorldPlaneVerts = QList<WorldVerts>; // one set per plane

    WorldVerts      worldPlaneVerts(const Sector &sector, const Plane &plane) const;
    WorldPlaneVerts worldSectorPlaneVerts(const Sector &sector) const;
    QHash<ID, Map::WorldPlaneVerts> worldSectorPlaneVerts() const;

    bool buildSector(Edge         startSide,
                     IDList &     sectorPoints,
                     IDList &     sectorWalls,
                     QList<Edge> &sectorEdges);
    ID splitLine(ID lineId, const Point &splitPoint);

    Block serialize() const;
    void  deserialize(const Block &data);

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_MAP_H
