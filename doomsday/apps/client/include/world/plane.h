/** @file plane.h  Map plane.
 * @ingroup world
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#pragma once

#if defined(__SERVER__)
#  error "plane.h is only for Client"
#endif

#include "world/surface.h"
#include <doomsday/world/plane.h>
#include <array>

#include "def_main.h"  // ded_ptcgen_t

struct Generator;
class Map;
class ClPlaneMover;
class Surface;

namespace world { class Sector; }

class Plane : public world::Plane
    , DE_OBSERVES(world::Surface, MaterialChange)
{
public:
    /// No generator is attached. @ingroup errors
    DE_ERROR(MissingGeneratorError);

    /// Notified whenever a @em smoothed height change occurs.
    DE_DEFINE_AUDIENCE(HeightSmoothedChange, void planeHeightSmoothedChanged(Plane &plane))

public:
    Plane(world::Sector &sector, const de::Vec3f &normal = de::Vec3f(0, 0, 1), double height = 0);

    ~Plane() override;

    Map &map() const;
    Surface &surface() { return world::Plane::surface().as<Surface>(); }
    const Surface &surface() const { return world::Plane::surface().as<Surface>(); }

    void setHeight(double newHeight) override;

    /**
     * Returns the current smoothed height of the plane (interpolated) in the
     * map coordinate space.
     *
     * @see heightTarget(), height()
     */
    double heightSmoothed() const;

    /**
     * Returns the delta between current height and the smoothed height of the
     * plane in the map coordinate space.
     *
     * @see heightSmoothed(), heightTarget()
     */
    double heightSmoothedDelta() const;

    /**
     * Perform smoothed height interpolation.
     *
     * @see heightSmoothed(), heightTarget()
     */
    void lerpSmoothedHeight();

    /**
     * Reset the plane's height tracking buffer (for smoothing).
     *
     * @see heightSmoothed(), heightTarget()
     */
    void resetSmoothedHeight();

    /**
     * Roll the plane's height tracking buffer.
     *
     * @see heightTarget()
     */
    void updateHeightTracking();

    /**
     * Returns @c true iff a generator is attached to the plane.
     *
     * @see generator()
     */
    bool hasGenerator() const;

    /**
     * Returns the generator attached to the plane.
     *
     * @see hasGenerator()
     */
    Generator &generator() const;

    /**
     * Creates a new flat-triggered particle generator based on the given
     * definition. Note that it may @em not be "this" plane to which the resultant
     * generator is attached as the definition may override this.
     */
    void spawnParticleGen(const ded_ptcgen_t *def);

    void addMover(ClPlaneMover &mover);
    void removeMover(ClPlaneMover &mover);

    /**
     * Determines whether the plane qualifies as a FakeRadio shadow caster (onto walls).
     */
    bool castsShadow() const;

    /**
     * Determines whether the plane qualifies as a FakeRadio shadow receiver (from walls).
     */
    bool receivesShadow() const;

private:
    Generator *tryFindGenerator() const;
    void notifySmoothedHeightChanged();
    void surfaceMaterialChanged(world::Surface &) override;

    std::array<double, 2> _oldHeight;  // Sharp height change tracking buffer (for smoothing).
    double _heightSmoothed = 0;
    double _heightSmoothedDelta = 0;   // Delta between the current sharp height and the visual height.
    ClPlaneMover *_mover = nullptr;    // The current mover.
};
