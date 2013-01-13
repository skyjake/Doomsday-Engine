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

#include "MapObject"
#include "p_mapdata.h"
#include "p_dmu.h"

// Helper macros for accessing sector floor/ceiling plane data elements.
#define SP_plane(n)             planes[(n)]

#define SP_planesurface(n)      SP_plane(n)->surface
#define SP_planeheight(n)       SP_plane(n)->height
#define SP_planetangent(n)      SP_plane(n)->surface.tangent
#define SP_planebitangent(n)    SP_plane(n)->surface.bitangent
#define SP_planenormal(n)       SP_plane(n)->surface.normal
#define SP_planematerial(n)     SP_plane(n)->surface.material
#define SP_planeoffset(n)       SP_plane(n)->surface.offset
#define SP_planergb(n)          SP_plane(n)->surface.rgba
#define SP_planetarget(n)       SP_plane(n)->target
#define SP_planespeed(n)        SP_plane(n)->speed
#define SP_planeorigin(n)       SP_plane(n)->origin
#define SP_planevisheight(n)    SP_plane(n)->visHeight

#define SP_ceilsurface          SP_planesurface(PLN_CEILING)
#define SP_ceilheight           SP_planeheight(PLN_CEILING)
#define SP_ceiltangent          SP_planetangent(PLN_CEILING)
#define SP_ceilbitangent        SP_planebitangent(PLN_CEILING)
#define SP_ceilnormal           SP_planenormal(PLN_CEILING)
#define SP_ceilmaterial         SP_planematerial(PLN_CEILING)
#define SP_ceiloffset           SP_planeoffset(PLN_CEILING)
#define SP_ceilrgb              SP_planergb(PLN_CEILING)
#define SP_ceiltarget           SP_planetarget(PLN_CEILING)
#define SP_ceilspeed            SP_planespeed(PLN_CEILING)
#define SP_ceilorigin           SP_planeorigin(PLN_CEILING)
#define SP_ceilvisheight        SP_planevisheight(PLN_CEILING)

#define SP_floorsurface         SP_planesurface(PLN_FLOOR)
#define SP_floorheight          SP_planeheight(PLN_FLOOR)
#define SP_floortangent         SP_planetangent(PLN_FLOOR)
#define SP_floorbitangent       SP_planebitangent(PLN_FLOOR)
#define SP_floornormal          SP_planenormal(PLN_FLOOR)
#define SP_floormaterial        SP_planematerial(PLN_FLOOR)
#define SP_flooroffset          SP_planeoffset(PLN_FLOOR)
#define SP_floorrgb             SP_planergb(PLN_FLOOR)
#define SP_floortarget          SP_planetarget(PLN_FLOOR)
#define SP_floorspeed           SP_planespeed(PLN_FLOOR)
#define SP_floororigin          SP_planeorigin(PLN_FLOOR)
#define SP_floorvisheight       SP_planevisheight(PLN_FLOOR)

#define S_skyfix(n)             skyFix[(n)]
#define S_floorskyfix           S_skyfix(PLN_FLOOR)
#define S_ceilskyfix            S_skyfix(PLN_CEILING)

// Sector frame flags
#define SIF_VISIBLE         0x1     // Sector is visible on this frame.
#define SIF_FRAME_CLEAR     0x1     // Flags to clear before each frame.
#define SIF_LIGHT_CHANGED   0x2

typedef struct msector_s {
    // Sector index. Always valid after loading & pruning.
    int index;
    int	refCount;
} msector_t;

struct sector_s {
    runtime_mapdata_header_t header;
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
    struct linedef_s**  lineDefs;      // [lineDefCount+1] size.
    unsigned int        bspLeafCount;
    struct bspleaf_s**  bspLeafs;     // [bspLeafCount+1] size.
    unsigned int        numReverbBspLeafAttributors;
    struct bspleaf_s**  reverbBspLeafs;  // [numReverbBspLeafAttributors] size.
    ddmobj_base_t       base;
    unsigned int        planeCount;
    struct plane_s**    planes;        // [planeCount+1] size.
    unsigned int        blockCount;    // Number of gridblocks in the sector.
    unsigned int        changedBlockCount; // Number of blocks to mark changed.
    unsigned short*     blocks;        // Light grid block indices.
    float               reverb[NUM_REVERB_DATA];
    msector_t           buildData;
};

class Sector : public de::MapObject, public sector_s
{
public:
    Sector() : de::MapObject(DMU_SECTOR)
    {
        memset(static_cast<sector_s *>(this), 0, sizeof(sector_s));
    }
};

extern "C" {

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
void Sector_UpdateBaseOrigin(sector_s *sector);

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

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_MAP_SECTOR
