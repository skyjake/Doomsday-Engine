/** @file surface.h  Map surface.
 * @ingroup world
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#include <doomsday/world/surface.h>

class Decoration;
class Map;
class MaterialAnimator;

class Surface : public world::Surface
{
public:
    Surface(world::MapElement &owner,
            float opacity        = 1,
            const de::Vec3f &color = de::Vec3f(1));

    ~Surface();

    MaterialAnimator *materialAnimator() const;

    /**
     * Resets all lookups that are used for accelerating common operations.
     */
    void resetLookups() override;

//- Origin smoothing --------------------------------------------------------------------

    /// Notified when the @em sharp material origin changes.
    DE_DEFINE_AUDIENCE(OriginSmoothedChange, void surfaceOriginSmoothedChanged(Surface &surface))

    void notifyOriginSmoothedChanged();

    /// Maximum speed for a smoothed material offset.
    static const int MAX_SMOOTH_MATERIAL_MOVE = 8;

    /**
     * Returns the current smoothed (interpolated) material origin for the
     * surface in the map coordinate space.
     *
     * @see setOrigin()
     */
    const de::Vec2f &originSmoothed() const;

    /**
     * Returns the delta between current and the smoothed material origin for
     * the surface in the map coordinate space.
     *
     * @see setOrigin(), smoothOrigin()
     */
    const de::Vec2f &originSmoothedAsDelta() const;

    /**
     * Perform smoothed material origin interpolation.
     *
     * @see originSmoothed()
     */
    void lerpSmoothedOrigin();

    /**
     * Reset the surface's material origin tracking.
     *
     * @see originSmoothed()
     */
    void resetSmoothedOrigin();

    /**
     * Roll the surface's material origin tracking buffer.
     */
    void updateOriginTracking();

//---------------------------------------------------------------------------------------

    /**
     * Determine the glow properties of the surface, which, are derived from the
     * bound material (averaged color).
     *
     * @param color  Amplified glow color is written here.
     *
     * @return  Glow strength/intensity or @c 0 if not presently glowing.
     */
    float glow(de::Vec3f &color) const;

    Map &map() const;

private:
    de::Vec2f _oldOrigin[2];                      ///< Old @em sharp surface space material origins, for smoothing.
    de::Vec2f _originSmoothed;                    ///< @em smoothed surface space material origin.
    de::Vec2f _originSmoothedDelta;               ///< Delta between @em sharp and @em smoothed.
    mutable MaterialAnimator *_matAnimator = nullptr; // created on demand
};

