/** @file biassurface.cpp Shadow Bias surface.
 *
 * @authors Copyright © 2005-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include <de/Observers>

#include <QtAlgorithms>
#include <QVector>

#include "de_base.h"
#include "de_console.h"

#include "BspLeaf"
#include "world/linesighttest.h"

#include "BiasTracker"
#include "render/rendpoly.h"

#include "render/biassurface.h"

using namespace de;

/// Ignore intensities below this threshold when accumulating contributions.
static float const MIN_INTENSITY = .005f;

/// Maximum number of sources which can contribute light to a vertex.
static int const MAX_CONTRIBUTORS = 6;

static int lightSpeed        = 130;  //cvar
static int devUpdateAffected = true; //cvar
static int devUseSightCheck  = true; //cvar

static byte activeContributors;
static byte changedContributions;

class VertexIllum
{
    BiasSurface &surface; ///< The owning surface.
    Vector3f color;       ///< Current light color.
    Vector3f dest;        ///< Destination light color (interpolated to).
    uint updateTime;      ///< When the value was calculated.
    bool interpolating;   ///< Set to @c true during interpolation.

    /**
     * Cast lighting contributions from each source that affects the map point.
     * Order is the same as that in the affected surface.
     */
    Vector3f casted[MAX_CONTRIBUTORS];

public:
    VertexIllum(BiasSurface &surface)
        : surface(surface),
          updateTime(0),
          interpolating(false)
    {}

    /**
     * (Re-)Evaluate lighting for this world point.
     *
     * @param color          Final color will be written here.
     * @param point          Point in the map to evaluate. Assumed not to have
     *                       moved since the last call unless the light source
     *                       contributions have since been updated.
     * @param normalAtPoint  Surface normal at @a point. Also assumed not to
     *                       have changed since the last call.
     * @param biasTime       Time in milliseconds of the last bias frame update.
     */
    void evaluate(Vector3f &color, Vector3d const &point,
                  Vector3f const &normalAtPoint, uint biasTime)
    {
        // Does the surface have any lighting changes to apply?
        if(changedContributions)
        {
            if(activeContributors & changedContributions)
            {
                /*
                 * Recalculate the contribution for each light.
                 * We can reuse the previously calculated value for a source
                 * if it hasn't changed.
                 */
                for(int i = 0; i < MAX_CONTRIBUTORS; ++i)
                {
                    if(activeContributors & changedContributions & (1 << i))
                    {
                        updateContribution(i, point, normalAtPoint);
                    }
                }
            }

            applyLightingChanges(activeContributors, biasTime);
        }

        // Factor in the current color (and perform interpolation if needed).
        lerp(color, biasTime);
    }

    /// @copydoc evaluate()
    void evaluate(ColorRawf &color, Vector3d const &point,
                  Vector3f const &normalAtPoint, uint biasTime)
    {
        Vector3f tmp;
        evaluate(tmp, point, normalAtPoint, biasTime);
        for(int c = 0; c < 3; ++c)
            color.rgba[c] += tmp[c];
    }

private:
    /**
     * Returns a previous light contribution by unique contributor index.
     */
    inline Vector3f &contribution(int index)
    {
        DENG_ASSERT(index >= 0 && index < MAX_CONTRIBUTORS);
        return casted[index];
    }

    /**
     * Update any changed lighting contributions.
     *
     * @param surfacePoint  Position of the vertex in the map coordinate space.
     */
    void applyLightingChanges(byte activeContributors, uint biasTime)
    {
#define COLOR_CHANGE_THRESHOLD  0.1f // Ignore small variations for perf

        // Determine the new color (initially, black).
        Vector3f newColor;

        // Do we need to re-accumulate light contributions?
        if(activeContributors)
        {
            /// Maximum accumulated color strength.
            static Vector3f const saturated(1, 1, 1);

            for(int i = 0; i < MAX_CONTRIBUTORS; ++i)
            {
                if(activeContributors & (1 << i))
                {
                    newColor += contribution(i);

                    // Stop once fully saturated.
                    if(newColor >= saturated)
                        break;
                }
            }

            // Clamp.
            newColor = newColor.min(saturated);
        }

        // Is there a new destination?
        if(!activeContributors ||
           (!de::fequal(dest.x, newColor.x, COLOR_CHANGE_THRESHOLD) ||
            !de::fequal(dest.y, newColor.y, COLOR_CHANGE_THRESHOLD) ||
            !de::fequal(dest.z, newColor.z, COLOR_CHANGE_THRESHOLD)))
        {
            if(interpolating)
            {
                // Must not lose the half-way interpolation.
                Vector3f mid; lerp(mid, biasTime);

                // This is current color at this very moment.
                color = mid;
            }

            // This is what we will be interpolating to.
            dest          = newColor;
            interpolating = true;
            updateTime    = surface.timeOfLatestContributorUpdate();
        }

#undef COLOR_CHANGE_THRESHOLD
    }

    /**
     * Update the lighting contribution for the specified contributor @a index.
     *
     * @param index          Unique index of the contributor.
     * @param point          Point in the map to evaluate.
     * @param normalAtPoint  Surface normal at @a point.
     */
    void updateContribution(int index, Vector3d const &point,
        Vector3f const &normalAtPoint)
    {
        BiasSource const &source = surface.contributor(index);

        Vector3f &casted = contribution(index);

        /// @todo LineSightTest should (optionally) perform this test.
        Sector *sector = &source.bspLeafAtOrigin().sector();
        if((!sector->floor().surface().hasSkyMaskedMaterial() &&
                source.origin().z < sector->floor().visHeight()) ||
           (!sector->ceiling().surface().hasSkyMaskedMaterial() &&
                source.origin().z > sector->ceiling().visHeight()))
        {
            // This affecting source does not contribute any light.
            casted = Vector3f();
            return;
        }

        Vector3d sourceToSurface = source.origin() - point;

        /// @todo Do not assume the current map.
        MapElement &bspRoot = App_World().map().bspRoot();

        if(devUseSightCheck &&
           !LineSightTest(source.origin(),
                          point + sourceToSurface / 100).trace(bspRoot))
        {
            // LOS fail.
            // This affecting source does not contribute any light.
            casted = Vector3f();
            return;
        }

        double distance = sourceToSurface.length();
        double dot = sourceToSurface.normalize().dot(normalAtPoint);

        // The surface faces away from the light?
        if(dot < 0)
        {
            casted = Vector3f();
            return;
        }

        // Apply light casted from this source.
        float strength = dot * source.evaluateIntensity() / distance;
        casted = source.color() * de::clamp(0.f, strength, 1.f);
    }

    /**
     * Interpolate between current and destination.
     */
    void lerp(Vector3f &result, uint currentTime)
    {
        if(!interpolating)
        {
            // Use the current color.
            result = color;
            return;
        }

        float inter = (currentTime - updateTime) / float( lightSpeed );

        if(inter > 1)
        {
            interpolating = false;
            color = dest;

            result = color;
        }
        else
        {
            result = color + (dest - color) * inter;
        }
    }
};

struct Contributor
{
    BiasSource *source;
    float influence;
};

/**
 * @todo Defer allocation of most data -- adopt a 'fly-weight' approach.
 *
 * @todo Do not observe source deletion. A better solution would represent any
 * source deletions within the change tracker.
 */
DENG2_PIMPL_NOREF(BiasSurface),
DENG2_OBSERVES(BiasSource, Deletion)
{
    QVector<VertexIllum *> illums; /// @todo use an external allocator.

    Contributor contributors[MAX_CONTRIBUTORS];
    byte activeContributors;
    byte changedContributions;

    uint lastUpdateOnFrame;
    uint lastSourceDeletion; // Milliseconds.

    Instance(int size)
        : illums(size),
          activeContributors(0),
          changedContributions(0),
          lastUpdateOnFrame(0),
          lastSourceDeletion(0)
    {
        zap(contributors);
    }

    ~Instance()
    {
        qDeleteAll(illums);
    }

    /// Observes BiasSource Deletion
    void biasSourceBeingDeleted(BiasSource const &source)
    {
        Contributor *ctbr = contributors;
        for(int i = 0; i < MAX_CONTRIBUTORS; ++i, ctbr++)
        {
            if(ctbr->source == &source)
            {
                ctbr->source = 0;
                activeContributors &= ~(1 << i);
                changedContributions |= 1 << i;

                // Remember the current time (used for interpolation).
                /// @todo Do not assume the 'current' map.
                lastSourceDeletion = App_World().map().biasCurrentTime();
                break;
            }
        }
    }
};

BiasSurface::BiasSurface(int size) : d(new Instance(size))
{}

void BiasSurface::consoleRegister() // static
{
    C_VAR_INT("rend-bias-lightspeed",   &lightSpeed,        0, 0, 5000);

    // Development variables.
    C_VAR_INT("rend-dev-bias-affected", &devUpdateAffected, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-dev-bias-sight",    &devUseSightCheck,  CVF_NO_ARCHIVE, 0, 1);
}

uint BiasSurface::lastUpdateOnFrame() const
{
    return d->lastUpdateOnFrame;
}

void BiasSurface::setLastUpdateOnFrame(uint newLastUpdateFrameNumber)
{
    d->lastUpdateOnFrame = newLastUpdateFrameNumber;
}

void BiasSurface::clearAffected()
{
    d->activeContributors = 0;
}

void BiasSurface::addAffected(float intensity, BiasSource *source)
{
    if(!source) return;

    // If its too weak we will ignore it entirely.
    if(intensity < MIN_INTENSITY)
        return;

    int firstUnusedSlot = -1;
    int slot = -1;

    // Do we have a latent contribution or an unused slot?
    Contributor *ctbr = d->contributors;
    for(int i = 0; i < MAX_CONTRIBUTORS; ++i, ctbr++)
    {
        if(!ctbr->source)
        {
            // Remember the first unused slot.
            if(firstUnusedSlot == -1)
                firstUnusedSlot = i;
        }
        // A latent contribution?
        else if(ctbr->source == source)
        {
            slot = i;
            break;
        }
    }

    if(slot == -1)
    {
        if(firstUnusedSlot != -1)
        {
            slot = firstUnusedSlot;
        }
        else
        {
            // Dang, we'll need to drop the weakest.
            int weakest = -1;
            Contributor *ctbr = d->contributors;
            for(int i = 0; i < MAX_CONTRIBUTORS; ++i, ctbr++)
            {
                DENG_ASSERT(ctbr->source != 0);
                if(i == 0 || ctbr->influence < d->contributors[weakest].influence)
                {
                    weakest = i;
                }
            }

            if(intensity <= d->contributors[weakest].influence)
                return;

            slot = weakest;
            ctbr->source->audienceForDeletion -= d;
            ctbr->source = 0;
        }
    }

    DENG_ASSERT(slot >= 0 && slot < MAX_CONTRIBUTORS);
    ctbr = &d->contributors[slot];

    // When reactivating a latent contribution if the intensity has not
    // changed we don't need to force an update.
    if(!(ctbr->source == source && de::fequal(ctbr->influence, intensity)))
        d->changedContributions |= (1 << slot);

    if(!ctbr->source)
        source->audienceForDeletion += d;

    ctbr->source = source;
    ctbr->influence = intensity;

    // (Re)activate this contributor.
    d->activeContributors |= 1 << slot;
}

void BiasSurface::updateAffection(BiasTracker &changes)
{
    // All contributions from changed sources will need to be updated.

    Contributor *ctbr = d->contributors;
    for(int i = 0; i < MAX_CONTRIBUTORS; ++i, ctbr++)
    {
        if(!ctbr->source)
            continue;

        /// @todo optimize: This O(n) lookup can be avoided if we 1) reference
        /// sources by unique in-map index, and 2) re-index source references
        /// here upon deletion. The assumption being that affection changes
        /// occur far more frequently.
        if(changes.check(App_World().map().toIndex(*ctbr->source)))
        {
            d->changedContributions |= 1 << i;
            break;
        }
    }
}

void BiasSurface::updateAfterMove()
{
    Contributor *ctbr = d->contributors;
    for(int i = 0; i < MAX_CONTRIBUTORS; ++i, ctbr++)
    {
        if(ctbr->source)
        {
            ctbr->source->forceUpdate();
        }
    }
}

BiasSource &BiasSurface::contributor(int index) const
{
    if(index >= 0 && index < MAX_CONTRIBUTORS &&
       (d->activeContributors & (1 << index)))
    {
        DENG_ASSERT(d->contributors[index].source != 0);
        return *d->contributors[index].source;
    }
    /// @throw UnknownContributorError An invalid contributor index was specified.
    throw UnknownContributorError("BiasSurface::lightContributor", QString("Index %1 invalid/out of range").arg(index));
}

uint BiasSurface::timeOfLatestContributorUpdate() const
{
    uint latest = 0;

    if(d->changedContributions)
    {
        Contributor const *ctbr = d->contributors;
        for(int i = 0; i < MAX_CONTRIBUTORS; ++i, ctbr++)
        {
            if(!(d->changedContributions & (1 << i)))
                continue;

            if(!ctbr->source && !(d->activeContributors & (1 << i)))
            {
                // The source of the contribution was deleted.
                if(latest < d->lastSourceDeletion)
                    latest = d->lastSourceDeletion;
            }
            else if(latest < ctbr->source->lastUpdateTime())
            {
                latest = ctbr->source->lastUpdateTime();
            }
        }
    }

    return latest;
}

void BiasSurface::lightPoly(Vector3f const &surfaceNormal, uint biasTime,
    int vertCount, rvertex_t const *positions, ColorRawf *colors)
{
    DENG_ASSERT(vertCount == d->illums.count()); // sanity check
    DENG_ASSERT(positions != 0 && colors != 0);

    // Time to allocate the illumination data?
    if(!d->illums[0])
    {
        for(int i = 0; i < d->illums.count(); ++i)
        {
            d->illums[i] = new VertexIllum(*this);
        }
    }

    // Configure global arguments for BiasVertex::evalLighting(), for perf
    /// @todo refactor away.
    activeContributors   = d->activeContributors;
    changedContributions = d->changedContributions;

    rvertex_t const *vtx = positions;
    ColorRawf *color     = colors;
    for(int i = 0; i < vertCount; ++i, vtx++, color++)
    {
        Vector3d surfacePoint(vtx->pos[VX], vtx->pos[VY], vtx->pos[VZ]);
        d->illums[i]->evaluate(*color, surfacePoint, surfaceNormal, biasTime);
    }

    // Any changes from contributors will have now been applied.
    d->changedContributions = 0;
}
