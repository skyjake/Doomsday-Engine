/** @file biasillum.h  Shadow Bias map point illumination.
 *
 * @authors Copyright © 2005-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_RENDER_BIASILLUM_H
#define DENG_CLIENT_RENDER_BIASILLUM_H

#include <de/Error>
#include <de/Vector>

class BiasTracker;

/**
 * Map point illumination sampler for the Shadow Bias lighting model.
 *
 * Used in conjunction with a BiasTracker (for routing change notifications).
 *
 * @ingroup render
 */
class BiasIllum
{
public:
    /// Required tracker is missing. @ingroup errors
    DENG2_ERROR(MissingTrackerError);

    /// Maximum number of light contributions/contributors.
    static int const MAX_CONTRIBUTORS = 6;

    /// Minimum light contribution intensity.
    static float const MIN_INTENSITY; // .005f

public:
    /**
     * Construct a new bias illumination point.
     *
     * @param tracker  Lighting-contributor tracker to assign to the new point.
     *                 Note that @ref assignTracker() can be used later.
     */
    explicit BiasIllum(BiasTracker *tracker = 0);

    /**
     * (Re-)Evaluate lighting for the map point. Any queued changes to lighting
     * contributions will be applied at this time (note that this is however a
     * fast operation which does not block).
     *
     * @param point          Point in the map to evaluate. It is assumed this
     *                       has @em not moved since the last call unless the
     *                       light source contributors have been redetermined.
     *                       (Failure to do so will result in briefly visible
     *                       artefacts/inconsistent lighting at worst.)
     * @param normalAtPoint  Surface normal at @a point. Also assumed not to
     *                       have changed since the last call.
     * @param biasTime       Time in milliseconds of the last bias frame update
     *                       used for interpolation.
     *
     * @return  Current color at this time.
     */
    de::Vector3f evaluate(de::Vector3d const &point,
                          de::Vector3f const &normalAtPoint, uint biasTime);

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
     * @see hasTracker()
     */
    void setTracker(BiasTracker *newTracker);

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_RENDER_BIASILLUM_H
