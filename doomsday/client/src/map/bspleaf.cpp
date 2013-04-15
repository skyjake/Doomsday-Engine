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

#include <de/Log>

#include "de_base.h"
#include "m_misc.h" // M_TriangleArea()

#include "map/hedge.h"
#include "map/polyobj.h"
#include "map/sector.h"
#include "map/vertex.h"

#include "map/bspleaf.h"

using namespace de;

DENG2_PIMPL(BspLeaf)
{
    /// Vertex bounding box in the map coordinate space.
    AABoxd aaBox;

    /// Center of vertices.
    coord_t center[2];

    /// Offset to align the top left of materials in the built geometry to the
    /// map coordinate space grid.
    coord_t worldGridOffset[2];

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

    /// Original index in the archived map.
    uint origIndex;

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
          origIndex(0),
          validCount(0)
    {
        std::memset(center, 0, sizeof(center));
        std::memset(worldGridOffset, 0, sizeof(worldGridOffset));
    }

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

        HEdge *firstNode = self._hedge;

        fanBase = firstNode;

        if(self.hedgeCount() > 3)
        {
            // Splines with higher vertex counts demand checking.
            Vertex const *base, *a, *b;

            // Search for a good base.
            do
            {
                HEdge *other = firstNode;

                base = &fanBase->v1();
                do
                {
                    // Test this triangle?
                    if(!(fanBase != firstNode && (other == fanBase || other == &fanBase->prev())))
                    {
                        a = &other->from();
                        b = &other->next().from();

                        if(M_TriangleArea(base->origin(), a->origin(), b->origin()) <= MIN_TRIANGLE_EPSILON)
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
                fanBase = NULL;
            }
        }
        //else Implicitly suitable (or completely degenerate...).

        needUpdateFanBase = false;

#undef MIN_TRIANGLE_EPSILON
    }

#endif // __CLIENT__
};

BspLeaf::BspLeaf()
    : MapElement(DMU_BSPLEAF), d(new Instance(this))
{
    _hedge = 0;
    _hedgeCount = 0;
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
        for(uint i = 0; i < d->sector->planeCount(); ++i)
        {
            SB_DestroySurface(_bsuf[i]);
        }
        Z_Free(_bsuf);
    }
#endif // __CLIENT__

    // Clear the HEdges.
    if(_hedge)
    {
        HEdge *he = _hedge;
        if(he->_next == he)
        {
            delete he;
        }
        else
        {
            // Break the ring, if linked.
            if(he->_prev)
            {
                he->_prev->_next = NULL;
            }

            while(he)
            {
                HEdge *next = he->_next;
                delete he;
                he = next;
            }
        }
    }
}

AABoxd const &BspLeaf::aaBox() const
{
    return d->aaBox;
}

void BspLeaf::updateAABox()
{
    V2d_Set(d->aaBox.min, DDMAXFLOAT, DDMAXFLOAT);
    V2d_Set(d->aaBox.max, DDMINFLOAT, DDMINFLOAT);

    if(!_hedge) return; // Very odd...

    HEdge *hedgeIt = _hedge;
    V2d_InitBox(d->aaBox.arvec2, hedgeIt->v1Origin());

    while((hedgeIt = &hedgeIt->next()) != _hedge)
    {
        V2d_AddToBox(d->aaBox.arvec2, hedgeIt->v1Origin());
    }
}

vec2d_t const &BspLeaf::center() const
{
    return d->center;
}

void BspLeaf::updateCenter()
{
    // The middle is the center of our AABox.
    d->center[VX] = d->aaBox.minX + (d->aaBox.maxX - d->aaBox.minX) / 2;
    d->center[VY] = d->aaBox.minY + (d->aaBox.maxY - d->aaBox.minY) / 2;
}

vec2d_t const &BspLeaf::worldGridOffset() const
{
    return d->worldGridOffset;
}

void BspLeaf::updateWorldGridOffset()
{
    d->worldGridOffset[VX] = fmod(d->aaBox.minX, 64);
    d->worldGridOffset[VY] = fmod(d->aaBox.maxY, 64);
}

HEdge *BspLeaf::firstHEdge() const
{
    return _hedge;
}

uint BspLeaf::hedgeCount() const
{
    return _hedgeCount;
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

Polyobj *BspLeaf::firstPolyobj() const
{
    return d->polyobj;
}

void BspLeaf::setFirstPolyobj(Polyobj *newPolyobj)
{
    d->polyobj = newPolyobj;
}

uint BspLeaf::origIndex() const
{
    return d->origIndex;
}

void BspLeaf::setOrigIndex(uint newOrigIndex)
{
    d->origIndex = newOrigIndex;
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

biassurface_t &BspLeaf::biasSurfaceForGeometryGroup(uint groupId)
{
    DENG2_ASSERT(d->sector != 0);
    if(groupId <= d->sector->planeCount())
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
    case DMU_HEDGE_COUNT: {
        int val = int( _hedgeCount );
        DMU_GetValue(DDVT_INT, &val, &args, 0);
        break; }
    default:
        /// @throw UnknownPropertyError  The requested property does not exist.
        throw UnknownPropertyError("BspLeaf::property", QString("Property '%1' is unknown").arg(DMU_Str(args.prop)));
    }
    return false; // Continue iteration.
}

int BspLeaf::setProperty(setargs_t const &args)
{
    /// @throw WritePropertyError  The requested property is not writable.
    throw WritePropertyError("Vertex::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
}
