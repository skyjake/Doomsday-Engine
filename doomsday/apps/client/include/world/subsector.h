/** @file subsector.h  Client-side world map subsector.
 * @ingroup world
 *
 * @authors Copyright © 2013-2016 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2016-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "world/audioenvironment.h"
#include "world/line.h"
#include "world/plane.h"
#include "render/ilightsource.h"
#include "world/convexsubspace.h"

#include <doomsday/world/subsector.h>
#include <doomsday/world/sector.h>
#include <de/bitarray.h>
#include <de/list.h>

class ClEdgeLoop;

class Subsector : public world::Subsector, public ILightSource
{
public:
    /**
     * Construct a new subsector comprised of the specified set of map subspace regions.
     * It is assumed that all the subspaces are attributed to the same Sector and there
     * is always at least one in the set.
     *
     * @param subspaces  Set of subspaces comprising the resulting subsector.
     */
    Subsector(const de::List<world::ConvexSubspace *> &subspaces);

    /**
     * Returns a human-friendly, textual description of the subsector.
     */
    de::String description() const override;

    /**
     * Returns @c true if @a height (up-axis offset) lies above/below the ceiling/floor
     * height of the subsector.
     */
    bool isHeightInVoid(double height) const;

    /**
     * Determines whether the subsector has positive world volume, i.e., the height of
     * the floor is lower than that of the ceiling plane.
     *
     * @param useSmoothedHeights  @c true= use the @em smoothed plane heights instead of
     * the @em sharp heights.
     */
    bool hasWorldVolume(bool useSmoothedHeights = true) const;

//- Edge loops --------------------------------------------------------------------------

    // Edge loop identifiers:
    enum
    {
        OuterLoop,
        InnerLoop
    };

    static de::String edgeLoopIdAsText(int loopId);

    /**
     * Returns the total number of EdgeLoops for the subsector.
     */
    int edgeLoopCount() const;

    /**
     * Iterate the EdgeLoops of the subsector.
     *
     * @param callback  Function to call for each edge loop.
     */
    de::LoopResult forAllEdgeLoops(const std::function<de::LoopResult (ClEdgeLoop       &)> &func);
    de::LoopResult forAllEdgeLoops(const std::function<de::LoopResult (const ClEdgeLoop &)> &func) const;

//- Audio environment -------------------------------------------------------------------

    /**
     * POD: Environmental audio parameters.
     */
    struct AudioEnvironment
    {
        float volume  = 0;
        float space   = 0;
        float decay   = 0;
        float damping = 0;

        void reset() { volume = space = decay = damping = 0; }
    };

    /**
     * Returns the environmental audio config for the subsector. Note that if a reverb
     * update is scheduled it will be done at this time (@ref markReverbDirty()).
     */
    const AudioEnvironment &reverb() const;

    /**
     * Request re-calculation of the environmental audio (reverb) characteristics of the
     * subsector (deferred until necessary).
     *
     * To be called whenever any of the properties governing reverb properties have changed
     * (i.e., wall/plane material changes).
     */
    void markReverbDirty(bool yes = true);

//- Decorations -------------------------------------------------------------------------

    /**
     * Returns @c true if the subsector has one or more decorations.
     */
    bool hasDecorations() const;

    /**
     * Perform scheduled decoration work.
     */
    void decorate();

    /**
     * Mark the surface as needing a decoration update.
     */
    void markForDecorationUpdate(bool yes = true);

    void generateLumobjs();

//- Light grid --------------------------------------------------------------------------

    /**
     * Returns the unique identifier of the light source.
     */
    LightId lightSourceId() const override;

    /**
     * Returns the final ambient light color for the source (which, may be affected by the
     * sky light color if one or more Plane Surfaces in the subsector are using a sky-masked
     * Material).
     */
    de::Vec3f lightSourceColorf() const override;

    /**
     * Returns the final ambient light intensity for the source.
     * @see lightSourceColorf()
     */
    float lightSourceIntensity(const de::Vec3d &viewPoint = de::Vec3d(0, 0, 0)) const override;

    /**
     * Returns the final ambient light color and intensity for the source.
     * @see lightSourceColorf()
     */
    inline de::Vec4f lightSourceColorfIntensity() const {
        return de::Vec4f(lightSourceColorf(), lightSourceIntensity());
    }

    /**
     * Returns the Z-axis bias scale factor for the light grid, block light source.
     */
    int blockLightSourceZBias();

//- Sky Planes --------------------------------------------------------------------------

    /**
     * Determines whether at least one of the referenced plane Surfaces has a sky-masked
     * Material currently bound (@ref Surface::hasSkyMaskedMaterial()).
     *
     * @param planeIndex  Index of the plane to examine, or @c -1 to check all planes.
     *
     * @see hasSkyFloor(), hasSkyCeiling()
     */
    bool hasSkyPlane(int planeIndex = -1) const;

    /**
     * Determines whether the Surface of the @em floor plane has a sky-masked Material
     * currently bound.
     *
     * @see hasSkyPlane(), hasSkyCeiling()
     */
    bool hasSkyFloor() const;

    /**
     * Determines whether the Surface of the @em ceiling plane has a sky-masked Material
     * currently bound.
     *
     * @see hasSkyPlane(), hasSkyFloor()
     */
    bool hasSkyCeiling() const;

//- Visual Planes (mapped) --------------------------------------------------------------

    enum VisPlaneLinkMode {
        LinkWhenLowerThanTarget     = 0x1,
        LinkWhenHigherThanTarget    = 0x2,
        LinkWhenDifferentThanTarget = 0x3,
        LinkAlways                  = 0x4,
    };

    void linkVisPlane(int planeIndex, Subsector &target, VisPlaneLinkMode linkMode);

    /**
     * Returns the total number of @em visual planes in the subsector.
     */
    int visPlaneCount() const;

    /**
     * Iterate the @em visual Planes of the subsector.
     *
     * @param callback  Function to call for each plane.
     */
    de::LoopResult forAllVisPlanes(const std::function<de::LoopResult (Plane &)>& func);
    de::LoopResult forAllVisPlanes(const std::function<de::LoopResult (const Plane &)>& func) const;

    /**
     * Returns the @em visual Plane of the subsector associated with @a planeIndex.
     *
     * @see visFloor(), visCeiling()
     */
    Plane       &visPlane(int planeIndex);
    const Plane &visPlane(int planeIndex) const;

    /**
     * Returns the @em visual floor Plane of the subsector.
     *
     * @see ceiling(), plane()
     */
    inline Plane       &visFloor()       { return visPlane(world::Sector::Floor); }
    inline const Plane &visFloor() const { return visPlane(world::Sector::Floor); }

    /**
     * Returns the @em visual ceiling Plane of the subsector.
     *
     * @see floor(), plane()
     */
    inline Plane       &visCeiling()       { return visPlane(world::Sector::Ceiling); }
    inline const Plane &visCeiling() const { return visPlane(world::Sector::Ceiling); }

private:
    DE_PRIVATE(d)
};
