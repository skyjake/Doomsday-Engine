/** @file factory.cpp  Factory for world objects.
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

#include "doomsday/world/factory.h"

// For Polyobj constructor:
#include "doomsday/world/polyobj.h"
#include "doomsday/doomsdayapp.h"
#include <de/legacy/memory.h>

namespace world {

using namespace de;

static std::function<ConvexSubspace *(mesh::Face &, BspLeaf *)>     convexSubspaceCtor;
static std::function<Line *(Vertex &, Vertex &, int, Sector *, Sector *)> lineCtor;
static std::function<LineSide *(Line &, Sector *)>                  lineSideCtor;
static std::function<LineSideSegment *(LineSide &, mesh::HEdge &)>  lineSideSegmentCtor;
static std::function<Map *()>                                       mapCtor;
static std::function<Material *(MaterialManifest &)>                materialCtor;
static std::function<MobjThinkerData *(const Id &)>                 mobjThinkerDataCtor;
static std::function<Plane *(Sector &, const Vec3f &, double)>      planeCtor;
static std::function<PolyobjData *()>                               polyobjDataCtor;
static std::function<Sky *(const defn::Sky *)>                      skyCtor;
static Factory::SubsectorConstructor                                subsectorCtor;
static std::function<Surface *(MapElement &, float, const Vec3f &)> surfaceCtor;
static std::function<Vertex *(mesh::Mesh &, const de::Vec2d &)>     vertexCtor;

void Factory::setConvexSubspaceConstructor(const std::function<ConvexSubspace *(mesh::Face &, BspLeaf *)> &ctor)
{
    convexSubspaceCtor = ctor;
}

ConvexSubspace *Factory::newConvexSubspace(mesh::Face &face, BspLeaf *leaf)
{
    DE_ASSERT(convexSubspaceCtor);
    return convexSubspaceCtor(face, leaf);
}

void Factory::setLineConstructor(const std::function<Line *(Vertex &, Vertex &, int, Sector *, Sector *)> &ctor)
{
    lineCtor = ctor;
}

Line *Factory::newLine(Vertex &from, Vertex &to, int flags, Sector *frontSector, Sector *backSector)
{
    DE_ASSERT(lineCtor);
    return lineCtor(from, to, flags, frontSector, backSector);
}

void Factory::setLineSideConstructor(const std::function<LineSide *(Line &, Sector *)> &ctor)
{
    lineSideCtor = ctor;
}

LineSide *Factory::newLineSide(Line &line, Sector *sector)
{
    DE_ASSERT(lineSideCtor);
    return lineSideCtor(line, sector);
}

void Factory::setLineSideSegmentConstructor(const std::function<LineSideSegment *(LineSide &, mesh::HEdge &)> &ctor)
{
    lineSideSegmentCtor = ctor;
}

LineSideSegment *Factory::newLineSideSegment(LineSide &side, mesh::HEdge &hedge)
{
    DE_ASSERT(lineSideSegmentCtor);
    return lineSideSegmentCtor(side, hedge);
}

void Factory::setMapConstructor(const std::function<Map *()> &ctor)
{
    mapCtor = ctor;
}

Map *Factory::newMap()
{
    DE_ASSERT(mapCtor);
    return mapCtor();
}

void Factory::setMaterialConstructor(const std::function<Material *(MaterialManifest &)> &ctor)
{
    materialCtor = ctor;
}

Material *Factory::newMaterial(MaterialManifest &m)
{
    DE_ASSERT(materialCtor);
    return materialCtor(m);
}

void Factory::setMobjThinkerDataConstructor(const std::function<MobjThinkerData *(const Id &)> &ctor)
{
    mobjThinkerDataCtor = ctor;
}

MobjThinkerData *Factory::newMobjThinkerData(const de::Id &id)
{
    DE_ASSERT(mobjThinkerDataCtor);
    return mobjThinkerDataCtor(id);
}

void Factory::setPlaneConstructor(const std::function<Plane *(Sector &, const Vec3f &, double)> &ctor)
{
    planeCtor = ctor;
}

Plane *Factory::newPlane(Sector &sector, const Vec3f &normal, double height)
{
    DE_ASSERT(planeCtor);
    return planeCtor(sector, normal, height);
}

void Factory::setPolyobjDataConstructor(const std::function<PolyobjData *()> &ctor)
{
    polyobjDataCtor = ctor;
}

PolyobjData *Factory::newPolyobjData()
{
    DE_ASSERT(polyobjDataCtor);
    return polyobjDataCtor();
}

void Factory::setSkyConstructor(const std::function<Sky *(const defn::Sky *)> &ctor)
{
    skyCtor = ctor;
}

Sky *Factory::newSky(const defn::Sky *def)
{
    DE_ASSERT(skyCtor);
    return skyCtor(def);
}

void Factory::setSubsectorConstructor(const SubsectorConstructor &ctor)
{
    subsectorCtor = ctor;
}

Subsector *Factory::newSubsector(const de::List<ConvexSubspace *> &subspaces)
{
    DE_ASSERT(subsectorCtor);
    return subsectorCtor(subspaces);
}

void Factory::setSurfaceConstructor(const std::function<Surface *(MapElement &, float, const Vec3f &)> &ctor)
{
    surfaceCtor = ctor;
}

Surface *Factory::newSurface(MapElement &owner, float opacity, const Vec3f &color)
{
    DE_ASSERT(surfaceCtor);
    return surfaceCtor(owner, opacity, color);
}

struct polyobj_s *Factory::newPolyobj(const de::Vec2d &origin)
{
    /// @todo The app should register their own constructor like with the others.
    const auto &gx = DoomsdayApp::app().plugins().gameExports();
    void *region = M_Calloc(gx.GetInteger(DD_POLYOBJ_SIZE));
    return new (region) Polyobj(origin);
}

void Factory::setVertexConstructor(const std::function<Vertex *(mesh::Mesh &, const de::Vec2d &)> &ctor)
{
    vertexCtor = ctor;
}

Vertex *Factory::newVertex(mesh::Mesh &mesh, const de::Vec2d &origin)
{
    DE_ASSERT(vertexCtor);
    return vertexCtor(mesh, origin);
}

} // namespace world
