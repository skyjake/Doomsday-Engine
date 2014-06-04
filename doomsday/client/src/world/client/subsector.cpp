/** @file subsector.h  Client-side world sector subspace.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#include "client/subsector.h"
#include "world/map.h"
#include "Face"
#include "Surface"
#include "WallEdge"
#include "render/rend_main.h"

namespace de {
namespace internal {
    /// Compute the area of a triangle defined by three 2D point vectors.
    static ddouble triangleArea(Vector2d const &v1, Vector2d const &v2, Vector2d const &v3)
    {
        Vector2d a = v2 - v1;
        Vector2d b = v3 - v1;
        return (a.x * b.y - b.x * a.y) / 2;
    }
}
}

using namespace de;
using namespace internal;

#ifdef __CLIENT__
Subsector::GeometryData::GeometryData(MapElement *mapElement, int geomId)
    : mapElement(mapElement)
    , geomId    (geomId)
{}

void Subsector::GeometryData::applyBiasDigest(BiasDigest &allChanges)
{
    if(!biasSurface.isNull())
    {
        biasSurface->tracker.applyChanges(allChanges);
    }
}

void Subsector::GeometryData::markBiasContributorUpdateNeeded()
{
    if(!biasSurface.isNull())
    {
        biasSurface->tracker.updateAllContributors();
    }
}

void Subsector::GeometryData::markBiasIllumUpdateCompleted()
{
    if(!biasSurface.isNull())
    {
        biasSurface->tracker.markIllumUpdateCompleted();
    }
}

void Subsector::GeometryData::setBiasLastUpdateFrame(uint updateFrame)
{
    if(!biasSurface.isNull())
    {
        biasSurface->lastUpdateFrame = updateFrame;
    }
}
#endif // __CLIENT__

Subsector::Subsector()
    : subspace          (0)
#ifdef __CLIENT__
    , _fanBase          (0)
    , _needUpdateFanBase(true)
#endif
{}

#ifdef __CLIENT__
Subsector::~Subsector()
{
    foreach(GeometryGroup const &group, geomGroups)
    {
        qDeleteAll(group); // Delete all GeometryData.
    }
}
#endif

ConvexSubspace &Subsector::convexSubspace() const
{
    DENG2_ASSERT(subspace != 0);
    return *subspace;
}

void Subsector::setConvexSubspace(ConvexSubspace &newSubspace)
{
    subspace = &newSubspace;
}

#ifdef __CLIENT__
void Subsector::addBiasSurfaceIfMissing(GeometryData &gdata)
{
    if(!gdata.biasSurface.isNull()) return;

    gdata.biasSurface.reset(new GeometryData::BiasSurface);

    // Determine the number of bias illumination points needed (presently
    // we define a 1:1 mapping to geometry vertices).
    int numIllums = 0;
    switch(gdata.mapElement->type())
    {
    case DMU_SUBSPACE: {
        ConvexSubspace &subspace = gdata.mapElement->as<ConvexSubspace>();
        DENG2_ASSERT(gdata.geomId >= 0 && gdata.geomId < subspace.sector().planeCount()); // sanity check
        numIllums = numFanVertices();
        break; }

    case DMU_SEGMENT:
        DENG2_ASSERT(gdata.geomId >= WallEdge::WallMiddle && gdata.geomId <= WallEdge::WallTop); // sanity check
        numIllums = 4;
        break;

    default:
        DENG2_ASSERT(!"Subsector::addBiasSurfaceIfMissing(): Invalid MapElement type");
    }

    if(numIllums > 0)
    {
        gdata.biasSurface->illums.reserve(numIllums);
        for(int i = 0; i < numIllums; ++i)
        {
            gdata.biasSurface->illums << new BiasIllum(&gdata.biasSurface->tracker);
        }
    }
}

Subsector::GeometryData::BiasIllums &Subsector::biasIllums(GeometryData &gdata)
{
    addBiasSurfaceIfMissing(gdata);
    return gdata.biasSurface->illums;
}

BiasTracker &Subsector::biasTracker(GeometryData &gdata)
{
    addBiasSurfaceIfMissing(gdata);
    return gdata.biasSurface->tracker;
}

uint Subsector::lastBiasUpdateFrame(GeometryData &gdata)
{
    addBiasSurfaceIfMissing(gdata);
    return gdata.biasSurface->lastUpdateFrame;
}

void Subsector::chooseFanBase()
{
#define MIN_TRIANGLE_EPSILON  (0.1) ///< Area

    _needUpdateFanBase = false;

    HEdge *firstNode = convexSubspace().poly().hedge();
    _fanBase = firstNode;

    if(convexSubspace().poly().hedgeCount() > 3)
    {
        // Splines with higher vertex counts demand checking.
        Vertex const *base, *a, *b;

        // Search for a good base.
        do
        {
            HEdge *other = firstNode;

            base = &_fanBase->vertex();
            do
            {
                // Test this triangle?
                if(!(_fanBase != firstNode && (other == _fanBase || other == &_fanBase->prev())))
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
                _fanBase = &_fanBase->next();
            }
        } while(!base && _fanBase != firstNode);

        // Did we find something suitable?
        if(!base) // No.
        {
            _fanBase = 0;
        }
    }
    //else Implicitly suitable (or completely degenerate...).

#undef MIN_TRIANGLE_EPSILON
}

HEdge *Subsector::fanBase() const
{
    if(_needUpdateFanBase)
    {
        const_cast<Subsector *>(this)->chooseFanBase();
    }
    return _fanBase;
}

int Subsector::numFanVertices() const
{
    // Are we to use one of the half-edge vertexes as the fan base?
    return convexSubspace().poly().hedgeCount() + (fanBase()? 0 : 2);
}

void Subsector::clearAllSubspaceShards() const
{
    convexSubspace().shards().clear();
}

Subsector::GeometryData *Subsector::geomData(MapElement &mapElement, int geomId, bool canAlloc)
{
    GeometryGroups::iterator foundGroup = geomGroups.find(&mapElement);
    if(foundGroup != geomGroups.end())
    {
        GeometryGroup &geomDatas = *foundGroup;
        GeometryGroup::iterator found = geomDatas.find(geomId);
        if(found != geomDatas.end())
        {
            return *found;
        }
    }

    if(!canAlloc) return 0;

    if(foundGroup == geomGroups.end())
    {
        foundGroup = geomGroups.insert(&mapElement, GeometryGroup());
    }

    return foundGroup->insert(geomId, new GeometryData(&mapElement, geomId)).value();
}

bool Subsector::updateBiasContributorsIfNeeded(GeometryData &gdata)
{
    if(!Rend_BiasContributorUpdatesEnabled())
        return false;

    Map &map = convexSubspace().map();

    uint const lastBiasChangeFrame = map.biasLastChangeOnFrame();
    if(lastBiasUpdateFrame(gdata) == lastBiasChangeFrame)
        return false;

    // Mark the data as having been updated (it will be soon).
    gdata.setBiasLastUpdateFrame(lastBiasChangeFrame);
    biasTracker(gdata).clearContributors();

    switch(gdata.mapElement->type())
    {
    case DMU_SUBSPACE: {
        ConvexSubspace &subspace = gdata.mapElement->as<ConvexSubspace>();
        Plane const &plane       = subspace.cluster().visPlane(gdata.geomId);
        Surface const &surface   = plane.surface();

        Vector3d surfacePoint(subspace.poly().center(), plane.heightSmoothed());

        foreach(BiasSource *source, map.biasSources())
        {
            // If the source is too weak we will ignore it completely.
            if(source->intensity() <= 0)
                continue;

            Vector3d sourceToSurface = (source->origin() - surfacePoint).normalize();
            coord_t distance = 0;

            // Calculate minimum 2D distance to the subspace.
            /// @todo This is probably too accurate an estimate.
            HEdge *baseNode = subspace.poly().hedge();
            HEdge *node = baseNode;
            do
            {
                coord_t len = (Vector2d(source->origin()) - node->origin()).length();
                if(node == baseNode || len < distance)
                    distance = len;
            } while((node = &node->next()) != baseNode);

            if(sourceToSurface.dot(surface.normal()) < 0)
                continue;

            biasTracker(gdata).addContributor(source, source->evaluateIntensity() / de::max(distance, 1.0));
        }
        break; }

    case DMU_SEGMENT: {
        LineSideSegment &seg   = gdata.mapElement->as<LineSideSegment>();
        Surface const &surface = seg.lineSide().middle();
        Vector2d const &from   = seg.hedge().origin();
        Vector2d const &to     = seg.hedge().twin().origin();
        Vector2d const center  = (from + to) / 2;

        foreach(BiasSource *source, map.biasSources())
        {
            // If the source is too weak we will ignore it completely.
            if(source->intensity() <= 0)
                continue;

            Vector3d sourceToSurface = (source->origin() - center).normalize();

            // Calculate minimum 2D distance to the segment.
            coord_t distance = 0;
            for(int k = 0; k < 2; ++k)
            {
                coord_t len = (Vector2d(source->origin()) - (!k? from : to)).length();
                if(k == 0 || len < distance)
                    distance = len;
            }

            if(sourceToSurface.dot(surface.normal()) < 0)
                continue;

            biasTracker(gdata).addContributor(source, source->evaluateIntensity() / de::max(distance, 1.0));
        }
        break; }

    default:
        DENG2_ASSERT(!"Subsector::updateBiasContributorsIfNeeded: Unknown map element referenced");
    }

    return true;
}

#endif // __CLIENT__
