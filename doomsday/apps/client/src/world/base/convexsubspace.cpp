/** @file convexsubspace.cpp  Map convex subspace.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#include "world/convexsubspace.h"

#ifdef __CLIENT__
#  include "world/audioenvironment.h"
#  include "audio/s_environ.h"
#  include "ClientMaterial"
#endif
#include "BspLeaf"
#include "Face"
#include "Polyobj"
#include "Subsector"
#include "Surface"
#include <de/Log>
#include <QSet>
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

namespace world {

DENG2_PIMPL(ConvexSubspace)
{
    Face *poly = nullptr;                  ///< Convex polygon geometry (not owned).

    typedef QSet<de::Mesh *> Meshes;
    Meshes extraMeshes;                    ///< Additional meshes (owned).

    typedef QSet<polyobj_s *> Polyobjs;
    Polyobjs polyobjs;                     ///< Linked polyobjs (not owned).

#ifdef __CLIENT__
    typedef QSet<Lumobj *> Lumobjs;
    Lumobjs lumobjs;                       ///< Linked lumobjs (not owned).

    typedef QSet<LineSide *> ShadowLines;
    ShadowLines shadowLines;               ///< Linked map lines for fake radio shadowing.

    HEdge *fanBase = nullptr;              ///< Trifan base Half-edge (otherwise the center point is used).
    bool needUpdateFanBase = true;         ///< @c true= need to rechoose a fan base half-edge.

    AudioEnvironment audioEnvironment;     ///< Cached audio characteristics.

    dint lastSpriteProjectFrame = 0;       ///< Frame number of last R_AddSprites.
#endif

    dint validCount = 0;                   ///< Used to prevent repeated processing.

    Impl(Public *i) : Base(i) {}
    ~Impl() { qDeleteAll(extraMeshes); }

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

        HEdge *firstNode = self().poly().hedge();

        fanBase = firstNode;

        if(self().poly().hedgeCount() > 3)
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
    , d(new Impl(this))
{
    d->poly = &convexPolygon;
    poly().setMapElement(this);
    setBspLeaf(bspLeaf);
}

ConvexSubspace *ConvexSubspace::newFromConvexPoly(de::Face &poly, BspLeaf *bspLeaf) // static
{
    if(!poly.isConvex())
    {
        /// @throw InvalidPolyError  Attempted to attribute a non-convex polygon.
        throw InvalidPolyError("ConvexSubspace::newFromConvexPoly", "Source is non-convex");
    }
    return new ConvexSubspace(poly, bspLeaf);
}

void ConvexSubspace::setBspLeaf(BspLeaf *newBspLeaf)
{
    _bspLeaf = newBspLeaf;
}

Face &ConvexSubspace::poly() const
{
    DENG2_ASSERT(d->poly);
    return *d->poly;
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

    dint const sizeBefore = d->extraMeshes.size();

    d->extraMeshes.insert(&newMesh);

    if(d->extraMeshes.size() != sizeBefore)
    {
        LOG_DEBUG("Assigned extra mesh to subspace %p") << this;

        // Attribute all faces to "this" subspace.
        for(Face *face : newMesh.faces())
        {
            face->setMapElement(this);
        }
    }
}

LoopResult ConvexSubspace::forAllExtraMeshes(std::function<LoopResult (Mesh &)> func) const
{
    for(Mesh *mesh : d->extraMeshes)
    {
        if(auto result = func(*mesh)) return result;
    }
    return LoopContinue;
}

dint ConvexSubspace::polyobjCount() const
{
    return d->polyobjs.count();
}

LoopResult ConvexSubspace::forAllPolyobjs(std::function<LoopResult (Polyobj &)> func) const
{
    for(Polyobj *pob : d->polyobjs)
    {
        if(auto result = func(*pob)) return result;
    }
    return LoopContinue;
}

void ConvexSubspace::link(Polyobj const &polyobj)
{
    d->polyobjs.insert(const_cast<Polyobj *>(&polyobj));
}

bool ConvexSubspace::unlink(Polyobj const &polyobj)
{
    dint sizeBefore = d->polyobjs.size();
    d->polyobjs.remove(const_cast<Polyobj *>(&polyobj));
    return d->polyobjs.size() != sizeBefore;
}

/*Subsector &ConvexSubspace::subsector() const
{
    if(d->subsector) return *d->subsector;
    /// @throw MissingSubsectorError  Attempted with no subsector attributed.
    throw MissingSubsectorError("ConvexSubspace::subsector", "No subsector is attributed");
}

Subsector *ConvexSubspace::subsectorPtr() const
{
    return hasSubsector() ? &subsector() : nullptr;
}*/

void ConvexSubspace::setSubsector(Subsector *newSubsector)
{
    _subsector = newSubsector;
}

dint ConvexSubspace::validCount() const
{
    return d->validCount;
}

void ConvexSubspace::setValidCount(dint newValidCount)
{
    d->validCount = newValidCount;
}

#ifdef __CLIENT__

dint ConvexSubspace::shadowLineCount() const
{
    return d->shadowLines.count();
}

void ConvexSubspace::clearShadowLines()
{
    d->shadowLines.clear();
}

void ConvexSubspace::addShadowLine(LineSide &side)
{
    d->shadowLines.insert(&side);
}

LoopResult ConvexSubspace::forAllShadowLines(std::function<LoopResult (LineSide &)> func) const
{
    for(LineSide *side : d->shadowLines)
    {
        if(auto result = func(*side)) return result;
    }
    return LoopContinue;
}

dint ConvexSubspace::lumobjCount() const
{
    return d->lumobjs.count();
}

LoopResult ConvexSubspace::forAllLumobjs(std::function<LoopResult (Lumobj &)> func) const
{
    for(Lumobj *lob : d->lumobjs)
    {
        if(auto result = func(*lob)) return result;
    }
    return LoopContinue;
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

dint ConvexSubspace::lastSpriteProjectFrame() const
{
    return d->lastSpriteProjectFrame;
}

void ConvexSubspace::setLastSpriteProjectFrame(dint newFrameNumber)
{
    d->lastSpriteProjectFrame = newFrameNumber;
}

HEdge *ConvexSubspace::fanBase() const
{
    if(d->needUpdateFanBase)
    {
        d->chooseFanBase();
    }
    return d->fanBase;
}

dint ConvexSubspace::fanVertexCount() const
{
    // Are we to use one of the half-edge vertexes as the fan base?
    return poly().hedgeCount() + (fanBase()? 0 : 2);
}

static void accumReverbForWallSections(HEdge const *hedge,
                                       dfloat envSpaceAccum[NUM_AUDIO_ENVIRONMENTS],
                                       dfloat &total)
{
    // Edges with no map line segment implicitly have no surfaces.
    if(!hedge || !hedge->hasMapElement())
        return;

    LineSideSegment const &seg = hedge->mapElementAs<LineSideSegment>();
    if(!seg.lineSide().hasSections() || !seg.lineSide().middle().hasMaterial())
        return;

    Material &material = seg.lineSide().middle().material();
    AudioEnvironmentId env = material.as<ClientMaterial>().audioEnvironment();
    if(!(env >= 0 && env < NUM_AUDIO_ENVIRONMENTS))
    {
        env = AE_WOOD; // Assume it's wood if unknown.
    }

    total += seg.length();

    envSpaceAccum[env] += seg.length();
}

bool ConvexSubspace::updateAudioEnvironment()
{
    world::AudioEnvironment &env = d->audioEnvironment;

    if(!hasSubsector())
    {
        env.reset();
        return false;
    }

    dfloat contrib[NUM_AUDIO_ENVIRONMENTS]; de::zap(contrib);

    // Space is the rough volume of the bounding box.
    AABoxd const &bounds = poly().bounds();
    env.space = dint(subsector().sector().ceiling().height() - subsector().sector().floor().height())
              * ((bounds.maxX - bounds.minX) * (bounds.maxY - bounds.minY));

    // The other reverb properties can be found out by taking a look at the
    // materials of all surfaces in the BSP leaf.
    dfloat coverage = 0;
    HEdge *base    = poly().hedge();
    HEdge *hedge   = base;
    do
    {
        accumReverbForWallSections(hedge, contrib, coverage);
    } while((hedge = &hedge->next()) != base);

    for(Mesh *mesh : d->extraMeshes)
    for(HEdge *hedge : mesh->hedges())
    {
        accumReverbForWallSections(hedge, contrib, coverage);
    }

    if(!coverage)
    {
        // Huh?
        env.reset();
        return false;
    }

    // Average the results.
    for(dint i = AE_FIRST; i < NUM_AUDIO_ENVIRONMENTS; ++i)
    {
        contrib[i] /= coverage;
    }

    // Accumulate contributions and clamp the results.
    dint volume  = 0;
    dint decay   = 0;
    dint damping = 0;
    for(dint i = AE_FIRST; i < NUM_AUDIO_ENVIRONMENTS; ++i)
    {
        ::AudioEnvironment const &envDef = S_AudioEnvironment(AudioEnvironmentId(i));
        volume  += envDef.volume  * contrib[i];
        decay   += envDef.decay   * contrib[i];
        damping += envDef.damping * contrib[i];
    }
    env.volume  = de::min(volume,  255);
    env.decay   = de::min(decay,   255);
    env.damping = de::min(damping, 255);

    return true;
}

AudioEnvironment const &ConvexSubspace::audioEnvironment() const
{
    return d->audioEnvironment;
}

#endif  // __CLIENT__

}  // namespace world
