/** @file render/wallspec.h Wall Geometry Specification.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_RENDER_WALLSPEC
#define DE_RENDER_WALLSPEC

#include "world/line.h"

/**
 * Wall geometry specification. The members are public for convenient access.
 */
class WallSpec
{
public:
    enum Flag
    {
        /// Force the geometry to be opaque, irrespective of material opacity.
        ForceOpaque           = 0x001,

        /// Fade out the geometry the closer it is to the viewer.
        NearFade              = 0x002,

        /**
         * Clip the geometry if the neighbor plane surface relevant for the
         * specified section (i.e., the floor if @c Side::Bottom or ceiling if
         * @c Side::Top) has a sky-masked material bound to it.
         */
        SkyClip               = 0x004,

        /// Sort the dynamic light projections by descending luminosity.
        SortDynLights         = 0x008,

        /// Do not generate geometry for dynamic lights.
        NoDynLights           = 0x010,

        /// Do not generate geometry for dynamic (mobj) shadows.
        NoDynShadows          = 0x020,

        /// Do not generate geometry for faked radiosity.
        NoFakeRadio           = 0x040,

        /// Do not apply angle based light level deltas.
        NoLightDeltas         = 0x080,

        /// Do not intercept edges with neighboring geometries.
        NoEdgeDivisions       = 0x100,

        /// Do not smooth edge normals.
        NoEdgeNormalSmoothing = 0x200,

        DefaultFlags = ForceOpaque | SkyClip
    };

    /// Specification flags.
    de::Flags flags;

    /// Wall section identifier.
    int section;

    /**
     * Construct a default wall geometry specification for the specifed @a section.
     */
    WallSpec(int section = 0, de::Flags flags = DefaultFlags)
        : flags(flags)
        , section(section)
    {}

    /**
     * Construct a wall geometry specification appropriate for the specified
     * @a side and @a section of a map Line considering the current map renderer
     * configuration.
     */
    static WallSpec fromMapSide(const LineSide &side, int section);
};

#endif // DE_RENDER_WALLSPEC
