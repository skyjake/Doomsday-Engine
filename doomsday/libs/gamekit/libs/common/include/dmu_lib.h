/**\file dmu_lib.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
#include "p_terraintype.h"

#define numvertexes             (P_Count(DMU_VERTEX))
#define numsectors              (P_Count(DMU_SECTOR))
#define numlines                (P_Count(DMU_LINE))
#define numsides                (P_Count(DMU_SIDE))
#define numpolyobjs             (*(int*) DD_GetVariable(DD_MAP_POLYOBJ_COUNT))

// DMU property aliases. For short-hand purposes:
#define DMU_FRONT_SECTOR        (DMU_FRONT_OF_LINE | DMU_SECTOR)
#define DMU_BACK_SECTOR         (DMU_BACK_OF_LINE | DMU_SECTOR)

#define DMU_FRONT_FLAGS         (DMU_FRONT_OF_LINE | DMU_FLAGS)
#define DMU_BACK_FLAGS          (DMU_BACK_OF_LINE | DMU_FLAGS)

#define DMU_TOP_MATERIAL        (DMU_TOP_OF_SIDE | DMU_MATERIAL)
#define DMU_TOP_MATERIAL_OFFSET_X (DMU_TOP_OF_SIDE | DMU_OFFSET_X)
#define DMU_TOP_MATERIAL_OFFSET_Y (DMU_TOP_OF_SIDE | DMU_OFFSET_Y)
#define DMU_TOP_MATERIAL_OFFSET_XY (DMU_TOP_OF_SIDE | DMU_OFFSET_XY)
#define DMU_TOP_FLAGS           (DMU_TOP_OF_SIDE | DMU_FLAGS)
#define DMU_TOP_COLOR           (DMU_TOP_OF_SIDE | DMU_COLOR)
#define DMU_TOP_COLOR_RED       (DMU_TOP_OF_SIDE | DMU_COLOR_RED)
#define DMU_TOP_COLOR_GREEN     (DMU_TOP_OF_SIDE | DMU_COLOR_GREEN)
#define DMU_TOP_COLOR_BLUE      (DMU_TOP_OF_SIDE | DMU_COLOR_BLUE)
#define DMU_TOP_EMITTER         (DMU_TOP_OF_SIDE | DMU_EMITTER)

#define DMU_MIDDLE_MATERIAL     (DMU_MIDDLE_OF_SIDE | DMU_MATERIAL)
#define DMU_MIDDLE_MATERIAL_OFFSET_X (DMU_MIDDLE_OF_SIDE | DMU_OFFSET_X)
#define DMU_MIDDLE_MATERIAL_OFFSET_Y (DMU_MIDDLE_OF_SIDE | DMU_OFFSET_Y)
#define DMU_MIDDLE_MATERIAL_OFFSET_XY (DMU_MIDDLE_OF_SIDE | DMU_OFFSET_XY)
#define DMU_MIDDLE_FLAGS        (DMU_MIDDLE_OF_SIDE | DMU_FLAGS)
#define DMU_MIDDLE_COLOR        (DMU_MIDDLE_OF_SIDE | DMU_COLOR)
#define DMU_MIDDLE_COLOR_RED    (DMU_MIDDLE_OF_SIDE | DMU_COLOR_RED)
#define DMU_MIDDLE_COLOR_GREEN  (DMU_MIDDLE_OF_SIDE | DMU_COLOR_GREEN)
#define DMU_MIDDLE_COLOR_BLUE   (DMU_MIDDLE_OF_SIDE | DMU_COLOR_BLUE)
#define DMU_MIDDLE_ALPHA        (DMU_MIDDLE_OF_SIDE | DMU_ALPHA)
#define DMU_MIDDLE_BLENDMODE    (DMU_MIDDLE_OF_SIDE | DMU_BLENDMODE)
#define DMU_MIDDLE_EMITTER      (DMU_MIDDLE_OF_SIDE | DMU_EMITTER)

#define DMU_BOTTOM_MATERIAL     (DMU_BOTTOM_OF_SIDE | DMU_MATERIAL)
#define DMU_BOTTOM_MATERIAL_OFFSET_X (DMU_BOTTOM_OF_SIDE | DMU_OFFSET_X)
#define DMU_BOTTOM_MATERIAL_OFFSET_Y (DMU_BOTTOM_OF_SIDE | DMU_OFFSET_Y)
#define DMU_BOTTOM_MATERIAL_OFFSET_XY (DMU_BOTTOM_OF_SIDE | DMU_OFFSET_XY)
#define DMU_BOTTOM_FLAGS        (DMU_BOTTOM_OF_SIDE | DMU_FLAGS)
#define DMU_BOTTOM_COLOR        (DMU_BOTTOM_OF_SIDE | DMU_COLOR)
#define DMU_BOTTOM_COLOR_RED    (DMU_BOTTOM_OF_SIDE | DMU_COLOR_RED)
#define DMU_BOTTOM_COLOR_GREEN  (DMU_BOTTOM_OF_SIDE | DMU_COLOR_GREEN)
#define DMU_BOTTOM_COLOR_BLUE   (DMU_BOTTOM_OF_SIDE | DMU_COLOR_BLUE)
#define DMU_BOTTOM_EMITTER      (DMU_BOTTOM_OF_SIDE | DMU_EMITTER)

#define DMU_FLOOR_HEIGHT        (DMU_FLOOR_OF_SECTOR | DMU_HEIGHT)
#define DMU_FLOOR_TARGET_HEIGHT (DMU_FLOOR_OF_SECTOR | DMU_TARGET_HEIGHT)
#define DMU_FLOOR_SPEED         (DMU_FLOOR_OF_SECTOR | DMU_SPEED)
#define DMU_FLOOR_MATERIAL      (DMU_FLOOR_OF_SECTOR | DMU_MATERIAL)
#define DMU_FLOOR_EMITTER       (DMU_FLOOR_OF_SECTOR | DMU_EMITTER)
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
#define DMU_CEILING_EMITTER     (DMU_CEILING_OF_SECTOR | DMU_EMITTER)
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

/// Side section indices.
typedef enum sidesection_e {
    SS_MIDDLE,
    SS_BOTTOM,
    SS_TOP
} SideSection;

#define VALID_SIDESECTION(v) ((v) >= SS_MIDDLE && (v) <= SS_TOP)

/// Helper macro for converting SideSection indices to their associated DMU flag. @ingroup world
#define DMU_FLAG_FOR_SIDESECTION(s) (\
    (s) == SS_MIDDLE? DMU_MIDDLE_OF_SIDE : \
    (s) == SS_BOTTOM? DMU_BOTTOM_OF_SIDE : DMU_TOP_OF_SIDE)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Same as P_PathTraverse except 'from' and 'to' arguments are specified
 * as two sets of separate X and Y map space coordinates.
 */
int P_PathXYTraverse2(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY,
    int flags, traverser_t callback, void *context);

int P_PathXYTraverse(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY,
    traverser_t callback, void *context);

void P_BuildLineTagLists(void);

void P_DestroyLineTagLists(void);

iterlist_t *P_GetLineIterListForTag(int tag, dd_bool createNewList);

void P_BuildSectorTagLists(void);

void P_DestroySectorTagLists(void);

iterlist_t *P_GetSectorIterListForTag(int tag, dd_bool createNewList);

void P_BuildAllTagLists(void);

void P_DestroyAllTagLists(void);

Line *P_AllocDummyLine(void);

void P_FreeDummyLine(Line *line);

/**
 * Get the sector on the other side of the line that is NOT the given sector.
 *
 * @param line  Ptr to the line to test.
 * @param sec  Reference sector to compare against.
 *
 * @return  Ptr to the other sector or @c NULL if the specified line is NOT twosided.
 */
Sector *P_GetNextSector(Line *line, Sector *sec);

#define FEPHF_MIN           0x1 // Get minium. If not set, get maximum.
#define FEPHF_FLOOR         0x2 // Get floors. If not set, get ceilings.

typedef struct findextremalplaneheightparams_s {
    Sector *baseSec;
    byte flags;
    coord_t val;
    Sector *foundSec;
} findextremalplaneheightparams_t;

/// Find the sector with the lowest floor height in surrounding sectors.
Sector *P_FindSectorSurroundingLowestFloor(Sector *sector, coord_t max, coord_t *val);

/// Find the sector with the highest floor height in surrounding sectors.
Sector *P_FindSectorSurroundingHighestFloor(Sector *sector, coord_t min, coord_t *val);

/// Find lowest ceiling in the surrounding sector.
Sector *P_FindSectorSurroundingLowestCeiling(Sector *sector, coord_t max, coord_t *val);

/// Find highest ceiling in the surrounding sectors.
Sector *P_FindSectorSurroundingHighestCeiling(Sector *sector, coord_t min, coord_t *val);

#define FNPHF_FLOOR             0x1 // Get floors, if not set get ceilings.
#define FNPHF_ABOVE             0x2 // Get next above, if not set get next below.

typedef struct findnextplaneheightparams_s {
    Sector *baseSec;
    coord_t baseHeight;
    byte flags;
    coord_t val;
    Sector *foundSec;
} findnextplaneheightparams_t;

/// Find the sector with the next highest floor in surrounding sectors.
Sector *P_FindSectorSurroundingNextHighestFloor(Sector *sector, coord_t baseHeight, coord_t *val);

/// Find the sector with the next lowest floor in surrounding sectors.
Sector *P_FindSectorSurroundingNextLowestFloor(Sector *sector, coord_t baseHeight, coord_t *val);

/// Find the sector with the next highest ceiling in surrounding sectors.
Sector *P_FindSectorSurroundingNextHighestCeiling(Sector *sector, coord_t baseHeight, coord_t *val);

/// Find the sector with the next lowest ceiling in surrounding sectors.
Sector *P_FindSectorSurroundingNextLowestCeiling(Sector *sector, coord_t baseHeight, coord_t *val);

#define FELLF_MIN               0x1 /// Get minimum. If not set, get maximum.

typedef struct findlightlevelparams_s {
    Sector *baseSec;
    byte flags;
    float val;
    Sector *foundSec;
} findlightlevelparams_t;

/// Find the sector with the lowest light level in surrounding sectors.
Sector *P_FindSectorSurroundingLowestLight(Sector *sector, float *val);

/// Find the sector with the highest light level in surrounding sectors.
Sector *P_FindSectorSurroundingHighestLight(Sector *sector, float *val);

#define FNLLF_ABOVE             0x1 /// Get next above, if not set get next below.

typedef struct findnextlightlevelparams_s {
    Sector *baseSec;
    float baseLight;
    byte flags;
    float val;
    Sector *foundSec;
} findnextlightlevelparams_t;

/// Find the sector with the lowest light level in surrounding sectors.
Sector *P_FindSectorSurroundingNextLowestLight(Sector *sector, float baseLight, float *val);

/// Find the sector with the next highest light level in surrounding sectors.
Sector *P_FindSectorSurroundingNextHighestLight(Sector *sector, float baseLight, float *val);

/**
 * Returns the material type of the specified sector, plane.
 *
 * @param sec  The sector to check.
 * @param plane  The plane id to check.
 */
const terraintype_t *P_PlaneMaterialTerrainType(Sector *sec, int plane);

/**
 * Copies all (changeable) properties from one line to another including the
 * extended properties.
 */
void P_CopyLine(Line *dest, Line *src);

/**
 * Copies all (changeable) properties from one sector to another including
 * the extended properties.
 */
void P_CopySector(Sector *dest, Sector *src);

float P_SectorLight(Sector *sector);
void P_SectorSetLight(Sector *sector, float level);
void P_SectorModifyLight(Sector *sector, float value);
void P_SectorModifyLightx(Sector *sector, fixed_t value);

void P_TranslateSideMaterialOrigin(Side *side, SideSection section, float deltaXY[2]);
void P_TranslateSideMaterialOriginXY(Side *side, SideSection section, float deltaX, float deltaY);

void P_TranslatePlaneMaterialOrigin(Plane *plane, float deltaXY[2]);
void P_TranslatePlaneMaterialOriginXY(Plane *plane, float deltaX, float deltaY);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBCOMMON_DMU_LIB_H */
