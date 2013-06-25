/** @file huecircle.h HueCircle manipulator, for runtime map editing.
 *
 * @authors Copyright © 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_HUECIRCLE_H
#define DENG_WORLD_HUECIRCLE_H

#include <de/Vector>

/**
 * Color manipulator for runtime map editing.
 *
 * @ingroup world
 */
class HueCircle
{
public:
    HueCircle();

    /**
     * Determine the absolute origin of the hue circle in the map coordinate space
     * (which is relative to the origin of the viewer).
     */
    de::Vector3d origin(de::Vector3d const &viewOrigin, double distance = 100) const;

    /**
     * Change the orientation of the hue circle.
     *
     * @param frontVec  New front vector.
     * @param sideVec   New side vector.
     * @param upVec     New up vector.
     */
    void setOrientation(de::Vector3f const &frontVec, de::Vector3f const &sideVec,
                        de::Vector3f const &upVec);

    /**
     * Determine a color by comparing the relative direction of the specified
     * front vector (the viewer) relative to the orientation of the hue circle.
     */
    de::Vector3f colorAt(de::Vector3f const &viewFrontVec, float *angle = 0,
                         float *sat = 0) const;

    de::Vector3f offset(double angle) const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_HUECIRCLE_H
