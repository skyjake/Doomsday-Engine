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
    QVector<ID>  lines;
    QSet<ID>     hasPoints;
    geo::Polygon polygon;
    int          parent = -1;

    Contour(ID line = 0)
    {
        if (line) lines << line;
    }

    int size() const
    {
        return polygon.size();
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
        update();
    }

    bool isInside(const Contour &other) const
    {
        return polygon.isInsideOf(other.polygon);
    }

    int findSharedPoint(const Contour &other) const
    {
        for (int i = 0; i < polygon.points.size(); ++i)
        {
            if (other.hasPoints.contains(polygon.points.at(i).id))
            {
                return i;
            }
        }
        return -1;
    }

    int findPoint(ID id) const
    {
        for (int i = 0; i < polygon.points.size(); ++i)
        {
            if (polygon.points.at(i).id == id)
            {
                return i;
            }
        }
        return -1;
    }

    bool hasLineWithPoints(const Map &map, ID a, ID b) const
    {
        for (ID lineId : lines)
        {
            const auto &line = map.line(lineId);
            if ((line.points[0] == a && line.points[1] == b) ||
                (line.points[0] == b && line.points[1] == a))
            {
                return true;
            }
        }
        return false;
    }

    void clear()
    {
        polygon.clear();
        hasPoints.clear();
    }

    void update()
    {
        hasPoints.clear();
        for (const auto &pp : polygon.points)
        {
            hasPoints.insert(pp.id);
        }
        polygon.updateBounds();
    }

    String asText() const
    {
        return polygon.asText() + " Lines: (" +
               String::join(map<StringList>(lines, [](ID id) { return String::number(id, 16); }),
                            " ") +
               ")";
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

    Vec3d worldScale;

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

    template <typename T>
    struct DataArray
    {
        explicit DataArray(Block data)
            : _data(std::move(data))
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

        const auto headerPos = lumps.find(mapId);

        levelFormat = (lumps.lumpName(headerPos + 11) == "BEHAVIOR" ? HexenFormat : DoomFormat);

        qDebug() << "Importing map:" << mapId
                 << (levelFormat == DoomFormat ? "(Doom)" : "(Hexen)");

        // Conversion from map units to meters.
        // (Based on Doom Guy vs. average male eye height.)
        const double MapUnit = 1.74 / (levelFormat == DoomFormat ? 41.0 : 48.0);

        worldScale = {MapUnit, MapUnit * 1.2, MapUnit}; // VGA aspect ratio for vertical

        const auto linedefData = lumps.read(headerPos + 2);

        const DataArray<DoomVertex>   vertices(lumps.read(headerPos + 4));
        const DataArray<DoomLinedef>  doomLinedefs(linedefData);
        const DataArray<HexenLinedef> hexenLinedefs(linedefData);
        const DataArray<DoomSidedef>  sidedefs(lumps.read(headerPos + 3));
        const DataArray<DoomSector>   sectors(lumps.read(headerPos + 8));

        const int linedefsCount = (levelFormat == DoomFormat ? doomLinedefs.size() : hexenLinedefs.size());

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

        QVector<ID> mappedLines(linedefsCount);

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

        // Create lines with one or two sides.
        for (int i = 0; i < linedefsCount; ++i)
        {
            uint16_t idx[2];
            uint16_t sides[2];

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

            Line line;

            for (int p = 0; p < 2; ++p)
            {
                if (!mappedVertex[idx[p]])
                {
                    mappedVertex[idx[p]] = map.append(
                        map.points(),
                        Point{Vec2d(le16(vertices[idx[p]].x), -le16(vertices[idx[p]].y)) *
                              worldScale.xz()});
                }
                line.points[p] = mappedVertex[idx[p]];
            }

            uint16_t sectorIdx[2]{NoSector, NoSector};

            for (int s = 0; s < 2; ++s)
            {
                const uint16_t sec = (sides[s] != NoSector? le16u(sidedefs[sides[s]].sector) : NoSector);
                line.surfaces[s].sector = (sec != NoSector? mappedSectors[sec].sector : 0);
                sectorIdx[s] = sec;
            }

            //qDebug("Line %i: side %i/%i sector %i/%i", i, sides[0], sides[1], sectorIdx[0], sectorIdx[1]);

            if (line.isOneSided())
            {
                line.surfaces[line.surfaces[Line::Front].sector ? 0 : 1].material[Line::Middle] =
                    "world.stone";
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

        for (int secIndex = 0; secIndex < mappedSectors.size(); ++secIndex)
        {
            const auto &ms            = mappedSectors[secIndex];
            const ID    currentSector = ms.sector;
            Sector     &sector        = map.sector(ms.sector);

            qDebug("Sector %i: boundary lines %i, points %i",
                   secIndex,
                   ms.boundaryLines.size(),
                   ms.points.size());

            // TODO: Add a Contours class that manages multiple Contour objects.

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
            for (auto &cont : contours)
            {
                if (cont.lines.size() >= 3) // && cont.isClosed(map, currentSector))
                {
                    cont.makePolygon(map, currentSector);
                }
                else
                {
                    qDebug("Ignoring contour %li (size: %i, closed: %i)",
                           &cont - &contours.at(0),
                           cont.lines.size(),
                           cont.isClosed(map, currentSector));
                    cont.clear();
                }
            }

            // Some contours may share points with other contours. Let's see if can get them
            // merged together.
            for (int i = 0; i < contours.size(); ++i)
            {
                for (int j = 0; j < contours.size(); ++j)
                {
                    if (i == j) continue;

                    Contour &host  = contours[i];
                    Contour &graft = contours[j];

                    if (host.size() < 3 || graft.size() < 3) continue;

                    int hostIdx = host.findSharedPoint(graft);
                    if (hostIdx >= 0)
                    {
                        int graftIdx = graft.findPoint(host.polygon.points.at(hostIdx).id);

                        geo::Polygon::Points joined = host.polygon.points.mid(0, hostIdx);
                        for (int k = 0; k < graft.size(); ++k)
                        {
                            joined << graft.polygon.pointAt(graftIdx + k);
                        }
                        joined += host.polygon.points.mid(hostIdx);

                        qDebug("Contours %i and %i have a shared point %i/%i", i, j, hostIdx, graftIdx);
                        qDebug() << "   Host:" << host.polygon.asText();
                        qDebug() << "   Graft:" << graft.polygon.asText();

                        host.polygon.points = joined;
                        host.update();
                        graft.clear();

                        qDebug() << "   Result:" << host.polygon.asText();
                    }
                }
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
                qDebug() << "- contour" << i << ":" << contours[i].polygon.asText()
                         << "closed:" << contours[i].isClosed(map, currentSector)
                         << "parent:" << contours[i].parent;
            }

            for (int i = 0; i < contours.size(); ++i)
            {
                if (contours[i].parent == -1 && !contours[i].polygon.isClockwiseWinding())
                {
                    qDebug("Ignoring top-level contour %i due to the wrong winding", i);
                    contours[i].clear();
                }
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

            auto countLinesContactingPoint = [this, &contours](ID pointId) -> int {
                int count = 0;
                for (const Contour &cont : contours)
                {
                    for (ID lineId : cont.lines)
                    {
                        const auto &line = map.line(lineId);
                        if (line.points[0] == pointId || line.points[1] == pointId)
                        {
                            qDebug("\tline %x contacting point %x", lineId, pointId);
                            count++;
                        }
                    }
                }
                return count;
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

                        bool success = false;
                        foreach (const auto &connector, candidates)
                        {
                            geo::Polygon::Points joined =
                                    outer.polygon.points.mid(0, connector.outer + 1);
                            for (int i = 0; i < inner.polygon.size() + 1; ++i)
                            {
                                joined.push_back(inner.polygon.pointAt(connector.inner + i));
                            }
                            joined += outer.polygon.points.mid(connector.outer);

                            // The outer contour must retain a clockwise winding.
                            const geo::Polygon joinedPoly{joined};
                            if (joinedPoly.isClockwiseWinding())
                            {
                                outer.polygon = joinedPoly;
                                outer.update();
                                inner.clear();
                                success = true;
                                break;
                            }
                        }
                        if (!success)
                        {
                            // Failure!
                            qDebug("Failed to join inner contour %i to its parent %i",
                                   innerIndex, outerIndex);
                        }
                    }
                }
            }

            // Clean up split contours. For example, in Hexen MAP02, there are some partial
            // contours inside walls that should be ignored.
            for (Contour &cont : contours)
            {
                if (cont.size() < 3) continue;

                // Find the gap.
                while (!cont.isClosed(map, currentSector))
                {
                    qDebug("Countour %li is not closed, finding the gap...", &cont - &contours[0]);
                    qDebug() << "   " << cont.asText();

                    bool modified = false;
                    for (int i = 0; i < cont.polygon.size(); ++i)
                    {
                        const ID startPoint = cont.polygon.pointAt(i).id;
                        const ID endPoint   = cont.polygon.pointAt(i + 1).id;

                        if (!cont.hasLineWithPoints(map, startPoint, endPoint))
                        {
                            qDebug("  Line %i-%i (%x...%x) has no corresponding line",
                                   i,
                                   mod(i + 1, cont.polygon.size()),
                                   startPoint,
                                   endPoint);

                            for (int j = 0; j < cont.size(); ++j)
                            {
                                qDebug("    point %i (%x) has %i contacts in sector %i",
                                       j,
                                       cont.polygon.points[j].id,
                                       countLinesContactingPoint(cont.polygon.points[j].id),
                                       currentSector);
                            }

                            // This line does not actually exist, so let's get rid of it.
                            if (countLinesContactingPoint(startPoint) < 3)
                            {
                                cont.polygon.points.removeAt(i);
                                modified = true;
                            }
                            else if (countLinesContactingPoint(endPoint) < 3)
                            {
                                cont.polygon.points.removeAt(mod(i + 1, cont.polygon.size()));
                                modified = true;
                            }

                            if (modified)
                            {
                                cont.update();
                                qDebug() << "  Removed fringe point:" << cont.polygon.asText();
                            }
                            break;
                        }
                    }
                    if (!modified && cont.polygon.size() > 0)
                    {
                        qDebug("  Contour could not be closed!");
                        break; // Hmm.
                    }
                }

                if (cont.size() < 3)
                {
                    qDebug("  Removing contour with %i points", cont.size());
                    cont.clear();
                }

                // Look for two-point zero-area loops.
                {
                    auto &poly = cont.polygon;
                    bool modified = false;
                    for (int i = 0; i < poly.size(); ++i)
                    {
                        if (poly.points[i].id == poly.pointAt(i + 2).id)
                        {
                            qDebug("  Removing zero-area loop %x..%x",
                                   poly.points[i].id,
                                   poly.pointAt(i + 1).id);
                            poly.points.remove(i, 2);
                            modified = true;
                            i = -1; // restart
                        }
                    }
                    if (modified) cont.update();
                }
            }

            // Remove walls outside the sector polygon.
            for (int i = 0; i < sector.walls.size(); ++i)
            {
                const auto mapLine = map.line(sector.walls[i]);
                bool       inPoly  = false;
                foreach (const Contour &cont, contours)
                {
                     if (cont.hasPoints.contains(mapLine.points[0]) &&
                         cont.hasPoints.contains(mapLine.points[1]))
                     {
                         inPoly = true;
                     }
                }
                if (!inPoly)
                {
                    qDebug("  Sector line %x is not inside the contours", sector.walls[i]);
                    auto &line = map.line(sector.walls[i]);
                    line.surfaces[line.sectorSide(currentSector)].sector = 0;
                    sector.walls.removeAt(i--);
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
                        DENG2_ASSERT(points.isEmpty() || points.back() != pp.id);
                        points << pp.id;
                    }

                    for (int i = 0; i < cont.size(); ++i)
                    {
                        DENG2_ASSERT(cont.polygon.lineAt(i).length() < 80);
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
