/** @file sector.h  Map sector.
 * @ingroup world
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_WORLD_SECTOR_H
#define DENG_WORLD_SECTOR_H

#include <functional>
#ifdef __CLIENT__
#  include <de/aabox.h>
#endif
#include <de/Error>
#include <de/Observers>
#include <de/Vector>

#include "MapElement"
#include "Line"
#include "Plane"

struct mobj_s;

/**
 * World map sector.
 *
 * Whenever the @em Floor or @em Ceiling Plane moves any SoundEmitter origins which are
 * dependent on the height of said plane are updated automatically. Also missing surface
 * materials are re-evaluated to fill any new gaps.
 */
class Sector : public world::MapElement
{
    DENG2_NO_COPY  (Sector)
    DENG2_NO_ASSIGN(Sector)

public:
    /// Required/referenced plane is missing. @ingroup errors
    DENG2_ERROR(MissingPlaneError);

    /// Notified whenever a light level change occurs.
    DENG2_DEFINE_AUDIENCE2(LightLevelChange, void sectorLightLevelChanged(Sector &sector))

    /// Notified whenever a light color change occurs.
    DENG2_DEFINE_AUDIENCE2(LightColorChange, void sectorLightColorChanged(Sector &sector))

    // Plane identifiers:
    enum { Floor, Ceiling };

public:
    /**
     * Construct a new sector.
     *
     * @param lightLevel  Ambient light level.
     * @param lightColor  Ambient light color.
     */
    Sector(de::dfloat lightLevel          = 1,
           de::Vector3f const &lightColor = de::Vector3f(1, 1, 1));

    /**
     * Returns the axis-aligned bounding box which encompases the geometry of all BSP leafs
     * attributed to the sector (map units squared). Note that if no BSP leafs reference
     * the sector the bounding box will be invalid (has negative dimensions).
     */
    AABoxd const &aaBox() const;

    /**
     * Returns the ambient light level in the sector. The LightLevelChange audience is
     * notified whenever the light level changes.
     *
     * @see setLightLevel()
     */
    de::dfloat lightLevel() const;

    /**
     * Change the ambient light level in the sector. The LightLevelChange audience is
     * notified whenever the light level changes.
     *
     * @param newLightLevel  New ambient light level.
     *
     * @see lightLevel()
     */
    void setLightLevel(de::dfloat newLightLevel);

    /**
     * Returns the ambient light color in the sector. The LightColorChange audience is
     * notified whenever the light color changes.
     *
     * @see setLightColor()
     */
    de::Vector3f const &lightColor() const;

    /**
     * Change the ambient light color in the sector. The LightColorChange audience is
     * notified whenever the light color changes.
     *
     * @param newLightColor  New ambient light color.
     *
     * @see lightColor()
     */
    void setLightColor(de::Vector3f const &newLightColor);

public:  //- Planes ---------------------------------------------------------------------

    /**
     * Returns @c true if at least one Plane in the sector is sky-masked.
     *
     * @see Surface::hasSkyMaskedMaterial()
     */
    bool hasSkyMaskPlane() const;

    /**
     * Returns the total number of planes in/owned by the sector.
     */
    de::dint planeCount() const;

    /**
     * Lookup a Plane by it's sector-unique @a planeIndex.
     */
    Plane       &plane(de::dint planeIndex);
    Plane const &plane(de::dint planeIndex) const;

    /**
     * Returns the @em floor Plane of the sector.
     */
    inline Plane       &floor()       { return plane(Floor); }
    inline Plane const &floor() const { return plane(Floor); }

    /**
     * Returns the @em ceiling Plane of the sector.
     */
    inline Plane       &ceiling()       { return plane(Ceiling); }
    inline Plane const &ceiling() const { return plane(Ceiling); }

    /**
     * Iterate Planes of the sector.
     *
     * @param callback  Function to call for each Plane.
     */
    de::LoopResult forAllPlanes(std::function<de::LoopResult (Plane &)> func) const;

    /**
     * Add another Plane to the sector.
     *
     * @param normal  Map space Surface normal.
     * @param height  Map space Z axis coordinate (the "height" of the plane).
     *
     * @return  The newly constructed Plane.
     */
    Plane *addPlane(de::Vector3f const &normal, de::ddouble height);

public:  //- Sides ----------------------------------------------------------------------

    /**
     * Returns the total number of Line::Sides which reference the sector.
     */
    de::dint sideCount() const;

    /**
     * Iterate Line::Sides of the sector.
     *
     * @param callback  Function to call for each Line::Side.
     */
    de::LoopResult forAllSides(std::function<de::LoopResult (Line::Side &)> func) const;

    /**
     * (Re)Build the side list for the sector.
     *
     * @note In the special case of self-referencing line, only the front side reference
     * is added to this list.
     *
     * @attention The behavior of some algorithms used in the DOOM game logic is dependant
     * upon the order of this list. For example, EV_DoFloor and EV_BuildStairs. That same
     * order is used here, for compatibility.
     *
     * Order: Original @em line index, ascending.
     */
    void buildSides();

public:

    /**
     * Unlink the mobj from the list of mobjs "in" the sector.
     *
     * @param mob  Mobj to be unlinked.
     */
    void unlink(struct mobj_s *mob);

    /**
     * Link the mobj to the head of the list of mobjs "in" the sector. Note that mobjs in
     * this list may not actually be inside the sector. This is because the sector is
     * determined by interpreting the BSP leaf as a half-space and not a closed convex
     * subspace (@ref world::Map::link()).
     *
     * @param mob  Mobj to be linked.
     */
    void link(struct mobj_s *mob);

    /**
     * Returns the first mobj in the linked list of mobjs "in" the sector.
     */
    struct mobj_s *firstMobj() const;

    /**
     * Returns the primary sound emitter for the sector. Other emitters in the sector are
     * linked to this, forming a chain which can be traversed using the 'next' pointer of
     * the emitter's thinker_t.
     */
    SoundEmitter       &soundEmitter();
    SoundEmitter const &soundEmitter() const;

    /**
     * (Re)Build the sound emitter chains for the sector. These chains are used for
     * efficiently traversing all sound emitters in the sector (e.g., when stopping all
     * sounds emitted in the sector). To be called during map load once planes and sides
     * have been initialized.
     *
     * @see addPlane(), buildSides()
     */
    void chainSoundEmitters();

    /**
     * Returns the @em validCount of the sector. Used by some legacy iteration algorithms
     * for marking sectors as processed/visited.
     *
     * @todo Refactor away.
     */
    de::dint validCount() const;

    /// @todo Refactor away.
    void setValidCount(de::dint newValidCount);

    /**
     * Register the console commands and/or variables of this module.
     */
    static void consoleRegister();

protected:
    de::dint property(de::DmuArgs &args) const;
    de::dint setProperty(de::DmuArgs const &args);

private:
    DENG2_PRIVATE(d)
};

#endif  // DENG_WORLD_SECTOR_H
