/** @file sector.h  World map sector.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_WORLD_SECTOR_H
#define DENG_WORLD_SECTOR_H

#include "dd_share.h" // AudioEnvironmentFactors

#include "HEdge"

#include "MapElement"
#include "Line"
#include "Plane"

#include <de/libdeng2.h>
#include <de/Error>
#include <de/Observers>
#include <de/Vector>
#include <de/aabox.h>
#include <QList>

class BspLeaf;
class Surface;
struct mobj_s;

/**
 * World map sector.
 *
 * @ingroup world
 */
class Sector : public de::MapElement
{
    DENG2_NO_COPY  (Sector)
    DENG2_NO_ASSIGN(Sector)

public:
    /// Required/referenced plane is missing. @ingroup errors
    DENG2_ERROR(MissingPlaneError);

    /// Notified whenever a light level change occurs.
    DENG2_DEFINE_AUDIENCE(LightLevelChange, void sectorLightLevelChanged(Sector &sector))

    /// Notified whenever a light color change occurs.
    DENG2_DEFINE_AUDIENCE(LightColorChange, void sectorLightColorChanged(Sector &sector))

    /*
     * Linked-element lists:
     */
    typedef QList<Plane *>    Planes;
    typedef QList<LineSide *> Sides;

    // Plane identifiers:
    enum { Floor, Ceiling };

public:
    /**
     * Construct a new sector.
     *
     * @param lightLevel  Ambient light level.
     * @param lightColor  Ambient light color.
     */
    Sector(float lightLevel               = 1,
           de::Vector3f const &lightColor = de::Vector3f(1, 1, 1));

    /**
     * Returns the sector plane with the specified @a planeIndex.
     */
    Plane &plane(int planeIndex);

    /// @copydoc plane()
    Plane const &plane(int planeIndex) const;

    /**
     * Returns the floor plane of the sector.
     */
    inline Plane &floor() { return plane(Floor); }

    /// @copydoc floor()
    inline Plane const &floor() const { return plane(Floor); }

    /**
     * Returns the ceiling plane of the sector.
     */
    inline Plane &ceiling() { return plane(Ceiling); }

    /// @copydoc ceiling()
    inline Plane const &ceiling() const { return plane(Ceiling); }

    Plane *addPlane(de::Vector3f const &normal, coord_t height);

    /**
     * Provides access to the list of planes in/owned by the sector, for efficient
     * traversal.
     */
    Planes const &planes() const;

    /**
     * Returns the total number of planes in/owned by the sector.
     */
    inline int planeCount() const { return planes().count(); }

    /**
     * Convenient accessor method for returning the surface of the specified
     * plane of the sector.
     */
    inline Surface &planeSurface(int planeIndex) { return plane(planeIndex).surface(); }

    /// @copydoc planeSurface()
    inline Surface const &planeSurface(int planeIndex) const { return plane(planeIndex).surface(); }

    /**
     * Convenient accessor method for returning the surface of the floor plane
     * of the sector.
     */
    inline Surface &floorSurface() { return floor().surface(); }

    /// @copydoc floorSurface()
    inline Surface const &floorSurface() const { return floor().surface(); }

    /**
     * Convenient accessor method for returning the surface of the ceiling plane
     * of the sector.
     */
    inline Surface &ceilingSurface() { return ceiling().surface(); }

    /// @copydoc ceilingSurface()
    inline Surface const &ceilingSurface() const { return ceiling().surface(); }

    /**
     * Provides access to the list of line sides which reference the sector,
     * for efficient traversal.
     */
    Sides const &sides() const;

    /**
     * Returns the total number of line sides which reference the sector.
     */
    inline int sideCount() const { return sides().count(); }

    /**
     * (Re)Build the side list for the sector.
     *
     * @note In the special case of self-referencing line, only the front side
     * reference is added to this list.
     *
     * @attention The behavior of some algorithms used in the DOOM game logic
     * is dependant upon the order of this list. For example, EV_DoFloor and
     * EV_BuildStairs. That same order is used here, for compatibility.
     *
     * Order: Original @em line index, ascending.
     */
    void buildSides();

    /**
     * Returns the primary sound emitter for the sector. Other emitters in the
     * sector are linked to this, forming a chain which can be traversed using
     * the 'next' pointer of the emitter's thinker_t.
     */
    SoundEmitter &soundEmitter();

    /// @copydoc soundEmitter()
    SoundEmitter const &soundEmitter() const;

    /**
     * (Re)Build the sound emitter chains for the sector. These chains are used
     * for efficiently traversing all sound emitters in the sector (e.g., when
     * stopping all sounds emitted in the sector). To be called during map load
     * once planes and sides have been initialized.
     *
     * @see addPlane(), buildSides()
     */
    void chainSoundEmitters();

    /**
     * Returns the ambient light level in the sector. The LightLevelChange
     * audience is notified whenever the light level changes.
     *
     * @see setLightLevel()
     */
    float lightLevel() const;

    /**
     * Change the ambient light level in the sector. The LightLevelChange
     * audience is notified whenever the light level changes.
     *
     * @param newLightLevel  New ambient light level.
     *
     * @see lightLevel()
     */
    void setLightLevel(float newLightLevel);

    /**
     * Returns the ambient light color in the sector. The LightColorChange
     * audience is notified whenever the light color changes.
     *
     * @see setLightColor()
     */
    de::Vector3f const &lightColor() const;

    /**
     * Change the ambient light color in the sector. The LightColorChange
     * audience is notified whenever the light color changes.
     *
     * @param newLightColor  New ambient light color.
     *
     * @see lightColor()
     */
    void setLightColor(de::Vector3f const &newLightColor);

    /**
     * Returns the first mobj in the linked list of mobjs "in" the sector.
     */
    struct mobj_s *firstMobj() const;

    /**
     * Unlink the mobj from the list of mobjs "in" the sector.
     *
     * @param mobj  Mobj to be unlinked.
     */
    void unlink(struct mobj_s *mobj);

    /**
     * Link the mobj to the head of the list of mobjs "in" the sector. Note that
     * mobjs in this list may not actually be inside the sector. This is because
     * the sector is determined by interpreting the BSP leaf as a half-space and
     * not a closed convex subspace (@ref Map::link()).
     *
     * @param mobj  Mobj to be linked.
     */
    void link(struct mobj_s *mobj);

    /**
     * Returns the @em validCount of the sector. Used by some legacy iteration
     * algorithms for marking sectors as processed/visited.
     *
     * @todo Refactor away.
     */
    int validCount() const;

    /// @todo Refactor away.
    void setValidCount(int newValidCount);

#ifdef __CLIENT__

    /**
     * Returns the axis-aligned bounding box which encompases the geometry of
     * all BSP leafs attributed to the sector (map units squared). Note that if
     * no BSP leafs reference the sector the bounding box will be invalid (has
     * negative dimensions).
     *
     * @todo Refactor away (still used by light decoration and particle systems).
     */
    AABoxd const &aaBox() const;

    /**
     * Returns a rough approximation of the total combined area of the geometry
     * for all BSP leafs attributed to the sector (map units squared).
     *
     * @todo Refactor away (still used by the particle system).
     */
    coord_t roughArea() const;

#endif // __CLIENT__

protected:
    int property(DmuArgs &args) const;
    int setProperty(DmuArgs const &args);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_SECTOR_H
