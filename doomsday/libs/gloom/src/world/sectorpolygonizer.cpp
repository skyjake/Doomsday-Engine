/** @file sectorpolygonizer.cpp   Construct polygon(s) for a sector.
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

#include "gloom/world/sectorpolygonizer.h"

#include <de/set.h>

using namespace de;

namespace gloom {

using namespace geo;

DE_PIMPL(SectorPolygonizer)
{
    struct Contour
    {
        List<ID> lines;
        Set<ID>  hasPoints;
        Polygon  polygon;
        int      parent = -1;

        Contour(ID line = 0)
        {
            if (line) lines << line;
        }

        Contour(const Contour &other)
        {
            *this = other;
        }

        Contour &operator=(const Contour &other)
        {
            lines   = other.lines;
            polygon = other.polygon;
            parent  = other.parent;
            hasPoints.clear();
            for (auto id : other.hasPoints) hasPoints << id;
            return *this;
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
            for (ID id : lines)
            {
                const auto &line    = map.line(id);
                const ID    pointId = line.startPointForSector(currentSector);
                polygon.points << Polygon::Point{map.point(pointId).coord, pointId};
            }
            update();
        }

        bool isInside(const Contour &other) const
        {
            return polygon.isInsideOf(other.polygon);
        }

        int findSharedPoint(const Contour &other) const
        {
            for (int i = 0; i < polygon.points.sizei(); ++i)
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
            for (int i = 0; i < polygon.points.sizei(); ++i)
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
                   String::join(
                       de::map<StringList>(lines, [](ID id) { return String::asText(id, 16); }),
                       " ") +
                   ")";
        }
    };

    Map &            map;
    ID               currentSector;
    List<ID>      boundaryLines;
    List<Contour> contours;

    Impl(Public *i, Map &map) : Base(i), map(map)
    {}

    static bool intersectsAnyLine(const Line2d &line, const List<Contour> &contours)
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

    /// Determines how deeply a contour is nested.
    int contourDepth(const Contour &cont) const
    {
        int depth = 0;
        for (const Contour *i = &cont; i->parent != -1; depth++)
        {
            i = &contours[i->parent];
        }
        return depth;
    }

    int countLinesContactingPoint(ID pointId) const
    {
        int count = 0;
        for (const Contour &cont : contours)
        {
            for (ID lineId : cont.lines)
            {
                const auto &line = map.line(lineId);
                if (line.points[0] == pointId || line.points[1] == pointId)
                {
                    debug("\tline %x contacting point %x", lineId, pointId);
                    count++;
                }
            }
        }
        return count;
    }

    void polygonize()
    {
        using namespace std;

        List<ID> remainingLines = boundaryLines;

        // Initialize with one contour.
        contours = {Contour{remainingLines.takeLast()}};

        // Each line belongs to exactly one contour.
        while (!remainingLines.isEmpty())
        {
            const auto oldSize = remainingLines.size();

            // Let's see if any of the lines fits on the existing contours.
            for (auto iter = remainingLines.begin(); iter != remainingLines.end(); )
            {
                bool erased = false;
                for (Contour &cont : contours)
                {
                    if (cont.tryExtend(*iter, map, currentSector))
                    {
                        iter = remainingLines.erase(iter);
                        erased = true;
                        break;
                    }
                }
                if (!erased) ++iter;
            }

            if (oldSize == remainingLines.size())
            {
                // None of the existing contours could be extended.
                contours << Contour{remainingLines.takeLast()};
            }
        }

        // Initialize polygons from the sorted contour points.
        for (auto &cont : contours)
        {
            if (cont.lines.size() >= 3)
            {
                cont.makePolygon(map, currentSector);
            }
            else
            {
                debug("Ignoring contour %li (size: %zu, closed: %i)",
                      &cont - &contours.at(0),
                      cont.lines.size(),
                      cont.isClosed(map, currentSector));
                cont.clear();
            }
        }

        // Some contours may share points with other contours. Let's see if can get them
        // merged together.
        for (int i = 0; i < contours.sizei(); ++i)
        {
            for (int j = 0; j < contours.sizei(); ++j)
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

                    debug("Contours %i and %i have a shared point %i/%i", i, j, hostIdx, graftIdx);
                    debug("   Host: %s", host.polygon.asText().c_str());
                    debug("   Graft: %s", graft.polygon.asText().c_str());

                    host.polygon.points = joined;
                    host.update();
                    graft.clear();

                    debug("   Result: %s", host.polygon.asText().c_str());
                }
            }
        }

        // Determine the containment hierarchy.
        for (int i = 0; i < contours.sizei(); ++i)
        {
            int parent = -1;
            for (int j = 0; j < contours.sizei(); ++j)
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

        for (int i = 0; i < contours.sizei(); ++i)
        {
            debug("- contour %i : %s : closed: %s parent: %i",
                  i,
                  contours[i].polygon.asText().c_str(),
                  DE_BOOL_YESNO(contours[i].isClosed(map, currentSector)),
                  contours[i].parent);
        }

        for (int i = 0; i < contours.sizei(); ++i)
        {
            if (contours[i].parent == -1 && !contours[i].polygon.isClockwiseWinding())
            {
                debug("Ignoring top-level contour %i due to the wrong winding", i);
                contours[i].clear();
            }
        }

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
        for (int outerIndex = 0; outerIndex < contours.sizei(); ++outerIndex)
        {
            Contour &outer = contours[outerIndex];

            const List<Contour> holes =
                filter(contours, [outerIndex](const Contour &c) { return c.parent == outerIndex; });

            for (int innerIndex = 0; innerIndex < contours.sizei(); ++innerIndex)
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
                    List<ConnectorCandidate> candidates;

                    for (int k = 0; k < inner.polygon.size(); ++k)
                    {
                        for (int j = 0; j < outer.polygon.size(); ++j)
                        {
                            const geo::Line2d connector{outer.polygon.at(j),
                                                        inner.polygon.at(k)};

                            if (!intersectsAnyLine(connector, holes + List<Contour>({outer})))
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
                    for (const auto &connector : candidates)
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
                        debug("Failed to join inner contour %i to its parent %i",
                              innerIndex,
                              outerIndex);
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
                debug("Countour %li is not closed, finding the gap...", &cont - &contours[0]);
                debug("   %s", cont.asText().c_str());

                bool modified = false;
                for (int i = 0; i < cont.polygon.size(); ++i)
                {
                    const ID startPoint = cont.polygon.pointAt(i).id;
                    const ID endPoint   = cont.polygon.pointAt(i + 1).id;

                    if (!cont.hasLineWithPoints(map, startPoint, endPoint))
                    {
                        debug("  Line %i-%i (%x...%x) has no corresponding line",
                              i,
                              mod(i + 1, cont.polygon.size()),
                              startPoint,
                              endPoint);

                        for (int j = 0; j < cont.size(); ++j)
                        {
                            debug("    point %i (%x) has %i contacts in sector %i",
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
                            debug("  Removed fringe point: %s", cont.polygon.asText().c_str());
                        }
                        break;
                    }
                }
                if (!modified && cont.polygon.size() > 0)
                {
                    debug("  Contour could not be closed!");
                    break; // Hmm.
                }
            }

            if (cont.size() < 3)
            {
                debug("  Removing contour with %i points", cont.size());
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
                        debug("  Removing zero-area loop %x..%x",
                              poly.points[i].id,
                              poly.pointAt(i + 1).id);
                        poly.points.erase(poly.points.begin() + i,
                                          poly.points.begin() + i + 2);
                        modified = true;
                        i = -1; // restart
                    }
                }
                if (modified) cont.update();
            }
        }

        Sector &sector = map.sector(currentSector);

        // Remove walls outside the sector polygon.
        for (dsize i = 0; i < sector.walls.size(); ++i)
        {
            const auto mapLine = map.line(sector.walls[i]);
            bool       inPoly  = false;
            for (const Contour &cont : contours)
            {
                 if (cont.hasPoints.contains(mapLine.points[0]) &&
                     cont.hasPoints.contains(mapLine.points[1]))
                 {
                     inPoly = true;
                 }
            }
            if (!inPoly)
            {
                debug("  Sector line %x is not inside the contours", sector.walls[i]);
                auto &line = map.line(sector.walls[i]);
                line.surfaces[line.sectorSide(currentSector)].sector = 0;
                sector.walls.removeAt(i--);
            }
        }

        for (duint i = 0; i < contours.size(); ++i)
        {
            debug("- contour %u : %i ", i, contours[i].polygon.size());
            for (const auto &pp : contours[i].polygon.points)
                debug("    %f, %f : %i", pp.pos.x, pp.pos.y, pp.id);
        }

        // The remaining contours are now disjoint and can be used for plane triangulation.
        IDList &points = map.sector(currentSector).points;
        for (const auto &cont : contours)
        {
            if (cont.polygon.size())
            {
                if (!points.isEmpty()) points << 0; // Separator.

                for (const auto &pp : cont.polygon.points)
                {
                    DE_ASSERT(points.isEmpty() || points.back() != pp.id);
                    points << pp.id;
                }
            }
        }
    }
};

SectorPolygonizer::SectorPolygonizer(Map &map)
    : d(new Impl(this, map))
{}

void SectorPolygonizer::polygonize(ID sector, const List<ID> &boundaryLines)
{
    d->currentSector = sector;
    d->boundaryLines = boundaryLines;
    d->polygonize();
}

} // namespace gloom
