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

#include <de/Error>
#include <de/Vector>

#include "render/rendpoly.h" /// @todo remove me

class BiasTracker;

/**
 * Stores map point lighting information for the Shadow Bias lighting model.
 * Used in conjunction with a BiasTracker (for routing change notifications).
 *
 * @ingroup render
 */
class BiasIllum
{
public:
    /// Required tracker is missing. @ingroup errors
    DENG2_ERROR(MissingTrackerError);

    /// Maximum number of light contributions.
    static int const MAX_CONTRIBUTORS = 6;

    /// Minimum intensity for a light contributor.
    static float const MIN_INTENSITY; // .005f

public:
    /**
     * Construct a new bias illumination point.
     *
     * @param tracker  Tracker to assigned to the new point (if any). Note that
     *                 @ref assignTracker() can be used later.
     */
    explicit BiasIllum(BiasTracker *tracker = 0);

    BiasIllum(BiasIllum const &other);
    BiasIllum &operator = (BiasIllum const &other);

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

    /**
     * Returns @c true iff a BiasTracker has been assigned for the illumination.
     *
     * @see setTracker()
     */
    bool hasTracker() const;

    /**
     * Provides access to the currently assigned tracker.
     *
     * @see hasTracker(), setTracker()
     */
    BiasTracker &tracker() const;

    /**
     * Assign the illumination point to the specified tracker.
     *
     * @param newTracker  New illumination tracker to be assigned. Use @c 0 to
     *                    unassign any current tracker.
     *
     * @see hasTracker()
     */
    void setTracker(BiasTracker *newTracker);

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
