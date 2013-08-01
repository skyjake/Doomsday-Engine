/** @file biasillum.h Shadow Bias map point illumination.
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

#ifndef DENG_RENDER_SHADOWBIAS_ILLUMINATION_H
#define DENG_RENDER_SHADOWBIAS_ILLUMINATION_H

#include <de/Vector>

#include "render/rendpoly.h" /// @todo remove me

class BiasSurface;

/**
 * Stores map point lighting information for the Shadow Bias lighting model.
 * Used in conjunction with a BiasSurface (for routing change notifications).
 *
 * @ingroup render
 */
class BiasIllum
{
public:
    /// Maximum number of light contributions.
    static int const MAX_CONTRIBUTORS = 6;

    /// Minimum intensity for a light contributor.
    static float const MIN_INTENSITY; // .005f

public:
    BiasIllum(BiasSurface *surface);

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

    /**
     * (Re-)Evaluate lighting for this map point.
     *
     * @param color          Final color will be written here.
     * @param point          Point in the map to evaluate. Assumed not to have
     *                       moved since the last call unless the light source
     *                       contributions have since been updated.
     * @param normalAtPoint  Surface normal at @a point. Also assumed not to
     *                       have changed since the last call.
     * @param biasTime       Time in milliseconds of the last bias frame update.
     */
    void evaluate(de::Vector3f &color, de::Vector3d const &point,
                  de::Vector3f const &normalAtPoint, uint biasTime,
                  /// @todo Refactor away:
                  byte activeContributors, byte changedContributions);

    /// @copydoc evaluate()
    /// @todo refactor away
    void evaluate(ColorRawf &color, de::Vector3d const &point,
                  de::Vector3f const &normalAtPoint, uint biasTime,
                  /// @todo Refactor away:
                  byte activeContributors, byte changedContributions);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_RENDER_SHADOWBIAS_ILLUMINATION_H
