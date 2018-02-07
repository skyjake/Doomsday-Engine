#include "mapbuild.h"

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
        const Vector3f dir = lineSpan(line).normalize();
        return dir.cross(Vector3f(0, 1, 0));
    }

    /**
     * Builds a mesh with triangles for all planes and walls.
     */
    Buffer *build()
    {
        Buffer *buf = new Buffer;
        Buffer::Vertices verts;

        for (const Sector &sector : map.sectors())
        {
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
                            planeVerts[p].insert(pointId, projectPoint(pointId, plane));
                        }
                    }
                }

                for (const ID lineId : sector.lines)
                {
                    const Line &   line   = map.lines()[lineId];
                    const ID       start  = line.points[0];
                    const ID       end    = line.points[1];
                    const Vector3f normal = normalVector(line);
                    const float    length = float((planeVerts[0][end] - planeVerts[0][start]).length());
                    const float    height[2] = {planeVerts[1][start].y - planeVerts[0][start].y,
                                                planeVerts[1][end].y   - planeVerts[0][end].y};
                    Buffer::Type v;

                    v.texture = textures["world.stone"];
                    v.normal  = normal;

                    v.pos = planeVerts[0][start];
                    v.texCoord = Vector2f(0, 0);
                    verts << v;

                    v.pos = planeVerts[0][end];
                    v.texCoord = Vector2f(length, 0);
                    verts << v;

                    v.pos = planeVerts[1][end];
                    v.texCoord = Vector2f(length, height[1]);
                    verts << v;

                    v.pos = planeVerts[0][start];
                    v.texCoord = Vector2f(0, 0);
                    verts << v;

                    v.pos = planeVerts[1][end];
                    v.texCoord = Vector2f(length, height[1]);
                    verts << v;

                    v.pos = planeVerts[1][start];
                    v.texCoord = Vector2f(0, height[0]);
                    verts << v;
                }
            }
        }

        buf->setVertices(gl::Triangles, verts, gl::Static);
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
