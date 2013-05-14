/** @file bspleaf.cpp World Map BSP Leaf.
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

#include <de/mathutil.h>
#include <de/memoryzone.h>

#include <de/Log>

#include "Face"
#include "HEdge"
#include "Polyobj"
#include "Sector"
#include "Vertex"

#ifdef __CLIENT__
#  include "render/rend_bias.h"
#endif

#include "map/bspleaf.h"

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
    /// Vertex bounding box in the map coordinate space.
    AABoxd aaBox;

    /// Center of vertices.
    Vector2d center;

    /// Offset to align the top left of materials in the built geometry to the
    /// map coordinate space grid.
    Vector2d worldGridOffset;

    /// Sector attributed to the leaf. @note can be @c 0 (degenerate!).
    Sector *sector;

    /// First Polyobj in the leaf. Can be @c 0 (none).
    Polyobj *polyobj;

#ifdef __CLIENT__

    /// HEdge whose vertex to use as the base for a trifan.
    /// If @c 0 the center point will be used instead.
    HEdge *fanBase;

    bool needUpdateFanBase; ///< @c true= need to rechoose a fan base half-edge.

    /// Frame number of last R_AddSprites.
    int addSpriteCount;

#endif // __CLIENT__

    /// Used by legacy algorithms to prevent repeated processing.
    int validCount;

    Instance(Public *i)
        : Base(i),
          sector(0),
          polyobj(0),
#ifdef __CLIENT__
          fanBase(0),
          needUpdateFanBase(true),
          addSpriteCount(0),
#endif
          validCount(0)
    {}

#ifdef __CLIENT__

    /**
     * Determine the HEdge from whose vertex is suitable for use as the center point
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

        HEdge *firstNode = self.firstHEdge();

        fanBase = firstNode;

        if(self.hedgeCount() > 3)
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

BspLeaf::BspLeaf() : MapElement(DMU_BSPLEAF), Face(), d(new Instance(this))
{
#ifdef __CLIENT__
    _shadows = 0;
    _bsuf = 0;
    std::memset(_reverb, 0, sizeof(_reverb));
#endif
}

BspLeaf::~BspLeaf()
{
#ifdef __CLIENT__
    if(_bsuf)
    {
        for(int i = 0; i < d->sector->planeCount(); ++i)
        {
            SB_DestroySurface(_bsuf[i]);
        }
        Z_Free(_bsuf);
    }
#endif // __CLIENT__
}

bool BspLeaf::hasDegenerateFace() const
{
    return hedgeCount() < 3;
}

AABoxd const &BspLeaf::aaBox() const
{
    return d->aaBox;
}

void BspLeaf::updateAABox()
{
    V2d_Set(d->aaBox.min, DDMAXFLOAT, DDMAXFLOAT);
    V2d_Set(d->aaBox.max, DDMINFLOAT, DDMINFLOAT);

    HEdge *base = firstHEdge();
    if(!base) return; // Very odd...

    HEdge *hedgeIt = base;
    V2d_InitBoxXY(d->aaBox.arvec2, hedgeIt->origin().x, hedgeIt->origin().y);

    while((hedgeIt = &hedgeIt->next()) != base)
    {
        V2d_AddToBoxXY(d->aaBox.arvec2, hedgeIt->origin().x, hedgeIt->origin().y);
    }
}

Vector2d const &BspLeaf::center() const
{
    return d->center;
}

void BspLeaf::updateCenter()
{
    // The middle is the center of our AABox.
    d->center = Vector2d(d->aaBox.min) + (Vector2d(d->aaBox.max) - Vector2d(d->aaBox.min)) / 2;
}

Vector2d const &BspLeaf::worldGridOffset() const
{
    return d->worldGridOffset;
}

void BspLeaf::updateWorldGridOffset()
{
    d->worldGridOffset = Vector2d(fmod(d->aaBox.minX, 64), fmod(d->aaBox.maxY, 64));
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
    if(hasDegenerateFace()) return false;
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
    return hedgeCount() + (fanBase()? 0 : 2);
}

BiasSurface &BspLeaf::biasSurfaceForGeometryGroup(int groupId)
{
    DENG2_ASSERT(d->sector != 0);
    if(groupId >= 0 && groupId < d->sector->planeCount())
    {
        DENG2_ASSERT(_bsuf != 0 && _bsuf[groupId] != 0);
        return *_bsuf[groupId];
    }
    /// @throw InvalidGeometryGroupError Attempted with an invalid geometry group id.
    throw UnknownGeometryGroupError("BspLeaf::biasSurfaceForGeometryGroup", QString("Invalid group id %1").arg(groupId));
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

#ifdef DENG_DEBUG
void BspLeaf::printFaceGeometry() const
{
    HEdge const *base = firstHEdge();
    if(!base) return;

    HEdge const *hedgeIt = base;
    do
    {
        HEdge const &hedge = *hedgeIt;
        coord_t angle = M_DirectionToAngleXY(hedge.origin().x - d->center.x,
                                             hedge.origin().y - d->center.y);

        LOG_DEBUG("  half-edge %p: Angle %1.6f %s -> %s")
            << de::dintptr(&hedge) << angle
            << hedge.origin().asText() << hedge.twin().origin().asText();

    } while((hedgeIt = &hedgeIt->next()) != base);
}
#endif
