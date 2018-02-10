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

using namespace de;

namespace gloom {

internal::AttribSpec const MapVertex::_spec[4] =
{
    { internal::AttribSpec::Position,  3, GL_FLOAT,        false, sizeof(MapVertex), 0     },
    { internal::AttribSpec::Normal,    3, GL_FLOAT,        false, sizeof(MapVertex), 3 * 4 },
    { internal::AttribSpec::TexCoord0, 2, GL_FLOAT,        false, sizeof(MapVertex), 6 * 4 },
    { internal::AttribSpec::Texture,   1, GL_UNSIGNED_INT, false, sizeof(MapVertex), 8 * 4 }
};
LIBGUI_VERTEX_FORMAT_SPEC(MapVertex, 9 * 4)

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
        const Vector3f pos   = vertex(pointId);
        const Vector3f delta = pos - plane.point;
        const double   dist  = delta.dot(plane.normal);
        return pos - plane.normal * dist;
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

        for (const Sector &sector : map.sectors())
        {
            // Split the polygon to convex parts (for triangulation).
            Polygon::Points polyPoints;
            for (const ID lineId : sector.lines) // assumed to have clockwise winding
            {
                const Line &line = map.lines()[lineId];
                polyPoints << Polygon::Point({map.points()[line.points[0]], line.points[0]});
            }
            const auto convexParts = Polygon(polyPoints).splitConvexParts();

            // Each volume is built separately.
            for (const ID volId : sector.volumes)
            {
                const Volume &volume = map.volumes()[volId];
                QHash<ID, Vector3f> planeVerts[2];
                for (int p = 0; p < 2; ++p)
                {
                    const Plane &plane = map.planes()[volume.planes[p]];
                    for (const ID lineId : sector.lines)
                    {
                        const Line &line = map.lines()[lineId];
                        for (const ID pointId : line.points)
                        {
                            if (!planeVerts[p].contains(pointId))
                            {
                                planeVerts[p].insert(pointId, projectPoint(pointId, plane));
                            }
                        }
                    }
                }

                // Build the floor and ceiling of this volume.
                {
                    // Insert vertices of both planes to the buffer.
                    Buffer::Type f, c;
                    QHash<ID, Buffer::Index> pointIndices;

                    f.texture = textures["world.grass"];
                    f.normal = Vector3f(0, 1, 0);

                    c.texture = textures["world.stone"];
                    c.normal = Vector3f(0, -1, 0);

                    for (const ID pointID : planeVerts[0].keys())
                    {
                        f.pos = planeVerts[0][pointID];
                        c.pos = planeVerts[1][pointID];

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

                // Build the walls.
                for (const ID lineId : sector.lines)
                {
                    const Line &   line   = map.lines()[lineId];
                    const ID       start  = line.points[0];
                    const ID       end    = line.points[1];
                    const Vector3f normal = normalVector(line);
                    const float    length = float((planeVerts[0][end] - planeVerts[0][start]).length());
                    const float    heights[2] = {planeVerts[1][start].y - planeVerts[0][start].y,
                                                 planeVerts[1][end].y   - planeVerts[0][end].y};

                    const Buffer::Index baseIndex = Buffer::Index(verts.size());
                    indices << baseIndex
                            << baseIndex + 3
                            << baseIndex + 2
                            << baseIndex
                            << baseIndex + 1
                            << baseIndex + 3;

                    Buffer::Type v;

                    v.texture = textures["world.dirt"];
                    v.normal  = normal;

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
                    verts << v;
                }
            }
        }

        buf->setVertices(verts, gl::Static);
        buf->setIndices(gl::Triangles, indices, gl::Static);

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
