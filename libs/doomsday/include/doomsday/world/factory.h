/** @file factory.h  Factory for constructing world objects.
 *
 * @authors Copyright (c) 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#pragma once

#include "../libdoomsday.h"
#include "../defs/sky.h"

#include <de/id.h>
#include <de/list.h>
#include <de/vector.h>

class MobjThinkerData;
struct polyobj_s;

namespace mesh { class Face; class HEdge; class Mesh; }

namespace world {

class BspLeaf;
class ConvexSubspace;
class Line;
class LineSide;
class LineSideSegment;
class Map;
class MapElement;
class Material;
class MaterialManifest;
class Plane;
class PolyobjData;
class Sector;
class Sky;
class Subsector;
class Surface;
class Vertex;

class LIBDOOMSDAY_PUBLIC Factory
{
public:
    using SubsectorConstructor = std::function<Subsector *(const de::List<ConvexSubspace *> &)>;

    static void setConvexSubspaceConstructor(const std::function<ConvexSubspace *(mesh::Face &, BspLeaf *)> &);
    static void setLineConstructor(const std::function<Line *(Vertex &, Vertex &, int, Sector *, Sector *)> &);
    static void setLineSideConstructor(const std::function<LineSide *(Line &, Sector *)> &);
    static void setLineSideSegmentConstructor(const std::function<LineSideSegment *(LineSide &, mesh::HEdge &)> &);
    static void setMapConstructor(const std::function<Map *()> &);
    static void setMaterialConstructor(const std::function<Material *(MaterialManifest &)> &);
    static void setMobjThinkerDataConstructor(const std::function<MobjThinkerData *(const de::Id &)> &);
    static void setPlaneConstructor(const std::function<Plane *(Sector &, const de::Vec3f &, double)> &);
    static void setPolyobjDataConstructor(const std::function<PolyobjData *()> &);
    static void setSkyConstructor(const std::function<Sky *(const defn::Sky *)> &);
    static void setSubsectorConstructor(const SubsectorConstructor &func);
    static void setSurfaceConstructor(const std::function<Surface *(world::MapElement &, float, const de::Vec3f &)> &);
    static void setVertexConstructor(const std::function<Vertex *(mesh::Mesh &, const de::Vec2d &)> &);

    static ConvexSubspace *  newConvexSubspace(mesh::Face &, BspLeaf *);
    static Map *             newMap();
    static Material *        newMaterial(MaterialManifest &);
    static MobjThinkerData * newMobjThinkerData(const de::Id &);
    static Line *            newLine(Vertex &from, Vertex &to, int flags = 0, Sector *frontSector = nullptr, Sector *backSector  = nullptr);
    static LineSide *        newLineSide(Line &line, Sector *sector);
    static LineSideSegment * newLineSideSegment(LineSide &side, mesh::HEdge &hedge);
    static Plane *           newPlane(Sector &sector, const de::Vec3f &normal = de::Vec3f(0, 0, 1), double height = 0);
    static PolyobjData *     newPolyobjData();
    static Sky *             newSky(const defn::Sky *);
    static Subsector *       newSubsector(const de::List<ConvexSubspace *> &subspaces);
    static Surface *         newSurface(world::MapElement &, float = 1.0f, const de::Vec3f & = {1.0f});
    static struct polyobj_s *newPolyobj(const de::Vec2d &);
    static Vertex *          newVertex(mesh::Mesh &, const de::Vec2d & = {});
};

} // namespace world
