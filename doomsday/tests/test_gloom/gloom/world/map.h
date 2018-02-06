#ifndef GLOOM_MAP_H
#define GLOOM_MAP_H

#include <de/Vector>
#include <QHash>

namespace gloom {

typedef uint32_t     ID;
typedef QList<ID>    IDList;
typedef de::Vector2d Point;

struct Line
{
    ID points[2];
};
struct Plane
{
    de::Vector3d point;
    de::Vector3f normal;
};
struct Volume
{
    ID planes[2];
};
struct Sector
{
    IDList lines;
    IDList volumes;
};

typedef QHash<ID, Point>  Points;
typedef QHash<ID, Line>   Lines;
typedef QHash<ID, Plane>  Planes;
typedef QHash<ID, Sector> Sectors;
typedef QHash<ID, Volume> Volumes;

/*class Vertices
{
public:
    Vertices();

    int           count() const;
    const Vertex &at(int index) const;
    Vertex &      at(int index);

    void clear();
    void append(const Vertex &vertex);
    int  removeAt(int index);

    Vertices &operator << (const Vertex &vertex)
    {
        append(vertex);
        return *this;
    }

private:
    DENG2_PRIVATE(d)
};*/

/**
 * Describes a map of polygon-based sectors.
 */
class Map
{
public:
    Map();

    ID newID();

    template <typename H, typename T>
    ID append(H &hash, const T& value)
    {
        const ID id = newID();
        hash.insert(id, value);
        return id;
    }

    Points & points();
    Lines &  lines();
    Planes & planes();
    Sectors &sectors();
    Volumes &volumes();

    const Points & points() const;
    const Lines &  lines() const;
    const Planes & planes() const;
    const Sectors &sectors() const;
    const Volumes &volumes() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOM_MAP_H
