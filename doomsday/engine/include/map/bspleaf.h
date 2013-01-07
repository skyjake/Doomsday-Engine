/**
 * @file bspleaf.h
 * Map BSP leaf. @ingroup map
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_MAP_BSPLEAF
#define LIBDENG_MAP_BSPLEAF

#include "resource/r_data.h"
#include "p_dmu.h"

#ifdef __cplusplus
extern "C" {
#endif

BspLeaf* BspLeaf_New(void);

void BspLeaf_Delete(BspLeaf* bspLeaf);

biassurface_t* BspLeaf_BiasSurfaceForGeometryGroup(BspLeaf* bspLeaf, uint groupId);

/**
 * Update the BspLeaf's map space axis-aligned bounding box to encompass
 * the points defined by it's vertices.
 *
 * @param bspLeaf  BspLeaf instance.
 * @return  This BspLeaf instance, for convenience.
 */
BspLeaf* BspLeaf_UpdateAABox(BspLeaf* bspLeaf);

/**
 * Update the mid point in the map coordinate space.
 *
 * @pre Axis-aligned bounding box must have been initialized.
 *
 * @param bspLeaf  BspLeaf instance.
 * @return  This BspLeaf instance, for convenience.
 */
BspLeaf* BspLeaf_UpdateMidPoint(BspLeaf* bspLeaf);

/**
 * Update the world grid offset.
 *
 * @pre Axis-aligned bounding box must have been initialized.
 *
 * @param bspLeaf  BspLeaf instance.
 */
BspLeaf* BspLeaf_UpdateWorldGridOffset(BspLeaf* bspLeaf);

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param bspLeaf  BspLeaf instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int BspLeaf_GetProperty(const BspLeaf* bspLeaf, setargs_t* args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param bspLeaf  BspLeaf instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int BspLeaf_SetProperty(BspLeaf* bspLeaf, const setargs_t* args);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_MAP_BSPLEAF
