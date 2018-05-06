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

struct Contour
{
    QVector<ID> lines;
    bool hole = false;
    geo::Polygon polygon;
    int parent = -1;

    Contour(ID line = 0)
    {
        if (line) lines << line;
    }

    bool isClosed(const Map &map, ID currentSector) const
    {
        if (lines.size() < 2) return false;

        const auto &first = map.line(lines.front());
        const auto &last  = map.line(lines.back());

        return last.endPointForSector(currentSector) ==
               first.startPointForSector(currentSector);
    }

    bool tryExtend(ID newLine, const Map &map, ID currentSector)
    {
        if (isClosed(map, currentSector)) return false;

        // Try the end.
        if (map.line(lines.back()).endPointForSector(currentSector) ==
            map.line(newLine).startPointForSector(currentSector))
        {
            lines << newLine;
            return true;
        }
        // What about the beginning?
        if (map.line(lines.front()).startPointForSector(currentSector) ==
            map.line(newLine).endPointForSector(currentSector))
        {
            lines.prepend(newLine);
            return true;
        }
        return false;
    }

    void makePolygon(const Map &map, ID currentSector)
    {
        polygon.clear();
        foreach (ID id, lines)
        {
            const auto &line    = map.line(id);
            const ID    pointId = line.startPointForSector(currentSector);
            polygon.points << geo::Polygon::Point{map.point(pointId).coord, pointId};
        }
        polygon.updateBounds();
    }

    bool isInside(const Contour &other) const
    {
        return polygon.isInsideOf(other.polygon);
    }
};

static bool intersectsAnyLine(const geo::Line2d &line, const QVector<Contour> &contours)
{
    for (const Contour &cont : contours)
    {
        for (int i = 0; i < cont.polygon.size(); ++i)
        {
            double t;
            if (line.intersect(cont.polygon.lineAt(i), t))
            {
                if (t > 0.0 && t < 1.0)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

static String fixedString(const char *name, dsize maxLen = 8)
{
    dsize len = 0;
    while (len < maxLen && name[len]) len++;
    return String(name, len);
}

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

    bool isSky(const char *texture) const
    {
        return fixedString(texture).startsWith("F_SKY");
    }

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
            ID       sector  = 0;
            ID       floor   = 0;
            ID       liquid  = 0;
            ID       ceiling = 0;
            QSet<ID> points;
            QVector<ID> boundaryLines;
        };
        QVector<MappedSector> mappedSectors(sectors.size());

        QVector<ID> mappedLines(linedefs.size());

        // Create planes for all sectors: each gets a separate floor and ceiling.
        for (int i = 0; i < sectors.size(); ++i)
        {
            const auto &sec = sectors[i];

            // Plane materials.
            String floorTexture   = "world.dirt";
            String ceilingTexture = "world.stone";
            if (isSky(sec.floorTexture))
            {
                floorTexture = "";
            }
            if (isSky(sec.ceilingTexture))
            {
                ceilingTexture = "";
            }

            mappedSectors[i].floor = map.append(map.planes(),
                                                  Plane{Vec3d(0, le16(sec.floorHeight) * MapUnit, 0),
                                                        Vec3f(0, 1, 0),
                                                        {floorTexture, ""}});
            mappedSectors[i].ceiling = map.append(map.planes(),
                                                  Plane{Vec3d(0, le16(sec.ceilingHeight) * MapUnit, 0),
                                                        Vec3f(0, -1, 0),
                                                        {ceilingTexture, ""}});

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
            uint16_t       sectorIdx[2]{NoSector, NoSector};

            for (int s = 0; s < 2; ++s)
            {
                const uint16_t sec = (sides[s] != NoSector? le16u(sidedefs[sides[s]].sector) : NoSector);
                line.surfaces[s].sector = (sec != NoSector? mappedSectors[sec].sector : 0);
                sectorIdx[s] = sec;
            }

            if (line.isOneSided())
            {
                line.surfaces[line.surfaces[Line::Front].sector? 0 : 1].material[Line::Middle]
                        = "world.stone";
            }
            else
            {
                for (int s = 0; s < 2; ++s)
                {
                    line.surfaces[s].material[Line::Top] =
                            line.surfaces[s].material[Line::Bottom] = "world.stone";

                    if (isSky(sectors[sectorIdx[s    ]].ceilingTexture) &&
                        isSky(sectors[sectorIdx[s ^ 1]].ceilingTexture))
                    {
                        line.surfaces[s].material[Line::Top].clear();
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
                        auto &sPoints = mappedSectors[sectorIdx[s]].points;
                        sPoints.insert(line.points[0]);
                        sPoints.insert(line.points[1]);

                        mappedSectors[sectorIdx[s]].boundaryLines << lineId;
                    }
                }
            }
        }

        for (int i = 0; i < mappedSectors.size(); ++i)
        {
            const auto &ms = mappedSectors[i];

            const ID currentSector = ms.sector;

            qDebug("Sector %i: boundary lines %i, points %i",
                   i,
                   ms.boundaryLines.size(),
                   ms.points.size());

            // TODO: Add a Contours class that manages multilpe Countour objects.

            QVector<Contour> contours;
            QVector<ID>      remainingLines = ms.boundaryLines;

            // Initialize with one contour.
            contours << Contour{remainingLines.takeLast()};

            // Each line belongs to exactly one contour.
            while (!remainingLines.isEmpty())
            {
                const int oldSize = remainingLines.size();

                // Let's see if any of the lines fits on the existing contours.
                QMutableVectorIterator<ID> iter(remainingLines);
                while (iter.hasNext())
                {
                    iter.next();
                    for (Contour &cont : contours)
                    {
                        if (cont.tryExtend(iter.value(), map, currentSector))
                        {
                            iter.remove();
                            break;
                        }
                    }
                }

                if (oldSize == remainingLines.size())
                {
                    // None of the existing contours could be extended.
                    contours << Contour{remainingLines.takeLast()};
                }
            }

            for (int i = 0; i < contours.size(); ++i)
            {
                contours[i].makePolygon(map, currentSector);
            }

            // Determine the containment hierarchy.
            for (int i = 0; i < contours.size(); ++i)
            {
                int parent = -1;
                for (int j = 0; j < contours.size(); ++j)
                {
                    if (i == j) continue;
                    if (contours[i].isInside(contours[j]))
                    {
                        if (parent == -1)
                        {
                            parent = j;
                        }
                        else
                        {
                            // This new parent may be a better fit.
                            if (contours[j].isInside(contours[parent]))
                            {
                                parent = j;
                            }
                        }
                    }
                }
                contours[i].parent = parent;
            }

            for (int i = 0; i < contours.size(); ++i)
            {
                qDebug() << "- contour" << i << ":" << contours[i].lines
                         << contours[i].isClosed(map, currentSector)
                         << "parent:" << contours[i].parent;
            }

            // Determines how deeply a contour is nested.
            auto contourDepth = [&contours](const Contour &cont) -> int {
                int depth = 0;
                for (const Contour *i = &cont; i->parent != -1; depth++)
                {
                    i = &contours[i->parent];
                }
                return depth;
            };

            // Promote nested outer contours to the top level.
            for (Contour &cont : contours)
            {
                const int depth = contourDepth(cont);
                if (depth % 2 == 0)
                {
                    cont.parent = -1;
                }
            }

            // Join outer and inner contours.
            for (int outerIndex = 0; outerIndex < contours.size(); ++outerIndex)
            {
                Contour &outer = contours[outerIndex];

                const QVector<Contour> holes = filter(
                    contours, [outerIndex](const Contour &c) { return c.parent == outerIndex; });

                for (int innerIndex = 0; innerIndex < contours.size(); ++innerIndex)
                {
                    Contour &inner = contours[innerIndex];
                    if (inner.parent == outerIndex && inner.polygon.size() > 0)
                    {
                        // Choose a pair of vertices: one from the outer contour and one from
                        // the inner. The line connecting these must not cross any lines in
                        // the outer contour or any of its inner contours.

                        struct ConnectorCandidate {
                            int inner, outer;
                            double len;
                        };
                        QVector<ConnectorCandidate> candidates;

                        for (int k = 0; k < inner.polygon.size(); ++k)
                        {
                            for (int j = 0; j < outer.polygon.size(); ++j)
                            {
                                const geo::Line2d connector{outer.polygon.at(j),
                                                            inner.polygon.at(k)};

                                if (!intersectsAnyLine(connector, holes + QVector<Contour>({outer})))
                                {
                                    // This connector could work.
                                    candidates << ConnectorCandidate{k, j, connector.length()};
                                }
                            }
                        }

                        std::sort(candidates.begin(),
                                  candidates.end(),
                                  [](const ConnectorCandidate &a, const ConnectorCandidate &b) {
                                      return a.len < b.len;
                                  });

                        if (candidates.size() > 0)
                        {
                            const auto connector = candidates.front();

                            geo::Polygon::Points joined =
                                    outer.polygon.points.mid(0, connector.outer + 1);
                            for (int i = 0; i < inner.polygon.size() + 1; ++i)
                            {
                                joined.push_back(inner.polygon.pointAt(connector.inner + i));
                            }
                            joined += outer.polygon.points.mid(connector.outer);

                            outer.polygon.points = joined;
                            inner.polygon.clear();
                        }
                        else
                        {
                            // Failure!
                            qDebug("Failed to join inner contour %i to its parent %i",
                                   innerIndex, outerIndex);
                        }
                    }
                }
            }

            for (int i = 0; i < contours.size(); ++i)
            {
                qDebug() << "- contour" << i << ":" << contours[i].polygon.size();
                foreach (const auto &pp, contours[i].polygon.points)
                    qDebug("    %f, %f : %i", pp.pos.x, pp.pos.y, pp.id);
            }

            // The remaining contours are now disjoint and can be used for plane triangulation.

            IDList &points = map.sector(currentSector).points;

            foreach (const auto &cont, contours)
            {
                if (cont.polygon.size())
                {
                    if (!points.isEmpty()) points << 0; // Separator.

                    for (const auto &pp : cont.polygon.points)
                    {
                        points << pp.id;
                    }
                }
            }
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
