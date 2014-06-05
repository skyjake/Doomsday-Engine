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

#include "world/client/subsector.h"
#include "world/map.h"
#include "ConvexSubspace"
#include "Face"
#include "Mesh"
#include "Surface"
#include "WallEdge"
#include "render/rend_main.h"
#include <de/Log>
#include <QtAlgorithms>

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

DENG2_PIMPL_NOREF(Subsector)
{
    ConvexSubspace *subspace;        ///< Convex map subspace (not owned).
    HEdge *fanBase;                  ///< Trifan base Half-edge (otherwise the center point is used).
    bool needFanBaseUpdate;          ///< @c true= need to rechoose a fan base half-edge.

    // Shard preparation data for all map geometries in the subsector (owned).
    GeometryGroups geomGroups;

    Lumobjs lumobjs;                 ///< Linked lumobjs (not owned).
    ShadowLines shadowLines;         ///< Linked map lines for fake radio shadowing (not owned).
    Shards shards;                   ///< Linked geometry shards (not owned).
    AudioEnvironmentFactors reverb;  ///< Cached characteristics.

    Instance(ConvexSubspace &subspace)
        : subspace         (&subspace)
        , fanBase          (0)
        , needFanBaseUpdate(true)
    {
        de::zap(reverb);
    }

    ~Instance()
    {
        // Delete all GeometryData.
        foreach(GeometryGroup const &group, geomGroups) qDeleteAll(group);
    }

    int numFanVertices()
    {
        updateFanBaseIfNeeded();
        // Are we to use one of the half-edge vertexes as the fan base?
        return subspace->poly().hedgeCount() + (fanBase? 0 : 2);
    }

    void addBiasSurfaceIfMissing(GeometryData &gdata)
    {
        if(!gdata.biasSurface.isNull()) return;

        gdata.biasSurface.reset(new GeometryData::BiasSurface);

        // Determine the number of bias illumination points needed (presently
        // we define a 1:1 mapping to geometry vertices).
        int numIllums = 0;
        switch(gdata.mapElement->type())
        {
        case DMU_SUBSPACE: {
            DENG2_ASSERT(gdata.geomId >= 0 && gdata.geomId < subspace->sector().planeCount()); // sanity check
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
    void updateFanBaseIfNeeded()
    {
#define MIN_TRIANGLE_EPSILON  (0.1) ///< Area

        if(!needFanBaseUpdate) return;
        needFanBaseUpdate = false;

        HEdge *firstNode = subspace->poly().hedge();
        fanBase = firstNode;

        if(subspace->poly().hedgeCount() > 3)
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

#undef MIN_TRIANGLE_EPSILON
    }
};

Subsector::Subsector(ConvexSubspace &convexSubspace) : d(new Instance(convexSubspace))
{}

int Subsector::numFanVertices() const
{
    return d->numFanVertices();
}

void Subsector::writePosCoords(WorldVBuf &vbuf, WorldVBuf::Indices const &indices,
    ClockDirection direction, coord_t height)
{
    d->updateFanBaseIfNeeded();

    Face const &poly = d->subspace->poly();
    HEdge *fanBase   = d->fanBase;

    WorldVBuf::Index n = 0;
    if(!fanBase)
    {
        vbuf[indices[n]].pos = Vector3f(poly.center(), height);
        n++;
    }

    // Add the vertices for each hedge.
    HEdge *base  = fanBase? fanBase : poly.hedge();
    HEdge *hedge = base;
    do
    {
        vbuf[indices[n]].pos = Vector3f(hedge->origin(), height);
        n++;
    } while((hedge = &hedge->neighbor(direction)) != base);

    // The last vertex is always equal to the first.
    if(!fanBase)
    {
        vbuf[indices[n]].pos = Vector3f(poly.hedge()->origin(), height);
    }
}

bool Subsector::hasGeomData(de::MapElement &mapElement, int geomId) const
{
    return const_cast<Subsector *>(this)->geomData(mapElement, geomId) != 0;
}

Subsector::GeometryData *Subsector::geomData(MapElement &mapElement, int geomId, bool canAlloc)
{
    GeometryGroups::iterator foundGroup = d->geomGroups.find(&mapElement);
    if(foundGroup != d->geomGroups.end())
    {
        GeometryGroup &geomDatas = *foundGroup;
        GeometryGroup::iterator found = geomDatas.find(geomId);
        if(found != geomDatas.end())
        {
            return *found;
        }
    }

    if(!canAlloc) return 0;

    if(foundGroup == d->geomGroups.end())
    {
        foundGroup = d->geomGroups.insert(&mapElement, GeometryGroup());
    }

    return foundGroup->insert(geomId, new GeometryData(&mapElement, geomId)).value();
}

Subsector::GeometryGroups const &Subsector::geomGroups() const
{
    return d->geomGroups;
}

Subsector::GeometryData::BiasIllums &Subsector::biasIllums(GeometryData &gdata)
{
    d->addBiasSurfaceIfMissing(gdata);
    return gdata.biasSurface->illums;
}

bool Subsector::updateBiasContributorsIfNeeded(GeometryData &gdata)
{
    if(!Rend_BiasContributorUpdatesEnabled())
        return false;

    if(gdata.biasSurface.isNull()) return false;

    Map &map = d->subspace->map();

    uint const lastBiasChangeFrame = map.biasLastChangeOnFrame();
    if(gdata.biasSurface->lastUpdateFrame == lastBiasChangeFrame)
        return false;

    // Mark the data as having been updated (it will be soon).
    gdata.setBiasLastUpdateFrame(lastBiasChangeFrame);

    BiasTracker &biasTracker = gdata.biasSurface->tracker;

    biasTracker.clearContributors();

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

            biasTracker.addContributor(source, source->evaluateIntensity() / de::max(distance, 1.0));
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

            biasTracker.addContributor(source, source->evaluateIntensity() / de::max(distance, 1.0));
        }
        break; }

    default:
        DENG2_ASSERT(!"Subsector::updateBiasContributorsIfNeeded: Unknown map element referenced");
    }

    return true;
}

void Subsector::clearShadowLines()
{
    d->shadowLines.clear();
}

void Subsector::addShadowLine(LineSide &side)
{
    DENG2_ASSERT(side.hasSector());
    DENG2_ASSERT(side.hasSections());

    d->shadowLines.insert(&side);
}

Subsector::ShadowLines const &Subsector::shadowLines() const
{
    return d->shadowLines;
}

void Subsector::unlinkAllLumobjs()
{
    d->lumobjs.clear();
}

void Subsector::unlink(Lumobj &lumobj)
{
    d->lumobjs.remove(&lumobj);
}

void Subsector::link(Lumobj &lumobj)
{
    d->lumobjs.insert(&lumobj);
}

Subsector::Lumobjs const &Subsector::lumobjs() const
{
    return d->lumobjs;
}

static void accumReverbForWallSections(HEdge const *hedge,
    float envSpaceAccum[NUM_AUDIO_ENVIRONMENTS], float &total)
{
    // Edges with no map line segment implicitly have no surfaces.
    if(!hedge || !hedge->hasMapElement())
        return;

    LineSideSegment const &seg = hedge->mapElementAs<LineSideSegment>();
    if(!seg.lineSide().hasSections() || !seg.lineSide().middle().hasMaterial())
        return;

    Material &material = seg.lineSide().middle().material();
    AudioEnvironmentId env = material.audioEnvironment();
    if(!(env >= 0 && env < NUM_AUDIO_ENVIRONMENTS))
        env = AE_WOOD; // Assume it's wood if unknown.

    total += seg.length();

    envSpaceAccum[env] += seg.length();
}

bool Subsector::updateReverb()
{
    DENG2_ASSERT(d->subspace->hasCluster()); // *never* false? -ds
    if(!d->subspace->hasCluster())
    {
        d->reverb[SRD_SPACE] = d->reverb[SRD_VOLUME] =
            d->reverb[SRD_DECAY] = d->reverb[SRD_DAMPING] = 0;
        return false;
    }
    SectorCluster &cluster = d->subspace->cluster();

    float envSpaceAccum[NUM_AUDIO_ENVIRONMENTS];
    de::zap(envSpaceAccum);

    // Space is the rough volume of the subspace bounding box.
    AABoxd const &aaBox = d->subspace->poly().aaBox();
    d->reverb[SRD_SPACE] = int(cluster.ceiling().height() - cluster.floor().height())
                         * (aaBox.maxX - aaBox.minX) * (aaBox.maxY - aaBox.minY);

    // The other reverb properties can be found out by taking a look at the
    // materials of all surfaces in the BSP leaf.
    float total  = 0;
    HEdge *base  = d->subspace->poly().hedge();
    HEdge *hedge = base;
    do
    {
        accumReverbForWallSections(hedge, envSpaceAccum, total);
    } while((hedge = &hedge->next()) != base);

    foreach(Mesh *mesh, d->subspace->extraMeshes())
    foreach(HEdge *hedge, mesh->hedges())
    {
        accumReverbForWallSections(hedge, envSpaceAccum, total);
    }

    if(!total)
    {
        // Huh?
        d->reverb[SRD_VOLUME] = d->reverb[SRD_DECAY] = d->reverb[SRD_DAMPING] = 0;
        return false;
    }

    // Average the results.
    for(int i = AE_FIRST; i < NUM_AUDIO_ENVIRONMENTS; ++i)
    {
        envSpaceAccum[i] /= total;
    }

    // Accumulate and clamp the final characteristics
    int accum[NUM_REVERB_DATA]; zap(accum);
    for(int i = AE_FIRST; i < NUM_AUDIO_ENVIRONMENTS; ++i)
    {
        AudioEnvironment const &envInfo = S_AudioEnvironment(AudioEnvironmentId(i));
        // Volume.
        accum[SRD_VOLUME]  += envSpaceAccum[i] * envInfo.volumeMul;

        // Decay time.
        accum[SRD_DECAY]   += envSpaceAccum[i] * envInfo.decayMul;

        // High frequency damping.
        accum[SRD_DAMPING] += envSpaceAccum[i] * envInfo.dampingMul;
    }
    d->reverb[SRD_VOLUME]  = de::min(accum[SRD_VOLUME],  255);
    d->reverb[SRD_DECAY]   = de::min(accum[SRD_DECAY],   255);
    d->reverb[SRD_DAMPING] = de::min(accum[SRD_DAMPING], 255);

    return true;
}

Subsector::AudioEnvironmentFactors const &Subsector::reverb() const
{
    return d->reverb;
}

Subsector::Shards &Subsector::shards()
{
    return d->shards;
}

ConvexSubspace &Subsector::convexSubspace() const
{
    DENG2_ASSERT(d->subspace != 0);
    return *d->subspace;
}
