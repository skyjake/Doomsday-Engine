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

internal::AttribSpec const MapVertex::_spec[8] =
{
    { internal::AttribSpec::Position, 3, GL_FLOAT, false, sizeof(MapVertex),  0     },
    { internal::AttribSpec::Normal,   3, GL_FLOAT, false, sizeof(MapVertex),  3 * 4 },
    { internal::AttribSpec::TexCoord, 4, GL_FLOAT, false, sizeof(MapVertex),  6 * 4 },
    { internal::AttribSpec::Texture0, 1, GL_FLOAT, false, sizeof(MapVertex), 10 * 4 },
    { internal::AttribSpec::Texture1, 1, GL_FLOAT, false, sizeof(MapVertex), 11 * 4 },
    { internal::AttribSpec::Index0,   3, GL_FLOAT, false, sizeof(MapVertex), 12 * 4 },
    { internal::AttribSpec::Index1,   2, GL_FLOAT, false, sizeof(MapVertex), 15 * 4 },
    { internal::AttribSpec::Flags,    1, GL_FLOAT, false, sizeof(MapVertex), 17 * 4 },
};
LIBGUI_VERTEX_FORMAT_SPEC(MapVertex, 18 * 4)

DENG2_PIMPL_NOREF(MapBuild)
{
    const Map &map;
    TextureIds textures;
    Mapper     planeMapper;
    Mapper     texOffsetMapper;

    Impl(const Map &map, const TextureIds &textures)
        : map(map)
        , textures(textures)
    {}

    Vector3f vertex(ID id) const
    {
        const Point &point = map.point(id);
        return Vector3f{float(point.x), 0.f, float(point.y)};
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
        planeMapper.clear();
        texOffsetMapper.clear();

        Buffer *buf = new Buffer;
        Buffer::Vertices verts;
        Buffer::Indices indices;

        // Project each sector's points to their floor and ceiling planes.
        const auto sectorPlaneVerts = map.worldSectorPlaneVerts();

        // Assign indices to planes.
        for (const Sector &sector : map.sectors())
        {
            for (const ID vol : sector.volumes)
            {
                for (const ID plane : map.volume(vol).planes)
                {
                    planeMapper.insert(plane);
                    texOffsetMapper.insert(plane);
                }
            }
        }

        for (const ID sectorId : map.sectors().keys())
        {
            const Sector &sector = map.sector(sectorId);

            // Split the polygon to convex parts (for triangulation).
            const auto convexParts = map.sectorPolygon(sectorId).splitConvexParts();

            {
                const auto &planeVerts = sectorPlaneVerts[sectorId];
                const auto &floor      = planeVerts.front();
                const auto &ceiling    = planeVerts.back();

                // Build the floor and ceiling of this volume.
                {
                    // Insert vertices of both planes to the buffer.
                    Buffer::Type f{}, c{};
                    QHash<ID, Buffer::Index> pointIndices;

                    f.texture[0] = textures["world.test"]; // "world.grass"];
                    f.normal     = map.floorPlane(sectorId).normal;
                    f.flags      = MapVertex::WorldSpaceXZToTexCoords | MapVertex::FlipTexCoordY |
                                   MapVertex::TextureOffset;
                    f.geoPlane     = planeMapper[map.floorPlaneId(sectorId)];
                    f.texOffset[0] = texOffsetMapper[map.floorPlaneId(sectorId)];

                    c.texture[0]   = textures["world.test"]; //"world.dirt"];
                    c.normal       = map.ceilingPlane(sectorId).normal;
                    c.flags        = MapVertex::WorldSpaceXZToTexCoords | MapVertex::TextureOffset;
                    c.geoPlane     = planeMapper[map.ceilingPlaneId(sectorId)];
                    c.texOffset[0] = texOffsetMapper[map.ceilingPlaneId(sectorId)];

                    for (const ID pointID : floor.keys())
                    {
                        f.pos = floor[pointID];
                        c.pos = ceiling[pointID];

                        f.texCoord = Vector4f(0, 0, 0, 0); // fixed offset
                        c.texCoord = Vector4f(0, 0, 0, 0); // fixed offset

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

                auto makeQuad = [this, &indices, &verts](const String &  frontTextureName,
                                                         const String &  backTextureName,
                                                         const Vector3f &normal,
                                                         const uint32_t *planeIndex,
                                                         uint32_t        flags,
                                                         const Vector3f &p1,
                                                         const Vector3f &p2,
                                                         const Vector3f &p3,
                                                         const Vector3f &p4,
                                                         float           length,
                                                         float           rotation) {
                    const Buffer::Index baseIndex = Buffer::Index(verts.size());
                    indices << baseIndex
                            << baseIndex + 3
                            << baseIndex + 2
                            << baseIndex
                            << baseIndex + 1
                            << baseIndex + 3;

                    Buffer::Type v{};

                    v.texture[0]  = textures[frontTextureName];
                    v.texture[1]  = textures[backTextureName];
                    v.normal      = normal;
                    v.flags       = flags;
                    v.texPlane[0] = planeIndex[0];
                    v.texPlane[1] = planeIndex[1];

                    v.pos      = p1;
                    v.texCoord = Vector4f(0, 0, length, rotation);
                    v.geoPlane = planeIndex[0];
                    verts << v;

                    v.pos      = p2;
                    v.texCoord = Vector4f(length, 0, length, rotation);
                    v.geoPlane = planeIndex[0];
                    verts << v;

                    v.pos      = p3;
                    v.texCoord = Vector4f(0, 0, length, rotation);
                    v.geoPlane = planeIndex[1];
                    verts << v;

                    v.pos      = p4;
                    v.texCoord = Vector4f(length, 0, length, rotation);
                    v.geoPlane = planeIndex[1];
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
                    const uint32_t planeIndex[2] = {planeMapper[map.floorPlaneId(sectorId)],
                                                    planeMapper[map.ceilingPlaneId(sectorId)]};

                    if (!line.isTwoSided())
                    {
                        makeQuad("world.test",
                                 "world.test",
                                 normal,
                                 planeIndex,
                                 MapVertex::WorldSpaceYToTexCoord,
                                 floor[start],
                                 floor[end],
                                 ceiling[start],
                                 ceiling[end],
                                 length,
                                 0);
                    }
                    else if (dir)
                    {
                        const ID    backSectorId   = line.sectors[dir];
                        const auto &backPlaneVerts = sectorPlaneVerts[backSectorId];

                        const uint32_t botIndex[2] = {planeIndex[0],
                                                      planeMapper[map.floorPlaneId(backSectorId)]};
                        const uint32_t topIndex[2] = {planeMapper[map.ceilingPlaneId(backSectorId)],
                                                      planeIndex[1]};

                        makeQuad("world.test",
                                 "world.test2",
                                 normal,
                                 botIndex,
                                 MapVertex::WorldSpaceYToTexCoord | MapVertex::AnchorTopPlane,
                                 floor[start],
                                 floor[end],
                                 backPlaneVerts.front()[start],
                                 backPlaneVerts.front()[end],
                                 length,
                                 0);
                        makeQuad("world.test",
                                 "world.test2",
                                 normal,
                                 topIndex,
                                 MapVertex::WorldSpaceYToTexCoord,
                                 backPlaneVerts.back()[start],
                                 backPlaneVerts.back()[end],
                                 ceiling[start],
                                 ceiling[end],
                                 length,
                                 0);
                    }
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

const MapBuild::Mapper &MapBuild::planeMapper() const
{
    return d->planeMapper;
}

const MapBuild::Mapper &MapBuild::texOffsetMapper() const
{
    return d->texOffsetMapper;
}

} // namespace gloom
