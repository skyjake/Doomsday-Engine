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

#include <QVector>

#include "de_base.h"
#include "de_console.h"

#include "world/map.h"
#include "world/linesighttest.h"
#include "BspLeaf"
#include "Segment"

#include "BiasTracker"
#include "render/rendpoly.h"

#include "render/biassurface.h"

using namespace de;

/// Ignore light intensities below this threshold when accumulating sources.
static float const MIN_INTENSITY = .005f;

/// Maximum number of sources which can contribute light to a vertex.
static int const MAX_AFFECTED = 6;

static float lightMin        = .85f; //cvar
static float lightMax        = 1.f;  //cvar
static int lightSpeed        = 130;  //cvar
static int devUpdateAffected = true; //cvar
static int devUseSightCheck  = true; //cvar

struct BiasAffection
{
    int sourceIndex;
};

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

        /// Vertex is still unseen (color is unknown).
        StillUnseen   = 0x2
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    /**
     * Light contribution from affecting sources.
     */
    struct Contribution
    {
        int sourceIndex; ///< Index of the source.
        Vector3f color;  ///< The contributed light intensity.
    };

    Vector3f color;  ///< Current light color at the vertex.
    Vector3f dest;   ///< Destination light color at the vertex (interpolated to).
    uint updateTime; ///< When the value was calculated.
    Flags flags;
    Contribution casted[MAX_AFFECTED];

    VertexIllum() : updateTime(0), flags(StillUnseen)
    {
        for(int i = 0; i < MAX_AFFECTED; ++i)
        {
            casted[i].sourceIndex = -1;
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
    Vector3f &contribution(int sourceIndex, BiasAffection *const affectedSources)
    {
        // Do we already have a contribution for this source?
        for(int i = 0; i < MAX_AFFECTED; ++i)
        {
            if(casted[i].sourceIndex == sourceIndex)
                return casted[i].color;
        }

        // Choose an element not in use by the affectedSources.
        for(int i = 0; i < MAX_AFFECTED; ++i)
        {
            bool inUse = false;
            for(int k = 0; k < MAX_AFFECTED; ++k)
            {
                if(affectedSources[k].sourceIndex < 0)
                    break;

                if(affectedSources[k].sourceIndex == casted[i].sourceIndex)
                {
                    inUse = true;
                    break;
                }
            }

            if(!inUse)
            {
                // This will do nicely.
                casted[i].sourceIndex = sourceIndex;
                casted[i].color       = Vector3f();

                return casted[i].color;
            }
        }

        // Now how'd that happen?
        throw Error("VertexIllum::casted", QString("No light emitted by source #%1").arg(sourceIndex));
    }
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VertexIllum::Flags)

static inline float biasDot(Vector3d const &sourceOrigin, Vector3d const &surfacePoint,
                            Vector3f const &surfaceNormal)
{
    return (sourceOrigin - surfacePoint).normalize().dot(surfaceNormal);
}

static inline void addLight(Vector3f &dest, Vector3f const &color, float howMuch = 1.0f)
{
    dest = (dest + color * howMuch).min(Vector3f(1, 1, 1));
}

static Vector3f ambientLight(Map &map, Vector3d const &point)
{
    if(map.hasLightGrid())
        return map.lightGrid().evaluate(point);
    return Vector3f();
}

/**
 * evalLighting uses these -- they must be set before it is called.
 */
static float biasAmount;
static BiasTracker trackChanged;
static BiasTracker trackApplied;

DENG2_PIMPL_NOREF(BiasSurface)
{
    struct Affection
    {
        float intensities[MAX_AFFECTED]; // init unnecessary
        int numFound;

        Affection() : numFound(0) {}

        int findWeakest() const
        {
            int weakest = 0;
            for(int i = 1; i < MAX_AFFECTED; ++i)
            {
                if(intensities[i] < intensities[weakest])
                    weakest = i;
            }
            return weakest;
        }
    };

    MapElement &owner;

    int subElemIndex;

    QVector<VertexIllum> illums; /// @todo use std::vector instead?

    BiasTracker tracker;

    BiasAffection affected[MAX_AFFECTED];

    uint lastUpdateOnFrame;

    Instance(MapElement &owner, int subElemIndex, int size)
        : owner(owner),
          subElemIndex(subElemIndex),
          illums(size),
          lastUpdateOnFrame(0)
    {}

    inline Surface &mapSurface()
    {
        if(owner.type() == DMU_SEGMENT)
            return owner.as<Segment>()->lineSide().middle();
        // Must be a BspLeaf, then.
        return owner.as<BspLeaf>()->sector().plane(subElemIndex).surface();
    }

    inline void clearAffected()
    {
        std::memset(affected, -1, sizeof(affected));
    }

    void addAffected(int sourceIndex, float intensity, Affection &aff)
    {
        if(aff.numFound < MAX_AFFECTED)
        {
            affected[aff.numFound].sourceIndex = sourceIndex;
            aff.intensities[aff.numFound] = intensity;
            aff.numFound++;
        }
        else
        {
            // Drop the weakest.
            int weakest = aff.findWeakest();

            affected[weakest].sourceIndex = sourceIndex;
            aff.intensities[weakest] = intensity;
        }
    }

    void updateAffected(Vector2d const &from, Vector2d const &to,
                        Vector3f const &surfaceNormal)
    {
        DENG_ASSERT(illums.count() == 4);

        Map &map = App_World().map();

        clearAffected();

        Affection aff;
        foreach(BiasSource *src, map.biasSources())
        {
            if(src->intensity() <= 0)
                continue;

            // Calculate minimum 2D distance to the segment.
            Vector2f delta;
            float distance = 0;
            for(int k = 0; k < 2; ++k)
            {
                delta = ((!k? from : to) - Vector2d(src->origin()));
                float len = delta.length();
                if(k == 0 || len < distance)
                    distance = len;
            }

            if(delta.normalize().dot(surfaceNormal) >= 0)
                continue;

            if(distance < 1)
                distance = 1;

            float intensity = src->intensity() / distance;

            // Is the source is too weak, ignore it entirely.
            if(intensity < MIN_INTENSITY)
                continue;

            addAffected(map.toIndex(*src), intensity, aff);
        }
    }

    void updateAffected(rvertex_t const *verts, int vertCount,
        Vector3d const &surfacePoint, Vector3f const &surfaceNormal)
    {
        DENG_ASSERT(illums.count() == vertCount && verts != 0);

        Map &map = App_World().map();

        clearAffected();

        Affection aff;
        foreach(BiasSource *src, map.biasSources())
        {
            if(src->intensity() <= 0)
                continue;

            // Calculate minimum 2D distance to the BSP leaf.
            /// @todo This is probably too accurate an estimate.
            float distance = 0;
            for(int k = 0; k < illums.count(); ++k)
            {
                Vector2d const vertPos = Vector2d(verts[k].pos[VX], verts[k].pos[VY]);

                float len = (vertPos - Vector2d(src->origin())).length();
                if(k == 0 || len < distance)
                    distance = len;
            }
            if(distance < 1)
                distance = 1;

            // Estimate the effect on this surface.
            float dot = biasDot(src->origin(), surfacePoint, surfaceNormal);
            if(dot <= 0)
                continue;

            float intensity = src->intensity() / distance;

            // Is the source is too weak, ignore it entirely.
            if(intensity < MIN_INTENSITY)
                continue;

            addAffected(map.toIndex(*src), intensity, aff);
        }
    }

    /**
     * Perform lighting calculations for the specified @a mapVertexOrigin.
     *
     * @todo Only recalculate the changed lights (colors contributed by
     * others can be stored in the "affected" array.
     */
    Vector3f evalLighting(VertexIllum &vi, Vector3d const &mapVertexOrigin,
                          Vector3f const &mapSurfaceNormal)
    {
#define COLOR_CHANGE_THRESHOLD  0.1f

        Map &map = App_World().map();
        uint currentTime = map.biasCurrentTime();

        bool illumChanged = false;
        uint latestSourceUpdate = 0;
        struct {
            int index;
            BiasSource *source;
            BiasAffection *affection;
            bool changed;
        } affecting[MAX_AFFECTED + 1], *aff;

        // Lighting must be fully evaluated the first time.
        if(vi.flags & VertexIllum::StillUnseen)
        {
            illumChanged = true;
            vi.flags &= ~VertexIllum::StillUnseen;
        }

        // Determine if any affecting sources have changed since last frame.
        aff = affecting;
        if(map.biasSourceCount() > 0)
        {
            for(int i = 0; affected[i].sourceIndex >= 0 && i < MAX_AFFECTED; ++i)
            {
                int idx = affected[i].sourceIndex;

                // Is this a valid index?
                if(idx < 0 || idx >= map.biasSourceCount())
                    continue;

                aff->index = idx;
                aff->source = map.biasSource(idx);
                aff->affection = &affected[i];

                if(trackChanged.check(idx))
                {
                    aff->changed = true;
                    illumChanged = true;
                    trackApplied.mark(idx);

                    // Remember the earliest time an affecting source changed.
                    if(latestSourceUpdate < aff->source->lastUpdateTime())
                    {
                        latestSourceUpdate = aff->source->lastUpdateTime();
                    }
                }
                else
                {
                    aff->changed = false;
                }

                // Move to the next.
                aff++;
            }
        }
        aff->source = 0;

        Vector3f color;
        if(illumChanged)
        {
            // Recalculate the contribution for each light.
            for(aff = affecting; aff->source; aff++)
            {
                if(!aff->changed) //tracker.check(aff->index))
                {
                    // We can reuse the previously calculated value. This
                    // can only be done if this particular source hasn't
                    // changed.
                    continue;
                }

                BiasSource *s = aff->source;

                Vector3f &casted = vi.contribution(aff->index, affected);
                Vector3d delta = s->origin() - mapVertexOrigin;

                if(devUseSightCheck &&
                   !LineSightTest(s->origin(), mapVertexOrigin + delta / 100).trace(map.bspRoot()))
                {
                    // LOS fail.
                    // This affecting source does not contribute any light.
                    casted = Vector3f();
                    continue;
                }

                double distance = delta.length();
                double dot = delta.normalize().dot(mapSurfaceNormal);

                // The surface faces away from the light?
                if(dot <= 0)
                {
                    casted = Vector3f();
                    continue;
                }

                // Apply light casted from this source.
                float strength = de::clamp(0.f, float( dot * s->intensity() / distance ), 1.f);
                casted = s->color() * strength;
            }

            /*
             * Accumulate light contributions from each affecting source.
             */
            Vector3f newColor; // Initial color is black.
            Vector3f const saturated(1, 1, 1);
            for(aff = affecting; aff->source; aff++)
            {
                newColor += vi.contribution(aff->index, affected);

                // Stop once fully saturated.
                if(newColor >= saturated)
                    break;
            }
            // Clamp.
            newColor = newColor.min(saturated);

            // Is there a new destination?
            if(!de::fequal(vi.dest.x, newColor.x, COLOR_CHANGE_THRESHOLD) ||
               !de::fequal(vi.dest.y, newColor.y, COLOR_CHANGE_THRESHOLD) ||
               !de::fequal(vi.dest.z, newColor.z, COLOR_CHANGE_THRESHOLD))
            {
                if(vi.flags & VertexIllum::Interpolating)
                {
                    // Must not lose the half-way interpolation.
                    Vector3f mid; vi.lerp(mid, currentTime);

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
        vi.lerp(color, currentTime);

        // Apply the ambient light term.
        addLight(color, ambientLight(map, mapVertexOrigin));

        return color;

#undef COLOR_CHANGE_THRESHOLD
    }
};

BiasSurface::BiasSurface(MapElement &owner, int subElemIndex, int size)
    : d(new Instance(owner, subElemIndex, size))
{}

void BiasSurface::consoleRegister() // static
{
    C_VAR_FLOAT ("rend-bias-min",           &lightMin,          0, 0, 1);
    C_VAR_FLOAT ("rend-bias-max",           &lightMax,          0, 0, 1);
    C_VAR_INT   ("rend-bias-lightspeed",    &lightSpeed,        0, 0, 5000);

    // Development variables.
    C_VAR_INT   ("rend-dev-bias-affected",  &devUpdateAffected, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT   ("rend-dev-bias-sight",     &devUseSightCheck,  CVF_NO_ARCHIVE, 0, 1);
}

void BiasSurface::updateAfterMove()
{
    Map &map = App_World().map();
    for(int i = 0; i < MAX_AFFECTED && d->affected[i].sourceIndex >= 0; ++i)
    {
        map.biasSource(d->affected[i].sourceIndex)->forceUpdate();
    }
}

void BiasSurface::updateAffection(BiasTracker &changes)
{
    // Everything that is affected by the changed lights will need an update.
    d->tracker.apply(changes);

    // Determine whether these changes affect us.
    bool needApplyChanges = false;
    for(int i = 0; i < MAX_AFFECTED; ++i)
    {
        if(d->affected[i].sourceIndex < 0)
            break;

        if(changes.check(d->affected[i].sourceIndex))
        {
            needApplyChanges = true;
            break;
        }
    }

    if(!needApplyChanges) return;

    // Mark the illumination unseen to force an update.
    for(int i = 0; i < d->illums.size(); ++i)
    {
        d->illums[i].flags |= VertexIllum::StillUnseen;
    }
}

void BiasSurface::lightPoly(struct ColorRawf_s *colors,
    struct rvertex_s const *verts, int vertCount, float sectorLightLevel)
{
    // Apply sectorlight bias (distance darkening is not a factor).
    if(sectorLightLevel > lightMin && lightMax > lightMin)
    {
        biasAmount = (sectorLightLevel - lightMin) / (lightMax - lightMin);

        if(biasAmount > 1)
            biasAmount = 1;
    }
    else
    {
        biasAmount = 0;
    }

    trackChanged = d->tracker;
    trackApplied.clear();

    Vector3f const &mapSurfaceNormal = d->mapSurface().normal();

    // Have any of the affected lights changed?
    if(devUpdateAffected)
    {
        Map &map = App_World().map();

        // If the data is already up to date, nothing needs to be done.
        if(d->lastUpdateOnFrame != map.biasLastChangeOnFrame())
        {
            d->lastUpdateOnFrame = map.biasLastChangeOnFrame();

            /**
             * @todo This could be enhanced so that only the lights on the right
             * side of the surface are taken into consideration.
             */
            if(d->owner.type() == DMU_SEGMENT)
            {
                Segment const *segment = d->owner.as<Segment>();

                d->updateAffected(segment->from().origin(), segment->to().origin(),
                                  mapSurfaceNormal);
            }
            else
            {
                BspLeaf const *bspLeaf = d->owner.as<BspLeaf>();
                Plane const &plane = bspLeaf->sector().plane(d->subElemIndex);

                Vector3d surfacePoint(bspLeaf->poly().center(), plane.visHeight());

                d->updateAffected(verts, vertCount, surfacePoint, mapSurfaceNormal);
            }
        }
    }

    int i; rvertex_t const *vtx = verts;
    for(i = 0; i < vertCount; ++i, vtx++)
    {
        Vector3d mapVertexOrigin(vtx->pos[VX], vtx->pos[VY], vtx->pos[VZ]);
        Vector3f light = d->evalLighting(d->illums[i], mapVertexOrigin, mapSurfaceNormal);

        for(int c = 0; c < 3; ++c)
            colors[i].rgba[c] = light[c];
    }

    d->tracker.remove(trackApplied);
}
