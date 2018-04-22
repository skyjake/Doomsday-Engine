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

#include "gloom/render/mapbuild.h"
#include "gloom/geo/polygon.h"
#include "gloom/geo/geomath.h"
#include "gloom/render/defs.h"

#include <array>

using namespace de;

namespace gloom {

internal::AttribSpec const MapVertex::_spec[10] =
{
    { internal::AttribSpec::Position,  3, GL_FLOAT, false, sizeof(MapVertex),  0     },
    { internal::AttribSpec::Normal,    3, GL_FLOAT, false, sizeof(MapVertex),  3 * 4 },
    { internal::AttribSpec::Tangent,   3, GL_FLOAT, false, sizeof(MapVertex),  6 * 4 },
    { internal::AttribSpec::TexCoord,  4, GL_FLOAT, false, sizeof(MapVertex),  9 * 4 },
    { internal::AttribSpec::Direction, 2, GL_FLOAT, false, sizeof(MapVertex), 13 * 4 },
    { internal::AttribSpec::Texture0,  1, GL_FLOAT, false, sizeof(MapVertex), 15 * 4 },
    { internal::AttribSpec::Texture1,  1, GL_FLOAT, false, sizeof(MapVertex), 16 * 4 },
    { internal::AttribSpec::Index0,    3, GL_FLOAT, false, sizeof(MapVertex), 17 * 4 },
    { internal::AttribSpec::Index1,    2, GL_FLOAT, false, sizeof(MapVertex), 20 * 4 },
    { internal::AttribSpec::Flags,     1, GL_FLOAT, false, sizeof(MapVertex), 22 * 4 },
};
LIBGUI_VERTEX_FORMAT_SPEC(MapVertex, 23 * 4)

DENG2_PIMPL_NOREF(MapBuild)
{
    const Map &        map;
    const MaterialLib &matLib;
    Mapper             planeMapper;
    Mapper             texOffsetMapper;

    Impl(const Map &map, const MaterialLib &materials)
        : map(map)
        , matLib(materials)
    {}

    Vec3f worldNormalVector(const Line &line) const
    {
        const Vec2d norm =
            geo::Line2d(map.point(line.points[0]).coord, map.point(line.points[1]).coord).normal();
        return Vec3d(norm.x, 0.0, norm.y);
    }

    /**
     * Builds a mesh with triangles for all planes and walls.
     */
    Buffers build()
    {
        Buffers bufs;

        planeMapper.clear();
        texOffsetMapper.clear();

        for (auto &buf : bufs.geom)
        {
            buf.reset(new Buffer);
        }

        Buffer::Vertices verts[2];
        Buffer::Indices indices[2];

        // Project each sector's points to all their planes.
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

        foreach (const ID sectorId, map.sectors().keys())
        {
            const Sector &sector = map.sector(sectorId);

            // Split the polygon to convex parts (for triangulation).
            const auto sectorPolygon = map.sectorPolygon(sectorId);
            const auto expanders     = sectorPolygon.expanders();
            const auto convexParts   = sectorPolygon.splitConvexParts();

            // -------- Planes --------

            const auto &planeVerts        = sectorPlaneVerts[sectorId];
            const auto &floor             = planeVerts.front();
            const auto &ceiling           = planeVerts.back();
            auto        currentPlaneVerts = planeVerts.begin();
            const auto  floorId           = map.floorPlaneId(sectorId);
            const auto  ceilingId         = map.ceilingPlaneId(sectorId);

            for (int v = 0; v < sector.volumes.size(); ++v)
            {
                const ID      volumeId = sector.volumes[v];
                const Volume &volume   = map.volume(volumeId);

                for (int i = 0; i < 2; ++i)
                {
                    if (i == 1 && v < sector.volumes.size() - 1) break;

                    const auto &currentVerts = *currentPlaneVerts++;
                    const auto &plane        = map.plane(volume.planes[i]);

                    if (!plane.material[0] && !plane.material[1])
                    {
                        continue;
                    }

                    const bool  isFacingUp = plane.normal.y > 0;
                    const int   geomBuf    = matLib.isTransparent(plane.material[0]);
                    const dsize firstIndex = indices[geomBuf].size();

                    // Insert vertices of both planes to the buffer.
                    Buffer::Type f{};
                    QHash<ID, Buffer::Index> pointIndices;

                    f.material[0]  = matLib.materials()[plane.material[0]];
                    f.material[1]  = matLib.materials()[plane.material[1]];
                    f.normal       = plane.normal;
                    f.tangent      = plane.tangent();
                    f.flags        = MapVertex::WorldSpaceXZToTexCoords | MapVertex::TextureOffset;
                    f.geoPlane     = planeMapper[volume.planes[i]];
                    f.texPlane[0]  = planeMapper[floorId];
                    f.texPlane[1]  = planeMapper[ceilingId];
                    f.texOffset[0] = texOffsetMapper[volume.planes[i]];

                    if (isFacingUp)
                    {
                        f.flags |= MapVertex::FlipTexCoordY;
                    }
                    else
                    {
                        f.tangent = -f.tangent;
                    }

                    foreach (const ID pointID, currentVerts.keys())
                    {
                        f.pos      = currentVerts[pointID];
                        f.texCoord = Vec4f(0, 0, 0, 0); // fixed offset
                        f.expander = expanders[pointID];

                        pointIndices.insert(pointID, Buffer::Index(verts[geomBuf].size()));
                        verts[geomBuf] << f;
                    }

                    for (const auto &convex : convexParts)
                    {
                        const ID baseID = convex.points[0].id;

                        // Floor.
                        if (isFacingUp)
                        {
                            for (int i = 1; i < convex.size() - 1; ++i)
                            {
                                indices[geomBuf] << pointIndices[baseID]
                                                 << pointIndices[convex.points[i + 1].id]
                                                 << pointIndices[convex.points[i].id];
                            }
                        }
                        else
                        {
                            for (int i = 1; i < convex.size() - 1; ++i)
                            {
                                indices[geomBuf] << pointIndices[baseID]
                                                 << pointIndices[convex.points[i].id]
                                                 << pointIndices[convex.points[i + 1].id];
                            }
                        }
                    }

                    if (geomBuf == TransparentGeometry)
                    {
                        bufs.transparencies << Buffers::Transparency{{
                            plane.projectPoint(Point{sectorPolygon.center()}), plane.normal}};
                        bufs.transparentRanges.append(
                            Rangez(firstIndex, indices[geomBuf].size()));
                    }
                }
            }

            // -------- Walls --------

            auto makeQuad = [this, &bufs, &indices, &verts](const String &  frontMaterial,
                                                            const String &  backMaterial,
                                                            const Vec3f &   normal,
                                                            const Vec2f &   expander1,
                                                            const Vec2f &   expander2,
                                                            const uint32_t *planeIndex,
                                                            uint32_t        flags,
                                                            const Vec3f &   p1,
                                                            const Vec3f &   p2,
                                                            const Vec3f &   p3,
                                                            const Vec3f &   p4,
                                                            float           length,
                                                            float           rotation) {
                if (!frontMaterial && !backMaterial) return;

                const int geomBuf = matLib.isTransparent(frontMaterial);

                const dsize firstIndex = indices[geomBuf].size();
                const Buffer::Index baseIndex = Buffer::Index(verts[geomBuf].size());
                indices[geomBuf] << baseIndex
                                 << baseIndex + 3
                                 << baseIndex + 2
                                 << baseIndex
                                 << baseIndex + 1
                                 << baseIndex + 3;

                Buffer::Type v{};

                v.material[0] = matLib.materials()[frontMaterial];
                v.material[1] = matLib.materials()[backMaterial];
                v.normal      = normal;
                v.tangent     = (p2 - p1).normalize();
                v.texPlane[0] = planeIndex[0];
                v.texPlane[1] = planeIndex[1];

                v.pos      = p1;
                v.texCoord = Vec4f(0, 0, length, rotation);
                v.geoPlane = planeIndex[0];
                v.expander = expander1;
                v.flags    = flags | MapVertex::LeftEdge;
                verts[geomBuf] << v;

                v.pos      = p2;
                v.texCoord = Vec4f(length, 0, length, rotation);
                v.geoPlane = planeIndex[0];
                v.expander = expander2;
                v.flags    = flags | MapVertex::RightEdge;
                verts[geomBuf] << v;

                v.pos      = p3;
                v.texCoord = Vec4f(0, 0, length, rotation);
                v.geoPlane = planeIndex[1];
                v.expander = expander1;
                v.flags    = flags | MapVertex::LeftEdge;
                verts[geomBuf] << v;

                v.pos      = p4;
                v.texCoord = Vec4f(length, 0, length, rotation);
                v.geoPlane = planeIndex[1];
                v.expander = expander2;
                v.flags    = flags | MapVertex::RightEdge;
                verts[geomBuf] << v;

                if (geomBuf == TransparentGeometry)
                {
                    bufs.transparencies << Buffers::Transparency{geo::Plane{p1, normal}};
                    bufs.transparentRanges.append(
                        Rangez(firstIndex, indices[geomBuf].size()));
                }
            };

            for (const ID lineId : sector.walls)
            {
                const Line &line = map.line(lineId);

                if (line.isSelfRef()) continue;

                const int      dir    = line.surfaces[0].sector == sectorId? 1 : 0;
                const ID       start  = line.points[dir^1];
                const ID       end    = line.points[dir];
                const Vec3f    normal = worldNormalVector(line);
                const float    length = float((floor[end] - floor[start]).length());
                const uint32_t planeIndex[2] = {planeMapper[map.floorPlaneId(sectorId)],
                                                planeMapper[map.ceilingPlaneId(sectorId)]};

                //if (!line.isTwoSided())
                {
                    makeQuad(line.surfaces[Line::Front].material[Line::Middle],
                             line.surfaces[Line::Back ].material[Line::Middle],
                             normal,
                             expanders[start],
                             expanders[end],
                             planeIndex,
                             MapVertex::WorldSpaceYToTexCoord,
                             floor[start],
                             floor[end],
                             ceiling[start],
                             ceiling[end],
                             length,
                             0);
                }

                if (line.isTwoSided() && dir)
                {
                    const ID    backSectorId   = line.sectors()[dir];
                    const auto &backPlaneVerts = sectorPlaneVerts[backSectorId];

                    const uint32_t botIndex[2] = {planeIndex[0],
                                                  planeMapper[map.floorPlaneId(backSectorId)]};
                    const uint32_t topIndex[2] = {planeMapper[map.ceilingPlaneId(backSectorId)],
                                                  planeIndex[1]};

                    makeQuad(line.surfaces[Line::Front].material[Line::Bottom],
                             line.surfaces[Line::Back ].material[Line::Bottom],
                             normal,
                             expanders[start],
                             expanders[end],
                             botIndex,
                             MapVertex::WorldSpaceYToTexCoord | MapVertex::AnchorTopPlane,
                             floor[start],
                             floor[end],
                             backPlaneVerts.front()[start],
                             backPlaneVerts.front()[end],
                             length,
                             0);

                    makeQuad(line.surfaces[Line::Front].material[Line::Top],
                             line.surfaces[Line::Back ].material[Line::Top],
                             normal,
                             expanders[start],
                             expanders[end],
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

        for (int i = 0; i < BufferCount; ++i)
        {
            bufs.geom[i]->setVertices(verts[i], gl::Static);
            bufs.geom[i]->setIndices(gl::Triangles, indices[i], gl::Static);
        }

        DENG2_ASSERT(indices[0].size() % 3 == 0);
        DENG2_ASSERT(indices[1].size() % 3 == 0);

        LOG_MSG("Built %i vertices and %i indices for opaque geometry; %i vertices and %i indices "
                "for transparent geometry")
            << verts[0].size() << indices[0].size() << verts[1].size() << indices[1].size();

        return bufs;
    }
};

MapBuild::MapBuild(const Map &map, const MaterialLib &materials)
    : d(new Impl(map, materials))
{}

MapBuild::Buffers MapBuild::build()
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
