#include "map.h"

namespace gloom {

DENG2_PIMPL(Map)
{
    ID      idGen{0};
    Points  points;
    Lines   lines;
    Planes  planes;
    Sectors sectors;
    Volumes volumes;

    Impl(Public *i) : Base(i)
    {}
};

Map::Map() : d(new Impl(this))
{
    IDList corners{{ append(points(), Point{-4, -4}),
                     append(points(), Point{ 4, -4}),
                     append(points(), Point{ 4,  4}),
                     append(points(), Point{ 0,  4}),
                     append(points(), Point{ 0,  8}),
                     append(points(), Point{-4,  8}) }};

    IDList sides{{ append(lines(), Line{{corners[0], corners[1]}}),
                   append(lines(), Line{{corners[1], corners[2]}}),
                   append(lines(), Line{{corners[2], corners[3]}}),
                   append(lines(), Line{{corners[3], corners[4]}}),
                   append(lines(), Line{{corners[4], corners[5]}}),
                   append(lines(), Line{{corners[5], corners[0]}}) }};

    ID floor = append(planes(), Plane{{0, 0, 0}, {0, 1, 0}});
    ID ceil  = append(planes(), Plane{{0, 3, 0}, {0, -1, 0}});

    ID vol = append(volumes(), Volume{{floor, ceil}});

    /*ID room =*/ append(sectors(), Sector{sides, IDList({vol})});
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

} // namespace gloom
