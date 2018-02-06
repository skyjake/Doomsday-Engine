#include "mapbuild.h"

using namespace de;

namespace gloom {

DENG2_PIMPL_NOREF(MapBuild)
{
    const Map &map;

    Impl(const Map &map) : map(map) {}

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
        Vector3f pos   = vertex(pointId);
        Vector3f delta = pos - plane.point;
        double   dist  = delta.dot(plane.normal);
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
                    const Vector3f normal = normalVector(line);

                    Buffer::Type v;

                    v.rgba     = Vector4f(1, 1, 1, 1);
                    v.normal   = normal;
                    v.texCoord = Vector2f(0, 0);

                    v.pos = planeVerts[1][line.points[0]];
                    verts << v;

                    v.pos = planeVerts[1][line.points[1]];
                    verts << v;

                    v.pos = planeVerts[0][line.points[1]];
                    verts << v;

                    v.pos = planeVerts[1][line.points[0]];
                    verts << v;

                    v.pos = planeVerts[0][line.points[1]];
                    verts << v;

                    v.pos = planeVerts[0][line.points[0]];
                    verts << v;
                }
            }
        }

        buf->setVertices(gl::Triangles, verts, gl::Static);
        return buf;
    }
};

MapBuild::MapBuild(const Map &map)
    : d(new Impl(map))
{}

MapBuild::Buffer *MapBuild::build()
{
    return d->build();
}

} // namespace gloom
