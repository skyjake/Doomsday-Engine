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
#include "gloom/world/sectorpolygonizer.h"
#include <doomsday/resource/idtech1flatlib.h>
#include <doomsday/resource/idtech1texturelib.h>
#include <doomsday/resource/idtech1util.h>
#include <de/ByteOrder>
#include <de/DataArray>

#include <QDebug>

using namespace de;

namespace gloom {

static inline int16_t le16(int16_t leValue)
{
    return fromLittleEndian(leValue);
}

static inline uint16_t le16u(uint16_t leValue)
{
    return fromLittleEndian(leValue);
}

#if !defined (_MSC_VER)
#  define PACKED_STRUCT __attribute__((packed))
#else
#  define PACKED_STRUCT
#endif

DENG2_PIMPL_NOREF(MapImport)
{
    const res::LumpCatalog &lumps;
    res::IdTech1FlatLib     flatLib;
    res::IdTech1TextureLib  textureLib;
    Map                     map;
    QSet<String>            textures;

    String scope;
    Vec3d  worldScale;

    enum LevelFormat { UnknownFormat, DoomFormat, HexenFormat };
    LevelFormat levelFormat = UnknownFormat;

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

    Impl(const res::LumpCatalog &lc)
        : lumps(lc)
        , flatLib(lc)
        , textureLib(lc)
    {}

    bool isSky(const char *texture) const
    {
        return res::wad::nameString(texture).startsWith("F_SKY");
    }

    bool import(const String &mapId)
    {
        static const uint16_t INVALID_INDEX = 0xffff;

        map.clear();
        textures.clear();

        const auto headerPos = lumps.find(mapId);

        levelFormat = (lumps.lumpName(headerPos + 11) == "BEHAVIOR" ? HexenFormat : DoomFormat);
        scope = (levelFormat == DoomFormat ? "doom" : "hexen"); // TODO: use the package ID

        qDebug() << "Importing map:" << mapId
                 << (levelFormat == DoomFormat ? "(Doom)" : "(Hexen)")
                 << "in scope:" << scope;

        // Conversion from map units to meters.
        // (Based on Doom Guy vs. average male eye height.)
        const double MapUnit = 1.74 / (levelFormat == DoomFormat ? 41.0 : 48.0);

        worldScale = {MapUnit, MapUnit * 1.2, MapUnit}; // VGA aspect ratio for vertical

        const auto linedefData = lumps.read(headerPos + 2);

        const DataArray<DoomVertex>   idVertices(lumps.read(headerPos + 4));
        const DataArray<DoomLinedef>  doomLinedefs(linedefData);
        const DataArray<HexenLinedef> hexenLinedefs(linedefData);
        const DataArray<DoomSidedef>  idSidedefs(lumps.read(headerPos + 3));
        const DataArray<DoomSector>   idSectors(lumps.read(headerPos + 8));

        const int linedefsCount =
            (levelFormat == DoomFormat ? doomLinedefs.size() : hexenLinedefs.size());

        QVector<ID> mappedVertex(idVertices.size());

        struct MappedSector {
            ID          sector  = 0;
            ID          floor   = 0;
            ID          liquid  = 0;
            ID          ceiling = 0;
            QSet<ID>    points;
            QVector<ID> boundaryLines;
        };
        QVector<MappedSector> mappedSectors(idSectors.size());

        QVector<ID> mappedLines(linedefsCount);

        // -------- Create planes for all sectors: each gets a separate floor and ceiling --------

        for (int i = 0; i < idSectors.size(); ++i)
        {
            const auto &sec = idSectors[i];

            // Plane materials.
            String floorTexture   = scope + ".flat." + res::wad::nameString(sec.floorTexture);
            String ceilingTexture = scope + ".flat." + res::wad::nameString(sec.ceilingTexture);

            if (isSky(sec.floorTexture))
            {
                floorTexture = "";
            }
            if (isSky(sec.ceilingTexture))
            {
                ceilingTexture = "";
            }

            textures.insert(floorTexture);
            textures.insert(ceilingTexture);

            mappedSectors[i].floor =
                map.append(map.planes(),
                           Plane{Vec3d(0, le16(sec.floorHeight) * worldScale.y, 0),
                                 Vec3f(0, 1, 0),
                                 {floorTexture, ""}});
            mappedSectors[i].ceiling =
                map.append(map.planes(),
                           Plane{Vec3d(0, le16(sec.ceilingHeight) * worldScale.y, 0),
                                 Vec3f(0, -1, 0),
                                 {ceilingTexture, ""}});

            Sector sector;
            Volume volume{{mappedSectors[i].floor, mappedSectors[i].ceiling}};

            ID vol = map.append(map.volumes(), volume);
            sector.volumes << vol;

            mappedSectors[i].sector = map.append(map.sectors(), sector);
        }

        // -------- Create lines with one or two sides --------

        for (int i = 0; i < linedefsCount; ++i)
        {
            uint16_t idx[2];
            uint16_t sides[2];
            uint16_t sectors[2]{INVALID_INDEX, INVALID_INDEX};
            String   middleTexture[2];
            String   upperTexture[2];
            String   lowerTexture[2];
            Line     line;

            if (levelFormat == DoomFormat)
            {
                const auto &ldef = doomLinedefs[i];

                idx[0]   = le16u(ldef.startVertex);
                idx[1]   = le16u(ldef.endVertex);
                sides[0] = le16u(ldef.frontSidedef);
                sides[1] = le16u(ldef.backSidedef);
            }
            else
            {
                const auto &ldef = hexenLinedefs[i];

                idx[0]   = le16u(ldef.startVertex);
                idx[1]   = le16u(ldef.endVertex);
                sides[0] = le16u(ldef.frontSidedef);
                sides[1] = le16u(ldef.backSidedef);
            }

            for (int p = 0; p < 2; ++p)
            {
                // Line points.
                if (!mappedVertex[idx[p]])
                {
                    mappedVertex[idx[p]] = map.append(
                        map.points(),
                        Point{Vec2d(le16(idVertices[idx[p]].x), -le16(idVertices[idx[p]].y)) *
                              worldScale.xz()});
                }
                line.points[p] = mappedVertex[idx[p]];

                // Sides.
                if (sides[p] != INVALID_INDEX)
                {
                    const auto &sdef = idSidedefs[sides[p]];

                    sectors[p]              = le16u(sdef.sector);
                    line.surfaces[p].sector = (sectors[p] != INVALID_INDEX? mappedSectors[sectors[p]].sector : 0);

                    const auto midTex = res::wad::nameString(sdef.middleTexture);
                    const auto upTex  = res::wad::nameString(sdef.upperTexture);
                    const auto lowTex = res::wad::nameString(sdef.lowerTexture);

                    if (midTex != "-")
                    {
                        middleTexture[p] = scope + ".texture." + midTex;
                    }
                    if (upTex != "-")
                    {
                        upperTexture[p]  = scope + ".texture." + upTex;
                    }
                    if (lowTex != "-")
                    {
                        lowerTexture[p]  = scope + ".texture." + lowTex;
                    }

                    textures.insert(middleTexture[p]);
                    textures.insert(upperTexture[p]);
                    textures.insert(lowerTexture[p]);
                }
            }

            //qDebug("Line %i: side %i/%i sector %i/%i", i, sides[0], sides[1], sectorIdx[0], sectorIdx[1]);

            if (line.isOneSided())
            {
                const int side = line.surfaces[Line::Front].sector ? 0 : 1;
                line.surfaces[side].material[Line::Middle] = middleTexture[side];
            }
            else
            {
                for (int s = 0; s < 2; ++s)
                {
                    line.surfaces[s].material[Line::Top]    = upperTexture[s];
                    line.surfaces[s].material[Line::Bottom] = lowerTexture[s];

                    if (isSky(idSectors[sectors[s    ]].ceilingTexture) &&
                        isSky(idSectors[sectors[s ^ 1]].ceilingTexture))
                    {
                        line.surfaces[s].material[Line::Top].clear();
                        // TODO: Flag for sky stenciling.
                    }
                }
            }

            const ID lineId = map.append(map.lines(), line);
            mappedLines[i] = lineId;

            for (int s = 0; s < 2; ++s)
            {
                if (line.surfaces[s].sector)
                {
                    auto &sec = map.sector(line.surfaces[s].sector);
                    sec.walls << lineId;

                    // An internal line won't influence the plane points.
                    if (line.surfaces[s].sector != line.surfaces[s ^ 1].sector)
                    {
                        auto &sPoints = mappedSectors[sectors[s]].points;
                        sPoints.insert(line.points[0]);
                        sPoints.insert(line.points[1]);

                        mappedSectors[sectors[s]].boundaryLines << lineId;
                    }
                }
            }
        }

        SectorPolygonizer builder(map);

        for (int secIndex = 0; secIndex < mappedSectors.size(); ++secIndex)
        {
            const auto &ms = mappedSectors[secIndex];

            qDebug("Sector %i: boundary lines %i, points %i",
                   secIndex,
                   ms.boundaryLines.size(),
                   ms.points.size());

            builder.polygonize(ms.sector, ms.boundaryLines);
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

StringList MapImport::textures() const
{
    return compose<StringList>(d->textures.constBegin(), d->textures.constEnd());
}

Image MapImport::textureImage(const String &name) const
{
    if (!name) return {};

    const DotPath path(name);
    if (path.segmentCount() < 3) return {};

    const auto &category = path.segment(1);

    if (category == QStringLiteral("texture"))
    {
        const auto img = d->textureLib.textureImage(path.segment(2));
        return Image::fromRgbaData(img.pixelSize(), img.pixels());
    }
    else if (category == QStringLiteral("flat"))
    {
        const auto img = d->flatLib.flatImage(path.segment(2));
        return Image::fromRgbaData(img.pixelSize(), img.pixels());
    }

    return {};
}

} // namespace gloom
