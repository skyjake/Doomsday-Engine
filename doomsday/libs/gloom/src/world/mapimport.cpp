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
#include <doomsday/res/idtech1flatlib.h>
#include <doomsday/res/idtech1texturelib.h>
#include <doomsday/res/idtech1util.h>

#include <de/byteorder.h>
#include <de/dataarray.h>
#include <de/filesystem.h>
#include <de/folder.h>
#include <de/version.h>

#include <nlohmann/json.hpp>
#include <iomanip>
#include <sstream>

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

DE_PIMPL_NOREF(MapImport)
{
    const res::LumpCatalog &lumps;
    res::IdTech1FlatLib     flatLib;
    res::IdTech1TextureLib  textureLib;
    String                  mapId;
    Map                     map;
    List<ID>                sectorLut; // id1 sector number => Gloom sector ID
    Set<String>             textures;
    Vec3d                   metersPerUnit;
    double                  worldAspectRatio = 1.2;

    enum LevelFormat { UnknownFormat, DoomFormat, HexenFormat };
    LevelFormat levelFormat = UnknownFormat;

#if defined (_MSC_VER)
#  pragma pack(push, 1)
#endif

    static const dint16 LineFlag_UpperTextureUnpegged = 0x0008;
    static const dint16 LineFlag_LowerTextureUnpegged = 0x0010;

    struct DoomVertex {
        dint16 x;
        dint16 y;
    };

    struct DoomSidedef {
        dint16 xOffset;
        dint16 yOffset;
        char   upperTexture[8];
        char   lowerTexture[8];
        char   middleTexture[8];
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
        dint16  floorHeight;
        dint16  ceilingHeight;
        char    floorTexture[8];
        char    ceilingTexture[8];
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
        return res::wad::nameString(texture).beginsWith("F_SKY");
    }

    bool import(const String &mapId)
    {
        static const uint16_t invalidIndex = 0xffff;

        map.clear();
        textures.clear();

        const auto headerPos = lumps.find(mapId);

        levelFormat = (lumps.lumpName(headerPos + 11) == "BEHAVIOR" ? HexenFormat : DoomFormat);

        debug("Importing map: %s %s",
              mapId.c_str(),
              levelFormat == DoomFormat ? "(Doom)" : "(Hexen)");

        this->mapId = mapId.lower();

        // Conversion from Doom map units (Doom texels) to meters.
        // (Approximate typical human eye height slightly adjusted for a short reciprocal.)
        const double humanEyeHeight = 1.74;
        const double mpu = humanEyeHeight / (levelFormat == DoomFormat ? 41.0 : 48.0);

        metersPerUnit = {mpu, mpu * worldAspectRatio, mpu}; // VGA aspect ratio for vertical
        map.setMetersPerUnit(metersPerUnit);

        const auto linedefData = lumps.read(headerPos + 2);

        const DataArray<DoomVertex>   idVertices(lumps.read(headerPos + 4));
        const DataArray<DoomLinedef>  doomLinedefs(linedefData);
        const DataArray<HexenLinedef> hexenLinedefs(linedefData);
        const DataArray<DoomSidedef>  idSidedefs(lumps.read(headerPos + 3));
        const DataArray<DoomSector>   idSectors(lumps.read(headerPos + 8));

        const int linedefsCount =
            (levelFormat == DoomFormat ? doomLinedefs.size() : hexenLinedefs.size());

        List<ID> mappedVertex(idVertices.size());

        struct MappedSector {
            ID       sector  = 0;
            ID       floor   = 0;
            ID       liquid  = 0;
            ID       ceiling = 0;
            Set<ID>  points;
            List<ID> boundaryLines;
        };
        List<MappedSector> mappedSectors(idSectors.size());

        List<ID> mappedLines(linedefsCount);

        // -------- Create planes for all sectors: each gets a separate floor and ceiling --------

        for (int i = 0; i < idSectors.size(); ++i)
        {
            const auto &sec = idSectors[i];

            // Plane materials.
            String floorTexture   = "flat." + res::wad::nameString(sec.floorTexture).lower();
            String ceilingTexture = "flat." + res::wad::nameString(sec.ceilingTexture).lower();

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
                           Plane{Vec3d(0, le16(sec.floorHeight), 0),
                                 Vec3f(0, 1, 0),
                                 {floorTexture, ""}});
            mappedSectors[i].ceiling =
                map.append(map.planes(),
                           Plane{Vec3d(0, le16(sec.ceilingHeight), 0),
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
            uint16_t sectors[2]{invalidIndex, invalidIndex};
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
                        Point{Vec2d(le16(idVertices[idx[p]].x), -le16(idVertices[idx[p]].y))});
                }
                line.points[p] = mappedVertex[idx[p]];

                // Sides.
                if (sides[p] != invalidIndex)
                {
                    const auto &sdef = idSidedefs[sides[p]];

                    sectors[p]              = le16u(sdef.sector);
                    line.surfaces[p].sector = (sectors[p] != invalidIndex? mappedSectors[sectors[p]].sector : 0);

                    const auto midTex = res::wad::nameString(sdef.middleTexture).lower();
                    const auto upTex  = res::wad::nameString(sdef.upperTexture).lower();
                    const auto lowTex = res::wad::nameString(sdef.lowerTexture).lower();

                    if (midTex != "-")
                    {
                        middleTexture[p] = "texture." + midTex;
                    }
                    if (upTex != "-")
                    {
                        upperTexture[p]  = "texture." + upTex;
                    }
                    if (lowTex != "-")
                    {
                        lowerTexture[p]  = "texture." + lowTex;
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
        sectorLut.clear();
        sectorLut.resize(mappedSectors.size());

        for (dsize secIndex = 0; secIndex < mappedSectors.size(); ++secIndex)
        {
            const auto &ms = mappedSectors[secIndex];

            sectorLut[secIndex] = ms.sector;

            debug("Sector %i: boundary lines %zu, points %zu",
                   secIndex,
                   ms.boundaryLines.size(),
                   ms.points.size());

            builder.polygonize(ms.sector, ms.boundaryLines);
        }

        textures.remove("");

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

String MapImport::mapId() const
{
    return d->mapId;
}

StringList MapImport::materials() const
{
    return compose<StringList>(d->textures.begin(), d->textures.end());
}

Image MapImport::materialImage(const String &name) const
{
    if (!name) return {};

    const DotPath path(name);
    if (path.segmentCount() < 2) return {};

    const auto &category = path.segment(0);

    if (category == DE_STR("texture"))
    {
        const auto img = d->textureLib.textureImage(path.segment(1).toLowercaseString());
        return Image::fromRgbaData(img.pixelSize(), img.pixels());
    }
    else if (category == DE_STR("flat"))
    {
        const auto img = d->flatLib.flatImage(path.segment(1).toLowercaseString());
        return Image::fromRgbaData(img.pixelSize(), img.pixels());
    }

    return {};
}

void MapImport::exportPackage(const String &packageRootPath) const
{
    Folder &root = FS::get().makeFolder(packageRootPath); // or use existing folder...

    debug("Export package: %s", root.correspondingNativePath().c_str());

    // Organize using subfolders.
    /*Folder &textures =*/ FS::get().makeFolder(packageRootPath / "textures");
    /*Folder &flats    =*/ FS::get().makeFolder(packageRootPath / "flats");
    Folder &maps       =   FS::get().makeFolder(packageRootPath / "maps");

    // Package info (with required metadata).
    {
        File & f   = root.replaceFile("info.dei");
        String dei = "title: " + d->mapId.upper() +
                     "\nversion: 1.0"
                     "\ntags: map"
                     "\nlicense: unknown"
                     "\ngenerator: Doomsday " + Version::currentBuild().fullNumber() +
                     "\n\n@include <materials.dei>\n"
                     "@include <maps.dei>\n";
        // TODO: Include all information known about the map based on the WAD file, etc.
        f << dei.toUtf8();
        f.flush();
    }

    // Maps included in the pacakge.
    {
        File & f   = root.replaceFile("maps.dei");
        String dei = "asset map." + mapId() + " {"
                     "\n    path = \"maps/" + d->mapId + ".gloommap\""
                     "\n    lookupPath = \"maps/" + d->mapId + ".lookup.json\""
                     "\n    metersPerUnit " +
                     Stringf("<%.16f, %.16f, %.16f>",
                             d->metersPerUnit.x, d->metersPerUnit.y, d->metersPerUnit.z) +
                     "\n}\n";
        f << dei.toUtf8();
        f.flush();
    }

    // The map itself.
    {
        File &f = maps.replaceFile(d->mapId + ".gloommap");
        f << d->map.serialize();
        f.flush();
    }

    // Source index lookup tables.
    {
        nlohmann::json lut;
        lut["sectorIds"] = nlohmann::json::array();
        for (const uint32_t id : d->sectorLut)
        {
            lut["sectorIds"].push_back(id);
        }
        File &f = maps.replaceFile(d->mapId + ".lookup.json");
        std::ostringstream os;
        os << lut;
        f << String(os.str()).toUtf8();
        f.flush();
    }

    // Materials used in the map.
    {
        std::ostringstream os;

        for (const String &name : materials())
        {
            debug("Exporting: %s", name.c_str());

            const DotPath path{name};
            const String  category  = path.segment(0).toLowercaseString();
            const String  subfolder = (category == "texture" ? "textures" : "flats");
            const String  imgPath   = subfolder / path.segment(1) + "_diffuse.png";

            const double ppm = 1.0 / d->metersPerUnit.x;

            os << std::setprecision(16);
            os << "asset material." << name << " {\n    ppm = " << ppm << "\n";
            os << "    verticalAspect = " << (category == "texture"? "True" : "False") << "\n";
            os << "    diffuse: " << imgPath << "\n}\n\n";

            const auto image = materialImage(name);
            DE_ASSERT(!image.isNull());

//            const Block imgData = ;
//            {
//                QBuffer outBuf(&imgData);
//                outBuf.open(QIODevice::WriteOnly);
//                image.toQImage().rgbSwapped().save(&outBuf, "PNG", 1);

//            }

            File &f = root.replaceFile(imgPath);
            f << image.serialize(Image::Png);
            f.reinterpret();

            // ".diffuse", ".specgloss", ".emissive", ".normaldisp"
            // OR: .basecolor .metallic .normal .roughness
            // - ppm (pixels per meter)
            // - opaque/transparent
        }

//        os.flush();

        File &f = root.replaceFile("materials.dei");
        f << String(os.str());
        f.flush();
    }
}

} // namespace gloom
