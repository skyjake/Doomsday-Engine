/** @file mapbuild.cpp
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

#include "mapbuild.h"
#include "polygon.h"
#include "../geomath.h"

#include <array>

using namespace de;

namespace gloom {

internal::AttribSpec const MapVertex::_spec[5] =
{
    { internal::AttribSpec::Position,  3, GL_FLOAT,        false, sizeof(MapVertex), 0     },
    { internal::AttribSpec::Normal,    3, GL_FLOAT,        false, sizeof(MapVertex), 3 * 4 },
    { internal::AttribSpec::TexCoord0, 2, GL_FLOAT,        false, sizeof(MapVertex), 6 * 4 },
    { internal::AttribSpec::Texture,   1, GL_UNSIGNED_INT, false, sizeof(MapVertex), 8 * 4 },
    { internal::AttribSpec::Flags,     1, GL_FLOAT,        false, sizeof(MapVertex), 9 * 4 },
};
LIBGUI_VERTEX_FORMAT_SPEC(MapVertex, 10 * 4)

DENG2_PIMPL_NOREF(MapBuild)
{
    const Map &map;
    TextureIds textures;

    Impl(const Map &map, const TextureIds &textures)
        : map(map)
        , textures(textures)
    {}

    Point mapPoint(ID id) const
    {
        return map.points()[id];
    }

    Vector3f vertex(ID id) const
    {
        const Point &point = mapPoint(id);
        return Vector3f{float(point.x), 0.f, float(point.y)};
    }

    Vector3f lineSpan(const Line &line) const
    {
        return vertex(line.points[1]) - vertex(line.points[0]);
    }

    Vector3f projectPoint(ID pointId, const Plane &plane) const
    {
        return plane.projectPoint(mapPoint(pointId));
    }

    Vector3f normalVector(const Line &line) const
    {
        return geo::Line<Vector3f>(vertex(line.points[0]), vertex(line.points[1])).normal();
    }

    /**
     * Builds a mesh with triangles for all planes and walls.
     */
    Buffer *build()
    {
        Buffer *buf = new Buffer;
        Buffer::Vertices verts;
        Buffer::Indices indices;

//        using PlaneVerts = std::array<QHash<ID, Vector3f>, 2>;
//        auto projectPlanes = [this](ID secId) {
//            const Sector &sec = map.sector(secId);
//            const Volume *vols[2]{&map.volume(sec.volumes.first()),
//                                  &map.volume(sec.volumes.last())};
//            PlaneVerts planeVerts;
//            for (int p = 0; p < 2; ++p)
//            {
//                const Plane &plane = map.plane(vols[p]->planes[p]);
//                const auto poly = map.sectorPolygon(secId);
//                for (const auto &pp : poly.points)
//                {
//                    if (!planeVerts[p].contains(pp.id))
//                    {
//                        planeVerts[p].insert(pp.id, projectPoint(pp.id, plane));
//                    }
//                }
//            }
//            return planeVerts;
//        };
//        QHash<ID, PlaneVerts> sectorPlaneVerts;

        // Project each sector's points to their floor and ceiling planes.
        const auto sectorPlaneVerts = map.worldSectorPlaneVerts();

        for (const ID sectorId : map.sectors().keys())
        {
            const Sector &sector = map.sector(sectorId);

            // Split the polygon to convex parts (for triangulation).
            const auto convexParts = map.sectorPolygon(sectorId).splitConvexParts();

            // Each volume is built separately.
            //for (const ID volId : sector.volumes)
            {
                /*const Volume &volume = map.volume(volId);
                QHash<ID, Vector3f> planeVerts[2];
                for (int p = 0; p < 2; ++p)
                {
                    const Plane &plane = map.plane(volume.planes[p]);
                    for (const ID lineId : sector.lines)
                    {
                        const Line &line = map.line(lineId);
                        for (const ID pointId : line.points)
                        {
                            if (!planeVerts[p].contains(pointId))
                            {
                                planeVerts[p].insert(pointId, projectPoint(pointId, plane));
                            }
                        }
                    }
                }*/

                const auto &planeVerts = sectorPlaneVerts[sectorId];
                const auto &floor      = planeVerts.front();
                const auto &ceiling    = planeVerts.back();

                // Build the floor and ceiling of this volume.
                {

                    // Insert vertices of both planes to the buffer.
                    Buffer::Type f, c;
                    QHash<ID, Buffer::Index> pointIndices;

                    f.texture = textures["world.grass"];
                    f.normal  = Vector3f(0, 1, 0);
                    f.flags   = MapVertex::WorldSpaceXZToTexCoords;

                    c.texture = textures["world.dirt"];
                    c.normal  = Vector3f(0, -1, 0);
                    c.flags   = MapVertex::WorldSpaceXZToTexCoords;

                    for (const ID pointID : floor.keys())
                    {
                        f.pos = floor[pointID];
                        c.pos = ceiling[pointID];

                        f.texCoord = Vector2f(0, 0); // fixed offset
                        c.texCoord = Vector2f(0, 0); // fixed offset

                        pointIndices.insert(pointID, Buffer::Index(verts.size()));
                        verts << f << c;
                    }

                    for (const auto &convex : convexParts)
                    {
                        const ID baseID = convex.points[0].id;

                        // Floor.
                        for (int i = 1; i < convex.size() - 1; ++i)
                        {
                            indices << pointIndices[baseID]
                                    << pointIndices[convex.points[i + 1].id]
                                    << pointIndices[convex.points[i].id];
                        }

                        // Ceiling.
                        for (int i = 1; i < convex.size() - 1; ++i)
                        {
                            indices << pointIndices[baseID] + 1
                                    << pointIndices[convex.points[i].id] + 1
                                    << pointIndices[convex.points[i + 1].id] + 1;
                        }
                    }
                }

                auto makeQuad = [this, &indices, &verts](const String &  textureName,
                                                         const Vector3f &normal,
                                                         uint32_t        flags,
                                                         const Vector3f &p1,
                                                         const Vector3f &p2,
                                                         const Vector3f &p3,
                                                         const Vector3f &p4,
                                                         const Vector2f &uv1,
                                                         const Vector2f &uv2,
                                                         const Vector2f &uv3,
                                                         const Vector2f &uv4) {
                    const Buffer::Index baseIndex = Buffer::Index(verts.size());
                    indices << baseIndex
                            << baseIndex + 3
                            << baseIndex + 2
                            << baseIndex
                            << baseIndex + 1
                            << baseIndex + 3;

                    Buffer::Type v;

                    v.texture = textures[textureName];
                    v.normal  = normal;
                    v.flags   = flags;

                    v.pos = p1;
                    v.texCoord = uv1;
                    verts << v;

                    v.pos = p2;
                    v.texCoord = uv2;
                    verts << v;

                    v.pos = p3;
                    v.texCoord = uv3;
                    verts << v;

                    v.pos = p4;
                    v.texCoord = uv4;
                    verts << v;
                };

                // Build the walls.
                for (const ID lineId : sector.walls)
                {
                    const Line &line = map.line(lineId);

                    if (line.isSelfRef()) continue;

                    const int      dir    = line.sectors[0] == sectorId? 1 : 0;
                    const ID       start  = line.points[dir^1];
                    const ID       end    = line.points[dir];
                    const Vector3f normal = normalVector(line);
                    const float    length = float((floor[end] - floor[start]).length());
                    const float    heights[2] = {ceiling[start].y - floor[start].y,
                                                 ceiling[end].y   - floor[end].y};

                    if (!line.isTwoSided())
                    {
                        makeQuad("world.stone",
                                 normal,
                                 0,
                                 floor[start],
                                 floor[end],
                                 ceiling[start],
                                 ceiling[end],
                                 Vector2f(0, 0),
                                 Vector2f(length, 0),
                                 Vector2f(0, heights[0]),
                                 Vector2f(length, heights[1]));
                    }
                    else if (dir)
                    {
                        const ID    backSectorId   = line.sectors[dir];
                        const auto &backPlaneVerts = sectorPlaneVerts[backSectorId];

                        makeQuad("world.stone",
                                 normal,
                                 0,
                                 floor[start],
                                 floor[end],
                                 backPlaneVerts.front()[start],
                                 backPlaneVerts.front()[end],
                                 Vector2f(0, 0),
                                 Vector2f(length, 0),
                                 Vector2f(0, backPlaneVerts.front()[start].y - floor[start].y),
                                 Vector2f(length, backPlaneVerts.front()[end].y - floor[end].y));
                        makeQuad("world.stone",
                                 normal,
                                 0,
                                 backPlaneVerts.back()[start],
                                 backPlaneVerts.back()[end],
                                 ceiling[start],
                                 ceiling[end],
                                 Vector2f(0, 0),
                                 Vector2f(length, 0),
                                 Vector2f(0, ceiling[start].y - backPlaneVerts.back()[start].y),
                                 Vector2f(length, ceiling[end].y - backPlaneVerts.back()[end].y));
                    }

                    /* const Buffer::Index baseIndex = Buffer::Index(verts.size());
                    indices << baseIndex
                            << baseIndex + 3
                            << baseIndex + 2
                            << baseIndex
                            << baseIndex + 1
                            << baseIndex + 3;

                    Buffer::Type v;

                    v.texture = textures["world.dirt"];
                    v.normal  = normal;
                    v.flags   = 0;

                    v.pos = planeVerts[0][start];
                    v.texCoord = Vector2f(0, 0);
                    verts << v;

                    v.pos = planeVerts[0][end];
                    v.texCoord = Vector2f(length, 0);
                    verts << v;

                    v.pos = planeVerts[1][start];
                    v.texCoord = Vector2f(0, heights[0]);
                    verts << v;

                    v.pos = planeVerts[1][end];
                    v.texCoord = Vector2f(length, heights[1]);
                    verts << v;*/
                }
            }
        }

        buf->setVertices(verts, gl::Static);
        buf->setIndices(gl::Triangles, indices, gl::Static);

        DENG2_ASSERT(indices.size() % 3 == 0);

        qDebug() << "Built" << verts.size() << "vertices and" << indices.size() << "indices";

        return buf;
    }
};

MapBuild::MapBuild(const Map &map, const TextureIds &textures)
    : d(new Impl(map, textures))
{}

MapBuild::Buffer *MapBuild::build()
{
    return d->build();
}

} // namespace gloom
