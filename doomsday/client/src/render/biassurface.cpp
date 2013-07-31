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
static int const MAX_AFFECTED = 6;

static int lightSpeed        = 130;  //cvar
static int devUpdateAffected = true; //cvar
static int devUseSightCheck  = true; //cvar

struct Contributor
{
    BiasSource *source;
    float influence;
};

typedef Contributor Contributors[MAX_AFFECTED];

/**
 * evalLighting uses these -- they must be set before it is called.
 */
static uint biasTime;
static MapElement const *bspRoot;
static Vector3f const *mapSurfaceNormal;

/// @todo defer allocation of most data -- adopt a 'fly-weight' approach.
DENG2_PIMPL_NOREF(BiasSurface),
DENG2_OBSERVES(BiasSource, Deletion)
{
    /**
     * Per-vertex illumination data.
     */
    struct VertexIllum
    {
        Vector3f color;      ///< Current light color at the vertex.
        Vector3f dest;       ///< Destination light color at the vertex (interpolated to).
        uint updateTime;     ///< When the value was calculated.
        bool interpolating;  ///< Set to @c true during interpolation.

        /**
         * Light contributions from each source affecting the vertex. The order of
         * which being the same as that in the affected surface.
         */
        Vector3f casted[MAX_AFFECTED];

        VertexIllum() : updateTime(0), interpolating(false)
        {}

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

        /**
         * @return Light contribution by the specified contributor.
         */
        Vector3f &contribution(int contributorIndex)
        {
            DENG_ASSERT(contributorIndex >= 0 && contributorIndex < MAX_AFFECTED);
            return casted[contributorIndex];
        }

        void updateContribution(int contributorIndex, BiasSource *source,
            Vector3d const &surfacePoint, Vector3f const &surfaceNormal,
            MapElement const &bspRoot)
        {
            DENG_ASSERT(source != 0);

            Vector3f &casted = contribution(contributorIndex);

            /// @todo LineSightTest should (optionally) perform this test.
            Sector *sector = &source->bspLeafAtOrigin().sector();
            if((!sector->floor().surface().hasSkyMaskedMaterial() &&
                    source->origin().z < sector->floor().visHeight()) ||
               (!sector->ceiling().surface().hasSkyMaskedMaterial() &&
                    source->origin().z > sector->ceiling().visHeight()))
            {
                // This affecting source does not contribute any light.
                casted = Vector3f();
                return;
            }

            Vector3d sourceToSurface = source->origin() - surfacePoint;

            if(devUseSightCheck &&
               !LineSightTest(source->origin(),
                              surfacePoint + sourceToSurface / 100).trace(bspRoot))
            {
                // LOS fail.
                // This affecting source does not contribute any light.
                casted = Vector3f();
                return;
            }

            double distance = sourceToSurface.length();
            double dot = sourceToSurface.normalize().dot(surfaceNormal);

            // The surface faces away from the light?
            if(dot < 0)
            {
                casted = Vector3f();
                return;
            }

            // Apply light casted from this source.
            float strength = dot * source->evaluateIntensity() / distance;
            casted = source->color() * de::clamp(0.f, strength, 1.f);
        }
    };
    QVector<VertexIllum> illums; /// @todo use std::vector instead?

    Contributors affected;
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
        zap(affected);
    }

    /**
     * Perform lighting calculations for a vertex.
     *
     * @param vi            Illumination data for the vertex to be evaluated.
     * @param surfacePoint  Position of the vertex in the map coordinate space.
     */
    Vector3f evalLighting(VertexIllum &vi, Vector3d const &surfacePoint)
    {
#define COLOR_CHANGE_THRESHOLD  0.1f // Ignore small variations for perf

        static Vector3f const saturated(1, 1, 1);

        // Do we have any lighting changes to apply?
        if(changedContributions)
        {
            uint latestSourceUpdate = 0;

            Contributor *ctbr = affected;
            for(int i = 0; i < MAX_AFFECTED; ++i, ctbr++)
            {
                if(!(changedContributions & (1 << i)))
                    continue;

                // Determine the earliest time an affecting source changed.
                if(!ctbr->source && !activeContributors)
                {
                    // The source of the last(!) contribution was deleted.
                    if(latestSourceUpdate < lastSourceDeletion)
                        latestSourceUpdate = lastSourceDeletion;
                }
                else if(latestSourceUpdate < ctbr->source->lastUpdateTime())
                {
                    latestSourceUpdate = ctbr->source->lastUpdateTime();
                }
            }

            if(activeContributors & changedContributions)
            {
                /*
                 * Recalculate the contribution for each light.
                 * We can reuse the previously calculated value for a source
                 * if it hasn't changed.
                 */
                ctbr = affected;
                for(int i = 0; i < MAX_AFFECTED; ++i, ctbr++)
                {
                    if(activeContributors & changedContributions & (1 << i))
                    {
                        vi.updateContribution(i, ctbr->source, surfacePoint,
                                              *mapSurfaceNormal, *bspRoot);
                    }
                }
            }

            // Determine the new color (initially, black).
            Vector3f newColor;

            // Do we need to accumulate light contributions?
            if(activeContributors)
            {
                ctbr = affected;
                for(int i = 0; i < MAX_AFFECTED; ++i, ctbr++)
                {
                    if(activeContributors & (1 << i))
                    {
                        newColor += vi.contribution(i);

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
               (!de::fequal(vi.dest.x, newColor.x, COLOR_CHANGE_THRESHOLD) ||
                !de::fequal(vi.dest.y, newColor.y, COLOR_CHANGE_THRESHOLD) ||
                !de::fequal(vi.dest.z, newColor.z, COLOR_CHANGE_THRESHOLD)))
            {
                if(vi.interpolating)
                {
                    // Must not lose the half-way interpolation.
                    Vector3f mid; vi.lerp(mid, biasTime);

                    // This is current color at this very moment.
                    vi.color = mid;
                }

                // This is what we will be interpolating to.
                vi.dest          = newColor;
                vi.interpolating = true;
                vi.updateTime    = latestSourceUpdate;
            }
        }

        // Finalize lighting (i.e., perform interpolation if needed).
        Vector3f color;
        vi.lerp(color, biasTime);

        // Apply an ambient light term?
        Map &map = bspRoot->map();
        if(map.hasLightGrid())
        {
            color = (color + map.lightGrid().evaluate(surfacePoint))
                    .min(saturated); // clamp
        }

        return color;

#undef COLOR_CHANGE_THRESHOLD
    }

    /// Observes BiasSource Deletion
    void biasSourceBeingDeleted(BiasSource const &source)
    {
        Contributor *ctbr = affected;
        for(int i = 0; i < MAX_AFFECTED; ++i, ctbr++)
        {
            if(ctbr->source == &source)
            {
                ctbr->source = 0;
                activeContributors &= ~(1 << i);
                changedContributions |= 1 << i;

                // Remember the current time (used for interpolation).
                /// @todo Do not assume the 'current' map. A better solution
                /// would represent source deletion in the per-frame change
                /// notifications.
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
    Contributor *ctbr = d->affected;
    for(int i = 0; i < MAX_AFFECTED; ++i, ctbr++)
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
            Contributor *ctbr = d->affected;
            for(int i = 0; i < MAX_AFFECTED; ++i, ctbr++)
            {
                DENG_ASSERT(ctbr->source != 0);
                if(i == 0 || ctbr->influence < d->affected[weakest].influence)
                {
                    weakest = i;
                }
            }

            if(intensity <= d->affected[weakest].influence)
                return;

            slot = weakest;
            ctbr->source->audienceForDeletion -= d;
            ctbr->source = 0;
        }
    }

    DENG_ASSERT(slot >= 0 && slot < MAX_AFFECTED);
    ctbr = &d->affected[slot];

    // When reactivating latent contributions if the intensity
    // has not changed we don't need to force an update.
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
    // Everything that is affected by the changed lights will need an update.
    Contributor *ctbr = d->affected;
    for(int i = 0; i < MAX_AFFECTED; ++i, ctbr++)
    {
        if(!ctbr->source)
            continue;

        if(changes.check(App_World().map().toIndex(*ctbr->source)))
        {
            d->changedContributions |= 1 << i;
            break;
        }
    }
}

void BiasSurface::updateAfterMove()
{
    Contributor *ctbr = d->affected;
    for(int i = 0; i < MAX_AFFECTED; ++i, ctbr++)
    {
        if(ctbr->source)
        {
            ctbr->source->forceUpdate();
        }
    }
}

void BiasSurface::lightPoly(Vector3f const &surfaceNormal, int vertCount,
    rvertex_t const *positions, ColorRawf *colors)
{
    DENG_ASSERT(vertCount == d->illums.count()); // sanity check
    DENG_ASSERT(positions != 0 && colors != 0);

    // Configure global arguments for evalLighting(), for perf
    bspRoot = &App_World().map().bspRoot();
    biasTime = bspRoot->map().biasCurrentTime();
    mapSurfaceNormal = &surfaceNormal;

    int i; rvertex_t const *vtx = positions;
    for(i = 0; i < vertCount; ++i, vtx++)
    {
        Vector3d origin(vtx->pos[VX], vtx->pos[VY], vtx->pos[VZ]);
        Vector3f light = d->evalLighting(d->illums[i], origin);

        for(int c = 0; c < 3; ++c)
            colors[i].rgba[c] = light[c];
    }

    // Any changes to contributors will have now been applied.
    d->changedContributions = 0;
}
