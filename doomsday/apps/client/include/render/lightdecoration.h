/** @file lightdecoration.h  World surface light decoration.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_CLIENT_RENDER_LIGHTDECORATION_H
#define DE_CLIENT_RENDER_LIGHTDECORATION_H

#include "decoration.h"
#include "lumobj.h"

/**
 * World surface light decoration.
 * @ingroup render
 */
class LightDecoration : public Decoration, public Lumobj::Source
{
public:
    /**
     * Construct a new light decoration.
     *
     * @param source  Source of the decoration (a material).
     * @param origin  Origin of the decoration in map space.
     */
    LightDecoration(const MaterialAnimator::Decoration &source,
                    const de::Vec3d &origin = de::Vec3d());

    de::String description() const;

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

    /**
     * Returns the current angle fade factor (user configurable).
     */
    static float angleFadeFactor();

    /**
     * Returns the current brightness scale factor (user configurable).
     */
    static float brightFactor();

    /**
     * Calculates an occlusion factor for the light source. Determined by the
     * relative position of the @a eye (the viewer) and the decoration.
     *
     * @param eye  Position of the eye in map space.
     *
     * @return  Occlusion factor in the range [0..1], where @c 0 is fully
     * occluded and @c 1 is fully visible.
     */
    float occlusion(const de::Vec3d &eye) const;

    /**
     * Generates a new lumobj for the light decoration. A map surface must be
     * attributed to the decoration.
     *
     * @see Decoration::setSurface(), Decoration::hasSurface()
     */
    Lumobj *generateLumobj() const;
};

#endif  // DE_CLIENT_RENDER_LIGHTDECORATION_H
