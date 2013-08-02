/** @file biasillum.cpp Shadow Bias map point illumination.
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

#include "de_base.h"
#include "de_console.h"

#include "world/map.h"
#include "world/linesighttest.h"
#include "BspLeaf"

#include "BiasTracker"

#include "render/biasillum.h"

using namespace de;

static int lightSpeed        = 130;  //cvar
static int devUseSightCheck  = true; //cvar

DENG2_PIMPL_NOREF(BiasIllum)
{
    BiasTracker *surface; ///< The "control" surface.
    Vector3f color;       ///< Current light color.
    Vector3f dest;        ///< Destination light color (interpolated to).
    uint updateTime;      ///< When the value was calculated.
    bool interpolating;   ///< Set to @c true during interpolation.

    /**
     * Cast lighting contributions from each source that affects the map point.
     * Order is the same as that in the affected surface.
     */
    Vector3f casted[MAX_CONTRIBUTORS];

    Instance(BiasTracker *surface)
        : surface(surface),
          updateTime(0),
          interpolating(false)
    {}

    /**
     * Returns a previous light contribution by unique contributor @a index.
     */
    inline Vector3f &contribution(int index)
    {
        DENG_ASSERT(index >= 0 && index < MAX_CONTRIBUTORS);
        return casted[index];
    }

    /**
     * Update any changed lighting contributions.
     *
     * @param activeContributors  Bit field denoting the active contributors.
     * @param biasTime            Time in milliseconds of the last bias frame update.
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
            updateTime    = surface->timeOfLatestContributorUpdate();
        }

#undef COLOR_CHANGE_THRESHOLD
    }

    /**
     * Update lighting contribution for the specified contributor @a index.
     *
     * @param index          Unique index of the contributor.
     * @param point          Point in the map to evaluate.
     * @param normalAtPoint  Surface normal at @a point.
     * @param bspRoot        Root BSP element for the map.
     */
    void updateContribution(int index, Vector3d const &point,
        Vector3f const &normalAtPoint, MapElement &bspRoot)
    {
        BiasSource const &source = surface->contributor(index);

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

        Vector3d sourceToPoint = source.origin() - point;

        if(devUseSightCheck &&
           !LineSightTest(source.origin(), point + sourceToPoint / 100)
                        .trace(bspRoot))
        {
            // LOS fail.
            // This affecting source does not contribute any light.
            casted = Vector3f();
            return;
        }

        double distance = sourceToPoint.length();
        double dot = sourceToPoint.normalize().dot(normalAtPoint);

        // The point faces away from the light?
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
     * Interpolate color from current to destination.
     *
     * @param result       Interpolated color will be written here.
     * @param currentTime  Time in milliseconds of the last bias frame update.
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

float const BiasIllum::MIN_INTENSITY = .005f;

BiasIllum::BiasIllum(BiasTracker *surface) : d(new Instance(surface))
{}

void BiasIllum::evaluate(Vector3f &color, Vector3d const &point,
    Vector3f const &normalAtPoint, uint biasTime,
    /// @todo Refactor away:
    byte activeContributors, byte changedContributions)
{
    // Does the surface have any lighting changes to apply?
    if(changedContributions)
    {
        if(activeContributors & changedContributions)
        {
            /// @todo Do not assume the current map.
            Map &map = App_World().map();

            /*
             * Recalculate the contribution for each changed light source.
             * Continue using the previously calculated value otherwise.
             */
            for(int i = 0; i < MAX_CONTRIBUTORS; ++i)
            {
                if(activeContributors & changedContributions & (1 << i))
                {
                    d->updateContribution(i, point, normalAtPoint, map.bspRoot());
                }
            }
        }

        // Accumulate light contributions and initiate interpolation.
        d->applyLightingChanges(activeContributors, biasTime);
    }

    // Factor in the current color (and perform interpolation if needed).
    d->lerp(color, biasTime);
}

void BiasIllum::evaluate(ColorRawf &color, Vector3d const &point,
    Vector3f const &normalAtPoint, uint biasTime,
    /// @todo Refactor away:
    byte activeContributors, byte changedContributions)
{
    Vector3f tmp; evaluate(tmp, point, normalAtPoint, biasTime,
                           activeContributors, changedContributions);

    for(int c = 0; c < 3; ++c)
        color.rgba[c] += tmp[c];
}

void BiasIllum::consoleRegister() // static
{
    C_VAR_INT("rend-bias-lightspeed",   &lightSpeed,        0, 0, 5000);

    // Development variables.
    C_VAR_INT("rend-dev-bias-sight",    &devUseSightCheck,  CVF_NO_ARCHIVE, 0, 1);
}
