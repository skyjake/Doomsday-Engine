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

#include <QMap>
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
 * Per-vertex illumination data.
 */
struct VertexIllum
{
    /// State flags:
    enum Flag
    {
        /// Interpolation is in progress.
        Interpolating = 0x1,

        /// Vertex is unseen (color is unknown).
        Unseen = 0x2
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    /**
     * Light contribution from affecting sources.
     */
    struct Contribution
    {
        BiasSource *source; ///< The contributing light source.
        Vector3f color;  ///< The contributed light intensity.
    };

    Vector3f color;  ///< Current light color at the vertex.
    Vector3f dest;   ///< Destination light color at the vertex (interpolated to).
    uint updateTime; ///< When the value was calculated.
    Flags flags;
    Contribution casted[MAX_AFFECTED];

    VertexIllum() : updateTime(0), flags(Unseen)
    {
        for(int i = 0; i < MAX_AFFECTED; ++i)
        {
            casted[i].source = 0;
        }
    }

    /**
     * Interpolate between current and destination.
     */
    void lerp(Vector3f &result, uint currentTime)
    {
        if(!flags.testFlag(Interpolating))
        {
            // No interpolation necessary -- use the current color.
            result = color;
            return;
        }

        float inter = (currentTime - updateTime) / float( lightSpeed );

        if(inter > 1)
        {
            flags &= ~Interpolating;
            color = dest;

            result = color;
        }
        else
        {
            result = color + (dest - color) * inter;
        }
    }

    /**
     * @return Light contribution by the specified source.
     */
    Vector3f &contribution(BiasSource *source, Contributors const &contributors)
    {
        DENG2_ASSERT(source != 0);

        // Do we already have a contribution for this source?
        for(int i = 0; i < MAX_AFFECTED; ++i)
        {
            if(casted[i].source == source)
                return casted[i].color;
        }

        // Choose an element not in use by the affectedSources.
        for(int i = 0; i < MAX_AFFECTED; ++i)
        {
            bool inUse = false;

            if(casted[i].source)
            {
                for(int k = 0; k < MAX_AFFECTED; ++k)
                {
                    Contributor const &ctbr = contributors[k];
                    if(ctbr.source == casted[i].source)
                    {
                        inUse = true;
                        break;
                    }
                }
            }

            if(!inUse)
            {
                // This will do nicely.
                casted[i].source = source;
                casted[i].color  = Vector3f();

                return casted[i].color;
            }
        }

        // Now how'd that happen?
        throw Error("VertexIllum::casted", QString("No light emitted by source"));
    }

    void updateContribution(int index, Contributors const &contributors,
        Vector3d const &surfacePoint, Vector3f const &surfaceNormal,
        MapElement const &bspRoot)
    {
        BiasSource *source = contributors[index].source;
        Vector3f &casted = contribution(source, contributors);

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

Q_DECLARE_OPERATORS_FOR_FLAGS(VertexIllum::Flags)

/**
 * evalLighting uses these -- they must be set before it is called.
 */
static uint biasTime;
static MapElement const *bspRoot;
static Vector3f const *mapSurfaceNormal;

/// @todo defer allocation of most data -- adopt a 'fly-weight' approach.
DENG2_PIMPL_NOREF(BiasSurface)
{
    QVector<VertexIllum> illums; /// @todo use std::vector instead?

    Contributors affected;
    byte activeContributors;
    byte changedContributors;

    uint lastUpdateOnFrame;

    Instance(int size)
        : illums(size),
          activeContributors(0),
          changedContributors(0),
          lastUpdateOnFrame(0)
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

        Map &map = bspRoot->map();

        uint latestSourceUpdate = 0;
        bool illumChanged = false;

        // Lighting must be fully evaluated the first time.
        if(vi.flags & VertexIllum::Unseen)
        {
            illumChanged = true;
            for(int i = 0; i < MAX_AFFECTED; ++i)
            {
                BiasSource *source = vi.casted[i].source;
                if(!source) continue;

                // Remember the earliest time an affecting source changed.
                if(latestSourceUpdate < source->lastUpdateTime())
                {
                    latestSourceUpdate = source->lastUpdateTime();
                }
            }
            vi.flags &= ~VertexIllum::Unseen;
        }

        /*
         * Determine if any affecting sources have changed since last frame.
         */

        Contributor *ctbr = affected;
        for(int i = 0; i < MAX_AFFECTED; ++i, ctbr++)
        {
            if(!(activeContributors & (1 << i))) continue;

            if(!(changedContributors & (1 << i))) continue;

            illumChanged = true;

            // Remember the earliest time an affecting source changed.
            if(latestSourceUpdate < ctbr->source->lastUpdateTime())
            {
                latestSourceUpdate = ctbr->source->lastUpdateTime();
            }
        }

        if(illumChanged)
        {
            /*
             * Recalculate the contribution for each light.
             * We can reuse the previously calculated value for a source if
             * it hasn't changed.
             */
            if(changedContributors)
            {
                Contributor *ctbr = affected;
                for(int i = 0; i < MAX_AFFECTED; ++i, ctbr++)
                {
                    if(changedContributors & (1 << i))
                    {
                        vi.updateContribution(i, affected, surfacePoint, *mapSurfaceNormal, *bspRoot);
                    }
                }
            }

            Vector3f newColor; // Initial color is black.

            /*
             * Accumulate light contributions from each affecting source.
             */
            if(activeContributors)
            {
                Contributor *ctbr = affected;
                for(int i = 0; i < MAX_AFFECTED; ++i, ctbr++)
                {
                    if(activeContributors & (1 << i))
                    {
                        newColor += vi.contribution(ctbr->source, affected);

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
                if((vi.flags & VertexIllum::Interpolating) && activeContributors)
                {
                    // Must not lose the half-way interpolation.
                    Vector3f mid; vi.lerp(mid, biasTime);

                    // This is current color at this very moment.
                    vi.color = mid;
                }

                // This is what we will be interpolating to.
                vi.dest       = newColor;
                vi.flags     |= VertexIllum::Interpolating;
                vi.updateTime = latestSourceUpdate;
            }
        }

        // Finalize lighting (i.e., perform interpolation if needed).
        Vector3f color;
        vi.lerp(color, biasTime);

        // Apply an ambient light term?
        if(map.hasLightGrid())
        {
            color = (color + map.lightGrid().evaluate(surfacePoint))
                    .min(saturated); // clamp
        }

        return color;

#undef COLOR_CHANGE_THRESHOLD
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

    // Do we have a latent contribution, or a spare slot?
    int weakest = -1;
    int slot = 0;
    Contributor *ctbr = d->affected;
    for(; slot < MAX_AFFECTED; ++slot, ctbr++)
    {
        // Remember the weakest.
        if(weakest < 0 || ctbr->influence < d->affected[weakest].influence)
            weakest = slot;

        // A latent contribution?
        if(ctbr->source == source)
            break;
    }

    if(slot == MAX_AFFECTED)
    {
        // No -- drop the weakest.
        if(intensity <= d->affected[weakest].influence)
            return;

        slot = weakest;
        ctbr = &d->affected[slot];
    }

    ctbr->source = source;
    ctbr->influence = intensity;
    d->activeContributors |= 1 << slot;
}

void BiasSurface::updateAffection(BiasTracker &changes)
{
    // Everything that is affected by the changed lights will need an update.

    // Determine whether these changes affect us.
    bool needApplyChanges = false;
    Contributor *ctbr = d->affected;
    for(int i = 0; i < MAX_AFFECTED; ++i, ctbr++)
    {
        if(!ctbr->source)
            continue;

        if(changes.check(App_World().map().toIndex(*ctbr->source)))
        {
            needApplyChanges = true;
            d->changedContributors |= 1 << i;
            break;
        }
    }

    if(!needApplyChanges) return;

    // Mark the illumination unseen to force an update.
    for(int i = 0; i < d->illums.size(); ++i)
    {
        d->illums[i].flags |= VertexIllum::Unseen;
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

    // Any changes to active contributors will have now been applied.
    d->changedContributors &= ~d->activeContributors;
}
