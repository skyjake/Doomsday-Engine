/** @file world/sector.h World Sector.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_WORLD_SECTOR_H
#define DENG_WORLD_SECTOR_H

#include <QList>

#include <de/aabox.h>

#include <de/Error>
#include <de/Observers>
#include <de/Vector>

#include "MapElement"
#include "Line"
#include "Plane"

#ifdef __CLIENT__
#  include "render/lightgrid.h"
#endif

class BspLeaf;
class Surface;

namespace de {
class Map;
}

/**
 * @defgroup sectorFrameFlags Sector frame flags
 * @ingroup world
 */
///@{
#define SIF_VISIBLE             0x1 ///< Sector is visible on this frame.

// Flags to clear before each frame.
#define SIF_FRAME_CLEAR         SIF_VISIBLE
///@}

/**
 * World sector.
 *
 * @ingroup world
 */
class Sector : public de::MapElement,
               DENG2_OBSERVES(Plane, HeightChange)
{
public:
    /// Required/referenced plane is missing. @ingroup errors
    DENG2_ERROR(MissingPlaneError);

    DENG2_DEFINE_AUDIENCE(LightLevelChange,
        void sectorLightLevelChanged(Sector &sector, float oldLightLevel))

    DENG2_DEFINE_AUDIENCE(LightColorChange,
        void sectorLightColorChanged(Sector &sector, de::Vector3f const &oldLightColor,
                                     int changedComponents /*bit-field (0x1=Red, 0x2=Green, 0x4=Blue)*/))

    static float const DEFAULT_LIGHT_LEVEL; ///< 1.f
    static de::Vector3f const DEFAULT_LIGHT_COLOR; ///< red=1.f green=1.f, blue=1.f

    typedef QList<Line::Side *> Sides;
    typedef QList<Plane *> Planes;
    typedef QList<BspLeaf *> BspLeafs;

#ifdef __CLIENT__
    /**
     * LightGrid data values for "smoothed sector lighting".
     *
     * @todo Encapsulate in LightGrid itself?
     */
    struct LightGridData
    {
        /// Number of blocks attributed to the sector.
        uint blockCount;

        /// Number of attributed blocks to mark changed.
        uint changedBlockCount;

        /// Block indices.
        de::LightGrid::Index *blocks;
    };
#endif

public: /// @todo Make private:
    /// @ref sectorFrameFlags
    int _frameFlags;

    /// Head of the linked list of mobjs "in" the sector (not owned).
    struct mobj_s *_mobjList;

    /// List of BSP leafs which contribute to the environmental audio
    /// characteristics of the sector (not owned).
    BspLeafs _reverbBspLeafs;

    /// Final environmental audio characteristics.
    AudioEnvironmentFactors _reverb;

public:
    Sector(float lightLevel               = DEFAULT_LIGHT_LEVEL,
           de::Vector3f const &lightColor = DEFAULT_LIGHT_COLOR);

    /**
     * Returns the sector plane with the specified @a planeIndex.
     */
    Plane &plane(int planeIndex);

    /// @copydoc plane()
    Plane const &plane(int planeIndex) const;

    /**
     * Returns the floor plane of the sector.
     */
    inline Plane &floor() { return plane(Plane::Floor); }

    /// @copydoc floor()
    inline Plane const &floor() const { return plane(Plane::Floor); }

    /**
     * Returns the ceiling plane of the sector.
     */
    inline Plane &ceiling() { return plane(Plane::Ceiling); }

    /// @copydoc ceiling()
    inline Plane const &ceiling() const { return plane(Plane::Ceiling); }

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
     * Returns @c true iff at least one of the surfaces of a plane in/owned
     * by the sector presently has a sky-masked material bound.
     *
     * @see Surface::hasSkyMaskedMaterial()
     */
    bool hasSkyMaskedPlane() const;

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
    inline uint sideCount() const { return uint(sides().count()); }

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
     *
     * @param map  Map to collate sides from. @todo Refactor away.
     */
    void buildSides(de::Map const &map);

    /**
     * Provides access to the list of BSP leafs which reference the sector, for
     * efficient traversal.
     */
    BspLeafs const &bspLeafs() const;

    /**
     * Returns the total number of BSP leafs which reference the sector.
     */
    inline uint bspLeafCount() const { return uint(bspLeafs().count()); }

    /**
     * (Re)Build the BSP leaf list for the sector.
     *
     * @param map  Map to collate BSP leafs from. @todo Refactor away.
     */
    void buildBspLeafs(de::Map const &map);

    /**
     * Provides access to the list of BSP leafs which contribute to the environmental
     * audio characteristics of the sector, for efficient traversal.
     */
    BspLeafs const &reverbBspLeafs() const;

    /**
     * Returns the total number of BSP leafs which contribute to the environmental
     * audio characteristics of the sector.
     */
    inline uint reverbBspLeafCount() const { return uint(reverbBspLeafs().count()); }

    /**
     * Returns the axis-aligned bounding box which encompases the geometry of
     * all BSP leafs attributed to the sector (map units squared). Note that if
     * no BSP leafs reference the sector the bounding box will be invalid (has
     * negative dimensions).
     */
    AABoxd const &aaBox() const;

    /**
     * Update the sector's map space axis-aligned bounding box to encompass
     * the geometry of all BSP leafs attributed to the sector.
     *
     * @pre BSP leaf list must have be initialized.
     *
     * @see buildBspLeafs()
     */
    void updateAABox();

    /**
     * Returns a rough approximation of the total combined area of the geometry
     * for all BSP leafs attributed to the sector (map units squared).
     *
     * @see updateRoughArea()
     */
    coord_t roughArea() const;

    /**
     * Update the sector's rough area approximation.
     *
     * @pre BSP leaf list must have be initialized.
     *
     * @see buildBspLeafs(), roughArea()
     */
    void updateRoughArea();

    /**
     * Returns the primary sound emitter for the sector. Other emitters in the
     * sector are linked to this, forming a chain which can be traversed using
     * the 'next' pointer of the emitter's thinker_t.
     */
    ddmobj_base_t &soundEmitter();

    /// @copydoc soundEmitter()
    ddmobj_base_t const &soundEmitter() const;

    /**
     * Update the sound emitter origin of the sector according to the point
     * defined by the center of the sector's axis-aligned bounding box (which
     * must be initialized before calling) and the mid point on the map up
     * axis between floor and ceiling planes.
     */
    void updateSoundEmitterOrigin();

    /**
     * @param newEmitter  Mobj base to link to the sector. Caller must ensure
     *                    that the object is not linked multiple times into
     *                    the chain.
     */
    void linkSoundEmitter(ddmobj_base_t &newEmitter);

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
     * @see setLightColor(), lightColorComponent(), lightRed(), lightGreen(), lightBlue()
     */
    de::Vector3f const &lightColor() const;

    /**
     * Returns the strength of the specified @a component of the ambient light
     * color in the sector. The LightColorChange audience is notified whenever
     * the light color changes.
     *
     * @param component    RGB index of the color component (0=Red, 1=Green, 2=Blue).
     *
     * @see lightColor(), lightRed(), lightGreen(), lightBlue()
     */
    inline float lightColorComponent(int component) const { return lightColor()[component]; }

    /**
     * Returns the strength of the @em red component of the ambient light
     * color in the sector. The LightColorChange audience is notified whenever
     * the light color changes.
     *
     * @see lightColorComponent(), lightGreen(), lightBlue()
     */
    inline float lightRed() const   { return lightColorComponent(0); }

    /**
     * Returns the strength of the @em green component of the ambient light
     * color in the sector. The LightColorChange audience is notified whenever
     * the light color changes.
     *
     * @see lightColorComponent(), lightRed(), lightBlue()
     */
    inline float lightGreen() const { return lightColorComponent(1); }

    /**
     * Returns the strength of the @em blue component of the ambient light
     * color in the sector. The LightColorChange audience is notified whenever
     * the light color changes.
     *
     * @see lightColorComponent(), lightRed(), lightGreen()
     */
    inline float lightBlue() const  { return lightColorComponent(2); }

    /**
     * Change the ambient light color in the sector. The LightColorChange
     * audience is notified whenever the light color changes.
     *
     * @param newLightColor  New ambient light color.
     *
     * @see lightColor(), setLightColorComponent(), setLightRed(), setLightGreen(), setLightBlue()
     */
    void setLightColor(de::Vector3f const &newLightColor);

    /// @copydoc setLightColor()
    inline void setLightColor(float red, float green, float blue) {
        setLightColor(de::Vector3f(red, green, blue));
    }

    /**
     * Change the strength of the specified @a component of the ambient light
     * color in the sector. The LightColorChange audience is notified whenever
     * the light color changes.
     *
     * @param component    RGB index of the color component (0=Red, 1=Green, 2=Blue).
     * @param newStrength  New strength factor for the color component.
     *
     * @see setLightColor(), setLightRed(), setLightGreen(), setLightBlue()
     */
    void setLightColorComponent(int component, float newStrength);

    /**
     * Change the strength of the red component of the ambient light color in
     * the sector. The LightColorChange audience is notified whenever the light
     * color changes.
     *
     * @param newStrength  New red strength for the ambient light color.
     *
     * @see setLightColorComponent(), setLightGreen(), setLightBlue()
     */
    inline void setLightRed(float newStrength)  { setLightColorComponent(0, newStrength); }

    /**
     * Change the strength of the green component of the ambient light color in
     * the sector. The LightColorChange audience is notified whenever the light
     * color changes.
     *
     * @param newStrength  New green strength for the ambient light color.
     *
     * @see setLightColorComponent(), setLightRed(), setLightBlue()
     */
    inline void setLightGreen(float newStrength) { setLightColorComponent(1, newStrength); }

    /**
     * Change the strength of the blue component of the ambient light color in
     * the sector. The LightColorChange audience is notified whenever the light
     * color changes.
     *
     * @param newStrength  New blue strength for the ambient light color.
     *
     * @see setLightColorComponent(), setLightRed(), setLightGreen()
     */
    inline void setLightBlue(float newStrength)  { setLightColorComponent(2, newStrength); }

    /**
     * Returns the first mobj in the linked list of mobjs "in" the sector.
     */
    struct mobj_s *firstMobj() const;

    /**
     * Returns the final environmental audio characteristics of the sector.
     */
    AudioEnvironmentFactors const &audioEnvironmentFactors() const;

#ifdef __CLIENT__
    /**
     * Returns the LightGrid data values (for smoothed ambient lighting) for
     * the sector.
     */
    LightGridData &lightGridData();
#endif

    /**
     * Returns the @ref sectorFrameFlags for the sector.
     */
    int frameFlags() const;

    /**
     * Returns the @em validCount of the sector. Used by some legacy iteration
     * algorithms for marking sectors as processed/visited.
     *
     * @todo Refactor away.
     */
    int validCount() const;

    /// @todo Refactor away.
    void setValidCount(int newValidCount);

protected:
    int property(setargs_t &args) const;
    int setProperty(setargs_t const &args);

    // Observes Plane HeightChange.
    void planeHeightChanged(Plane &plane, coord_t oldHeight);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_SECTOR_H
