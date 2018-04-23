/** @file mapimport.cpp  Importer for id-formatted map data.
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

#include "gloom/world/mapimport.h"
#include <de/ByteOrder>

#include <QDebug>

using namespace de;

namespace gloom {

static inline int16_t le16(int16_t leValue)
{
#ifdef __BIG_ENDIAN__
    return int16_t(swap16(leValue));
#else
    return leValue;
#endif
}

static inline uint16_t le16u(int16_t leValue)
{
#ifdef __BIG_ENDIAN__
    return swap16(uint16_t(leValue));
#else
    return uint16_t(leValue);
#endif
}

#if !defined (_MSC_VER)
#  define PACKED_STRUCT __attribute__((packed))
#else
#  define PACKED_STRUCT
#endif

DENG2_PIMPL_NOREF(MapImport)
{
    const res::LumpCatalog &lumps;
    Map map;

#if defined (_MSC_VER)
#  pragma pack(push, 1)
#endif

    struct DoomVertex {
        dint16 x;
        dint16 y;
    };

    struct DoomSidedef {
        dint16 xOffset;
        dint16 yOffset;
        char upperTexture[8];
        char lowerTexture[8];
        char middleTexture[8];
        dint16 sector;
    };

    struct DoomLinedef {
        dint16 startVertex;
        dint16 endVertex;
        dint16 flags;
        dint16 special;
        dint16 tag;
        dint16 frontSidedef;
        dint16 backSidedef;
    };

    struct HexenLinedef {
        dint16 startVertex;
        dint16 endVertex;
        dint16 flags;
        duint8 special;
        duint8 args[5];
        dint16 frontSidedef;
        dint16 backSidedef;
    } PACKED_STRUCT;

    struct DoomSector {
        dint16 floorHeight;
        dint16 ceilingHeight;
        char floorTexture[8];
        char ceilingTexture[8];
        duint16 lightLevel;
        duint16 type;
        duint16 tag;
    };

#if defined (_MSC_VER)
#  pragma pack(pop)
#endif

    template <typename T>
    struct DataArray
    {
        DataArray(const Block &data)
            : _data(data)
            , _entries(reinterpret_cast<const T *>(_data.constData()))
            , _size(int(_data.size() / sizeof(T)))
        {}
        int size() const
        {
            return _size;
        }
        const T &at(int pos) const
        {
            DENG2_ASSERT(pos >= 0);
            DENG2_ASSERT(pos < _size);
            return _entries[pos];
        }
        const T &operator[](int pos) const
        {
            return at(pos);
        }

    private:
        Block    _data;
        const T *_entries;
        int      _size;
    };

    Impl(const res::LumpCatalog &lc) : lumps(lc)
    {}

    bool import(const String &mapId)
    {
        map.clear();

        // Conversion from map units to meters.
        const double MapUnit = 1.74 / 41.0; // Based on Doom Guy vs. average male eye height.

        const auto headerPos = lumps.find(mapId);

        const DataArray<DoomVertex>  vertices(lumps.read(headerPos, 4));
        const DataArray<DoomLinedef> linedefs(lumps.read(headerPos, 2));
        const DataArray<DoomSidedef> sidedefs(lumps.read(headerPos, 3));
        const DataArray<DoomSector>  sectors (lumps.read(headerPos, 8));

        /*qDebug("%i vertices", vertices.size());
        for (int i = 0; i < vertices.size(); ++i)
        {
            qDebug("%i: x=%i, y=%i", i, vertices[i].x, vertices[i].y);
        }*/

        QVector<ID> mappedVertex(vertices.size());

        const uint16_t NoSector = 0xffff;

        struct MappedSector {
            ID sector = 0;
            ID floor = 0;
            ID liquid = 0;
            ID ceiling = 0;
            QSet<ID> points;
        };
        QVector<MappedSector> mappedSectors(sectors.size());

        QVector<ID> mappedLines(linedefs.size());

        // Create planes for all sectors: each gets a separate floor and ceiling.
        for (int i = 0; i < sectors.size(); ++i)
        {
            const auto &sec = sectors[i];

            mappedSectors[i].floor = map.append(map.planes(),
                                                  Plane{Vec3d(0, le16(sec.floorHeight) * MapUnit, 0),
                                                        Vec3f(0, 1, 0),
                                                        {"world.stone", "world.stone"}});
            mappedSectors[i].ceiling = map.append(map.planes(),
                                                  Plane{Vec3d(0, le16(sec.ceilingHeight) * MapUnit, 0),
                                                        Vec3f(0, -1, 0),
                                                        {"world.stone", "world.stone"}});

            Sector sector;
            Volume volume{{mappedSectors[i].floor, mappedSectors[i].ceiling}};

            ID vol = map.append(map.volumes(), volume);
            sector.volumes << vol;

            mappedSectors[i].sector = map.append(map.sectors(), sector);
        }

        // Create lines with one or two sides.
        for (int i = 0; i < linedefs.size(); ++i)
        {
            const auto &   ldef = linedefs[i];
            const uint16_t idx[2]{le16u(ldef.startVertex), le16u(ldef.endVertex)};

            Line line;

            for (int p = 0; p < 2; ++p)
            {
                if (!mappedVertex[idx[p]])
                {
                    mappedVertex[idx[p]] = map.append(
                        map.points(),
                        Point{Vec2d(le16(vertices[idx[p]].x), -le16(vertices[idx[p]].y)) * MapUnit});
                }
                line.points[p] = mappedVertex[idx[p]];
            }

            const uint16_t sides[2]{le16u(ldef.frontSidedef), le16u(ldef.backSidedef)};
            uint16_t sectorIdx[2]{NoSector, NoSector};

            for (int s = 0; s < 2; ++s)
            {
                const uint16_t sec = (sides[s] != NoSector? le16u(sidedefs[sides[s]].sector) : NoSector);
//            const uint16_t backSec  = (sides[1] != NoSector? le16u(sidedefs[sides[1]].sector) : NoSector);
                sectorIdx[s] = sec;
                line.surfaces[s].sector = (sec != NoSector? mappedSectors[sec].sector : 0);
//            line.surfaces[Line::Back ].sector = (backSec  != NoSector? mappedSectors[backSec ].sector : 0);
            }

            const ID lineId = map.append(map.lines(), line);
            mappedLines[i] = lineId;

            for (int s = 0; s < 2; ++s)
            {
                if (line.surfaces[s].sector)
                {
                    auto &sec = map.sector(line.surfaces[s].sector);
                    sec.walls << lineId;

                    auto &sPoints = mappedSectors[sectorIdx[s]].points;
                    sPoints.insert(line.points[0]);
                    sPoints.insert(line.points[1]);
                }
            }
        }

        for (int i = 0; i < mappedSectors.size(); ++i)
        {
            const auto &ms = mappedSectors[i];
            foreach (const ID pid, ms.points)
            {
                map.sector(ms.sector).points << pid;
            }

            // The points must be polygonized (traverse edges clockwise).

        }

        return true;
    }
};

MapImport::MapImport(const res::LumpCatalog &lumps)
    : d(new Impl(lumps))
{}

bool MapImport::importMap(const String &mapId)
{
    return d->import(mapId);
}

Map &MapImport::map()
{
    return d->map;
}

} // namespace gloom
