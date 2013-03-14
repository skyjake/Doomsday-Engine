/** @file sector.h Map Sector.
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

#ifndef LIBDENG_MAP_SECTOR
#define LIBDENG_MAP_SECTOR

#include <QList>
#include <de/aabox.h>
#include <de/Error>
#include "map/plane.h"
#include "p_mapdata.h"
#include "p_dmu.h"
#include "MapElement"

class BspLeaf;
class LineDef;

// Sector frame flags
#define SIF_VISIBLE         0x1     // Sector is visible on this frame.
#define SIF_FRAME_CLEAR     0x1     // Flags to clear before each frame.
#define SIF_LIGHT_CHANGED   0x2

typedef struct msector_s {
    /// Sector index. Always valid after loading & pruning.
    int index;
    int	refCount;
} msector_t;

/**
 * Map sector.
 *
 * @ingroup map
 */
class Sector : public de::MapElement
{
public:
    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

public: /// @todo Make private:
    typedef QList<Plane *> Planes;

    int frameFlags;

    /// if == validCount, already checked.
    int validCount;

    /// Bounding box for the sector.
    AABoxd aaBox;

    /// Rough approximation of sector area.
    coord_t roughArea;

    float lightLevel;

    float oldLightLevel;

    float rgb[3];

    float oldRGB[3];

    /// List of mobjs in the sector.
    struct mobj_s *mobjList;

    /// [lineDefCount+1] size.
    LineDef **lineDefs;
    uint lineDefCount;

    /// [bspLeafCount+1] size.
    BspLeaf **bspLeafs;
    uint bspLeafCount;

    /// [numReverbBspLeafAttributors] size.
    BspLeaf **reverbBspLeafs;
    uint numReverbBspLeafAttributors;

    ddmobj_base_t base;

    /// List of planes in the sector.
    Planes _planes;

    /// Number of gridblocks in the sector.
    uint blockCount;

    /// Number of blocks to mark changed.
    uint changedBlockCount;

    /// Light grid block indices.
    ushort *blocks;

    float reverb[NUM_REVERB_DATA];

    msector_t buildData;

public:
    Sector();    
    ~Sector();

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
     * Provides access to the list of planes for efficient traversal.
     */
    Planes const &planes() const;

    /**
     * Returns the total number of planes in the sector.
     */
    inline uint planeCount() const { return uint(planes().count()); }

    /**
     * Update the sector's map space axis-aligned bounding box to encompass
     * the points defined by it's LineDefs' vertexes.
     *
     * @pre LineDef list must have been initialized.
     */
    void updateAABox();

    /**
     * Update the sector's rough area approximation.
     *
     * @pre Axis-aligned bounding box must have been initialized.
     */
    void updateArea();

    /**
     * Update the origin of the sector according to the point defined by the
     * center of the sector's axis-aligned bounding box (which must be
     * initialized before calling).
     */
    void updateBaseOrigin();

    /**
     * Get a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int property(setargs_t &args) const;

    /**
     * Update a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int setProperty(setargs_t const &args);
};

#endif // LIBDENG_MAP_SECTOR
