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

#include "render/rend_bias.h" // BiasTracker

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
    int sourceIdx; /// Light source index.
};

struct Affection
{
    float intensities[MAX_AFFECTED];
    int numFound;
    BiasAffection *affected;

    void addAffected(int sourceIdx, float intensity)
    {
        if(numFound < MAX_AFFECTED)
        {
            affected[numFound].sourceIdx = sourceIdx;
            intensities[numFound] = intensity;
            numFound++;
        }
        else
        {
            // Drop the weakest.
            int weakest = 0;

            for(int i = 1; i < MAX_AFFECTED; ++i)
            {
                if(intensities[i] < intensities[weakest])
                    weakest = i;
            }

            affected[weakest].sourceIdx = sourceIdx;
            intensities[weakest] = intensity;
        }
    }
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
        short source;       ///< Index of the source.
        de::Vector3f color; ///< The contributed light intensity.
    };

    Vector3f color;  /// Current light color at the vertex.
    Vector3f dest;   /// Destination light color at the vertex (interpolated to).
    uint updateTime; /// When the value was calculated.
    Flags flags;
    Contribution casted[MAX_AFFECTED];

    VertexIllum() : updateTime(0), flags(StillUnseen)
    {
        for(int i = 0; i < MAX_AFFECTED; ++i)
        {
            casted[i].source = -1;
        }
    }

    /**
     * Interpolate between current and destination.
     */
    void lerp(uint currentTime, int lightSpeed, Vector3f &result)
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
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VertexIllum::Flags)

static float biasDot(Vector3d const &sourceOrigin, Vector3d const &surfacePoint,
                     Vector3f const &surfaceNormal)
{
    Vector3d delta = (sourceOrigin - surfacePoint).normalize();
    return delta.dot(surfaceNormal);
}

static void addLight(Vector3f &dest, Vector3f const &color, float howMuch = 1.0f)
{
    for(int i = 0; i < 3; ++i)
    {
        float newval = dest[i] + (color[i] * howMuch);

        if(newval > 1)
            newval = 1;

        dest[i] = newval;
    }
}

/**
 * @return Light contributed by the specified source.
 */
static Vector3f *getCasted(VertexIllum *illum, int sourceIndex,
    BiasAffection *affectedSources)
{
    DENG_ASSERT(illum && affectedSources);

    for(int i = 0; i < MAX_AFFECTED; ++i)
    {
        if(illum->casted[i].source == sourceIndex)
            return &illum->casted[i].color;
    }

    // Choose an array element not used by the affectedSources.
    for(int i = 0; i < MAX_AFFECTED; ++i)
    {
        bool inUse = false;
        for(int k = 0; k < MAX_AFFECTED; ++k)
        {
            if(affectedSources[k].sourceIdx < 0)
                break;

            if(affectedSources[k].sourceIdx == illum->casted[i].source)
            {
                inUse = true;
                break;
            }
        }

        if(!inUse)
        {
            illum->casted[i].source = sourceIndex;
            illum->casted[i].color.x =
                illum->casted[i].color.y =
                    illum->casted[i].color.z = 0;

            return &illum->casted[i].color;
        }
    }

    // Now how'd that happen?
    throw Error("getCasted", QString("No light emitted by source #%1").arg(sourceIndex));
}

static Vector3f ambientLight(Map &map, Vector3d const &point)
{
    if(map.hasLightGrid())
        return map.lightGrid().evaluate(point);
    return Vector3f(0, 0, 0);
}

/**
 * evalPoint uses these, so they must be set before it is called.
 */
static float biasAmount;
static BiasTracker trackChanged;
static BiasTracker trackApplied;

DENG2_PIMPL_NOREF(BiasSurface)
{
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

    void updateAffected(Vector2d const &from, Vector2d const &to,
                        Vector3f const &normal)
    {
        Map &map = App_World().map();

        Affection aff;
        aff.affected = affected;
        aff.numFound = 0;
        std::memset(aff.affected, -1, sizeof(affected));

        Vector2f delta;
        foreach(BiasSource *src, map.biasSources())
        {
            if(src->intensity() <= 0)
                continue;

            // Calculate minimum 2D distance to the segment.
            float distance = 0;
            for(int k = 0; k < 2; ++k)
            {
                if(!k)
                    delta = Vector2f(from - Vector2d(src->origin()));
                else
                    delta = Vector2f(to - Vector2d(src->origin()));

                float len = delta.length();
                if(k == 0 || len < distance)
                    distance = len;
            }

            if(delta.normalize().dot(normal) >= 0)
                continue;

            if(distance < 1)
                distance = 1;

            float intensity = src->intensity() / distance;

            // Is the source is too weak, ignore it entirely.
            if(intensity < MIN_INTENSITY)
                continue;

            aff.addAffected(map.toIndex(*src), intensity);
        }
    }

    void updateAffected(struct rvertex_s const *rvertices,
        size_t numVertices, Vector3d const &point, Vector3f const &normal)
    {
        DENG_ASSERT(rvertices != 0);
        DENG_UNUSED(numVertices);

        Map &map = App_World().map();

        Affection aff;
        aff.affected = affected;
        aff.numFound = 0;
        std::memset(aff.affected, -1, sizeof(affected)); // array of MAX_BIAS_AFFECTED

        Vector2f delta;
        foreach(BiasSource *src, map.biasSources())
        {
            if(src->intensity() <= 0)
                continue;

            // Calculate minimum 2D distance to the BSP leaf.
            /// @todo This is probably too accurate an estimate.
            coord_t distance = 0;
            for(int k = 0; k < illums.size(); ++k)
            {
                float const *vtxPos = rvertices[k].pos;
                delta = Vector2d(vtxPos[VX], vtxPos[VY]) - Vector2d(src->origin());

                float len = delta.length();
                if(k == 0 || len < distance)
                    distance = len;
            }
            if(distance < 1)
                distance = 1;

            // Estimate the effect on this surface.
            float dot = biasDot(src->origin(), point, normal);
            if(dot <= 0)
                continue;

            float intensity = src->intensity() / distance;

            // Is the source is too weak, ignore it entirely.
            if(intensity < MIN_INTENSITY)
                continue;

            aff.addAffected(map.toIndex(*src), intensity);
        }
    }

    /**
     * Apply shadow bias to the given map surface @a point (at a vertex).
     *
     * @todo Only recalculate the changed lights (colors contributed by others
     * can be stored in @ref BiasSurface::affected).
     */
    void evalPoint(Vector3f &light, int vertexOrd, Vector3d const &point,
                   Vector3f const &normal)
    {
#define COLOR_CHANGE_THRESHOLD  0.1f

        Map &map = App_World().map();

        VertexIllum &vi = illums[vertexOrd];
        BiasAffection *affectedSources = affected;

        bool illuminationChanged = false;
        uint latestSourceUpdate = 0;
        struct {
            int index;
            BiasSource *source;
            BiasAffection *affection;
            bool changed;
        } affecting[MAX_AFFECTED + 1], *aff;

        // Vertices that are rendered for the first time need to be fully
        // evaluated.
        if(vi.flags & VertexIllum::StillUnseen)
        {
            illuminationChanged = true;
            vi.flags &= ~VertexIllum::StillUnseen;
        }

        // Determine if any of the affecting lights have changed since
        // last frame.
        aff = affecting;
        if(map.biasSourceCount() > 0)
        {
            for(int i = 0; affectedSources[i].sourceIdx >= 0 && i < MAX_AFFECTED; ++i)
            {
                int idx = affectedSources[i].sourceIdx;

                // Is this a valid index?
                if(idx < 0 || idx >= map.biasSourceCount())
                    continue;

                aff->index = idx;
                aff->source = map.biasSource(idx);
                aff->affection = &affectedSources[i];

                if(trackChanged.check(idx))
                {
                    aff->changed = true;
                    illuminationChanged = true;
                    trackApplied.mark(idx);

                    // Keep track of the earliest time when an affected source changed.
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

        if(!illuminationChanged)
        {
            // Reuse the previous value.
            vi.lerp(map.biasCurrentTime(), lightSpeed, light);

            // Add ambient lighting.
            addLight(light, ambientLight(map, point));

            return;
        }

        // Initial color is black.
        Vector3f newColor;

        // Calculate the contribution from each light.
        for(aff = affecting; aff->source; aff++)
        {
            if(!aff->changed) //SB_TrackerCheck(&trackChanged, aff->index))
            {
                // We can reuse the previously calculated value.  This can
                // only be done if this particular light source hasn't
                // changed.
                continue;
            }

            BiasSource *s = aff->source;

            Vector3f *casted = getCasted(&vi, aff->index, affectedSources);

            Vector3d delta = s->origin() - point;
            Vector3d surfacePoint = point + delta / 100;

            if(devUseSightCheck &&
               !LineSightTest(s->origin(), surfacePoint).trace(map.bspRoot()))
            {
                // LOS fail.
                if(casted)
                {
                    // This affecting source does not contribute any light.
                    *casted = Vector3f();
                }
                continue;
            }

            double distance = delta.length();
            double dot = delta.normalize().dot(normal);

            // The surface faces away from the light.
            if(dot <= 0)
            {
                if(casted)
                {
                    *casted = Vector3f();
                }

                continue;
            }

            // Apply light casted from this source.
            float strength = de::clamp(0.f, float( dot * s->intensity() / distance ), 1.f);
            *casted = s->color() * strength;
        }

        // Combine the casted light from each source.
        for(aff = affecting; aff->source; aff++)
        {
            Vector3f *casted = getCasted(&vi, aff->index, affectedSources);

            for(int i = 0; i < 3; ++i)
            {
                newColor[i] = de::clamp(0.f, newColor[i] + (*casted)[i], 1.f);
            }
        }

        // Is there a new destination?
        if(!(vi.dest.x < newColor.x + COLOR_CHANGE_THRESHOLD &&
             vi.dest.x > newColor.x - COLOR_CHANGE_THRESHOLD) ||
           !(vi.dest.y < newColor.y + COLOR_CHANGE_THRESHOLD &&
             vi.dest.y > newColor.y - COLOR_CHANGE_THRESHOLD) ||
           !(vi.dest.z < newColor.z + COLOR_CHANGE_THRESHOLD &&
             vi.dest.z > newColor.z - COLOR_CHANGE_THRESHOLD))
        {
            if(vi.flags & VertexIllum::Interpolating)
            {
                // Must not lose the half-way interpolation.
                Vector3f mid; vi.lerp(map.biasCurrentTime(), lightSpeed, mid);

                // This is current color at this very moment.
                vi.color = mid;
            }

            // This is what we will be interpolating to.
            vi.dest       = newColor;
            vi.flags     |= VertexIllum::Interpolating;
            vi.updateTime = latestSourceUpdate;
        }

        vi.lerp(map.biasCurrentTime(), lightSpeed, light);

        // Add ambient lighting.
        addLight(light, ambientLight(map, point));

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
    for(int i = 0; i < MAX_AFFECTED && d->affected[i].sourceIdx >= 0; ++i)
    {
        map.biasSource(d->affected[i].sourceIdx)->forceUpdate();
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
        if(d->affected[i].sourceIdx < 0)
            break;

        if(changes.check(d->affected[i].sourceIdx))
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
    struct rvertex_s const *verts, size_t vertCount,
    Vector3f const &surfaceNormal, float sectorLightLevel)
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

    std::memcpy(&trackChanged, &d->tracker, sizeof(trackChanged));
    std::memset(&trackApplied, 0, sizeof(trackApplied));

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
                                  surfaceNormal);
            }
            else
            {
                BspLeaf const *bspLeaf = d->owner.as<BspLeaf>();

                Vector3d surfacePoint(bspLeaf->poly().center(),
                                      bspLeaf->sector().plane(d->subElemIndex).height());

                d->updateAffected(verts, vertCount, surfacePoint, surfaceNormal);
            }
        }
    }

    for(uint i = 0; i < vertCount; ++i)
    {
        rvertex_t const &vtx = verts[i];
        Vector3f light(colors[i].rgba);

        d->evalPoint(light, i, Vector3d(vtx.pos[VX], vtx.pos[VY], vtx.pos[VZ]),
                     surfaceNormal);

        for(int c = 0; c < 3; ++c)
            colors[i].rgba[c] = light[c];
    }

    d->tracker.clear(trackApplied);
}
