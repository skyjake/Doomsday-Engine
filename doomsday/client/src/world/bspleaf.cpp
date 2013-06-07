/** @file world/bspleaf.cpp World BSP Leaf.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <cmath> // fmod

#include <QSet>
#include <QtAlgorithms>

#include <de/memoryzone.h>

#include <de/Log>

#include "Polyobj"
#include "Sector"
#include "Segment"

#ifdef __CLIENT__
#  include "render/rend_bias.h"
#endif

#include "world/bspleaf.h"

using namespace de;

/// Compute the area of a triangle defined by three 2D point vectors.
ddouble triangleArea(Vector2d const &v1, Vector2d const &v2, Vector2d const &v3)
{
    Vector2d a = v2 - v1;
    Vector2d b = v3 - v1;
    return (a.x * b.y - b.x * a.y) / 2;
}

DENG2_PIMPL(BspLeaf)
{
    typedef QSet<Mesh *> Meshes;

    /// Convex polygon geometry assigned to the BSP leaf (owned).
    QScopedPointer<Mesh> polygon;

    /// Additional meshes assigned to the BSP leaf (owned).
    Meshes extraMeshes;

    /// Clockwise ordering of the line segments from the primary polygon.
    Segments clockwiseSegments;

    /// Set to @c true when the clockwise segment list needs updating.
    bool needUpdateClockwiseSegments;

    /// All line segments in the BSP leaf from all polygons.
    Segments allSegments;

    /// Set to @c true when the all segment list need updating.
    bool needUpdateAllSegments;

    /// Offset to align the top left of materials in the built geometry to the
    /// map coordinate space grid.
    Vector2d worldGridOffset;

    /// Sector attributed to the leaf. @note can be @c 0 (degenerate!).
    Sector *sector;

    /// First Polyobj in the leaf. Can be @c 0 (none).
    Polyobj *polyobj;

#ifdef __CLIENT__

    /// Half-edge whose vertex to use as the base for a trifan.
    /// If @c 0 the center point will be used instead.
    HEdge *fanBase;

    bool needUpdateFanBase; ///< @c true= need to rechoose a fan base half-edge.

    /// Frame number of last R_AddSprites.
    int addSpriteCount;

    /// Sector::planeCount() size.
    BiasSurface **biasSurfaces;

#endif // __CLIENT__

    /// Used by legacy algorithms to prevent repeated processing.
    int validCount;

    Instance(Public *i, Sector *sector = 0)
        : Base(i),
          needUpdateClockwiseSegments(false),
          needUpdateAllSegments(false),
          sector(sector),
          polyobj(0),
#ifdef __CLIENT__
          fanBase(0),
          needUpdateFanBase(true),
          addSpriteCount(0),
          biasSurfaces(0),
#endif
          validCount(0)
    {}

    ~Instance()
    {
        qDeleteAll(extraMeshes);

#ifdef __CLIENT__
        // Destroy the biassurfaces for each plane.
        if(biasSurfaces)
        {
            for(int i = 0; i < sector->planeCount(); ++i)
            {
                SB_DestroySurface(biasSurfaces[i]);
            }
            Z_Free(biasSurfaces);
        }
#endif // __CLIENT__
    }

    void updateClockwiseSegments()
    {
        needUpdateClockwiseSegments = false;

        clockwiseSegments.clear();

        if(polygon.isNull())
            return;

        Face const &face = self.face();
#ifdef DENG2_QT_4_7_OR_NEWER
        clockwiseSegments.reserve(face.hedgeCount());
#endif

        HEdge *hedge = face.hedge();
        do
        {
            if(MapElement *elem = hedge->mapElement())
            {
                clockwiseSegments.append(elem->castTo<Segment>());
            }
        } while((hedge = &hedge->next()) != face.hedge());

#ifdef DENG_DEBUG
        // See if we received a partial geometry...
        {
            int discontinuities = 0;
            HEdge *hedge = face.hedge();
            do
            {
                if(hedge->next().origin() != hedge->twin().origin())
                {
                    discontinuities++;
                }
            } while((hedge = &hedge->next()) != face.hedge());

            if(discontinuities)
            {
                LOG_WARNING("Face geometry for BSP leaf [%p] at %s in sector %i "
                            "is not contiguous (%i gaps/overlaps).\n%s")
                    << de::dintptr(&self)
                    << face.center().asText()
                    << sector->indexInArchive()
                    << discontinuities
                    << face.description();
            }
        }
#endif
    }

    void updateAllSegments()
    {
        needUpdateAllSegments = false;

        if(needUpdateClockwiseSegments)
        {
            updateClockwiseSegments();
        }

        allSegments.clear();

        int numSegments = clockwiseSegments.count();
        foreach(Mesh *mesh, extraMeshes)
        {
            numSegments += mesh->hedgeCount();
        }
#ifdef DENG2_QT_4_7_OR_NEWER
        allSegments.reserve(numSegments);
#endif

        // Populate the segment list.
        allSegments.append(clockwiseSegments);

        foreach(Mesh *mesh, extraMeshes)
        foreach(HEdge *hedge, mesh->hedges())
        {
            if(MapElement *elem = hedge->mapElement())
            {
                allSegments.append(elem->castTo<Segment>());
            }
        }
    }

#ifdef __CLIENT__

    /**
     * Determine the half-edge whose vertex is suitable for use as the center point
     * of a trifan primitive.
     *
     * Note that we do not want any overlapping or zero-area (degenerate) triangles.
     *
     * @par Algorithm
     * <pre>For each vertex
     *    For each triangle
     *        if area is not greater than minimum bound, move to next vertex
     *    Vertex is suitable
     * </pre>
     *
     * If a vertex exists which results in no zero-area triangles it is suitable for
     * use as the center of our trifan. If a suitable vertex is not found then the
     * center of BSP leaf should be selected instead (it will always be valid as
     * BSP leafs are convex).
     */
    void chooseFanBase()
    {
#define MIN_TRIANGLE_EPSILON  (0.1) ///< Area

        Face const &face = self.face();
        HEdge *firstNode = face.hedge();

        fanBase = firstNode;

        if(face.hedgeCount() > 3)
        {
            // Splines with higher vertex counts demand checking.
            Vertex const *base, *a, *b;

            // Search for a good base.
            do
            {
                HEdge *other = firstNode;

                base = &fanBase->vertex();
                do
                {
                    // Test this triangle?
                    if(!(fanBase != firstNode && (other == fanBase || other == &fanBase->prev())))
                    {
                        a = &other->vertex();
                        b = &other->next().vertex();

                        if(de::abs(triangleArea(base->origin(), a->origin(), b->origin())) <= MIN_TRIANGLE_EPSILON)
                        {
                            // No good. We'll move on to the next vertex.
                            base = 0;
                        }
                    }

                    // On to the next triangle.
                } while(base && (other = &other->next()) != firstNode);

                if(!base)
                {
                    // No good. Select the next vertex and start over.
                    fanBase = &fanBase->next();
                }
            } while(!base && fanBase != firstNode);

            // Did we find something suitable?
            if(!base) // No.
            {
                fanBase = 0;
            }
        }
        //else Implicitly suitable (or completely degenerate...).

        needUpdateFanBase = false;

#undef MIN_TRIANGLE_EPSILON
    }

#endif // __CLIENT__
};

BspLeaf::BspLeaf(Sector *sector)
    : MapElement(DMU_BSPLEAF), d(new Instance(this, sector))
{
#ifdef __CLIENT__
    _shadows = 0;
    zap(_reverb);
#endif
}

bool BspLeaf::hasFace() const
{
    return !d->polygon.isNull();
}

Face const &BspLeaf::face() const
{
    if(!d->polygon.isNull())
    {
        return *d->polygon->firstFace();
    }
    /// @throw MissingFaceError Attempted with no face assigned.
    throw MissingFaceError("BspLeaf::face", "No face is assigned");
}

void BspLeaf::assignPoly(Mesh *newPoly)
{
    if(newPoly && !newPoly->firstFace()->isConvex())
    {
        /// @throw InvalidPolygonError Attempted to assign a non-convex polygon.
        throw InvalidPolygonError("BspLeaf::assignPoly", "Non-convex polygons cannot be assigned");
    }

    // Assign the new polygon (if any).
    d->polygon.reset(newPoly);

    if(newPoly)
    {
        // Attribute the new face geometry to "this" BSP leaf.
        newPoly->firstFace()->setMapElement(this);

        // We'll need to update segment lists.
        d->needUpdateClockwiseSegments = true;
        d->needUpdateAllSegments = true;

        // Update the world grid offset.
        d->worldGridOffset = Vector2d(fmod(newPoly->firstFace()->aaBox().minX, 64),
                                      fmod(newPoly->firstFace()->aaBox().maxY, 64));
    }
    else
    {
        d->worldGridOffset = Vector2d(0, 0);
    }
}

void BspLeaf::assignExtraMesh(Mesh &newMesh)
{
    int const sizeBefore = d->extraMeshes.size();

    d->extraMeshes.insert(&newMesh);

    if(d->extraMeshes.size() != sizeBefore)
    {
        LOG_AS("BspLeaf");
        LOG_DEBUG("Assigned extra mesh to leaf [%p].") << de::dintptr(this);

        // Attribute all faces to "this" BSP leaf.
        foreach(Face *face, newMesh.faces())
        {
            face->setMapElement(this);
        }

        // We'll need to update the all segment list.
        d->needUpdateAllSegments = true;
    }
}

BspLeaf::Segments const &BspLeaf::clockwiseSegments() const
{
    if(d->needUpdateClockwiseSegments)
    {
        d->updateClockwiseSegments();
    }
    return d->clockwiseSegments;
}

BspLeaf::Segments const &BspLeaf::allSegments() const
{
    if(d->needUpdateAllSegments)
    {
        d->updateAllSegments();
    }
    return d->allSegments;
}

Vector2d const &BspLeaf::worldGridOffset() const
{
    return d->worldGridOffset;
}

bool BspLeaf::hasSector() const
{
    return d->sector != 0;
}

Sector &BspLeaf::sector() const
{
    if(d->sector)
    {
        return *d->sector;
    }
    /// @throw MissingSectorError Attempted with no sector attributed.
    throw MissingSectorError("BspLeaf::sector", "No sector is attributed");
}

void BspLeaf::setSector(Sector *newSector)
{
    d->sector = newSector;
}

bool BspLeaf::hasWorldVolume(bool useVisualHeights) const
{
    if(isDegenerate()) return false;
    if(!hasSector()) return false;

    coord_t const floorHeight = useVisualHeights? d->sector->floor().visHeight() : d->sector->floor().height();
    coord_t const ceilHeight  = useVisualHeights? d->sector->ceiling().visHeight() : d->sector->ceiling().height();

    return (ceilHeight - floorHeight > 0);
}

Polyobj *BspLeaf::firstPolyobj() const
{
    return d->polyobj;
}

void BspLeaf::setFirstPolyobj(Polyobj *newPolyobj)
{
    d->polyobj = newPolyobj;
}

int BspLeaf::validCount() const
{
    return d->validCount;
}

void BspLeaf::setValidCount(int newValidCount)
{
    d->validCount = newValidCount;
}

#ifdef __CLIENT__

HEdge *BspLeaf::fanBase() const
{
    if(d->needUpdateFanBase)
        d->chooseFanBase();
    return d->fanBase;
}

int BspLeaf::numFanVertices() const
{
    // Are we to use one of the half-edge vertexes as the fan base?
    if(!hasFace()) return 0;
    return face().hedgeCount() + (fanBase()? 0 : 2);
}

BiasSurface &BspLeaf::biasSurface(int groupId)
{
    DENG2_ASSERT(d->sector != 0);
    if(groupId >= 0 && groupId < d->sector->planeCount())
    {
        DENG2_ASSERT(d->biasSurfaces != 0 && d->biasSurfaces[groupId] != 0);
        return *d->biasSurfaces[groupId];
    }
    /// @throw InvalidGeometryGroupError Attempted with an invalid geometry group id.
    throw UnknownGeometryGroupError("BspLeaf::biasSurface", QString("Invalid group id %1").arg(groupId));
}

void BspLeaf::setBiasSurface(int groupId, BiasSurface *biasSurface)
{
    LOG_AS("BspLeaf::setBiasSurface");

    if(isDegenerate())
        LOG_TRACE("Adding a BiasSurface to a degenerate BSP leaf??");

    if(!d->sector)
        /// @throw MissingSectorError Attempted with no sector attributed.
        throw MissingSectorError("BspLeaf::setBiasSurface", "No sector is attributed");

    if(groupId < 0 || groupId >= d->sector->planeCount())
        /// @throw InvalidGeometryGroupError Attempted with an invalid geometry group id.
        throw UnknownGeometryGroupError("BspLeaf::setBiasSurface", QString("Invalid group id %1").arg(groupId));

    if(!d->biasSurfaces)
    {
        d->biasSurfaces = (BiasSurface **) Z_Calloc(d->sector->planeCount() * sizeof(BiasSurface *), PU_MAP, 0);
    }
    else if(d->biasSurfaces[groupId])
    {
        SB_DestroySurface(d->biasSurfaces[groupId]);
    }

    d->biasSurfaces[groupId] = biasSurface;
}

ShadowLink *BspLeaf::firstShadowLink() const
{
    return _shadows;
}

int BspLeaf::addSpriteCount() const
{
    return d->addSpriteCount;
}

void BspLeaf::setAddSpriteCount(int newFrameCount)
{
    d->addSpriteCount = newFrameCount;
}

#endif // __CLIENT__

int BspLeaf::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_SECTOR:
        DMU_GetValue(DMT_BSPLEAF_SECTOR, &d->sector, &args, 0);
        break;
    default:
        return MapElement::property(args);
    }
    return false; // Continue iteration.
}
