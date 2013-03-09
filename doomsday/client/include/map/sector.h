/**
 * @file sector.h
 * Map Sector. @ingroup map
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef __cplusplus
#  error "map/sector.h requires C++"
#endif

#include "MapElement"
#include "map/plane.h"
#include "p_mapdata.h"
#include "p_dmu.h"
#include <QList>

// Helper macros for accessing sector floor/ceiling plane data elements.
#define SP_plane(n)             planes[(n)]

#define SP_planesurface(n)      SP_plane(n)->surface()
#define SP_planeheight(n)       SP_plane(n)->height()
#define SP_planetangent(n)      SP_plane(n)->surface().tangent
#define SP_planebitangent(n)    SP_plane(n)->surface().bitangent
#define SP_planenormal(n)       SP_plane(n)->surface().normal
#define SP_planematerial(n)     SP_plane(n)->surface().material
#define SP_planeoffset(n)       SP_plane(n)->surface().offset
#define SP_planergb(n)          SP_plane(n)->surface().rgba
#define SP_planetarget(n)       SP_plane(n)->targetHeight()
#define SP_planespeed(n)        SP_plane(n)->speed()
#define SP_planevisheight(n)    SP_plane(n)->visHeight()

#define SP_ceilsurface          SP_planesurface(Plane::Ceiling)
#define SP_ceilheight           SP_planeheight(Plane::Ceiling)
#define SP_ceiltangent          SP_planetangent(Plane::Ceiling)
#define SP_ceilbitangent        SP_planebitangent(Plane::Ceiling)
#define SP_ceilnormal           SP_planenormal(Plane::Ceiling)
#define SP_ceilmaterial         SP_planematerial(Plane::Ceiling)
#define SP_ceiloffset           SP_planeoffset(Plane::Ceiling)
#define SP_ceilrgb              SP_planergb(Plane::Ceiling)
#define SP_ceiltarget           SP_planetarget(Plane::Ceiling)
#define SP_ceilspeed            SP_planespeed(Plane::Ceiling)
#define SP_ceilvisheight        SP_planevisheight(Plane::Ceiling)

#define SP_floorsurface         SP_planesurface(Plane::Floor)
#define SP_floorheight          SP_planeheight(Plane::Floor)
#define SP_floortangent         SP_planetangent(Plane::Floor)
#define SP_floorbitangent       SP_planebitangent(Plane::Floor)
#define SP_floornormal          SP_planenormal(Plane::Floor)
#define SP_floormaterial        SP_planematerial(Plane::Floor)
#define SP_flooroffset          SP_planeoffset(Plane::Floor)
#define SP_floorrgb             SP_planergb(Plane::Floor)
#define SP_floortarget          SP_planetarget(Plane::Floor)
#define SP_floorspeed           SP_planespeed(Plane::Floor)
#define SP_floorvisheight       SP_planevisheight(Plane::Floor)

#define S_skyfix(n)             skyFix[(n)]
#define S_floorskyfix           S_skyfix(Plane::Floor)
#define S_ceilskyfix            S_skyfix(Plane::Ceiling)

// Sector frame flags
#define SIF_VISIBLE         0x1     // Sector is visible on this frame.
#define SIF_FRAME_CLEAR     0x1     // Flags to clear before each frame.
#define SIF_LIGHT_CHANGED   0x2

typedef struct msector_s {
    // Sector index. Always valid after loading & pruning.
    int index;
    int	refCount;
} msector_t;

class Plane;

class Sector : public de::MapElement
{
public:
    typedef QList<Plane *> Planes;

    int                 frameFlags;
    int                 validCount;    // if == validCount, already checked.
    AABoxd              aaBox;         // Bounding box for the sector.
    coord_t             roughArea;    // Rough approximation of sector area.
    float               lightLevel;
    float               oldLightLevel;
    float               rgb[3];
    float               oldRGB[3];
    struct mobj_s*      mobjList;      // List of mobjs in the sector.
    unsigned int        lineDefCount;
    LineDef**  lineDefs;      // [lineDefCount+1] size.
    unsigned int        bspLeafCount;
    BspLeaf**  bspLeafs;     // [bspLeafCount+1] size.
    unsigned int        numReverbBspLeafAttributors;
    BspLeaf**  reverbBspLeafs;  // [numReverbBspLeafAttributors] size.
    ddmobj_base_t       base;
    Planes              planes;
    unsigned int        blockCount;    // Number of gridblocks in the sector.
    unsigned int        changedBlockCount; // Number of blocks to mark changed.
    unsigned short*     blocks;        // Light grid block indices.
    float               reverb[NUM_REVERB_DATA];
    msector_t           buildData;

public:
    Sector();    
    ~Sector();

    /**
     * Returns the total number of planes in the sector.
     */
    inline uint planeCount() const { return planes.size(); }

    /**
     * Returns the floor plane of the sector.
     */
    inline Plane &floor() { return *planes[Plane::Floor]; }

    /// @copydoc floor()
    inline Plane const &floor() const { return *planes[Plane::Floor]; }

    /**
     * Returns the ceiling plane of the sector.
     */
    inline Plane &ceiling() { return *planes[Plane::Ceiling]; }

    /// @copydoc ceiling()
    inline Plane const &ceiling() const { return *planes[Plane::Ceiling]; }
};

/**
 * Update the Sector's map space axis-aligned bounding box to encompass the points
 * defined by it's LineDefs' vertices.
 *
 * @pre LineDef list must have been initialized.
 *
 * @param sector  Sector instance.
 */
void Sector_UpdateAABox(Sector* sector);

/**
 * Update the Sector's rough area approximation.
 *
 * @pre Axis-aligned bounding box must have been initialized.
 *
 * @param sector  Sector instance.
 */
void Sector_UpdateArea(Sector* sector);

/**
 * Update the origin of the sector according to the point defined by the center of
 * the sector's axis-aligned bounding box (which must be initialized before calling).
 *
 * @param sector  Sector instance.
 */
void Sector_UpdateBaseOrigin(Sector *sector);

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param sector  Sector instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Sector_GetProperty(const Sector* sector, setargs_t* args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param sector  Sector instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Sector_SetProperty(Sector* sector, const setargs_t* args);

#endif // LIBDENG_MAP_SECTOR
