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
#define DMU_TOP_BASE            (DMU_TOP_OF_SIDEDEF | DMU_BASE)

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
#define DMU_MIDDLE_BASE         (DMU_MIDDLE_OF_SIDEDEF | DMU_BASE)

#define DMU_BOTTOM_MATERIAL     (DMU_BOTTOM_OF_SIDEDEF | DMU_MATERIAL)
#define DMU_BOTTOM_MATERIAL_OFFSET_X (DMU_BOTTOM_OF_SIDEDEF | DMU_OFFSET_X)
#define DMU_BOTTOM_MATERIAL_OFFSET_Y (DMU_BOTTOM_OF_SIDEDEF | DMU_OFFSET_Y)
#define DMU_BOTTOM_MATERIAL_OFFSET_XY (DMU_BOTTOM_OF_SIDEDEF | DMU_OFFSET_XY)
#define DMU_BOTTOM_FLAGS        (DMU_BOTTOM_OF_SIDEDEF | DMU_FLAGS)
#define DMU_BOTTOM_COLOR        (DMU_BOTTOM_OF_SIDEDEF | DMU_COLOR)
#define DMU_BOTTOM_COLOR_RED    (DMU_BOTTOM_OF_SIDEDEF | DMU_COLOR_RED)
#define DMU_BOTTOM_COLOR_GREEN  (DMU_BOTTOM_OF_SIDEDEF | DMU_COLOR_GREEN)
#define DMU_BOTTOM_COLOR_BLUE   (DMU_BOTTOM_OF_SIDEDEF | DMU_COLOR_BLUE)
#define DMU_BOTTOM_BASE         (DMU_BOTTOM_OF_SIDEDEF | DMU_BASE)

#define DMU_FLOOR_HEIGHT        (DMU_FLOOR_OF_SECTOR | DMU_HEIGHT)
#define DMU_FLOOR_TARGET_HEIGHT (DMU_FLOOR_OF_SECTOR | DMU_TARGET_HEIGHT)
#define DMU_FLOOR_SPEED         (DMU_FLOOR_OF_SECTOR | DMU_SPEED)
#define DMU_FLOOR_MATERIAL      (DMU_FLOOR_OF_SECTOR | DMU_MATERIAL)
#define DMU_FLOOR_BASE          (DMU_FLOOR_OF_SECTOR | DMU_BASE)
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
#define DMU_CEILING_BASE        (DMU_CEILING_OF_SECTOR | DMU_BASE)
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

LineDef* P_AllocDummyLine(void);
void P_FreeDummyLine(LineDef* line);

SideDef* P_AllocDummySideDef(void);
void P_FreeDummySideDef(SideDef* sideDef);

/**
 * Get the sector on the other side of the line that is NOT the given sector.
 *
 * @param line  Ptr to the line to test.
 * @param sec  Reference sector to compare against.
 *
 * @return  Ptr to the other sector or @c NULL if the specified line is NOT twosided.
 */
Sector* P_GetNextSector(LineDef* line, Sector* sec);

#define FEPHF_MIN           0x1 // Get minium. If not set, get maximum.
#define FEPHF_FLOOR         0x2 // Get floors. If not set, get ceilings.

typedef struct findextremalplaneheightparams_s {
    Sector* baseSec;
    byte flags;
    coord_t val;
    Sector* foundSec;
} findextremalplaneheightparams_t;

/// Find the sector with the lowest floor height in surrounding sectors.
Sector* P_FindSectorSurroundingLowestFloor(Sector* sector, coord_t max, coord_t* val);

/// Find the sector with the highest floor height in surrounding sectors.
Sector* P_FindSectorSurroundingHighestFloor(Sector* sector, coord_t min, coord_t* val);

/// Find lowest ceiling in the surrounding sector.
Sector* P_FindSectorSurroundingLowestCeiling(Sector* sector, coord_t max, coord_t* val);

/// Find highest ceiling in the surrounding sectors.
Sector* P_FindSectorSurroundingHighestCeiling(Sector* sector, coord_t min, coord_t* val);

#define FNPHF_FLOOR             0x1 // Get floors, if not set get ceilings.
#define FNPHF_ABOVE             0x2 // Get next above, if not set get next below.

typedef struct findnextplaneheightparams_s {
    Sector* baseSec;
    coord_t baseHeight;
    byte flags;
    coord_t val;
    Sector* foundSec;
} findnextplaneheightparams_t;

/// Find the sector with the next highest floor in surrounding sectors.
Sector* P_FindSectorSurroundingNextHighestFloor(Sector* sector, coord_t baseHeight, coord_t* val);

/// Find the sector with the next lowest floor in surrounding sectors.
Sector* P_FindSectorSurroundingNextLowestFloor(Sector* sector, coord_t baseHeight, coord_t* val);

/// Find the sector with the next highest ceiling in surrounding sectors.
Sector* P_FindSectorSurroundingNextHighestCeiling(Sector* sector, coord_t baseHeight, coord_t* val);

/// Find the sector with the next lowest ceiling in surrounding sectors.
Sector* P_FindSectorSurroundingNextLowestCeiling(Sector* sector, coord_t baseHeight, coord_t* val);

#define FELLF_MIN               0x1 /// Get minimum. If not set, get maximum.

typedef struct findlightlevelparams_s {
    Sector* baseSec;
    byte flags;
    float val;
    Sector* foundSec;
} findlightlevelparams_t;

/// Find the sector with the lowest light level in surrounding sectors.
Sector* P_FindSectorSurroundingLowestLight(Sector* sector, float* val);

/// Find the sector with the highest light level in surrounding sectors.
Sector* P_FindSectorSurroundingHighestLight(Sector* sector, float* val);

#define FNLLF_ABOVE             0x1 /// Get next above, if not set get next below.

typedef struct findnextlightlevelparams_s {
    Sector* baseSec;
    float baseLight;
    byte flags;
    float val;
    Sector* foundSec;
} findnextlightlevelparams_t;

/// Find the sector with the lowest light level in surrounding sectors.
Sector* P_FindSectorSurroundingNextLowestLight(Sector* sector, float baseLight, float* val);

/// Find the sector with the next highest light level in surrounding sectors.
Sector* P_FindSectorSurroundingNextHighestLight(Sector* sector, float baseLight, float* val);

/**
 * Returns the material type of the specified sector, plane.
 *
 * @param sec  The sector to check.
 * @param plane  The plane id to check.
 */
const terraintype_t* P_PlaneMaterialTerrainType(Sector* sec, int plane);

/**
 * Copies all (changeable) properties from one line to another including the
 * extended properties.
 */
void P_CopyLine(LineDef* dest, LineDef* src);

/**
 * Copies all (changeable) properties from one sector to another including
 * the extended properties.
 */
void P_CopySector(Sector* dest, Sector* src);

float P_SectorLight(Sector* sector);
void P_SectorSetLight(Sector* sector, float level);
void P_SectorModifyLight(Sector* sector, float value);
void P_SectorModifyLightx(Sector* sector, fixed_t value);
void* P_SectorOrigin(Sector* sector);

#endif /* LIBCOMMON_DMU_LIB_H */
