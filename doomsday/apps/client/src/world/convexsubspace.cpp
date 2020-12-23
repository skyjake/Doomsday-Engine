/** @file convexsubspace.cpp  Map convex subspace.
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "world/audioenvironment.h"
#include "world/line.h"
#include "world/surface.h"
#include "resource/clientmaterial.h"

#include <doomsday/audio/s_environ.h>
#include <doomsday/mesh/face.h>

#include <de/set.h>

using namespace de;

/// Compute the area of a triangle defined by three 2D point vectors.
static double triangleArea(const Vec2d &v1, const Vec2d &v2, const Vec2d &v3)
{
    Vec2d a = v2 - v1;
    Vec2d b = v3 - v1;
    return (a.x * b.y - b.x * a.y) / 2;
}

DE_PIMPL(ConvexSubspace)
{
    Vec2d           worldGridOffset; // For aligning the materials to the map space grid.
    Set<Lumobj *>   lumobjs;         // Linked lumobjs (not owned).
    Set<LineSide *> shadowLines;     // Linked map lines for fake radio shadowing.

    mesh::HEdge *fanBase = nullptr; // Trifan base Half-edge (otherwise the center point is used).
    bool         needUpdateFanBase = true; // true: need to rechoose a fan base half-edge.

    int lastSpriteProjectFrame = 0; // Frame number of last R_AddSprites.

    world::AudioEnvironment audioEnvironment; // Cached audio characteristics.

    Impl(Public *i) : Base(i)
    {}

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
        using world::Vertex;

        const double MIN_TRIANGLE_EPSILON = 0.1; // Area

        auto *firstNode = self().poly().hedge();

        fanBase = firstNode;

        if (self().poly().hedgeCount() > 3)
        {
            // Splines with higher vertex counts demand checking.
            const Vertex *base, *a, *b;

            // Search for a good base.
            do
            {
                auto *other = firstNode;
                base = &fanBase->vertex();
                do
                {
                    // Test this triangle?
                    if(!(fanBase != firstNode && (other == fanBase || other == &fanBase->prev())))
                    {
                        a = &other->vertex();
                        b = &other->next().vertex();

                        if (de::abs(triangleArea(base->origin(), a->origin(), b->origin())) <=
                            MIN_TRIANGLE_EPSILON)
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
    }
};

ConvexSubspace::ConvexSubspace(mesh::Face &face, world::BspLeaf *bspLeaf)
    : world::ConvexSubspace(face, bspLeaf)
    , d(new Impl(this))
{
    // Determine the world grid offset.
    d->worldGridOffset = Vec2d(fmod(poly().bounds().minX, 64),
                               fmod(poly().bounds().maxY, 64));
}

const Vec2d &ConvexSubspace::worldGridOffset() const
{
    return d->worldGridOffset;
}

int ConvexSubspace::shadowLineCount() const
{
    return d->shadowLines.size();
}

void ConvexSubspace::clearShadowLines()
{
    d->shadowLines.clear();
}

void ConvexSubspace::addShadowLine(LineSide &side)
{
    d->shadowLines.insert(&side);
}

LoopResult ConvexSubspace::forAllShadowLines(const std::function<LoopResult (LineSide &)>& func) const
{
    for(LineSide *side : d->shadowLines)
    {
        if(auto result = func(*side)) return result;
    }
    return LoopContinue;
}

int ConvexSubspace::lumobjCount() const
{
    return d->lumobjs.size();
}

LoopResult ConvexSubspace::forAllLumobjs(const std::function<LoopResult (Lumobj &)>& func) const
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

int ConvexSubspace::lastSpriteProjectFrame() const
{
    return d->lastSpriteProjectFrame;
}

void ConvexSubspace::setLastSpriteProjectFrame(int newFrameNumber)
{
    d->lastSpriteProjectFrame = newFrameNumber;
}

mesh::HEdge *ConvexSubspace::fanBase() const
{
    if(d->needUpdateFanBase)
    {
        d->chooseFanBase();
    }
    return d->fanBase;
}

int ConvexSubspace::fanVertexCount() const
{
    // Are we to use one of the half-edge vertexes as the fan base?
    return poly().hedgeCount() + (fanBase()? 0 : 2);
}

static void accumReverbForWallSections(const mesh::HEdge *hedge,
                                       float envSpaceAccum[NUM_AUDIO_ENVIRONMENTS],
                                       float &total)
{
    // Edges with no map line segment implicitly have no surfaces.
    if(!hedge || !hedge->hasMapElement())
        return;

    const LineSideSegment &seg = hedge->mapElementAs<LineSideSegment>();
    if(!seg.lineSide().hasSections() || !seg.lineSide().middle().hasMaterial())
        return;

    world::Material &material = seg.lineSide().middle().material();
    AudioEnvironmentId env = material.as<ClientMaterial>().audioEnvironment();
    if(!(env >= 0 && env < NUM_AUDIO_ENVIRONMENTS))
    {
        env = AE_WOOD; // Assume it's wood if unknown.
    }

    total += float(seg.length());

    envSpaceAccum[env] += float(seg.length());
}

bool ConvexSubspace::updateAudioEnvironment()
{
    using namespace mesh;

    auto &env = d->audioEnvironment;

    if(!hasSubsector())
    {
        env.reset();
        return false;
    }

    float contrib[NUM_AUDIO_ENVIRONMENTS]{};

    // Space is the rough volume of the bounding box.
    const AABoxd &bounds = poly().bounds();
    env.space = int(subsector().sector().ceiling().height() - subsector().sector().floor().height())
                * ((bounds.maxX - bounds.minX) * (bounds.maxY - bounds.minY));

    // The other reverb properties can be found out by taking a look at the
    // materials of all surfaces in the BSP leaf.
    float  coverage = 0;
    HEdge *base     = poly().hedge();
    HEdge *hedge    = base;
    do
    {
        accumReverbForWallSections(hedge, contrib, coverage);
    } while ((hedge = &hedge->next()) != base);

    forAllExtraMeshes([&contrib, &coverage](mesh::Mesh &mesh) {
        for (auto *hedge : mesh.hedges())
        {
            accumReverbForWallSections(hedge, contrib, coverage);
        }
        return LoopContinue;
    });

    if (fequal(coverage, 0.0f))
    {
        // Huh?
        env.reset();
        return false;
    }

    // Average the results.
    for (int i = AE_FIRST; i < NUM_AUDIO_ENVIRONMENTS; ++i)
    {
        contrib[i] /= coverage;
    }

    // Accumulate contributions and clamp the results.
    int volume  = 0;
    int decay   = 0;
    int damping = 0;
    for (int i = AE_FIRST; i < NUM_AUDIO_ENVIRONMENTS; ++i)
    {
        const auto &envDef = S_AudioEnvironment(AudioEnvironmentId(i));
        volume  += envDef.volume  * contrib[i];
        decay   += envDef.decay   * contrib[i];
        damping += envDef.damping * contrib[i];
    }
    env.volume  = de::min(volume, 255);
    env.decay   = de::min(decay, 255);
    env.damping = de::min(damping, 255);

    return true;
}

const world::AudioEnvironment &ConvexSubspace::audioEnvironment() const
{
    return d->audioEnvironment;
}
