/** @file convexsubspace.cpp  World map convex subspace.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "world/convexsubspace.h"
#include "Bspleaf"
#include "Face"
#include "Polyobj"
#include "SectorCluster"
#include "Surface"
#include <de/Log>
#include <QtAlgorithms>

using namespace de;

#ifdef __CLIENT__

/// Compute the area of a triangle defined by three 2D point vectors.
ddouble triangleArea(Vector2d const &v1, Vector2d const &v2, Vector2d const &v3)
{
    Vector2d a = v2 - v1;
    Vector2d b = v3 - v1;
    return (a.x * b.y - b.x * a.y) / 2;
}

#endif // __CLIENT__

DENG2_PIMPL(ConvexSubspace)
{
    Face &poly;               ///< Convex polygon geometry (not owned).
    Meshes extraMeshes;       ///< Additional meshes (owned).
    Polyobjs polyobjs;        ///< Linked polyobjs (if any, not owned).
    SectorCluster *cluster;   ///< Attributed cluster (if any, not owned).
    int validCount;           ///< Used to prevent repeated processing.
    Vector2d worldGridOffset; ///< For aligning the materials to the map space grid.
    BspLeaf *bspLeaf;

#ifdef __CLIENT__
    int addSpriteCount;             ///< Frame number of last R_AddSprites.
    Lumobjs lumobjs;                ///< Linked lumobjs (not owned).
    ShadowLines shadowLines;        ///< Linked map lines for fake radio shadowing.

    HEdge *fanBase;                 ///< Trifan base Half-edge (otherwise the center point is used).
    bool needUpdateFanBase;         ///< @c true= need to rechoose a fan base half-edge.
    AudioEnvironmentFactors reverb; ///< Cached characteristics.

    Shards shards;
#endif

    Instance(Public *i, Face &poly)
        : Base(i)
        , poly             (poly)
        , cluster          (0)
        , validCount       (0)
        , bspLeaf          (0)
#ifdef __CLIENT__
        , addSpriteCount   (0)
        , fanBase          (0)
        , needUpdateFanBase(true)
#endif
    {
#ifdef __CLIENT__
        de::zap(reverb);
#endif
    }

    ~Instance() { qDeleteAll(extraMeshes); }

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

        HEdge *firstNode = poly.hedge();

        fanBase = firstNode;

        if(poly.hedgeCount() > 3)
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

ConvexSubspace::ConvexSubspace(Face &convexPolygon, BspLeaf *bspLeaf)
    : MapElement(DMU_SUBSPACE)
    , d(new Instance(this, convexPolygon))
{
    // Determine the world grid offset.
    d->worldGridOffset = Vector2d(fmod(d->poly.aaBox().minX, 64),
                                  fmod(d->poly.aaBox().maxY, 64));

    d->poly.setMapElement(this);
    setBspLeaf(bspLeaf);
}

ConvexSubspace *ConvexSubspace::newFromConvexPoly(de::Face &poly, BspLeaf *bspLeaf) // static
{
    if(!poly.isConvex())
    {
        /// @throw InvalidPolyError Attempted to attribute a non-convex polygon.
        throw InvalidPolyError("ConvexSubspace::newFromConvexPoly", "Source is non-convex");
    }
    return new ConvexSubspace(poly, bspLeaf);
}

BspLeaf &ConvexSubspace::bspLeaf() const
{
    DENG2_ASSERT(d->bspLeaf != 0);
    return *d->bspLeaf;
}

void ConvexSubspace::setBspLeaf(BspLeaf *newBspLeaf)
{
    d->bspLeaf = newBspLeaf;
}

Face &ConvexSubspace::poly() const
{
    return d->poly;
}

bool ConvexSubspace::contains(Vector2d const &point) const
{
    HEdge const *hedge = poly().hedge();
    do
    {
        Vertex const &va = hedge->vertex();
        Vertex const &vb = hedge->next().vertex();

        if(((va.origin().y - point.y) * (vb.origin().x - va.origin().x) -
            (va.origin().x - point.x) * (vb.origin().y - va.origin().y)) < 0)
        {
            // Outside the BSP leaf's edges.
            return false;
        }

    } while((hedge = &hedge->next()) != poly().hedge());

    return true;
}

void ConvexSubspace::assignExtraMesh(Mesh &newMesh)
{
    LOG_AS("ConvexSubspace");

    int const sizeBefore = d->extraMeshes.size();

    d->extraMeshes.insert(&newMesh);

    if(d->extraMeshes.size() != sizeBefore)
    {
        LOG_DEBUG("Assigned extra mesh to subspace %p") << this;

        // Attribute all faces to "this" subspace.
        foreach(Face *face, newMesh.faces())
        {
            face->setMapElement(this);
        }
    }
}

ConvexSubspace::Meshes const &ConvexSubspace::extraMeshes() const
{
    return d->extraMeshes;
}

void ConvexSubspace::link(Polyobj const &polyobj)
{
    d->polyobjs.insert(const_cast<Polyobj *>(&polyobj));
}

bool ConvexSubspace::unlink(polyobj_s const &polyobj)
{
    int sizeBefore = d->polyobjs.size();
    d->polyobjs.remove(const_cast<Polyobj *>(&polyobj));
    return d->polyobjs.size() != sizeBefore;
}

ConvexSubspace::Polyobjs const &ConvexSubspace::polyobjs() const
{
    return d->polyobjs;
}

Vector2d const &ConvexSubspace::worldGridOffset() const
{
    return d->worldGridOffset;
}

bool ConvexSubspace::hasCluster() const
{
    return d->cluster != 0;
}

SectorCluster &ConvexSubspace::cluster() const
{
    if(d->cluster)
    {
        return *d->cluster;
    }
    /// @throw MissingClusterError Attempted with no sector cluster attributed.
    throw MissingClusterError("ConvexSubspace::cluster", "No sector cluster is attributed");
}

void ConvexSubspace::setCluster(SectorCluster *newCluster)
{
    d->cluster = newCluster;
}

int ConvexSubspace::validCount() const
{
    return d->validCount;
}

void ConvexSubspace::setValidCount(int newValidCount)
{
    d->validCount = newValidCount;
}

#ifdef __CLIENT__
void ConvexSubspace::clearShadowLines()
{
    d->shadowLines.clear();
}

void ConvexSubspace::addShadowLine(LineSide &side)
{
    d->shadowLines.insert(&side);
}

ConvexSubspace::ShadowLines const &ConvexSubspace::shadowLines() const
{
    return d->shadowLines;
}

void ConvexSubspace::unlinkAllLumobjs()
{
    d->lumobjs.clear();
}

void ConvexSubspace::unlink(Lumobj &lumobj)
{
    d->lumobjs.remove(&lumobj);
}

void ConvexSubspace::link(Lumobj &lumobj)
{
    d->lumobjs.insert(&lumobj);
}

ConvexSubspace::Lumobjs const &ConvexSubspace::lumobjs() const
{
    return d->lumobjs;
}

int ConvexSubspace::lastSpriteProjectFrame() const
{
    return d->addSpriteCount;
}

void ConvexSubspace::setLastSpriteProjectFrame(int newFrameCount)
{
    d->addSpriteCount = newFrameCount;
}

HEdge *ConvexSubspace::fanBase() const
{
    if(d->needUpdateFanBase)
    {
        d->chooseFanBase();
    }
    return d->fanBase;
}

int ConvexSubspace::numFanVertices() const
{
    // Are we to use one of the half-edge vertexes as the fan base?
    return d->poly.hedgeCount() + (fanBase()? 0 : 2);
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

bool ConvexSubspace::updateReverb()
{
    if(!hasCluster())
    {
        d->reverb[SRD_SPACE] = d->reverb[SRD_VOLUME] =
            d->reverb[SRD_DECAY] = d->reverb[SRD_DAMPING] = 0;
        return false;
    }

    float envSpaceAccum[NUM_AUDIO_ENVIRONMENTS];
    de::zap(envSpaceAccum);

    // Space is the rough volume of the BSP leaf (bounding box).
    AABoxd const &aaBox = d->poly.aaBox();
    d->reverb[SRD_SPACE] = int(cluster().ceiling().height() - cluster().floor().height())
                         * (aaBox.maxX - aaBox.minX) * (aaBox.maxY - aaBox.minY);

    // The other reverb properties can be found out by taking a look at the
    // materials of all surfaces in the BSP leaf.
    float total  = 0;
    HEdge *base  = d->poly.hedge();
    HEdge *hedge = base;
    do
    {
        accumReverbForWallSections(hedge, envSpaceAccum, total);
    } while((hedge = &hedge->next()) != base);

    foreach(Mesh *mesh, d->extraMeshes)
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

ConvexSubspace::AudioEnvironmentFactors const &ConvexSubspace::reverb() const
{
    return d->reverb;
}

ConvexSubspace::Shards &ConvexSubspace::shards()
{
    return d->shards;
}

#endif // __CLIENT__
