/**\file dmu_lib.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Helper routines for accessing the DMU API
 */

#ifndef LIBCOMMON_DMU_LIB_H
#define LIBCOMMON_DMU_LIB_H

#include "doomsday.h"
#include "p_iterlist.h"

// DMU property aliases. For short-hand purposes:
#define DMU_TOP_MATERIAL        (DMU_TOP_OF_SIDEDEF | DMU_MATERIAL)
#define DMU_TOP_MATERIAL_OFFSET_X (DMU_TOP_OF_SIDEDEF | DMU_OFFSET_X)
#define DMU_TOP_MATERIAL_OFFSET_Y (DMU_TOP_OF_SIDEDEF | DMU_OFFSET_Y)
#define DMU_TOP_MATERIAL_OFFSET_XY (DMU_TOP_OF_SIDEDEF | DMU_OFFSET_XY)
#define DMU_TOP_FLAGS           (DMU_TOP_OF_SIDEDEF | DMU_FLAGS)
#define DMU_TOP_COLOR           (DMU_TOP_OF_SIDEDEF | DMU_COLOR)
#define DMU_TOP_COLOR_RED       (DMU_TOP_OF_SIDEDEF | DMU_COLOR_RED)
#define DMU_TOP_COLOR_GREEN     (DMU_TOP_OF_SIDEDEF | DMU_COLOR_GREEN)
#define DMU_TOP_COLOR_BLUE      (DMU_TOP_OF_SIDEDEF | DMU_COLOR_BLUE)

#define DMU_MIDDLE_MATERIAL     (DMU_MIDDLE_OF_SIDEDEF | DMU_MATERIAL)
#define DMU_MIDDLE_MATERIAL_OFFSET_X (DMU_MIDDLE_OF_SIDEDEF | DMU_OFFSET_X)
#define DMU_MIDDLE_MATERIAL_OFFSET_Y (DMU_MIDDLE_OF_SIDEDEF | DMU_OFFSET_Y)
#define DMU_MIDDLE_MATERIAL_OFFSET_XY (DMU_MIDDLE_OF_SIDEDEF | DMU_OFFSET_XY)
#define DMU_MIDDLE_FLAGS        (DMU_MIDDLE_OF_SIDEDEF | DMU_FLAGS)
#define DMU_MIDDLE_COLOR        (DMU_MIDDLE_OF_SIDEDEF | DMU_COLOR)
#define DMU_MIDDLE_COLOR_RED    (DMU_MIDDLE_OF_SIDEDEF | DMU_COLOR_RED)
#define DMU_MIDDLE_COLOR_GREEN  (DMU_MIDDLE_OF_SIDEDEF | DMU_COLOR_GREEN)
#define DMU_MIDDLE_COLOR_BLUE   (DMU_MIDDLE_OF_SIDEDEF | DMU_COLOR_BLUE)
#define DMU_MIDDLE_ALPHA        (DMU_MIDDLE_OF_SIDEDEF | DMU_ALPHA)
#define DMU_MIDDLE_BLENDMODE    (DMU_MIDDLE_OF_SIDEDEF | DMU_BLENDMODE)

#define DMU_BOTTOM_MATERIAL     (DMU_BOTTOM_OF_SIDEDEF | DMU_MATERIAL)
#define DMU_BOTTOM_MATERIAL_OFFSET_X (DMU_BOTTOM_OF_SIDEDEF | DMU_OFFSET_X)
#define DMU_BOTTOM_MATERIAL_OFFSET_Y (DMU_BOTTOM_OF_SIDEDEF | DMU_OFFSET_Y)
#define DMU_BOTTOM_MATERIAL_OFFSET_XY (DMU_BOTTOM_OF_SIDEDEF | DMU_OFFSET_XY)
#define DMU_BOTTOM_FLAGS        (DMU_BOTTOM_OF_SIDEDEF | DMU_FLAGS)
#define DMU_BOTTOM_COLOR        (DMU_BOTTOM_OF_SIDEDEF | DMU_COLOR)
#define DMU_BOTTOM_COLOR_RED    (DMU_BOTTOM_OF_SIDEDEF | DMU_COLOR_RED)
#define DMU_BOTTOM_COLOR_GREEN  (DMU_BOTTOM_OF_SIDEDEF | DMU_COLOR_GREEN)
#define DMU_BOTTOM_COLOR_BLUE   (DMU_BOTTOM_OF_SIDEDEF | DMU_COLOR_BLUE)

#define DMU_FLOOR_HEIGHT        (DMU_FLOOR_OF_SECTOR | DMU_HEIGHT)
#define DMU_FLOOR_TARGET_HEIGHT (DMU_FLOOR_OF_SECTOR | DMU_TARGET_HEIGHT)
#define DMU_FLOOR_SPEED         (DMU_FLOOR_OF_SECTOR | DMU_SPEED)
#define DMU_FLOOR_MATERIAL      (DMU_FLOOR_OF_SECTOR | DMU_MATERIAL)
#define DMU_FLOOR_SOUND_ORIGIN  (DMU_FLOOR_OF_SECTOR | DMU_SOUND_ORIGIN)
#define DMU_FLOOR_FLAGS         (DMU_FLOOR_OF_SECTOR | DMU_FLAGS)
#define DMU_FLOOR_COLOR         (DMU_FLOOR_OF_SECTOR | DMU_COLOR)
#define DMU_FLOOR_COLOR_RED     (DMU_FLOOR_OF_SECTOR | DMU_COLOR_RED)
#define DMU_FLOOR_COLOR_GREEN   (DMU_FLOOR_OF_SECTOR | DMU_COLOR_GREEN)
#define DMU_FLOOR_COLOR_BLUE    (DMU_FLOOR_OF_SECTOR | DMU_COLOR_BLUE)
#define DMU_FLOOR_MATERIAL_OFFSET_X (DMU_FLOOR_OF_SECTOR | DMU_OFFSET_X)
#define DMU_FLOOR_MATERIAL_OFFSET_Y (DMU_FLOOR_OF_SECTOR | DMU_OFFSET_Y)
#define DMU_FLOOR_MATERIAL_OFFSET_XY (DMU_FLOOR_OF_SECTOR | DMU_OFFSET_XY)
#define DMU_FLOOR_TANGENT_X     (DMU_FLOOR_OF_SECTOR | DMU_TANGENT_X)
#define DMU_FLOOR_TANGENT_Y     (DMU_FLOOR_OF_SECTOR | DMU_TANGENT_Y)
#define DMU_FLOOR_TANGENT_Z     (DMU_FLOOR_OF_SECTOR | DMU_TANGENT_Z)
#define DMU_FLOOR_TANGENT_XYZ   (DMU_FLOOR_OF_SECTOR | DMU_TANGENT_XYZ)
#define DMU_FLOOR_BITANGENT_X   (DMU_FLOOR_OF_SECTOR | DMU_BITANGENT_X)
#define DMU_FLOOR_BITANGENT_Y   (DMU_FLOOR_OF_SECTOR | DMU_BITANGENT_Y)
#define DMU_FLOOR_BITANGENT_Z   (DMU_FLOOR_OF_SECTOR | DMU_BITANGENT_Z)
#define DMU_FLOOR_BITANGENT_XYZ (DMU_FLOOR_OF_SECTOR | DMU_BITANGENT_XYZ)
#define DMU_FLOOR_NORMAL_X      (DMU_FLOOR_OF_SECTOR | DMU_NORMAL_X)
#define DMU_FLOOR_NORMAL_Y      (DMU_FLOOR_OF_SECTOR | DMU_NORMAL_Y)
#define DMU_FLOOR_NORMAL_Z      (DMU_FLOOR_OF_SECTOR | DMU_NORMAL_Z)
#define DMU_FLOOR_NORMAL_XYZ    (DMU_FLOOR_OF_SECTOR | DMU_NORMAL_XYZ)

#define DMU_CEILING_HEIGHT      (DMU_CEILING_OF_SECTOR | DMU_HEIGHT)
#define DMU_CEILING_TARGET_HEIGHT (DMU_CEILING_OF_SECTOR | DMU_TARGET_HEIGHT)
#define DMU_CEILING_SPEED       (DMU_CEILING_OF_SECTOR | DMU_SPEED)
#define DMU_CEILING_MATERIAL    (DMU_CEILING_OF_SECTOR | DMU_MATERIAL)
#define DMU_CEILING_SOUND_ORIGIN (DMU_CEILING_OF_SECTOR | DMU_SOUND_ORIGIN)
#define DMU_CEILING_FLAGS       (DMU_CEILING_OF_SECTOR | DMU_FLAGS)
#define DMU_CEILING_COLOR       (DMU_CEILING_OF_SECTOR | DMU_COLOR)
#define DMU_CEILING_COLOR_RED   (DMU_CEILING_OF_SECTOR | DMU_COLOR_RED)
#define DMU_CEILING_COLOR_GREEN (DMU_CEILING_OF_SECTOR | DMU_COLOR_GREEN)
#define DMU_CEILING_COLOR_BLUE  (DMU_CEILING_OF_SECTOR | DMU_COLOR_BLUE)
#define DMU_CEILING_MATERIAL_OFFSET_X (DMU_CEILING_OF_SECTOR | DMU_OFFSET_X)
#define DMU_CEILING_MATERIAL_OFFSET_Y (DMU_CEILING_OF_SECTOR | DMU_OFFSET_Y)
#define DMU_CEILING_MATERIAL_OFFSET_XY (DMU_CEILING_OF_SECTOR | DMU_OFFSET_XY)
#define DMU_CEILING_TANGENT_X   (DMU_CEILING_OF_SECTOR | DMU_TANGENT_X)
#define DMU_CEILING_TANGENT_Y   (DMU_CEILING_OF_SECTOR | DMU_TANGENT_Y)
#define DMU_CEILING_TANGENT_Z   (DMU_CEILING_OF_SECTOR | DMU_TANGENT_Z)
#define DMU_CEILING_TANGENT_XYZ (DMU_CEILING_OF_SECTOR | DMU_TANGENT_XYZ)
#define DMU_CEILING_BITANGENT_X (DMU_CEILING_OF_SECTOR | DMU_BITANGENT_X)
#define DMU_CEILING_BITANGENT_Y (DMU_CEILING_OF_SECTOR | DMU_BITANGENT_Y)
#define DMU_CEILING_BITANGENT_Z (DMU_CEILING_OF_SECTOR | DMU_BITANGENT_Z)
#define DMU_CEILING_BITANGENT_XYZ (DMU_CEILING_OF_SECTOR | DMU_BITANGENT_XYZ)
#define DMU_CEILING_NORMAL_X    (DMU_CEILING_OF_SECTOR | DMU_NORMAL_X)
#define DMU_CEILING_NORMAL_Y    (DMU_CEILING_OF_SECTOR | DMU_NORMAL_Y)
#define DMU_CEILING_NORMAL_Z    (DMU_CEILING_OF_SECTOR | DMU_NORMAL_Z)
#define DMU_CEILING_NORMAL_XYZ  (DMU_CEILING_OF_SECTOR | DMU_NORMAL_XYZ)

extern iterlist_t* linespecials; /// For surfaces that tick eg wall scrollers.

void P_DestroyLineTagLists(void);
iterlist_t* P_GetLineIterListForTag(int tag, boolean createNewList);

void P_DestroySectorTagLists(void);
iterlist_t* P_GetSectorIterListForTag(int tag, boolean createNewList);

linedef_t* P_AllocDummyLine(void);
void P_FreeDummyLine(linedef_t* line);

sidedef_t* P_AllocDummySideDef(void);
void P_FreeDummySideDef(sidedef_t* sideDef);

/**
 * Get the sector on the other side of the line that is NOT the given sector.
 *
 * @param line  Ptr to the line to test.
 * @param sec  Reference sector to compare against.
 *
 * @return  Ptr to the other sector or @c NULL if the specified line is NOT twosided.
 */
sector_t* P_GetNextSector(linedef_t* line, sector_t* sec);

#define FEPHF_MIN           0x1 // Get minium. If not set, get maximum.
#define FEPHF_FLOOR         0x2 // Get floors. If not set, get ceilings.

typedef struct findextremalplaneheightparams_s {
    sector_t* baseSec;
    byte flags;
    float val;
    sector_t* foundSec;
} findextremalplaneheightparams_t;

/// Find the sector with the lowest floor height in surrounding sectors.
sector_t* P_FindSectorSurroundingLowestFloor(sector_t* sector, float max, float* val);

/// Find the sector with the highest floor height in surrounding sectors.
sector_t* P_FindSectorSurroundingHighestFloor(sector_t* sector, float min, float* val);

/// Find lowest ceiling in the surrounding sector.
sector_t* P_FindSectorSurroundingLowestCeiling(sector_t* sector, float max, float* val);

/// Find highest ceiling in the surrounding sectors.
sector_t* P_FindSectorSurroundingHighestCeiling(sector_t* sector, float min, float* val);

#define FNPHF_FLOOR             0x1 // Get floors, if not set get ceilings.
#define FNPHF_ABOVE             0x2 // Get next above, if not set get next below.

typedef struct findnextplaneheightparams_s {
    sector_t* baseSec;
    float baseHeight;
    byte flags;
    float val;
    sector_t* foundSec;
} findnextplaneheightparams_t;

/// Find the sector with the next highest floor in surrounding sectors.
sector_t* P_FindSectorSurroundingNextHighestFloor(sector_t* sector, float baseHeight, float* val);

/// Find the sector with the next lowest floor in surrounding sectors.
sector_t* P_FindSectorSurroundingNextLowestFloor(sector_t* sector, float baseHeight, float* val);

/// Find the sector with the next highest ceiling in surrounding sectors.
sector_t* P_FindSectorSurroundingNextHighestCeiling(sector_t* sector, float baseHeight, float* val);

/// Find the sector with the next lowest ceiling in surrounding sectors.
sector_t* P_FindSectorSurroundingNextLowestCeiling(sector_t* sector, float baseHeight, float* val);

#define FELLF_MIN               0x1 /// Get minimum. If not set, get maximum.

typedef struct findlightlevelparams_s {
    sector_t* baseSec;
    byte flags;
    float val;
    sector_t* foundSec;
} findlightlevelparams_t;

/// Find the sector with the lowest light level in surrounding sectors.
sector_t* P_FindSectorSurroundingLowestLight(sector_t* sector, float* val);

/// Find the sector with the highest light level in surrounding sectors.
sector_t* P_FindSectorSurroundingHighestLight(sector_t* sector, float* val);

#define FNLLF_ABOVE             0x1 /// Get next above, if not set get next below.

typedef struct findnextlightlevelparams_s {
    sector_t* baseSec;
    float baseLight;
    byte flags;
    float val;
    sector_t* foundSec;
} findnextlightlevelparams_t;

/// Find the sector with the lowest light level in surrounding sectors.
sector_t* P_FindSectorSurroundingNextLowestLight(sector_t* sector, float baseLight, float* val);

/// Find the sector with the next highest light level in surrounding sectors.
sector_t* P_FindSectorSurroundingNextHighestLight(sector_t* sector, float baseLight, float* val);

/**
 * Returns the material type of the specified sector, plane.
 *
 * @param sec  The sector to check.
 * @param plane  The plane id to check.
 */
const terraintype_t* P_PlaneMaterialTerrainType(sector_t* sec, int plane);

/**
 * Copies all (changeable) properties from one line to another including the
 * extended properties.
 */
void P_CopyLine(linedef_t* dest, linedef_t* src);

/**
 * Copies all (changeable) properties from one sector to another including
 * the extended properties.
 */
void P_CopySector(sector_t* dest, sector_t* src);

float P_SectorLight(sector_t* sector);
void P_SectorSetLight(sector_t* sector, float level);
void P_SectorModifyLight(sector_t* sector, float value);
void P_SectorModifyLightx(sector_t* sector, fixed_t value);
void* P_SectorSoundOrigin(sector_t* sector);

#endif /* LIBCOMMON_DMU_LIB_H */
